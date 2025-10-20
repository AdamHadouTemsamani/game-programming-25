#ifndef ITU_LIB_DEBUG_UI_HPP
#define ITU_LIB_DEBUG_UI_HPP

void itu_debug_ui_render_transform(SDLContext* context, void* data);
void itu_debug_ui_render_sprite(SDLContext* context, void* data);
void itu_debug_ui_render_physicsdata(SDLContext* context, void* data);
void itu_debug_ui_render_physicsstaticdata(SDLContext* context, void* data);
void itu_debug_ui_render_shapedata(SDLContext* context, void* data);

#endif // ITU_LIB_DEBUG_UI_HPP

// TMP
#define ITU_LIB_DEBUG_UI_IMPLEMENTATION

#if (defined ITU_LIB_DEBUG_UI_IMPLEMENTATION) || (defined ITU_UNITY_BUILD)

#ifndef ITU_UNITY_BUILD
#include <imgui/imgui.h>
#include <box2d/box2d.h>

#endif

void itu_debug_ui_render_transform(SDLContext* context, void* data)
{
	Transform* data_transform  = (Transform*)data;
	ImGui::DragFloat2("position", &data_transform->position.x);
	ImGui::DragFloat2("scale", &data_transform->scale.x);

	float rotation_deg = data_transform->rotation * RAD_2_DEG;
	if(ImGui::DragFloat("rotation", &rotation_deg))
		data_transform->rotation = rotation_deg * DEG_2_RAD;

	itu_lib_render_draw_world_point(context, data_transform->position, 5, COLOR_YELLOW);
}

void itu_debug_ui_render_sprite(SDLContext* context, void* data)
{
	Sprite* data_sprite = (Sprite*)data;

	//ImGui::LabelText("texture", "TODO NotYetImplemented");
	ImGui::DragFloat4("texture rect", &data_sprite->rect.x);
	ImGui::DragFloat2("pivot", &data_sprite->pivot.x);

	ImGui::ColorEdit4("tint", &data_sprite->tint.r);
	ImGui::Checkbox("Flip Hor.", &data_sprite->flip_horizontal);
}

void itu_debug_ui_render_physicsdata(SDLContext* context, void* data)
{
	PhysicsData* data_body = (PhysicsData*)data;

	ImGui::DragFloat2("velocity", &data_body->velocity.x);
	ImGui::DragFloat("torque", &data_body->torque);

	// TODO show definition data (either here, or in a more appropriate place)
}

void itu_debug_ui_render_physicsstaticdata(SDLContext* context, void* data)
{
	// nothing to show here at runtime

	// TODO show definition data (either here, or in a more appropriate place)

}

const char* const b2_shape_names[6] = 
{
	"circle",
	"capsule",
	"segment (not supported)",
	"polygon",
	"chainSegment (not supported)",
	"invalid shape"
};

// NOTE: this function renders shapes using current b2d transform information, so it looks like the
//       shapes "stutter" a bit, since we are doing a smooth interpolation between b2d states for rendering
void itu_debug_ui_render_shapedata(SDLContext* context, void* data)
{
	ShapeData* data_shape = (ShapeData*)data;
	
	b2ShapeId id_shape = data_shape->shape_id;
	b2BodyId id_body = b2Shape_GetBody(id_shape);
	b2Transform transform = b2Body_GetTransform(id_body);

	b2ShapeType shape_type = b2Shape_GetType(id_shape);
	int shape_idx = (int)shape_type;
	int tmp = (sizeof(b2_shape_names) / sizeof((b2_shape_names)[0]));
	ImGui::Combo("type", &shape_idx, b2_shape_names, tmp);

	bool dirty_shape = shape_idx != shape_type;
	bool dirty_data = false;

	// show debug ui based on CURRENT shape
	switch(shape_type)
	{
		case b2_circleShape:
		{
			b2Circle circle = b2Shape_GetCircle(id_shape);
			transform.p = b2TransformPoint(transform, circle.center);
			fn_box2d_wrapper_draw_circle(transform , circle.radius, b2_colorLightGreen, context);

			dirty_data |= ImGui::DragFloat2("center", &circle.center.x);
			dirty_data |= ImGui::DragFloat("radius", &circle.radius);
			if(dirty_data)
				b2Shape_SetCircle(id_shape, &circle);
			break;
		}
		case b2_polygonShape:
		{
			b2Polygon poly = b2Shape_GetPolygon(id_shape);
			fn_box2d_wrapper_draw_polygon(transform, poly.vertices, poly.count, poly.radius, b2_colorLightGreen, context);

			dirty_data |= ImGui::DragInt("vertex count", &poly.count, 1, 3, B2_MAX_POLYGON_VERTICES);
			for(int i = 0; i < poly.count; ++i)
			{
				char buf[4];
				SDL_snprintf(buf, 4, "v%d", i);
				dirty_data |= ImGui::DragFloat2(buf, &poly.vertices[i].x);
			}
			if(dirty_data)
				b2Shape_SetPolygon(id_shape, &poly);
			break;
		}
		case b2_capsuleShape:
		{
			b2Capsule capsule = b2Shape_GetCapsule(id_shape);
			b2Vec2 p1 = b2TransformPoint(transform, capsule.center1);
			b2Vec2 p2 = b2TransformPoint(transform, capsule.center2);
			fn_box2d_wrapper_draw_capsule(p1, p2, capsule.radius, b2_colorLightGreen, context);

			dirty_data |= ImGui::DragFloat2("p1", &capsule.center1.x);
			dirty_data |= ImGui::DragFloat2("p2", &capsule.center1.x);
			dirty_data |= ImGui::DragFloat("radius", &capsule.radius);

			if(dirty_data)
				b2Shape_SetCapsule(id_shape, &capsule);
			break;
		}
		default:
		{
			ImGui::Text("shape not supported");
			break;
		}
	}

	if(dirty_shape)
		// update data based on NEWLY SELECTED shape
		switch(shape_idx)
		{
			case b2_circleShape:
			{
				b2Circle circle = { 0 };
				circle.radius = 0.5f;
				b2Shape_SetCircle(id_shape, &circle);
				break;
			}
			case b2_polygonShape:
			{
				b2Polygon poly = b2MakeBox(0.5f, 0.5f);
				b2Shape_SetPolygon(id_shape, &poly);
				break;
			}
			case b2_capsuleShape:
			{
				b2Capsule capsule = { 0 };
				capsule.radius = 0.5f;
				b2Shape_SetCapsule(id_shape, &capsule);
				break;
			}
		}
}

#endif // (defined ITU_LIB_DEBUG_UI_IMPLEMENTATION) || (defined ITU_UNITY_BUILD)