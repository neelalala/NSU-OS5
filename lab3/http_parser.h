#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include "proxy.h"

#define BAD_REQUEST 1
#define METHOD_NOT_ALLOWED 2

int parse_request(char *buffer, http_request_t *request);
void parse_url(char *url, http_request_t *request);

#endif