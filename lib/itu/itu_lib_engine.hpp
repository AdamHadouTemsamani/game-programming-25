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

struct Camera
{
	vec2f position;
	vec2f size;
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

	Camera camera;
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

inline SDL_FRect rect_global_to_screen(Camera* camera, SDL_FRect rect)
{
	vec2f pos  = vec2f{ rect.x, rect.y };
	vec2f size = vec2f{ rect.w, rect.h };

	pos = pos - camera->position;
	pos = pos * camera->zoom;
	pos = pos + camera->size / 2;
	pos.y = camera->size.y - pos.y - size.y * camera->zoom;
	
	SDL_FRect ret;
	ret.w = camera->pixels_per_unit * size.x * camera->zoom;
	ret.h = camera->pixels_per_unit * size.y * camera->zoom;
	ret.x = camera->pixels_per_unit * pos.x;
	ret.y = camera->pixels_per_unit * pos.y;

	return ret;
}

inline vec2f point_global_to_screen(Camera* camera, vec2f p)
{
	vec2f ret = p;
	ret = ret - camera->position;
	ret = ret * camera->zoom;
	ret = ret + camera->size / 2;
	ret.y = camera->size.y - ret.y;
	ret = ret * camera->pixels_per_unit;

	return ret;
}

inline vec2f point_screen_to_global(Camera* camera, vec2f p)
{
	vec2f ret = p;
	ret = ret / camera->pixels_per_unit;
	ret.y = camera->size.y - ret.y;
	ret = ret - camera->size / 2;
	ret = ret / camera->zoom;
	ret = ret + camera->position;

	return ret;
}

void sdl_input_clear(SDLContext* context)
{
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

static SDL_Texture* texture_create(SDLContext* context, const char* path, SDL_ScaleMode mode)
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

inline void sdl_set_render_draw_color(SDLContext* context, color c)
{
	SDL_SetRenderDrawColorFloat(context->renderer, c.r, c.g, c.b, c.a);
}

inline void sdl_set_texture_tint(SDL_Texture* texture, color c)
{
	SDL_SetTextureColorModFloat(texture, c.r, c.g, c.b);
	SDL_SetTextureAlphaModFloat(texture, c.a);
}

#endif // ITU_LIB_ENGINE_HPP

#if (defined ITU_LIB_ENGINE_IMPLEMENTATION) || (defined ITU_UNITY_BUILD)



#endif // ITU_LIB_ENGINE_IMPLEMENTATION