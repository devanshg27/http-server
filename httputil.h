#ifndef HTTP_SERVER_HTTPUTIL_H
#define HTTP_SERVER_HTTPUTIL_H

#include "main.h"
#include "fileutil.h"
#include <string>
#include <assert.h>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

class response {
public:
    int fd;
    std::string headers;
    int header_left;
    int file_left;
    off_t file_offset;
    response(int __code, int __fd = -1, long unsigned int file_size = -1, const char* file_type = NULL, const char* modified = NULL) {
        int code = __code;
        file_left = 0;
        fd = __fd;
        file_offset = 0;
        if(code == 200) {
            char buff[4096];
            sprintf(
                buff,
                "HTTP/1.1 200 OK\r\nContent-length: %lu\r\nCache-Control: max-age=86400, public\r\nContent-type: %s\r\nLast-Modified: %s\r\n\r\n",
                file_size,
                file_type,
                modified
            );
            headers = buff;
            if(__fd != -1) file_left = file_size;
        }
        else if(code == 400) {
            headers = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
        }
        else if(code == 403) {
            headers = "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n";
        }
        else if(code == 404) {
            headers = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        }
        else if(code == 413) {
            headers = "HTTP/1.1 413 Payload Too Large\r\nConnection: close\r\nContent-Length: 0\r\n\r\n";
        }
        else if(code == 500) {
            headers = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";
        }
        else {
            assert(false);
        }
        header_left = headers.size();
    }
    ~response() {
        close(fd);
    }
};

const int MAX_HEADER_SIZE = 8192;

// return false when ptr is deleted
bool read_request(dict_epoll_data*, const std::string&);

void serve_file(dict_epoll_data*, const std::string&, bool);

// return false when ptr is deleted
bool write_response(dict_epoll_data*);

void parse_request(dict_epoll_data*, const std::string&);

#endif //HTTP_SERVER_HTTPUTIL_H
