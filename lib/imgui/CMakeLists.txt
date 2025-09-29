file(GLOB src "imgui/*.cpp")

add_library(imgui STATIC ${src})
target_include_directories(imgui PUBLIC "${CMAKE_SOURCE_DIR}/lib/SDL/include/")
target_include_directories(imgui PUBLIC ".")

# Ensure ImGui consumers also link SDL so the final link order is: imgui -> SDL

