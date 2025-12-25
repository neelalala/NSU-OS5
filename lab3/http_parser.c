#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "http_parser.h"

void parse_url(char *url, http_request_t *request) {
    char *p = url;

    if (strncasecmp(p, "http://", 7) == 0) {
        p += 7;
    }

    char *h = request->hostname;
    while (*p && *p != ':' && *p != '/') {
        *h++ = *p++;
    }
    *h = '\0';

    if (*p == ':') {
        p++;
        request->port = atoi(p);
        while (*p && *p != '/') {
            p++;
        }
    } else {
        request->port = 80;
    }

    if (*p == '/') {
        strncpy(request->path, p, sizeof(request->path) - 1);
        request->path[sizeof(request->path) - 1] = '\0';
    } else {
        strcpy(request->path, "/");
    }
}

int parse_request(char *buffer, http_request_t *request) {
    char url[2048];
    char protocol[16];

    memset(request, 0, sizeof(http_request_t));

    if (sscanf(buffer, "%s %s %s", request->method, url, protocol) != 3) {
        return -1; 
    }

    if (strcasecmp(request->method, "GET") != 0) {
        return -1;
    }

    parse_url(url, request);

    return 0;
}