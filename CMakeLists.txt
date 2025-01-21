cmake_minimum_required(VERSION 3.0)
project(LSPaintBoard VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message("Building LSPaintBoard")

message("Setting linker flags to -static")
set(CMAKE_EXE_LINKER_FLAGS "-static")

add_executable(LSPaintBoard LSPaintBoard.cpp)

set(PYTHON_SCRIPT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/python)

# Build Python
message("Building Python scripts")

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/python/dist/LSPaint.exe DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/python/dist/png_to_rgb.exe DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/python/dist/rgb_to_png.exe DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/python/dist/GetPaint.exe DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Build Python done
message("Building Python scripts done")

# link libraries

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(LSPaintBoard PRIVATE wsock32 ws2_32)

message("Building LSPaintBoard done")
