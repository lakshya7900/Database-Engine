cmake_minimum_required(VERSION 3.20)

project(MiniDB LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(minidb
    src/main.cpp
    src/record_io.cpp
)

target_include_directories(minidb PRIVATE include)

target_compile_options(minidb PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)