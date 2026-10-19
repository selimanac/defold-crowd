#pragma once

#include <cstdint>

// Position cache controls for benchmarking. Define on the compiler command line, never in source.
// CROWD_FORCE_POSITION_CACHE=1  : force cache on regardless of platform
// CROWD_DISABLE_POSITION_CACHE=1: force cache off regardless of platform
#if defined(CROWD_FORCE_POSITION_CACHE)
#define CROWD_USE_POSITION_CACHE
#elif defined(CROWD_DISABLE_POSITION_CACHE)
// cache explicitly off
#else
// Apple Silicon: large shared/cluster L2 keeps the full agent array warm.  All other platforms: 160 KB -> 20 KB reduces L1 pressure and L2 contention.
#if !(defined(__APPLE__) && defined(__aarch64__))
#define CROWD_USE_POSITION_CACHE
#endif
#endif

// Query timing controls.
// Set CROWD_ENABLE_TIMINGS=1 on the compiler command line to enable kdtree query stats.
// Default off: the runtime branches add measurable overhead to the neighbor query hot path.
// #define CROWD_ENABLE_TIMINGS 1
#ifndef CROWD_ENABLE_TIMINGS
#define CROWD_ENABLE_TIMINGS 0
#endif

// Set CROWD_ENABLE_PARALLEL_TIMINGS=1 to collect do_step_parallel phase timings.
// Kept separate from CROWD_ENABLE_TIMINGS because query stats are shared state
// and intentionally force the serial step path.
#ifndef CROWD_ENABLE_PARALLEL_TIMINGS
#define CROWD_ENABLE_PARALLEL_TIMINGS 0
#endif

// Set by CMake when ENABLE_TRACY=ON for Debug/RelWithDebInfo builds.
// Default off so release/native-extension builds never include Tracy by accident.
#ifndef CROWD_ENABLE_TRACY
#define CROWD_ENABLE_TRACY 0
#endif

// Dynamic compute batching keeps one executor per worker lane but lets each
// executor pull smaller compute ranges from a shared counter. Default off.
#ifndef CROWD_ENABLE_DYNAMIC_COMPUTE_BATCHES
#define CROWD_ENABLE_DYNAMIC_COMPUTE_BATCHES 0
#endif

#ifndef CROWD_DYNAMIC_COMPUTE_BATCH_MIN_SIZE
#define CROWD_DYNAMIC_COMPUTE_BATCH_MIN_SIZE 128U
#endif

#ifndef CROWD_DYNAMIC_COMPUTE_BATCH_MULTIPLIER
#define CROWD_DYNAMIC_COMPUTE_BATCH_MULTIPLIER 16U
#endif

#if CROWD_ENABLE_DYNAMIC_COMPUTE_BATCHES && (CROWD_DYNAMIC_COMPUTE_BATCH_MIN_SIZE < 1)
#error "CROWD_DYNAMIC_COMPUTE_BATCH_MIN_SIZE must be >= 1"
#endif

#if CROWD_ENABLE_DYNAMIC_COMPUTE_BATCHES && (CROWD_DYNAMIC_COMPUTE_BATCH_MULTIPLIER < 1)
#error "CROWD_DYNAMIC_COMPUTE_BATCH_MULTIPLIER must be >= 1"
#endif

namespace crowd
{
    static const float EPSILON = 0.00001f;
    static const float MATH_PI = 3.14159265358979323846f;
    static const float AVOIDANCE_WEIGHT_MAX = 1000000.0f;
    // Minimum agent count for do_step_parallel to dispatch jobs; below this
    // threshold the serial path is faster due to fixed job-management overhead.
    static const uint16_t PARALLEL_AGENT_THRESHOLD = 500U;

    // NAV_MASK_NONE (0x00) and NAV_MASK_ALL (0xFF) are the only built-in masks.
    // Bits 0–7 are free for the game to define (e.g. 1U<<0 = ground, 1U<<1 = air).
    // Combine with bitwise OR for multi-layer membership: (1U<<0)|(1U<<2).
    static const uint8_t NAV_MASK_NONE = 0U;
    static const uint8_t NAV_MASK_ALL = 0xFFU;

    // Agent state flags share one byte to keep Agent at its fixed 64-byte size.
    static const uint8_t  AGENT_FLAG_KINEMATIC = 1 << 0;        // 0000 0001
    static const uint8_t  AGENT_FLAG_AVOIDANCE_LOCKED = 1 << 1; // 0000 0010
    static const uint8_t  AGENT_FLAG_SQUAD_LEADER = 1 << 2;     // 0000 0100

    static const uint16_t AGENT_ID_NONE = 0xFFFFU;
    static const uint16_t OBSTACLE_ID_NONE = 0xFFFFU;

#if defined(__APPLE__) && defined(__aarch64__)
    static const uint8_t KDTREE_MAX_LEAF = 20U;
#else
    static const uint8_t KDTREE_MAX_LEAF = 32U;
#endif

    static const uint8_t OBSTACLE_FLAG_CONVEX = 1U << 0;
    static const uint8_t OBSTACLE_FLAG_INACTIVE = 1U << 1;
    static const uint8_t AGENT_FLAG_INACTIVE = 1U << 3; // 0000 1000

    static const uint8_t CIRCLE_OBS_FLAG_INACTIVE = 1U << 0;

} // namespace crowd
