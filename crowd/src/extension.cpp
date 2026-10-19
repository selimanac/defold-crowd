// crowd.cpp
// Extension lib defines
#include <cstdint>
#define LIB_NAME "Crowd"
#define MODULE_NAME "crowd"

// include the Defold SDK
#include <dmsdk/sdk.h>
#include "crowd/const.h"
#include "crowd_manager.h"

// Functions exposed to Lua
static const luaL_reg Module_methods[] = {
    { "init", crowd::manager::init },
    { "destroy", crowd::manager::destroy },
    { "add_agent", crowd::manager::add_agent },
    { "remove_agent", crowd::manager::remove_agent },
    { "add_obstacle", crowd::manager::add_obstacle },
    { "remove_obstacle", crowd::manager::remove_obstacle },
    { "process_obstacles", crowd::manager::process_obstacles },
    { "add_circle_obstacle", crowd::manager::add_circle_obstacle },
    { "remove_circle_obstacle", crowd::manager::remove_circle_obstacle },
    { "process_circle_obstacles", crowd::manager::process_circle_obstacles },
    { "get_agent_count", crowd::manager::get_agent_count },
    { "get_obstacle_count", crowd::manager::get_obstacle_count },

    // Gameobjects
    { "init_gameobjects", crowd::manager::init_gameobjects },
    { "add_gameobject", crowd::manager::add_gameobject },
    { "set_gameobject_goal", crowd::manager::set_gameobject_goal },
    { "remove_gameobject_goal", crowd::manager::remove_gameobject_goal },
    { "get_gameobject_count", crowd::manager::get_gameobject_count },
    { "run_simulation", crowd::manager::run_simulation },

    // Agent setters
    { "set_agent_pref_velocity", crowd::manager::set_agent_pref_velocity },
    { "set_agent_position", crowd::manager::set_agent_position },
    { "set_agent_velocity", crowd::manager::set_agent_velocity },
    { "set_agent_max_speed", crowd::manager::set_agent_max_speed },
    { "set_agent_radius", crowd::manager::set_agent_radius },
    { "set_agent_neighbor_dist", crowd::manager::set_agent_neighbor_dist },
    { "set_agent_max_neighbors", crowd::manager::set_agent_max_neighbors },
    { "set_agent_time_horizon", crowd::manager::set_agent_time_horizon },
    { "set_agent_time_horizon_obstacle", crowd::manager::set_agent_time_horizon_obstacle },
    { "set_agent_nav_mask", crowd::manager::set_agent_nav_mask },
    { "set_obstacle_nav_mask", crowd::manager::set_obstacle_nav_mask },
    { "set_circle_obstacle_nav_mask", crowd::manager::set_circle_obstacle_nav_mask },
    { "set_agent_avoidance_weight", crowd::manager::set_agent_avoidance_weight },
    { "set_agent_avoidance_priority", crowd::manager::set_agent_avoidance_priority },
    { "set_agent_avoidance_locked", crowd::manager::set_agent_avoidance_locked },
    { "set_agent_kinematic", crowd::manager::set_agent_kinematic },

    // Agent setters (batch)
    { "set_agent_positions", crowd::manager::set_agent_positions },
    { "set_agent_velocities", crowd::manager::set_agent_velocities },
    { "set_agent_pref_velocities", crowd::manager::set_agent_pref_velocities },
    { "set_agent_positions_and_velocities", crowd::manager::set_agent_positions_and_velocities },

    // Agent getters
    { "get_agent_position", crowd::manager::get_agent_position },
    { "get_agent_pref_velocity", crowd::manager::get_agent_pref_velocity },
    { "get_agent_velocity", crowd::manager::get_agent_velocity },
    { "get_agent_max_speed", crowd::manager::get_agent_max_speed },
    { "get_agent_radius", crowd::manager::get_agent_radius },
    { "get_agent_neighbor_dist", crowd::manager::get_agent_neighbor_dist },
    { "get_agent_max_neighbors", crowd::manager::get_agent_max_neighbors },
    { "get_agent_time_horizon", crowd::manager::get_agent_time_horizon },
    { "get_agent_time_horizon_obstacle", crowd::manager::get_agent_time_horizon_obstacle },
    { "get_agent_nav_mask", crowd::manager::get_agent_nav_mask },
    { "get_obstacle_nav_mask", crowd::manager::get_obstacle_nav_mask },
    { "get_circle_obstacle_nav_mask", crowd::manager::get_circle_obstacle_nav_mask },
    { "get_agent_avoidance_weight", crowd::manager::get_agent_avoidance_weight },
    { "get_agent_avoidance_priority", crowd::manager::get_agent_avoidance_priority },
    { "get_agent_avoidance_locked", crowd::manager::get_agent_avoidance_locked },
    { "get_agent_kinematic", crowd::manager::get_agent_kinematic },
    { "get_agent_num_agent_neighbors", crowd::manager::get_agent_num_agent_neighbors },
    { "get_agent_num_obstacle_neighbors", crowd::manager::get_agent_num_obstacle_neighbors },
    { "get_agent_num_circle_obstacle_neighbors", crowd::manager::get_agent_num_circle_obstacle_neighbors },
    { "get_agent_num_orca_lines", crowd::manager::get_agent_num_orca_lines },

    // Agent getters (batch)
    { "get_agent_positions", crowd::manager::get_agent_positions },
    { "get_agent_velocities", crowd::manager::get_agent_velocities },
    { "get_agent_positions_and_velocities", crowd::manager::get_agent_positions_and_velocities },

    // Indexed neighbor / ORCA-line accessors
    { "get_agent_agent_neighbor", crowd::manager::get_agent_agent_neighbor },
    { "get_agent_obstacle_neighbor", crowd::manager::get_agent_obstacle_neighbor },
    { "get_agent_circle_obstacle_neighbor", crowd::manager::get_agent_circle_obstacle_neighbor },
    { "get_agent_orca_line", crowd::manager::get_agent_orca_line },

    // Obstacle vertex accessors
    { "get_obstacle_vertex", crowd::manager::get_obstacle_vertex },
    { "get_next_obstacle_vertex_id", crowd::manager::get_next_obstacle_vertex_id },
    { "get_prev_obstacle_vertex_id", crowd::manager::get_prev_obstacle_vertex_id },
    { "get_num_obstacle_vertices", crowd::manager::get_num_obstacle_vertices },

    // Simulation time
    { "set_time_step", crowd::manager::set_time_step },
    { "get_time_step", crowd::manager::get_time_step },
    { "get_global_time", crowd::manager::get_global_time },

    { "do_step", crowd::manager::do_step },
    { "do_step_parallel", crowd::manager::do_step_parallel },

    // Spatial queries
    { "query_agents_in_radius", crowd::manager::query_agents_in_radius },
    { "set_max_agent_query", crowd::manager::set_max_agent_query },
    { "raycast_agents", crowd::manager::raycast_agents },
    { "set_max_raycast_query", crowd::manager::set_max_raycast_query },
    { "query_circle_obstacles_in_radius", crowd::manager::query_circle_obstacles_in_radius },
    { "query_circle_obstacles_colliding", crowd::manager::query_circle_obstacles_colliding },
    { "set_max_circle_query", crowd::manager::set_max_circle_query },
    { "raycast_circle_obstacles", crowd::manager::raycast_circle_obstacles },
    { "set_max_circle_raycast_query", crowd::manager::set_max_circle_raycast_query },

    { 0, 0 }
};

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);

    // Register lua names
    luaL_register(L, MODULE_NAME, Module_methods);

    // Constants
    lua_pushnumber(L, (lua_Number)crowd::AGENT_ID_NONE);
    lua_setfield(L, -2, "INVALID_ID");

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

static dmExtension::Result AppInitializeCrowd(dmExtension::AppParams* params)
{
    dmLogInfo("AppInitializeCrowd");

    uint8_t update_frequency = (uint8_t)dmConfigFile::GetInt(params->m_ConfigFile, "display.update_frequency", 0);
    float   max_time_step = dmConfigFile::GetFloat(params->m_ConfigFile, "engine.max_time_step", 1.0f / 30);

    crowd::manager::set_update_frequency(update_frequency);
    crowd::manager::set_max_time_step(max_time_step);

    return dmExtension::RESULT_OK;
}

static dmExtension::Result InitializeCrowd(dmExtension::Params* params)
{
    // Init Lua
    LuaInit(params->m_L);
    dmLogInfo("Registered %s Extension", MODULE_NAME);
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizeCrowd(dmExtension::AppParams* params)
{
    dmLogInfo("AppFinalizeCrowd");
    return dmExtension::RESULT_OK;
}

static dmExtension::Result FinalizeCrowd(dmExtension::Params* params)
{
    dmLogInfo("FinalizeCrowd");
    crowd::manager::shutdown();
    return dmExtension::RESULT_OK;
}

static dmExtension::Result OnUpdateCrowd(dmExtension::Params* params)
{
    crowd::manager::gameobject_update();
    return dmExtension::RESULT_OK;
}

static void OnEventCrowd(dmExtension::Params* params, const dmExtension::Event* event)
{
    // TODO: Check if needed to pause update
    /*switch (event->m_Event)
    {
        case dmExtension::EVENT_ID_ACTIVATEAPP:
            dmLogInfo("OnEventCrowd - EVENT_ID_ACTIVATEAPP");
            break;
        case dmExtension::EVENT_ID_DEACTIVATEAPP:
            dmLogInfo("OnEventCrowd - EVENT_ID_DEACTIVATEAPP");
            break;
        case dmExtension::EVENT_ID_ICONIFYAPP:
            dmLogInfo("OnEventCrowd - EVENT_ID_ICONIFYAPP");
            break;
        case dmExtension::EVENT_ID_DEICONIFYAPP:
            dmLogInfo("OnEventCrowd - EVENT_ID_DEICONIFYAPP");
            break;
        default:
            dmLogWarning("OnEventCrowd - Unknown event id");
            break;
    }*/
}

// Defold SDK uses a macro for setting up extension entry points:
//
// DM_DECLARE_EXTENSION(symbol, name, app_init, app_final, init, update, on_event, final)

// Crowd is the C++ symbol that holds all relevant extension data.
// It must match the name field in the `ext.manifest`
DM_DECLARE_EXTENSION(Crowd, LIB_NAME, AppInitializeCrowd, AppFinalizeCrowd, InitializeCrowd, OnUpdateCrowd, 0, FinalizeCrowd)
