#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/types.h>

#include "proxy.h"
#include "http_parser.h"
#include "cache/cache.h"

typedef struct {
    int client_socket;
    Cache* cache;
} client_args_t;

void printRequest(http_request_t* req) {
    printf("REQUEST:\n");
    printf("Method: %s\nHost: %s\nPath: %s\nPort: %d\nVersion: HTTP/%d.%d\n\n", req->method, req->hostname, req->path, req->port, req->version_major, req->version_minor);
}

int read_request(int sock, http_request_t* req) {
    char buffer[BUFFER_SIZE];
    int total_read = 0;
    int header_end_found = 0;
    int n;

    while (total_read < BUFFER_SIZE - 1) {
        n = read(sock, buffer + total_read, BUFFER_SIZE - 1 - total_read); // TODO: may read body in this buffer
        if (n <= 0) {
            close(sock);
            return -1;
        }

        total_read += n;

        buffer[total_read] = '\0';

        if (strstr(buffer, "\r\n\r\n") != NULL) {
            header_end_found = 1;
            break;
        }
    }

    if (!header_end_found) {
        printf("Header too large or connection closed\n");
        close(sock);
        return -1;
    }

    return parse_request(buffer, req);
}

void stream_from_cache(Entry* entry, int client_sock) {
    Node* current_node = NULL; 
    
    pthread_mutex_lock(&entry->mutex);

    while (1) {
        Node* next_node;
        if (current_node == NULL) {
            // printf("[Client] Stream from cache : current node == NULL, idx = %d\n", i);
            next_node = entry->first;
        } else {
            // printf("[Client] Stream from cache : current node != NULL, idx = %d\n", i);
            next_node = current_node->next;
        }

        if (next_node != NULL) {
            char* data = next_node->value;
            int size = next_node->size;

            current_node = next_node;

            pthread_mutex_unlock(&entry->mutex);

            int sent = write(client_sock, data, size);
            if (sent <= 0) {
                return; 
            }

            pthread_mutex_lock(&entry->mutex);
        } 
        else {
            if (entry->is_error) {
                fprintf(stderr, "Stream aborted due to download error\n");
                break;
            }

            if (entry->is_complete) {
                break;
            }

            pthread_cond_wait(&entry->cond, &entry->mutex);
        }
    }

    pthread_mutex_unlock(&entry->mutex);
}

void* handle_client(void *args) {
    client_args_t *client_info = (client_args_t *)args;
    int client_sock = client_info->client_socket;
    Cache *cache = client_info->cache;
    free(client_info);

    http_request_t req;
    if (read_request(client_sock, &req) < 0) {
        printf("[Client] Failed to parse request or unsupported method\n");
        close(client_sock);
        return NULL;
    }

    printf("[Client] Serving %s %s:%d%s\n", req.method, req.hostname, req.port, req.path);

    Entry *entry = cache_find_or_create(cache, req.hostname, req.port, req.path);

    if (!entry) {
        fprintf(stderr, "[Client] Failed to get cache entry\n");
    } else {
        stream_from_cache(entry, client_sock);
        cache_entry_release(entry);
    }

    printf("[Client] Finished serving %s\n", req.path);

    close(client_sock);
    return NULL;
}

void* server_routine(void *arg) {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    Cache *cache;
    int err = cache_init(&cache);
    if (err) {
        printf("Error creating cache\n");
        exit(1);
    }

    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        exit(2);
    }

    if (listen(server_sock, 10) < 0) {
        perror("listen failed");
        exit(3);
    }

    printf("HTTP 1.0 Proxy listening on port %d\n", PORT);

    while (1) {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_len);
        if (client_sock < 0) {
            perror("accept failed");
            continue;
        }

        client_args_t *args = malloc(sizeof(client_args_t));
        args->client_socket = client_sock;
        args->cache = cache;

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handle_client, (void *)args) != 0) {
            perror("pthread_create failed");
            close(client_sock);
            free(args);
        } else {
            pthread_detach(thread_id);
        }
    }

    close(server_sock);
    return NULL;
}

int main() {
    signal(SIGPIPE, SIG_IGN);
    
    pthread_t server;
    if (pthread_create(&server, NULL, server_routine, NULL) != 0) {
        perror("Error creating server thread");
        return 1;
    }

    pthread_join(server, NULL);

    return 0;
}