#pragma once

#include "crowd/types.h"

namespace crowd
{
    namespace orca
    {
        bool     linear_program1(const OrcaLine* lines, uint16_t line_index, float max_speed, Vec2 opt_velocity, bool direction_opt, Vec2* out_result);
        uint16_t linear_program2(const OrcaLine* lines, uint16_t line_count, float max_speed, Vec2 opt_velocity, bool direction_opt, Vec2* out_result);
        void     linear_program3(const OrcaLine* lines, uint16_t line_count, uint16_t num_obstacle_lines, uint16_t begin_line, float max_speed, Vec2* out_result, OrcaLine* proj_lines_scratch);
    } // namespace orca
} // namespace crowd
