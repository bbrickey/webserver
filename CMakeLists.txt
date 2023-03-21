cmake_minimum_required(VERSION 3.23)
project(Wk4TcpServer)

set(CMAKE_CXX_STANDARD 14)

add_executable(Wk4TcpServer webserver.cpp examplecode.cpp httpcontext.h)
