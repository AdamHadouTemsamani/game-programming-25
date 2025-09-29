#define STB_IMAGE_IMPLEMENTATION
#define ITU_LIB_ENGINE_IMPLEMENTATION
#define ITU_LIB_RENDER_IMPLEMENTATION
#define ITU_LIB_OVERLAPS_IMPLEMENTATION

#include <SDL3/SDL.h>

#include <itu_common.hpp>
#include <itu_lib_render.hpp>
#include <itu_lib_overlaps.hpp>

#define ENABLE_DIAGNOSTICS

#define TARGET_FRAMERATE SECONDS(1) / 60
#define WINDOW_W 1920
#define WINDOW_H 1080

// collision "performance data" (all eyeballed, don't care about precise measurement yet),
// collected on my laptop (i7-1260P, 2100 Mhz)
// 
// - staring point                                128        0.1 ms/f TODO: retake measuring without rendering
// - dynamic entities                            4096      180   ms/f TODO: retake measuring without rendering
// - static entities                           8*4096       16   ms/f TODO: retake measuring without rendering
// - dynamic entities                            ~700       16   ms/f TODO: retake measuring without rendering
// - world partition,  4 cells (all dynamic)    ~2500       16   ms/f
// - world partition, 16 cells (all dynamic)    ~3000       16   ms/f
// - world partition, 64 cells (all dynamic)    ~4000       16   ms/f
// 
#define ENTITY_COUNT 1600

#define MAX_COLLISIONS (ENTITY_COUNT * 6)   // num max collisions per frame

#define WORLD_PARTITION_CELL_SPLITS 8

// NOTE: this is actually an interesting design choice: how many entities can exist in a single cell *at the same time*
//       In the worst case, all entities are in the same cell, and as unlikely as that this can we affort to do less?
//       Maybe, but finding a reasonable number that you can enforce efficiently is not trivial, and this is why this "fixed split"
//       method is never used in real-world application, and more refined algorithms exist.
#define WORLD_PARTITION_CELL_MAX_ENTITY_COUNT   (ENTITY_COUNT)


bool DEBUG_separate_collisions   = true;
bool DEBUG_render_colliders      = true;
bool DEBUG_render_texture_border = false;
bool DEBUG_render_texture        = false;

struct Entity;
struct EntityCollisionInfo;
struct WorldPartitionCell;

struct MySDLContext
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

	vec2f mouse_pos;
};

struct GameState
{
	Entity* player;

	// game-allocated memory
	Entity* entities;
	int entities_alive_count;

	
	// collision system data
	EntityCollisionInfo* frame_collisions;
	int frame_collisions_count;

	WorldPartitionCell* world_partition_cells;
	int                 world_partition_cells_count;
	vec2f               world_partition_cell_size;

	// SDL-allocated structures
	SDL_Texture* atlas;
};

static SDL_Texture* texture_create(MySDLContext* context, const char* path)
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

// quick sprite rendering function that takes care of most of the functionalities
// NOTE: this function is still temporary since ATM we can't really deal with game worlds bigger than the rendering window
//       we will address it in lecture 03, and then we will just create a final sprite system and be done with it
static void sprite_render(MySDLContext* context, vec2f position, vec2f size, Sprite* sprite)
{
	SDL_FRect dst_rect;
	dst_rect.w = size.x;
	dst_rect.h = size.y;
	dst_rect.x = position.x - dst_rect.w * sprite->pivot.x;
	dst_rect.y = position.y - dst_rect.h * sprite->pivot.y;
		
	if(DEBUG_render_texture)
	{
		SDL_SetTextureColorModFloat(sprite->texture, sprite->tint.r, sprite->tint.g, sprite->tint.b);
		SDL_SetTextureAlphaModFloat(sprite->texture, sprite->tint.a);
		SDL_RenderTexture(context->renderer, sprite->texture, &sprite->rect, &dst_rect);
	}

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

	// collider info
	bool  collider_is_static;
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
// world partition
// ********************************************************************************************************************

struct WorldPartitionCell
{
	// NOTE: this is an array of pointers
	Entity** entity_refs;
	int      entity_refs_counts;

	vec2f min;
	vec2f max;
};

// fancy debug visualization for an incredibly simple world partition
// the code is a mess tho, we will try to make it better when we talk about UIs
static void world_partition_debug_cells(MySDLContext* context, GameState* state)
{
	// render cell boundaries
	SDL_SetRenderDrawColor(context->renderer, 0xFF, 0xFF, 0xFF, 0xFF);
	for(int i = 0; i < state->world_partition_cells_count; ++i)
	{
		WorldPartitionCell* cell = &state->world_partition_cells[i];
		SDL_FRect rect = { cell->min.x, cell->min.y, state->world_partition_cell_size.x, state->world_partition_cell_size.y };
		SDL_RenderRect(context->renderer, &rect);

	}

	// render cell debug info
	SDL_SetRenderDrawColor(context->renderer, 0x0, 0x00, 0x00, 0xCC);
	SDL_FRect rect = SDL_FRect{ 250, 5, 230 + 225, state->world_partition_cells_count * 10.0f + 35.0f };
	SDL_RenderFillRect(context->renderer, &rect);
	
	SDL_SetRenderDrawColor(context->renderer, 0xFF, 0xFF, 0xFF, 0xFF);
	SDL_RenderDebugText(context->renderer, 255, 10, "cell   entities   rect min          rect max");
	SDL_RenderLine(context->renderer, 255, 20, 250+230 + 220, 20);

	float base_text_render_y = 40;
	int entity_refs_total = 0;
	for(int i = 0; i < state->world_partition_cells_count; ++i)
	{
		WorldPartitionCell* cell = &state->world_partition_cells[i];
		entity_refs_total += cell->entity_refs_counts;
		SDL_RenderDebugTextFormat(
			context->renderer, 255, base_text_render_y + 10 * i,
			"%4d   %4d       (%6.1f, %6.1f)   (%6.1f,  %6.1f)",
			i, cell->entity_refs_counts, cell->min.x, cell->min.y, cell->max.x, cell->max.y
		);
	}
	SDL_RenderDebugTextFormat(
		context->renderer, 255, base_text_render_y -15,
		" tot   %4d",
		entity_refs_total
	);
	SDL_RenderLine(context->renderer, 255, base_text_render_y-5, 250+230 + 220, base_text_render_y + -5);
}

static void world_partition_cell_add_entity(Entity* entity, WorldPartitionCell* cell)
{
	SDL_assert(cell->entity_refs_counts < WORLD_PARTITION_CELL_MAX_ENTITY_COUNT);
	cell->entity_refs[cell->entity_refs_counts] = entity;
	cell->entity_refs_counts++;
}

// (overly) simple world partition logic, just split the world in 4 quadrant
static void world_partition_assign_entity_to_cell(Entity* entity, GameState* state)
{
	vec2f p = entity->position + entity->collider_offset;

	// get cell coordinates form collider center
	int coord_x = SDL_clamp((int) p.x / state->world_partition_cell_size.x, 0, WORLD_PARTITION_CELL_SPLITS - 1);
	int coord_y = SDL_clamp((int) p.y / state->world_partition_cell_size.y, 0, WORLD_PARTITION_CELL_SPLITS - 1);
			
	// get cell index from coordinates
	int idx_center = coord_x + coord_y * WORLD_PARTITION_CELL_SPLITS;
	SDL_assert(idx_center < state->world_partition_cells_count);
	world_partition_cell_add_entity(entity, &state->world_partition_cells[idx_center]);

	vec2f bounds[] = { p, p, p, p };
	bounds[0].x += entity->collider_radius; 
	bounds[1].y -= entity->collider_radius;
	bounds[2].x -= entity->collider_radius;
	bounds[3].y += entity->collider_radius;

	for(int i = 0; i < 4; ++i)
	{
		vec2f p = bounds[i];
		// get cell coordinates form entity extremes
		int coord_x = SDL_clamp((int) p.x / state->world_partition_cell_size.x, 0, WORLD_PARTITION_CELL_SPLITS - 1);
		int coord_y = SDL_clamp((int) p.y / state->world_partition_cell_size.y, 0, WORLD_PARTITION_CELL_SPLITS - 1);
			
		// get cell index from coordinates
		int idx_bound = coord_x + coord_y * WORLD_PARTITION_CELL_SPLITS;
		SDL_assert(idx_center < state->world_partition_cells_count);
		if(idx_bound != idx_center)
			world_partition_cell_add_entity(entity, &state->world_partition_cells[idx_bound]);
	}
}

static void world_partition_assign_all_entitites(GameState* state)
{
	for(int i = 0; i < state->world_partition_cells_count; ++i)
		state->world_partition_cells[i].entity_refs_counts = 0;

	for(int i = 0; i < state->entities_alive_count; ++i)
	{
		Entity* entity = &state->entities[i];
		world_partition_assign_entity_to_cell(entity, state);
	}
}

// ********************************************************************************************************************
// collisions
// ********************************************************************************************************************

struct EntityCollisionInfo
{
	Entity* e1;
	Entity* e2;

	vec2f normal;
	float separation;
};

static void collision_check_references(GameState* state, Entity** entity_refs, int entity_refs_count)
{
	for(int i = 0; i < entity_refs_count - 1; ++i)
	{
		Entity* e1 = entity_refs[i];

		if(e1->collider_is_static)
			continue;
		
		for(int j = i + 1; j < entity_refs_count; ++j)
		{
			Entity* e2 = entity_refs[j];

			if(itu_lib_overlaps_circle_circle(
				e1->position + e1->collider_offset, e1->collider_radius,
				e2->position + e2->collider_offset, e2->collider_radius
			))
			{
				// // epilepsy warning right there
				// e1->sprite.tint = COLOR_RED;
				// e2->sprite.tint = COLOR_RED;

				if(state->frame_collisions_count >= MAX_COLLISIONS)
				{
					SDL_Log("[WARNING] too many collisions!");
					return;
				}

				// NOTE: here we are redoing a bunch of work that we already done in the overlap test. An easy optimization is do to have the test return the collision info
				vec2f v = (e2->position + e2->collider_offset) - (e1->position + e1->collider_offset);
				float l = length(v);
				float separation_vector = e1->collider_radius + e2->collider_radius - l;
				int new_collision_idx = state->frame_collisions_count++;

				state->frame_collisions[new_collision_idx].e1 = e1;
				state->frame_collisions[new_collision_idx].e2 = e2;
				state->frame_collisions[new_collision_idx].normal = v / l; // normalize vector (we already need the length, so we don't need to call normalize which would do that anyway)
				state->frame_collisions[new_collision_idx].separation = separation_vector;
			}
		}
	}
}
static void collision_check(GameState* state)
{
	state->frame_collisions_count = 0;

	if(state->world_partition_cells_count > 0)
	{
		// world partition

		for(int i = 0; i < state->world_partition_cells_count; ++i)
		{
			WorldPartitionCell* cell = &state->world_partition_cells[i];

			collision_check_references(state, cell->entity_refs, cell->entity_refs_counts);
		}
	}
	else {
		for(int i = 0; i < state->entities_alive_count - 1; ++i)
		{
			Entity* e1 = &state->entities[i];
			if(e1->collider_is_static)
				continue;
		
			for(int j = i + 1; j < state->entities_alive_count; ++j)
			{
				Entity* e2 = &state->entities[j];

				if(itu_lib_overlaps_circle_circle(
					e1->position + e1->collider_offset, e1->collider_radius,
					e2->position + e2->collider_offset, e2->collider_radius
				))
				{
					if(state->frame_collisions_count >= MAX_COLLISIONS)
					{
						SDL_Log("[WARNING] too many collisions!");
						return;
					}

					// NOTE: here we are redoing a bunch of work that we already done in the overlap test. An easy optimization is do to have the test return the collision info
					vec2f v = (e2->position + e2->collider_offset) - (e1->position + e1->collider_offset);
					float l = length(v);
					float separation_vector = e1->collider_radius + e2->collider_radius - l;
					int new_collision_idx = state->frame_collisions_count++;

					state->frame_collisions[new_collision_idx].e1 = e1;
					state->frame_collisions[new_collision_idx].e2 = e2;
					state->frame_collisions[new_collision_idx].normal = v / l; // normalize vector (we already need the length, so we don't need to call normalize which would do that anyway)
					state->frame_collisions[new_collision_idx].separation = separation_vector;
				}
			}
		}
	}
}

static void collision_separate(GameState* state)
{
	for(int i = 0; i < state->frame_collisions_count; ++i)
	{
		EntityCollisionInfo entity_collision_info = state->frame_collisions[i];

		vec2f sep = entity_collision_info.normal * entity_collision_info.separation;

		// NOTE: for an entity to be static, it must never move!
		//       Otherwise, it will phase through other static entities when moved by a dynamic collider.
		// TMP added reflection vectors
		if(entity_collision_info.e2->collider_is_static)
		{
			entity_collision_info.e1->position -= sep;
		}
		else
		{
			sep = sep / 2;
			entity_collision_info.e1->position -= sep;
			entity_collision_info.e2->position += sep;
		}

	}
}

// ********************************************************************************************************************
// game
// ********************************************************************************************************************

static void game_init(MySDLContext* context, GameState* state)
{
	state->entities = (Entity*)SDL_calloc(ENTITY_COUNT, sizeof(Entity));
	SDL_assert(state->entities);

	state->frame_collisions = (EntityCollisionInfo*)SDL_calloc(MAX_COLLISIONS, sizeof(EntityCollisionInfo));
	SDL_assert(state->frame_collisions);

	const int num_cells = 4;

	// world partitioning data allocation
	{
		// we will split out game world in WORLD_PARTITION_CELL_SPLITS vertically and WORLD_PARTITION_CELL_SPLITS horizontally
		state->world_partition_cells_count = WORLD_PARTITION_CELL_SPLITS * WORLD_PARTITION_CELL_SPLITS;

		// allocate one integer for each cell (to know how many entities actually reside here)
		state->world_partition_cells = (WorldPartitionCell*)SDL_calloc(state->world_partition_cells_count, sizeof(WorldPartitionCell));

		// allocate actual array of references for the entities
		for(int i = 0; i < state->world_partition_cells_count; ++i)
		{
			WorldPartitionCell* cell = &state->world_partition_cells[i];
			cell->entity_refs = (Entity**)SDL_calloc(WORLD_PARTITION_CELL_MAX_ENTITY_COUNT, sizeof(Entity*));
		}
	}

	// texture atlases
	state->atlas = texture_create(context, "data/kenney/simpleSpace_tilesheet_2.png");

}

static void game_reset(MySDLContext* context, GameState* state)
{
	// entities
	{
		SDL_memset(state->entities, 0, ENTITY_COUNT * sizeof(Entity));
		state->entities_alive_count = 0;

		// // NOTE: for the world partition test, we would like all entitites to be evenly spread, so we can check for both balanced and unbalanced cell work,
		// //       so we're leaving the player out of the equation for this. We can re-enable it for the rest of the exercise
		// Entity* player = entity_create(state);
		// SDL_assert(player);
		// player->position.x = (float)context->window_w / 2;
		// player->position.y = (float)context->window_h / 2;
		// player->size = vec2f{ 64, 64 };
		// player->sprite = {
		// 	.texture = state->atlas,
		// 	.rect = SDL_FRect{ 0, 0, 128, 128 },
		// 	.tint = COLOR_WHITE,
		// 	.pivot = vec2f{ 0.5f, 0.5f }
		// };
		// player->collider_radius = 32;
		// state->player = player;

		// grid pattern
		const float scale_size = 0.2f; // factor to tune all entity size, to test world partitioning easier

		int grid_side = (int)SDL_sqrt(ENTITY_COUNT);
		int grid_side_half = grid_side / 2;
		float separation_factor = 1.1f;
		for(int i = 0; i < ENTITY_COUNT; ++i)
		{
			Entity* entity = entity_create(state);
			if(!entity)
			{
				// NOTE: the exercise is actually asking us to spawn as many as possible,
				//       might as well keep running until we run out
				// SDL_Log("[WARNING] too many entity spawned!");
				break;
			}
		
			vec2f coords = vec2f { (float)((i % grid_side) * separation_factor - grid_side_half), (float)((i / grid_side) * separation_factor - grid_side_half) };

			entity->size = vec2f{ 64, 64 } *scale_size;
			entity->position = mul_element_wise(entity->size, coords) + vec2f { WINDOW_W / 2, WINDOW_H / 2};
			entity->sprite = Sprite
			{
				state->atlas,
				SDL_FRect{ 0, 4*128, 128, 128 },
				COLOR_WHITE,
				vec2f{ 0.5f, 0.5f }
			};
			entity->collider_is_static = false;
			entity->collider_radius = 18 * scale_size;
		}
	}

	// world partition
	if(WORLD_PARTITION_CELL_SPLITS > 0)
	{
		state->world_partition_cell_size.x = WINDOW_W / WORLD_PARTITION_CELL_SPLITS;
		state->world_partition_cell_size.y = WINDOW_H / WORLD_PARTITION_CELL_SPLITS;
		// initialize cells
		for(int i = 0; i < state->world_partition_cells_count; ++i)
		{
			int cell_coord_x = i % WORLD_PARTITION_CELL_SPLITS;
			int cell_coord_y = i / WORLD_PARTITION_CELL_SPLITS;

			WorldPartitionCell* cell = &state->world_partition_cells[i];
			cell->entity_refs_counts = 0;
			cell->min.x = cell_coord_x * state->world_partition_cell_size.x;
			cell->min.y = cell_coord_y * state->world_partition_cell_size.y;
			cell->max.x = (cell_coord_x + 1) * state->world_partition_cell_size.x;
			cell->max.y = (cell_coord_y + 1) * state->world_partition_cell_size.y;
		}
		world_partition_assign_all_entitites(state);
	}
}

static void game_update(MySDLContext* context, GameState* state)
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

	// // move player only
	// state->player->position = state->player->position + velocity;

	// // move all entities (to test world partition balancing)
	for(int i = 0; i < state->entities_alive_count; ++i)
	{
		Entity* entity = &state->entities[i];
		entity->position = entity->position + velocity;
	}

	// reset tint
	for(int i = 0; i < state->entities_alive_count; ++i)
	{
		Entity* entity = &state->entities[i];
		entity->sprite.tint = COLOR_WHITE;
	}

	collision_check(state);
	if(DEBUG_separate_collisions)
		collision_separate(state);

	// NOTE: here is where we would like to "update" our cells, checking if any Entity moved in or out of a cell
	//       However, pointers make it really annoying to handle two-way references this way.
	//       Surely, re-assigning every entity EVERY frame is a waste? We will discuss this next lecture
	if(state->world_partition_cells_count > 0)
		world_partition_assign_all_entitites(state);
}

static void game_render(MySDLContext* context, GameState* state)
{
	// render
	for(int i = 0; i < state->entities_alive_count; ++i)
	{
		Entity* entity = &state->entities[i];
		sprite_render(context, entity->position, entity->size, &entity->sprite);

		if(DEBUG_render_colliders)
		{
			itu_lib_render_draw_point(context->renderer, entity->position + entity->collider_offset, 5, COLOR_GREEN);
			itu_lib_render_draw_circle(
				context->renderer,
				entity->position + entity->collider_offset,
				entity->collider_radius,
				16, COLOR_GREEN
			);
		}
	}

	// debug world partition
	{
		world_partition_debug_cells(context, state);
	}
	// debug window
	SDL_SetRenderDrawColor(context->renderer, 0xFF, 0x00, 0xFF, 0xff);
	SDL_RenderRect(context->renderer, NULL);
}

int main(void)
{
	int a = sizeof(int*);
	bool quit = false;
	SDL_Window* window;
	MySDLContext context = { 0 };
	GameState  state   = { 0 };

	context.window_w = WINDOW_W;
	context.window_h = WINDOW_H;

	SDL_CreateWindowAndRenderer("E02 - Collisions", context.window_w, context.window_h, 0, &window, &context.renderer);

	SDL_SetRenderDrawBlendMode(context.renderer, SDL_BLENDMODE_BLEND);
	
	// increase the zoom to make debug text more legible
	// (ie, on the class projector, we will usually use 2)
	{
		context.zoom = 1;
		context.window_w /= context.zoom;
		context.window_h /= context.zoom;
		SDL_SetRenderScale(context.renderer, context.zoom, context.zoom);
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
				case SDL_EVENT_MOUSE_MOTION:
				{
					context.mouse_pos.x = event.motion.x;
					context.mouse_pos.y = event.motion.y;
					break;
				}
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
							case SDLK_F4: DEBUG_render_texture        = !DEBUG_render_texture;        break;
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
			SDL_FRect rect = SDL_FRect{ 5, 5, 225, 85 };
			SDL_RenderFillRect(context.renderer, &rect);
			SDL_SetRenderDrawColor(context.renderer, 0xFF, 0xFF, 0xFF, 0xFF);
			SDL_RenderDebugTextFormat(context.renderer, 10, 10, "entities : %d", ENTITY_COUNT);
			SDL_RenderDebugTextFormat(context.renderer, 10, 20, "work     : %9.6f ms/f", (float)elapsed_work  / (float)MILLIS(1));
			SDL_RenderDebugTextFormat(context.renderer, 10, 30, "tot      : %9.6f ms/f", (float)elapsed_frame / (float)MILLIS(1));
			SDL_RenderDebugTextFormat(context.renderer, 10, 40, "[TAB] reset ");
			SDL_RenderDebugTextFormat(context.renderer, 10, 50, "[F1]  collisions        %s", DEBUG_separate_collisions   ? " ON" : "OFF");
			SDL_RenderDebugTextFormat(context.renderer, 10, 60, "[F2]  render colliders  %s", DEBUG_render_colliders      ? " ON" : "OFF");
			SDL_RenderDebugTextFormat(context.renderer, 10, 70, "[F3]  render tex border %s", DEBUG_render_texture_border ? " ON" : "OFF");
			SDL_RenderDebugTextFormat(context.renderer, 10, 80, "[F4]  render textures   %s", DEBUG_render_texture        ? " ON" : "OFF");
		}
#endif

		// render
		SDL_RenderPresent(context.renderer);

		context.delta = (float)elapsed_frame / (float)SECONDS(1);
		context.uptime += context.delta;
		walltime_frame_beg = walltime_frame_end;
	}
}
