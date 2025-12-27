#include "cache.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "downloader.h"

int cache_init(Cache** cache) {
    (*cache) = malloc(sizeof(Cache));
    (*cache)->first = NULL;

    if (pthread_rwlock_init(&(*cache)->rwlock, NULL) != 0) {
        free(cache);
        return 1;
    }

    return 0;
}

Entry* cache_find_or_create(Cache* cache, char* host, int port, char* path) {
    char url[2048];
    int len = snprintf(url, sizeof(url), "%s:%d%s", host, port, path);
    if (len >= sizeof(url)) {
        printf("ERROR: URL IS TO LONG (%d) : %s:%d%s\n", len, host, port, path);
        return NULL;
    }
    pthread_rwlock_rdlock(&cache->rwlock);
    Entry* current = cache->first;
    while (current) {
        if (strcmp(current->url, url) == 0) {
            pthread_rwlock_unlock(&cache->rwlock);
            pthread_mutex_lock(&current->mutex);
            current->ref_count++;
            pthread_mutex_unlock(&current->mutex);
            return current;
        }
        current = current->next;
    }
    pthread_rwlock_unlock(&cache->rwlock);
    
    pthread_rwlock_wrlock(&cache->rwlock);
    current = cache->first;
    while (current) {
        if (strcmp(current->url, url) == 0) {
            pthread_rwlock_unlock(&cache->rwlock);
            pthread_mutex_lock(&current->mutex);
            current->ref_count++;
            pthread_mutex_unlock(&current->mutex);
            return current;
        }
        current = current->next;
    }

    Entry* new_entry = malloc(sizeof(Entry));
    new_entry->url = strdup(url);

    new_entry->ref_count = 3; // 1 - в кеше, 2 - в downloader, 3 - в client_handler
    new_entry->first = new_entry->last = NULL;
    new_entry->total_size = 0;
    new_entry->is_complete = 0;
    new_entry->is_error = 0;
    
    pthread_mutex_init(&new_entry->mutex, NULL);
    pthread_cond_init(&new_entry->cond, NULL);
    
    new_entry->next = cache->first;
    cache->first = new_entry;

    download_args *args = malloc(sizeof(download_args));
    args->hostname = strdup(host);
    args->port = port;
    args->path = strdup(path);
    args->cache = cache;
    args->entry = new_entry;

    pthread_t downloader;
    if (pthread_create(&downloader, NULL, download_routine, args) != 0) {
        new_entry->is_complete = new_entry->is_error = 1;
        new_entry->ref_count--;
        pthread_cond_broadcast(&new_entry->cond);
        free(args->hostname);
        free(args->path);
        free(args);
    } else {
        pthread_detach(downloader);
    }
    
    pthread_rwlock_unlock(&cache->rwlock);
    return new_entry;
}

void cache_append_data(Entry* entry, char* data, int len) {
    Node* node = malloc(sizeof(Node));
    node->value = malloc(len);
    memcpy(node->value, data, len);
    node->size = len;
    node->next = NULL;

    pthread_mutex_lock(&entry->mutex);
    
    if (entry->last) {
        entry->last->next = node;
        entry->last = node;
    } else {
        entry->first = node;
        entry->last = node;
    }
    entry->total_size += len;

    pthread_cond_broadcast(&entry->cond); 
    
    pthread_mutex_unlock(&entry->mutex);
}

void cache_mark_complete(Entry* entry) {
    pthread_mutex_lock(&entry->mutex);
    entry->is_complete = 1;
    pthread_cond_broadcast(&entry->cond);
    pthread_mutex_unlock(&entry->mutex);
}

void free_entry(Entry* entry) {
    if (!entry) return;

    Node* current = entry->first;
    while (current) {
        Node* next = current->next;
        free(current->value);
        free(current);
        current = next;
    }

    pthread_mutex_destroy(&entry->mutex);
    pthread_cond_destroy(&entry->cond);

    free(entry->url);
    free(entry);
}

void cache_entry_release(Entry* entry) {
    int should_free = 0;

    pthread_mutex_lock(&entry->mutex);
    entry->ref_count--;
    if (entry->ref_count == 0) {
        should_free = 1;
    }
    pthread_mutex_unlock(&entry->mutex);

    if (should_free) {
        free_entry(entry);
    }
}

void cache_remove_unsafe(Cache* cache, Entry* entry) {
    pthread_rwlock_wrlock(&cache->rwlock);

    Entry* prev = NULL;
    Entry* current = cache->first;
    while (current) {
        if (current == entry) {
            if (prev) {
                prev->next = current->next;
            } else {
                cache->first = current->next;
            }
        }
        prev = current;
        current = current->next;
    }

    pthread_rwlock_unlock(&cache->rwlock);
    entry->next = NULL;

    cache_entry_release(entry); // лист больше не держит сылку
}