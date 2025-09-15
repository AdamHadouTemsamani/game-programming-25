#define STB_IMAGE_IMPLEMENTATION
#define ITU_UNITY_BUILD

#include <SDL3/SDL.h>
#include <itu_lib_engine.hpp>
#include <itu_lib_render.hpp>
#include <itu_lib_sprite.hpp>

#define ENABLE_DIAGNOSTICS

#define TARGET_FRAMERATE SECONDS(1) / 60
#define WINDOW_W         800
#define WINDOW_H         600

#define ENTITY_COUNT 4096

#define PIXELS_PER_UNIT 128

bool DEBUG_render_textures = true;
bool DEBUG_render_outlines = true;

struct Entity
{
	Sprite sprite;
	Transform transform;
};

struct GameState
{
	// shortcut references
	Entity* player;

	// game-allocated memory
	Entity* entities;
	int entities_alive_count;

	// SDL-allocated structures
	SDL_Texture* atlas;
	SDL_Texture* bg;
};

static Entity* entity_create(GameState* state)
{
	if(!(state->entities_alive_count < ENTITY_COUNT))
		// NOTE: this might as well be an assert, if we don't have a way to recover/handle it
		return NULL;

	// // concise version
	//return &state->entities[state->entities_alive_count++];

	Entity* ret = &state->entities[state->entities_alive_count];
	++state->entities_alive_count;
	return ret;
}

// NOTE: this only works if nobody holds references to other entities!
//       if that were the case, we couldn't swap them around.
//       We will see in later lectures how to handle this kind of problems
static void entity_destroy(GameState* state, Entity* entity)
{
	// NOTE: here we want to fail hard, nobody should pass us a pointer not gotten from `entity_create()`
	SDL_assert(entity < state->entities ||entity > state->entities + ENTITY_COUNT);

	--state->entities_alive_count;
	*entity = state->entities[state->entities_alive_count];
}


static void game_init(SDLContext* context, GameState* state)
{
	// allocate memory
	state->entities = (Entity*)SDL_calloc(ENTITY_COUNT, sizeof(Entity));
	SDL_assert(state->entities);

	// TODO allocate space for tile info (when we'll load those from file)

	// texture atlases
	state->atlas = texture_create(context, "data/kenney/simpleSpace_tilesheet_2.png", SDL_SCALEMODE_LINEAR);
	state->bg    = texture_create(context, "data/kenney/prototype_texture_dark/texture_13.png", SDL_SCALEMODE_LINEAR);
}

static void game_reset(SDLContext* context, GameState* state)
{
	state->entities_alive_count = 0;
	// entities
	{
		Entity* bg = entity_create(state);
		SDL_FRect sprite_rect = SDL_FRect{ 0, 0, 1024, 1024};
		itu_lib_sprite_init(&bg->sprite, state->bg, sprite_rect);
		bg->transform.scale = VEC2F_ONE * 8;
	}

	{
		state->player = entity_create(state);
		state->player->transform.position = VEC2F_ZERO;
		state->player->transform.scale = VEC2F_ONE;
		itu_lib_sprite_init(
			&state->player->sprite,
			state->atlas,
			itu_lib_sprite_get_rect(0, 1, 128, 128)
		);

		// raise sprite a bit, so that the position concides with the center of the image
		state->player->sprite.pivot.y = 0.3f;
	}
}

static void game_update(SDLContext* context, GameState* state)
{
	{
		const float player_speed = 1;

		Entity* entity = state->player;
		vec2f mov = { 0 };
		if(context->btn_isdown_up)
			mov.y += 1;
		if(context->btn_isdown_down)
			mov.y -= 1;
		if(context->btn_isdown_left)
			mov.x -= 1;
		if(context->btn_isdown_right)
			mov.x += 1;
	
		entity->transform.position = entity->transform.position + mov * (player_speed * context->delta);

		// camera follows player
		context->camera.position = entity->transform.position;
	}
}

static void game_render(SDLContext* context, GameState* state)
{
	for(int i = 0; i < state->entities_alive_count; ++i)
	{
		Entity* entity = &state->entities[i];
		// render texture
		SDL_FRect rect_src = entity->sprite.rect;
		SDL_FRect rect_dst;

		if(DEBUG_render_textures)
			itu_lib_sprite_render(context, &entity->sprite, &entity->transform);

		if(DEBUG_render_outlines)
			itu_lib_sprite_render_debug(context, &entity->sprite, &entity->transform);
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

	SDL_CreateWindowAndRenderer("E03 - Coordinate Systems", WINDOW_W, WINDOW_H, 0, &window, &context.renderer);
	SDL_SetRenderDrawBlendMode(context.renderer, SDL_BLENDMODE_BLEND);

	// increase the zoom to make debug text more legible
	// (ie, on the class projector, we will usually use 2)
	{
		context.zoom = 1;
		context.window_w /= context.zoom;
		context.window_h /= context.zoom;
		SDL_SetRenderScale(context.renderer, context.zoom, context.zoom);
	}

	context.camera.size.x = context.window_w / PIXELS_PER_UNIT;
	context.camera.size.y = context.window_h / PIXELS_PER_UNIT;
	context.camera.zoom = 1;
	context.camera.pixels_per_unit = PIXELS_PER_UNIT;

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
						case SDLK_W: sdl_input_key_process(&context, BTN_TYPE_UP, &event);        break;
						case SDLK_A: sdl_input_key_process(&context, BTN_TYPE_LEFT, &event);      break;
						case SDLK_S: sdl_input_key_process(&context, BTN_TYPE_DOWN, &event);      break;
						case SDLK_D: sdl_input_key_process(&context, BTN_TYPE_RIGHT, &event);     break;
						case SDLK_Q: sdl_input_key_process(&context, BTN_TYPE_ACTION_0, &event);  break;
						case SDLK_E: sdl_input_key_process(&context, BTN_TYPE_ACTION_1, &event);  break;
						case SDLK_SPACE: sdl_input_key_process(&context, BTN_TYPE_SPACE, &event); break;
					}

					// debug keys
					if(event.key.down && !event.key.repeat)
					{
						switch(event.key.key)
						{
							case SDLK_TAB: game_reset(&context, &state); break;
							case SDLK_F1: DEBUG_render_textures = !DEBUG_render_textures; break;
							case SDLK_F2: DEBUG_render_outlines = !DEBUG_render_outlines; break;
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
			SDL_DelayNS(TARGET_FRAMERATE - elapsed_work);
		SDL_GetCurrentTime(&walltime_frame_end);
		elapsed_frame = walltime_frame_end - walltime_frame_beg;
		
#ifdef ENABLE_DIAGNOSTICS
		{
			SDL_SetRenderDrawColor(context.renderer, 0x0, 0x00, 0x00, 0xCC);
			SDL_FRect rect = SDL_FRect{ 5, 5, 225, 55 };
			SDL_RenderFillRect(context.renderer, &rect);

			SDL_SetRenderDrawColor(context.renderer, 0xFF, 0xFF, 0xFF, 0xFF);
			SDL_RenderDebugTextFormat(context.renderer, 10, 10, "work: %9.6f ms/f", (float)elapsed_work  / (float)MILLIS(1));
			SDL_RenderDebugTextFormat(context.renderer, 10, 20, "tot : %9.6f ms/f", (float)elapsed_frame / (float)MILLIS(1));
			SDL_RenderDebugTextFormat(context.renderer, 10, 30, "[TAB] reset ");
			SDL_RenderDebugTextFormat(context.renderer, 10, 40, "[F1]  render textures   %s", DEBUG_render_textures   ? " ON" : "OFF");
			SDL_RenderDebugTextFormat(context.renderer, 10, 50, "[F2]  render outlines   %s", DEBUG_render_outlines   ? " ON" : "OFF");
		}
#endif
		// render
		SDL_RenderPresent(context.renderer);

		context.delta = (float)elapsed_frame / (float)SECONDS(1);
		context.uptime += context.delta;
		walltime_frame_beg = walltime_frame_end;
	}
}
