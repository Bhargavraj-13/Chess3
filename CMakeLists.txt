# 3.25 is the first CMake version with FetchContent's SYSTEM option (see tests/CMakeLists.txt).
cmake_minimum_required(VERSION 3.25)

project(Chess3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# Defines the BUILD_TESTING option (ON by default) and turns on ctest.
include(CTest)
include(cmake/Warnings.cmake)

add_subdirectory(src/core)
add_subdirectory(src/server)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
