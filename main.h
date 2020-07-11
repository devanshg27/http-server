#ifndef HTTP_SERVER_MAIN_H
#define HTTP_SERVER_MAIN_H

#include <chrono>
#include <unistd.h>
#include <string>
#include <set>

const int MAX_EPOLL_EVENTS = 256;
const int TIMEOUT = 7; // in seconds

class dict_epoll_data{
public:
//    char* body_bufptr;          // offset in dict array data
    int fd;
//    int body_cnt;               // how remainning byte
//
//    char headers[RESP_HEADER_LENTH];
//    char *headers_bufptr;       // header pointer
//    int headers_cnt;            // header length unwrite
//    int static_fd;
//    int file_cnt;               // unwrite file
//    off_t file_offset;
    decltype(std::chrono::steady_clock::now()) last_access_time;
    bool something_to_write;
    explicit dict_epoll_data(int __fd) : fd(__fd) {
        last_access_time = std::chrono::steady_clock::now();
        something_to_write = false;
    }

    ~dict_epoll_data() {
        close(fd);
    }
};

class timeout_set {
    std::set<std::pair<decltype(std::chrono::steady_clock::now()), dict_epoll_data*>> s;
public:
    void add(dict_epoll_data* ptr) {
        s.insert(std::make_pair(ptr->last_access_time, ptr));
    }
    void remove(dict_epoll_data* ptr) {
        s.erase(std::make_pair(ptr->last_access_time, ptr));
    }
    void remove_expired() {
        auto expiry_time = std::chrono::steady_clock::now() - std::chrono::seconds(TIMEOUT);
        while(!s.empty() and s.begin()->first < expiry_time) {
            delete s.begin()->second;
            s.erase(s.begin());
        }
    }
};

void accept_conns_loop(int, int, const std::string&);
void accept_incoming_connection(int, int, timeout_set&);

#endif //HTTP_SERVER_MAIN_H
