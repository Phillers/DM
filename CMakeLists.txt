cmake_minimum_required(VERSION 3.3)
project(DM)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES main.cpp DM.cpp DM.h)
add_executable(DM ${SOURCE_FILES})
target_link_libraries(DM -lzmq -pthread)