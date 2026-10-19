#pragma once

#include "crowd/types.h"

namespace crowd
{
    namespace kdtree
    {
        void        build_agent_tree(CrowdContext* crowd_context);
        CrowdResult build_obstacle_tree(CrowdContext* crowd_context);
        CrowdResult build_circle_obstacle_tree(CrowdContext* crowd_context);
        void        compute_agent_neighbors(CrowdContext* crowd_context, uint16_t agent_id, float* range_sq);
        uint16_t    query_point_neighbors(const CrowdContext* crowd_context, Vec2 point, float range_sq, uint8_t nav_mask, AgentNeighbor* out_buf, uint16_t out_cap);
        uint16_t    raycast_agents(const CrowdContext* crowd_context, Vec2 start, Vec2 end, float radius, uint8_t nav_mask, AgentRaycastHit* out_buf, uint16_t out_cap);
        void        compute_obstacle_neighbors(CrowdContext* crowd_context, uint16_t agent_id, float range_sq);
        void        compute_circle_obstacle_neighbors(CrowdContext* crowd_context, uint16_t agent_id, float range_sq);
        uint16_t    query_point_circle_obstacles(const CrowdContext* crowd_context, Vec2 point, float range_sq, uint8_t nav_mask, CircleObstacleNeighbor* out_buf, uint16_t out_cap);
        uint16_t    query_point_circle_obstacles_colliding(const CrowdContext* crowd_context, Vec2 point, float query_r, uint8_t nav_mask, CircleObstacleNeighbor* out_buf, uint16_t out_cap);
        uint16_t    raycast_circle_obstacles(const CrowdContext* crowd_context, Vec2 start, Vec2 end, float radius, uint8_t nav_mask, CircleObstacleRaycastHit* out_buf, uint16_t out_cap);
    } // namespace kdtree
} // namespace crowd
