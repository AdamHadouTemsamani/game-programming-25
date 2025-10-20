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

#ifndef ITU_UNITY_BUILD
#include <SDL3/SDL_render.h>
#include <itu_common.hpp>
#include <itu_lib_engine.hpp>
#endif

#define MAX_CIRCLE_VERTICES 16

void itu_lib_render_draw_point(SDL_Renderer* renderer, vec2f pos, float half_size, color color);
void itu_lib_render_draw_line(SDL_Renderer* renderer, vec2f p0, vec2f p1, color color);
void itu_lib_render_draw_rect(SDL_Renderer* renderer, vec2f min, vec2f max, color color);
void itu_lib_render_draw_rect_fill(SDL_Renderer* renderer, vec2f min, vec2f max, color color);
void itu_lib_render_draw_circle(SDL_Renderer* renderer, vec2f center, float radius, int vertex_count, color);
void itu_lib_render_draw_polygon(SDL_Renderer* renderer, vec2f position, const vec2f* vertices, int vertexCount, color color);

void itu_lib_render_draw_world_point(SDLContext* context, vec2f pos, float half_size, color color);
void itu_lib_render_draw_world_line(SDLContext* context, vec2f p0, vec2f p1, color color);
void itu_lib_render_draw_world_rect(SDLContext* context, vec2f min, vec2f max, color color);
void itu_lib_render_draw_world_rect_fill(SDLContext* context, vec2f min, vec2f max, color color);
void itu_lib_render_draw_world_circle(SDLContext* context, vec2f center, float radius, int vertex_count, color);
void itu_lib_render_draw_world_polygon(SDLContext* context, vec2f position, const vec2f* vertices, int vertexCount, color color);

void itu_lib_render_draw_world_grid(SDLContext* context);

#endif // ITU_LIB_RENDER_HPP

#if defined ITU_LIB_RENDER_IMPLEMENTATION || defined ITU_UNITY_BUILD

void itu_lib_render_draw_point(SDL_Renderer* renderer, vec2f pos, float half_size, color color)
{
	//itu_lib_render_draw_rect(renderer, pos - vec2f { size / 2, size / 2}, vec2f { size, size }, color);
	SDL_SetRenderDrawColorFloat(renderer, color.r, color.g, color.b,color.a);
	SDL_RenderLine(renderer, pos.x - half_size, pos.y, pos.x + half_size, pos.y);
	SDL_RenderLine(renderer, pos.x, pos.y - half_size, pos.x, pos.y + half_size);
}

void itu_lib_render_draw_line(SDL_Renderer* renderer, vec2f p0, vec2f p1, color color)
{
	SDL_SetRenderDrawColorFloat(renderer, color.r, color.g, color.b,color.a);
	SDL_RenderLine(renderer, p0.x, p0.y, p1.x, p1.y);
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
		float px = radius * SDL_cos(angle);
		float py = radius * SDL_sin(angle);
		// 1st quadrant
		points[i].x = center.x + px;
		points[i].y = center.y + py;
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

void itu_lib_render_draw_world_point(SDLContext* context, vec2f pos, float half_size, color color)
{
	itu_lib_render_draw_point(context->renderer, point_global_to_screen(context, pos), half_size, color);
}

void itu_lib_render_draw_world_line(SDLContext* context, vec2f p0, vec2f p1, color color)
{
	itu_lib_render_draw_line(context->renderer, point_global_to_screen(context, p0), point_global_to_screen(context, p1), color);
}

void itu_lib_render_draw_world_rect(SDLContext* context, vec2f min, vec2f max, color color);
void itu_lib_render_draw_world_rect_fill(SDLContext* context, vec2f min, vec2f max, color color);
void itu_lib_render_draw_world_circle(SDLContext* context, vec2f center, float radius, int vertex_count, color c)
{
	itu_lib_render_draw_circle(context->renderer, point_global_to_screen(context, center), size_global_to_screen(context, radius), vertex_count, c);
}

void itu_lib_render_draw_world_polygon(SDLContext* context, vec2f position, const vec2f* vertices, int vertexCount, color color)
{
	itu_lib_render_draw_polygon(context->renderer, point_global_to_screen(context, position), vertices, vertexCount, color);
}

void itu_lib_render_draw_world_grid(SDLContext* context)
{
	// const float spacing_min = 32;
	// const float spacing_max = 128;
	
	Camera* camera = context->camera_active;

	float spacing = spacing = camera->pixels_per_unit * camera->zoom;

	// float scaling_factor = 1;
	// while(spacing > spacing_max)
	// {
	// 	spacing /= 2;
	// 	scaling_factor /= 2;
	// }
	// while(spacing < spacing_min)
	// {
	// 	spacing *= 2;
	// 	scaling_factor *= 2;
	// }

	
	vec2f screen_size = vec2f { context->window_w, context->window_h };
	vec2f camera_world_min = camera->world_position - screen_size / (camera->pixels_per_unit * camera->zoom * 2);
	vec2f camera_world_max = camera->world_position + screen_size / (camera->pixels_per_unit * camera->zoom * 2);
	camera_world_min.x = ((int)camera_world_min.x - 1);
	camera_world_min.y = ((int)camera_world_min.y - 1);
	camera_world_max.x = ((int)camera_world_max.x + 1);
	camera_world_max.y = ((int)camera_world_max.y + 1);

	vec2f camera_window_min = point_global_to_screen(context, camera_world_min);
	vec2f camera_window_max = point_global_to_screen(context, camera_world_max);

	float tmp = camera_window_min.y;
	camera_window_min.y = camera_window_max.y;
	camera_window_max.y = tmp;

	
	vec2f offset;
	offset.x = 0;
	offset.y = 0;
	
	SDL_SetRenderDrawColorFloat(context->renderer, 0.7f, 0.7f, 0.7f, 0.5f);
	float min_x = camera_window_min.x;
	float min_y = camera_window_min.y;

	for(float i = min_x + offset.x; i <= camera_window_max.x; i += spacing)
		SDL_RenderLine(context->renderer, i, camera_window_min.y, i, camera_window_max.y);

	for(float i = min_y + offset.y; i <= camera_window_max.y; i += spacing)
		SDL_RenderLine(context->renderer, camera_window_min.x, i, camera_window_max.x, i);
}
# endif //ITU_LIB_RENDER_IMPLEMENTATION
