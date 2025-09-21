// NOTE: this can probably be part of `itu_common.hpp`
//       BUT we don't want to break all the old exercises

// NOTE: we technically don't need include guards since we are mostly doing unity builds, but better safe than sorry
//       review this if we start using precompiler headers
#ifndef ITU_LIB_ENGINE_HPP
#define ITU_LIB_ENGINE_HPP

#include <SDL3/SDL.h>
#include <stb_image.h>
#include <itu_common.hpp>

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
};

void camera_set_active(SDLContext* context, Camera* camera);
SDL_FRect rect_global_to_screen(SDLContext* context, SDL_FRect rect);
vec2f point_global_to_screen(SDLContext* context,vec2f p);
vec2f point_screen_to_global(SDLContext* context, vec2f p);
void sdl_input_clear(SDLContext* context);
void sdl_input_key_process(SDLContext* context, BtnType button_id, SDL_Event* event);
SDL_Texture* texture_create(SDLContext* context, const char* path, SDL_ScaleMode mode);
void sdl_set_render_draw_color(SDLContext* context, color c);
void sdl_set_texture_tint(SDL_Texture* texture, color c);

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

SDL_Texture* texture_create(SDLContext* context, const char* path, SDL_ScaleMode mode)
{
	// texture could accept which pixel format it has as parameter, but this for now seems good enough
	const SDL_PixelFormat pixel_format = SDL_PIXELFORMAT_ABGR8888;

	// number of parameters is determined by the pixel format. If that is allowed to change in the future,
	// we will need to acquire the correct one through some kind of mapping
	const int num_components_requested = 4;

	int w=0, h=0, n=0;
	unsigned char* pixels = stbi_load(path, &w, &h, &n, num_components_requested);
	
	// TODO how do we recover from inability to load the asset? Do we want to?
	SDL_assert(pixels);

	SDL_Surface* surface = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_ABGR8888, pixels, w * num_components_requested);

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

#endif // ITU_LIB_ENGINE_IMPLEMENTATION

#endif // ITU_LIB_ENGINE_HPP