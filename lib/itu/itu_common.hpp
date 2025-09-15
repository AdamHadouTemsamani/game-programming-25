#ifndef ITU_COMMON_HPP
#define ITU_COMMON_HPP

#include <SDL3/SDL_log.h>    // SDL_Log()
#include <SDL3/SDL_error.h>  // SDL_Error()
#include <SDL3/SDL_stdinc.h> // SDL_assert(), all math functions and macros

// *******************************************************************
// common macros
// *******************************************************************

// tests the return error of SDL APIs, logging the SDL error in case of failue
#define VALIDATE(expression) if(!(expression)) { SDL_Log("%s\n", SDL_GetError()); }

// tests the return error of SDL APIs, logging the SDL error in case of failue and stopping the program
#define VALIDATE_PANIC(expression) if(!(expression)) { SDL_Log("%s\n", SDL_GetError()); SDL_assert(false); }

#define NANOS(x)   (x)                // converts nanoseconds into nanoseconds
#define MICROS(x)  (NANOS(x) * 1000)  // converts microseconds into nanoseconds
#define MILLIS(x)  (MICROS(x) * 1000) // converts milliseconds into nanoseconds
#define SECONDS(x) (MILLIS(x) * 1000) // converts seconds into nanoseconds

#define NS_TO_MILLIS(x)  ((float)(x)/(float)1000000)    // converts nanoseconds to milliseconds (in floating point precision)
#define NS_TO_SECONDS(x) ((float)(x)/(float)1000000000) // converts nanoseconds to seconds (in floating point precision)

#define KB(x) ((x) * 1000ll)   // converts kilobytes to bytes
#define MB(x) (KB(x) * 1000ll) // converts megabytes to bytes
#define GB(x) (MB(x) * 1000ll) // converts gigabytes to bytes
#define TB(x) (GB(x) * 1000ll) // converts terabytes to bytes

// returns the number of elements in an array
// NOTE: this is one of the rare cases where arrays variables and pointer variables behave differently!
//       - sizeof(int[3]) == sizeof(int) * 3
//       - sizeof(int*)   == 8 (or 4 if you are targeting a 32bit build)
#define array_size(x) (sizeof(x) / sizeof((x)[0]))

// trigonometric macros
#define PI_HALF 1.570796f // radians equivalent of 45 degrees
#define PI      3.141592f // radians equivalent of 180 degrees
#define TAU     6.283185f // radians equivalent of 360 degrees

#define RAD_2_DEG 57.295780f // constant that, multiplied with an angle in radians, returns the equivalent in degrees
#define DEG_2_RAD  0.017453f // constant that, multiplied with an angle in degrees, returns the equivalent in radians

// arbitrary threshold for floating point equality comparison (to avoid approximation errors)
//
// NOTE: this may be too big for certain game designs. Change this if you need
#define FLOAT_EPSILON 0.001f

// *******************************************************************
// vector math
// NOTE: this is the C++ version, so operator overloading party
// *******************************************************************

struct vec2f
{
	float x;
	float y;

	vec2f operator+(vec2f b)
	{
		return { this->x + b.x, this->y + b.y };
	}

	vec2f operator+(float f)
	{
		return { this->x + f, this->y + f };
	}

	vec2f operator-(vec2f b)
	{
		return{ this->x - b.x, this->y - b.y };
	}

	vec2f operator-(float f)
	{
		return{ this->x - f, this->y - f };
	}

	vec2f operator*(float v)
	{
		return{ this->x * v, this->y * v };
	}

	vec2f operator/(float v)
	{
		return { this->x / v, this->y / v };
	}

	vec2f operator-()
	{
		return{ -this->x, -this->y };
	}

	vec2f operator+=(vec2f a)
	{
		this->x += a.x;
		this->y += a.y;
		return *this;
	}

	vec2f operator-=(vec2f a)
	{
		this->x -= a.x;
		this->y -= a.y;
		return *this;
	}
};

#define VEC2F_ZERO  vec2f {  0.0f,  0.0f }
#define VEC2F_ONE   vec2f {  1.0f,  1.0f }
#define VEC2F_UP    vec2f {  0.0f,  1.0f }
#define VEC2F_DOWN  vec2f {  0.0f, -1.0f }
#define VEC2F_LEFT  vec2f { -1.0f,  0.0f }
#define VEC2F_RIGHT vec2f {  1.0f,  0.0f }

inline vec2f mul_element_wise(vec2f a, vec2f b)
{
	vec2f ret;
	ret.x = a.x * b.x;
	ret.y = a.x * b.y;
	return ret;
}

inline float dot(vec2f a, vec2f b)
{
	return a.x * b.x + a.y * b.y;
}

// NOTE cross product is not really defined in 2D, this is just a hack to check some useful properties
// - clockwise/conter-clockwise rotation
// - position left/right of a line
// (mathematically, we are simplifying the formula of a 3D cross product where
// - both z components of the inputs are 0
// - both x and y components of the output are 0
// see https://allenchou.net/2013/07/cross-product-of-2d-vectors/ for more info
inline float cross(vec2f a, vec2f b)
{
	return a.x * b.y - a.y * b.x;
}

// optimized 2D version of the triple product
inline vec2f cross_triplet(vec2f a, vec2f b, vec2f c)
{
	float z = cross(a, b);
	vec2f ret;
	ret.x = -z * c.y;
	ret.y =  z * c.x;
	return ret;
}

inline float distance(vec2f a, vec2f b)
{
	float dx = a.x - b.x;
	float dy = a.y - b.y;
	return SDL_sqrtf(dx*dx + dy*dy);
}

inline float distance_sq(vec2f a, vec2f b)
{
	float dx = a.x - b.x;
	float dy = a.y - b.y;
	return dx*dx + dy*dy;
}

inline float length(vec2f a)
{
	return SDL_sqrt(a.x*a.x + a.y*a.y);
}

inline float length_sq(vec2f a)
{
	return a.x*a.x + a.y*a.y;
}

inline vec2f normalize(vec2f a)
{
	float l =  length(a);
	if(l == 0)
		return a;

	return a * ( 1.0f / l);
}

// NOTE: this comparison checks if both componenents differ by less than FLOAT_EPSILON
inline bool check_equality(vec2f a, vec2f b)
{
	// // posiible alternate implementation (nor sure which one is best TBH)
	// return SDL_fabs(length_sq(a - b)) < FLOAT_EPSILON;

	return SDL_fabsf(a.x - b.x) < FLOAT_EPSILON && SDL_fabsf(a.y - b.y) < FLOAT_EPSILON;
}

inline vec2f clamp(vec2f a, float len)
{
	vec2f ret = a;
	float len_sq = len * len;
	float len_curr_sq = length_sq(a);
	if(len_sq < len_curr_sq)
	{
		float shirk_factor = len / length(a);
		ret = ret * shirk_factor;
	}
	return ret;

	// vec2f ret = a;
	// float len_curr = length(a);
	// if(len < len_curr)
	// {
	// 	float shirk_factor = len / len_curr;
	// 	ret = ret * shirk_factor;
	// }
	// return ret;
}

inline vec2f rotate(vec2f a, float angle)
{
	float sin_a = SDL_sinf(angle);
	float cos_a = SDL_cosf(angle);
	vec2f ret;
	ret.x = a.x * cos_a - a.y * sin_a;
	ret.y = a.x * sin_a + a.y * cos_a;
	return ret;
}

inline vec2f lerp(vec2f a, vec2f b, float t)
{
	vec2f ret;
	ret.x = a.x * t + (1 - t) * b.x;
	ret.y = a.y * t + (1 - t) * b.y;
	return ret;
}

// *******************************************************************
// colors
// NOTE: this is the C++ version, so operator overloading party
// *******************************************************************

struct color
{
	float r;
	float g;
	float b;
	float a;

	color operator*(color b)
	{
		return { this->r * b.r, this->g * b.g, this->b * b.b };
	}
};

#define COLOR_RED    color { 1.0f, 0.0f, 0.0f, 1.0f }
#define COLOR_GREEN  color { 0.0f, 1.0f, 0.0f, 1.0f }
#define COLOR_BLUE   color { 0.0f, 0.0f, 1.0f, 1.0f }
#define COLOR_YELLOW color { 1.0f, 1.0f, 0.0f, 1.0f }
#define COLOR_BLACK  color { 0.0f, 0.0f, 0.0f, 1.0f }
#define COLOR_DARK   color { 0.1f, 0.1f, 0.1f, 1.0f }
#define COLOR_WHITE  color { 1.0f, 1.0f, 1.0f, 1.0f }

#define COLOR_TRANSPARENT_WHITE color { 1.0f, 1.0f, 1.0f, 0.0f }
#define COLOR_TRANSPARENT_BLACK color { 0.0f, 0.0f, 0.0f, 0.0f }

inline color color_saturate(color c)
{
	return
	{
		SDL_clamp(c.r, 0.0f, 1.0f),
		SDL_clamp(c.g, 0.0f, 1.0f),
		SDL_clamp(c.b, 0.0f, 1.0f),
		SDL_clamp(c.a, 0.0f, 1.0f)
	};
}

#endif // ITU_COMMON_HPP