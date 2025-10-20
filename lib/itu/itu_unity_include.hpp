// unity build header
// include this only once, and no other header

#define ITU_UNITY_BUILD

#define STB_DS_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <stb_ds.h>
#include <stb_image.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_sdl3.h>
#include <imgui/imgui_impl_sdlrenderer3.h>

#include <box2d/box2d.h>

#include <itu_common.hpp>
#include <itu_lib_engine.hpp>

#include <itu_entity_storage.hpp>

#include <itu_lib_render.hpp>
#include <itu_lib_overlaps.hpp>
#include <itu_lib_sprite.hpp>
#include <itu_lib_imgui.hpp>
// #include <itu_lib_box2d.hpp> // deprecated
#include <itu_sys_physics.hpp>

#include <itu_lib_debug_ui.hpp>
#include <itu_default_systems.cpp>
#include <itu_entity_storage.cpp>