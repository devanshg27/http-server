# http-server

A basic HTTP server that supports HTTP/1.1 in C++

## Usage

```sh
cmake . -DCMAKE_BUILD_TYPE=Release
make
./http_server <path> <port>
```

For demo purposes, please run `./http_server public 8080` and open `localhost:8080` in any browser.

To test the performance, please rune `./test_server`.

## Features

* Runs on Linux.
* Does not use any third party networking library, i.e. use only Linux system API.
* Supports persistent connections along with a timeout after inactivity.
* Supports caching(Appropriately sets Last-modified, max-age and Cache-control headers).
* Support for content-type header.
* Supports GET and HEAD methods(with support for 404 Not found and 403 Forbidden error codes).
* Has error handling support(with a basic mapping to error codes in response).