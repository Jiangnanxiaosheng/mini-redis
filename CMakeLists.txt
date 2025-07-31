cmake_minimum_required(VERSION 3.20)
project(mini-redis CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED on)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(mini-redis 
    src/main.cpp 
    src/server.cpp 
    src/store.cpp 
    src/command.cpp 
)

# Enable warnings
target_compile_options(mini-redis PRIVATE -Wall -Wextra)