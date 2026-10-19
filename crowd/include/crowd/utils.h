#pragma once

#include "crowd/types.h"
#include "crowd/const.h"

#include <cstdint>
#include <cstring>
#include <dmsdk/dlib/memory.h>

namespace crowd
{
    namespace utils
    {

        static inline void aligned_free(void* ptr)
        {
            if (ptr)
            {
                dmMemory::AlignedFree(ptr);
            }
        }

        static inline float sanitize_avoidance_weight(float weight)
        {
            uint32_t bits;
            memcpy(&bits, &weight, sizeof(bits));
            const uint32_t exp = (bits >> 23U) & 0xFFU;
            const uint32_t mantissa = bits & 0x7FFFFFU;
            const bool     neg = (bits >> 31U) != 0U;

            if (exp == 0xFFU)
            {
                if (mantissa != 0U || neg)
                    return EPSILON;
                return AVOIDANCE_WEIGHT_MAX;
            }
            if (neg || weight <= 0.0f)
                return EPSILON;
            if (weight > AVOIDANCE_WEIGHT_MAX)
                return AVOIDANCE_WEIGHT_MAX;
            if (weight < EPSILON)
                return EPSILON;
            return weight;
        }

        static inline void restore_obstacle_source_records(CrowdContext* ctx, uint16_t source_count)
        {
            if (!ctx || source_count > ctx->obstacles.Size())
            {
                return;
            }

            const uint16_t current_count = (uint16_t)ctx->obstacles.Size();
            for (uint16_t i = current_count; i > source_count; --i)
            {
                const Obstacle& split = ctx->obstacles[i - 1U];
                ctx->obstacles[split.prev_obstacle_id].next_obstacle_id = split.next_obstacle_id;
                ctx->obstacles[split.next_obstacle_id].prev_obstacle_id = split.prev_obstacle_id;
            }

            ctx->obstacles.SetSize(source_count);
        }
    } // namespace utils
} // namespace crowd
