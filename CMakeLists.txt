cmake_minimum_required(VERSION 3.16)
project(ThreadContentionVisualizer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(thread_contention_visualizer
    src/main.cpp
)

if (MSVC)
    target_compile_options(thread_contention_visualizer PRIVATE /W4)
else()
    target_compile_options(thread_contention_visualizer PRIVATE -Wall -Wextra -Wpedantic)
endif()
