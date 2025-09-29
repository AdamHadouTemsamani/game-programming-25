# set(imgui_src "${CMAKE_SOURCE_DIR}/lib/imgui/imgui/imgui.cpp;${CMAKE_SOURCE_DIR}/lib/imgui/imgui/imgui_draw.cpp;${CMAKE_SOURCE_DIR}/lib/imgui/imgui/imgui_impl_sdlgpu3.cpp;${CMAKE_SOURCE_DIR}/lib/imgui/imgui/imgui_impl_sdlrenderer3.cpp;${CMAKE_SOURCE_DIR}/lib/imgui/imgui/imgui_impl_sdl3.cpp;${CMAKE_SOURCE_DIR}/lib/imgui/imgui/imgui_impl_sdl3.cpp;${CMAKE_SOURCE_DIR}/lib/imgui/imgui/imgui_tables.cpp;${CMAKE_SOURCE_DIR}/lib/imgui/imgui/imgui_widgets.cpp"  CACHE INTERNAL "")
file(GLOB src "imgui/*.cpp")

add_library(imgui STATIC ${src})
target_include_directories(imgui PUBLIC "${CMAKE_SOURCE_DIR}/lib/SDL/include/")
target_include_directories(imgui PUBLIC ".")

# Ensure ImGui consumers also link SDL so the final link order is: imgui -> SDL

