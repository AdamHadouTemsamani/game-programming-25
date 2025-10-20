void itu_system_sprite_render(SDLContext* context, ITU_EntityId* entity_ids, int entity_ids_count)
{
	for(int i = 0; i < entity_ids_count; ++i)
	{
		ITU_EntityId id = entity_ids[i];
		Transform* transform = entity_get_data(id, Transform);
		Sprite*    sprite = entity_get_data(id, Sprite);

		itu_lib_sprite_render(context, sprite, transform);
	}
}

void itu_system_physics(SDLContext* context, ITU_EntityId* entity_ids, int entity_ids_count)
{
	for(int i = 0; i < entity_ids_count; ++i)
	{
		ITU_EntityId id = entity_ids[i];
		PhysicsData* physics_data = entity_get_data(id, PhysicsData);

		b2Body_SetLinearVelocity(physics_data->body_id, value_cast(b2Vec2, physics_data->velocity));
		b2Body_SetAngularVelocity(physics_data->body_id, physics_data->torque);
	}

	context->physics_steps_count = 0;
	context->accumulator_physics += context->elapsed_frame;

	// decouple physics step from framerate, running 0, 1 or multiple physics step per frame
	while(context->accumulator_physics >= PHYSICS_TIMESTEP_NSECS && context->physics_steps_count < PHYSICS_MAX_TIMESTEPS_PER_FRAME)
	{
		itu_sys_physics_step(PHYSICS_TIMESTEP_SECS);
		context->physics_steps_count++;
		context->accumulator_physics -= PHYSICS_TIMESTEP_NSECS;

		// update game state from b2d state, interpolating when physics step is out of synch with game logic
		// NOTE: we need to read the b2d state every step in order to get a correct interpolation, even if it's wasteful
		//       (there is definitely a way to adjust `t` and `t_inv` based on the numbers of steps done and do the reading only once,
		//       marking it as a TODO for the future)
		float t = (float)(context->accumulator_physics) / (float)PHYSICS_TIMESTEP_NSECS;
		float t_inv = 1 - t;

		for(int i = 0; i < entity_ids_count; ++i)
		{
			ITU_EntityId id = entity_ids[i];
			Transform*  transform = entity_get_data(id, Transform);
			PhysicsData* physics_data = entity_get_data(id, PhysicsData);

			b2Vec2 physics_vel = b2Body_GetLinearVelocity(physics_data->body_id);
			float  physics_trq = b2Body_GetAngularVelocity(physics_data->body_id);
			b2Vec2 physics_pos = b2Body_GetPosition(physics_data->body_id);
			b2Rot  physics_rot = b2Body_GetRotation(physics_data->body_id);

			physics_data->velocity = value_cast(vec2f, physics_vel) * t + physics_data->fixed_step_velocity * t_inv;
			physics_data->torque   = physics_trq * t + physics_data->fixed_step_torque * t_inv;


			if(!physics_data->ignore_position)
				transform->position = value_cast(vec2f, physics_pos) * t + physics_data->fixed_step_position * t_inv;

			if(!physics_data->ignore_rotation)
				transform->rotation = b2Rot_GetAngle(physics_rot) * t + physics_data->fixed_step_rotation * t_inv;

			physics_data->fixed_step_velocity = value_cast(vec2f, physics_vel);
			physics_data->fixed_step_torque = physics_trq;
			physics_data->fixed_step_position = value_cast(vec2f, physics_pos);
			physics_data->fixed_step_rotation = b2Rot_GetAngle(physics_rot);
		}
	}
}
