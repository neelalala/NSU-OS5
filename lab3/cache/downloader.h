#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include "cache.h"

typedef struct download_args_t {
    char* hostname;
    int port;
    char* path;

    Cache* cache;
    Entry* entry;
} download_args;

void* download_routine(void* arg);


#endif