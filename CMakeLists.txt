cmake_minimum_required(VERSION 3.16)
project(szakdolgozat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(szakdolgozat
    src/main.cpp
    src/mesh.cpp
    src/xs.cpp
    src/model.cpp
)

target_include_directories(szakdolgozat PRIVATE src)
