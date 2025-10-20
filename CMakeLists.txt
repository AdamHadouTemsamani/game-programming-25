cmake_minimum_required(VERSION 3.12)

project(gp25_exercises)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

FILE(COPY data DESTINATION ${CMAKE_BINARY_DIR})


set(SDLMIXER_VENDORED OFF)

# 3rd party libraries
add_subdirectory(lib/SDL)
add_subdirectory(lib/SDL_mixer)
add_subdirectory(lib/SDL_ttf)
add_subdirectory(lib/box2d)
<<<<<<< HEAD
# add_subdirectory(lib/imgui)
=======
# changed imgui to be compiled as part of the game executable
add_subdirectory(lib/imgui)
>>>>>>> upstream/main

# our libraries
include_directories(lib/stb)

# repo's executables
add_subdirectory(examples)
add_subdirectory(playground)
add_subdirectory(exercises)
add_subdirectory(exercises_solutions)
