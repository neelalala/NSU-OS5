#ifndef CACHE_H
#define CACHE_H

#include <pthread.h>

typedef struct Node_t {
    char* value;
    int size;
    struct Node_t* next;
} Node;

typedef struct Entry_t {
    char* url;
    
    Node* first;
    Node* last;
    int total_size;
    
    int is_complete;
    int is_error;
    
    pthread_mutex_t mutex;
    pthread_cond_t cond;

    struct Entry_t* next;   
} Entry;

typedef struct Cache_t {
    Entry* first;
    pthread_rwlock_t rwlock;
} Cache;

int cache_init(Cache** list);
Entry* cache_find_or_create(Cache* list, char* host, int port, char* path);
void cache_append_data(Entry* entry, char* data, int len);
void cache_mark_complete(Entry* entry);

#endif