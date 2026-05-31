cmake_minimum_required(VERSION 3.30)
project(qumir)
enable_testing()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_SERVICE "Build service component with externals" OFF)

add_subdirectory(qumir)
if(PROJECT_IS_TOP_LEVEL)
    add_subdirectory(test)
endif()
add_subdirectory(bin)

if(BUILD_SERVICE)
    add_subdirectory(externals)
    add_subdirectory(service)
endif()