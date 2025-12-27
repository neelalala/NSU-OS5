#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>

#include "downloader.h"
#include "proxy.h"

int connect_to_remote(char *hostname, int port) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    char port_str[6];

    snprintf(port_str, sizeof(port_str), "%d", port);

    memset(&hints, 0, sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(hostname, port_str, &hints, &servinfo) != 0) {
        perror("getaddrinfo failed");
        return -1;
    }

    for (p = servinfo; p; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1 ) {
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1 ) {
            close(sockfd);
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if (!p) {
        return -1;
    }

    return sockfd;
}

void cleanup_on_error(download_args *args) {
    pthread_mutex_lock(&args->entry->mutex);
    args->entry->is_error = args->entry->is_complete = 1;

    pthread_cond_broadcast(&args->entry->cond);
    pthread_mutex_unlock(&args->entry->mutex);

    cache_remove_unsafe(args->cache, args->entry);
    cache_entry_release(args->entry);

    free(args->hostname);
    free(args->path);
    free(args);
}

void* download_routine(void* arg) {
    download_args *args = (download_args*)arg;
    Cache* cache = args->cache;
    Entry* entry = args->entry;
    char *hostname = args->hostname;
    char *path = args->path;
    int port = args->port;

    printf("[Downloader] Started for %s:%d%s\n", hostname, port, path);

    int server_sock = connect_to_remote(hostname, port);
    if (server_sock < 0) {
        printf("[Downloader] Connection to remote failed\n");
        cleanup_on_error(args);
        return NULL;
    }

    char request[BUFFER_SIZE];
    int req_len = snprintf(request, sizeof(request), 
            "GET %s HTTP/1.0\r\n"
            "Host: %s\r\n"
            "Connection: close\r\n\r\n",
            path, hostname);

    if (req_len >= sizeof(request)) {
        printf("[Downloader] Request is too long (%dbytes)\n", req_len);
        close(server_sock);

        cleanup_on_error(args);
        return NULL;
    }

    if (write(server_sock, request, req_len) < 0) {
        perror("[Downloader] Write to server failed");
        close(server_sock);

        cleanup_on_error(args);
        return NULL;
    }

    int first_chunk = 1;
    int status_code;
    char buf[BUFFER_SIZE];
    int n;
    while ((n = read(server_sock, buf, sizeof(buf))) > 0) {
        if (first_chunk) {
            first_chunk = 0;
            int http_ver_major, http_ver_minor;
            if (sscanf(buf, "HTTP/%d.%d %d", &http_ver_major, &http_ver_minor, &status_code) == 3) {
                if (status_code != 200) {
                    printf("[Downloader] Status %d for %s. Removing from cache list.\n", status_code, path);
                    cache_remove_unsafe(cache, entry);
                }
            }
        }
        cache_append_data(entry, buf, n);
    }

    if (n < 0) {
        perror("[Downloader] Read error");
        close(server_sock);

        if (status_code == 200) {
            cache_remove_unsafe(cache, entry);
        }

        pthread_mutex_lock(&entry->mutex);
        entry->is_error = entry->is_complete = 1;
 
        pthread_cond_broadcast(&entry->cond);
        pthread_mutex_unlock(&entry->mutex);

        free(hostname);
        free(path);
        free(args);
        return NULL;
    }

    printf("[Downloader] Finished: %s\n", path);

    cache_mark_complete(entry);
    cache_entry_release(entry);

    close(server_sock);
    
    free(hostname);
    free(path);
    free(args);
    
    return NULL;
}