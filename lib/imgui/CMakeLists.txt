file(GLOB src "imgui/*.cpp")

add_library(imgui STATIC ${src})
target_include_directories(imgui PUBLIC "${CMAKE_SOURCE_DIR}/lib/SDL/include/")
target_include_directories(imgui PUBLIC ".")