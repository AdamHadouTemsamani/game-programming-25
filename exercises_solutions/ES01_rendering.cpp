// list of (non-exausthive) changes from base exercise
// - added simple function to debug draw entities' "collision" area
// - added toggle of debug draw functionality with on F1 press
// - changed `GameState::asteroids` from array to pointer to showcase dynamic memory allocation
// - added very simple spawning logic for asteroids
// - unified function for entity spawning
// - unified function for entity rendering
// - moved "design" variables in global scope (they would belong in a config file anyway, so we can be a bit lazy with it for now)


#include <SDL3/SDL.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define ENABLE_DIAGNOSTICS

#define NUM_ASTEROIDS   4096
#define NUM_PROJECTILES 4096

#define DEBUG_CIRCLE_POINT_COUNT 8

#define NANOS(x)   (x)                // converts nanoseconds into nanoseconds
#define MICROS(x)  (NANOS(x) * 1000)  // converts microseconds into nanoseconds
#define MILLIS(x)  (MICROS(x) * 1000) // converts milliseconds into nanoseconds
#define SECONDS(x) (MILLIS(x) * 1000) // converts seconds into nanoseconds

#define NS_TO_MILLIS(x)  ((float)(x)/(float)1000000)    // converts nanoseconds to milliseconds (in floating point precision)
#define NS_TO_SECONDS(x) ((float)(x)/(float)1000000000) // converts nanoseconds to seconds (in floating point precision)

// PI*2
#define TAU 6.2831f

// debug flags
static bool DEBUG_draw_outline_collision = false;

// NOTE: these are a "design" parameter
//       it is worth specifying a proper structure for this
const float entity_size_world = 64;
const float entity_size_texture = 128;
const float player_speed = entity_size_world * 5;
const float player_collision_radius = 16;
const int   player_sprite_coords_x = 4;
const int   player_sprite_coords_y = 0;
const float asteroid_speed_min = entity_size_world * 2;
const float asteroid_speed_range = entity_size_world * 4;
const float asteroid_collision_radius = 18;
const int   asteroid_sprite_coords_x = 0;
const int   asteroid_sprite_coords_y = 4;
const float asteroid_spawning_period_base = 1.0f;
const float asteroid_spawning_period_mult = .95f;
const float asteroid_spawning_period_max  = .2f;
const float projectile_collision_radius = 8;
const int   projectile_sprite_coords_x = 7;
const int   projectile_sprite_coords_y = 3;
const float projectile_speed = entity_size_world * 6;
const float projectile_cooldow = 0.1f;

struct SDLContext
{
	SDL_Renderer* renderer;
	float window_w; // current window width after render zoom has been applied
	float window_h; // current window height after render zoom has been applied

	float delta;    // in seconds
	float uptime;   // in seconds

	bool btn_pressed_up    = false;
	bool btn_pressed_down  = false;
	bool btn_pressed_left  = false;
	bool btn_pressed_right = false;
	bool btn_pressed_space = false;
};

struct Entity
{
	// NOTE: this is the simplest (and laziest) way to do it.
    //       We would prefer the object pool to do this more efficiently,
	//       but without profiling this properly it's kinda pointless
	bool       alive;
	SDL_FPoint position;
	float      size;
	float      velocity;

	SDL_FRect    rect;
	SDL_Texture* texture_atlas;
	SDL_FRect    texture_rect;
};

struct GameState
{
	Entity player;

	// changed pools to pointers (to showcase dynamic allocation)
	Entity* asteroids;
	Entity* projectiles;

	float walltime_last_projectile_spawn;
	float walltime_last_asteroid_spawn;
	float asteroid_spawning_period;

	SDL_Texture* texture_atlas;

	bool game_over;
};

// simple function to draw debug circles
// 
// NOTE: this function is very slow (it does a lot of trig. for instance)
//       there are many ways to make this faster, any ideas?
void draw_circle(SDLContext* context, float x, float y, float radius)
{
	SDL_FPoint points[DEBUG_CIRCLE_POINT_COUNT+1];
	
	float angle_increment = TAU / DEBUG_CIRCLE_POINT_COUNT;
	for(int i = 0; i < DEBUG_CIRCLE_POINT_COUNT; ++i)
	{
		float angle = angle_increment * i;
		points[i].x = x + radius * SDL_cos(angle);
		points[i].y = y + radius * SDL_sin(angle);
	}
	points[DEBUG_CIRCLE_POINT_COUNT] = points[0];
	SDL_SetRenderDrawColor(context->renderer, 0x00, 0xFF, 0x00, 0xFF);
	SDL_RenderLines(context->renderer, points, DEBUG_CIRCLE_POINT_COUNT+1);
}

static float distance_between_sq(SDL_FPoint a, SDL_FPoint b)
{
	float dx = a.x - b.x;
	float dy = a.y - b.y;
	return dx*dx + dy*dy;
}

static float distance_between(SDL_FPoint a, SDL_FPoint b)
{
	float dx = a.x - b.x;
	float dy = a.y - b.y;
	return SDL_sqrtf(dx*dx + dy*dy);
}

// simple circle<>circle collision check
static bool entity_collision_check(Entity* e1, Entity* e2)
{
	float distance_sq = distance_between_sq(e1->position, e2->position);
	float collision_distance_sq = e1->size + e2->size;
	collision_distance_sq *= collision_distance_sq;

	return distance_sq < collision_distance_sq;
}

// spawn a new entity
// NOTE: this works even if we are passing an array (ie, the original declaration of `GameState::asteroids` and `GameState::projectiles`
//       as `Entity asteroids[NUM_ASTEROIDS]), since they are *mostly* equivalent.
Entity* entity_spawn(Entity* pool, int max_amount, float x, float y, float velocity)
{
	int new_entity_idx = 0;
	while(new_entity_idx < max_amount && pool[new_entity_idx].alive)
		++new_entity_idx;

	if(new_entity_idx == max_amount)
	{
		// NOTE: since we changed `GameState::asteroids` and `GameState::projectiles` from array of entities to pointers,
		//       in theory we could resize entity array here. It would require some additional changes to keep track of
		//       the new `max_amount` of entities in the pool (and we would need to set a ceiling anyway, we can't keep
		//       spawning new entities forever
		SDL_Log("[WARNING] too many entities, cannot spawn more!");
		return NULL;
	}

	Entity* entity = &pool[new_entity_idx];

	entity->alive = true;
	entity->position.x = x;
	entity->position.y = y;
	entity->velocity   = velocity;

	// return the newly spawned entity, so we can do additional initialization if needed
	return entity;
}

void entity_render(SDLContext* context, Entity* entity)
{
	entity->rect.x = entity->position.x - entity->rect.w / 2;
	entity->rect.y = entity->position.y - entity->rect.h / 2;

	// NOTE: we probably want to add texture tinting here

	SDL_RenderTexture(
		context->renderer,
		entity->texture_atlas,
		&entity->texture_rect,
		&entity->rect
	);

	if(DEBUG_draw_outline_collision)
		draw_circle(context, entity->position.x, entity->position.y, entity->size);
}

static void init(SDLContext* context, GameState* game_state)
{
	// reset game state
	{
		// NOTE: from `SDL_Free` documentation: "if `mem` [the parameter] is NULL, this function does nothing
		//       ie, we don't need to test for null here (like most implementations of the standard `free`).
		SDL_free(game_state->asteroids);
		SDL_free(game_state->projectiles);

		// NOTE: from `SDL_DestroyTexture`: "Passing NULL or an otherwise invalid texture will set the SDL error message to "Invalid texture""
		//       ie, we COULD avoid this test, but if you're keeping track of SDL internal messages your logs will be full "errors" that will drown
		//       the real problems in a sea of noise, so let's avoid it
		if(game_state->texture_atlas)
			SDL_DestroyTexture(game_state->texture_atlas);
	}
	// zero out memory (need to do this explicitely if we want to reset the game state)
	SDL_memset(game_state, 0, sizeof(GameState));

	// load textures
	{
		int w = 0;
		int h = 0;
		int n = 0;
		unsigned char* pixels = stbi_load("data/kenney/simpleSpace_tilesheet_2.png", &w, &h, &n, 0);

		// NOTE: for actual production code we definitely don't want to crash the game, but ATM we don't have a way to
		//       recover from this, so we might as well make it easier for us to if there are any problems here
		SDL_assert(pixels);

		// we don't really need this SDL_Surface, but it's the most conveninet way to create out texture
		// NOTE: out image has the color channels in RGBA order, but SDL_PIXELFORMAT
		//       behaves the opposite on little endina architectures (ie, most of them)
		//       we won't worry too much about that, just remember this if your textures looks wrong
		//       - check that the the color channels are actually what you expect (how many? how big? which order)
		//       - if everythig looks right, you might just need to flip the channel order, because of SDL
		SDL_Surface* surface = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_ABGR8888, pixels, w * n);
		game_state->texture_atlas = SDL_CreateTextureFromSurface(context->renderer, surface);
		
		// NOTE: the texture will make a copy of the pixel data, so after creatio we can release both surface and pixel data
		SDL_DestroySurface(surface);
		stbi_image_free(pixels);
	}

	// game state
	{
		game_state->asteroids = (Entity*)SDL_calloc(NUM_ASTEROIDS, sizeof(Entity));
		game_state->projectiles = (Entity*)SDL_calloc(NUM_PROJECTILES, sizeof(Entity));
		game_state->asteroid_spawning_period = asteroid_spawning_period_base;
	}
	// player
	{
		game_state->player.alive = true;
		game_state->player.position.x = context->window_w / 2 - entity_size_world / 2;
		game_state->player.position.y = context->window_h - entity_size_world * 2;
		game_state->player.size = player_collision_radius;
		game_state->player.velocity = player_speed;
		game_state->player.texture_atlas = game_state->texture_atlas;

		// player size in the game world
		game_state->player.rect.w = entity_size_world;
		game_state->player.rect.h = entity_size_world;

		// sprite size (in the tilemap)
		game_state->player.texture_rect.w = entity_size_texture;
		game_state->player.texture_rect.h = entity_size_texture;
		// sprite position (in the tilemap)
		game_state->player.texture_rect.x = entity_size_texture * player_sprite_coords_x;
		game_state->player.texture_rect.y = entity_size_texture * player_sprite_coords_y;
	}

	// asteroids
	{
		for(int i = 0; i < NUM_ASTEROIDS; ++i)
		{
			Entity* asteroid_curr = &game_state->asteroids[i];

			// // NOTE: we are splitting asteroid's initialiation from spawning,
			// //       so we don't need to set up spawning properties here

			asteroid_curr->size = asteroid_collision_radius;
			asteroid_curr->rect.w = entity_size_world;
			asteroid_curr->rect.h = entity_size_world;
			asteroid_curr->texture_atlas = game_state->texture_atlas;
			asteroid_curr->texture_rect.w = entity_size_texture;
			asteroid_curr->texture_rect.h = entity_size_texture;
			asteroid_curr->texture_rect.x = entity_size_texture * asteroid_sprite_coords_x;
			asteroid_curr->texture_rect.y = entity_size_texture * asteroid_sprite_coords_y;
		}
	}

	// projectiles
	{
		for(int i = 0; i < NUM_PROJECTILES; ++i)
		{
			Entity* projectile_curr = &game_state->projectiles[i];
			
			projectile_curr->size = projectile_collision_radius;
			projectile_curr->rect.w = entity_size_world;
			projectile_curr->rect.h = entity_size_world;
			projectile_curr->texture_atlas = game_state->texture_atlas;
			projectile_curr->texture_rect.w = entity_size_texture;
			projectile_curr->texture_rect.h = entity_size_texture;
			projectile_curr->texture_rect.x = entity_size_texture * projectile_sprite_coords_x;
			projectile_curr->texture_rect.y = entity_size_texture * projectile_sprite_coords_y;
		}
	}
}

static void update(SDLContext* context, GameState* game_state)
{
	if (game_state->game_over)
		init(context, game_state);

	// player update
	{
		Entity* entity_player = &game_state->player; 
		if(context->btn_pressed_up)
			entity_player->position.y -= context->delta * entity_player->velocity;
		if(context->btn_pressed_down)
			entity_player->position.y += context->delta * entity_player->velocity;
		if(context->btn_pressed_left)
			entity_player->position.x -= context->delta * entity_player->velocity;
		if(context->btn_pressed_right)
			entity_player->position.x += context->delta * entity_player->velocity;
		if(context->btn_pressed_space && context->uptime - game_state->walltime_last_projectile_spawn > projectile_cooldow)
		{
			entity_spawn(
				game_state->projectiles,
				NUM_PROJECTILES,
				entity_player->position.x,
				// adding a small (arbitrary) offset to spawn the projectile a bit closer to the nose of the ship
				entity_player->position.y - 32,
				projectile_speed
			);
			game_state->walltime_last_projectile_spawn = context->uptime;
		}

		// clamp player position
		entity_player->position.x = SDL_clamp(
			entity_player->position.x,
			entity_player->rect.w / 2,
			context->window_w - entity_player->rect.w / 2
		);
		entity_player->position.y = SDL_clamp(
			entity_player->position.y,
			entity_player->rect.h / 2,
			context->window_h - entity_player->rect.h / 2
		);
	}

	// projectiles update
	{
		for(int i = 0; i < NUM_PROJECTILES; ++i)
		{
			Entity* entity = &game_state->projectiles[i];
			if(!entity->alive)
				continue;

			entity->position.y -= context->delta * entity->velocity;

			// despawn when out of bounds
			if(entity->position.y < -entity->texture_rect.h)
				entity->alive = false;

			for(int i = 0; i < NUM_ASTEROIDS; ++i)
			{
				Entity* asteroid_curr = &game_state->asteroids[i];
				if(!asteroid_curr->alive)
					continue;

				if(entity_collision_check(entity, asteroid_curr))
				{
					entity->alive = false;
					asteroid_curr->alive = false;
				}
			}
		}
	}

	// asteroids update
	{
		// very simple exponential spawning logic
		if(context->uptime - game_state->walltime_last_asteroid_spawn > game_state->asteroid_spawning_period)
		{
			game_state->walltime_last_asteroid_spawn = context->uptime;
			entity_spawn(
				game_state->asteroids,
				NUM_ASTEROIDS,
				entity_size_world + SDL_randf() * (context->window_w - entity_size_world * 2),
				-entity_size_world, // spawn asteroids off screen
				asteroid_speed_min + SDL_randf() * asteroid_speed_range
			);
			game_state->asteroid_spawning_period *= asteroid_spawning_period_mult;
		}

		for(int i = 0; i < NUM_ASTEROIDS; ++i)
		{
			Entity* asteroid_curr = &game_state->asteroids[i];
			if(!asteroid_curr->alive)
				continue;

			asteroid_curr->position.y += context->delta * asteroid_curr->velocity;

			if(entity_collision_check(asteroid_curr, &game_state->player))
			{
				//SDL_Log("game over");
				game_state->game_over = true;
				return;
			}

			// despawn when out of bounds
			if(asteroid_curr->position.y > context->window_h + asteroid_curr->texture_rect.h)
				asteroid_curr->alive = false;
		}
	}

	// asteroids render
	{
		for(int i = 0; i < NUM_ASTEROIDS; ++i)
		{
			Entity* asteroid_curr = &game_state->asteroids[i];
			if(!asteroid_curr->alive)
				continue;

			entity_render(context, asteroid_curr);
		}
	}

	// projectiles render
	{
		for(int i = 0; i < NUM_PROJECTILES; ++i)
		{
			Entity* entity = &game_state->projectiles[i];
			if(!entity->alive)
				continue;

			
			entity_render(context, entity);
		}
	}

	// player render
	{
		Entity* entity_player = &game_state->player; 
		entity_render(context, entity_player);
	}
}

int main(void)
{
	SDLContext context = { 0 };
	GameState game_state = { 0 };

	float window_w = 600;
	float window_h = 800;
	int target_framerate = SECONDS(1) / 60;

	SDL_Window* window = SDL_CreateWindow("ES01 - Rendering", window_w, window_h, 0);
	context.renderer = SDL_CreateRenderer(window, NULL);
	SDL_SetRenderDrawBlendMode(context.renderer, SDL_BLENDMODE_BLEND);
	context.window_w = window_w;
	context.window_h = window_h;

	// increase the zoom to make debug text more legible
	// (ie, on the class projector, we will usually use 2)
	{
		float zoom = 1;
		context.window_w /= zoom;
		context.window_h /= zoom;
		SDL_SetRenderScale(context.renderer, zoom, zoom);
	}

	bool quit = false;

	SDL_Time walltime_frame_beg;
	SDL_Time walltime_work_end;
	SDL_Time walltime_frame_end;
	SDL_Time time_elapsed_frame;
	SDL_Time time_elapsed_work;

	init(&context, &game_state);

	SDL_GetCurrentTime(&walltime_frame_beg);
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

				case SDL_EVENT_KEY_UP:
				case SDL_EVENT_KEY_DOWN:
					if(event.key.key == SDLK_W)
						context.btn_pressed_up = event.key.down;
					if(event.key.key == SDLK_A)
						context.btn_pressed_left = event.key.down;
					if(event.key.key == SDLK_S)
						context.btn_pressed_down = event.key.down;
					if(event.key.key == SDLK_D)
						context.btn_pressed_right = event.key.down;
					if(event.key.key == SDLK_SPACE)
						context.btn_pressed_space = event.key.down;

					// debug keys
					if(event.key.down)
					{
						if(event.key.key == SDLK_F1)
							DEBUG_draw_outline_collision = !DEBUG_draw_outline_collision;
					}
			}
		}

		// clear screen
		SDL_SetRenderDrawColor(context.renderer, 0x00, 0x00, 0x00, 0x00);
		SDL_RenderClear(context.renderer);

		update(&context, &game_state);

		SDL_GetCurrentTime(&walltime_work_end);
		time_elapsed_work = walltime_work_end - walltime_frame_beg;

		if(target_framerate > time_elapsed_work)
		{
			SDL_DelayPrecise(target_framerate - time_elapsed_work);
		}

		SDL_GetCurrentTime(&walltime_frame_end);
		time_elapsed_frame = walltime_frame_end - walltime_frame_beg;

		context.delta = NS_TO_SECONDS(time_elapsed_frame);
		context.uptime += context.delta;

#ifdef ENABLE_DIAGNOSTICS
		{
			SDL_SetRenderDrawColor(context.renderer, 0x00, 0x00, 0x00, 0x99);
			const SDL_FRect rect = SDL_FRect{ 5, 5, 305, 35 };
			SDL_RenderFillRect(context.renderer, &rect); 
			SDL_SetRenderDrawColor(context.renderer, 0xFF, 0xFF, 0xFF, 0xFF);
			SDL_RenderDebugTextFormat(context.renderer, 10.0f, 10.0f, "elapsed (frame): %9.6f ms", NS_TO_MILLIS(time_elapsed_frame));
			SDL_RenderDebugTextFormat(context.renderer, 10.0f, 20.0f, "elapsed(work)  : %9.6f ms", NS_TO_MILLIS(time_elapsed_work));
			SDL_RenderDebugTextFormat(context.renderer, 10.0f, 30.0f, "show object sizes [F1] : %3s", DEBUG_draw_outline_collision ? "ON" : "OFF");
		}
#endif

		// render
		SDL_RenderPresent(context.renderer);

		walltime_frame_beg = walltime_frame_end;
	}

	return 0;
};