void player_compute_jump_parameters(Entity* entity, float* out_vertical_speed, float* out_gravity)
{
	float v_x = design_player_hor_speed_ground_max;
	float h = design_player_jump_height_max;
	float x_h = design_player_jump_hor_distance_to_apex_max;
	*out_vertical_speed = (2 * h * v_x) / x_h;
	*out_gravity        = (-2 * h * v_x*v_x) / (x_h*x_h);
}

void player_reset(SDLContext* context, Entity* entity, PlayerData* player_data, AnimationData* animation_data, SDL_Texture* texture, b2Filter collision_filter)
{
	SDL_memset(player_data, 0, sizeof(PlayerData));
	player_data->g = -66.67f;

	entity->transform.position = VEC2F_ONE;
	entity->transform.scale = VEC2F_ONE;
	itu_lib_sprite_init(
		&entity->sprite,
		texture,
		SDL_FRect { 0, 0, 96, 128 } 
	);
	entity->sprite.pivot.y = 0;

	// box2d body, shape and polygon
	{
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
		shape_def.filter = collision_filter;

		b2Capsule capsule = { };
		capsule.radius = 0.5f;
		capsule.center1.y =  1.0f ;
		capsule.center2.y =  0.5f;

		entity->physics_data.body_id = itu_sys_physics_add_body(entity, &body_def);
		b2CreateCapsuleShape(entity->physics_data.body_id, &shape_def, &capsule);
	}

	// animations
	{
		KEY_ANIM_IDLE = sys_animation_add_clip_empty(animation_data, "idle");
		sys_animation_frame_add(animation_data, KEY_ANIM_IDLE, AnimationFrame{ SDL_FRect {   0,   0, 96, 128 }, 2.0f });
		sys_animation_frame_add(animation_data, KEY_ANIM_IDLE, AnimationFrame{ SDL_FRect { 288, 384, 96, 128 }, 0.5f });
			
		// walk
		KEY_ANIM_WALK = sys_animation_add_clip_empty(animation_data, "walk");
		for(int i = 0; i < 8; ++i)
			sys_animation_frame_add(animation_data, KEY_ANIM_WALK, AnimationFrame{ SDL_FRect { i * 96.0f, 512, 96, 128 }, 16.0f / design_player_hor_accel_groud });

		// jump
		KEY_ANIM_JUMP = sys_animation_add_clip_empty(animation_data, "jump");
		sys_animation_frame_add(animation_data, KEY_ANIM_JUMP, AnimationFrame{ SDL_FRect { 86, 0, 96, 128 }, 0.0f });
			
		// fall
		KEY_ANIM_FALL = sys_animation_add_clip_empty(animation_data, "fall");
		sys_animation_frame_add(animation_data, KEY_ANIM_FALL, AnimationFrame{ SDL_FRect { 192, 0, 96, 128 }, 0.0f });
	}
}

void player_update(SDLContext* context, Entity* entity, PlayerData* player_data, AnimationData* animation_data)
{
	vec2f velocity = player_data->velocity_desired;

	float hor_accel;
	float hor_decel_factor;
	float hor_speed_max;
	if(player_data->is_grounded)
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

	if(player_data->is_grounded)
	{
			
		if(context->btn_isdown[BTN_TYPE_LEFT])
			velocity += cross_triplet(player_data->normal_ground, VEC2F_LEFT, player_data->normal_ground) * hor_accel * context->delta;
		else if(context->btn_isdown[BTN_TYPE_RIGHT])
			velocity += cross_triplet(player_data->normal_ground, VEC2F_RIGHT, player_data->normal_ground) * hor_accel * context->delta;
		else
			velocity = velocity * hor_decel_factor;

		velocity = clamp(velocity, hor_speed_max);

		if(player_data->is_colliding_left && velocity.x < 0)
			velocity = VEC2F_ZERO;
		if(player_data->is_colliding_right && velocity.x > 0)
			velocity = VEC2F_ZERO;

		if(context->btn_isjustpressed[BTN_TYPE_SPACE])
			player_compute_jump_parameters(entity, &velocity.y, &player_data->g);
	}
	else
	{
		if(player_data->is_colliding_top)
			velocity.y = SDL_min(velocity.y, 0);

		if(context->btn_isdown[BTN_TYPE_LEFT])
			velocity.x -= hor_accel * context->delta;
		else if(context->btn_isdown[BTN_TYPE_RIGHT])
			velocity.x += hor_accel * context->delta;
		else
			velocity.x *= hor_decel_factor;

		if(velocity.y > 0)
			velocity.y += player_data->g * context->delta;
		else
			velocity.y += player_data->g * context->delta * 3;
	}
		
	float TMP_player_desired_speed_hor_threshold = 0.1f;
	if(SDL_fabsf(velocity.x) < TMP_player_desired_speed_hor_threshold)
		velocity.x = 0;

	player_data->velocity_desired = velocity;
	vec2f velocity_total = velocity + player_data->velocity_ground;

		
	float TMP_player_speed_threshold = 0.1f;
	if(length_sq(velocity_total) < TMP_player_speed_threshold)
		velocity_total = VEC2F_ZERO;
	b2Body_SetLinearVelocity(entity->physics_data.body_id, value_cast(b2Vec2, velocity_total));


	// animation
	{
		if(velocity.x < 0)
			entity->sprite.flip_horizontal = true;
		else if(velocity.x > 0)
			entity->sprite.flip_horizontal = false;

		// adjust velocity
		if(animation_data->anim_current_key == 1)
			animation_data->current_speed_multiplier = BASE_design_player_hor_speed_ground_max / design_player_hor_speed_ground_max;
		else
			animation_data->current_speed_multiplier = 1;

		// here is were we decide in which state we are, depending on gameplay properties
		// in a fully fledges animation system, this would be a state machine
		if(entity->physics_data.velocity.y > FLOAT_EPSILON)
			sys_animation_current_clip_set(context, animation_data, KEY_ANIM_JUMP);
		else if(entity->physics_data.velocity.y < -FLOAT_EPSILON)
			sys_animation_current_clip_set(context, animation_data, KEY_ANIM_FALL);
		else if(SDL_abs(velocity.x) > TMP_player_desired_speed_hor_threshold && (context->btn_isdown_left || context->btn_isdown_right))
			sys_animation_current_clip_set(context, animation_data, KEY_ANIM_WALK);
		else if(SDL_abs(velocity.x) < TMP_player_desired_speed_hor_threshold && !(context->btn_isdown_left || context->btn_isdown_right))
			sys_animation_current_clip_set(context, animation_data, KEY_ANIM_IDLE);

		if(sys_animation_update(context, animation_data))
		{
			// fire footstep SFX only if we are walking and we are on a frame where the foot touches the ground
			// in a fully fledged animation system, these kind of operations would be performed at callbacks,
			// set up to trigger either at a certain points in the timeline or when transitioning between states
			if(animation_data->anim_current_key == KEY_ANIM_WALK && (animation_data->anim_current_frame == 1 || animation_data->anim_current_frame == 5))
				sys_audio_play_sfx(KEY_SFX_FOOTSTEP);
		}

		// similarly as above, we could set the frame only when stuff actuall change and when we change frame, instead of every frame
		entity->sprite.rect = sys_animation_get_current_rect(animation_data);
	}
}

void player_handle_collisions(SDLContext* context, Entity* entity, PlayerData* player_data, b2ContactData* contact_data, int contact_data_count)
{
	// in this mode we are re-checking all collisions every frame, so we need to reset this all the time
	// (in a real game we woul probably like to "buffer" at least the ground: pretend we are still colliding for a few frames so that the games feels less finnicky)
	player_data->is_grounded        = false;
	player_data->is_colliding_top   = false;
	player_data->is_colliding_left  = false;
	player_data->is_colliding_right = false;
	for(int i = 0; i < contact_data_count; ++i)
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
					player_data->is_grounded = true;
					player_data->velocity_ground = value_cast(vec2f, other_velocity);
					player_data->normal_ground = value_cast(vec2f, collision_normal);
				}
			}
			else if(normal_diff_ceiling > design_player_surface_normal_diff_threshold)
				player_data->is_colliding_top = true;
			else if(normal_diff_left > design_player_surface_normal_diff_threshold)
				player_data->is_colliding_left = true;
			else if(normal_diff_right > design_player_surface_normal_diff_threshold)
				player_data->is_colliding_right = true;
		}
	}
}

void door_reset(Entity* entity, DoorData* data, SDL_Texture* texture)
{
	entity->transform.position.x = 7;
	entity->transform.position.y = 1.5;
	entity->transform.scale = VEC2F_ONE;

	data->offset_movement.y = 3;
	data->start_pos = entity->transform.position;
	data->offset_pos = entity->transform.position + data->offset_movement;
	data->animation_current_t = data->animation_target_t = 0;

	b2BodyDef body_def = b2DefaultBodyDef();
	body_def.type = b2_kinematicBody;
	body_def.name = "door";
	body_def.position = value_cast(b2Vec2, entity->transform.position);

	// collision
	b2ShapeDef shape_def_collision = b2DefaultShapeDef();
	shape_def_collision.filter.categoryBits = COLLISION_FILTER_GROUND;
	shape_def_collision.filter.maskBits     = COLLISION_FILTER_PLAYER;
	b2Polygon polygon_collision = b2MakeBox(0.5f, 1.5f);

	// sensor
	b2ShapeDef shape_def_sensor = b2DefaultShapeDef();
	shape_def_sensor.isSensor = true;
	shape_def_sensor.enableSensorEvents = true;
	shape_def_sensor.filter.categoryBits = COLLISION_FILTER_SENSOR;
	b2Polygon polygon_sensor = b2MakeOffsetBox(2.5f, 3.0f, b2Vec2 { 0, -1.5f}, b2Rot_identity);

	entity->physics_data.body_id = itu_sys_physics_add_body(entity, &body_def);
	b2CreatePolygonShape(entity->physics_data.body_id, &shape_def_collision, &polygon_collision);
	b2CreatePolygonShape(entity->physics_data.body_id, &shape_def_sensor, &polygon_sensor);

	itu_lib_sprite_init(
		&entity->sprite,
		texture,
		SDL_FRect { 0, 64*9, 64, 64*3 } 
	);
}
