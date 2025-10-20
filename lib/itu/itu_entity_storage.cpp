#ifndef ITU_UNITY_BUILD
#include <itu_entity_storage.hpp>
#include <imgui/imgui.h>
#endif

struct ITU_Component
{
	ITU_ComponentType type;
	const char* name;
	
	Uint64 element_size;
	int count_max;
	int count_alive;

	Uint64*       data_loc;   // maps EntityId.index to location in data array
	ITU_EntityId* entity_ids; // maps data array location to an EntityId
	void*         data;

	ITU_ComponendDebugUIRender fn_debug_ui_render;
};

struct ITU_ComponentTagStorage
{
	ITU_EntityId key;
};

struct ITU_ComponentTag
{
	ITU_ComponentType type;
	const char* name;
	
	int count_max;
	int count_alive;

	ITU_ComponentTagStorage* entitiy_ids;
};

struct ITU_System
{
	const char* name;
	ITU_Component* components[SYSTEM_COMPONENTS_MAX];
	int components_count;

	ITU_TagType tags[SYSTEM_TAGS_MAX];
	int tags_count;

	ITU_SystemUpdateFunction fn_update;
};

struct ITU_Entity
{
	ITU_EntityId id;
	Uint64 component_mask;
};

struct ITU_EntityStorageContext
{
	stbds_arr(ITU_Entity)   entities;
	stbds_arr(ITU_EntityId) entities_free;

	ITU_Component* components[COMPONENTS_COUNT_MAX];
	int components_count;

	ITU_ComponentTagStorage* tags[TAGS_COUNT_MAX];

	ITU_System systems[SYSTEMS_COUNT_MAX];
	int systems_count;

	// debug properties
	stbds_hm(ITU_EntityId, char*) entities_debug_names;
	stbds_hm(Sint32, const char*) tag_debug_names;
};

static ITU_ComponentType component_type_counter;
static ITU_EntityStorageContext ctx;

ITU_Component* itu_component_pool_create(size_t element_size, Uint64 total_num_component, const char* component_name);
void  itu_component_pool_assign(ITU_Component* component_pool, ITU_EntityId entity);
void  itu_component_pool_data_get(ITU_Component* component_pool, ITU_EntityId entity, void* out_data_copy);
void  itu_component_pool_data_set(ITU_Component* component_pool, ITU_EntityId entity, void* in_data_copy);
void  itu_component_pool_remove(ITU_Component* component_pool, ITU_EntityId entity);
void  itu_component_pool_clear(ITU_Component* component_pool);
int   itu_system_get_matching_entities(ITU_System* system, ITU_EntityId* out_entitiy_group);

ITU_Component* itu_component_pool_create(Uint64 element_size, Uint64 total_num_component, const char* component_name)
{
	// common pattern: we are allocating enough space for the metadata (ITU_Component) + the array data
	size_t size_metadata   = sizeof(ITU_Component);
	size_t size_data_loc   = sizeof(Uint64) * ENTITIES_COUNT_MAX;
	size_t size_entity_ids = sizeof(ITU_EntityId) * total_num_component;
	size_t size_data       = element_size * total_num_component;
	size_t total_size = size_metadata + size_data_loc + size_entity_ids + size_data;

	ITU_Component* ret = (ITU_Component*)SDL_malloc(total_size);
	SDL_memset(ret, -1, total_size);

	ret->name = component_name;
	ret->element_size = element_size;
	ret->count_max = total_num_component;
	ret->count_alive = 0;

	
	//Uint64* valid = (Uint64*)((unsigned char*)ret + size_metadata);

	ret->data_loc   = pointer_offset(Uint64, ret, size_metadata);                 // first array starts at the end of the metadata
	ret->entity_ids = pointer_offset(ITU_EntityId, ret->data_loc, size_data_loc); // second array starts at the end of first array
	ret->data       = pointer_offset(void, ret->entity_ids, size_entity_ids);     // third array starts at the end of second array

	ret->fn_debug_ui_render = NULL;

	return ret;
}

ITU_ComponentType itu_sys_estorage_add_component_pool(Uint64 element_size, Uint64 total_num_component, ITU_ComponentType* ref_component_type, const char* component_name);
void itu_sys_estorage_add_component_debug_ui_render(ITU_ComponentType component_type, ITU_ComponendDebugUIRender fn_debug_ui_render)
;

void itu_sys_estorage_init(int starting_entities_count, bool enable_standard_components=true)
{
	// allocate a minimum of elements at initialization time, to minimize early reallocs
	stbds_arrsetlen(ctx.entities, starting_entities_count);
	//stbds_hmset(ctx.entities_debug_names, starting_entities_count);

	if(enable_standard_components)
	{
		enable_component(Transform);
		enable_component(Sprite);
		enable_component(PhysicsData);
		enable_component(PhysicsStaticData);
		enable_component(ShapeData);

		add_component_debug_ui_render(ShapeData, itu_debug_ui_render_shapedata);
		add_component_debug_ui_render(Transform, itu_debug_ui_render_transform);
		add_component_debug_ui_render(Sprite, itu_debug_ui_render_sprite);
		add_component_debug_ui_render(PhysicsData, itu_debug_ui_render_physicsdata);
		add_component_debug_ui_render(PhysicsStaticData, itu_debug_ui_render_physicsstaticdata);

		add_system(itu_system_physics       , component_mask(PhysicsData)                                  , 0);
		add_system(itu_system_sprite_render , component_mask(Transform)   | component_mask(Sprite)         , 0);
	}
}

ITU_ComponentType itu_sys_estorage_add_component_pool(Uint64 element_size, Uint64 total_num_component, ITU_ComponentType* ref_component_type, const char* component_name)
{
	ITU_Component* pool = itu_component_pool_create(element_size, total_num_component, component_name);
	pool->type = ctx.components_count++;
	ctx.components[pool->type] = pool;

	// make component type globally available
	*ref_component_type = pool->type;

	return pool->type;
}

void itu_sys_estorage_add_component_debug_ui_render(ITU_ComponentType component_type, ITU_ComponendDebugUIRender fn_debug_ui_render)
{
	ctx.components[component_type]->fn_debug_ui_render = fn_debug_ui_render;
}

void itu_sys_estorage_clear_all_entities()
{
	stbds_arrfree(ctx.entities);
	stbds_arrfree(ctx.entities_free);
}

void itu_sys_estorage_set_systems(ITU_SystemDef* systems, int systems_count)
{
	SDL_assert(systems_count <= SYSTEMS_COUNT_MAX);

	ctx.systems_count = systems_count;
	for(int i = 0; i < systems_count; ++i)
	{
		ITU_System* system_runtime = &ctx.systems[i];
		ITU_SystemDef* system_def  = &systems[i];

		// build component pool pointers (this requires component pools to be alredy set up)
		for(int j = 0; j < COMPONENTS_COUNT_MAX; ++j)
		{
			Uint64 component_bitmask = 1ll << j;
			if(system_def->component_mask & component_bitmask)
				system_runtime->components[system_runtime->components_count++] = ctx.components[j];
		}
		for(int j = 0; j < TAGS_COUNT_MAX; ++j)
		{
			Uint64 tag_bitmask = 1ll << j;
			if(system_def->tag_mask & tag_bitmask)
				system_runtime->tags[system_runtime->tags_count++] = j;
		}
		system_runtime->fn_update = system_def->fn_update;
		system_runtime->name = system_def->name;
	}
}

void itu_sys_estorage_add_system(ITU_SystemDef system_def)
{
	if(ctx.systems_count == SYSTEMS_COUNT_MAX)
	{
		SDL_Log("WARNING maximum number of systes reached");
		return;
	}

	ITU_System* system_runtime = &ctx.systems[ctx.systems_count++];

	// build component pool pointers (this requires component pools to be alredy set up)
	for(int j = 0; j < COMPONENTS_COUNT_MAX; ++j)
	{
		Uint64 component_bitmask = 1ll << j;
		if(system_def.component_mask & component_bitmask)
			system_runtime->components[system_runtime->components_count++] = ctx.components[j];
	}
	for(int j = 0; j < TAGS_COUNT_MAX; ++j)
	{
		Uint64 tag_bitmask = 1ll << j;
		if(system_def.tag_mask & tag_bitmask)
			system_runtime->tags[system_runtime->tags_count++] = j;
	}
	system_runtime->fn_update = system_def.fn_update;
	system_runtime->name = system_def.name;
}


int itu_system_get_matching_entities(ITU_System* system, ITU_EntityId* out_entitiy_group)
{
	ITU_EntityId* min_component;
	Uint64  min_component_size = ENTITIES_COUNT_MAX + 1;
	int system_ids_count = 0;

	for(int j = 0; j < system->components_count; ++j)
		if(system->components[j]->count_alive < min_component_size)
		{
			min_component = system->components[j]->entity_ids;
			min_component_size = system->components[j]->count_alive;
		}

	for(int j = 0; j < system->tags_count; ++j)
	{
		auto tmp = ctx.tags[system->tags[j]];
		if(!tmp)
			continue;
		if(stbds_hmlen(tmp) < min_component_size)
		{
			min_component = (ITU_EntityId*)tmp;
			min_component_size = stbds_hmlen(tmp);
		}
	}

	// filter entities
	for(int k = 0; k < min_component_size; ++k)
	{
		ITU_EntityId entity_curr = min_component[k];
		bool filter_out = false;
		for(int j = 0; j < system->components_count; ++j)
		{
			ITU_Component* filtered_component = system->components[j];

			if(filtered_component->data_loc[entity_curr.index] == -1)
			{
				filter_out = true;
				break;
			}
		}

		for(int j = 0; j < system->tags_count; ++j)
		{
			ITU_TagType filtered_tag = system->tags[j];
				
			if(stbds_hmgeti(ctx.tags[filtered_tag], entity_curr) == -1)
			{
				filter_out = true;
				break;
			}
		}
		if(!filter_out)
			out_entitiy_group[system_ids_count++] = entity_curr;
	}

	return system_ids_count;
}

void itu_sys_estorage_systems_update(SDLContext* context)
{
	for(int i = 0; i < ctx.systems_count; ++i)
	{
		ITU_System* system = &ctx.systems[i];
		ITU_EntityId system_ids[ENTITIES_COUNT_MAX];
		int system_ids_count = itu_system_get_matching_entities(system, system_ids);

		system->fn_update(context, system_ids, system_ids_count);
	}
}

enum ITU_SysEstorageDebugDetailCategory { ITU_SYS_ESTORAGE_DETAIL_CATEGORY_ENTITY, ITU_SYS_ESTORAGE_DETAIL_CATEGORY_SYSTEM, ITU_SYS_ESTORAGE_DETAIL_CATEGORY_MAX };

void itu_sys_estorage_debug_render_detail_entity(SDLContext* context, ITU_EntityId id)
{
	if(!itu_entity_is_valid(id))
	{
		ImGui::Text("INVALID ENTITY");
		return;
	}
	// TODO wrap tag list rendering in appropriate function
	{
		ImGui::CollapsingHeader("tags", ImGuiTreeNodeFlags_Leaf);
		int num_tags = 0;
		for(int i = 0; i < TAGS_COUNT_MAX; ++i)
		{
			if(stbds_hmgeti(ctx.tags[i], id) == -1)
				continue;

			++num_tags;
			// TODO also wrap single tag (idx + name) rendering in appropriate function
			int loc_tag_name = stbds_hmgeti(ctx.tag_debug_names, i);
			if(loc_tag_name == -1)
				ImGui::Text("%3d", i);
			else
				ImGui::Text("%3d: %s", i, ctx.tag_debug_names[loc_tag_name].value);
		}
		if(num_tags == 0)
			ImGui::Text("none");
	}

	for(int i = 0; i < ctx.components_count; ++i)
	{
		void* component_data = itu_entity_data_get(id, i);

		if(!component_data)
			continue;

		ImGui::CollapsingHeader(ctx.components[i]->name, ImGuiTreeNodeFlags_Leaf);
		{
			if(ctx.components[i]->fn_debug_ui_render)
				ctx.components[i]->fn_debug_ui_render(context, component_data);
			else
				ImGui::Text("TODO NotYetImplemented");
		}
	}
}

void itu_sys_estorage_debug_render_detail_system(SDLContext* context, ITU_System* system, ITU_EntityId* system_ids, int system_ids_count)
{
	ImGui::CollapsingHeader("components", ImGuiTreeNodeFlags_Leaf);
	for(int i = 0; i < system->components_count; ++i)
		ImGui::Text("%s", system->components[i]->name);

	// TODO wrap tag list rendering in appropriate function
	{
		ImGui::CollapsingHeader("tags", ImGuiTreeNodeFlags_Leaf);
		int num_tags = 0;
		for(int i = 0; i < system->tags_count; ++i)
		{
			++num_tags;
			// TODO also wrap single tag (idx + name) rendering in appropriate function
			int tag = system->tags[i];
			int loc_tag_name = stbds_hmgeti(ctx.tag_debug_names, tag);
			if(loc_tag_name == -1)
				ImGui::Text("%3d", tag);
			else
				ImGui::Text("%3d: %s", tag, ctx.tag_debug_names[loc_tag_name].value);
		}
		if(num_tags == 0)
			ImGui::Text("none");
	}

	ImGui::CollapsingHeader("currently iterated entities", ImGuiTreeNodeFlags_Leaf);
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
	for(int i = 0; i < system_ids_count; ++i)
	{
		char buf[8];
		SDL_snprintf(buf, 8, "%d", i);
		itu_debug_ui_widget_entityid((char*)buf, system_ids[i]);
	}
	ImGui::PopStyleVar();
}

void itu_sys_estorage_debug_render(SDLContext* context)
{
	static ITU_SysEstorageDebugDetailCategory detail_category = ITU_SYS_ESTORAGE_DETAIL_CATEGORY_MAX;
	static int loc_selected = -1;

	ITU_EntityId selected_system_ids[ENTITIES_COUNT_MAX];
	int selected_system_ids_count;

	ImGui::Begin("debug_estorage", NULL, ImGuiWindowFlags_NoCollapse);
	ImGui::SameLine();
	ImGui::BeginChild("debug_estorage_master", ImVec2(200, 0), ImGuiChildFlags_Border | ImGuiChildFlags_ResizeX);
	{
		if(ImGui::CollapsingHeader("Entities", ImGuiTreeNodeFlags_DefaultOpen))
		{
			int entities_count = stbds_arrlen(ctx.entities);
			if(ImGui::BeginTable("debug_estorage_master_entities", 5, ImGuiTableFlags_SizingFixedFit))
			{

				ImGui::TableSetupColumn("");
				ImGui::TableSetupColumn("");
				ImGui::TableSetupColumn("name");
				ImGui::TableSetupColumn("gen");
				ImGui::TableSetupColumn("idx");
				ImGui::TableHeadersRow();
				int row_idx = 0;
				for(int i = 0; i < entities_count; ++i)
				{
					ITU_EntityId id = ctx.entities[i].id;
					if(!itu_entity_is_valid(id))
						continue;
					ImGui::TableNextRow();

					ImGui::TableNextColumn();
					char buf_del[48];
					SDL_snprintf(buf_del, 48, "X##%3ddebug_estorage_master_entities", row_idx);
					if(ImGui::Button(buf_del))
					{
						itu_entity_destroy(id);
						loc_selected = -1;
					}

					ImGui::TableNextColumn();
					char buf_id[48];
					SDL_snprintf(buf_id, 48, "%3d##debug_estorage_master_entities", row_idx);
					if(ImGui::Selectable(
						buf_id,
						detail_category == ITU_SYS_ESTORAGE_DETAIL_CATEGORY_ENTITY && row_idx == loc_selected,
						ImGuiSelectableFlags_SpanAllColumns
					))
					{
						loc_selected = i;
						detail_category = ITU_SYS_ESTORAGE_DETAIL_CATEGORY_ENTITY;
					}

					ImGui::TableNextColumn();
					int pos_debug_name = stbds_hmgeti(ctx.entities_debug_names, id);
					if(pos_debug_name != -1)
						ImGui::Text("%s", ctx.entities_debug_names[pos_debug_name].value);

					ImGui::TableNextColumn();
					ImGui::Text("%d", id.generation);

					ImGui::TableNextColumn();
					ImGui::Text("%d", id.index);

					++row_idx;
				}

				ImGui::EndTable();
			}
		}

		if(ImGui::CollapsingHeader("Systems", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if(ImGui::BeginTable("debug_estorage_master_systems", 5, ImGuiTableFlags_SizingFixedFit))
			{
				ImGui::TableSetupColumn("");
				ImGui::TableSetupColumn("name");
				ImGui::TableSetupColumn("comp");
				ImGui::TableSetupColumn("tags");
				ImGui::TableSetupColumn("entities");
				ImGui::TableHeadersRow();
				for(int i = 0; i < ctx.systems_count; ++i)
				{
					ITU_System* system = &ctx.systems[i];
					ImGui::TableNextRow();

					ImGui::TableNextColumn();
					char buf_id[48];
					SDL_snprintf(buf_id, 48, "%3d##debug_estorage_master_systems", i);
					if(ImGui::Selectable(
						buf_id,
						detail_category == ITU_SYS_ESTORAGE_DETAIL_CATEGORY_SYSTEM && i == loc_selected,
						ImGuiSelectableFlags_SpanAllColumns
					))
					{
						loc_selected = i;
						detail_category = ITU_SYS_ESTORAGE_DETAIL_CATEGORY_SYSTEM;
					}

					ImGui::TableNextColumn();
					ImGui::Text("%s", system->name);

					ImGui::TableNextColumn();
					ImGui::Text("%d", system->components_count);

					ImGui::TableNextColumn();
					ImGui::Text("%d", system->tags_count);

					ImGui::TableNextColumn();
					ITU_EntityId scratch_system_ids[ENTITIES_COUNT_MAX];
					int scratch_system_ids_count;

					ITU_EntityId* system_ids;
					int* system_ids_count;

					if(detail_category == ITU_SYS_ESTORAGE_DETAIL_CATEGORY_SYSTEM && i == loc_selected)
					{
						system_ids = selected_system_ids;
						system_ids_count = &selected_system_ids_count;
					}
					else
					{
						system_ids = scratch_system_ids;
						system_ids_count = &scratch_system_ids_count;
					}

					*system_ids_count = itu_system_get_matching_entities(system, system_ids);
					ImGui::Text("%d", *system_ids_count);
				}

				ImGui::EndTable();
			}
		}
		ImGui::EndChild();
	}
	ImGui::SameLine();

	ImGui::BeginChild("debug_estorage_detail", ImVec2(0, 0), ImGuiChildFlags_Border);
	{
		if(loc_selected != -1)
			switch(detail_category)
			{
				case ITU_SYS_ESTORAGE_DETAIL_CATEGORY_ENTITY: itu_sys_estorage_debug_render_detail_entity(context, ctx.entities[loc_selected].id); break;
				case ITU_SYS_ESTORAGE_DETAIL_CATEGORY_SYSTEM: itu_sys_estorage_debug_render_detail_system(context, &ctx.systems[loc_selected], selected_system_ids, selected_system_ids_count); break;
				default: /* do nothing */ break;
			}
		ImGui::EndChild();
	}
	
	ImGui::End();
}

void itu_sys_estorage_tag_set_debug_name(int tag, const char* tag_debug_name)
{
	stbds_hmput(ctx.tag_debug_names, tag, tag_debug_name);
}

void itu_component_pool_assign(ITU_Component* component_pool, ITU_EntityId entity)
{
	SDL_assert(component_pool);
	SDL_assert(component_pool->count_alive < component_pool->count_max);

	// TODO check that requested entry is actually free

	Uint64 i = component_pool->count_alive++;
	component_pool->data_loc[entity.index] = i;
	component_pool->entity_ids[i] = entity;
	SDL_memset((unsigned char*)component_pool->data + component_pool->element_size * i, 0, component_pool->element_size);
}

void itu_component_pool_data_get(ITU_Component* component_pool, ITU_EntityId entity, void* out_data_copy)
{
	SDL_assert(component_pool);

	Uint64 loc = component_pool->data_loc[entity.index];
	void* data = pointer_offset(void, component_pool->data, component_pool->element_size * loc);
	SDL_memcpy(out_data_copy, data, component_pool->element_size);
}

void itu_component_pool_data_set(ITU_Component* component_pool, ITU_EntityId entity, void* in_data_copy)
{
	SDL_assert(component_pool);

	Uint64 loc = component_pool->data_loc[entity.index];
	void* data = pointer_offset(void, component_pool->data, component_pool->element_size * loc);
	SDL_memcpy(data, in_data_copy, component_pool->element_size);
}

void itu_component_pool_remove(ITU_Component* component_pool, ITU_EntityId entity)
{
	SDL_assert(component_pool);
	SDL_assert(component_pool->data_loc[entity.index] != -1);

	Uint64 loc_curr = entity.index;
	Uint64 loc_last = component_pool->count_alive - 1;
	component_pool->entity_ids[loc_curr] = component_pool->entity_ids[loc_last];

	void* ptr_curr = pointer_offset(void, component_pool->data, loc_curr * component_pool->element_size);
	void* ptr_last = pointer_offset(void, component_pool->data, loc_last * component_pool->element_size);
	SDL_memcpy(ptr_curr, ptr_last, component_pool->element_size);

	component_pool->count_alive--;
}

void itu_component_pool_clear(ITU_Component* component_pool)
{
	SDL_assert(component_pool);

	component_pool->count_alive = 0;
}


ITU_EntityId itu_entity_create()
{
	if(stbds_arrlen(ctx.entities_free) > 0)
	{
		ITU_EntityId id_recycled = stbds_arrpop(ctx.entities_free);
		ctx.entities[id_recycled.index].id.index = id_recycled.index;
		ctx.entities[id_recycled.index].id.generation = id_recycled.generation + 1;
		return ctx.entities[id_recycled.index].id;
	}

	ITU_Entity entity_data;
	entity_data.id.generation = 0;
	entity_data.id.index = stbds_arrlen(ctx.entities);
	entity_data.component_mask = 0;
	stbds_arrput(ctx.entities, entity_data);

	return entity_data.id;
}

void  itu_entity_set_debug_name(ITU_EntityId id, const char* debug_name)
{
	// NOTE: allocating every single name is BAD, but we haven't looked in allocaiton startegies and memory arenas yet
	int len = SDL_strlen(debug_name);
	char* name_storage = (char*)SDL_malloc(len + 1);
	SDL_memcpy(name_storage, debug_name, len);
	name_storage[len] = 0;

	stbds_hmput(ctx.entities_debug_names, id, name_storage);
}

bool itu_entity_equals(ITU_EntityId a, ITU_EntityId b)
{
	return a.generation == b.generation && a.generation == b.generation;
}

bool itu_entity_is_valid(ITU_EntityId id)
{
	return id.index != -1 && ctx.entities[id.index].id.generation == id.generation;
}

void itu_entity_id_to_stringid(ITU_EntityId id, char* buffer, int max_len)
{
	// NOTE: this is super slow, but we have to do this if we want to leverage imgui for out UI toolkit
	SDL_snprintf(buffer, max_len, "%d-%d", id.generation, id.index);
}

// `in_data_copy`: default component init. Can be null
void itu_entity_component_add(ITU_EntityId id, ITU_ComponentType component_type, void* in_data_copy)
{
	SDL_assert(component_type < COMPONENTS_COUNT_MAX);
	Uint64 component_bit = 1ll << component_type;

	if(!itu_entity_is_valid(id))
	{
		SDL_Log("WARNING invalid entity\n");
		return;
	}

	if(ctx.entities[id.index].component_mask & component_bit)
	{
		SDL_Log("WARNING entity %d alread has component type %d\n", id.index, component_type);
		return;
	}

	ctx.entities[id.index].component_mask |= component_bit;

	ITU_Component* component = ctx.components[component_type];
	itu_component_pool_assign(component, id);
	if(in_data_copy)
		itu_component_pool_data_set(component, id, in_data_copy);
}

void itu_entity_component_remove(ITU_EntityId id, ITU_ComponentType component_type)
{
	SDL_assert(component_type < COMPONENTS_COUNT_MAX);
	Uint64 component_bit = 1ll << component_type;
	
	if(!itu_entity_is_valid(id))
	{
		SDL_Log("WARNING invalid entity\n");
		return;
	}

	if(!(ctx.entities[id.index].component_mask & component_bit))
	{
		SDL_Log("WARNING entity %d does NOT has component type %d\n", id.index, component_type);
		return;
	}

	ctx.entities[id.index].component_mask &= ~component_bit; // keeps all bits of `id.component_mask` the same except for component_bit, which is set to 0

	ITU_Component* component = ctx.components[component_type];
	itu_component_pool_remove(component, id);
}

void* itu_entity_data_get(ITU_EntityId id, ITU_ComponentType component_type)
{
	SDL_assert(component_type < COMPONENTS_COUNT_MAX);

	if(!itu_entity_is_valid(id))
	{
		SDL_Log("WARNING invalid entity\n");
		return NULL;
	}

	Uint64 component_bit = 1ll << component_type;
	if(!(ctx.entities[id.index].component_mask & component_bit))
	{
		//SDL_Log("WARNING entity %d does NOT have component type %d\n", id.index, component_type);
		return NULL;
	}

	ITU_Component* component = ctx.components[component_type];
	
	Uint64 loc = component->data_loc[id.index];
	return pointer_index(component->data, loc, component->element_size);
}

void itu_entity_tag_add(ITU_EntityId id, ITU_TagType tag)
{
	SDL_assert(tag < TAGS_COUNT_MAX);
	ITU_ComponentTagStorage foo = { id };
	stbds_hmputs(ctx.tags[tag], foo);
}

void itu_entity_tag_remove(ITU_EntityId id, ITU_TagType tag)
{
	SDL_assert(tag < TAGS_COUNT_MAX);
	stbds_hmdel(ctx.tags[tag], id);
}

bool itu_entity_tag_has(ITU_EntityId id, ITU_TagType tag)
{
	SDL_assert(tag < TAGS_COUNT_MAX);
	return stbds_hmgeti(ctx.tags[tag], id) != -1;
}

void itu_entity_destroy(ITU_EntityId id)
{
	if(!itu_entity_is_valid(id))
	{
		SDL_Log("WARNING invalid entity\n");
		return;
	}

	//ITU_EntityId target_id = ctx.entities[id.index].id;
	//if(target_id.index == -1)
	//{
	//	SDL_Log("WARNING trying to delete entity already deleted\n");
	//	return;
	//}
	//
	//if(target_id.generation != id.generation)
	//{
	//	SDL_Log("WARNING generation mismatch\n");
	//	return;
	//}

	Uint64 component_mask = ctx.entities[id.index].component_mask;

	// free all components
	// TODO faster way to do this?
	for(int i = 0; i < ctx.components_count; ++i)
	{
		Uint64 component_bit = 1ll << i;
		if(!(component_mask & component_bit))
			continue;
		itu_entity_component_remove(id, i);
	}

	// free all tags
	// TODO faster way to do this?
	for(int i = 0; i < TAGS_COUNT_MAX; ++i)
		itu_entity_tag_remove(id, i);

	// clear debug name
	int pos_name_storage = stbds_hmgeti(ctx.entities_debug_names, id);
	if(pos_name_storage != -1)
	{
		// NOTE: as mentioned when allocating this, doing alloc/dealloc of every single name in isolation is not good for peformance
		//       we'll give ourself a pass for this since this is a debug feature, but it's worth to keep in mind
		SDL_free(ctx.entities_debug_names[pos_name_storage].value);
		stbds_hmdel(ctx.entities_debug_names, id);
	}

	ctx.entities[id.index].id.index = -1;
	ctx.entities[id.index].id.generation++;
	ctx.entities[id.index].component_mask = 0;
	stbds_arrput(ctx.entities_free, id);
}


void itu_debug_ui_widget_entityid(const char* label, ITU_EntityId id)
{
	if(!itu_entity_is_valid(id))
		ImGui::LabelText(label, "INVALID ENTITY");
	else
		ImGui::LabelText(label, "%s (%d, %d)", ctx.entities_debug_names[id.index].value, id.generation, id.index);
}