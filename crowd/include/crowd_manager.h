#pragma once

#include <dmsdk/sdk.h>

namespace crowd
{

    namespace manager
    {

        // Until we have dmThread::PlatformHasThreadSupport() in dmsdk
        static bool PlatformHasThreadSupport()
        {
#if defined(DM_PLATFORM_HTML5)
#if defined(__EMSCRIPTEN_PTHREADS__)
            return true;
#else
            return false;
#endif
#else
            return true;
#endif
        }

        int init(lua_State* L);

        int destroy(lua_State* L);
        int add_agent(lua_State* L);
        int remove_agent(lua_State* L);
        int add_obstacle(lua_State* L);
        int remove_obstacle(lua_State* L);
        int process_obstacles(lua_State* L);
        int add_circle_obstacle(lua_State* L);
        int remove_circle_obstacle(lua_State* L);
        int process_circle_obstacles(lua_State* L);
        int get_agent_count(lua_State* L);
        int get_obstacle_count(lua_State* L);

        // Gameobject
        int init_gameobjects(lua_State* L);
        int add_gameobject(lua_State* L);
        int set_gameobject_goal(lua_State* L);
        int remove_gameobject_goal(lua_State* L);
        int get_gameobject_count(lua_State* L);
        int run_simulation(lua_State* L);

        // Agent setters
        int set_agent_pref_velocity(lua_State* L);
        int set_agent_position(lua_State* L);
        int set_agent_velocity(lua_State* L);
        int set_agent_max_speed(lua_State* L);
        int set_agent_radius(lua_State* L);
        int set_agent_neighbor_dist(lua_State* L);
        int set_agent_max_neighbors(lua_State* L);
        int set_agent_time_horizon(lua_State* L);
        int set_agent_time_horizon_obstacle(lua_State* L);
        int set_agent_nav_mask(lua_State* L);
        int set_obstacle_nav_mask(lua_State* L);
        int set_circle_obstacle_nav_mask(lua_State* L);
        int set_agent_avoidance_weight(lua_State* L);
        int set_agent_avoidance_priority(lua_State* L);
        int set_agent_avoidance_locked(lua_State* L);
        int set_agent_kinematic(lua_State* L);

        // Agent setters (batch)
        int set_agent_positions(lua_State* L);
        int set_agent_velocities(lua_State* L);
        int set_agent_pref_velocities(lua_State* L);
        int set_agent_positions_and_velocities(lua_State* L);

        // Agent getters
        int get_agent_position(lua_State* L);
        int get_agent_pref_velocity(lua_State* L);
        int get_agent_velocity(lua_State* L);
        int get_agent_max_speed(lua_State* L);
        int get_agent_radius(lua_State* L);
        int get_agent_neighbor_dist(lua_State* L);
        int get_agent_max_neighbors(lua_State* L);
        int get_agent_time_horizon(lua_State* L);
        int get_agent_time_horizon_obstacle(lua_State* L);
        int get_agent_nav_mask(lua_State* L);
        int get_obstacle_nav_mask(lua_State* L);
        int get_circle_obstacle_nav_mask(lua_State* L);
        int get_agent_avoidance_weight(lua_State* L);
        int get_agent_avoidance_priority(lua_State* L);
        int get_agent_avoidance_locked(lua_State* L);
        int get_agent_kinematic(lua_State* L);
        int get_agent_num_agent_neighbors(lua_State* L);
        int get_agent_num_obstacle_neighbors(lua_State* L);
        int get_agent_num_circle_obstacle_neighbors(lua_State* L);
        int get_agent_num_orca_lines(lua_State* L);

        // Agent getters (batch)
        int get_agent_positions(lua_State* L);
        int get_agent_velocities(lua_State* L);
        int get_agent_positions_and_velocities(lua_State* L);

        // Indexed neighbor / ORCA-line accessors
        int get_agent_agent_neighbor(lua_State* L);
        int get_agent_obstacle_neighbor(lua_State* L);
        int get_agent_circle_obstacle_neighbor(lua_State* L);
        int get_agent_orca_line(lua_State* L);

        // Obstacle vertex accessors
        int get_obstacle_vertex(lua_State* L);
        int get_next_obstacle_vertex_id(lua_State* L);
        int get_prev_obstacle_vertex_id(lua_State* L);
        int get_num_obstacle_vertices(lua_State* L);

        // Simulation time
        int set_time_step(lua_State* L);
        int get_time_step(lua_State* L);
        int get_global_time(lua_State* L);

        int do_step(lua_State* L);
        int do_step_parallel(lua_State* L);

        // Spatial queries
        int query_agents_in_radius(lua_State* L);
        int set_max_agent_query(lua_State* L);

        int raycast_agents(lua_State* L);
        int set_max_raycast_query(lua_State* L);

        int query_circle_obstacles_in_radius(lua_State* L);
        int query_circle_obstacles_colliding(lua_State* L);
        int set_max_circle_query(lua_State* L);

        int raycast_circle_obstacles(lua_State* L);
        int set_max_circle_raycast_query(lua_State* L);

        // Finalize
        void shutdown();

        void set_update_frequency(uint8_t frequency);
        void set_max_time_step(float max_time_step);
        void gameobject_update();

    } // namespace manager

} // namespace crowd
