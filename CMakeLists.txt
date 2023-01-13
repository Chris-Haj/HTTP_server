cmake_minimum_required(VERSION 3.24)
project(HTTP_server C)

set(CMAKE_C_STANDARD 11)

add_executable(HTTP_server threadpool.c threadpool.h server.c wisam.c)
