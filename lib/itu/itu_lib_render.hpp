// itu_lib_renderer.hpp
// simple library to render debug shapes
// limitations
// - no rotation
// - only polygons have color fill
// 
// TODO
// - get rid of VLAs
// - get rid of BUFFER_SIZE

#ifndef ITU_LIB_RENDER_HPP
#define ITU_LIB_RENDER_HPP

#include <SDL3/SDL_render.h>
#include <itu_common.hpp>

#define MAX_CIRCLE_VERTICES 16

void itu_lib_render_draw_point(SDL_Renderer* renderer, vec2f pos, float half_size, color color);
void itu_lib_render_draw_rect(SDL_Renderer* renderer, vec2f min, vec2f max, color color);
void itu_lib_render_draw_rect_fill(SDL_Renderer* renderer, vec2f min, vec2f max, color color);
void itu_lib_render_draw_circle(SDL_Renderer* renderer, vec2f center, float radius, int vertex_count, color);
void itu_lib_render_draw_polygon(SDL_Renderer* renderer, vec2f position, const vec2f* vertices, int vertexCount, color color);

#if defined ITU_LIB_RENDER_IMPLEMENTATION || defined ITU_UNITY_BUILD

void itu_lib_render_draw_point(SDL_Renderer* renderer, vec2f pos, float half_size, color color)
{
	//itu_lib_render_draw_rect(renderer, pos - vec2f { size / 2, size / 2}, vec2f { size, size }, color);
	SDL_SetRenderDrawColorFloat(renderer, color.r, color.g, color.b,color.a);
	SDL_RenderLine(renderer, pos.x - half_size, pos.y, pos.x + half_size, pos.y);
	SDL_RenderLine(renderer, pos.x, pos.y - half_size, pos.x, pos.y + half_size);
}

void itu_lib_render_draw_rect(SDL_Renderer* renderer, vec2f min, vec2f extents, color color)
{
	SDL_FRect rect;
	rect.x = min.x;
	rect.y = min.y;
	rect.w = extents.x;
	rect.h = extents.y;

	SDL_SetRenderDrawColorFloat(renderer, color.r, color.g, color.b, color.a);
	SDL_RenderRect(renderer, &rect);
}

void itu_lib_render_draw_rect_fill(SDL_Renderer* renderer, vec2f min, vec2f extents, color color)
{
	SDL_FRect rect;
	rect.x = min.x;
	rect.y = min.y;
	rect.w = extents.x;
	rect.h = extents.y;

	SDL_SetRenderDrawColorFloat(renderer, color.r, color.g, color.b, color.a);
	SDL_RenderFillRect(renderer, &rect);
}

// NOTE: vertex count must be smaller than `MAX_CIRCLE_VERTICES` (defaults to 16, but you can change it if you need to)
void itu_lib_render_draw_circle(SDL_Renderer* renderer, vec2f center, float radius, int vertex_count, color color)
{
	SDL_assert(vertex_count <= MAX_CIRCLE_VERTICES);

	SDL_FPoint points[MAX_CIRCLE_VERTICES + 1];
	
	float angle_increment = TAU / vertex_count;

	// very slow, a lot of trig
	// we could also do a single quadrant and mirror the rest
	for(int i = 0; i < vertex_count; ++i)
	{
		float angle = angle_increment * i;
		points[i].x = center.x + radius * SDL_cos(angle);
		points[i].y = center.y + radius * SDL_sin(angle);
	}
	points[vertex_count] = points[0];
	
	SDL_SetRenderDrawColorFloat(renderer, color.r, color.g, color.b, 0xFF);
	SDL_RenderLines(renderer, points, vertex_count + 1);
}

void itu_lib_render_draw_polygon(SDL_Renderer* renderer, vec2f position, const vec2f* vertices, int vertexCount, color color)
{
#define BUFFER_SIZE 128 + 1
	SDL_FColor color_fill = { color.r, color.g, color.b, color.a };

	SDL_FPoint vs_outline[BUFFER_SIZE];
	SDL_Vertex vs[BUFFER_SIZE];
	SDL_zeroa(vs);
	
	for (int i = 0; i < vertexCount; ++i)
	{
		vec2f pos = position + vertices[i];

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
	int indices[BUFFER_SIZE];
	int c = 0;
	for (int i = 2; i < vertexCount; ++i)
	{
		indices[c++] = 0;
		indices[c++] = i - 1;
		indices[c++] = i;
	}

	SDL_RenderGeometry(renderer, NULL, vs, vertexCount, indices, indices_count);
	
	SDL_SetRenderDrawColorFloat(renderer, color.r, color.g, color.b, 1.0f);
	SDL_RenderLines(renderer, vs_outline, vertexCount + 1);
}

# endif //ITU_LIB_RENDER_IMPLEMENTATION

#endif // ITU_LIB_RENDER_HPP