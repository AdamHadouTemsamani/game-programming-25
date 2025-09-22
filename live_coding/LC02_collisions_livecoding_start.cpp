#define STB_IMAGE_IMPLEMENTATION

#define ITU_UNITY_BUILD

#include <SDL3/SDL.h>
#include <stb_image.h>

#include <itu_common.hpp>
#include <itu_lib_render.hpp>
#include <itu_lib_overlaps.hpp>

#define ENABLE_DIAGNOSTICS

#define TARGET_FRAMERATE SECONDS(1) / 60
#define WINDOW_W 1920
#define WINDOW_H 1080

#define ENTITY_COUNT 128

bool DEBUG_separate_collisions   = true;
bool DEBUG_render_colliders      = true;
bool DEBUG_render_texture_border = true;

struct Entity;
struct SDLContext
{
	SDL_Renderer* renderer;
	float zoom;     // render zoom
	float window_w;	// current window width after render zoom has been applied
	float window_h;	// current window width after render zoom has been applied

	float delta;    // in seconds
	float uptime;   // in seconds

	bool btn_isdown_up;
	bool btn_isdown_down;
	bool btn_isdown_left;
	bool btn_isdown_right;
	bool btn_isdown_space;
};

struct GameState
{
	Entity* player;

	// game-allocated memory
	Entity* entities;
	int entities_alive_count;

	// SDL-allocated structures
	SDL_Texture* atlas;
};

static SDL_Texture* texture_create(SDLContext* context, const char* path)
{
	int w=0, h=0, n=0;
	unsigned char* pixels = stbi_load(path, &w, &h, &n, 0);
	SDL_Surface* surface = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_ABGR8888, pixels, w * n);

	SDL_Texture* ret = SDL_CreateTextureFromSurface(context->renderer, surface);

	SDL_DestroySurface(surface);
	stbi_image_free(pixels);

	return ret;
}

// ********************************************************************************************************************
// sprite
// ********************************************************************************************************************

struct Sprite
{
	SDL_Texture* texture;
	SDL_FRect    rect;
	color        tint;
	vec2f        pivot;
};

static void sprite_render(SDLContext* context, vec2f position, vec2f size, Sprite* sprite)
{
	SDL_FRect dst_rect;
	dst_rect.w = size.x;
	dst_rect.h = size.y;
	dst_rect.x = position.x - dst_rect.w * sprite->pivot.x;
	dst_rect.y = position.y - dst_rect.h * sprite->pivot.y;
		
	SDL_SetTextureColorModFloat(sprite->texture, sprite->tint.r, sprite->tint.g, sprite->tint.b);
	SDL_SetTextureAlphaModFloat(sprite->texture, sprite->tint.a);
	SDL_RenderTexture(context->renderer, sprite->texture, &sprite->rect, &dst_rect);

	if(DEBUG_render_texture_border)
	{
		SDL_SetRenderDrawColorFloat(context->renderer, 1, 1, 1, 1);
		SDL_RenderRect(context->renderer, &dst_rect);
	}
}

// ********************************************************************************************************************
// entity
// ********************************************************************************************************************

struct Entity
{
	vec2f position;
	vec2f size;

	Sprite sprite;

	// collider data
	float collider_radius;
	vec2f collider_offset;
};

static Entity* entity_create(GameState* state)
{
	if(!(state->entities_alive_count < ENTITY_COUNT))
		return NULL;

	// // concise version
	//return &state->entities[state->entities_alive_count++];

	Entity* ret = &state->entities[state->entities_alive_count];
	++state->entities_alive_count;

	return ret;
}

// NOTE: this only works if nobody holds references to other entities!
static void entity_destroy(GameState* state, Entity* entity)
{
	// NOTE: here we want to fail hard, nobody should pass us a pointer not gotten from `entity_create()`
	SDL_assert(entity < state->entities || entity > state->entities + ENTITY_COUNT);

	--state->entities_alive_count;
	*entity = state->entities[state->entities_alive_count];
}
// ********************************************************************************************************************
// collisions
// ********************************************************************************************************************

static void collision_check(GameState* state)
{
	for(int i = 0; i < state->entities_alive_count; ++i)
	{
		Entity* e1 = &state->entities[i];
		for(int j = i + 1; j < state->entities_alive_count; ++j)
		{
			Entity* e2 = &state->entities[j];
			if(itu_lib_overlaps_circle_circle(
				e1->position + e1->collider_offset, e1->collider_radius,
				e2->position + e2->collider_offset, e2->collider_radius
			))
			{
				e1->sprite.tint = COLOR_RED;
				e2->sprite.tint = COLOR_RED;
			}
		}
	}
}

static void collision_separate(GameState* state)
{
	// TODO
}

// ********************************************************************************************************************
// game
// ********************************************************************************************************************

static void game_init(SDLContext* context, GameState* state)
{
	// contiguous memory
	{
		state->entities = (Entity*)SDL_malloc(ENTITY_COUNT * sizeof(Entity));
		SDL_assert(state->entities);
	}

	// texture atlases
	state->atlas = texture_create(context, "data/kenney/simpleSpace_tilesheet_2.png");

}

static void game_reset(SDLContext* context, GameState* state)
{
	SDL_memset(state->entities, 0, ENTITY_COUNT * sizeof(Entity));
	state->entities_alive_count = 0;

	const float sprite_size = 64;

	// entities
	Entity* player = entity_create(state);
	// we always have a player. This should also always be the first entity created, so it should never fail
	SDL_assert(player);
	player->position.x = (float)context->window_w / 2;
	player->position.y = (float)context->window_h / 2;
	player->size = vec2f{ 64, 64 };
	player->sprite = {
		.texture = state->atlas,
		.rect = SDL_FRect{ 0, 0, 128, 128 },
		.tint = COLOR_WHITE,
		.pivot = vec2f{ 0.5f, 0.5f }
	};
	player->collider_radius = 16;
	player->collider_offset = vec2f { 0, 8 };

	state->player = player;

	// box-inside-box pattern for testing most corner collisions
	// XXXXXXXXXXXXXXXX
	// X              X
	// X XXX          X
	// X XXX          X
	// X XXX          X
	// X              X
	// X              X
	// XXXXXXXXXXXXXXXX
	int num_rows = 8;
	int num_cols = 12;
	for(int i = 0; i < num_cols; ++i)
		for(int j = 0; j < num_rows; ++j)
		{
			bool condition_out = i == 0 || i == num_cols - 1 || j == 0 || j == num_rows - 1;
			bool condition_in = i >= 2 && i <= 4 && j >= 2 && j <= 4;
			if(!(condition_out || condition_in))
				continue;

			Entity* obstacle = entity_create(state);

			if(!obstacle)
			{
				SDL_Log("too many entityties spawned!");
				break;
			}

			obstacle->position.x = (float)context->window_w / 2 + sprite_size*(i - num_cols / 2 + 0.5f);
			obstacle->position.y = (float)context->window_h / 2 + sprite_size*(j - num_rows / 2 + 0.5f);
			obstacle->size = vec2f{ 64, 64 };
			obstacle->sprite = {
				.texture = state->atlas,
				.rect = SDL_FRect{ 0, 4*128, 128, 128 },
				.tint = COLOR_WHITE,
				.pivot = vec2f{ 0.5f, 0.5f }
			};
			obstacle->collider_radius = 24;
		}
}

static void game_update(SDLContext* context, GameState* state)
{
	vec2f mov = { 0 };
	if(context->btn_isdown_up)
		mov.y -= 1;
	if(context->btn_isdown_down)
		mov.y += 1;
	if(context->btn_isdown_left)
		mov.x -= 1;
	if(context->btn_isdown_right)
		mov.x += 1;

	vec2f velocity = normalize(mov) * (128 * context->delta);
	state->player->position = state->player->position + velocity;

	// reset entity tint
	for(int i = 0; i < state->entities_alive_count; ++i)
		state->entities[i].sprite.tint = COLOR_WHITE;

	// collision detection
	collision_check(state);
	if(DEBUG_separate_collisions)
		collision_separate(state);
}

static void game_render(SDLContext* context, GameState* state)
{
	// render
	for(int i = 0; i < state->entities_alive_count; ++i)
	{
		Entity* entity = &state->entities[i];
		sprite_render(context, entity->position, entity->size, &entity->sprite);

		if(DEBUG_render_colliders)
			itu_lib_render_draw_circle(
				context->renderer,
				entity->position + entity->collider_offset,
				entity->collider_radius,
				8,
				COLOR_GREEN
			);
	}
		
	// debug window
	SDL_SetRenderDrawColor(context->renderer, 0xFF, 0x00, 0xFF, 0xff);
	SDL_RenderRect(context->renderer, NULL);
}

int main(void)
{
	bool quit = false;
	SDL_Window* window;
	SDLContext context = { 0 };
	GameState  state   = { 0 };

	context.window_w = WINDOW_W;
	context.window_h = WINDOW_H;

	SDL_CreateWindowAndRenderer("E02 - Collisions", context.window_w, context.window_h, 0, &window, &context.renderer);
	SDL_SetRenderDrawBlendMode(context.renderer, SDL_BLENDMODE_BLEND);
	
	// increase the zoom to make debug text more legible
	// (ie, on the class projector, we will usually use 2)
	{
		float zoom = 2;
		context.window_w /= zoom;
		context.window_h /= zoom;
		SDL_SetRenderScale(context.renderer, zoom, zoom);
	}

	game_init(&context, &state);
	game_reset(&context, &state);

	SDL_Time walltime_frame_beg;
	SDL_Time walltime_frame_end;
	SDL_Time walltime_work_end;
	SDL_Time elapsed_work;
	SDL_Time elapsed_frame;

	SDL_GetCurrentTime(&walltime_frame_beg);
	walltime_frame_end = walltime_frame_beg;

	while(!quit)
	{
		// input
		SDL_Event event;
		while(SDL_PollEvent(&event))
		{
			switch(event.type)
			{
				case SDL_EVENT_QUIT:
					quit = true;
					break;
					
				case SDL_EVENT_KEY_DOWN:
				case SDL_EVENT_KEY_UP:
					switch(event.key.key)
					{
						case SDLK_W: context.btn_isdown_up    = event.key.down; break;
						case SDLK_A: context.btn_isdown_left  = event.key.down; break;
						case SDLK_S: context.btn_isdown_down  = event.key.down; break;
						case SDLK_D: context.btn_isdown_right = event.key.down; break;
						case SDLK_SPACE: context.btn_isdown_space = event.key.down; break;
					}

					// debug keys
					if(event.key.down && !event.key.repeat)
					{
						switch(event.key.key)
						{
							case SDLK_TAB: game_reset(&context, &state); break;
							case SDLK_F1: DEBUG_separate_collisions   = !DEBUG_separate_collisions;   break;
							case SDLK_F2: DEBUG_render_colliders      = !DEBUG_render_colliders;      break;
							case SDLK_F3: DEBUG_render_texture_border = !DEBUG_render_texture_border; break;
						}
					}
					break;
			}
		}

		SDL_SetRenderDrawColor(context.renderer, 0x00, 0x00, 0x00, 0x00);
		SDL_RenderClear(context.renderer);

		// update
		game_update(&context, &state);
		game_render(&context, &state);

		SDL_GetCurrentTime(&walltime_work_end);
		elapsed_work = walltime_work_end - walltime_frame_beg;

		if(elapsed_work < TARGET_FRAMERATE)
			SDL_DelayPrecise(TARGET_FRAMERATE - elapsed_work);
		SDL_GetCurrentTime(&walltime_frame_end);
		elapsed_frame = walltime_frame_end - walltime_frame_beg;
		

#ifdef ENABLE_DIAGNOSTICS
		{
			SDL_SetRenderDrawColor(context.renderer, 0x0, 0x00, 0x00, 0xCC);
			SDL_FRect rect = SDL_FRect{ 5, 5, 225, 65 };
			SDL_RenderFillRect(context.renderer, &rect);
			SDL_SetRenderDrawColor(context.renderer, 0xFF, 0xFF, 0xFF, 0xFF);
			SDL_RenderDebugTextFormat(context.renderer, 10, 10, "work: %9.6f ms/f", (float)elapsed_work  / (float)MILLIS(1));
			SDL_RenderDebugTextFormat(context.renderer, 10, 20, "tot : %9.6f ms/f", (float)elapsed_frame / (float)MILLIS(1));
			SDL_RenderDebugTextFormat(context.renderer, 10, 30, "[TAB] reset ");
			SDL_RenderDebugTextFormat(context.renderer, 10, 40, "[F1]  collisions        %s", DEBUG_separate_collisions   ? " ON" : "OFF");
			SDL_RenderDebugTextFormat(context.renderer, 10, 50, "[F2]  render colliders  %s", DEBUG_render_colliders      ? " ON" : "OFF");
			SDL_RenderDebugTextFormat(context.renderer, 10, 60, "[F3]  render tex border %s", DEBUG_render_texture_border ? " ON" : "OFF");
		}
#endif

		// render
		SDL_RenderPresent(context.renderer);

		context.delta = (float)elapsed_frame / (float)SECONDS(1);
		context.uptime += context.delta;
		walltime_frame_beg = walltime_frame_end;
	}
}
