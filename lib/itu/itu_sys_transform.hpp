#include <SDL3/SDL.h>

#include <itu_common.hpp>
#include <itu_lib_engine.hpp>

#include <stb_ds.h>

struct ITU_EntityId
{
	Uint64 id;
	Uint64 component_mask;
};



//vec2f itu_trasform_get_position(ITU_IdComponentTransform id);
//vec2f itu_trasform_get_scale(ITU_IdComponentTransform id);
//float itu_trasform_get_rotation(ITU_IdComponentTransform id);
//
//void itu_trasform_set_position(ITU_IdComponentTransform id, vec2f position);
//void itu_trasform_set_scale(ITU_IdComponentTransform id, vec2f scale);
//void itu_trasform_set_rotation(ITU_IdComponentTransform id, float rotation);

struct ITU_ComponentTransformData
{
	vec2f pos;
	vec2f scale;
	float rotation;
};

#define TRANSFORM_COUNT_MAX 1024

ITU_ComponentTransformData data[TRANSFORM_COUNT_MAX];
ITU_EntityId entity_ids[TRANSFORM_COUNT_MAX];
Uint64 data_loc[TRANSFORM_COUNT_MAX];
int data_alive_count;

void itu_entity_add_component(ITU_EntityId id, ITU_ComponentType component_type)
{
	SDL_assert(component_type < 64);
	Uint64 component_bit = 1 << component_type;
	if(id.component_mask & component_bit)
	{
		SDL_Log("WARNING entity %lld alread has component type %d\n", id.id, component_type);
		return;
	}


}


//struct { ITU_EntityId key; Uint64 value; }* data_;

hmput