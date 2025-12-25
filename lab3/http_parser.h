#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include "proxy.h"

int parse_request(char *buffer, http_request_t *request);
void parse_url(char *url, http_request_t *request);

#endif