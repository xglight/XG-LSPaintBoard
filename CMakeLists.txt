cmake_minimum_required(VERSION 3.0)
project(LSPaintBoard VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message("Building LSPaintBoard")

message("Setting linker flags to -static")
set(CMAKE_EXE_LINKER_FLAGS "-static")

add_executable(LSPaintBoard LSPaintBoard.cpp ${CMAKE_CURRENT_SOURCE_DIR}/src/logo.rc)

# link libraries

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(LSPaintBoard PRIVATE wsock32 ws2_32)

message("Building LSPaintBoard done")
