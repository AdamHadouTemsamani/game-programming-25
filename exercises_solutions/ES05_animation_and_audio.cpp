#define TEXTURE_PIXELS_PER_UNIT 64   // how many pixels of textures will be mapped to a single world unit
#define CAMERA_PIXELS_PER_UNIT  32   // how many pixels of windows will be used to render a single world unit

#include <itu_unity_include.hpp>

#include "ES05_sys_audio.hpp"
#include "ES05_sys_animation.hpp"

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
#define COLLISION_FILTER_SENSOR         0b00100

bool DEBUG_render_textures = true;
bool DEBUG_render_outlines = false;
bool DEBUG_physics = true;

// design variables
float BASE_design_player_hor_speed_ground_max = 7.5f;
float design_player_hor_speed_ground_max = BASE_design_player_hor_speed_ground_max;
float design_player_hor_speed_air_max    = 5.0f;
float design_player_hor_accel_groud = 125.0f;
float design_player_hor_accel_air   = 125.0f;
float design_player_hor_decel_factor_ground = 0.90f;
float design_player_hor_decel_factor_air    = 0.99f;
float design_player_jump_height_max = 3.0f;
float design_player_jump_hor_distance_to_apex_max = 3.0f;
// how much the normal can change from the perfect 4 cardinal coordinates to consider a piece of terrain "ground", "wall" or "ceiling"
// NOTE: not tested with values different from 50%, movement code may need some tweaks to handle collisions that do not fall in the 4 cardinal directions
float design_player_surface_normal_diff_threshold = 0.5f;

float design_music_crossfade_time_ms = 500;

float          design_door_anim_duration     = 1.0f;
EasingFunction design_door_anim_easing_open  = EASING_OUT_BOUNCE; // something funny as default
EasingFunction design_door_anim_easing_close = EASING_IN_BOUNCE;  // something funny as default

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

const char* animations[] = 
{
	"idle",
	"walk",
	"jump",
	"fall",
	"cheer"
};

const char* music_files[] =
{
	"data/opengameart.org/music_level_0.ogg",
	"data/opengameart.org/music_level_1.ogg"
};

struct DoorData
{
	vec2f offset_movement;

	// runtime
	float duration;
	vec2f start_pos;
	vec2f offset_pos;
	float animation_time_delta;
	float animation_current_t;
	float animation_target_t;
};

struct GameState
{
	// shortcut references
	Entity* player;
	Entity* door;
	
	// game-allocated memory
	Entity* entities;
	int entities_alive_count;
	PlayerData player_data;
	AnimationData player_animation_data;
	DoorData door_data;

	// SDL-allocated structures
	SDL_Texture* atlas_character;
	SDL_Texture* atlas_tiles;

	// audio data
	float volume_music;
	float volume_sfx;
	float volume_master;
	int curr_music_idx;
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
	ImGui::PushItemWidth(200);

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
	ImGui::LabelText("velocity", "%4.2f   %4.2f", entity->physics_data.velocity.x, entity->physics_data.velocity.y);
	ImGui::LabelText("gravity" , "%4.2f", data->g);
	ImGui::LabelText("grounded", "%s", data->is_grounded        ? "X" : "O");
	ImGui::LabelText("Lcollide", "%s", data->is_colliding_left  ? "X" : "O");
	ImGui::LabelText("Rcollide", "%s", data->is_colliding_right ? "X" : "O");
	ImGui::LabelText("Tcollide", "%s", data->is_colliding_top   ? "X" : "O");
	ImGui::Separator();
	ImGui::LabelText("velocity desired",  "%4.2f   %4.2f", data->velocity_desired.x, data->velocity_desired.y);
	ImGui::LabelText("velocity ground" ,  "%4.2f   %4.2f", data->velocity_ground.x, data->velocity_ground.y);

	ImGui::CollapsingHeader("Non-player stuff", ImGuiTreeNodeFlags_Leaf);
	ImGui::DragFloat("design_door_anim_duration", &design_door_anim_duration);
	
	ImGui::Combo("design_door_anim_easing_open",  (int*)&design_door_anim_easing_open, easing_names,  EASING_MAX);
	ImGui::Combo("design_door_anim_easing_close", (int*)&design_door_anim_easing_close, easing_names, EASING_MAX);
	ImGui::PopItemWidth();
	ImGui::End();
}

void debug_ui_game_audio(GameState* state)
{
	ImGui::Begin("game_audio");
	ImGui::PushItemWidth(300);


	ImGui::DragFloat("music_crossfade_time_ms", &design_music_crossfade_time_ms, 10);
	if(ImGui::DragFloat("volume_master", &state->volume_master, 0.05f, 0.0f, 1.0f))
		sys_audio_set_gain_master(state->volume_master);
	if(ImGui::DragFloat("volume_music", &state->volume_music, 0.05f, 0.0f, 1.0f))
		sys_audio_set_gain_music(state->volume_music);
	if(ImGui::DragFloat("volume_sfx", &state->volume_sfx, 0.05f, 0.0f, 1.0f))
		sys_audio_set_gain_sfx(state->volume_sfx);

	if(ImGui::Combo("current_music", &state->curr_music_idx, music_files, array_size(music_files)))
		sys_audio_play_music(music_files[state->curr_music_idx], design_music_crossfade_time_ms);

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

static void add_terrain_piece(b2BodyId id, vec2f halfsize, vec2f position, float rotation)
{
	b2ShapeDef shape_def = b2DefaultShapeDef();
	shape_def.filter.categoryBits = COLLISION_FILTER_GROUND;

	b2Polygon polygon = b2MakeOffsetBox(halfsize.x, halfsize.y, value_cast(b2Vec2, position), b2MakeRot(rotation));

	b2CreatePolygonShape(id, &shape_def, &polygon);
}

static AudioKey KEY_SFX_FOOTSTEP;
static AudioKey KEY_SFX_DOOR;

static AnimationKey KEY_ANIM_WALK;
static AnimationKey KEY_ANIM_IDLE;
static AnimationKey KEY_ANIM_JUMP;
static AnimationKey KEY_ANIM_FALL;

// you will occasionally see in unity build style projects cpp files included in the middle of other cpp files
// you NEVER want to do this with classic build projects, and also for unity builds this should be avoided
// (also IDEs tend to freak out if you do it, so most of the time other extensions like .inl are used)
// the unity build pattern should be a single header file which includes all the headers and then all the surce files, in the appropriate order
// here I'm doing it exeptionally since we have too much exercise-specific code which is not relevant for the actual solution
#include "ES05_entity_implementations.inl"

void door_update(SDLContext* context, Entity* entity, DoorData* data)
{
	float delta_t = (1.0f / data->duration) * context->delta;
	EasingFunction fn_easing;
	if(data->animation_target_t == 0 && data->animation_current_t > data->animation_target_t)
	{
		data->animation_current_t -= delta_t;
		fn_easing = design_door_anim_easing_close;
	}
	else if(data->animation_target_t == 1 && data->animation_current_t < data->animation_target_t)
	{
		data->animation_current_t += delta_t;
		fn_easing = design_door_anim_easing_open;

	}
	else
		return;
	
	// NOTE: lerping works just fine if `t` is outside the [0..1] range, it's usually called extrapolation and it can be veru useful
	//       in this case however, we don't wont the door to overshoot its targets, so we will clamp it
	data->animation_current_t = SDL_clamp(data->animation_current_t, 0.0f, 1.0f);

	// apply easing (I went ahead and dumped them all int `itu_common.cpp` in a fancy package)

	//float v = data->animation_current_t;
	float v = easing(data->animation_current_t, fn_easing);
	entity->transform.position = lerp(data->start_pos, data->offset_pos, v);
	b2Body_SetTransform(entity->physics_data.body_id, value_cast(b2Vec2, entity->transform.position), b2MakeRot(entity->transform.rotation));
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

	sys_audio_init(4);
	
	// audio
	{
		// here we see two different ways to handle resource references:
		// 1. music ignores the `AudioKey` returned and always uses the string
		// 2. sound effects store the key and use it to interface with the audio system going forward
		// using strings directly can make sense in certain situations (see L06 when we talk about tools and editors), but they ar epretty slow,
		// even when hashed. At runtime, games should always use keys (which can be read from file, so usually that's not a problem)
		sys_audio_load(music_files[0], true);
		sys_audio_load(music_files[1], true);
		KEY_SFX_FOOTSTEP = sys_audio_load("data/kenney/SFX/footstep00.ogg", true);
		KEY_SFX_DOOR     = sys_audio_load("data/kenney/SFX/doorClose_1.ogg", true);

		// arbitrary decision to make audio settings not reset with games
		// in an actual game those would be stored togheter with savefiles and read form a file,
		// so it makes sense
		state->volume_master = 1.0f;
		state->volume_music = 1.0f;
		state->volume_sfx = 1.0f;
	}
}

static void game_reset(SDLContext* context, GameState* state)
{
	// TMP reset uptime (should probably be two different variables
	context->uptime = 0;
	state->entities_alive_count = 0;

	b2WorldDef def_world = b2DefaultWorldDef();
	def_world.gravity.y = GRAVITY;
	itu_sys_physics_reset(&def_world);

	sys_animation_reset(&state->player_animation_data);

	sys_audio_play_music_immediate(music_files[state->curr_music_idx]);

	// player
	{
		state->player = entity_create(state);

		b2Filter collision_filter = { 0 };
		collision_filter.categoryBits = COLLISION_FILTER_PLAYER;
		collision_filter.maskBits     = COLLISION_FILTER_GROUND | COLLISION_FILTER_SENSOR;
		player_reset(context, state->player, &state->player_data, &state->player_animation_data, state->atlas_character, collision_filter);
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
		state->door = entity_create(state);
		door_reset(state->door, &state->door_data, state->atlas_tiles);
	}
}

static void game_update(SDLContext* context, GameState* state)
{
	player_update(context, state->player, &state->player_data, &state->player_animation_data);
	door_update(context, state->door, &state->door_data);
}

static void game_update_post_physics(SDLContext* context, GameState* state)
{
	static b2ContactData* contact_data = (b2ContactData*)SDL_calloc(PHYSICS_MAX_CONTACTS_PER_ENTITY, sizeof(b2ContactData));
	int contacts = b2Body_GetContactCapacity(state->player->physics_data.body_id);
	SDL_assert(contacts <= PHYSICS_MAX_CONTACTS_PER_ENTITY && "Max number of contacts exceeded. If this is not an error, increase `PHYSICS_MAX_CONTACTS_PER_ENTITY`");
	int actual_contacts = b2Body_GetContactData(state->player->physics_data.body_id, contact_data, contacts);

	
	player_handle_collisions(context, state->player, &state->player_data, contact_data, actual_contacts);

	b2SensorEvents sensor_events = ity_sys_physics_get_sensor_events();
	for(int i = 0; i < sensor_events.beginCount; ++i)
	{
		b2ShapeId shape_visitor = sensor_events.beginEvents[i].visitorShapeId;
		b2Filter filter_visitor = b2Shape_GetFilter(shape_visitor);

		if(filter_visitor.categoryBits & COLLISION_FILTER_PLAYER)
		{
			state->door_data.animation_target_t = 1;

			// better way: if another transition is in place, we need to shorten the duration
			// this avoid most visual artifacts. Another way would be to queue the transition, and start it only when the current one is finished
			state->door_data.duration = design_door_anim_duration * SDL_fabsf(state->door_data.animation_target_t - state->door_data.animation_current_t);
			// // easy way: work just fine, as long as we never trigger a transition while another one is happening
			// state->door_data.duration = design_door_anim_duration;
		}
	}

	for(int i = 0; i < sensor_events.endCount; ++i)
	{
		b2ShapeId shape_visitor = sensor_events.endEvents[i].visitorShapeId;
		b2Filter filter_visitor = b2Shape_GetFilter(shape_visitor);

		if(filter_visitor.categoryBits & COLLISION_FILTER_PLAYER)
		{
			state->door_data.animation_target_t = 0;

			// better way: if another transition is in place, we need to shorten the duration
			// this avoid most visual artifacts. Another way would be to queue the transition, and start it only when the current one is finished
			state->door_data.duration = design_door_anim_duration * SDL_fabsf(state->door_data.animation_target_t - state->door_data.animation_current_t);
			// // easy way: work just fine, as long as we never trigger a transition while another one is happening
			// state->door_data.duration = design_door_anim_duration;
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
	itu_lib_render_draw_world_grid(context);

	// entities
	for(int i = 0; i < state->entities_alive_count; ++i)
	{
		Entity* entity = &state->entities[i];

		if(DEBUG_render_textures)
			itu_lib_sprite_render(context, &entity->sprite, &entity->transform);

		if(DEBUG_render_outlines)
			itu_lib_sprite_render_debug(context, &entity->sprite, &entity->transform);
	}

	if(DEBUG_physics)
		itu_sys_physics_debug_draw();

	debug_ui_player_data(state);
	debug_ui_game_audio(state);
	
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
