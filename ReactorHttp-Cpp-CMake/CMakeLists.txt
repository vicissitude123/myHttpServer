cmake_minimum_required(VERSION 3.15)
project(HttpWeb)

include_directories(${PROJECT_SOURCE_DIR}/common)
include_directories(${PROJECT_SOURCE_DIR}/http)
include_directories(${PROJECT_SOURCE_DIR}/reactor)
include_directories(${PROJECT_SOURCE_DIR}/tcp)
include_directories(${PROJECT_SOURCE_DIR}/thread)

add_subdirectory(common)
add_subdirectory(http)
add_subdirectory(reactor)
add_subdirectory(tcp)
add_subdirectory(thread)

link_libraries(common http reactor tcp thread)
add_executable(server main.cpp)
