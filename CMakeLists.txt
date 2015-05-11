cmake_minimum_required(VERSION 2.4)

include_directories(hdr)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNDEBUG -O3 -march=native -Wall -Wextra -pedantic -Wsign-conversion -std=c++11")

#add_definitions(-DMSLICE_USE_MMAP)

add_executable(test-speed test/speed.cpp)
add_executable(test-regression test/regression.cpp)

target_link_libraries(test-speed -pthread)
