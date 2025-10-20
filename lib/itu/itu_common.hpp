#ifndef ITU_COMMON_HPP
#define ITU_COMMON_HPP

#ifndef ITU_UNITY_BUILD
#include <SDL3/SDL_log.h>    // SDL_Log()
#include <SDL3/SDL_error.h>  // SDL_Error()
#include <SDL3/SDL_stdinc.h> // SDL_assert(), all math functions and macros
#endif

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

// casts a value type to another value type
// NOTE: `type` must be a type
// NOTE: `x` must be a value variable (not expression, not return value, just a variable)
#define value_cast(type, x) *(type*)(&(x))

// offset a pointer by an amount of bytes
#define pointer_offset(type, pointer, amount) ((type*)((unsigned char*)(pointer) + (amount)))

// indexes a void* array with the given size
#define pointer_index(pointer, i, elem_size) (((unsigned char*)(pointer) + (i)*(elem_size)))

// effectively checks if `var` is of the given type `T`
// To be more precise, checks if the compiler can safely cast `var` to `T`.
// Unless you're doing something very weird, if you use this with non-integral(ie, structs) value types this will work as intended
#define type_check_struct(T, var) { typedef void (*type_t)(T); type_t tmp__ = (type_t)0; if(0) tmp__(var); }

// trigonometric macros
#define PI_HALF 1.570796f // radians equivalent of 45 degrees
#define PI      3.141592f // radians equivalent of 180 degrees
#define TAU     6.283185f // radians equivalent of 360 degrees

#define RAD_2_DEG 57.295780f // constant that, multiplied with an angle in radians, returns the equivalent in degrees
#define DEG_2_RAD  0.017453f // constant that, multiplied with an angle in degrees, returns the equivalent in radians

// arbitrary threshold for floating point equality comparison (to avoid approximation errors)
// NOTE: this may be too big for certain game designs. Change this if you need
#define FLOAT_EPSILON 0.001f

#define FLOAT_MAX_VAL  2e64; // arbitrary floating point max value. Techically we can hold bigger numbers (https://en.wikipedia.org/wiki/Single-precision_floating-point_format), but precision will be abismal. For games, this should be more than enough
#define FLOAT_MIN_VAL -2e64; // arbitrary floating point min value. Techically we can hold bigger numbers (https://en.wikipedia.org/wiki/Single-precision_floating-point_format), but precision will be abismal. For games, this should be more than enough


// *******************************************************************
// data structures
// NOTE: this is the C++ version, so operator overloading party
// *******************************************************************

// wrapper for a pointer of type `T` (reminder that we intend to manipulate data referenced by this pointer using a stbds array)
#define stbds_arr(T) T*

// wrapper for a pointer of type `struct { TK* key, TV* value }` (reminder that we intend to manipulate data referenced by this pointer using a stbds hashmap)
// NOTE: `f the key is a string, use `stbds_sm` instead
#define stbds_hm(TK, TV) struct { TK key; TV value; }*

// wrapper for a pointer of type `struct { TK* key, TV* value }` (reminder that we intend to manipulate data referenced by this pointer using a stbds hashmap)
// NOTE: this version is for char* key values
#define stbds_sm(TK, TV) struct { TK key; TV value; }*




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
	ret.y = a.y * b.y;
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
	ret.x = a.x * (1 - t) + t * b.x;
	ret.y = a.y * (1 - t) + t * b.y;
	return ret;
}

inline float lerp(float a, float b, float t)
{
	float ret;
	ret = a * (1 - t) + t * b;
	return ret;
}

inline vec2f reflect(vec2f a, vec2f n)
{
	return a - n * (2*dot(a, n));
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

// *******************************************************************
// easing functions
// *******************************************************************

enum EasingFunction
{
	EASING_LINEAR,
	EASING_CONSTANT_0,
	EASING_CONSTANT_1,
	EASING_IN_QUAD,
	EASING_IN_CUBIC,
	EASING_IN_QUART,
	EASING_IN_QUINT,
	EASING_IN_EXPO,
	EASING_IN_ELASTIC,
	EASING_IN_BOUNCE,
	EASING_OUT_QUAD,
	EASING_OUT_CUBIC,
	EASING_OUT_QUART,
	EASING_OUT_QUINT,
	EASING_OUT_EXPO,
	EASING_OUT_ELASTIC,
	EASING_OUT_BOUNCE,

	EASING_MAX // fake item, useful as count of how many entries in the enum
};

// for UI
const char* easing_names[] =
{
	"EASING_LINEAR",
	"EASING_CONSTANT_0",
	"EASING_CONSTANT_1",
	"EASING_IN_QUAD",
	"EASING_IN_CUBIC",
	"EASING_IN_QUART",
	"EASING_IN_QUINT",
	"EASING_IN_EXPO",
	"EASING_IN_ELASTIC",
	"EASING_IN_BOUNCE",
	"EASING_OUT_QUAD",
	"EASING_OUT_CUBIC",
	"EASING_OUT_QUART",
	"EASING_OUT_QUINT",
	"EASING_OUT_EXPO",
	"EASING_OUT_ELASTIC",
	"EASING_OUT_BOUNCE",

	"EASING_INVALID"
};

// wrapper for no easing (it allow sus to have a nice common interface)
float fn_easing_linear(float t) { return t; }

// returns always 0 unless `t == 1`
float fn_easing_constant_0(float t) { return t == 1 ? 1 : 0; }

// returns always 1 unless `t == 0`
float fn_easing_constant_1(float t) { return t == 0 ? 0 : 1; }

float fn_easing_out_quad (float t) { return 1 - (1 - t) * (1 - t); }
float fn_easing_out_cubic(float t) { return 1 - SDL_powf(1 - t, 3); }
float fn_easing_out_quart(float t) { return 1 - SDL_powf(1 - t, 4); }
float fn_easing_out_quint(float t) { return 1 - SDL_powf(1 - t, 5); }
float fn_easing_out_expo (float t) { return t >= 1 ? 1 : 1 - SDL_powf(2, -10.0f * t); };
float fn_easing_out_elastic (float t)
{
	if(t <= 0) return 0;
	if(t >= 1) return 1;
	return SDL_powf(2.0f, -10.0f * t) * SDL_sinf((t * 10.0f - 0.75f) * (2.0f * PI) / 3.0f) + 1;
};
float fn_easing_out_bounce(float t)
{
	static const float n1 = 7.5625f;
	static const float d1_inv = 1.0f / 2.75f;
	if(t < d1_inv)        return n1 * t * t;
	if(t < 2.0f * d1_inv) return n1 * (t - 1.5f * d1_inv) * (t - 1.5f * d1_inv) + 0.75f;
	if(t < 2.5f * d1_inv) return n1 * (t - 2.25f * d1_inv) * (t - 2.25f * d1_inv) + 0.9375f;

	return n1 * (t - 2.625f * d1_inv) * (t - 2.625f * d1_inv) + 0.984375f;
}

float fn_easing_in_quad (float t) { return t * t; }
float fn_easing_in_cubic(float t) { return t * t * t; }
float fn_easing_in_quart(float t) { return t * t * t * t; }
float fn_easing_in_quint(float t) { return t * t * t * t * t; }
float fn_easing_in_expo (float t) { return t <= 0 ? 0 : SDL_powf(2, 10.0f * t - 10.0f); };
float fn_easing_in_elastic (float t)
{
	if(t <= 0) return 0;
	if(t >= 1) return 1;
	return -SDL_powf(2.0f, 10.0f * t - 10.0f) * SDL_sinf((t * 10.0f - 10.75f) * (2.0f * PI) / 3.0f);
};
float fn_easing_in_bounce(float t) { return 1 - fn_easing_out_bounce(1.0f - t); }

float easing(float t, EasingFunction fn)
{
	switch(fn)
	{
		case EASING_LINEAR		: return fn_easing_linear(t);
		case EASING_CONSTANT_0	: return fn_easing_constant_0(t);
		case EASING_CONSTANT_1	: return fn_easing_constant_1(t);
		case EASING_IN_QUAD		: return fn_easing_in_quad(t);
		case EASING_IN_CUBIC	: return fn_easing_in_cubic(t);
		case EASING_IN_QUART	: return fn_easing_in_quart(t);
		case EASING_IN_QUINT	: return fn_easing_in_quint(t);
		case EASING_IN_EXPO		: return fn_easing_in_expo(t);
		case EASING_IN_ELASTIC	: return fn_easing_in_elastic(t);
		case EASING_IN_BOUNCE	: return fn_easing_in_bounce(t);
		case EASING_OUT_QUAD	: return fn_easing_out_quad(t);
		case EASING_OUT_CUBIC	: return fn_easing_out_cubic(t);
		case EASING_OUT_QUART	: return fn_easing_out_quart(t);
		case EASING_OUT_QUINT	: return fn_easing_out_quint(t);
		case EASING_OUT_EXPO	: return fn_easing_out_expo(t);
		case EASING_OUT_ELASTIC	: return fn_easing_out_elastic(t);
		case EASING_OUT_BOUNCE	: return fn_easing_out_bounce(t);
	}

	// unknown easing, just return `t` and call it a day
	return t;
}


#endif // ITU_COMMON_HPP