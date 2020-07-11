#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <resolv.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>

#define PORT 8080
#define SERVER "127.0.0.1"
#define MAXBUF 1024
#define MAX_EPOLL_EVENTS 256

const char* request = "GET /public/404.html HTTP/1.1\r\n\r\n";
const char* reply = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
const int request_len = strlen(request);

class dict_epoll_data{
public:
    int fd;
    int write_left;
    int read_left;
    dict_epoll_data(int __fd) : fd(__fd) {
        write_left = strlen(request);
        read_left = strlen(reply);
    }

    ~dict_epoll_data() {
        close(fd);
    }
};

int main() {
    int sockfd;
    struct sockaddr_in dest;
    char buffer[MAXBUF];
    struct epoll_event events[MAX_EPOLL_EVENTS];
    int num_ready;

    int epfd = epoll_create(1);

    int times = 100, successful = 0;

    auto t_start = std::chrono::steady_clock::now();

    for(int j=0; j<times; ++j) {
        int count = 1000;
        for (int i = 0; i < count; ++i) {
            /*---Open socket for streaming---*/
            if ((sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0)) < 0) {
                perror("Socket");
                exit(errno);
            }

            /*---Add socket to epoll---*/
            struct epoll_event event;
            event.events = EPOLLIN | EPOLLOUT | EPOLLET;
            event.data.ptr = new dict_epoll_data(sockfd);
            epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &event);

            /*---Initialize server address/port struct---*/
            bzero(&dest, sizeof(dest));
            dest.sin_family = AF_INET;
            dest.sin_port = htons(PORT);
            if (inet_pton(AF_INET, SERVER, &dest.sin_addr.s_addr) == 0) {
                perror(SERVER);
                exit(errno);
            }

            /*---Connect to server---*/
            if (connect(sockfd, (struct sockaddr *) &dest, sizeof(dest)) != 0) {
                if (errno != EINPROGRESS) {
                    perror("Connect ");
                    exit(errno);
                }
            }
        }

        while (1) {
            if (!count) break;
            /*---Wait for socket connect to complete---*/
            num_ready = epoll_wait(epfd, events, MAX_EPOLL_EVENTS, 1000/*timeout*/);
            for (int k = 0; k < num_ready; k++) {
                if ((events[k].events & EPOLLERR) || (events[k].events & EPOLLHUP)) {
                    fprintf(stderr, "epoll error %d\n", events[k].events);
                    --count;
                    close(((dict_epoll_data *) events[k].data.ptr)->fd);
                    continue;
                }
                if (((dict_epoll_data *) events[k].data.ptr)->write_left) {
                    for (;;) {
                        ssize_t nbytes = write(
                                ((dict_epoll_data *) events[k].data.ptr)->fd,
                                request + request_len - ((dict_epoll_data *) events[k].data.ptr)->write_left,
                                ((dict_epoll_data *) events[k].data.ptr)->write_left
                        );
                        if (nbytes <= 0) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS) {
                                break;
                            } else {
                                perror("write ");
                                --count;
                                close(((dict_epoll_data *) events[k].data.ptr)->fd);
                                break;
                            }
                        } else {
                            ((dict_epoll_data *) events[k].data.ptr)->write_left -= nbytes;
                        }
                    }
                }
                if (!((dict_epoll_data *) events[k].data.ptr)->write_left and
                    ((dict_epoll_data *) events[k].data.ptr)->read_left) {
                    for (;;) {
                        ssize_t nbytes = read(((dict_epoll_data *) events[k].data.ptr)->fd, buffer, sizeof(buffer));
                        if (nbytes == -1) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                                break;
                            } else {
                                perror("read ");
                                --count;
                                close(((dict_epoll_data *) events[k].data.ptr)->fd);
                                break;
                            }
                        }
                        else if (nbytes == ((dict_epoll_data *) events[k].data.ptr)->read_left) {
                            --count;
                            ++successful;
                            close(((dict_epoll_data *) events[k].data.ptr)->fd);
                            break;
                        }
                        else if(nbytes == 0) {
                            --count;
                            close(((dict_epoll_data *) events[k].data.ptr)->fd);
                            break;
                        }
                        else {
                            ((dict_epoll_data *) events[k].data.ptr)->read_left -= nbytes;
                        }
                    }
                }
            }
        }
    }

    auto t_end = std::chrono::steady_clock::now();
    double elapsed_time_ms = std::chrono::duration<double, std::milli>(t_end-t_start).count();

    std::cout << successful << " connections served in " << elapsed_time_ms/1000 <<" seconds." << std::endl;

    return 0;
}