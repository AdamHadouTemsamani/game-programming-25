#define TEXTURE_PIXELS_PER_UNIT 64   // how many pixels of textures will be mapped to a single world unit
#define CAMERA_PIXELS_PER_UNIT  32   // how many pixels of windows will be used to render a single world unit

#include <itu_unity_include.hpp>

#define ENABLE_DIAGNOSTICS

// rendering framerate
#define TARGET_FRAMERATE_NS    SECONDS(1) / 60

// physics timestep
#define PHYSICS_TIMESTEP_NSECS  SECONDS(1) / 60
#define PHYSICS_TIMESTEP_SECS   NS_TO_SECONDS(PHYSICS_TIMESTEP_NSECS)
#define PHYSICS_MAX_TIMESTEPS_PER_FRAME 4
#define PHYSICS_MAX_CONTACTS_PER_ENTITY 16

#define WINDOW_W         800
#define WINDOW_H         600

#define ENTITY_COUNT   1024
#define PLATFORM_COUNT   32

#define GRAVITY  -9.8f

#define COLLISION_FILTER_PLAYER         0b00001
#define COLLISION_FILTER_GROUND         0b00010

bool DEBUG_render_textures = true;
bool DEBUG_render_outlines = false;
bool DEBUG_physics = true;



// design variables
float design_player_hor_speed_ground_max = 7.5f;
float design_player_hor_speed_air_max    = 5.0f;
float design_player_hor_accel_groud = 125.0f;
float design_player_hor_accel_air   = 125.0f;
float design_player_hor_decel_factor_ground = 0.90f;
float design_player_hor_decel_factor_air    = 0.99f;
float design_player_jump_height_max = 3.0f;
float design_player_jump_hor_distance_to_apex_max = 3.0f;
// how much the normal can change from the perfect 4 cardinal coordinates to consider a piece of terrain "ground", "wall" or "ceiling"
// NOTE: not tested with values different from 50%, movement code may need some tweaks to handle collisions that do not fall in the 4 cardina directions
float design_player_surface_normal_diff_threshold = 0.5f;
struct Entity
{
	Sprite    sprite;
	Transform transform;
	PhysicsData physics_data;
};

struct PlayerData
{
	vec2f normal_ground;
	vec2f velocity_ground;
	vec2f velocity_desired;
	bool is_grounded;
	bool is_colliding_left;
	bool is_colliding_right;
	bool is_colliding_top;
	b2ShapeId colliding_shape_ground;
	b2ShapeId colliding_shape_left;
	b2ShapeId colliding_shape_right;
	b2ShapeId colliding_shape_top;
	float g;   // gravity
};

struct GameState
{
	// shortcut references
	Entity* player;
	
	// game-allocated memory
	Entity* entities;
	int entities_alive_count;
	PlayerData player_data;

	// SDL-allocated structures
	SDL_Texture* atlas_character;
	SDL_Texture* atlas_tiles;
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



void debug_ui_player_data(GameState* state)
{
	Entity* entity = state->player;
	PlayerData* data = &state->player_data;

	ImGui::Begin("game_player");
	ImGui::PushItemWidth(120);

	ImGui::CollapsingHeader("Player (design)", ImGuiTreeNodeFlags_Leaf);
	
	ImGui::DragFloat("hor_speed_ground_max", &design_player_hor_speed_ground_max);
	ImGui::DragFloat("hor_speed_air_max", &design_player_hor_speed_air_max);
	ImGui::DragFloat("hor_accel_groud", &design_player_hor_accel_groud);
	ImGui::DragFloat("hor_accel_air", &design_player_hor_accel_air);
	ImGui::DragFloat("hor_decel_factor_ground", &design_player_hor_decel_factor_ground);
	ImGui::DragFloat("hor_decel_factor_air", &design_player_hor_decel_factor_air);
	ImGui::DragFloat("jump_height_max", &design_player_jump_height_max);
	ImGui::DragFloat("jump_hor_distance_to_apex_max", &design_player_jump_hor_distance_to_apex_max);

	ImGui::CollapsingHeader("Player (Runtime)", ImGuiTreeNodeFlags_Leaf);
	ImGui::LabelText("position", "%4.2f   %4.2f", entity->transform.position.x, entity->transform.position.y);
	ImGui::LabelText("velocity",  "%4.2f   %4.2f", entity->physics_data.velocity.x, entity->physics_data.velocity.y);
	ImGui::LabelText("gravity", "%4.2f", data->g);
	ImGui::LabelText("grounded", "%s", data->is_grounded        ? "X" : "O");
	ImGui::LabelText("Lcollide", "%s", data->is_colliding_left  ? "X" : "O");
	ImGui::LabelText("Rcollide", "%s", data->is_colliding_right ? "X" : "O");
	ImGui::LabelText("Tcollide", "%s", data->is_colliding_top   ? "X" : "O");
	ImGui::Separator();
	ImGui::LabelText("velocity desired",  "%4.2f   %4.2f", data->velocity_desired.x, data->velocity_desired.y);
	ImGui::LabelText("velocity ground" ,  "%4.2f   %4.2f", data->velocity_ground.x, data->velocity_ground.y);

	ImGui::PopItemWidth();
	ImGui::End();
}

// NOTE: this only works if nobody holds references to other entities!
//       if that were the case, we couldn't swap them around.
//       We will see in later lectures how to handle this kind of problems
static void entity_destroy(GameState* state, Entity* entity)
{
	// NOTE: here we want to fail hard, nobody should pass us a pointer not gotten from `entity_create()`
	SDL_assert(entity < state->entities || entity > state->entities + ENTITY_COUNT);

	--state->entities_alive_count;
	*entity = state->entities[state->entities_alive_count];
}

void compute_jump_parameters(Entity* entity, float* out_vertical_speed, float* out_gravity)
{
	float v_x = design_player_hor_speed_ground_max;
	float h = design_player_jump_height_max;
	float x_h = design_player_jump_hor_distance_to_apex_max;
	*out_vertical_speed = (2 * h * v_x) / x_h;
	*out_gravity        = (-2 * h * v_x*v_x) / (x_h*x_h);
}

static void add_terrain_piece(b2BodyId id, vec2f halfsize, vec2f position, float rotation)
{
	b2ShapeDef shape_def = b2DefaultShapeDef();
	shape_def.filter.categoryBits = COLLISION_FILTER_GROUND;

	b2Polygon polygon = b2MakeOffsetBox(halfsize.x, halfsize.y, value_cast(b2Vec2, position), b2MakeRot(rotation));

	b2CreatePolygonShape(id, &shape_def, &polygon);
}

static void game_init(SDLContext* context, GameState* state)
{
	// allocate memory
	state->entities = (Entity*)SDL_calloc(ENTITY_COUNT, sizeof(Entity));
	SDL_assert(state->entities);
	
	// texture atlases
	state->atlas_character = texture_create(context, "data/kenney/character_femalePerson_sheet.png", SDL_SCALEMODE_LINEAR);
	state->atlas_tiles     = texture_create(context, "data/kenney/spritesheet-tiles-default.png", SDL_SCALEMODE_LINEAR);

	itu_sys_physics_init(context);

	// TEST to see if audio is working
	MIX_Mixer* mixer;
	MIX_Audio* music;
	VALIDATE_PANIC(SDL_Init(SDL_INIT_AUDIO));
	VALIDATE_PANIC(MIX_Init());
	VALIDATE_PANIC(mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL));
	VALIDATE_PANIC(music = MIX_LoadAudio(mixer, "data/opengameart.org/music_level_0.ogg", false));
	VALIDATE_PANIC(MIX_PlayAudio(mixer, music));
}

static void game_reset(SDLContext* context, GameState* state)
{
	// TMP reset uptime (should probably be two different variables
	context->uptime = 0;

	b2WorldDef def_world = b2DefaultWorldDef();
	def_world.gravity.y = GRAVITY;
	itu_sys_physics_reset(&def_world);


	// player
	{
		Entity* entity = entity_create(state);
		state->player = entity;
		state->player_data = { 0 };
		state->player_data.g = -66.67f;


		entity->transform.position = VEC2F_ONE;
		entity->transform.scale = VEC2F_ONE;
		itu_lib_sprite_init(
			&entity->sprite,
			state->atlas_character,
			SDL_FRect { 0, 0, 96, 128 } 
		);
		entity->sprite.pivot.y = 0;

		// box2d body, shape and polygon
		{
			vec2f size = itu_lib_sprite_get_world_size(context, &entity->sprite, &entity->transform);
			vec2f offset = -mul_element_wise(size, entity->sprite.pivot - vec2f{ 0.5f, 0.5f });

			b2BodyDef body_def = b2DefaultBodyDef();
			body_def.type = b2_dynamicBody;
			body_def.fixedRotation = true;
			body_def.position = b2Vec2{ 0, 1 };
			body_def.name = "player";
			body_def.gravityScale = 0;
			body_def.sleepThreshold = 0.1f;

			b2ShapeDef shape_def = b2DefaultShapeDef();
			shape_def.density = 1; // NOTE: default density of 0 will mess with collisions and gravity!
			shape_def.material.friction = 0.0f;
			shape_def.enableSensorEvents  = true;
			shape_def.enableContactEvents = true;
			shape_def.enableHitEvents     = true;
			shape_def.filter.categoryBits = COLLISION_FILTER_PLAYER;
			shape_def.filter.maskBits     = COLLISION_FILTER_GROUND;
			
			b2Capsule capsule = { 0 };
			capsule.radius = 0.5f;
			capsule.center1.y =  1.0f ;
			capsule.center2.y =  0.5f;

			entity->physics_data.body_id = itu_sys_physics_add_body(entity, &body_def);
			b2CreateCapsuleShape(entity->physics_data.body_id, &shape_def, &capsule);
		}
	}

	// terrain
	{
		
		b2BodyDef body_def = b2DefaultBodyDef();
		body_def.type = b2_staticBody;
		body_def.name = "terrain";
		Entity* entity = entity_create(state);
		
		entity->physics_data.body_id = itu_sys_physics_add_body(entity, &body_def);
		add_terrain_piece(entity->physics_data.body_id, vec2f { 32, 0.5f }, vec2f { 0, -0.5f }, 0); // ground
		add_terrain_piece(entity->physics_data.body_id, vec2f { 3, 0.5f }, vec2f { 7, 6 }, PI_HALF); // wall
	}

	// door
	{
		Entity* entity = entity_create(state);
	
		entity->transform.position.x = 7;
		entity->transform.position.y = 1.5;
		entity->transform.scale = VEC2F_ONE;

		b2BodyDef body_def = b2DefaultBodyDef();
		body_def.type = b2_kinematicBody;
		body_def.name = "door";
		body_def.position = value_cast(b2Vec2, entity->transform.position);

		b2ShapeDef shape_def = b2DefaultShapeDef();
		shape_def.filter.categoryBits = COLLISION_FILTER_GROUND;
		shape_def.filter.maskBits     = COLLISION_FILTER_PLAYER;

		b2Polygon polygon = b2MakeOffsetBox(1.5f, 0.5f, b2Vec2_zero, b2MakeRot(PI_HALF));


		entity->physics_data.body_id = itu_sys_physics_add_body(entity, &body_def);
		b2CreatePolygonShape(entity->physics_data.body_id, &shape_def, &polygon);

		itu_lib_sprite_init(
			&entity->sprite,
			state->atlas_tiles,
			SDL_FRect { 0, 64*9, 64, 64*3 } 
		);
	}
}

static void game_update(SDLContext* context, GameState* state)
{
	// player
	{
		Entity* entity = state->player;
		PlayerData* data = &state->player_data;
		vec2f velocity = data->velocity_desired;

		float hor_accel;
		float hor_decel_factor;
		float hor_speed_max;
		if(data->is_grounded)
		{
			hor_accel = design_player_hor_accel_groud;
			hor_decel_factor = design_player_hor_decel_factor_ground;
			hor_speed_max = design_player_hor_speed_ground_max;
		}
		else
		{
			hor_accel = design_player_hor_accel_air;
			hor_decel_factor = design_player_hor_decel_factor_air;
			hor_speed_max = design_player_hor_speed_air_max;
		}

		velocity.x = SDL_clamp(velocity.x, -hor_speed_max, hor_speed_max);

		if(data->is_grounded)
		{
			
			if(context->btn_isdown[BTN_TYPE_LEFT])
				velocity += cross_triplet(data->normal_ground, VEC2F_LEFT, data->normal_ground) * hor_accel * context->delta;
			else if(context->btn_isdown[BTN_TYPE_RIGHT])
				velocity += cross_triplet(data->normal_ground, VEC2F_RIGHT, data->normal_ground) * hor_accel * context->delta;
			else
				velocity = velocity * hor_decel_factor;

			velocity = clamp(velocity, hor_speed_max);

			if(data->is_colliding_left && velocity.x < 0)
				velocity = VEC2F_ZERO;
			if(data->is_colliding_right && velocity.x > 0)
				velocity = VEC2F_ZERO;

			if(context->btn_isjustpressed[BTN_TYPE_SPACE])
				compute_jump_parameters(entity, &velocity.y, &data->g);

		}
		else
		{
			if(data->is_colliding_top)
				velocity.y = SDL_min(velocity.y, 0);

			if(context->btn_isdown[BTN_TYPE_LEFT])
				velocity.x -= hor_accel * context->delta;
			else if(context->btn_isdown[BTN_TYPE_RIGHT])
				velocity.x += hor_accel * context->delta;
			else
				velocity.x *= hor_decel_factor;

			if(velocity.y > 0)
				velocity.y += state->player_data.g * context->delta;
			else
				velocity.y += state->player_data.g * context->delta * 3;
		}
		
		data->velocity_desired = velocity;
		vec2f velocity_total = velocity + data->velocity_ground;

		float TMP_player_speed_threshold = 0.1f;
		if(length_sq(velocity_total) < TMP_player_speed_threshold)
			velocity_total = VEC2F_ZERO;
		b2Body_SetLinearVelocity(entity->physics_data.body_id, value_cast(b2Vec2, velocity_total));
	}
}

static void game_update_post_physics(SDLContext* context, GameState* state)
{
	// player
	Entity* entity = state->player;
	PlayerData* data = &state->player_data;
	
		
	static b2ContactData* contact_data = (b2ContactData*)SDL_calloc(PHYSICS_MAX_CONTACTS_PER_ENTITY, sizeof(b2ContactData));
	
	int contacts = b2Body_GetContactCapacity(entity->physics_data.body_id);
	SDL_assert(contacts <= PHYSICS_MAX_CONTACTS_PER_ENTITY && "Max number of contacts exceeded. If this is not an error, increase PHYSICS_MAX_CONTACTS_PER_ENTITY");

	int actual_contacts = b2Body_GetContactData(state->player->physics_data.body_id, contact_data, contacts);

	// in this mode we are re-checking all collisions every frame, so we need to reset this all the time
	// (in a real game we woul probably like to "buffer" at least the ground: pretend we are still colliding for a few frames so that the games feels less finnicky)
	data->is_grounded        = false;
	data->is_colliding_top   = false;
	data->is_colliding_left  = false;
	data->is_colliding_right = false;
	for(int i = 0; i < actual_contacts; ++i)
	{
		b2ContactData* contact = &contact_data[i];
		b2Filter filter_a = b2Shape_GetFilter(contact->shapeIdA);
		vec2f collision_normal = value_cast(vec2f, contact->manifold.normal);

		// NOTE: normal points AWAY from the surface, so we need to check with the opposite vector
		float normal_diff_ground  = dot(VEC2F_UP, collision_normal);
		float normal_diff_ceiling = dot(VEC2F_DOWN, collision_normal);
		float normal_diff_left    = dot(VEC2F_RIGHT, collision_normal);
		float normal_diff_right   = dot(VEC2F_LEFT, collision_normal);

		if(filter_a.categoryBits & COLLISION_FILTER_GROUND)
		{
			if(normal_diff_ground > design_player_surface_normal_diff_threshold)
			{
				// TMP velocity should come from entity, not b2Body
				b2BodyId other_id = b2Shape_GetBody(contact->shapeIdA);
				b2BodyId my_id    = b2Shape_GetBody(contact->shapeIdB);
				b2Vec2 other_velocity = b2Body_GetLinearVelocity(other_id);
				b2Vec2 my_velocity    = b2Body_GetLinearVelocity(my_id);

				float TMP_grouded_velocity_threshold = 4;
				// NOTE: adding an extra safety check because sometimes we still detect collision with the ground for one frame after jumping
				//       Probably that this is a mistake on our side, but investigating it would take time I don't have.
				//       What we're doing instead is saying "we can't be grounded if our vertical velocity is pushing us upwards"
				//       Normally just `<= 0` would do, but since the game has additional custom logic for inclined slopes we need to increase the threshold to keep into
				//       account climbing those. The threshold must be bigger than the velocity on the highest possible slope inclide, but smaller than the minimim jump initial vertical velocity
				if(my_velocity.y <= TMP_grouded_velocity_threshold)
				{
					data->is_grounded = true;
					data->velocity_ground = value_cast(vec2f, other_velocity);
					data->normal_ground = value_cast(vec2f, collision_normal);
				}
			}
			else if(normal_diff_ceiling > design_player_surface_normal_diff_threshold)
				data->is_colliding_top = true;
			else if(normal_diff_left > design_player_surface_normal_diff_threshold)
				data->is_colliding_left = true;
			else if(normal_diff_right > design_player_surface_normal_diff_threshold)
				data->is_colliding_right = true;
		}
	}

	// camera
	{
		const float zoom_speed = 1;
		vec2f camera_offset = vec2f { -2, 1.2f } / context->camera_active->zoom;
		// camera follows player
		context->camera_active->world_position = state->player->transform.position + camera_offset;
		context->camera_active->zoom += context->mouse_scroll * zoom_speed * context->delta;
	}
}

static void game_render(SDLContext* context, GameState* state)
{
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
		itu_sys_physics_debug_draw();

	debug_ui_player_data(state);
	
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

	SDL_CreateWindowAndRenderer("ES04.1.3 - Platformer game", WINDOW_W, WINDOW_H, 0, &window, &context.renderer);
	SDL_SetRenderDrawBlendMode(context.renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderVSync(context.renderer, SDL_RENDERER_VSYNC_ADAPTIVE);
	
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
						case SDLK_W: sdl_input_key_process(&context,     BTN_TYPE_UP, &event);        break;
						case SDLK_A: sdl_input_key_process(&context,     BTN_TYPE_LEFT, &event);      break;
						case SDLK_S: sdl_input_key_process(&context,     BTN_TYPE_DOWN, &event);      break;
						case SDLK_D: sdl_input_key_process(&context,     BTN_TYPE_RIGHT, &event);     break;
						case SDLK_Q: sdl_input_key_process(&context,     BTN_TYPE_ACTION_0, &event);  break;
						case SDLK_E: sdl_input_key_process(&context,     BTN_TYPE_ACTION_1, &event);  break;
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
			// decouple physics step from framerate, running 0, 1 or multiple physics step per frame
			while(accumulator_physics >= PHYSICS_TIMESTEP_NSECS && physics_steps_count < PHYSICS_MAX_TIMESTEPS_PER_FRAME)
			{
				itu_sys_physics_step(PHYSICS_TIMESTEP_SECS);
				++physics_steps_count;
				accumulator_physics -= PHYSICS_TIMESTEP_NSECS;
			}

			// entities
			for(int i = 0; i < state.entities_alive_count; ++i)
			{
				Entity* entity = &state.entities[i];
				b2Vec2 physics_vel = b2Body_GetLinearVelocity(entity->physics_data.body_id);
				b2Vec2 physics_pos = b2Body_GetPosition(entity->physics_data.body_id);
				b2Rot  physics_rot = b2Body_GetRotation(entity->physics_data.body_id);
				entity->physics_data.velocity = value_cast(vec2f, physics_vel); 
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
			ImGui::PushItemWidth(120);
			ImGui::Text("Timing");
			ImGui::LabelText("work", "%6.3f ms/f", (float)elapsed_work / (float)MILLIS(1));
			ImGui::LabelText("tot", "%6.3f ms/f", (float)elapsed_frame / (float)MILLIS(1));

			ImGui::Text("Debug");
			if(ImGui::Button("[TAB] reset"))
				game_reset(&context, &state);
			ImGui::Checkbox("[F1] render textures", &DEBUG_render_textures);
			ImGui::Checkbox("[F2] render outlines", &DEBUG_render_outlines);
			ImGui::Checkbox("[F3] render physics", &DEBUG_physics);
			ImGui::PopItemWidth();
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
