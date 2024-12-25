cmake_minimum_required(VERSION 3.10)

project(SimpleHTTPServer VERSION 1.0)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

add_executable(simple simple.c)

add_executable(io_uring_simple io_uring_simple.c)
target_link_libraries(io_uring_simple uring)

add_executable(io_uring_single_opt io_uring_single_opt.c)
target_link_libraries(io_uring_single_opt uring)

add_executable(io_uring_opt io_uring_opt.c)
target_link_libraries(io_uring_opt uring)

