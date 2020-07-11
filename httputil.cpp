#include <chrono>
#include <cstring>
#include <map>
#include <sys/sendfile.h>
#include <iostream>
#include "httputil.h"
#include "fileutil.h"

const char *default_mime_type = "text/plain";
const std::map<std::string, std::string> mime_types = {
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".css", "text/css"},
    {".gif", "image/gif"},
    {".html", "text/html"},
    {".ico", "image/x-icon"},
    {".js", "application/javascript"},
    {".pdf", "application/pdf"},
    {".mp4", "video/mp4"},
    {".png", "image/png"},
    {".svg", "image/svg+xml"},
    {".xml", "text/xml"}
};

bool read_request(dict_epoll_data* ptr, const std::string& root_path) {
    char buf[1024];
    while(1) {
        if(ptr->invalid_request_header) return true;
        ssize_t nbytes = read(ptr->fd, buf, sizeof(buf));
        if (nbytes == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            else {
                perror("read failed");
                delete ptr;
                return false;
            }
        }
        else if (nbytes == 0) {
            printf("finished with %d\n", ptr->fd);
            delete ptr;
            return false;
        }
        else {
            ptr->request_headers.append(buf, nbytes);
        }
        parse_request(ptr, root_path);
    }
    ptr->last_access_time = std::chrono::steady_clock::now();
    return write_response(ptr);
}

bool write_response(dict_epoll_data* ptr) {
    while(!ptr->to_write.empty()) {
        if(ptr->to_write.front().header_left) {
            int start_pos = ptr->to_write.front().headers.size()-ptr->to_write.front().header_left;
            int nwritten = write(
                ptr->fd,
                ptr->to_write.front().headers.c_str()+start_pos,
                ptr->to_write.front().header_left
            );
            if(nwritten <= 0) {
                if(errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
                else {
                    perror("write failed");
                    delete ptr;
                    return false;
                }
            }
            else {
                ptr->to_write.front().header_left -= nwritten;
                ptr->last_access_time = std::chrono::steady_clock::now();
            }
        }
        else if(ptr->to_write.front().file_left) {
            auto nwritten = sendfile(
                ptr->fd,
                ptr->to_write.front().fd,
                &ptr->to_write.front().file_offset,
                ptr->to_write.front().file_left
            );
            if(nwritten <= 0) {
                if(errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                }
                else {
                    perror("sendfile failed");
                    delete ptr;
                    return false;
                }
            }
            else {
                ptr->to_write.front().file_left -= nwritten;
                ptr->last_access_time = std::chrono::steady_clock::now();
            }
        }
        else {
            ptr->to_write.pop_front();
        }
    }
    if(ptr->to_write.empty() && ptr->invalid_request_header) {
        delete ptr;
        return false;
    }
    return true;
}

void serve_file(dict_epoll_data* ptr, const std::string& file_path, bool isGet) {
    if(access(file_path.data(), F_OK) == -1) {
        // file does not exist
        ptr->to_write.emplace_back(404);
    }
    else if(access(file_path.data(), R_OK) == -1) {
        // do not have read permission
        ptr->to_write.emplace_back(403);
    }
    else if(isDirectory(file_path)){
        serve_file(ptr, file_path + "/index.html", isGet);
    }
    else {
        struct stat result;
        char file_modified_time[128];
        if(stat(file_path.c_str(), &result) != -1) {
            auto mod_time = result.st_mtime;
            struct tm *tm = gmtime(&mod_time);
            strftime(file_modified_time, sizeof(file_modified_time), "%a, %d %b %Y %H:%M:%S %Z", tm);
        }
        else {
            ptr->to_write.emplace_back(500);
            perror("stat failed");
            return;
        }
        int file_fd = open(file_path.c_str(), O_RDONLY);
        if(file_fd == -1) {
            ptr->to_write.emplace_back(500);
            perror("stat failed");
            return;
        }
        else {
            bool isDefaultMIME = true;
            auto lpos = file_path.find_last_of('.');
            if(lpos == std::string::npos) {
                isDefaultMIME = true;
            }
            else if(mime_types.find(file_path.substr(lpos)) == mime_types.end()) {
                isDefaultMIME = true;
            }
            else {
                isDefaultMIME = false;
            }
            if (!isGet) {
                ptr->to_write.emplace_back(
                        200,
                        -1,
                        result.st_size,
                        mime_types.find(file_path.substr(lpos + 1))->second.c_str(),
                        file_modified_time
                );
                close(file_fd);
            }
            else if (!isDefaultMIME) {
                ptr->to_write.emplace_back(
                        200,
                        file_fd,
                        result.st_size,
                        mime_types.find(file_path.substr(lpos + 1))->second.c_str(),
                        file_modified_time
                );
            }
            else {
                ptr->to_write.emplace_back(
                        200,
                        file_fd,
                        result.st_size,
                        default_mime_type,
                        file_modified_time
                );
            }
        }
    }
}

void parse_request(dict_epoll_data* ptr, const std::string& root_path) {
    while(1) {
        auto pos = ptr->request_headers.find("\r\n\r\n");
        if(pos == std::string::npos) {
            if(ptr->request_headers.length() >= MAX_HEADER_SIZE) {
                ptr->invalid_request_header = true;
                ptr->to_write.emplace_back(413);
            }
            break;
        }
        if(ptr->request_headers.substr(0, 4) == "GET " || ptr->request_headers.substr(0, 5) == "HEAD ") {
            int pos1 = ptr->request_headers.find(' ') + 1;
            int pos2 = ptr->request_headers.find(' ', pos1);
            std::string filePath = root_path + '/' + ptr->request_headers.substr(pos1, pos2-pos1);
            serve_file(ptr, filePath, (ptr->request_headers[0] == 'G'));
        }
        else {
            ptr->to_write.emplace_back(400);
        }
        ptr->request_headers = ptr->request_headers.substr(pos+4);
    }
}