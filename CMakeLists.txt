cmake_minimum_required(VERSION 3.16)
project(problem_064 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(code
    src/main.cpp
    src/printf.hpp
)

target_include_directories(code PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

