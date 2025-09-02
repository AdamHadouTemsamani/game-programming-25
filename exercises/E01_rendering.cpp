#include <SDL3/SDL.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define ENABLE_DIAGNOSTICS
#define NUM_ASTEROIDS 10
#define PROJECTILE_POOL_SIZE 16



#define VALIDATE(expression) if(!(expression)) { SDL_Log("%s\n", SDL_GetError()); }

#define NANOS(x)   (x)                // converts nanoseconds into nanoseconds
#define MICROS(x)  (NANOS(x) * 1000)  // converts microseconds into nanoseconds
#define MILLIS(x)  (MICROS(x) * 1000) // converts milliseconds into nanoseconds
#define SECONDS(x) (MILLIS(x) * 1000) // converts seconds into nanoseconds

#define NS_TO_MILLIS(x)  ((float)(x)/(float)1000000)    // converts nanoseconds to milliseconds (in floating point precision)
#define NS_TO_SECONDS(x) ((float)(x)/(float)1000000000) // converts nanoseconds to seconds (in floating point precision)

struct SDLContext
{
	SDL_Renderer* renderer;
	float window_w; // current window width after render zoom has been applied
	float window_h; // current window height after render zoom has been applied

	float delta;    // in seconds

	bool btn_pressed_up    = false;
	bool btn_pressed_down  = false;
	bool btn_pressed_left  = false;
	bool btn_pressed_right = false;
	bool btn_pressed_fire  = false;
};

struct Entity
{
	SDL_FPoint   position;
	float        size;
	float        velocity;

	SDL_FRect    rect;
	SDL_Texture* texture_atlas;
	SDL_FRect    texture_rect;
};

struct Projectile
{
	SDL_FPoint position;
	float      size;
	float      velocity;

	SDL_FRect  rect;
	bool	   active;
};

struct GameState
{
	Entity player;
	Entity asteroids[NUM_ASTEROIDS];
	Projectile projectiles[PROJECTILE_POOL_SIZE];

	SDL_Texture* texture_atlas;	
	int player_score;
};

static float distance_between(SDL_FPoint a, SDL_FPoint b)
{
	float dx = a.x - b.x;
	float dy = a.y - b.y;
	return SDL_sqrtf(dx*dx + dy*dy);
}

static float distance_between_sq(SDL_FPoint a, SDL_FPoint b)
{
	float dx = a.x - b.x;
	float dy = a.y - b.y;
	return dx*dx + dy*dy;
}

static void deactivate_projectile(Projectile* projectile)
{
	projectile->active = false;
}

static Projectile* spawn_projectile(SDLContext* context, GameState* game_state) 
{
	for(int i = 0; i < PROJECTILE_POOL_SIZE; ++i)
	{
		Projectile* projectile = &game_state->projectiles[i];
		if(!projectile->active)
		{
			projectile->active = true;
			projectile->size = 16;
			projectile->position.x = game_state->player.position.x + game_state->player.size / 2 - projectile->size / 2;
			projectile->position.y = game_state->player.position.y - projectile->size; // spawn just above the player
			projectile->velocity = - game_state->player.velocity * 2;
			projectile->rect.w = projectile->size;
			projectile->rect.h = projectile->size;
			return projectile;

		}
	}
	SDL_Log("WARNING: no more projectiles available in the pool\n");
	return NULL;
}

static void respawn_asteroid(SDLContext* context, Entity* asteroid)
{
	if (asteroid->size <= 0) asteroid->size = 64; // default size if not set
	asteroid->position.x = asteroid->size + SDL_randf() * (context->window_w - asteroid->size * 2);
	asteroid->position.y = -asteroid->size; // spawn asteroids off screen 
	asteroid->velocity   = asteroid->size * 2 + SDL_randf() * asteroid->size * 4; //randomize the velocity again
	asteroid->rect.x = asteroid->position.x;
	asteroid->rect.y = asteroid->position.y;
	asteroid->rect.w = asteroid->size;
	asteroid->rect.h = asteroid->size;
}

static void reset_game(SDLContext* context, GameState* game_state)
{
	game_state->player_score = 0;
	// reset player position
	game_state->player.position.x = context->window_w / 2 - game_state->player.size / 2;
	game_state->player.position.y = context->window_h - game_state->player.size * 2;
	game_state->player.rect.x = game_state->player.position.x;
	game_state->player.rect.y = game_state->player.position.y;

	// reset asteroids
	for(int i = 0; i < NUM_ASTEROIDS; ++i)
	{
		respawn_asteroid(context, &game_state->asteroids[i]);
	}

	// reset projectiles
	for(int i = 0; i < PROJECTILE_POOL_SIZE; ++i)
	{
		deactivate_projectile(&game_state->projectiles[i]);
	}
}

static SDL_Texture* load_texture(SDL_Renderer* renderer, const char* path)
{
	int w = 0;
	int h = 0;
	int n = 0;
	unsigned char* pixels = stbi_load(path, &w, &h, &n, 0);

	SDL_assert(pixels);

	// we don't really need this SDL_Surface, but it's the most conveninet way to create out texture
	// NOTE: out image has the color channels in RGBA order, but SDL_PIXELFORMAT
	//       behaves the opposite on little endina architectures (ie, most of them)
	//       we won't worry too much about that, just remember this if your textures looks wrong
	//       - check that the the color channels are actually what you expect (how many? how big? which order)
	//       - if everythig looks right, you might just need to flip the channel order, because of SDL
	SDL_Surface* surface = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_ABGR8888, pixels, w * n);
	SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
	
	// NOTE: the texture will make a copy of the pixel data, so after creatio we can release both surface and pixel data
	SDL_DestroySurface(surface);
	stbi_image_free(pixels);

	return texture;
}

static void init(SDLContext* context, GameState* game_state)
{
	// Game constants
	const float entity_size_world = 64;
	const float entity_size_texture = 128;
	const float player_speed = entity_size_world * 5;
	const int   player_sprite_coords_x = 4;
	const int   player_sprite_coords_y = 0;
	const float asteroid_speed_min = entity_size_world * 2;
	const float asteroid_speed_range = entity_size_world * 4;
	const int   asteroid_sprite_coords_x = 0;
	const int   asteroid_sprite_coords_y = 4;


	// load textures
	game_state->texture_atlas = load_texture(context->renderer, "data/kenney/simpleSpace_tilesheet_2.png");

	// Initialize player
	{
		game_state->player.position.x = context->window_w / 2 - entity_size_world / 2;
		game_state->player.position.y = context->window_h - entity_size_world * 2;
		game_state->player.size = entity_size_world;
		game_state->player.velocity = player_speed;
		game_state->player.texture_atlas = game_state->texture_atlas;

		// player size in the game world
		game_state->player.rect.w = game_state->player.size;
		game_state->player.rect.h = game_state->player.size;

		// sprite size (in the tilemap)
		game_state->player.texture_rect.w = entity_size_texture;
		game_state->player.texture_rect.h = entity_size_texture;
		// sprite position (in the tilemap)
		game_state->player.texture_rect.x = entity_size_texture * player_sprite_coords_x;
		game_state->player.texture_rect.y = entity_size_texture * player_sprite_coords_y;
	}

	// Initialize asteroids
	{
		for(int i = 0; i < NUM_ASTEROIDS; ++i)
		{
			Entity* asteroid_curr = &game_state->asteroids[i];

			asteroid_curr->position.x = entity_size_world + SDL_randf() * (context->window_w - entity_size_world * 2);
			asteroid_curr->position.y = -entity_size_world; // spawn asteroids off screen (almost)
			asteroid_curr->size       = entity_size_world;
			asteroid_curr->velocity   = asteroid_speed_min + SDL_randf() * asteroid_speed_range;
			asteroid_curr->texture_atlas = game_state->texture_atlas;

			asteroid_curr->rect.w = asteroid_curr->size;
			asteroid_curr->rect.h = asteroid_curr->size;

			asteroid_curr->texture_rect.w = entity_size_texture;
			asteroid_curr->texture_rect.h = entity_size_texture;

			asteroid_curr->texture_rect.x = entity_size_texture * asteroid_sprite_coords_x;
			asteroid_curr->texture_rect.y = entity_size_texture * asteroid_sprite_coords_y;
		}
	}

	// Initialize projectiles 
	{
		
		for(int i = 0; i < PROJECTILE_POOL_SIZE; ++i)
		{
			game_state->projectiles[i].active = false;
			game_state->projectiles[i].size = 0;
			game_state->projectiles[i].velocity = 0;
			game_state->projectiles[i].rect.w = 0;
			game_state->projectiles[i].rect.h = 0;
			game_state->projectiles[i].position.x = 0;
			game_state->projectiles[i].position.y = 0;
		}
	}

	// Set initial game state
	reset_game(context, game_state);

}

static void update(SDLContext* context, GameState* game_state)
{
	// player
	{	
		// player movement
		Entity* entity_player = &game_state->player; 
		if(context->btn_pressed_up)
			entity_player->position.y -= context->delta * entity_player->velocity;
		if(context->btn_pressed_down)
			entity_player->position.y += context->delta * entity_player->velocity;
		if(context->btn_pressed_left)
			entity_player->position.x -= context->delta * entity_player->velocity;
		if(context->btn_pressed_right)
			entity_player->position.x += context->delta * entity_player->velocity;

		// player wrapping
        float cx = entity_player->position.x + entity_player->size / 2;
        float cy = entity_player->position.y + entity_player->size / 2;
        if (cx < 0) cx += context->window_w;
        if (cx > context->window_w) cx -= context->window_w;
        if (cy < 0) cy += context->window_h;
        if (cy > context->window_h) cy -= context->window_h;

		entity_player->position.x = cx - entity_player->size / 2;
		entity_player->position.y = cy - entity_player->size / 2;

		entity_player->rect.x = entity_player->position.x;
		entity_player->rect.y = entity_player->position.y;


		SDL_SetTextureColorMod(entity_player->texture_atlas, 0xFF, 0xFF, 0xFF);
		SDL_RenderTexture(
			context->renderer,
			entity_player->texture_atlas,
			&entity_player->texture_rect,
			&entity_player->rect
		);
	}

	// asteroids
	{
		// how close an asteroid must be before categorizing it as "too close" (100 pixels. We square it because we can avoid doing the square root later)
		const float warning_distance_sq = 100*100;
		// how close an asteroid must be before triggering a collision (64 pixels. We square it because we can avoid doing the square root later)
		// the number 64 is obtained by summing togheter the "radii" of the sprites
		const float collision_distance_sq = 64*64;

		for(int i = 0; i < NUM_ASTEROIDS; ++i)
		{
			Entity* asteroid_curr = &game_state->asteroids[i];
			asteroid_curr->position.y += context->delta * asteroid_curr->velocity;

			asteroid_curr->rect.x = asteroid_curr->position.x;
			asteroid_curr->rect.y = asteroid_curr->position.y;

			float distance_sq = distance_between_sq(asteroid_curr->position, game_state->player.position);
			if(distance_sq < collision_distance_sq)
			{
				SDL_SetTextureColorMod(asteroid_curr->texture_atlas, 0xFF, 0x00, 0x00);
				reset_game(context, game_state);
			}
			else if(distance_sq < warning_distance_sq)
				SDL_SetTextureColorMod(asteroid_curr->texture_atlas, 0xCC, 0xCC, 0x00);
			else
				SDL_SetTextureColorMod(asteroid_curr->texture_atlas, 0xFF, 0xFF, 0xFF);

			SDL_RenderTexture(
				context->renderer,
				asteroid_curr->texture_atlas,
				&asteroid_curr->texture_rect,
				&asteroid_curr->rect
			);

			// if the asteroid has moved off screen, respawn it
			if (asteroid_curr->position.y > context->window_h + asteroid_curr->size)
			{
				respawn_asteroid(context, asteroid_curr);
			}
		}
	}

	// projectiles
	{
		for (int i = 0; i < PROJECTILE_POOL_SIZE; ++i)
		{
			Projectile* projectile = &game_state->projectiles[i];
			if(projectile->active)
			{
				projectile->position.y += context->delta * projectile->velocity;

				projectile->rect.x = projectile->position.x;
				projectile->rect.y = projectile->position.y;
				projectile->rect.w = projectile->size;
				projectile->rect.h = projectile->size;

				//Render projectile as white square
				SDL_SetRenderDrawColor(context->renderer, 0xFF, 0xFF, 0xFF, 0xFF);
				SDL_RenderFillRect(context->renderer, &projectile->rect);

				// if the projectile has moved off screen, deactivate it
				if (projectile->position.y + projectile->size < 0 || projectile->position.y > context->window_h)
				{
					deactivate_projectile(projectile);
				}

				// check for collisions with asteroids
				for (int i = 0; i < NUM_ASTEROIDS; ++i)
				{
					Entity* asteroid_curr = &game_state->asteroids[i];
					SDL_FPoint asteroid_center = { asteroid_curr->position.x + asteroid_curr->size / 2, asteroid_curr->position.y + asteroid_curr->size / 2 };
					SDL_FPoint projectile_center = { projectile->position.x + projectile->size / 2, projectile->position.y + projectile->size / 2 };
					float radius_sum = asteroid_curr->size / 2 + projectile->size / 2;
				
					if (distance_between_sq(asteroid_center, projectile_center) < radius_sum * radius_sum)
					{
						// collision detected
						game_state->player_score += 1;
						respawn_asteroid(context, asteroid_curr);
						deactivate_projectile(projectile);
						break; // break out of the asteroid loop, since this projectile is now deactivated
					}
				}
			}
		}
	}
}

int main(void)
{
	SDLContext context = { 0 };
	GameState game_state = { 0 };

	float window_w = 600;
	float window_h = 800;
	int target_framerate = SECONDS(1) / 60;

	SDL_Window* window = SDL_CreateWindow("E01 - Rendering", window_w, window_h, 0);
	context.renderer = SDL_CreateRenderer(window, NULL);
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
					if(event.key.key == SDLK_SPACE && event.key.down)
						spawn_projectile(&context, &game_state);
					break;
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

#ifdef ENABLE_DIAGNOSTICS
		SDL_SetRenderDrawColor(context.renderer, 0xFF, 0xFF, 0xFF, 0xFF);
		SDL_RenderDebugTextFormat(context.renderer, 10.0f, 10.0f, "elapsed (frame): %9.6f ms", NS_TO_MILLIS(time_elapsed_frame));
		SDL_RenderDebugTextFormat(context.renderer, 10.0f, 20.0f, "elapsed(work)  : %9.6f ms", NS_TO_MILLIS(time_elapsed_work));
#endif

		// render
		SDL_RenderDebugTextFormat(context.renderer, 10.0f, 40.0f, "score          : %d", game_state.player_score);
		SDL_RenderPresent(context.renderer);

		walltime_frame_beg = walltime_frame_end;
	}

	return 0;
};