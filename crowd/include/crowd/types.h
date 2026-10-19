#pragma once

#include "crowd/const.h"
#include <cstdint>
#include <dmsdk/dlib/align.h>
#include "dmsdk/dlib/array.h"
#if CROWD_ENABLE_DYNAMIC_COMPUTE_BATCHES
#include <dmsdk/dlib/atomic.h>
#endif

namespace crowd
{
    enum CrowdResult
    {
        CROWD_RESULT_OK = 0,
        CROWD_RESULT_INVALID_ARGUMENT = 1,
        CROWD_RESULT_OUT_OF_MEMORY = 2,
        CROWD_RESULT_OBSTACLE_CAPACITY_EXHAUSTED = 3,
        CROWD_RESULT_OBSTACLE_SCRATCH_EXHAUSTED = 4,
        CROWD_RESULT_OBSTACLE_DEGENERATE = 5
    };

    struct Vec2
    {
        float x;
        float y;

        Vec2()
            : x(0.0f)
            , y(0.0f)
        {
        }

        Vec2(float x_, float y_)
            : x(x_)
            , y(y_)
        {
        }
    };

    struct OrcaLine
    {
        Vec2 point;
        Vec2 direction;

        OrcaLine()
            : point(0.0f, 0.0f)
            , direction(0.0f, 0.0f)
        {
        }
    };

    struct AgentNeighbor
    {
        float    distance_sq;
        uint16_t agent_id;
        uint8_t  _pad[2];
    };

    struct ObstacleNeighbor
    {
        float    distance_sq;
        uint16_t obstacle_id;
        uint8_t  _pad[2];
    };

    struct AgentTreeNode
    {
        uint16_t begin, end, left, right;
        float    min_x, max_x, min_y, max_y;
    };

    struct ObstacleTreeNode
    {
        uint16_t obstacle_id;
        uint16_t left, right;
        uint8_t  _pad[2];
    };

    struct DM_ALIGNED(16) CircleObstacle
    {
        Vec2     center;   // 8 bytes
        float    radius;   // 4 bytes
        uint16_t id;       // 2 bytes
        uint8_t  flags;    // 1 byte (CIRCLE_OBS_FLAG_INACTIVE)
        uint8_t  nav_mask; // 1 byte (agent/nav-layer filter mask)
    };

    struct CircleObstacleTreeNode
    {
        uint16_t obs_id; // circle obstacle at this node
        uint16_t left;   // left child (OBSTACLE_ID_NONE = leaf)
        uint16_t right;  // right child
        uint8_t  _pad[2];
    };

    struct CircleObstacleNeighbor
    {
        float    center_dist_sq; // abs_sq(obs.center - agent.position), for sort ordering
        uint16_t obs_id;
        uint8_t  _pad[2];
    };

    struct AgentRaycastHit
    {
        float    t; // normalized [0,1] position along start -> end, clamped
        uint16_t agent_id;
        uint8_t  _pad[2];
    };

    struct CircleObstacleRaycastHit
    {
        float    t; // normalized [0,1] position along start -> end, clamped
        uint16_t obs_id;
        uint8_t  _pad[2];
    };

#if CROWD_ENABLE_TIMINGS
    struct QueryStats
    {
        uint32_t agent_tree_nodes_visited;
        uint32_t agent_leaf_candidate_tests;
        uint32_t agent_neighbor_accepts;
        uint32_t agent_neighbor_count_sum;
        uint16_t agent_neighbor_count_max;
        uint16_t agent_query_count;
        uint32_t circle_tree_nodes_visited;
        uint32_t circle_candidate_tests;
        uint32_t circle_neighbor_accepts;
        uint32_t circle_neighbor_count_sum;
        uint16_t circle_neighbor_count_max;
        uint16_t circle_query_count;
    };
#endif

#if CROWD_ENABLE_PARALLEL_TIMINGS
    struct ParallelStepTimings
    {
        double   tree_ms;
        double   clear_ms;
        double   compute_ms;
        double   update_ms;
        double   total_ms;
        double   compute_imbalance; // max_worker_compute_ms / min_worker_compute_ms
        float    avg_neighbors;     // average neighbor count across all active agents
        uint16_t max_neighbors;     // highest neighbor count any active agent had
        uint16_t max_orca_lines;    // highest ORCA line count any active agent had
        uint16_t active_count;      // total active agents processed in compute phase
        uint8_t  _pad[2];
    };
#endif

    struct AgentJobContext; // forward declaration — defined after CrowdContext

    struct DM_ALIGNED(32) Obstacle
    {
        // 1. Geometry Vectors (16 bytes)
        Vec2 point;     // 8 bytes (2 x 4-byte floats)
        Vec2 direction; // 8 bytes (2 x 4-byte floats)

        // 2. Topology Array Indices (6 bytes)
        uint16_t id;               // 2 bytes
        uint16_t next_obstacle_id; // 2 bytes (Uses OBSTACLE_ID_NONE if open-ended)
        uint16_t prev_obstacle_id; // 2 bytes (Uses OBSTACLE_ID_NONE if open-ended)

        // 3. Flags & Navigation Layers (2 bytes)
        uint8_t flags;    // 1 byte (convex/inactive flags)
        uint8_t nav_mask; // 1 byte (agent/nav-layer filter mask)

        // 4. Cache Alignment Padding (8 bytes)
        uint8_t padding[8]; // 8 bytes of pure padding to hit exactly 32 bytes
    };

    struct DM_ALIGNED(64) Agent
    {
        // 1. Structural Vectors (32 bytes total)
        Vec2 position;
        Vec2 velocity;
        Vec2 pref_velocity;
        Vec2 new_velocity;

        // 2. Configuration Floats (24 bytes total)
        float radius;
        float max_speed;
        float neighbor_distance;
        float time_horizon;
        float time_horizon_obstacle;
        float avoidance_weight;

        // 3. Identification & Logic Caps (4 bytes total)
        uint16_t id;
        uint8_t  max_neighbors;
        uint8_t  avoidance_priority;

        // 4. Gameplay Flags & System Alignment (4 bytes total)
        uint8_t  flags;
        uint8_t  nav_mask;
        uint16_t squad_id; // not used
    }; // EXACTLY 64 BYTES

    struct CrowdContext
    {
        dmArray<Agent>    agents;
        dmArray<Obstacle> obstacles;

        void*             raw_agents_allocation;
        void*             raw_obstacles_allocation;

        uint16_t          max_agents;
        uint16_t          max_obstacles;
        uint16_t          active_agent_count;
        float             max_agent_radius; // max radius among active agents, cached in build_agent_tree()

        float             global_time;
        float             time_step;

#if CROWD_ENABLE_TIMINGS
        QueryStats query_stats;
#endif
#if CROWD_ENABLE_PARALLEL_TIMINGS
        ParallelStepTimings parallel_timings;
#endif

        // Capacities
        uint8_t  max_workers;
        uint8_t  max_neighbors;          // global agent-neighbor scratch cap
        uint16_t max_obstacle_neighbors; // global obstacle-neighbor scratch cap
        uint16_t max_orca_line_cap;      // max_neighbors + max_obstacle_neighbors
        uint16_t proj_scratch_stride;    // per-worker proj_lines_scratch stride in OrcaLine units (≥ max_orca_line_cap, aligned to 64-byte cache-line)
        uint16_t obstacle_tree_root;
        uint16_t source_obstacle_count;

        // KdTree arrays (allocated in create_context)
        AgentTreeNode*    agent_tree_nodes;    // size: 2 * max_agents
        ObstacleTreeNode* obstacle_tree_nodes; // size: max_obstacles
        uint16_t*         agent_sort_ids;      // size: max_agents
        uint16_t*         obstacle_sort_ids;   // size: max_obstacles — BSP build scratch
        AgentJobContext*  job_contexts;        // size: max_workers

        // Raw tracking for KdTree allocations
        void* raw_agent_tree_nodes_allocation;
        void* raw_obstacle_tree_nodes_allocation;
        void* raw_agent_sort_ids_allocation;
        void* raw_obstacle_sort_ids_allocation;
        void* raw_job_contexts_allocation;

        // Scratch buffers (allocated in create_context)
        AgentNeighbor*    agent_neighbors;          // size: max_agents * max_neighbors
        ObstacleNeighbor* obstacle_neighbors;       // size: max_agents * max_obstacle_neighbors
        OrcaLine*         orca_lines;               // size: max_agents * max_orca_line_cap
        OrcaLine*         proj_lines_scratch;       // size: max_workers * max_orca_line_cap
        uint16_t*         agent_neighbor_counts;    // size: max_agents
        uint16_t*         obstacle_neighbor_counts; // size: max_agents
        uint16_t*         orca_line_counts;         // size: max_agents

        // Free list for slot reuse (remove_agent pushes, add_agent pops)
        uint16_t* free_slots;      // pre-allocated stack of reusable agent slot indices
        uint16_t  free_slot_count; // current depth of free list
        void*     raw_free_slots_allocation;

        // Raw tracking for scratch allocations
        void* raw_agent_neighbors_allocation;
        void* raw_obstacle_neighbors_allocation;
        void* raw_orca_lines_allocation;
        void* raw_proj_lines_scratch_allocation;
        void* raw_neighbor_counts_allocation; // covers all three count arrays in one block

#ifdef CROWD_USE_POSITION_CACHE
        Vec2* agent_positions;
        void* raw_agent_positions_allocation;
#endif

        // Circle obstacle system
        dmArray<CircleObstacle> circle_obstacles;
        void*                   raw_circle_obstacles_allocation;
        CircleObstacleTreeNode* circle_obstacle_tree_nodes;
        uint16_t*               circle_obstacle_sort_ids; // build scratch [max_circle_obstacles]
        uint16_t                circle_obstacle_tree_root;
        uint16_t                max_circle_obstacles;
        float                   max_circle_obstacle_radius; // cached in process_circle_obstacles()
        CircleObstacleNeighbor* circle_obstacle_neighbors;  // [max_agents][max_circle_obstacles]
        uint16_t*               circle_obstacle_neighbor_counts;

        void*                   raw_circle_obstacle_tree_nodes_allocation;
        void*                   raw_circle_obstacle_sort_ids_allocation;
        void*                   raw_circle_obstacle_neighbors_allocation;
        void*                   raw_circle_obstacle_neighbor_counts_allocation;
    };

    struct AgentJobContext
    {
        CrowdContext* crowd_context;
        uint16_t      begin;
        uint16_t      end;
        uint16_t      worker_id;
        uint8_t       _pad[2];
#if CROWD_ENABLE_DYNAMIC_COMPUTE_BATCHES
        int32_atomic_t* next_compute_begin;
        uint16_t        compute_count;
        uint16_t        compute_batch_size;
#endif
#if CROWD_ENABLE_PARALLEL_TIMINGS
        double   worker_compute_ms; // wall time for this worker's compute chunk
        uint16_t active_count;      // agents processed (not skipped)
        uint32_t neighbor_sum;      // sum of neighbor counts across active agents
        uint16_t neighbor_max;      // max neighbor count in this chunk
        uint32_t orca_line_sum;     // sum of ORCA line counts
        uint16_t orca_line_max;     // max ORCA line count in this chunk
#endif
    };

    struct CrowdMemoryUsage
    {
        uint16_t max_agents;
        uint16_t effective_agent_slots;
        uint16_t max_obstacles;
        uint16_t max_circle_obstacles;
        uint8_t  max_neighbors;
        uint16_t max_obstacle_neighbors;
        uint16_t max_orca_line_cap;
        uint8_t  max_workers;
        uint16_t proj_scratch_stride;

        uint64_t context_bytes;
        uint64_t agents_bytes;
        uint64_t obstacles_bytes;
        uint64_t agent_tree_nodes_bytes;
        uint64_t obstacle_tree_nodes_bytes;
        uint64_t agent_sort_ids_bytes;
        uint64_t obstacle_sort_ids_bytes;
        uint64_t job_contexts_bytes;
        uint64_t agent_neighbors_bytes;
        uint64_t obstacle_neighbors_bytes;
        uint64_t circle_obstacles_bytes;
        uint64_t circle_obstacle_tree_nodes_bytes;
        uint64_t circle_obstacle_sort_ids_bytes;
        uint64_t circle_obstacle_neighbors_bytes;
        uint64_t circle_obstacle_neighbor_counts_bytes;
        uint64_t orca_lines_bytes;
        uint64_t proj_lines_scratch_bytes;
        uint64_t neighbor_counts_bytes;
        uint64_t free_slots_bytes;
        uint64_t agent_positions_bytes;
        uint64_t total_bytes;
    };

    static_assert(sizeof(Vec2) == 8U, "Vec2 must be 8 bytes");
    static_assert(sizeof(OrcaLine) == 16U, "OrcaLine must be 16 bytes");
    static_assert(sizeof(AgentNeighbor) == 8U, "AgentNeighbor must be 8 bytes");
    static_assert(sizeof(ObstacleNeighbor) == 8U, "ObstacleNeighbor must be 8 bytes");
    static_assert(sizeof(Agent) == 64U, "Agent must be 64 bytes");
    static_assert(sizeof(Obstacle) == 32U, "Obstacle must be 32 bytes");
    static_assert(sizeof(AgentTreeNode) == 24U, "AgentTreeNode must be 24 bytes");
    static_assert(sizeof(ObstacleTreeNode) == 8U, "ObstacleTreeNode must be 8 bytes");
    static_assert(sizeof(CircleObstacle) == 16U, "CircleObstacle must be 16 bytes");
    static_assert(sizeof(CircleObstacleTreeNode) == 8U, "CircleObstacleTreeNode must be 8 bytes");
    static_assert(sizeof(CircleObstacleNeighbor) == 8U, "CircleObstacleNeighbor must be 8 bytes");
    static_assert(sizeof(AgentRaycastHit) == 8U, "AgentRaycastHit must be 8 bytes");
    static_assert(sizeof(CircleObstacleRaycastHit) == 8U, "CircleObstacleRaycastHit must be 8 bytes");

} // namespace crowd
