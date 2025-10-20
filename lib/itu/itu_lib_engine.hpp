// NOTE: this can probably be part of `itu_common.hpp`
//       BUT we don't want to break all the old exercises

// NOTE: we technically don't need include guards since we are mostly doing unity builds, but better safe than sorry
//       review this if we start using precompiler headers
#ifndef ITU_LIB_ENGINE_HPP
#define ITU_LIB_ENGINE_HPP

#ifndef ITU_UNITY_BUILD
#include <SDL3/SDL.h>
#include <stb_ds.h>
#include <stb_image.h>
#include <itu_common.hpp>
#include <imgui/imgui.h>
#endif


enum BtnType
{
	BTN_TYPE_UP,
	BTN_TYPE_DOWN,
	BTN_TYPE_LEFT,
	BTN_TYPE_RIGHT,
	BTN_TYPE_ACTION_0,
	BTN_TYPE_ACTION_1,
	BTN_TYPE_SPACE,

	BTN_TYPE_MAX
};

struct Transform
{
	vec2f position;
	vec2f scale;
	float rotation;
};

struct SDLContext;

struct Camera
{
	vec2f world_position; // world position
	vec2f normalized_screen_size;     // NORMALIZED size   (inside the screen rect)
	vec2f normalized_screen_offset;   // NORMALIZED offset (inside the screen rect)
	float zoom;
	float pixels_per_unit;
};

struct SDLContext
{
	SDL_Renderer* renderer;
	float zoom;     // render zoom
	float window_w;	// current window width after render zoom has been applied
	float window_h;	// current window width after render zoom has been applied

	float delta;    // in seconds
	float uptime;   // in seconds

	SDL_Time elapsed_frame; // high precision timer
	SDL_Time accumulator_physics;
	int physics_steps_count;

	Camera* camera_active;
	Camera camera_default; // default camera

	union
	{
		bool btn_isdown[BTN_TYPE_MAX];
		struct
		{
			bool btn_isdown_up;
			bool btn_isdown_down;
			bool btn_isdown_left;
			bool btn_isdown_right;
			bool btn_isdown_action0;
			bool btn_isdown_action1;
			bool btn_isdown_space;
		};
	};

	union
	{
		bool btn_isjustpressed[BTN_TYPE_MAX];
		struct
		{
			bool btn_isjustpressed_up;
			bool btn_isjustpressed_down;
			bool btn_isjustpressed_left;
			bool btn_isjustpressed_right;
			bool btn_isjustpressed_action0;
			bool btn_isjustpressed_action1;
			bool btn_isjustpressed_space;
		};
	};
	vec2f mouse_pos;
	float mouse_scroll;

	stbds_hm(SDL_Keycode, BtnType) mappings_keyboard;
	stbds_hm(Uint8, BtnType)       mappings_mouse;
};

#define TRANSFORM_DEFAULT Transform { { 0, 0 }, { 1, 1 }, 0 }

void camera_set_active(SDLContext* context, Camera* camera);
SDL_FRect rect_global_to_screen(SDLContext* context, SDL_FRect rect);
vec2f point_global_to_screen(SDLContext* context, vec2f p);
vec2f point_screen_to_global(SDLContext* context, vec2f p);
vec2f point_screen_to_window(SDLContext* context, vec2f p);
vec2f point_window_to_screen(SDLContext* context, vec2f p);
void sdl_input_clear(SDLContext* context);
void sdl_input_key_process(SDLContext* context, BtnType button_id, SDL_Event* event);
SDL_Texture* texture_create(SDLContext* context, const char* path, SDL_ScaleMode mode);
void sdl_set_render_draw_color(SDLContext* context, color c);
void sdl_set_texture_tint(SDL_Texture* texture, color c);

#endif // ITU_LIB_ENGINE_HPP

#if (defined ITU_LIB_ENGINE_IMPLEMENTATION) || (defined ITU_UNITY_BUILD)

void camera_set_active(SDLContext* context, Camera* camera)
{
	context->camera_active = camera;

	SDL_Rect rect;
	rect.w = context->window_w * camera->normalized_screen_size.x;
	rect.h = context->window_h * camera->normalized_screen_size.y;
	rect.x = context->window_w * camera->normalized_screen_offset.x;
	rect.y = context->window_h * camera->normalized_screen_offset.y;

	SDL_SetRenderViewport(context->renderer, &rect);
}

SDL_FRect camera_get_viewport_rect(SDLContext* context, Camera* camera)
{
	SDL_FRect rect;
	rect.w = context->window_w * camera->normalized_screen_size.x;
	rect.h = 0.99f * context->window_h * camera->normalized_screen_size.y;
	rect.x = 0.99f * context->window_w * camera->normalized_screen_offset.x;
	rect.y = 0.99f * context->window_h * camera->normalized_screen_offset.y;

	return rect;
}

// converts the given rect to the viewport of the given camera
SDL_FRect rect_global_to_screen(SDLContext* context, SDL_FRect rect)
{
	SDL_assert(context);
	Camera* camera = context->camera_active;

	SDL_assert(camera);

	vec2f camera_size;
	camera_size.x = (context->window_w / camera->pixels_per_unit) * camera->normalized_screen_size.x;
	camera_size.y = (context->window_h / camera->pixels_per_unit) * camera->normalized_screen_size.y;

	vec2f camera_offset;
	camera_offset.x = (context->window_w / camera->pixels_per_unit)* camera->normalized_screen_offset.x;
	camera_offset.y = (context->window_h / camera->pixels_per_unit)* camera->normalized_screen_offset.y;

	vec2f pos  = vec2f{ rect.x, rect.y };
	vec2f size = vec2f{ rect.w, rect.h };

	pos = pos - camera->world_position;
	pos = pos * camera->zoom;
	// pos = pos + camera->normalized_screen_size / 2;
	// pos.y = camera->normalized_screen_size.y - pos.y - size.y * camera->zoom;
	pos = pos + camera_size / 2;
	pos.y = camera_size.y - pos.y - size.y * camera->zoom;
	
	SDL_FRect ret;
	ret.w = camera->pixels_per_unit * size.x * camera->zoom;
	ret.h = camera->pixels_per_unit * size.y * camera->zoom;
	// ret.x = camera->pixels_per_unit * pos.x;
	// ret.y = camera->pixels_per_unit * pos.y;
	ret.x = camera->pixels_per_unit * pos.x + camera_offset.x;
	ret.y = camera->pixels_per_unit * pos.y + camera_offset.y;

	return ret;
}


float size_global_to_screen(SDLContext* context, float size)
{
	SDL_assert(context);
	Camera* camera = context->camera_active;

	SDL_assert(camera);

	return size * camera->pixels_per_unit * camera->zoom;
}

// converts the given point to the viewport of the given camera
vec2f point_global_to_screen(SDLContext* context,vec2f p)
{
	SDL_assert(context);
	Camera* camera = context->camera_active;

	SDL_assert(camera);

	vec2f camera_size;
	camera_size.x = (context->window_w / camera->pixels_per_unit) * camera->normalized_screen_size.x;
	camera_size.y = (context->window_h / camera->pixels_per_unit) * camera->normalized_screen_size.y;
	
	vec2f camera_offset;
	camera_offset.x = (context->window_w / camera->pixels_per_unit)* camera->normalized_screen_offset.x;
	camera_offset.y = (context->window_h / camera->pixels_per_unit)* camera->normalized_screen_offset.y;

	vec2f ret = p;
	ret = ret - camera->world_position;
	ret = ret * camera->zoom;
	ret = ret + camera_size / 2;
	ret.y = camera_size.y - ret.y;
	ret = ret * camera->pixels_per_unit + camera_offset;

	return ret;
}

// converts the given point from the viewport of the given camera to world space
vec2f point_screen_to_global(SDLContext* context, vec2f p)
{
	SDL_assert(context);
	Camera* camera = context->camera_active;

	SDL_assert(camera);

	vec2f camera_size;
	camera_size.x = (context->window_w / camera->pixels_per_unit) * camera->normalized_screen_size.x;
	camera_size.y = (context->window_h / camera->pixels_per_unit) * camera->normalized_screen_size.y;

	vec2f camera_offset;
	camera_offset.x = (context->window_w / camera->pixels_per_unit)* camera->normalized_screen_offset.x;
	camera_offset.y = (context->window_h / camera->pixels_per_unit)* camera->normalized_screen_offset.y;

	vec2f ret = p;
	ret = ret / camera->pixels_per_unit;
	ret.y = camera_size.y - ret.y;
	ret = ret - camera_offset;
	ret = ret - camera_size / 2;
	ret = ret / camera->zoom;
	ret = ret + camera->world_position;

	return ret;
}

vec2f point_screen_to_window(SDLContext* context, vec2f p)
{
	vec2f ret = p + mul_element_wise(context->camera_active->normalized_screen_offset, vec2f { WINDOW_W, WINDOW_H});
	return ret;
}

vec2f point_window_to_screen(SDLContext* context, vec2f p)
{
	vec2f ret = p - mul_element_wise(context->camera_active->normalized_screen_offset, vec2f { WINDOW_W, WINDOW_H});
	return ret;
}

void sdl_input_set_mapping_keyboard(SDLContext* context, SDL_Keycode key, BtnType input)
{
	stbds_hmput(context->mappings_keyboard, key, input);
}

void sdl_input_set_mapping_mouse(SDLContext* context, Uint8 key, BtnType input)
{
	stbds_hmput(context->mappings_mouse, key, input);
}

void sdl_input_clear(SDLContext* context)
{
	context->mouse_scroll = 0;

	for(int i = 0; i < BTN_TYPE_MAX; ++i)
		context->btn_isjustpressed[i] = false;
}

// Auxiliary function to process low-frequency inputs.
// May drop inputs that are firing higher than the current framerate
void sdl_input_key_process(SDLContext* context, BtnType button_id, SDL_Event* event)
{
	context->btn_isdown[button_id] = event->key.down;
	context->btn_isjustpressed[button_id] = event->key.down && !event->key.repeat;
}

// Auxiliary function to process low-frequency inputs.
// May drop inputs that are firing higher than the current framerate
void sdl_input_mouse_button_process(SDLContext* context, BtnType button_id, SDL_Event* event)
{
	context->btn_isjustpressed[button_id] = event->button.down && !context->btn_isjustpressed[button_id];
	context->btn_isdown[button_id] = event->button.down;
}


// forward declaration
bool itu_lib_imgui_process_sdl_event(SDL_Event* event);

bool sdl_process_events(SDLContext* context)
{
	// input
	bool ret = false;
	SDL_Event event;
	sdl_input_clear(context);

	while(SDL_PollEvent(&event))
	{
		if(itu_lib_imgui_process_sdl_event(&event))
			continue;
		switch(event.type)
		{
			case SDL_EVENT_QUIT:
				ret = true;
				break;
			// listen for mouse motion and store the absolute position in screen space
			case SDL_EVENT_MOUSE_MOTION:
			{
				context->mouse_pos.x = event.motion.x;
				context->mouse_pos.y = event.motion.y;
				break;
			}
			// listen for mouse wheel and store the relative position in screen space
			case SDL_EVENT_MOUSE_WHEEL:
			{
				context->mouse_scroll = event.wheel.y;
				break;
			}
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
			{
				int i = stbds_hmgeti(context->mappings_mouse, event.button.button);
				if(i != -1)
					sdl_input_mouse_button_process(context, context->mappings_mouse[i].value, &event);
				break;
			}
			case SDL_EVENT_KEY_DOWN:
			case SDL_EVENT_KEY_UP:
			{
				int i = stbds_hmgeti(context->mappings_keyboard, event.key.key);
				if(i != -1)
					sdl_input_key_process(context,  context->mappings_keyboard[i].value, &event);
				break;
			}
		}
	}

	return ret;
}

SDL_Texture* texture_create(SDLContext* context, const char* path, SDL_ScaleMode mode)
{
	// texture could accept which pixel format it has as parameter, but this for now seems good enough
	const SDL_PixelFormat pixel_format = SDL_PIXELFORMAT_RGBA32;

	// number of parameters is determined by the pixel format. If that is allowed to change in the future,
	// we will need to acquire the correct one through some kind of mapping
	const int num_components_requested = 4;

	int w=0, h=0, n=0;
	unsigned char* pixels = stbi_load(path, &w, &h, &n, num_components_requested);
	
	// TODO how do we recover from inability to load the asset? Do we want to?
	SDL_assert(pixels);

	SDL_Surface* surface = SDL_CreateSurfaceFrom(w, h, pixel_format, pixels, w * num_components_requested);

	SDL_Texture* ret = SDL_CreateTextureFromSurface(context->renderer, surface);
	SDL_SetTextureScaleMode(ret, mode);

	SDL_DestroySurface(surface);
	stbi_image_free(pixels);

	return ret;
}

void sdl_set_render_draw_color(SDLContext* context, color c)
{
	SDL_SetRenderDrawColorFloat(context->renderer, c.r, c.g, c.b, c.a);
}

void sdl_set_texture_tint(SDL_Texture* texture, color c)
{
	SDL_SetTextureColorModFloat(texture, c.r, c.g, c.b);
	SDL_SetTextureAlphaModFloat(texture, c.a);
}

void sdl_render_diagnostics(SDLContext* context, float elapsed_work, float elapsed_frame)
{
	SDL_SetRenderDrawColor(context->renderer, 0x0, 0x00, 0x00, 0xCC);
	SDL_FRect rect = SDL_FRect{ 5, 5, 145, 25 };
	SDL_RenderFillRect(context->renderer, &rect);
		 
	SDL_SetRenderDrawColor(context->renderer, 0xFF, 0xFF, 0xFF, 0xFF);
	SDL_RenderDebugTextFormat(context->renderer, 10, 10, "work: %6.3f ms/f", (float)elapsed_work  / (float)MILLIS(1));
	SDL_RenderDebugTextFormat(context->renderer, 10, 20, "tot : %6.3f ms/f", (float)elapsed_frame / (float)MILLIS(1));
}

// busy waits to introduce artificial delay
void engine_artificial_delay(float delay_ms, float delay_spread_ms)
{
	SDL_Time walltime_start;
	SDL_Time walltime_busywait;
	SDL_Time target_wait = (delay_ms + (SDL_randf() - 0.5f) * delay_spread_ms) * 1000000;

	SDL_GetCurrentTime(&walltime_start);
	walltime_busywait = walltime_start;
	while(walltime_busywait - walltime_start < target_wait)
		SDL_GetCurrentTime(&walltime_busywait);
}


#endif // ITU_LIB_ENGINE_IMPLEMENTATION
