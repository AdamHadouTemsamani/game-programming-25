#ifndef ITU_LIB_BOX2D_HPP
#define ITU_LIB_BOX2D_HPP

#include <box2d/box2d.h>
#include <itu_lib_engine.hpp>

struct b2BodyId;
struct b2WorldId;

#if (defined ITU_LIB_BOX2D_IMPLEMENTATION) || (defined ITU_UNITY_BUILD)

void fn_box2d_wrapper_draw_polygon(b2Transform transform, const b2Vec2* vertices, int vertexCount, float radius, b2HexColor color, void* context)
{
	SDLContext* sdl_context = (SDLContext*)context;

	float r = (float)((color & 0xFF0000) >> 16) / 255.0f;
	float g = (float)((color & 0x00FF00) >>  8) / 255.0f;
	float b = (float)((color & 0x0000FF))       / 255.0f;
	SDL_FColor color_fill = {r, g, b, 0.25f};

	SDL_FPoint vs_outline[vertexCount+1];
	SDL_Vertex vs[vertexCount];
	SDL_zeroa(vs);
	
	for (int i = 0; i < vertexCount; ++i)
	{
		b2Vec2 pos_b2world = b2TransformPoint(transform, vertices[i]);
		vec2f pos = point_global_to_screen(sdl_context, value_cast(vec2f, pos_b2world));

		vs[i].color = color_fill;
		vs[i].position.x = pos.x;
		vs[i].position.y = pos.y;
		vs[i].tex_coord.x = 0;
		vs[i].tex_coord.y = 0;
		vs_outline[i].x = vs[i].position.x;
		vs_outline[i].y = vs[i].position.y;
	}
	vs_outline[vertexCount].x = vs_outline[0].x;
	vs_outline[vertexCount].y = vs_outline[0].y;

	int indices_count = (vertexCount - 2)*3;
	int indices[indices_count];
	int c = 0;
	for (int i = 2; i < vertexCount; ++i)
	{
		indices[c++] = 0;
		indices[c++] = i - 1;
		indices[c++] = i;
	}

	SDL_RenderGeometry(sdl_context->renderer, NULL, vs, vertexCount, indices, indices_count);
	
	SDL_SetRenderDrawColor(sdl_context->renderer, (color & 0xFF0000) >> 16, (color & 0x00FF00) >>  8, (color & 0x0000FF), 0xFF);
	SDL_RenderLines(sdl_context->renderer, vs_outline, vertexCount+1);
}

void fn_box2d_wrapper_draw_circle(b2Transform transform, float radius, b2HexColor b2_color, void* context)
{
	SDLContext* sdl_context = (SDLContext*)context;

	float r = (float)((b2_color & 0xFF0000) >> 16) / 255.0f;
	float g = (float)((b2_color & 0x00FF00) >>  8) / 255.0f;
	float b = (float)((b2_color & 0x0000FF))       / 255.0f;
	color color_fill = {r, g, b, 1.0f};

	float angle_increment = TAU / 8;
	b2Vec2 vertices[8];

	for(int i = 0; i < 8; ++i)
	{
		float angle = angle_increment * i;
		float px = radius * SDL_cos(angle);
		float py = radius * SDL_sin(angle);
		// 1st quadrant
		vertices[i].x = px;
		vertices[i].y = py;
	}

	//itu_lib_render_draw_world_circle(sdl_context, pos, radius, 8, color_fill);
	fn_box2d_wrapper_draw_polygon(transform, vertices, 8, radius, b2_color, context);
}
#endif // (defined ITU_LIB_BOX2D_IMPLEMENTATION) || (define ITU_UNITY_BUILD)

#endif // ITU_LIB_BOX2D_HPP