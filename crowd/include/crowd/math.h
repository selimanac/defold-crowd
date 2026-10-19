#pragma once

#include "const.h"
#include "types.h"
#include <cmath>

namespace crowd
{
    namespace math
    {

        static inline Vec2 subtract(const Vec2& a, const Vec2& b)
        {
            return Vec2(a.x - b.x, a.y - b.y);
        }

        static inline float abs(const Vec2& v)
        {
            return sqrtf(v.x * v.x + v.y * v.y);
        }

        static inline float abs_sq(const Vec2& v)
        {
            return v.x * v.x + v.y * v.y;
        }

        static inline Vec2 add(const Vec2& a, const Vec2& b)
        {
            return Vec2(a.x + b.x, a.y + b.y);
        }

        static inline void add_in_place(Vec2& a, const Vec2& b)
        {
            a.x += b.x;
            a.y += b.y;
        }

        static inline Vec2 multiply(const Vec2& v, float s)
        {
            return Vec2(v.x * s, v.y * s);
        }

        static inline void multiply_in_place(Vec2& v, float s)
        {
            v.x *= s;
            v.y *= s;
        }

        static inline Vec2 divide(const Vec2& v, float s)
        {
            const float invScalar = 1.0f / s;
            return Vec2(v.x * invScalar, v.y * invScalar);
        }

        static inline Vec2 negate(const Vec2& v)
        {
            return Vec2(-v.x, -v.y);
        }

        static inline float distance(const Vec2& a, const Vec2& b)
        {
            return abs(subtract(b, a));
        }

        // Safe normalization for inputs that may be near zero length.
        static inline Vec2 normalize_safe(const Vec2& v)
        {
            const float magSq = v.x * v.x + v.y * v.y;

            if (magSq < (EPSILON * EPSILON))
            {
                return Vec2(0.0f, 0.0f);
            }

            const float invMag = 1.0f / sqrtf(magSq);

            return Vec2(v.x * invMag, v.y * invMag);
        }

        // RVO2-style normalization for callers that already require non-zero input.
        static inline Vec2 normalize(const Vec2& v)
        {
            return divide(v, abs(v));
        }

        static inline float det(const Vec2& a, const Vec2& b)
        {
            return a.x * b.y - a.y * b.x;
        }

        static inline float dot(const Vec2& a, const Vec2& b)
        {
            return a.x * b.x + a.y * b.y;
        }

        static inline float cross(const Vec2& a, const Vec2& b)
        {
            return a.x * b.y - a.y * b.x;
        }

        static inline float left_of(const Vec2& vector1, const Vec2& vector2, const Vec2& vector3)
        {
            return det(subtract(vector1, vector3), subtract(vector2, vector1));
        }

        static inline void subtract_in_place(Vec2& a, const Vec2& b)
        {
            a.x -= b.x;
            a.y -= b.y;
        }

        static inline void divide_in_place(Vec2& v, float scalar)
        {
            const float inv = 1.0f / scalar;
            v.x *= inv;
            v.y *= inv;
        }
    } // namespace math
} // namespace crowd
