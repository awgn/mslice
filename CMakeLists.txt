cmake_minimum_required(VERSION 2.4)

include_directories(hdr)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNDEBUG -Ofast -march=native -Wall -Wextra -std=c++11")

#add_definitions(-DMSLICE_USE_MMAP)

add_executable(alloc-speed test/speed.cpp)

target_link_libraries(alloc-speed -pthread)
