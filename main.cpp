#include "fileutil.h"
#include "httputil.h"
#include "main.h"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <set>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char* argv[]) {

    // parsing arguments
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <path> <port>" << std::endl;
        return EXIT_FAILURE;
    }
    std::string root_path = argv[1];
    if(not isDirectory(root_path)) {
        std::cerr << "Error: Path must be a valid directory." << std::endl;
        return EXIT_FAILURE;
    }
    char *pEnd;
    int portNo = std::strtol(argv[2], &pEnd, 10);
    if(portNo <= 0 or portNo > 65535 or *pEnd != '\0') {
        std::cerr << "Invalid Port number" << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Starting server on port " << portNo << " serving files from directory " << root_path << ".\n";

    // creating a socket using socket() and setting options SO_REUSEADDR and SO_REUSEPORT
    int server_fd;
    if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket failed");
		exit(EXIT_FAILURE);
	}
    int enable = 1;
    if(setsockopt(server_fd, SOL_SOCKET, (SO_REUSEADDR | SO_REUSEPORT), &enable, sizeof(int)) < 0) {
        perror("setsockopt failed");
		exit(EXIT_FAILURE);
    }

    // binding the socket to an address
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short) portNo);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    memset(&(address.sin_zero), 0, sizeof(address.sin_zero));
    if(bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
		perror("bind failed");
		exit(EXIT_FAILURE);
	}

    // make socket non blocking
    int flags = fcntl(server_fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl failed");
        exit(EXIT_FAILURE);
    }
    flags |= O_NONBLOCK;
    if (fcntl(server_fd, F_SETFL, flags) == -1) {
        perror("fcntl failed");
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    if (listen(server_fd, SOMAXCONN) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    // create an epoll instance
    int epoll_fd;
    if((epoll_fd = epoll_create1(0)) < 0) {
        perror("epoll failed");
        exit(EXIT_FAILURE);
    }

    // mark the server socket for reading, and become edge-triggered
    struct epoll_event event;
    memset(&event, 0, sizeof(event));
    event.data.ptr = (void*) new dict_epoll_data(server_fd);
    event.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
        perror("epoll_ctl failed");
        exit(EXIT_FAILURE);
    }

    accept_conns_loop(server_fd, epoll_fd, root_path);

    return EXIT_SUCCESS;
}

void accept_conns_loop(int server_fd, int epoll_fd, const std::string& root_path) {
    struct epoll_event epoll_events[MAX_EPOLL_EVENTS];
    timeout_set timeout_sockets;
    while(1) {
        // close file descriptors that have timed out
        timeout_sockets.remove_expired();

        // query epoll, update to use min timeout+5
        int num_events = epoll_wait(epoll_fd, epoll_events, MAX_EPOLL_EVENTS, TIMEOUT*1000);
        if (num_events == -1) {
            perror("epoll_wait failed");
            exit(EXIT_FAILURE);
        }
        for (int i = 0; i < num_events; i++) {
            int event_mask = epoll_events[i].events;
            if (((dict_epoll_data*) epoll_events[i].data.ptr)->fd == server_fd) {
                if((event_mask & EPOLLHUP) || (event_mask & EPOLLERR)) {
                    std::cerr << "epoll event error on listening socket\n";
                    exit(EXIT_FAILURE);
                }
                accept_incoming_connection(server_fd, epoll_fd, timeout_sockets);
            }
            else if((event_mask & EPOLLRDHUP) || (event_mask & EPOLLERR) || (event_mask & EPOLLHUP)) {
                if(!(event_mask & EPOLLRDHUP)) std::cerr << "epoll event error\n";
                timeout_sockets.remove((dict_epoll_data*) epoll_events[i].data.ptr);
                delete (dict_epoll_data*) epoll_events[i].data.ptr;
            }
            else if(event_mask & EPOLLIN) {
                timeout_sockets.remove((dict_epoll_data*) epoll_events[i].data.ptr);
                if(read_request((dict_epoll_data*) epoll_events[i].data.ptr, root_path)) {
                    timeout_sockets.add((dict_epoll_data*) epoll_events[i].data.ptr);
                }
            }
            else if(event_mask & EPOLLOUT) {
                timeout_sockets.remove((dict_epoll_data*) epoll_events[i].data.ptr);
                if(write_response((dict_epoll_data*) epoll_events[i].data.ptr)) {
                    timeout_sockets.add((dict_epoll_data*) epoll_events[i].data.ptr);
                }
            }
        }
    }
}

void accept_incoming_connection(int server_fd, int epoll_fd, timeout_set& timeout_sockets) {
    while(1) {
        struct sockaddr in_addr;
        socklen_t in_addr_len = sizeof(in_addr);
        int client_fd = accept4(server_fd, &in_addr, &in_addr_len, SOCK_NONBLOCK);
        if (client_fd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            else {
                perror("accept failed");
            }
        }
        else {
            struct epoll_event event;
            event.data.ptr = (void*) new dict_epoll_data(client_fd);
            event.events = EPOLLIN | EPOLLOUT | EPOLLRDHUP| EPOLLET;;
            if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
                perror("epoll_ctl failed");
                exit(EXIT_FAILURE);
            }
            timeout_sockets.add((dict_epoll_data*) event.data.ptr);
        }
    }
}