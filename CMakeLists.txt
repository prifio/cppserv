cmake_minimum_required(VERSION 2.8)

SET(CMAKE_CXX_STANDARD 14)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fsanitize=leak -g")

project(serv)
add_executable(${PROJECT_NAME} "main.cpp" "background_worker.cpp" "myserver.cpp" "all_consts.h")
