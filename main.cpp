#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include "fileutil.h"
#include <unistd.h>
#include <fcntl.h>

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

    //creating a socket using socket() and setting options SO_REUSEADDR and SO_REUSEPORT
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

    return EXIT_SUCCESS;
}