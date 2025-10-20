// NOTE: this is just an auxiliary file for exercise 05 - animation and audio
//       to access this funcitonality for the final project, see `itu_sys_animation.hpp` when available

#ifndef ES05_SYS_ANIMATION_HPP
#define ES05_SYS_ANIMATION_HPP

#ifndef ITU_UNITY_BUILD
#include <SDL3/SDL_rect.h>
#include <stb_ds.h>
#include <itu_common.hpp>
#include <itu_lib_engine.hpp>
#endif

typedef size_t AnimationKey;

struct AnimationFrame 
{
	SDL_FRect rect;
	float duration;
};

struct AnimationClip
{
	stbds_arr(AnimationFrame) frames;
};

struct AnimationData
{
	stbds_hm(AnimationKey, AnimationClip) clips;

	AnimationKey anim_current_key;
	float        anim_current_start;
	int          anim_current_frame;

	float current_speed_multiplier;
};

void         sys_animation_reset(AnimationData* animation_data);
bool         sys_animation_update(SDLContext* context, AnimationData* animation_data);
bool         sys_animation_delete(SDLContext* context, AnimationData* animation_data);

AnimationKey sys_animation_add_clip_empty(AnimationData* animation_data, const char* name);
AnimationKey sys_animation_add_clip(AnimationData* animation_data, const char* name, AnimationClip clip);
void         sys_animation_frame_add(AnimationData* animation_data, AnimationKey clip_key, AnimationFrame frame);
void         sys_animation_current_clip_set(SDLContext* context, AnimationData* animation_data, AnimationKey current_animation_key);
SDL_FRect    sys_animation_get_current_rect(AnimationData* animation_data);

void sys_animation_clip_frame_add(AnimationClip* clip, AnimationFrame frame);
void sys_animation_clip_frames_set(AnimationClip* clip, AnimationFrame* frames, int frames_count);
void sys_animation_clip_reset(AnimationClip* clip);

#endif // ES05_SYS_ANIMATION_HPP

#if (defined ES05_SYS_ANIMATION_IMPLEMENTATION) || (defined ITU_UNITY_BUILD)


AnimationKey sys_animation_add_clip_empty(AnimationData* animation_data, const char* name)
{
	// NOTE: casting from const to non-const is usually a bad idea,
	//       but in this case we know that `stbds_hash_string` does not modify our pointer so it's fine. 
	AnimationKey key = stbds_hash_string((char*)name, 0);
	AnimationClip empty = AnimationClip{ 0 };
	stbds_hmput(animation_data->clips, key, empty);

	return key;
}

AnimationKey sys_animation_add_clip(AnimationData* animation_data, const char* name, AnimationClip clip)
{
	// NOTE: casting from const to non-const is usually a bad idea,
	//       but in this case we know that `stbds_hash_string` does not modify our pointer so it's fine. 
	AnimationKey key = stbds_hash_string((char*)name, 0);
	stbds_hmput(animation_data->clips, key, clip);

	return key;
}

void sys_animation_clip_frame_add(AnimationClip* clip, AnimationFrame frame)
{
	stbds_arrput(clip->frames, frame);
}

void sys_animation_clip_frames_set(AnimationClip* clip, AnimationFrame* frames, int frames_count)
{
	stbds_arrfree(clip->frames);
	// add `frames_count` new initialized elements at the
	// NOTE: "array add N pointer" function below returns the pointer to the first of the newly added elements,
	//       we don't care because we just free-d the array above, so the first new one will be the first one, period.
	stbds_arraddnptr(clip->frames, frames_count);

	SDL_memcpy(clip->frames, frames, sizeof(AnimationFrame) * frames_count);
}


// adds a new frame to the given animation clip
void sys_animation_frame_add(AnimationData* animation_data, AnimationKey clip_key, AnimationFrame frame)
{
	SDL_assert(animation_data);
	SDL_assert(animation_data->clips);

	size_t loc = stbds_hmgeti(animation_data->clips, clip_key);
	if(loc == -1)
	{
		SDL_Log("WARNING animation clip key not present: %lld", clip_key);
		return;
	}
	AnimationClip* clip = &animation_data->clips[loc].value;
	sys_animation_clip_frame_add(clip, frame);
}

void sys_animation_current_clip_set(SDLContext* context, AnimationData* animation_data, AnimationKey current_animation_key)
{
	if(animation_data->anim_current_key == current_animation_key)
		return;

	animation_data->anim_current_key = current_animation_key;
	animation_data->anim_current_frame = 0;
	animation_data->anim_current_start = context->uptime;
}

SDL_FRect sys_animation_get_current_rect(AnimationData* animation_data)
{
	SDL_assert(animation_data);
	SDL_assert(animation_data->clips);

	size_t loc = stbds_hmgeti(animation_data->clips, animation_data->anim_current_key);
	if(loc == -1)
		// clip not present
		return SDL_FRect { 0 };

	AnimationClip clip = animation_data->clips[loc].value;
	SDL_assert(clip.frames);
	SDL_assert(stbds_arrlen(clip.frames) > 0);

	return clip.frames[animation_data->anim_current_frame].rect;
}

bool sys_animation_update(SDLContext* context, AnimationData* animation_data)
{
	AnimationClip animation_curr = stbds_hmget(animation_data->clips, animation_data->anim_current_key);

	if(stbds_arrlen(animation_curr.frames) <= 1)
		return false;

	float play_length = context->uptime - animation_data->anim_current_start;
	int frame_idx_curr = animation_data->anim_current_frame;
	AnimationFrame frame_curr = animation_curr.frames[frame_idx_curr];
	float frame_curr_duration_adjusted = frame_curr.duration * animation_data->current_speed_multiplier;

	if(play_length > frame_curr_duration_adjusted)
	{
		int frame_idx_next = (frame_idx_curr + 1) % stbds_arrlen(animation_curr.frames);

		animation_data->anim_current_frame = frame_idx_next;
		animation_data->anim_current_start = context->uptime - (play_length - frame_curr_duration_adjusted); // adjust for overshooting
		//animation_data->anim_current_start = context->uptime; // no adjustment version

		return true;
	}

	return false;
}

void sys_animation_clip_reset(AnimationClip* clip)
{
	if(!clip)
		return;

	stbds_arrfree(clip->frames);
}

void sys_animation_reset(AnimationData* animation_data)
{
	if(!animation_data)
		return;

	animation_data->anim_current_key = -1;
	animation_data->anim_current_frame = 0;
	animation_data->anim_current_start = 0;
}

void sys_animation_delete(AnimationData* animation_data)
{
	if(!animation_data)
		return;

	int num_clips = stbds_hmlen(animation_data->clips);
	for(int i = 0; i < num_clips; ++i)
	{
		sys_animation_clip_reset(&animation_data->clips[i].value);
	}
	
	stbds_arrfree(animation_data->clips);
}

#endif // (defined ES05_SYS_ANIMATION_IMPLEMENTATION) || (defined ITU_UNITY_BUILD)