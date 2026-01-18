#ifndef PROXY_H
#define PROXY_H

#include <stdio.h>
#include <stdlib.h>

#define PORT 12345
#define BUFFER_SIZE (1 << 12)

typedef struct {
    char method[16];
    char hostname[256];
    char path[2048]; 
    int port;
    int version_major;
    int version_minor;
} http_request_t;

#endif