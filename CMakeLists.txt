cmake_minimum_required(VERSION 3.10)
project(ipstat C)
set(CMAKE_C_STANDARD 11)
include_directories(include)
add_library(avl avl.c)
add_library(capture capture.c)
add_library(server server.c)

add_executable(daemon
        main.c
)
target_link_libraries(daemon
        avl
        capture
        server
        pthread
)
add_executable(cli
       cli.c
)
target_link_libraries(cli pthread)
if(CMAKE_COMPILER_IS_GNUCC)
    target_compile_options(daemon PRIVATE -Wall -Wextra -O2)
    target_compile_options(cli    PRIVATE -Wall -Wextra -O2)
endif()
