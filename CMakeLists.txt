cmake_minimum_required(VERSION 3.20)
project(MiniDB LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(minidb_core
    src/record.cpp
    src/page.cpp
    src/disk_manager.cpp
    src/table.cpp
    src/parser.cpp
)
target_include_directories(minidb_core PUBLIC include)
target_compile_options(minidb_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(minidb src/main.cpp)
target_link_libraries(minidb PRIVATE minidb_core)

add_executable(minidb_tests tests/test_main.cpp)
target_link_libraries(minidb_tests PRIVATE minidb_core)

enable_testing()
add_test(NAME minidb_tests COMMAND minidb_tests)
 
add_executable(minidb_benchmark
    benchmarks/benchmark.cpp
)

target_link_libraries(minidb_benchmark
    PRIVATE minidb_core
)

target_compile_options(minidb_benchmark PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)