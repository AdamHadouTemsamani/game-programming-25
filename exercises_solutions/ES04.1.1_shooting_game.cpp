#define TEXTURE_PIXELS_PER_UNIT 16    // how many pixels of textures will be mapped to a single world unit
#define CAMERA_PIXELS_PER_UNIT  16*2  // how many pixels of windows will be used to render a single world unit

#include <itu_unity_include.hpp>

#define ENABLE_DIAGNOSTICS

// rendering framerate
#define TARGET_FRAMERATE_NS    SECONDS(1) / 200

// physics timestep
#define PHYSICS_TIMESTEP_NSECS  SECONDS(1) / 60
#define PHYSICS_TIMESTEP_SECS   NS_TO_SECONDS(PHYSICS_TIMESTEP_NSECS)
#define PHYSICS_MAX_TIMESTEPS_PER_FRAME 4
#define PHYSICS_MAX_CONTACTS_PER_ENTITY 16

#define WINDOW_W         800
#define WINDOW_H         600

#define ENTITY_COUNT 1024
#define GRAVITY      -9.8f

#define COLLISION_FILTER_PLAYER     0b00001
#define COLLISION_FILTER_GROUND     0b00010
#define COLLISION_FILTER_CLUTTER    0b00100
#define COLLISION_FILTER_PROJECTILE 0b01000

bool DEBUG_render_textures = true;
bool DEBUG_render_outlines = false;
bool DEBUG_physics = true;
b2DebugDraw debug_draw;

enum GameplayPhase
{
	GAMEPLAY_PHASE_AIM,
	GAMEPLAY_PHASE_WAIT
};

struct Entity
{
	Sprite    sprite;
	Transform transform;
	b2BodyId  body_id;
	vec2f     velocity;
};

typedef struct PlayerData
{
	float shooting_angle;
	float shooting_power;
	float keyboard_angle_speed;
	float keyboard_power_speed;
} PlayerData;

struct GameState
{
	// shortcut references
	Entity* player;
	Entity* projectile;

	// game-allocated memory
	Entity* entities;
	int entities_alive_count;
	PlayerData player_data;

	// SDL-allocated structures
	SDL_Texture* atlas;

	// box2d
	b2WorldId world_id;
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


// game parameters
float design_cannon_length = 4;
float design_shooting_angle_min = PI_HALF / 8;
float design_shooting_angle_max = PI_HALF - design_shooting_angle_min;
float design_shooting_power_min = 1;
float design_shooting_power_max = 50;


void debug_player_data(SDLContext* context, GameState* state)
{
	PlayerData* data = &state->player_data;
	
	// show angle as degrees to the user (easier to understand)
	float shooting_angle_deg = data->shooting_angle * RAD_2_DEG;
	float shooting_angle_deg_min = design_shooting_power_min * RAD_2_DEG;
	float shooting_angle_deg_max = design_shooting_power_max * RAD_2_DEG;

	ImGui::Begin("game_player_data");
	if(ImGui::DragFloat("shooting_angle", &shooting_angle_deg, 1.0f, shooting_angle_deg_min, shooting_angle_deg_max))
		data->shooting_angle = shooting_angle_deg * DEG_2_RAD;
	ImGui::DragFloat("shooting_power", &data->shooting_power, 1.0f, design_shooting_power_min, design_shooting_power_max);
	ImGui::DragFloat("keyboard_angle_speed", &data->keyboard_angle_speed, 1.0f);
	ImGui::DragFloat("keyboard_power_speed", &data->keyboard_power_speed, 1.0f);
	ImGui::End();

}


static void game_init(SDLContext* context, GameState* state)
{
	// allocate memory
	state->entities = (Entity*)SDL_calloc(ENTITY_COUNT, sizeof(Entity));
	SDL_assert(state->entities);

	state->world_id = { 0 };
	
	state->player_data = { 0 };
	state->player_data.shooting_angle = (design_shooting_angle_max - design_shooting_angle_min) / 2;
	state->player_data.shooting_power = design_shooting_power_min;
	state->player_data.keyboard_angle_speed = 1;
	state->player_data.keyboard_power_speed = 10;

	// texture atlases
	state->atlas = texture_create(context, "data/kenney/tiny_dungeon_packed.png", SDL_SCALEMODE_NEAREST);
}

static void game_reset(SDLContext* context, GameState* state)
{
	// TMP reset uptime (should probably be two different variables
	context->uptime = 0;

	if(b2World_IsValid(state->world_id))
		b2DestroyWorld(state->world_id);
	b2WorldDef def_world = b2DefaultWorldDef();
	def_world.gravity.y = GRAVITY;
	state->world_id = b2CreateWorld(&def_world);

	state->entities_alive_count = 0;

	// player cannon
	{
		Entity* entity = entity_create(state);
		state->player = entity;
		entity->transform.position = vec2f { -15, -2 };
		entity->transform.scale = vec2f { design_cannon_length, 1 };
		itu_lib_sprite_init(&entity->sprite, state->atlas, itu_lib_sprite_get_rect(0, 0, 16, 16));
		entity->sprite.pivot.x = 1 / (design_cannon_length * 2);
	}

	// projectile
	{
		Entity* entity = entity_create(state);
		state->projectile = entity;
		entity->transform.scale = vec2f { 1, 1 };
		itu_lib_sprite_init(&entity->sprite, state->atlas, itu_lib_sprite_get_rect(10, 6, 16, 16));
		
		b2BodyDef body_def = b2DefaultBodyDef();
		body_def.type = b2_dynamicBody;
		body_def.position = b2Vec2{ 99, 99 };
		body_def.isEnabled = false;

		b2ShapeDef shape_def = b2DefaultShapeDef();
		shape_def.filter.categoryBits = COLLISION_FILTER_PROJECTILE;

		b2Polygon polygon = b2MakeBox(0.3f, 0.5f);

		entity->body_id = b2CreateBody(state->world_id, &body_def);
		b2CreatePolygonShape(entity->body_id, &shape_def, &polygon);
	}

	// floor
	{
		b2BodyDef body_def = b2DefaultBodyDef();
		body_def.type = b2_staticBody;
		body_def.position = b2Vec2{ 0, -3 };
		b2ShapeDef shape_def = b2DefaultShapeDef();

		shape_def.filter.categoryBits = COLLISION_FILTER_GROUND;
		b2Polygon polygon = b2MakeBox(32.0f, 1.0f);

		Entity* entity = entity_create(state);
		entity->body_id = b2CreateBody(state->world_id, &body_def);
		b2CreatePolygonShape(entity->body_id, &shape_def, &polygon);
	}

	// clutter
	{
		b2BodyDef body_def = b2DefaultBodyDef();
		body_def.type = b2_dynamicBody;
		body_def.fixedRotation = false;

		// collider shape (to enable collisions with the ground)
		b2ShapeDef shape_def = b2DefaultShapeDef();
		shape_def.density = 1;
		shape_def.filter.categoryBits = COLLISION_FILTER_CLUTTER;

		b2Polygon polygon_box = b2MakeBox(0.5f, 0.5f);
		for(int i = 0; i < 32; ++i)
		{
			Entity* entity = entity_create(state);
			entity->transform.scale = VEC2F_ONE;
			
			vec2f size = itu_lib_sprite_get_world_size(context, &entity->sprite, &entity->transform);
			vec2f offset = -mul_element_wise(size, entity->sprite.pivot - vec2f{ 0.5f, 0.5f });

			body_def.position = b2Vec2{ 3.0f + (float)(i % 4), (float)(i / 4) - 1.5f};
			body_def.angularVelocity = 1;
			entity->body_id = b2CreateBody(state->world_id, &body_def);
			b2CreatePolygonShape(entity->body_id, &shape_def, &polygon_box);
			itu_lib_sprite_init(
				&entity->sprite,
				state->atlas,
				itu_lib_sprite_get_rect(3, 5, 16, 16)
			);
		}
	}

	// debug draw
	debug_draw.context = context;
	debug_draw.drawShapes = true;
	debug_draw.DrawSolidPolygonFcn = fn_box2d_wrapper_draw_polygon;
	debug_draw.DrawSolidCircleFcn = fn_box2d_wrapper_draw_circle;
}

static void game_update(SDLContext* context, GameState* state)
{
	// player cannon
	{
		Entity* entity = state->player;
		PlayerData* data = &state->player_data;
		if(context->btn_isdown[BTN_TYPE_UP])
			data->shooting_angle += data->keyboard_angle_speed * context->delta;
		if(context->btn_isdown[BTN_TYPE_DOWN])
			data->shooting_angle -= data->keyboard_angle_speed * context->delta;
		if(context->btn_isdown[BTN_TYPE_LEFT])
			data->shooting_power -= data->keyboard_power_speed * context->delta;
		if(context->btn_isdown[BTN_TYPE_RIGHT])
			data->shooting_power += data->keyboard_power_speed * context->delta;

		data->shooting_angle = SDL_clamp(data->shooting_angle, design_shooting_angle_min, design_shooting_angle_max);
		data->shooting_power = SDL_clamp(data->shooting_power, design_shooting_power_min, design_shooting_power_max);

		entity->transform.rotation = data->shooting_angle;
	}

	// projectile
	{
		Entity* entity = state->projectile;
		if(context->btn_isjustpressed[BTN_TYPE_ACTION_0])
		{
			float spawn_rotation = state->player_data.shooting_angle;
			vec2f spawn_direction = vec2f { SDL_cosf(spawn_rotation), SDL_sinf(spawn_rotation) };
			vec2f spawn_position = state->player->transform.position + spawn_direction * design_cannon_length;
			vec2f spawn_impulse = spawn_direction * state->player_data.shooting_power;
			b2Body_SetLinearVelocity(entity->body_id, b2Vec2_zero);
			b2Body_SetTransform(entity->body_id, value_cast(b2Vec2, spawn_position), b2MakeRot(spawn_rotation));
			b2Body_ApplyLinearImpulseToCenter(entity->body_id, value_cast(b2Vec2, spawn_impulse), true);
			b2Body_Enable(entity->body_id);
		}
	}
}

static void game_update_post_physics(SDLContext* context, GameState* state)
{
	// camera
	{
		const float zoom_speed = 1;
		vec2f camera_offset = vec2f { -5.0f, 4.0f } / context->camera_active->zoom;
		context->camera_active->world_position = camera_offset;
		context->camera_active->zoom += context->mouse_scroll * zoom_speed * context->delta;
	}
}

static void game_render(SDLContext* context, GameState* state)
{
	itu_lib_render_draw_world_grid(context);
	
	// entities
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

	if(DEBUG_physics)
		b2World_Draw(state->world_id, &debug_draw);

	// debug window
	itu_lib_render_draw_world_point(context, VEC2F_ZERO, 10, color { 1, 0, 1, 1 });

	SDL_SetRenderDrawColor(context->renderer, 0xFF, 0x00, 0xFF, 0xff);
	SDL_RenderRect(context->renderer, NULL);

	// imgui debug windows
	{
		debug_player_data(context, state);
	}
}

int main(void)
{
	bool quit = false;
	SDL_Window* window;
	SDLContext context = { 0 };
	GameState  state   = { 0 };

	context.window_w = WINDOW_W;
	context.window_h = WINDOW_H;

	SDL_CreateWindowAndRenderer("ES04.1.1 - Shooting game", WINDOW_W, WINDOW_H, 0, &window, &context.renderer);
	SDL_SetRenderDrawBlendMode(context.renderer, SDL_BLENDMODE_BLEND);
	
	// increase the zoom to make debug text more legible
	// (ie, on the class projector, we will usually use 2)
	{
		context.zoom = 1;
		context.window_w /= context.zoom;
		context.window_h /= context.zoom;
		SDL_SetRenderScale(context.renderer, context.zoom, context.zoom);
	}
	
	itu_lib_imgui_setup(window, &context, true);

	// FIXES: camera improvements
	// 1. store a reference to the SDL context (ugly as hell, but this way we avoid changing all the functions that take a `Camera*` into)
	// 2. size is now expressed as normalized window size (name should be updated, but again we want to update old exercises
	context.camera_default.normalized_screen_size.x = 1.0f;
	context.camera_default.normalized_screen_size.y = 1.0f;
	context.camera_default.zoom = 1;
	context.camera_default.pixels_per_unit = CAMERA_PIXELS_PER_UNIT;

	camera_set_active(&context, &context.camera_default);


	game_init(&context, &state);
	game_reset(&context, &state);

	SDL_Time walltime_frame_beg;
	SDL_Time walltime_frame_end;
	SDL_Time walltime_work_end;
	SDL_Time elapsed_work = 0;
	SDL_Time elapsed_frame = 0;
	SDL_Time accumulator_physics = 0;

	SDL_GetCurrentTime(&walltime_frame_beg);
	walltime_frame_end = walltime_frame_beg;

	while(!quit)
	{
		// input
		SDL_Event event;
		sdl_input_clear(&context);
		while(SDL_PollEvent(&event))
		{
			if(itu_lib_imgui_process_sdl_event(&event))
				continue;
			switch(event.type)
			{
				case SDL_EVENT_QUIT:
					quit = true;
					break;
				// listen for mouse motion and store the absolute position in screen space
				case SDL_EVENT_MOUSE_MOTION:
				{
					context.mouse_pos.x = event.motion.x;
					context.mouse_pos.y = event.motion.y;
					break;
				}
				// listen for mouse wheel and store the relative position in screen space
				case SDL_EVENT_MOUSE_WHEEL:
				{
					context.mouse_scroll = event.wheel.y;
					break;
				}
				case SDL_EVENT_MOUSE_BUTTON_DOWN:
				case SDL_EVENT_MOUSE_BUTTON_UP:
				{
					switch(event.button.button)
					{
						case 1: sdl_input_mouse_button_process(&context, BTN_TYPE_ACTION_0, &event);  break;
						case 3: sdl_input_mouse_button_process(&context, BTN_TYPE_ACTION_1, &event);  break;
					}
					break;
				}
				case SDL_EVENT_KEY_DOWN:
				case SDL_EVENT_KEY_UP:
					switch(event.key.key)
					{
						case SDLK_W: sdl_input_key_process(&context, BTN_TYPE_UP, &event);        break;
						case SDLK_A: sdl_input_key_process(&context, BTN_TYPE_LEFT, &event);      break;
						case SDLK_S: sdl_input_key_process(&context, BTN_TYPE_DOWN, &event);      break;
						case SDLK_D: sdl_input_key_process(&context, BTN_TYPE_RIGHT, &event);     break;
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
							case SDLK_F3: DEBUG_physics = !DEBUG_physics; break;
						}
					}
					break;
			}
		}

		SDL_SetRenderDrawColor(context.renderer, 0x00, 0x00, 0x00, 0x00);
		SDL_RenderClear(context.renderer);

		itu_lib_imgui_frame_begin();

		// update
		game_update(&context, &state);

		// physics
		{
			int physics_steps_count = 0;
			while(accumulator_physics >= PHYSICS_TIMESTEP_NSECS && physics_steps_count < PHYSICS_MAX_TIMESTEPS_PER_FRAME)
			{
				b2World_Step(state.world_id, PHYSICS_TIMESTEP_SECS, 4);
				++physics_steps_count;
				accumulator_physics -= PHYSICS_TIMESTEP_NSECS;
			}

			// entities
			for(int i = 0; i < state.entities_alive_count; ++i)
			{
				Entity* entity = &state.entities[i];
				if(!b2Body_IsValid(entity->body_id))
					continue;
				b2Vec2 physics_vel = b2Body_GetLinearVelocity(entity->body_id);
				b2Vec2 physics_pos = b2Body_GetPosition(entity->body_id);
				b2Rot  physics_rot = b2Body_GetRotation(entity->body_id);
				entity->velocity = value_cast(vec2f, physics_vel); 
				entity->transform.position = value_cast(vec2f, physics_pos);
				entity->transform.rotation = b2Rot_GetAngle(physics_rot);
			}
		}
		game_update_post_physics(&context, &state);

		game_render(&context, &state);

#ifdef ENABLE_DIAGNOSTICS
		// NOTE: moving the diagnostic rendering here means that we are effectively showing information about the previous frame.
		//       previous version (rendering between SDL_DelayNS and SDL_RenderPresent) was showing current frame info,
		//       but at the price of being less precise (we are doing some work AFTER we have done all the timing calculation)
		//       ImGui, while very performant, takes time to do all the rendering, so we have to decide between
		//       - more accurate info, but about previsou frame [WE ARE CURRENTLY DOING THIS]
		//       - info about this frame, but less accurate
		{
			ImGui::Begin("itu_diagnostics");
			ImGui::Text("Timing");
			ImGui::LabelText("work", "%6.3f ms/f", (float)elapsed_work / (float)MILLIS(1));
			ImGui::LabelText("tot", "%6.3f ms/f", (float)elapsed_frame / (float)MILLIS(1));

			ImGui::Text("Debug");
			if(ImGui::Button("[TAB] reset"))
				game_reset(&context, &state);
			ImGui::Checkbox("[F1] render textures", &DEBUG_render_textures);
			ImGui::Checkbox("[F2] render outlines", &DEBUG_render_outlines);
			ImGui::Checkbox("[F3] render physics", &DEBUG_physics);
			ImGui::End();
		}
#endif
		
		itu_lib_imgui_frame_end(&context);

		SDL_GetCurrentTime(&walltime_work_end);
		elapsed_work = walltime_work_end - walltime_frame_beg;

		if(elapsed_work < TARGET_FRAMERATE_NS)
			SDL_DelayNS(TARGET_FRAMERATE_NS - elapsed_work);

		SDL_GetCurrentTime(&walltime_frame_end);
		elapsed_frame = walltime_frame_end - walltime_frame_beg;

		// // NOTE: corrently rendering diagnostics through ImGui (see above)
		// sdl_render_diagnostics(&context, elapsed_work, elapsed_frame);

		// render
		SDL_RenderPresent(context.renderer);

		context.delta = (float)elapsed_frame / (float)SECONDS(1);
		context.uptime += context.delta;
		accumulator_physics += elapsed_frame;
		walltime_frame_beg = walltime_frame_end;
	}
}
