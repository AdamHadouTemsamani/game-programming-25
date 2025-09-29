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

#define COLLISION_FILTER_PLAYER         0b00001
#define COLLISION_FILTER_GROUND         0b00010
#define COLLISION_FILTER_CLUTTER        0b00100
#define COLLISION_FILTER_CLUTTER_SENSOR 0b01000

bool DEBUG_render_textures = true;
bool DEBUG_render_outlines = false;
bool DEBUG_physics = true;
bool DEBUG_physics_fixed_step = true;

int DEBUG_physics_steps_per_frame_counter;
float DEBUG_delay_physics_ms;
float DEBUG_delay_frame_ms;

b2DebugDraw debug_draw;

struct Entity
{
	Sprite    sprite;
	Transform transform;
	b2BodyId body_id;
	vec2f    velocity;
};

typedef struct PlayerData
{
	// definitions
	float h;   // jump height
	float x_h; // jump horizontal distance

	bool grounded;

	// runtime (jupm)
	float g;   // gravity (for current jump)
	vec2f p_0; // initial position (for current jump)
	float v_0; // initial VERTICAL velocity (for current jump)
	float v_x; // initial foot speed (for current jump)
	float t_h; // jump duration (for current jump)
} PlayerData;

struct GameState
{
	// shortcut references
	Entity* player;

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

void compute_jump_parameters(PlayerData* data)
{
	data->grounded = false;
	data->v_0 = (2*data->h*data->v_x) / (data->x_h);
	data->g   = (-2*data->h * data->v_x * data->v_x) / (data->x_h * data->x_h);
}

void clutter_apply_impulse_random(b2ShapeId clutte_entity, b2Vec2 direction, float amount, float spread)
{
	
	float angle = (SDL_randf() - 0.5f) * spread;

	b2Vec2 point   = b2Vec2_zero;
	b2Vec2 impulse = b2RotateVector(b2MakeRot(angle), direction);
	impulse = b2MulSV(amount, impulse);

	b2BodyId body_id = b2Shape_GetBody(clutte_entity);
	b2Body_ApplyLinearImpulse(body_id, impulse, point, true);
}

static void game_init(SDLContext* context, GameState* state)
{
	// allocate memory
	state->entities = (Entity*)SDL_calloc(ENTITY_COUNT, sizeof(Entity));
	SDL_assert(state->entities);

	state->world_id = { 0 };
	
	state->player_data = { 0 };
	state->player_data.g = -66.67f;
	state->player_data.h   = 3.0f;
	state->player_data.x_h = 1.5f;
	state->player_data.v_x = 5.0f;

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

	// player
	{
		Entity* entity = entity_create(state);
		state->player = entity;
		entity->transform.position = VEC2F_ZERO;
		entity->transform.scale = VEC2F_ONE;
		itu_lib_sprite_init(
			&entity->sprite,
			state->atlas,
			itu_lib_sprite_get_rect(0, 9, 16, 16)
		);
		entity->sprite.pivot.y = 0;

		// box2d body, shape and polygon
		{
			vec2f size = itu_lib_sprite_get_world_size(context, &entity->sprite, &entity->transform);
			vec2f offset = -mul_element_wise(size, entity->sprite.pivot - vec2f{ 0.5f, 0.5f });

			b2BodyDef body_def = b2DefaultBodyDef();
			body_def.type = b2_dynamicBody;
			body_def.fixedRotation = true;
			body_def.position = b2Vec2{ 0, 0 };

			b2ShapeDef shape_def = b2DefaultShapeDef();
			shape_def.density = 1; // NOTE: default density of 0 will mess with collisions and gravity!
			shape_def.enableSensorEvents  = true;
			shape_def.enableContactEvents = true;
			shape_def.enableHitEvents     = true;
			shape_def.filter.categoryBits = COLLISION_FILTER_PLAYER;
			shape_def.filter.maskBits = COLLISION_FILTER_GROUND | COLLISION_FILTER_CLUTTER_SENSOR;
			b2Polygon polygon = b2MakeOffsetBox(size.x / 2, size.y / 2, value_cast(b2Vec2, offset), b2MakeRot(entity->transform.rotation));
			b2Circle circle;
			circle.radius = 0.5f;
			circle.center = value_cast(b2Vec2, offset);
			entity->body_id = b2CreateBody(state->world_id, &body_def);
			b2CreateCircleShape(entity->body_id, &shape_def, &circle);
		}
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

#if 1
	// clutter
	{
		b2BodyDef body_def = b2DefaultBodyDef();
		body_def.type = b2_dynamicBody;
		body_def.fixedRotation = false;

		// collider shape (to enable collisions with the ground)
		b2ShapeDef shape_def = b2DefaultShapeDef();
		shape_def.density = 1;
		shape_def.filter.categoryBits = COLLISION_FILTER_CLUTTER;

		// sensor shape (to enable interaction with the player)
		b2ShapeDef shape_def_clutter = b2DefaultShapeDef();
		shape_def_clutter.density = 0;
		shape_def_clutter.isSensor = true;
		shape_def_clutter.enableSensorEvents = true;
		shape_def_clutter.filter.categoryBits = COLLISION_FILTER_CLUTTER_SENSOR;
		shape_def_clutter.filter.maskBits     = COLLISION_FILTER_PLAYER;

		b2Polygon polygon_box = b2MakeBox(0.5f, 0.5f);
		for(int i = 0; i < 32; ++i)
		{
			Entity* entity = entity_create(state);
			entity->transform.scale = VEC2F_ONE;
			
			vec2f size = itu_lib_sprite_get_world_size(context, &entity->sprite, &entity->transform);
			vec2f offset = -mul_element_wise(size, entity->sprite.pivot - vec2f{ 0.5f, 0.5f });

			body_def.position = b2Vec2{ 3.0f + (i % 4) * 1.5f, (i / 4) * 3.0f };
			body_def.rotation = b2MakeRot(SDL_randf() * TAU);
			body_def.angularVelocity = 1;
			entity->body_id = b2CreateBody(state->world_id, &body_def);
			b2CreatePolygonShape(entity->body_id, &shape_def, &polygon_box);
			b2CreatePolygonShape(entity->body_id, &shape_def_clutter, &polygon_box);
			itu_lib_sprite_init(
				&entity->sprite,
				state->atlas,
				itu_lib_sprite_get_rect(3, 5, 16, 16)
			);
		}
	}
#endif

	// debug draw
	debug_draw.context = context;
	debug_draw.drawShapes = true;
	debug_draw.DrawSolidPolygonFcn = fn_box2d_wrapper_draw_polygon;
	debug_draw.DrawSolidCircleFcn = fn_box2d_wrapper_draw_circle;
}

static void game_update(SDLContext* context, GameState* state)
{
	// player
	{
		Entity* player = state->player;
		PlayerData* data = &state->player_data;
		vec2f velocity = player->velocity;
		if(data->grounded)
		{
			if(context->btn_isdown[BTN_TYPE_LEFT])
		 		velocity.x = -data->v_x;
		 	else if(context->btn_isdown[BTN_TYPE_RIGHT])
		 		velocity.x = data->v_x;
			else
				velocity.x = 0;
				 
			if(context->btn_isjustpressed[BTN_TYPE_SPACE])
			{
				compute_jump_parameters(data);
				velocity.y = state->player_data.v_0;
			}
		}
		else
		{
			if(velocity.y > 0)
				velocity.y += state->player_data.g * context->delta;
			else
				velocity.y += state->player_data.g * context->delta * 3;
		}

		b2Body_SetLinearVelocity(player->body_id, value_cast(b2Vec2, velocity));
	}
}

static void game_update_post_physics(SDLContext* context, GameState* state)
{
	// player
	{
		Entity* entity = state->player;
		PlayerData* data = &state->player_data;
	
		
		static b2ContactData* contact_data = (b2ContactData*)SDL_calloc(PHYSICS_MAX_CONTACTS_PER_ENTITY, sizeof(b2ContactData));
	
		int contacts = b2Body_GetContactCapacity(entity->body_id);
		SDL_assert(contacts <= PHYSICS_MAX_CONTACTS_PER_ENTITY && "Max number of contacts exceeded. If this is not an error, increase the ");

		int actual_contacts = b2Body_GetContactData(state->player->body_id, contact_data, contacts);
		 
		data->grounded = false;
		for(int i = 0; i < actual_contacts; ++i)
		{
			b2Filter filter_a = b2Shape_GetFilter(contact_data[i].shapeIdA);
			if(filter_a.categoryBits & COLLISION_FILTER_GROUND)
		 		data->grounded = true;
		}
	}

	// world
	b2SensorEvents world_sensor_events = b2World_GetSensorEvents(state->world_id);
	for(int i = 0; i < world_sensor_events.beginCount; ++i)
	{
		b2SensorBeginTouchEvent* sensor_event = &world_sensor_events.beginEvents[i];
		b2Vec2 direction = b2Vec2 { 0, 1 };

		float vel_sq = length_sq(state->player->velocity);
		if(SDL_fabsf(vel_sq) < FLOAT_EPSILON)
			// apply impulse only if the player is moving
			// (boxes falling on player when it's not moving feel unnatural)
			continue;

		float amount = SDL_clamp(length_sq(state->player->velocity) * 2, 2, 4);
		float spread = 0;
		clutter_apply_impulse_random(sensor_event->sensorShapeId, direction, amount, spread);
	}

	{
		const float zoom_speed = 1;
		vec2f camera_offset = vec2f { -5.0f, 3.0f } / context->camera_active->zoom;
		// camera follows player
		context->camera_active->world_position = state->player->transform.position + camera_offset;
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
}

int main(void)
{
	bool quit = false;
	SDL_Window* window;
	SDLContext context = { 0 };
	GameState  state   = { 0 };

	context.window_w = WINDOW_W;
	context.window_h = WINDOW_H;

	SDL_CreateWindowAndRenderer("ES04.2 - Physics step", WINDOW_W, WINDOW_H, 0, &window, &context.renderer);
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
							case SDLK_F3: DEBUG_physics = !DEBUG_physics; break;
							case SDLK_F4: DEBUG_physics_fixed_step = !DEBUG_physics_fixed_step; accumulator_physics = 0; break;
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
		engine_artificial_delay(DEBUG_delay_frame_ms, 0.0f);

		// physics
		{
			DEBUG_physics_steps_per_frame_counter = 0;
			if(DEBUG_physics_fixed_step)
			{
				// decouple physics step from framerate, running 0, 1 or multiple physics step per frame
				while(accumulator_physics >= PHYSICS_TIMESTEP_NSECS && DEBUG_physics_steps_per_frame_counter < PHYSICS_MAX_TIMESTEPS_PER_FRAME)
				{
					b2World_Step(state.world_id, PHYSICS_TIMESTEP_SECS, 4);
					engine_artificial_delay(DEBUG_delay_physics_ms, 0.0f);
					++DEBUG_physics_steps_per_frame_counter;
					accumulator_physics -= PHYSICS_TIMESTEP_NSECS;
				}
			}
			else
			{
				// run the simulation in synch with the rest of the game. Works fine as long as
				// 1. PHYSICS_TIMESTEP == TARGET_FRAMERATE (which we can ensure, it's just parameters that we have to decide)
				// 2. computing a frame takes less than PHYSICS_TIMESTEP (which we CANNOT ensure, of course)
				b2World_Step(state.world_id, PHYSICS_TIMESTEP_SECS, 4);
				engine_artificial_delay(DEBUG_delay_physics_ms, 0.0f);
				++DEBUG_physics_steps_per_frame_counter;
			}

			// entities
			for(int i = 0; i < state.entities_alive_count; ++i)
			{
				Entity* entity = &state.entities[i];
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

			ImGui::Text("Simulation config");
			b2Vec2 player_pos_physics = b2Body_GetPosition(state.player->body_id);
			ImGui::LabelText("player pos game ", "%4.2f   %4.2f", state.player->transform.position.x, state.player->transform.position.y);
			ImGui::LabelText("player pos box2d", "%4.2f   %4.2f", player_pos_physics.x, player_pos_physics.y);
			b2Vec2 velocity = b2Body_GetLinearVelocity(state.player->body_id);
			ImGui::LabelText("player velocity", "%4.2f   %4.2f", velocity.x, velocity.y);

			ImGui::Text("Jump params");
			ImGui::DragFloat("horizontal velocity", &state.player_data.v_x);
			ImGui::DragFloat("jump height", &state.player_data.h);
			ImGui::DragFloat("jump distance", &state.player_data.x_h);
			ImGui::LabelText("jump gravity", "%4.2f", state.player_data.g);
			ImGui::LabelText("jump initial vertical velocity", "%4.2f", state.player_data.v_0);

			ImGui::Text("Physics Timing params");
			ImGui::LabelText("steps per frame", "%3d", DEBUG_physics_steps_per_frame_counter);
			ImGui::LabelText("accumulator", "%6.3f ms/f", (float)accumulator_physics  / (float)MILLIS(1));
			ImGui::DragFloat("delay (physics)", &DEBUG_delay_physics_ms, 0.1f, 0.0f, 1000.0f);
			ImGui::DragFloat("delay (frame)", &DEBUG_delay_frame_ms, 0.1f, 0.0f, 1000.0f);
			

			ImGui::Text("Debug");
			if(ImGui::Button("[TAB] reset"))
				game_reset(&context, &state);
			ImGui::Checkbox("[F1] render textures", &DEBUG_render_textures);
			ImGui::Checkbox("[F2] render outlines", &DEBUG_render_outlines);
			ImGui::Checkbox("[F3] render physics", &DEBUG_physics);
			if(ImGui::Checkbox("[F4] use fixed step", &DEBUG_physics_fixed_step))
				accumulator_physics = 0;
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
