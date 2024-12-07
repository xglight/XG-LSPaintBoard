cmake_minimum_required(VERSION 3.5.0)
project(LSPaintBoard VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXE_LINKER_FLAGS "-static")

find_package(cpr REQUIRED)

add_executable(LSPaintBoard LSPaintBoard.cpp)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(LSPaintBoard PRIVATE cpr::cpr)
target_link_libraries(LSPaintBoard PRIVATE wsock32 ws2_32)


