// NOTE: this is just an auxiliary file for exercise 05 - animation and audio
//       to access this funcitonality for the final project, see `itu_sys_audio.hpp` when available

#ifndef ES05_SYS_AUDIO_HPP
#define ES05_SYS_AUDIO_HPP

#ifndef ITU_UNITY_BUILD
#include <SDL3_mixer/SDL_mixer.h>
#include <stb_ds.h>
#include <itu_common.hpp>
#endif

#define NUM_TRACKS_MAX 32

typedef size_t AudioKey;

struct AudioData
{
	// MIX
	MIX_Mixer* mixer;

	// reference to which track is actually playing the main soundtrack
	int track_music_ref;

	MIX_Track* tracks[NUM_TRACKS_MAX];
	Uint64     track_time_start[NUM_TRACKS_MAX];
	int tracks_count;

	float gain_music;
	float gain_sfx;

	stbds_hm(AudioKey, MIX_Audio*)  audio_files;
};

AudioData sys_audio_data;

void sys_audio_init(int tracks_count);
AudioKey sys_audio_load(const char* path, bool preload);
void sys_audio_play_music(AudioKey key, Sint64 crossfade_duration_ms);
void sys_audio_play_music_immediate(AudioKey key);
void sys_audio_play_sfx(AudioKey key);
void sys_audio_set_gain_master(float gain);
void sys_audio_set_gain_music(float gain);
void sys_audio_set_gain_sfx(float gain);

// convenience method that get the path directly instead of the key,
// game at runtime should use the `AudioKey` version of the API instead
void sys_audio_play_music(const char* path, Sint64 crossfade_duration_ms);

// convenience method that get the path directly instead of the key,
// game at runtime should use the `AudioKey` version of the API instead
void sys_audio_play_music_immediate(const char* path);

// convenience method that get the path directly instead of the key,
// game at runtime should use the `AudioKey` version of the API instead
void sys_audio_play_sfx(const char* path);

#endif // ES05_SYS_AUDIO_HPP

#if (defined ES05_SYS_AUDIO_IMPLEMENTATION) || (defined ITU_UNITY_BUILD)

// internal methods

inline MIX_Audio* get_audio(AudioKey key); 

// plays the given audio on the given track
inline void play_audio(int track_idx, MIX_Audio* audio, float gain, SDL_PropertiesID props); 

// returns the first free track, OR the track that started playing the earliest
inline int find_track(); 

MIX_Audio* get_audio(AudioKey key)
{
	size_t loc = stbds_hmgeti(sys_audio_data.audio_files, key);
	if(loc == -1)
	{
		SDL_Log("WARNING key not present in audio db: %lld", key);
		return NULL;
	}

	return sys_audio_data.audio_files[loc].value;
}

void play_audio(int track_idx, MIX_Audio* audio, float gain, SDL_PropertiesID props)
{
	MIX_SetTrackGain(sys_audio_data.tracks[track_idx], gain);
	MIX_SetTrackAudio(sys_audio_data.tracks[track_idx], audio);
	MIX_PlayTrack(sys_audio_data.tracks[track_idx], props);
	sys_audio_data.track_time_start[track_idx] = SDL_GetTicks();
}


int find_track()
{
	Uint64 oldest_start_time = -1;
	int    oldest_start_idx  = 0;
	for(int i = 0; i < sys_audio_data.tracks_count; ++i)
	{
		if(!MIX_TrackPlaying(sys_audio_data.tracks[i]))
		{
			return i;
		}
		else if(sys_audio_data.track_time_start[i] < oldest_start_time)
		{
			oldest_start_time = sys_audio_data.track_time_start[i];
			oldest_start_idx  = i;
		}
	}
	return oldest_start_idx;
}

void sys_audio_init(int tracks_count)
{
	if(tracks_count > NUM_TRACKS_MAX)
	{
		SDL_Log("WARNING max audio tracks supported is %d", NUM_TRACKS_MAX);
		tracks_count = NUM_TRACKS_MAX;
	}

	// VALIDATE_PANIC(SDL_Init(SDL_INIT_AUDIO));
	VALIDATE(MIX_Init());
	VALIDATE(sys_audio_data.mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL));

	sys_audio_data.track_music_ref = -1;

	sys_audio_data.tracks_count = tracks_count;
	for(int i = 0; i < tracks_count; ++i)
		VALIDATE(sys_audio_data.tracks[i] = MIX_CreateTrack(sys_audio_data.mixer));

	sys_audio_data.gain_music = 1.0f;
	sys_audio_data.gain_sfx = 1.0f;
}

AudioKey sys_audio_load(const char* path, bool preload)
{
	// NOTE: casting from const to non-const is usually a bad idea,
	//       but in this case we know that `stbds_hash_string` does not modify our pointer so it's fine. 
	AudioKey key = stbds_hash_string((char*)path, 0);

	if(stbds_hmgeti(sys_audio_data.audio_files, key) >= 0)
	{
		SDL_Log("WARNING audio file already loaded: %s", path);
		return key;
	}

	MIX_Audio* audio;
	VALIDATE(audio = MIX_LoadAudio(sys_audio_data.mixer, path, preload));

	stbds_hmput(sys_audio_data.audio_files, key, audio);

	return key;
}

void sys_audio_play_music(AudioKey key, Sint64 crossfade_duration_ms)
{
	MIX_Audio* audio = get_audio(key);

	if(sys_audio_data.track_music_ref != -1)
	{
		MIX_Track* track_fadeout = sys_audio_data.tracks[sys_audio_data.track_music_ref];
		Sint64 fadeout_frames = MIX_TrackMSToFrames(track_fadeout, crossfade_duration_ms);
		MIX_StopTrack(track_fadeout, fadeout_frames);
	}
	
	int track_idx = find_track();
	MIX_Track* track_fadein = sys_audio_data.tracks[track_idx];

	Sint64 fadein_frames = MIX_TrackMSToFrames(track_fadein, crossfade_duration_ms);
	SDL_PropertiesID props = SDL_CreateProperties();
	SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, -1);
	SDL_SetNumberProperty(props, MIX_PROP_PLAY_FADE_IN_FRAMES_NUMBER, fadein_frames);
	play_audio(track_idx, audio, sys_audio_data.gain_music, props);
	sys_audio_data.track_music_ref = track_idx;
}

void sys_audio_play_music_immediate(AudioKey key)
{
	MIX_Audio* audio = get_audio(key);
	
	if(sys_audio_data.track_music_ref == -1)
		sys_audio_data.track_music_ref = find_track();
	play_audio(sys_audio_data.track_music_ref, audio, sys_audio_data.gain_music, 0);
}

void sys_audio_play_sfx(AudioKey key)
{
	int track_idx = find_track();
	MIX_Audio* audio = get_audio(key);

	play_audio(track_idx, audio, sys_audio_data.gain_sfx, 0);
}

void sys_audio_set_gain_master(float gain)
{
	MIX_SetMasterGain(sys_audio_data.mixer, gain);
}

void sys_audio_set_gain_music(float gain)
{
	sys_audio_data.gain_music = gain;

	int track_idx = sys_audio_data.track_music_ref;
	if(track_idx != -1)
		MIX_SetTrackGain(sys_audio_data.tracks[track_idx], gain);
}

void sys_audio_set_gain_sfx(float gain)
{
	sys_audio_data.gain_sfx = gain;
	for(int i = 0; i < sys_audio_data.tracks_count; ++i)
	{
		if(i == sys_audio_data.track_music_ref)
			continue;
		MIX_SetTrackGain(sys_audio_data.tracks[i], gain);
	}
}

void sys_audio_play_music(const char* path, Sint64 crossfade_duration_ms)
{
	AudioKey key = stbds_hash_string((char*)path, 0);
	sys_audio_play_music(key, crossfade_duration_ms);
}

void sys_audio_play_music_immediate(const char* path)
{
	AudioKey key = stbds_hash_string((char*)path, 0);
	sys_audio_play_music_immediate(key);
}

void sys_audio_play_sfx(const char* path)
{
	AudioKey key = stbds_hash_string((char*)path, 0);
	sys_audio_play_sfx(key);
}

#endif // (defined ES05_SYS_AUDIO_IMPLEMENTATION) || (defined ITU_UNITY_BUILD)