cmake_minimum_required(VERSION 3.0)
project(LSPaintBoard VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message("Building LSPaintBoard")

message("Setting linker flags to -static")
set(CMAKE_EXE_LINKER_FLAGS "-static")

find_package(cpr REQUIRED)

add_executable(LSPaintBoard LSPaintBoard.cpp)

message("Building python scripts")

set(PYTHON_SCRIPT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/python)

execute_process(
    COMMAND pyinstaller -y LSPaint.spec
    WORKING_DIRECTORY ${PYTHON_SCRIPT_DIR}
    RESULT_VARIABLE pyinstaller_result
)

if(NOT pyinstaller_result EQUAL 0)
    message(FATAL_ERROR "pyinstaller failed with error code: ${pyinstaller_result}")
endif()

message("LSPaint.exe built successfully")

set(PYTHON_SCRIPT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/python)

execute_process(
    COMMAND pyinstaller -y png_to_rgb.spec
    WORKING_DIRECTORY ${PYTHON_SCRIPT_DIR}
    RESULT_VARIABLE pyinstaller_result
)

if(NOT pyinstaller_result EQUAL 0)
    message(FATAL_ERROR "pyinstaller failed with error code: ${pyinstaller_result}")
endif()

message("png_to_rgb.exe built successfully")

set(PYTHON_SCRIPT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/python)

execute_process(
    COMMAND pyinstaller -y rgb_to_png.spec
    WORKING_DIRECTORY ${PYTHON_SCRIPT_DIR}
    RESULT_VARIABLE pyinstaller_result
)

if(NOT pyinstaller_result EQUAL 0)
    message(FATAL_ERROR "pyinstaller failed with error code: ${pyinstaller_result}")
endif()

message("rgb_to_png.exe built successfully")

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/python/dist/LSPaint.exe DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/python/dist/png_to_rgb.exe DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/python/dist/rgb_to_png.exe DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(LSPaintBoard PRIVATE cpr::cpr)
target_link_libraries(LSPaintBoard PRIVATE wsock32 ws2_32)

message("Building LSPaintBoard done")
