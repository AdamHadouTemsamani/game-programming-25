cmake_minimum_required(VERSION 3.10)

project(gp25_exercises)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

FILE(COPY data DESTINATION ${CMAKE_BINARY_DIR})

# 3rd party libraries
add_subdirectory(lib/SDL)
add_subdirectory(lib/box2d)
add_subdirectory(lib/imgui)

# our libraries
include_directories(lib/stb)

# repo's executables
add_subdirectory(examples)
add_subdirectory(playground)
add_subdirectory(exercises)
add_subdirectory(exercises_solutions)
