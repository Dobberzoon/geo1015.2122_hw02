cmake_minimum_required(VERSION 3.20)
project(runoff)

set(CMAKE_CXX_STANDARD 17)

find_package(GDAL)

if (NOT GDAL_FOUND)
    message(SEND_ERROR "runoff requires the GDAL library")
endif()

include_directories(/opt/homebrew/include)

link_directories(/opt/homebrew/lib)

add_executable(runoff main.cpp)
target_link_libraries(runoff gdal)
