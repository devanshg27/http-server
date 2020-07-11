cmake_minimum_required(VERSION 3.16)
project(http_server)

set(CMAKE_CXX_STANDARD 14)

add_executable(http_server main.cpp fileutil.h fileutil.cpp httputil.cpp httputil.h main.h)