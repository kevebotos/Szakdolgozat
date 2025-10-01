cmake_minimum_required(VERSION 3.16)
project(stove_heat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(stove_heat
    src/main.cpp
    src/mesh.cpp
    src/xs.cpp
)

target_include_directories(stove_heat PRIVATE src)

