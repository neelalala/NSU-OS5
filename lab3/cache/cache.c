#include "cache.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "downloader.h"

int cache_init(Cache** list) {
    (*list) = malloc(sizeof(Cache));
    (*list)->first = NULL;

    if (pthread_rwlock_init(&(*list)->rwlock, NULL) != 0) {
        free(list);
        return 1;
    }

    return 0;
}

Entry* cache_find_or_create(Cache* list, char* host, int port, char* path) {
    char url[2048];
    int len = snprintf(url, sizeof(url), "%s:%d%s", host, port, path);
    if (len >= sizeof(url)) {
        printf("ERROR: URL IS TO LONG (%d) : %s:%d%s\n", len, host, port, path);
        return NULL;
    }
    pthread_rwlock_rdlock(&list->rwlock);
    Entry* current = list->first;
    while (current) {
        if (strcmp(current->url, url) == 0) {
            pthread_rwlock_unlock(&list->rwlock);
            return current;
        }
        current = current->next;
    }
    pthread_rwlock_unlock(&list->rwlock);
    
    pthread_rwlock_wrlock(&list->rwlock);
    current = list->first;
    while (current) {
        if (strcmp(current->url, url) == 0) {
            pthread_rwlock_unlock(&list->rwlock);
            return current;
        }
        current = current->next;
    }

    Entry* new_entry = malloc(sizeof(Entry));
    new_entry->url = strdup(url);

    new_entry->first = new_entry->last = NULL;
    new_entry->total_size = 0;
    new_entry->is_complete = 0;
    new_entry->is_error = 0;
    
    pthread_mutex_init(&new_entry->mutex, NULL);
    pthread_cond_init(&new_entry->cond, NULL);
    
    new_entry->next = list->first;
    list->first = new_entry;

    download_args *args = malloc(sizeof(download_args));
    args->hostname = host;
    args->port = port;
    args->path = path;
    args->entry = new_entry;

    pthread_t downloader;
    if (pthread_create(&downloader, NULL, download_routine, args) != 0) {
        new_entry->is_complete = new_entry->is_error = 1;
        pthread_cond_broadcast(&new_entry->cond);
        free(args);
    } else {
        pthread_detach(downloader);
    }
    
    pthread_rwlock_unlock(&list->rwlock);
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