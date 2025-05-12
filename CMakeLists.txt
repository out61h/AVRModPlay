cmake_minimum_required(VERSION 3.19)

project(AvrModPlay)

add_library(${PROJECT_NAME} INTERFACE)
target_include_directories(${PROJECT_NAME} INTERFACE src)

enable_testing()
add_subdirectory(test)
