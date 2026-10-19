#pragma once

#include "crowd/types.h"

#ifdef CROWD_HAS_JOBSYSTEM
#include <dmsdk/dlib/jobsystem.h>
#endif

namespace crowd
{
    const char* result_to_string(CrowdResult result);

    /**
     * @brief Create a fixed-capacity crowd simulation context.
     *
     * Allocates all backing memory — agents, obstacles, KdTree node arrays,
     * job contexts, and per-agent scratch buffers — in one shot. No further
     * setup call is required before adding agents or obstacles.
     *
     * @param max_agents             Maximum number of agents (capacity, not initial count)
     * @param max_obstacle_vertices  Number of source polygon vertices across all obstacles
     *                               (maximum: (65534 - max_neighbors) / 4). Internally
     *                               multiplied by 4 to accommodate BSP split records and
     *                               obstacle-neighbor scratch. Pass 0 to disable obstacle
     *                               support for this context and skip obstacle allocations.
     * @param max_neighbors          Global scratch cap — the maximum neighbor slots any
     *                               agent will use. Mirrors RVO2's maxNeighbors default.
     * @param max_workers            Total simulation lanes, including the calling thread (>= 1)
     * @return New context on success, NULL on invalid arguments or allocation failure
     */
    CrowdContext* create_context(uint16_t max_agents, uint16_t max_obstacle_vertices, uint16_t max_circle_obstacles, uint8_t max_neighbors, uint8_t max_workers);

    /**
     * @brief Calculate fixed-capacity memory reserved by a context with the
     * given creation parameters. Reports requested allocation bytes only,
     * excluding allocator bookkeeping and hidden alignment overhead.
     */
    CrowdResult calculate_memory_usage(uint16_t          max_agents,
                                       uint16_t          max_obstacle_vertices,
                                       uint16_t          max_circle_obstacles,
                                       uint8_t           max_neighbors,
                                       uint8_t           max_workers,
                                       CrowdMemoryUsage* out_usage);

    /**
     * @brief Report fixed-capacity memory reserved by an existing context.
     * Reports requested allocation bytes only, excluding allocator bookkeeping
     * and hidden alignment overhead.
     */
    CrowdResult get_memory_usage(const CrowdContext* crowd_context, CrowdMemoryUsage* out_usage);

    /**
     * @brief Destroy a crowd simulation context and release all memory.
     * Passing NULL is allowed and has no effect.
     */
    void destroy_context(CrowdContext* crowd_context);

    /**
     * @brief Add an agent. Works at any time up to max_agents capacity.
     * Returns AGENT_ID_NONE on overflow.
     */
    uint16_t add_agent(CrowdContext* crowd_context, Vec2 position, float radius, float max_speed, float neighbor_distance, float time_horizon, float time_horizon_obstacle, uint8_t max_neighbors);

    /**
     * @brief Remove an agent by marking it inactive (AGENT_FLAG_INACTIVE).
     * Clears velocity/preferred velocity, clears neighbor scratch, and pushes
     * the slot onto the free list for reuse. Returns false for NULL, out-of-range,
     * or already inactive agents.
     */
    bool remove_agent(CrowdContext* crowd_context, uint16_t agent_id);

    /**
     * @brief Add a polygon obstacle (closed vertex ring). Works at any time.
     * Returns the first obstacle record index, or OBSTACLE_ID_NONE on failure.
     * Call process_obstacles() afterwards to rebuild the obstacle KdTree.
     */
    uint16_t add_obstacle(CrowdContext* crowd_context, const Vec2* vertices, uint16_t vertex_count);

    /**
     * @brief Remove a source obstacle by marking its polygon ring inactive.
     * Split/internal BSP records are not valid public obstacle ids.
     * Call process_obstacles() afterwards to rebuild the obstacle KdTree.
     */
    bool remove_obstacle(CrowdContext* crowd_context, uint16_t obstacle_id);

    /**
     * @brief Build the static obstacle KdTree. Call once after adding obstacles,
     * and again any time obstacles are added or removed. Mirrors RVO2's
     * processObstacles().
     *
     * Returns CROWD_RESULT_OK on success. On failure the tree root is set to
     * OBSTACLE_ID_NONE, any partial split records are rolled back, and a
     * specific CrowdResult code describes the reason.
     */
    CrowdResult process_obstacles(CrowdContext* crowd_context);

    /**
     * @brief Add a circular obstacle defined by center position and radius.
     * Returns the obstacle id on success, or OBSTACLE_ID_NONE on failure.
     * Call process_circle_obstacles() afterwards to rebuild the KD-tree.
     */
    uint16_t add_circle_obstacle(CrowdContext* crowd_context, Vec2 center, float radius);

    /**
     * @brief Mark a circular obstacle inactive. Returns false if already
     * inactive or id is out of range. Call process_circle_obstacles() to rebuild.
     */
    bool remove_circle_obstacle(CrowdContext* crowd_context, uint16_t obstacle_id);

    /**
     * @brief Build the circle obstacle KD-tree. Call after add/remove.
     */
    CrowdResult process_circle_obstacles(CrowdContext* crowd_context);

    uint16_t    get_agent_count(const CrowdContext* crowd_context);
    uint16_t    get_obstacle_count(const CrowdContext* crowd_context);
    uint16_t    get_num_obstacle_vertices(const CrowdContext* crowd_context);

    /**
     * @brief Find active agents within `radius` of `center`, using the currently built
     * agent KD-tree. Pure center-distance test; agent radius is not included.
     *
     * Results are written to `out_neighbors`, sorted by distance ascending. If more
     * agents match than `max_count`, only the closest `max_count` are kept.
     *
     * Thread-safe for concurrent query calls when each caller provides a separate
     * output buffer, the agent tree has already been built at least once, and the
     * CrowdContext is not being mutated concurrently.
     *
     * @return number of AgentNeighbor entries written.
     */
    uint16_t query_agents_in_radius(const CrowdContext* crowd_context, Vec2 center, float radius, uint8_t nav_mask, AgentNeighbor* out_neighbors, uint16_t max_count);

    /**
     * @brief Cast a finite segment from `start` to `end` against active agents, using the
     * currently built agent KD-tree. Each candidate agent is tested with a combined radius
     * of `radius + agent.radius`, so `radius` of 0 tests the segment against agents' own
     * radii only.
     *
     * Results are written to `out_hits`, sorted by `t` (normalized position along the
     * segment, clamped to [0,1]) ascending, with `agent_id` as a deterministic tiebreak.
     * If more agents match than `max_count`, only the earliest `max_count` hits are kept.
     *
     * Thread-safe for concurrent query calls when each caller provides a separate
     * output buffer, the agent tree has already been built at least once, and the
     * CrowdContext is not being mutated concurrently.
     *
     * @return number of AgentRaycastHit entries written.
     */
    uint16_t raycast_agents(const CrowdContext* crowd_context, Vec2 start, Vec2 end, float radius, uint8_t nav_mask, AgentRaycastHit* out_hits, uint16_t max_count);

    /**
     * @brief Find circle obstacles within `radius` of `center`, using the currently built
     * circle-obstacle KD-tree. Pure center-distance test; the obstacle's own radius is not
     * included (strict `center_dist_sq < radius^2`, an exactly-tangent obstacle is excluded).
     *
     * Results are written to `out_neighbors`, sorted by distance ascending. If more
     * obstacles match than `max_count`, only the closest `max_count` are kept.
     *
     * Requires `crowd::process_circle_obstacles()` to have been called at least once.
     * Returned `obs_id`s are current slot indices, not permanent identities: a removed
     * obstacle's slot can be reused by a later `add_circle_obstacle()` call.
     *
     * @return number of CircleObstacleNeighbor entries written.
     */
    uint16_t query_circle_obstacles_in_radius(const CrowdContext* crowd_context, Vec2 center, float radius, uint8_t nav_mask, CircleObstacleNeighbor* out_neighbors, uint16_t max_count);

    /**
     * @brief Find circle obstacles overlapping a circle of `radius` centered at `center`,
     * using the currently built circle-obstacle KD-tree. True circle-vs-circle overlap test:
     * an obstacle matches if `distance(center, obstacle.center) < radius + obstacle.radius`
     * (strict `<`; exact tangency is excluded).
     *
     * Results are written to `out_neighbors`, sorted by distance ascending. If more
     * obstacles match than `max_count`, only the closest `max_count` are kept.
     *
     * Requires `crowd::process_circle_obstacles()` to have been called at least once.
     * Returned `obs_id`s are current slot indices, not permanent identities: a removed
     * obstacle's slot can be reused by a later `add_circle_obstacle()` call.
     *
     * @return number of CircleObstacleNeighbor entries written.
     */
    uint16_t query_circle_obstacles_colliding(const CrowdContext* crowd_context, Vec2 center, float radius, uint8_t nav_mask, CircleObstacleNeighbor* out_neighbors, uint16_t max_count);

    /**
     * @brief Cast a finite segment from `start` to `end` against circle obstacles, using the
     * currently built circle-obstacle KD-tree. Each candidate obstacle is tested with a combined
     * radius of `radius + obstacle.radius`, so `radius` of 0 tests the segment against obstacles'
     * own radii only.
     *
     * Results are written to `out_hits`, sorted by `t` (normalized position along the
     * segment, clamped to [0,1]) ascending, with `obs_id` as a deterministic tiebreak.
     * If more obstacles match than `max_count`, only the earliest `max_count` hits are kept.
     *
     * Requires `crowd::process_circle_obstacles()` to have been called at least once.
     * Returned `obs_id`s are current slot indices, not permanent identities: a removed
     * obstacle's slot can be reused by a later `add_circle_obstacle()` call.
     *
     * Thread-safe for concurrent query calls when each caller provides a separate
     * output buffer, the circle-obstacle tree has already been built at least once, and the
     * CrowdContext is not being mutated concurrently.
     *
     * @return number of CircleObstacleRaycastHit entries written.
     */
    uint16_t raycast_circle_obstacles(const CrowdContext* crowd_context, Vec2 start, Vec2 end, float radius, uint8_t nav_mask, CircleObstacleRaycastHit* out_hits, uint16_t max_count);

    // Agent setters
    void set_agent_pref_velocity(CrowdContext* crowd_context, uint16_t agent_id, Vec2 pref_velocity);
    void set_agent_position(CrowdContext* crowd_context, uint16_t agent_id, Vec2 position);
    void set_agent_velocity(CrowdContext* crowd_context, uint16_t agent_id, Vec2 velocity);
    void set_agent_max_speed(CrowdContext* crowd_context, uint16_t agent_id, float max_speed);
    void set_agent_radius(CrowdContext* crowd_context, uint16_t agent_id, float radius);
    void set_agent_neighbor_dist(CrowdContext* crowd_context, uint16_t agent_id, float neighbor_dist);
    void set_agent_max_neighbors(CrowdContext* crowd_context, uint16_t agent_id, uint8_t max_neighbors);
    void set_agent_time_horizon(CrowdContext* crowd_context, uint16_t agent_id, float time_horizon);
    void set_agent_time_horizon_obstacle(CrowdContext* crowd_context, uint16_t agent_id, float time_horizon_obstacle);
    void set_agent_nav_mask(CrowdContext* crowd_context, uint16_t agent_id, uint8_t nav_mask);
    void set_obstacle_nav_mask(CrowdContext* crowd_context, uint16_t obstacle_id, uint8_t nav_mask);
    void set_circle_obstacle_nav_mask(CrowdContext* crowd_context, uint16_t obstacle_id, uint8_t nav_mask);

    // Agent getters
    Vec2     get_agent_position(const CrowdContext* crowd_context, uint16_t agent_id);
    Vec2     get_agent_pref_velocity(const CrowdContext* crowd_context, uint16_t agent_id);
    Vec2     get_agent_velocity(const CrowdContext* crowd_context, uint16_t agent_id);
    float    get_agent_max_speed(const CrowdContext* crowd_context, uint16_t agent_id);
    float    get_agent_radius(const CrowdContext* crowd_context, uint16_t agent_id);
    float    get_agent_neighbor_dist(const CrowdContext* crowd_context, uint16_t agent_id);
    uint8_t  get_agent_max_neighbors(const CrowdContext* crowd_context, uint16_t agent_id);
    float    get_agent_time_horizon(const CrowdContext* crowd_context, uint16_t agent_id);
    float    get_agent_time_horizon_obstacle(const CrowdContext* crowd_context, uint16_t agent_id);
    uint8_t  get_agent_nav_mask(const CrowdContext* crowd_context, uint16_t agent_id);
    uint8_t  get_obstacle_nav_mask(const CrowdContext* crowd_context, uint16_t obstacle_id);
    uint8_t  get_circle_obstacle_nav_mask(const CrowdContext* crowd_context, uint16_t obstacle_id);

    void     set_agent_avoidance_weight(CrowdContext* crowd_context, uint16_t agent_id, float weight);
    float    get_agent_avoidance_weight(const CrowdContext* crowd_context, uint16_t agent_id);
    void     set_agent_avoidance_priority(CrowdContext* crowd_context, uint16_t agent_id, uint8_t priority);
    uint8_t  get_agent_avoidance_priority(const CrowdContext* crowd_context, uint16_t agent_id);
    void     set_agent_avoidance_locked(CrowdContext* crowd_context, uint16_t agent_id, bool locked);
    bool     get_agent_avoidance_locked(const CrowdContext* crowd_context, uint16_t agent_id);
    void     set_agent_kinematic(CrowdContext* crowd_context, uint16_t agent_id, bool is_kinematic);
    bool     get_agent_kinematic(const CrowdContext* crowd_context, uint16_t agent_id);
    uint16_t get_agent_num_agent_neighbors(const CrowdContext* crowd_context, uint16_t agent_id);
    uint16_t get_agent_num_obstacle_neighbors(const CrowdContext* crowd_context, uint16_t agent_id);
    uint16_t get_agent_num_circle_obstacle_neighbors(const CrowdContext* crowd_context, uint16_t agent_id);
    uint16_t get_agent_num_orca_lines(const CrowdContext* crowd_context, uint16_t agent_id);

    // Indexed neighbor / ORCA-line accessors
    uint16_t get_agent_agent_neighbor(const CrowdContext* crowd_context, uint16_t agent_id, uint16_t neighbor_idx);
    uint16_t get_agent_obstacle_neighbor(const CrowdContext* crowd_context, uint16_t agent_id, uint16_t neighbor_idx);
    uint16_t get_agent_circle_obstacle_neighbor(const CrowdContext* crowd_context, uint16_t agent_id, uint16_t neighbor_idx);
    OrcaLine get_agent_orca_line(const CrowdContext* crowd_context, uint16_t agent_id, uint16_t line_idx);

    // Obstacle vertex accessors
    Vec2     get_obstacle_vertex(const CrowdContext* crowd_context, uint16_t vertex_id);
    uint16_t get_next_obstacle_vertex_id(const CrowdContext* crowd_context, uint16_t vertex_id);
    uint16_t get_prev_obstacle_vertex_id(const CrowdContext* crowd_context, uint16_t vertex_id);

    // Simulation time
    void  set_time_step(CrowdContext* crowd_context, float time_step);
    float get_time_step(const CrowdContext* crowd_context);
    float get_global_time(const CrowdContext* crowd_context);

    /**
     * @brief Advance the simulation by one time step.
     * Rebuilds the agent KdTree, computes neighbors and new velocities for all
     * active agents, then integrates positions. Requires time_step > 0.
     */
    void do_step(CrowdContext* crowd_context);

#ifdef CROWD_HAS_JOBSYSTEM
    /**
     * @brief Parallel simulation step via Defold's JobSystem.
     * Phase 1: compute_neighbors + compute_new_velocity across worker chunks.
     * Phase 2: position integration across worker chunks.
     * Falls back to do_step() for timing-enabled builds, invalid JobSystem
     * contexts, or small agent counts. For the fast path, context max_workers
     * must match the JobSystem background worker count plus the calling thread.
     */
    void do_step_parallel(CrowdContext* crowd_context, HJobContext job_system_context);
#endif

    void init();
    void shutdown();

    enum CrowdBuildFeature
    {
        CROWD_BUILD_FEATURE_JOBSYSTEM = 1U << 0U
    };

    uint32_t get_build_features();

} // namespace crowd
