cmake_minimum_required(VERSION 2.8)
SET(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build Type")
add_definitions("-Wall")
add_library(evdsptc STATIC
    src/evdsptc.c
)
