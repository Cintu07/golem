cmake_minimum_required(VERSION 3.25)
project(golem LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GOLEM_BUILD_TESTS "Build tests" ON)
option(GOLEM_BUILD_BENCHMARKS "Build benchmarks" OFF)

add_library(golem INTERFACE)
target_include_directories(golem INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(MSVC)
    target_compile_options(golem INTERFACE /W4 /permissive-)
else()
    target_compile_options(golem INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
endif()

if(GOLEM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(GOLEM_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()
