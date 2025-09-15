// itu_lib_overlaps.hpp
// simple library implementing a number of overlap tests for 2D convex shapes
// this library works with instananeus information only, no previos positions or velocities, therefore can't be too precise
// (that's also the reason why it's not called a "collision" library)
//
// supported primitives:
// - points
// - segments
// - circles
// - rects
// - convex polygons
// 
// important notes:
// - all tests are performed with strict disequalities, which is probably worse for heavily physic-based games but
//   it's nicer for grid-based games. A few concequences of this are:
//       - points and segment cannot "overlap" (which makes sense mathematically. The test to decide if a point lies on
//         a segment will reside in a different library)
//       - parallel segments are not considering overlapping (see point above)
// - polygons are assumed to be in CCW (counter-clockwise) order
// - polygon methods use simple algorithms, so they won't scale to polygons with a lot of edges

#ifndef ITU_LIB_COLLISIONS_HPP
#define ITU_LIB_COLLISIONS_HPP

#include <itu_common.hpp>

// SDL functions used here (all coming from `itu_common`):
// - SDL_Log()
// - SDL_sqrt()
// - SDL_assert()


bool itu_lib_overlaps_point_circle(vec2f point, vec2f circle_center, float circle_radius);
bool itu_lib_overlaps_point_rect(vec2f point, vec2f rect_min, vec2f rect_max);
bool itu_lib_overlaps_segment_circle(vec2f segment_a, vec2f segment_b, vec2f circle_center, float circle_radius);
bool itu_lib_overlaps_segment_segment(vec2f segment_0_a, vec2f segment_0_b, vec2f segment_1_a, vec2f segment_1_b);
bool itu_lib_overlaps_segment_rect(vec2f segment_a, vec2f segment_b, vec2f rect_min, vec2f rect_max);
bool itu_lib_overlaps_circle_circle(vec2f circle_center_0, float circle_radius_0, vec2f circle_center_1, float circle_radius_1);
bool itu_lib_overlaps_circle_rect(vec2f circle_center, float circle_radius, vec2f rect_min, vec2f rect_max);
bool itu_lib_overlaps_rect_rect(vec2f rect_min_0, vec2f rect_max_0, vec2f rect_min_1, vec2f rect_max_1);

bool itu_lib_overlaps_point_polygon(vec2f point, vec2f* polygon_vertices, int poligon_vertices_count);
bool itu_lib_overlaps_segment_polygon(vec2f segment_a, vec2f segment_b, vec2f* polygon_vertices, int poligon_vertices_count);
bool itu_lib_overlaps_circle_polygon(vec2f circle_center, float circle_radius, vec2f* polygon_vertices, int poligon_vertices_count);
bool itu_lib_overlaps_rect_polygon(vec2f rect_min, vec2f rect_max, vec2f* polygon_vertices, int poligon_vertices_count);
bool itu_lib_overlaps_polygon_polygon(vec2f* polygon_0_vertices, int poligon_0_vertices_count, vec2f* polygon_1_vertices, int poligon_1_vertices_count, vec2f* out_simplex, int* out_simplex_count);

#endif

#if defined ITU_LIB_COLLISIONS_IMPLEMENTATION || defined ITU_UNITY_BUILD

inline bool itu_lib_overlaps_point_circle(vec2f point, vec2f circle_center, float circle_radius)
{
	return length_sq(point - circle_center) < circle_radius * circle_radius;
}

inline bool itu_lib_overlaps_point_rect(vec2f point, vec2f rect_min, vec2f rect_max)
{
	return point.x > rect_min.x && point.x < rect_max.x && point.y > rect_min.y && point.y < rect_max.y;
}

inline bool itu_lib_overlaps_segment_circle(vec2f segment_a, vec2f segment_b, vec2f circle_center, float circle_radius)
{
	vec2f d = segment_b - segment_a;
	vec2f f = segment_a - circle_center;
	
	float a = dot(d, d);
	float b = 2*dot(f, d);
	float c = dot(f, f) - circle_radius*circle_radius;

	// TODO this sqrt is not needed, find formula to get rid of it
	float discriminant = SDL_sqrt(b*b-4*a*c);

	float t1 = (-b - discriminant) / (2*a);
	float t2 = (-b + discriminant) / (2*a);

	return (t1 >= 0 && t1 <= 1) || (t2 >= 0 && t2 <= 1) || itu_lib_overlaps_point_circle(segment_a, circle_center, circle_radius);
}

// NOTE: colinear segments are NOT considered overlapping!
inline bool itu_lib_overlaps_segment_segment(vec2f segment_0_a, vec2f segment_0_b, vec2f segment_1_a, vec2f segment_1_b)
{
	// from https://en.wikipedia.org/wiki/Line%E2%80%93line_intersection#Given_two_points_on_each_line_segment
	float x1 = segment_0_a.x;
	float x2 = segment_0_b.x;
	float x3 = segment_1_a.x;
	float x4 = segment_1_b.x;
	float y1 = segment_0_a.y;
	float y2 = segment_0_b.y;
	float y3 = segment_1_a.y;
	float y4 = segment_1_b.y;
	
	float d =   (x1 - x2)*(y3 - y4) - (y1 - y2)*(x3 - x4);
	float t =  ((x1 - x3)*(y3 - y4) - (y1 - y3)*(x3 - x4))/d;
	float u = -((x1 - x2)*(y1 - y3) - (y1 - y2)*(x1 - x3))/d;

	return t > 0 && t < 1 && u > 0 && u < 1;
}

inline bool itu_lib_overlaps_segment_rect(vec2f segment_a, vec2f segment_b, vec2f rect_min, vec2f rect_max)
{
	vec2f a = vec2f{ rect_min.x, rect_min.y };	// d ---- c
	vec2f b = vec2f{ rect_max.x, rect_min.y };	// |      |
	vec2f c = vec2f{ rect_max.x, rect_max.y };	// |      |
	vec2f d = vec2f{ rect_min.x, rect_max.y };	// a ---- b

	// NOTE: an infine line is much faster to test (just need to check if all points of the rect are on the same side of the line)
	//       segments are tricky tho. Should probably add infinite lines and planes in the future

	// test needed:
	// 1. base case: is the segment intersecting any of the rect's edges?
	// 2. segment fully contained in rect: both segment's endpoints are inside the rect
	//    (we need to test both, otherwise we have a deadzone when the entire segment is inside but the tested point is on the rect's edge)
	// NOTE: tests are sorted from cheaper to most expensive, since we won't need to do any more tests after we get a positive
	//       (see https://en.cppreference.com/w/cpp/language/operator_logical.html, "short-circuit evaluation)
	return
		
		// extra test: if the segment is fully contained there will be no segment<>edge overlap,
		//             but both segment ends will be inside the rect (we can test either one
		itu_lib_overlaps_point_rect(segment_a, rect_min, rect_max) ||
		itu_lib_overlaps_point_rect(segment_b, rect_min, rect_max) ||
		// base tests: is the segment overlapping any of the rectangle's sides?
		itu_lib_overlaps_segment_segment(segment_a, segment_b, a, b) ||
		itu_lib_overlaps_segment_segment(segment_a, segment_b, b, c) ||
		itu_lib_overlaps_segment_segment(segment_a, segment_b, c, d) ||
		itu_lib_overlaps_segment_segment(segment_a, segment_b, d, a);
}

inline bool itu_lib_overlaps_circle_circle(vec2f circle_center_0, float circle_radius_0, vec2f circle_center_1, float circle_radius_1)
{
	float d_sq = length_sq(circle_center_0 - circle_center_1);
	float r_sum_sq = (circle_radius_0 + circle_radius_1) * (circle_radius_0 + circle_radius_1);

	// NOTE: checking if the sum of the radii is equal to distance is perfectly valid (and probably better for heavily physics-based games),
	//       BUT it creates contacts when objects are arranged in a perfect grid, which is annoying for grid-based games.
	//       Could be worth making this a configurable option (either runtime with a variable or compile-time with a define)
	return d_sq < r_sum_sq;
}

inline bool itu_lib_overlaps_circle_rect(vec2f circle_center, float circle_radius, vec2f rect_min, vec2f rect_max)
{
	vec2f a = vec2f{ rect_min.x, rect_min.y };	// d ---- c
	vec2f b = vec2f{ rect_max.x, rect_min.y };	// |      |
	vec2f c = vec2f{ rect_max.x, rect_max.y };	// |      |
	vec2f d = vec2f{ rect_min.x, rect_max.y };	// a ---- b

	// tests needed:
	// 1. base case: is any of the rect's edges overlapping the circle?
	// 2. circle fully contained in rect: we can just test the circle center against the rect itself
	// 3. rect fully contained in circle: all vertices of the rect will be inside the rect (we can test any one of them)
	// NOTE: tests are sorted from cheaper to most expensive, since we won't need to do any more tests after we get a positive
	//       (see https://en.cppreference.com/w/cpp/language/operator_logical.html, "short-circuit evaluation)
	return
		// 3. rect fully contained in circle
		itu_lib_overlaps_point_circle(a, circle_center, circle_radius) ||
		// 2. circle fully contained in rec
		itu_lib_overlaps_point_rect(circle_center, rect_min, rect_max) ||
		// base tests: is any of the rect's edges overlapping the circle?
		itu_lib_overlaps_segment_circle(a, b, circle_center, circle_radius) ||
		itu_lib_overlaps_segment_circle(b, c, circle_center, circle_radius) ||
		itu_lib_overlaps_segment_circle(c, d, circle_center, circle_radius) ||
		itu_lib_overlaps_segment_circle(d, a, circle_center, circle_radius);
}

inline bool itu_lib_overlaps_rect_rect(vec2f rect_min_0, vec2f rect_max_0, vec2f rect_min_1, vec2f rect_max_1)
{
	bool ret = false;

	// NOTE: checking for edges that have the exact same x or y coord is perfecly valid (and probably better for heavily physics-based games),
	//       BUT it creates contacts when objects are arranged in a perfect grid, which is annoying for grid-based games.
	//       Could be worth making this a configurable option (either runtime with a variable or compile-time with a define)
	return rect_min_0.y < rect_max_1.y && rect_max_0.y > rect_min_1.y &&
		   rect_min_0.x < rect_max_1.x && rect_max_0.x > rect_min_1.x;
}

bool itu_lib_overlaps_point_polygon(vec2f point, vec2f* polygon_vertices, int poligon_vertices_count)
{
	SDL_assert(polygon_vertices);

	// check that the point is on the left of all edges of the triangle
	// NOTE: there are better algorithms (O(log(N)) to do this, but we are limiting our polygons to small sizes, so it's fine
	for(int i = 0; i < poligon_vertices_count - 1; ++i)
	{
		vec2f a = polygon_vertices[i];
		vec2f b = polygon_vertices[(i + 1) % poligon_vertices_count];
		vec2f c = polygon_vertices[(i + 2) % poligon_vertices_count];

		vec2f ab = b - a;
		vec2f bc = c - b;
		if (cross(ab, point - a) < 0 || cross(bc, point - b) < 0)
			return false;
	}

	return true;
}

bool itu_lib_overlaps_segment_polygon(vec2f segment_a, vec2f segment_b, vec2f* polygon_vertices, int poligon_vertices_count)
{
	SDL_assert(polygon_vertices);

	for(int i = 0; i < poligon_vertices_count; ++i)
	{
		vec2f a = polygon_vertices[i];
		vec2f b = polygon_vertices[(i + 1) % poligon_vertices_count];
		if(itu_lib_overlaps_segment_segment(segment_a, segment_b, a, b))
			return true;
	}

	return itu_lib_overlaps_point_polygon(segment_a, polygon_vertices, poligon_vertices_count) ||
		   itu_lib_overlaps_point_polygon(segment_b, polygon_vertices, poligon_vertices_count);
}

bool itu_lib_overlaps_circle_polygon(vec2f circle_center, float circle_radius, vec2f* polygon_vertices, int poligon_vertices_count)
{
	// possible cases:
	// - circle center is inside polygon
	// - one of the polygon's vertices is inside the circle
	// - none of the above but still collision
	// - no collision

	if(itu_lib_overlaps_point_polygon(circle_center, polygon_vertices, poligon_vertices_count))
		return true;

	for(int i = 0; i < poligon_vertices_count; ++i)
	{
		vec2f a = polygon_vertices[i];
		vec2f b = polygon_vertices[(i + 1) % poligon_vertices_count];
		
		if(itu_lib_overlaps_point_circle(a, circle_center, circle_radius))
			return true;

		if(itu_lib_overlaps_segment_circle(a, b, circle_center, circle_radius))
			return true;
	}

	return false;
}

bool itu_lib_overlaps_rect_polygon(vec2f rect_min, vec2f rect_max, vec2f* polygon_vertices, int poligon_vertices_count)
{
	// possible cases:
	// - rect vertex is inside polygon
	// - polygon vertex is inside rect
	// - none of the above but still collision
	// - no collision

	if(itu_lib_overlaps_point_polygon(rect_min, polygon_vertices, poligon_vertices_count))
		return true;
	if(itu_lib_overlaps_point_polygon(rect_max, polygon_vertices, poligon_vertices_count))
		return true;
	if(itu_lib_overlaps_point_polygon({ rect_min.x, rect_max.y }, polygon_vertices, poligon_vertices_count))
		return true;
	if(itu_lib_overlaps_point_polygon({ rect_max.x, rect_max.y }, polygon_vertices, poligon_vertices_count))
		return true;

	for(int i = 0; i < poligon_vertices_count; ++i)
	{
		vec2f a = polygon_vertices[i];
		
		if(itu_lib_overlaps_point_rect(a, rect_min, rect_max))
			return true;
	}

	return false;
}

static vec2f gjk_support_polygon(vec2f dir, vec2f* vertices, int vertices_count)
{
	vec2f ret = vertices[0];
	float support_max = dot(dir, ret);

	for (int i = 1; i < vertices_count; ++i)
	{
		float support = dot(vertices[i], dir);
		if (support > support_max)
		{
			support_max = support;
			ret = vertices[i];
		}
	}

	return ret;
}

// NOTE: assumes polygons are convex AND counter0clockwise
bool itu_lib_overlaps_polygon_polygon(vec2f* polygon_0_vertices, int poligon_0_vertices_count, vec2f* polygon_1_vertices, int poligon_1_vertices_count, vec2f* out_simplex)
{
	SDL_assert(polygon_0_vertices);
	SDL_assert(polygon_1_vertices);

	// main GJK implementation
	// from https://www.youtube.com/watch?v=Qupqu1xe7Io (adapted for 2D)
	// NOTE: we need only 3 vertices, since the 2D simplex is a triangle
	// NOTE: most algorithms that perform separation of arbitrary polygons will need the last simplex found by GJK
	vec2f support_points[3] = { };
	vec2f closest_points1[3] = { };
	vec2f closest_points2[3] = { };
	int support_points_count = 0;

	// NOTE: choose appropriate first direction
	vec2f direction = VEC2F_UP;

	// NOTE: hardcoding the support function here because the purpose of this library is illustrative.
	//       the true power of GJK comes form the fact that the algorithms works exactly the same *disregarding the support function implementation*
	//       We could just pass arbitrary data and a type (ie, tagged union or similar) and we have an extrimely performant function that works for
	//       all shapes
	closest_points1[support_points_count] = gjk_support_polygon(direction, polygon_0_vertices, poligon_0_vertices_count);
	closest_points2[support_points_count] = gjk_support_polygon(-direction, polygon_1_vertices, poligon_1_vertices_count);
	support_points[support_points_count++] = closest_points1[support_points_count] - closest_points2[support_points_count];
	direction = -support_points[0];

	const int max_iter = 128;
	int i = 0;
	bool ret = false;

	for(i = 0; i < max_iter; ++i)
	{
		closest_points1[support_points_count] = gjk_support_polygon(direction, polygon_0_vertices, poligon_0_vertices_count);
		closest_points2[support_points_count] = gjk_support_polygon(-direction, polygon_1_vertices, poligon_1_vertices_count);
		vec2f a = closest_points1[support_points_count] - closest_points2[support_points_count];

		if(dot(a, direction) < 0)
			break;

		support_points[support_points_count++] = a;

		// do simplex
		{
			/* simplex configuration
			 * 
			 * 
			 * edge            a -- b
			 * 
			 * triangle        a -- b
			 *                  \   |
			 *                   \  |
			 *                    \ |
			 *                      c
			 */
			vec2f b = support_points[support_points_count-2];
			vec2f ab = b - a;
			vec2f a0 = -a;
			float sign_ab = dot(ab, a0);

			// edge
			if(support_points_count == 2)
			{
				if(sign_ab > 0)
				{
					// if the origin points in the same direction as B, we need to search perpendicular to the segment
					// (no need to check if we need to search in the direction of B, since we came from there)
					support_points[0] = b;
					support_points[1] = a;
					support_points_count = 2;
					direction = cross_triplet(ab, a0, ab);
				}
				else 
				{
					// if the origin points away from B, we need to search in the direction of the origin
					support_points[0] = a;
					support_points_count = 1;
					direction = a0;
				}
			}
			// triangle
			else if(support_points_count == 3)
			{
				vec2f c = support_points[support_points_count-3];
				vec2f ac = c - a;
				float sign_ac = dot(ac, a0);

				if(sign_ab > 0 && sign_ac > 0)
				{
					// inside the triangle
					ret = true;
					break;
				}
				else if(sign_ac > 0)
				{
					//case AC
					support_points[0] = c;
					support_points[1] = a;
					support_points_count = 2;
					direction = cross_triplet(ac, a0, ac);
					continue;
				}
				else
				{
					support_points[0] = b;
					support_points[1] = a;
					support_points_count = 2;
					direction = cross_triplet(ab, a0, ab);
					continue;
				}
				break;
			}
			else
			{
				// this should never happen in 2D
				SDL_Log("[GJK] inpossible case: support_points_count == %d\n", support_points_count);
				return false;
			}
		}
	}
	return ret;
}

#endif // ITU_LIB_COLLISIONS_IMPLEMENTATION