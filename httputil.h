#ifndef HTTP_SERVER_HTTPUTIL_H
#define HTTP_SERVER_HTTPUTIL_H

#include "main.h"
#include <string>

// return false when ptr is deleted
bool read_request(dict_epoll_data*, const std::string&);

// return false when ptr is deleted
bool write_response(dict_epoll_data*, const std::string&);

#endif //HTTP_SERVER_HTTPUTIL_H
