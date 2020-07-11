#include <chrono>
#include <cstring>
#include "httputil.h"

bool read_request(dict_epoll_data* ptr, const std::string& root_path) {
    char buf[1024];
    while(1) {
        ssize_t nbytes = read(ptr->fd, buf, sizeof(buf));
        if (nbytes == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                printf("finished reading data from client\n");
                break;
            }
            else {
                perror("read failed");
                exit(EXIT_FAILURE);
            }
        }
        else if (nbytes == 0) {
            printf("finished with %d\n", ptr->fd);
            delete ptr;
            return false;
        }
        else {
            fwrite(buf, sizeof(char), nbytes, stdout);
        }
    }
    ptr->last_access_time = std::chrono::steady_clock::now();
    ptr->something_to_write = true;
    return write_response(ptr, root_path);
}

bool write_response(dict_epoll_data* ptr, const std::string& root_path) {
    if(ptr->something_to_write) {
        char *hello = "HTTP/1.1 200 OK\nContent-Type: text/plain\nContent-Length: 12\n\nHello world!";
        write(ptr->fd, hello, strlen(hello));
        ptr->last_access_time = std::chrono::steady_clock::now();
    }
    return true;
}