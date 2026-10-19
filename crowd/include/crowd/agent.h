#pragma once

#include "crowd/types.h"

namespace crowd
{
    namespace agent
    {
        void  compute_neighbors(CrowdContext* crowd_context, uint16_t agent_id);
        void  compute_new_velocity(CrowdContext* crowd_context, uint16_t agent_id);
        void  compute_new_velocity_for_worker(CrowdContext* crowd_context, uint16_t agent_id, uint16_t worker_id);
        void  update(CrowdContext* crowd_context, uint16_t agent_id, float time_step);
        float compute_responsibility(const Agent* self_agent, const Agent* other_agent);
    } // namespace agent
} // namespace crowd
