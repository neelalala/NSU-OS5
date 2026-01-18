#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "http_parser.h"

void parse_url(char *url, http_request_t *request) {
    char *ptr = url;

    if (strncasecmp(ptr, "http://", 7) == 0) {
        ptr += 7;
    }

    char *hostname = request->hostname;
    while (*ptr && *ptr != ':' && *ptr != '/') {
        *hostname++ = *ptr++;
    }
    *hostname = '\0';

    if (*ptr == ':') {
        ptr++;
        request->port = atoi(ptr);
        while (*ptr && *ptr != '/') {
            ptr++;
        }
    } else {
        request->port = 80;
    }

    if (*ptr == '/') {
        strncpy(request->path, ptr, sizeof(request->path) - 1);
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
        return BAD_REQUEST;
    }

    if (strcasecmp(request->method, "GET") != 0) {
        return METHOD_NOT_ALLOWED;
    }

    parse_url(url, request);

    return 0;
}