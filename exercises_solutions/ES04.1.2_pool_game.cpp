#define TEXTURE_PIXELS_PER_UNIT 128  // how many pixels of textures will be mapped to a single world unit
#define CAMERA_PIXELS_PER_UNIT  128  // how many pixels of windows will be used to render a single world unit

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
#define BALL_COUNT   64
#define GRAVITY      0.0f

#define COLLISION_FILTER_WALL   0b00001
#define COLLISION_FILTER_HOLE   0b00010
#define COLLISION_FILTER_BALL   0b00100

bool DEBUG_render_textures = true;
bool DEBUG_render_outlines = false;
bool DEBUG_physics = true;
b2DebugDraw debug_draw;

struct Entity
{
	bool alive;
	int index;
	Sprite    sprite;
	Transform transform;
	b2BodyId  body_id;
	vec2f     velocity;
};

struct PlayerData
{
	Entity* entity;
	vec2f p0;
	vec2f p1;
	bool p0_placed;
};

struct BallData
{
	Entity* entity;
};

struct GameState
{
	// shortcut references
	vec2f camera_pos;

	// game-allocated memory
	Entity* entities;
	int entities_alive_count;
	PlayerData player_data;
	BallData   balls_data[BALL_COUNT];

	// SDL-allocated structures
	SDL_Texture* atlas;
	SDL_Texture* bg;

	// box2d
	b2WorldId world_id;
	// hashmap to retrieve entity from bodyId (which is the only thing we have when handling collision ovents from b2d)
	// NOTE: I (chris) used an hashmap to test viability in future exercises. An array + linear search would have been more than enough for our current needs)
	struct { b2BodyId key; Entity* value; } *map_b2body_entity;
};



static Entity* entity_create(GameState* state)
{
	if(!(state->entities_alive_count < ENTITY_COUNT))
		// NOTE: this might as well be an assert, if we don't have a way to recover/handle it
		return NULL;

	// // concise version
	//return &state->entities[state->entities_alive_count++];

	Entity* ret = &state->entities[state->entities_alive_count];
	

	ret->alive = true;
	ret->index = state->entities_alive_count;
	++state->entities_alive_count;
	return ret;
}

static void entity_add_physics_body(GameState* state, Entity* entity, b2BodyDef* body_def)
{
	entity->body_id = b2CreateBody(state->world_id, body_def);
	hmput(state->map_b2body_entity, entity->body_id, entity);
}

// NOTE: this only works if nobody holds references to other entities!
//       if that were the case, we couldn't swap them around.
//       We will see in later lectures how to handle this kind of problems
static void entity_destroy(GameState* state, Entity* entity)
{
	// NOTE: here we want to fail hard, nobody should pass us a pointer not gotten from `entity_create()`
	SDL_assert(entity >= state->entities && entity < state->entities + ENTITY_COUNT);

	b2DestroyBody(entity->body_id);
	hmdel(state->map_b2body_entity, entity->body_id);
	entity->alive = false;
}


// game parameters
vec2f design_area_halfsize = vec2f { 5.4f, 2.7f };
float design_shoot_force_min = 1;
float design_shoot_force_max = 10;
float design_shoot_force_dst_min = 0.1;
float design_shoot_force_dst_max = 4;
float design_balls_radius = 0.2f;
float design_balls_restitution = 0.9f;
float design_balls_friction = 1.2f;
float design_balls_velocity_slowdown_threshold = 0.2f;
float design_balls_velocity_slowdown_factor = 0.95;
int design_triangle_side = 5;
float design_holes_radius = 0.1f;
float design_camera_speed = 1.5f;

void debug_design_data(SDLContext* context)
{
	ImGui::Begin("design_data");
	ImGui::PushItemWidth(120);
	ImGui::DragFloat2("area_halfsize", &(design_area_halfsize.x));
	ImGui::DragFloat("shoot_force_min", &design_shoot_force_min);
	ImGui::DragFloat("shoot_force_max", &design_shoot_force_max);
	ImGui::DragFloat("shoot_force_dst_min", &design_shoot_force_dst_min);
	ImGui::DragFloat("shoot_force_dst_max", &design_shoot_force_dst_max);
	ImGui::DragFloat("balls_radius", &design_balls_radius);
	ImGui::DragFloat("balls_restitution", &design_balls_restitution);
	ImGui::DragFloat("balls_friction", &design_balls_friction);
	ImGui::DragFloat("balls_velocity_slowdown_threshold", &design_balls_velocity_slowdown_threshold);
	ImGui::DragFloat("balls_velocity_slowdown_factor", &design_balls_velocity_slowdown_factor);
	ImGui::DragInt("triangle_side", &design_triangle_side);
	ImGui::DragFloat("camera_spped", &design_camera_speed);
	ImGui::PopItemWidth();
	ImGui::End();
}

static void game_init(SDLContext* context, GameState* state)
{
	// allocate memory
	state->entities = (Entity*)SDL_calloc(ENTITY_COUNT, sizeof(Entity));
	SDL_assert(state->entities);

	state->world_id = { 0 };
	
	state->player_data = { 0 };
	

	// texture atlases
	state->atlas = texture_create(context, "data/kenney/simpleSpace_tilesheet_2.png", SDL_SCALEMODE_LINEAR);
	state->bg = texture_create(context, "data/billiard_table.jpg", SDL_SCALEMODE_LINEAR);

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
	
	context->camera_active->zoom = 0.2f;
	
	// reset body<>entity map
	hmfree(state->map_b2body_entity);

	// background
	{
		Entity* entity = entity_create(state);
		entity->transform.scale = vec2f { 1.0f, 1.0f };
		itu_lib_sprite_init(&entity->sprite, state->bg, SDL_FRect{0, 0, 1704, 980} );
	}

	// balls
	{
		
		b2BodyDef body_def = b2DefaultBodyDef();
		body_def.type = b2_dynamicBody;
		body_def.linearDamping = 0.2f;
		body_def.angularDamping = 0.9f;

		b2ShapeDef shape_def = b2DefaultShapeDef();
		shape_def.filter.categoryBits = COLLISION_FILTER_BALL;
		shape_def.material.restitution = design_balls_restitution;
		shape_def.material.friction = design_balls_friction;
		shape_def.density = 10;
		shape_def.enableSensorEvents = true;
		
		b2Circle circle = { 0 };
		circle.radius = design_balls_radius;

		
		float row_offset_y = SDL_sqrtf((design_balls_radius * 2)*(design_balls_radius * 2) - design_balls_radius*design_balls_radius);
		int row_size = 1;
		int row_count = design_triangle_side;
		for(int i = 0; i < row_count; ++i)
		{
			float row_offset_x = -design_balls_radius * i;
			for(int j = 0; j < row_size; ++j)
			{
				Entity* entity = entity_create(state);

				entity->transform.scale = vec2f { .5f, .5f };
				itu_lib_sprite_init(&entity->sprite, state->atlas, itu_lib_sprite_get_rect(0, 4, TEXTURE_PIXELS_PER_UNIT, TEXTURE_PIXELS_PER_UNIT));

				body_def.position = b2Vec2
				{
					i * (row_offset_y) + 2.0f,
					j * (design_balls_radius) * 2 + row_offset_x,
				};
				
				entity_add_physics_body(state, entity, &body_def);
				b2CreateCircleShape(entity->body_id, &shape_def, &circle);
			}
			++row_size;
		}

		// add extra one for flare
		Entity* entity = entity_create(state);
		entity->transform.scale = vec2f { .5f, .5f };
		itu_lib_sprite_init(&entity->sprite, state->atlas, itu_lib_sprite_get_rect(0, 4, TEXTURE_PIXELS_PER_UNIT, TEXTURE_PIXELS_PER_UNIT));

		body_def.position = b2Vec2 { -2, 0, };
				
		entity_add_physics_body(state, entity, &body_def);
		b2CreateCircleShape(entity->body_id, &shape_def, &circle);
	}

	// walls
	{
		b2BodyDef body_def = b2DefaultBodyDef();
		body_def.type = b2_staticBody;

		b2ShapeDef shape_def = b2DefaultShapeDef();
		shape_def.filter.categoryBits = COLLISION_FILTER_WALL;

		b2Polygon polygon_lr = b2MakeBox(0.5f, design_area_halfsize.y);
		b2Polygon polygon_tb = b2MakeBox(design_area_halfsize.x, 0.5f);

		for(int i = 0; i < 4; ++i)
		{
			int x, y;
			b2Polygon* wall_shape;

			// compute sides coordinates and choose polygon shape from index
			if(i / 2 == 0)
			{
				// left-right walls
				x = (i % 2) * 2 - 1;
				y = 0;
				wall_shape = &polygon_lr;
			}
			else
			{
				// top-down walls
				x = 0;
				y = (i % 2) * 2 - 1;
				wall_shape = &polygon_tb;
			}

			body_def.position = b2Vec2{ x * (design_area_halfsize.x + 0.5f), y * (design_area_halfsize.y + 0.5f) };

			Entity* entity = entity_create(state);
			entity->transform.scale = VEC2F_ONE;
			itu_lib_sprite_init(&entity->sprite, NULL, { 0 });
			entity_add_physics_body(state, entity, &body_def);
			b2CreatePolygonShape(entity->body_id, &shape_def, wall_shape);
		}
	}

	// holes
	{
		b2BodyDef body_def = b2DefaultBodyDef();
		body_def.type = b2_staticBody;

		b2ShapeDef shape_def = b2DefaultShapeDef();
		shape_def.filter.categoryBits = COLLISION_FILTER_HOLE;
		shape_def.filter.maskBits = COLLISION_FILTER_BALL;
		shape_def.enableSensorEvents = true;
		shape_def.isSensor = true;

		b2Circle circle = { 0 };
		circle.radius = design_holes_radius;

		for(int i = 0; i < 6; ++i)
		{
			int x = (i % 3)-1;
			int y = (i / 3)*2-1;

			body_def.position = b2Vec2{ x * (design_area_halfsize.x), y * (design_area_halfsize.y) };

			Entity* entity = entity_create(state);
			entity->transform.scale = VEC2F_ONE;

			entity_add_physics_body(state, entity, &body_def);
			b2CreateCircleShape(entity->body_id, &shape_def, &circle);
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
	// small kinematic tweaks to physics
	{
		for(int i = 0; i < BALL_COUNT; ++i)
		{
			BallData* data = &state->balls_data[i];
			if(!data->entity)
				continue;

			Entity* entity = data->entity;
			if(!entity->alive)
				continue;

			// add artifial slowdown to simulate rough surface
			if(length_sq(entity->velocity) < design_balls_velocity_slowdown_threshold*design_balls_velocity_slowdown_threshold)
			{
				entity->velocity = entity->velocity * design_balls_velocity_slowdown_factor;
				b2Body_SetLinearVelocity(entity->body_id, value_cast(b2Vec2, entity->velocity));
				// enable tinting to see precisely when the exponential velocity decay kicks in
				// entity->sprite.tint = COLOR_RED;
			}
			else
				entity->sprite.tint = COLOR_WHITE;
		}
	}

	// input
	{
		PlayerData* data = &state->player_data;
		if(context->btn_isjustpressed[BTN_TYPE_ACTION_0])
		{
			if(data->p0_placed)
			{
				// check if shot hit a target
				b2QueryFilter filter;
				filter.maskBits = COLLISION_FILTER_BALL;
				vec2f ray_offset = data->p1 - data->p0;
				b2RayResult res = b2World_CastRayClosest(state->world_id, value_cast(b2Vec2, data->p0), value_cast(b2Vec2, ray_offset), filter);

				if(res.hit)
				{
					float t = distance_sq(data->p0, data->p1) / (design_shoot_force_dst_max - design_shoot_force_dst_min);
					vec2f impulse_dir = normalize(data->p1 - data->p0);
					float impulse_strength = (design_shoot_force_max - design_shoot_force_min) * t;
					impulse_strength = SDL_clamp(impulse_strength, design_shoot_force_min, design_shoot_force_max);
					vec2f impulse = impulse_dir * impulse_strength;

					b2BodyId target = b2Shape_GetBody(res.shapeId);
					b2Body_ApplyLinearImpulse(target, value_cast(b2Vec2, impulse), res.point, true);
				}

				data->p0_placed = false;
			}
			else
			{
				data->p0 = point_screen_to_global(context, context->mouse_pos);
				data->p0_placed = true;
			}
		}

		if(data->p0_placed)
		{
			data->p1 =  point_screen_to_global(context, context->mouse_pos);
		}
	}

	// camera
	{
		if(context->btn_isdown[BTN_TYPE_UP])
			state->camera_pos.y += design_camera_speed * context->delta;
		if(context->btn_isdown[BTN_TYPE_DOWN])
			state->camera_pos.y -= design_camera_speed * context->delta;
		if(context->btn_isdown[BTN_TYPE_LEFT])
			state->camera_pos.x -= design_camera_speed * context->delta;
		if(context->btn_isdown[BTN_TYPE_RIGHT])
			state->camera_pos.x += design_camera_speed * context->delta;

		const float zoom_speed = 1;
		vec2f camera_offset = vec2f { -1.5f, 0.0f } / context->camera_active->zoom;
		context->camera_active->world_position = state->camera_pos + camera_offset;
		context->camera_active->zoom += context->mouse_scroll * zoom_speed * context->delta;
	}
}

static void game_update_post_physics(SDLContext* context, GameState* state)
{
	// sensor events
	{
		b2SensorEvents sensor_events = b2World_GetSensorEvents(state->world_id);
		for(int i = 0; i < sensor_events.beginCount; ++i)
		{
			b2SensorBeginTouchEvent* sensor_data = &sensor_events.beginEvents[i];
			b2Filter filter_sensor = b2Shape_GetFilter(sensor_data->sensorShapeId);
			b2Filter filter_visitor = b2Shape_GetFilter(sensor_data->visitorShapeId);
			if(filter_sensor.categoryBits == COLLISION_FILTER_HOLE && filter_visitor.categoryBits == COLLISION_FILTER_BALL)
			{
				b2BodyId body_id = b2Shape_GetBody(sensor_data->visitorShapeId);
				Entity* entity = hmget(state->map_b2body_entity, body_id);
				if(!entity)
				{
					SDL_Log("error!");
					continue;
				}
				entity_destroy(state, entity);
			}
		}
	}
}

static void game_render(SDLContext* context, GameState* state)
{
	itu_lib_render_draw_world_grid(context);
	
	// entities
	for(int i = 0; i < state->entities_alive_count; ++i)
	{
		Entity* entity = &state->entities[i];
		if(!entity->alive)
			continue;

		// render texture
		SDL_FRect rect_src = entity->sprite.rect;
		SDL_FRect rect_dst;

		if(DEBUG_render_textures)
			itu_lib_sprite_render(context, &entity->sprite, &entity->transform);

		if(DEBUG_render_outlines)
			itu_lib_sprite_render_debug(context, &entity->sprite, &entity->transform);
	}

	// player aim
	{
		PlayerData* data = &state->player_data;
		if(data->p0_placed)
		{
			itu_lib_render_draw_world_point(context, data->p0, 5, COLOR_YELLOW);
			itu_lib_render_draw_world_line(context, data->p0, data->p1, COLOR_YELLOW);
		}
	}

	if(DEBUG_physics)
		b2World_Draw(state->world_id, &debug_draw);

	// debug window
	itu_lib_render_draw_world_point(context, VEC2F_ZERO, 10, color { 1, 0, 1, 1 });

	SDL_SetRenderDrawColor(context->renderer, 0xFF, 0x00, 0xFF, 0xff);
	SDL_RenderRect(context->renderer, NULL);

	// imgui debug windows
	{
		debug_design_data(context);
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

	SDL_CreateWindowAndRenderer("ES04.1.2 - Pool game", WINDOW_W, WINDOW_H, 0, &window, &context.renderer);
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

		SDL_SetRenderDrawColor(context.renderer, 0x00, 0x00, 0x00, 0xFF);
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

				game_update_post_physics(&context, &state);
			}

			// update entities entities
			for(int i = 0; i < state.entities_alive_count; ++i)
			{
				Entity* entity = &state.entities[i];
				if(!entity->alive)
					continue;

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
