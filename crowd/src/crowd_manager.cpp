#include "crowd_manager.h"
#include "crowd.h"
#include "crowd/math.h"
#include "crowd/types.h"
#include "dmsdk/dlib/array.h"

#include "dmsdk/dlib/log.h"
#include "dmsdk/dlib/vmath.h"
#include "dmsdk/gameobject/gameobject.h"
#include "dmsdk/lua/lua.h"
#include "unit.h"
#include <cstdint>

namespace crowd
{

    namespace manager
    {

        // TODO: Clean
        struct GameUpdate
        {
            uint8_t  update_frequency = 0;
            float    max_time_step = 0.033333;
            uint64_t previous_frame_time = dmTime::GetMonotonicTime();
            float    accum_frame_time = 0.0f;
            bool     update_loop_state = false;
            bool     validate_gameobjects = false;
        };
        GameUpdate                                      game_update;

        static crowd::CrowdContext*                     crowd_context = NULL;
        static dmArray<crowd::AgentNeighbor>            agent_query_scratch;
        static dmArray<crowd::AgentRaycastHit>          agent_raycast_scratch;
        static dmArray<crowd::CircleObstacleNeighbor>   circle_query_scratch;
        static dmArray<crowd::CircleObstacleRaycastHit> circle_raycast_scratch;

#ifdef CROWD_HAS_JOBSYSTEM
        struct JobSystemCreateParams job_params;
        HJobContext                  job_system = NULL;
#endif

        static void reset_gameobject_update(bool enabled)
        {
            game_update.update_loop_state = enabled;
            game_update.previous_frame_time = dmTime::GetMonotonicTime();
            game_update.accum_frame_time = 0.0f;
        }

        static void reset_agent_query_scratch()
        {
            agent_query_scratch.SetSize(0U);
            agent_query_scratch.SetCapacity(0U);
        }

        static void reset_agent_raycast_scratch()
        {
            agent_raycast_scratch.SetSize(0U);
            agent_raycast_scratch.SetCapacity(0U);
        }

        static void reset_circle_query_scratch()
        {
            circle_query_scratch.SetSize(0U);
            circle_query_scratch.SetCapacity(0U);
        }

        static void reset_circle_raycast_scratch()
        {
            circle_raycast_scratch.SetSize(0U);
            circle_raycast_scratch.SetCapacity(0U);
        }

        static void set_agent_query_scratch_capacity(uint32_t capacity, const char* caller)
        {
            agent_query_scratch.SetCapacity(capacity);
            agent_query_scratch.SetSize(0U);
            if (agent_query_scratch.Capacity() != capacity)
            {
                dmLogError("%s: failed to set agent query scratch capacity to %u", caller, capacity);
                reset_agent_query_scratch();
            }
        }

        static void set_agent_raycast_scratch_capacity(uint32_t capacity, const char* caller)
        {
            agent_raycast_scratch.SetCapacity(capacity);
            agent_raycast_scratch.SetSize(0U);
            if (agent_raycast_scratch.Capacity() != capacity)
            {
                dmLogError("%s: failed to set agent raycast scratch capacity to %u", caller, capacity);
                reset_agent_raycast_scratch();
            }
        }

        static void set_circle_query_scratch_capacity(uint32_t capacity, const char* caller)
        {
            circle_query_scratch.SetCapacity(capacity);
            circle_query_scratch.SetSize(0U);
            if (circle_query_scratch.Capacity() != capacity)
            {
                dmLogError("%s: failed to set circle query scratch capacity to %u", caller, capacity);
                reset_circle_query_scratch();
            }
        }

        static void set_circle_raycast_scratch_capacity(uint32_t capacity, const char* caller)
        {
            circle_raycast_scratch.SetCapacity(capacity);
            circle_raycast_scratch.SetSize(0U);
            if (circle_raycast_scratch.Capacity() != capacity)
            {
                dmLogError("%s: failed to set circle raycast scratch capacity to %u", caller, capacity);
                reset_circle_raycast_scratch();
            }
        }

        // From Defold source
        // https://github.com/defold/defold/blob/cdaa870389ca00062bfc03bcda8f4fb34e93124a/engine/engine/src/engine.cpp#L1860
        static void calc_time_step(float& step_dt, uint32_t& num_steps)
        {
            uint64_t time = dmTime::GetMonotonicTime();
            uint64_t frame_time = time - game_update.previous_frame_time;
            game_update.previous_frame_time = time;

            float frame_dt = (float)(frame_time / 1000000.0);

            // Never allow for large hitches
            if (frame_dt > game_update.max_time_step)
            {
                frame_dt = game_update.max_time_step;
            }

            // Variable frame rate
            if (game_update.update_frequency == 0)
            {
                step_dt = frame_dt;
                num_steps = 1;
                return;
            }

            // Fixed frame rate
            float fixed_dt = 1.0f / (float)game_update.update_frequency;

            // We don't allow having a higher framerate than the actual variable frame
            // rate since the update+render is currently coupled together and also Flip()
            // would be called more than once. E.g. if the fixed_dt == 1/120 and the
            // frame_dt == 1/60
            if (fixed_dt < frame_dt)
            {
                fixed_dt = frame_dt;
            }

            game_update.accum_frame_time += frame_dt;

            float num_steps_f = game_update.accum_frame_time / fixed_dt;

            num_steps = (uint32_t)num_steps_f;
            step_dt = fixed_dt;

            game_update.accum_frame_time = game_update.accum_frame_time - num_steps * fixed_dt;
        }

        int init(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);

            shutdown();

            uint16_t          max_agents = luaL_checkinteger(L, 1);
            uint8_t           max_neighbors = luaL_checkinteger(L, 2);
            uint16_t          max_obstacle_vertices = luaL_optinteger(L, 3, 0);
            uint16_t          max_circle_obstacles = luaL_optinteger(L, 4, 0);
            const lua_Integer max_workers_value = luaL_optinteger(L, 5, 1);
            luaL_argcheck(L, max_workers_value >= 1 && max_workers_value <= 255, 5, "max_workers must be between 1 and 255");

            const uint8_t max_workers = (uint8_t)max_workers_value;

            uint8_t       worker_count = 1U;
#ifdef CROWD_HAS_JOBSYSTEM
            if (max_workers > 1U && PlatformHasThreadSupport())
            {
                job_params.m_ThreadNamePrefix = "crowd";
                job_params.m_ThreadCount = (uint8_t)(max_workers - 1U);
                job_system = JobSystemCreate(&job_params);

                if (job_system)
                {
                    const uint32_t actual_background_workers = JobSystemGetWorkerCount(job_system);
                    if (actual_background_workers > 0U)
                    {
                        const uint32_t actual_worker_count = actual_background_workers + 1U;
                        worker_count = (uint8_t)(actual_worker_count > (uint32_t)max_workers ? max_workers : actual_worker_count);
                    }
                    else
                    {
                        JobSystemDestroy(job_system);
                        job_system = NULL;
                    }
                }
            }
#endif

            crowd_context = crowd::create_context(max_agents, max_obstacle_vertices, max_circle_obstacles, max_neighbors, worker_count);

            if (!crowd_context)
            {
                dmLogError("Create_context failed");
#ifdef CROWD_HAS_JOBSYSTEM
                if (job_system)
                {
                    JobSystemDestroy(job_system);
                    job_system = NULL;
                }
#endif
            }
            else
            {
                set_agent_query_scratch_capacity(max_agents, "init");
                set_agent_raycast_scratch_capacity(max_agents, "init");

                if (max_circle_obstacles > 0U)
                {
                    set_circle_query_scratch_capacity(max_circle_obstacles, "init");
                    set_circle_raycast_scratch_capacity(max_circle_obstacles, "init");
                }
            }

            return 0;
        }

        int init_gameobjects(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);

            if (!crowd_context)
            {
                dmLogError("Can't initialize game objects without a crowd context");
                return 0;
            }

            if (crowd::unit::get_count() != 0U)
            {
                dmLogError("Can't reinitialize game objects while units are active");
                return 0;
            }

            crowd::unit::init(crowd_context->max_agents);

            return 0;
        }

        int add_gameobject(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);

            if (!crowd_context)
            {
                dmLogError("Can't add game object without a crowd context");
                lua_pushinteger(L, crowd::AGENT_ID_NONE);
                return 1;
            }

            dmGameObject::HInstance   instance = dmScript::CheckGOInstance(L, 1);
            dmGameObject::HCollection collection = dmGameObject::GetCollection(instance);
            dmhash_t                  identifier = dmGameObject::GetIdentifier(instance);
            dmVMath::Point3           position = dmGameObject::GetPosition(instance);

            float                     x = position.getX();
            float                     y = position.getY();
            float                     radius = (float)luaL_checknumber(L, 2);
            float                     max_speed = (float)luaL_checknumber(L, 3);
            uint8_t                   max_neighbors = (uint8_t)luaL_checkinteger(L, 4);
            float                     neighbor_distance = (float)luaL_checknumber(L, 5);
            float                     time_horizon = (float)luaL_checknumber(L, 6);
            float                     time_horizon_obstacle = (float)luaL_checknumber(L, 7);

            uint16_t                  agent_id = crowd::add_agent(crowd_context, crowd::Vec2(x, y), radius, max_speed, neighbor_distance, time_horizon, time_horizon_obstacle, max_neighbors);

            if (agent_id == crowd::AGENT_ID_NONE)
            {
                dmLogError("Can't add agent");
            }
            else
            {
                unit::Unit unit;
                unit.agent_id = agent_id;
                unit.collection = collection;
                unit.identifier = identifier;
                unit.instance = instance;

                unit.goal = Vec2(x, y);
                unit.has_goal = false;
                unit.pending_delete = false;
                unit.position_z = position.getZ();

                const dmVMath::Quat q = dmGameObject::GetRotation(instance);
                unit.rotation = 2.0f * atan2f(q.getZ(), q.getW());

                const unit::UnitResult result = crowd::unit::add(unit);

                if (result != unit::UNIT_RESULT_OK)
                {
                    dmLogError("Can't add game object unit: %s", unit::result_to_string(result));
                    if (!crowd::remove_agent(crowd_context, agent_id))
                    {
                        dmLogError("Can't roll back game object agent");
                    }
                    agent_id = crowd::AGENT_ID_NONE;
                }
            }

            lua_pushinteger(L, agent_id);

            return 1;
        }

        int set_gameobject_goal(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);

            uint16_t               agent_id = luaL_checkinteger(L, 1);
            float                  goal_x = (float)luaL_checknumber(L, 2);
            float                  goal_y = (float)luaL_checknumber(L, 3);

            const unit::UnitResult result = unit::set_goal(agent_id, crowd::Vec2(goal_x, goal_y));

            if (result != unit::UNIT_RESULT_OK)
            {
                dmLogError("Can't set game object goal: %s", unit::result_to_string(result));
            }

            return 0;
        }

        int remove_gameobject_goal(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);

            const uint16_t         agent_id = luaL_checkinteger(L, 1);
            const unit::UnitResult result = unit::clear_goal(agent_id);

            if (result != unit::UNIT_RESULT_OK)
            {
                dmLogError("Can't remove game object goal: %s", unit::result_to_string(result));
            }

            return 0;
        }

        int get_gameobject_count(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);

            lua_pushinteger(L, unit::get_count());
            return 1;
        }

        int run_simulation(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);

            luaL_checktype(L, 1, LUA_TBOOLEAN);
            const bool state = lua_toboolean(L, 1) != 0;
            reset_gameobject_update(state);

            return 0;
        }

        int destroy(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);

            shutdown();

            return 0;
        }

        int add_agent(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);

            float          x = (float)luaL_checknumber(L, 1);
            float          y = (float)luaL_checknumber(L, 2);
            float          radius = (float)luaL_checknumber(L, 3);
            float          max_speed = (float)luaL_checknumber(L, 4);
            uint8_t        max_neighbors = (uint8_t)luaL_checkinteger(L, 5);
            float          neighbor_distance = (float)luaL_checknumber(L, 6);
            float          time_horizon = (float)luaL_checknumber(L, 7);
            float          time_horizon_obstacle = (float)luaL_checknumber(L, 8);

            const uint16_t agent_id = crowd::add_agent(crowd_context, crowd::Vec2(x, y), radius, max_speed, neighbor_distance, time_horizon, time_horizon_obstacle, max_neighbors);

            if (agent_id == crowd::AGENT_ID_NONE)
            {
                dmLogError("Can't add agent");
            }

            lua_pushinteger(L, agent_id);

            return 1;
        }

        int remove_agent(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);

            uint16_t         agent_id = luaL_checkinteger(L, 1);

            unit::UnitResult result = crowd::unit::mark_for_delete(agent_id);
            const bool       status = crowd::remove_agent(crowd_context, agent_id);

            if (!status)
            {
                dmLogError("Can't remove agent");
            }

            lua_pushboolean(L, status);

            return 1;
        }

        int add_obstacle(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);
            luaL_checktype(L, 1, LUA_TTABLE);
            const uint32_t n = (uint32_t)lua_objlen(L, 1);

            if (n > UINT16_MAX)
            {
                dmLogError("Obstacle vertex count exceeds uint16_t capacity");
                lua_pushinteger(L, crowd::OBSTACLE_ID_NONE);
                return 1;
            }

            dmArray<crowd::Vec2> vertices;
            vertices.SetCapacity(n);
            vertices.SetSize(n);

            for (uint32_t i = 0U; i < n; ++i)
            {
                lua_rawgeti(L, 1, (int)(i + 1U));
                luaL_checktype(L, -1, LUA_TTABLE);

                lua_getfield(L, -1, "x");
                float x = (float)luaL_checknumber(L, -1);
                lua_pop(L, 1);

                lua_getfield(L, -1, "y");
                float y = (float)luaL_checknumber(L, -1);
                lua_pop(L, 1);

                lua_pop(L, 1);
                vertices[i] = crowd::Vec2(x, y);
            }

            uint16_t obstacle_id = crowd::add_obstacle(crowd_context, vertices.Begin(), (uint16_t)n);

            if (obstacle_id == OBSTACLE_ID_NONE)
            {
                dmLogError("Can't add_circle_obstacle");
            }

            lua_pushinteger(L, obstacle_id);

            return 1;
        }

        int remove_obstacle(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);

            uint16_t   obstacle_id = luaL_checkinteger(L, 1);
            const bool status = crowd::remove_obstacle(crowd_context, obstacle_id);

            if (!status)
            {
                dmLogError("Can't remove obstacle");
            }

            lua_pushboolean(L, status);

            return 1;
        }

        int process_obstacles(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);

            crowd::CrowdResult res = crowd::process_obstacles(crowd_context);

            if (res != crowd::CROWD_RESULT_OK)
            {
                dmLogError("process_obstacles failed: %s", crowd::result_to_string(res));
            }

            return 0;
        }

        int add_circle_obstacle(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);

            float    x = (float)luaL_checknumber(L, 1);
            float    y = (float)luaL_checknumber(L, 2);
            float    radius = (float)luaL_checknumber(L, 3);

            uint16_t obstacle_id = crowd::add_circle_obstacle(crowd_context, crowd::Vec2(x, y), radius);

            if (obstacle_id == OBSTACLE_ID_NONE)
            {
                dmLogError("Can't add_obstacle");
            }

            lua_pushinteger(L, obstacle_id);

            return 1;
        }

        int remove_circle_obstacle(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);

            uint16_t   obstacle_id = luaL_checkinteger(L, 1);
            const bool status = crowd::remove_circle_obstacle(crowd_context, obstacle_id);

            if (!status)
            {
                dmLogError("Can't remove obstacle");
            }

            lua_pushboolean(L, status);

            return 1;
        }

        int process_circle_obstacles(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);

            crowd::CrowdResult res = crowd::process_circle_obstacles(crowd_context);

            if (res != crowd::CROWD_RESULT_OK)
            {
                dmLogError("process_circle_obstacles failed: %s", crowd::result_to_string(res));
            }

            return 0;
        }

        int get_agent_count(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);

            lua_pushinteger(L, crowd::get_agent_count(crowd_context));

            return 1;
        }

        int get_obstacle_count(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);

            lua_pushinteger(L, crowd::get_obstacle_count(crowd_context));

            return 1;
        }

        // Agent setters
        int set_agent_pref_velocity(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);

            uint16_t agent_id = luaL_checkinteger(L, 1);
            float    x = (float)luaL_checknumber(L, 2);
            float    y = (float)luaL_checknumber(L, 3);
            crowd::set_agent_pref_velocity(crowd_context, agent_id, crowd::Vec2(x, y));

            return 0;
        }

        int set_agent_position(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);

            uint16_t agent_id = luaL_checkinteger(L, 1);
            float    x = (float)luaL_checknumber(L, 2);
            float    y = (float)luaL_checknumber(L, 3);
            crowd::set_agent_position(crowd_context, agent_id, crowd::Vec2(x, y));

            return 0;
        }

        int set_agent_velocity(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);

            uint16_t agent_id = luaL_checkinteger(L, 1);
            float    x = (float)luaL_checknumber(L, 2);
            float    y = (float)luaL_checknumber(L, 3);
            crowd::set_agent_velocity(crowd_context, agent_id, crowd::Vec2(x, y));

            return 0;
        }

        int set_agent_max_speed(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);

            uint16_t agent_id = luaL_checkinteger(L, 1);
            float    speed = (float)luaL_checknumber(L, 2);
            crowd::set_agent_max_speed(crowd_context, agent_id, speed);

            return 0;
        }

        // Agent setters (batch)
        int set_agent_positions(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);
            luaL_checktype(L, 1, LUA_TTABLE); // ids
            luaL_checktype(L, 2, LUA_TTABLE); // xs
            luaL_checktype(L, 3, LUA_TTABLE); // ys
            const uint32_t count = (uint32_t)luaL_checkinteger(L, 4);

            for (uint32_t i = 0U; i < count; ++i)
            {
                lua_rawgeti(L, 1, (int)(i + 1U));
                const uint16_t agent_id = (uint16_t)lua_tointeger(L, -1);
                lua_pop(L, 1);

                lua_rawgeti(L, 2, (int)(i + 1U));
                const float x = (float)lua_tonumber(L, -1);
                lua_pop(L, 1);

                lua_rawgeti(L, 3, (int)(i + 1U));
                const float y = (float)lua_tonumber(L, -1);
                lua_pop(L, 1);

                crowd::set_agent_position(crowd_context, agent_id, crowd::Vec2(x, y));
            }

            return 0;
        }

        int set_agent_velocities(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);
            luaL_checktype(L, 1, LUA_TTABLE); // ids
            luaL_checktype(L, 2, LUA_TTABLE); // xs
            luaL_checktype(L, 3, LUA_TTABLE); // ys
            const uint32_t count = (uint32_t)luaL_checkinteger(L, 4);

            for (uint32_t i = 0U; i < count; ++i)
            {
                lua_rawgeti(L, 1, (int)(i + 1U));
                const uint16_t agent_id = (uint16_t)lua_tointeger(L, -1);
                lua_pop(L, 1);

                lua_rawgeti(L, 2, (int)(i + 1U));
                const float x = (float)lua_tonumber(L, -1);
                lua_pop(L, 1);

                lua_rawgeti(L, 3, (int)(i + 1U));
                const float y = (float)lua_tonumber(L, -1);
                lua_pop(L, 1);

                crowd::set_agent_velocity(crowd_context, agent_id, crowd::Vec2(x, y));
            }

            return 0;
        }

        int set_agent_pref_velocities(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);
            luaL_checktype(L, 1, LUA_TTABLE); // ids
            luaL_checktype(L, 2, LUA_TTABLE); // xs
            luaL_checktype(L, 3, LUA_TTABLE); // ys
            const uint32_t count = (uint32_t)luaL_checkinteger(L, 4);

            for (uint32_t i = 0U; i < count; ++i)
            {
                lua_rawgeti(L, 1, (int)(i + 1U));
                const uint16_t agent_id = (uint16_t)lua_tointeger(L, -1);
                lua_pop(L, 1);

                lua_rawgeti(L, 2, (int)(i + 1U));
                const float x = (float)lua_tonumber(L, -1);
                lua_pop(L, 1);

                lua_rawgeti(L, 3, (int)(i + 1U));
                const float y = (float)lua_tonumber(L, -1);
                lua_pop(L, 1);

                crowd::set_agent_pref_velocity(crowd_context, agent_id, crowd::Vec2(x, y));
            }

            return 0;
        }

        int set_agent_positions_and_velocities(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);
            luaL_checktype(L, 1, LUA_TTABLE); // ids
            luaL_checktype(L, 2, LUA_TTABLE); // xs
            luaL_checktype(L, 3, LUA_TTABLE); // ys
            luaL_checktype(L, 4, LUA_TTABLE); // vxs
            luaL_checktype(L, 5, LUA_TTABLE); // vys
            const uint32_t count = (uint32_t)luaL_checkinteger(L, 6);

            for (uint32_t i = 0U; i < count; ++i)
            {
                lua_rawgeti(L, 1, (int)(i + 1U));
                const uint16_t agent_id = (uint16_t)lua_tointeger(L, -1);
                lua_pop(L, 1);

                lua_rawgeti(L, 2, (int)(i + 1U));
                const float x = (float)lua_tonumber(L, -1);
                lua_pop(L, 1);

                lua_rawgeti(L, 3, (int)(i + 1U));
                const float y = (float)lua_tonumber(L, -1);
                lua_pop(L, 1);

                lua_rawgeti(L, 4, (int)(i + 1U));
                const float vx = (float)lua_tonumber(L, -1);
                lua_pop(L, 1);

                lua_rawgeti(L, 5, (int)(i + 1U));
                const float vy = (float)lua_tonumber(L, -1);
                lua_pop(L, 1);

                crowd::set_agent_position(crowd_context, agent_id, crowd::Vec2(x, y));
                crowd::set_agent_velocity(crowd_context, agent_id, crowd::Vec2(vx, vy));
            }

            return 0;
        }

        int set_agent_radius(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);

            uint16_t agent_id = luaL_checkinteger(L, 1);
            float    radius = (float)luaL_checknumber(L, 2);
            crowd::set_agent_radius(crowd_context, agent_id, radius);

            return 0;
        }

        int set_agent_neighbor_dist(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);

            uint16_t agent_id = luaL_checkinteger(L, 1);
            float    neighbor_dist = (float)luaL_checknumber(L, 2);
            crowd::set_agent_neighbor_dist(crowd_context, agent_id, neighbor_dist);

            return 0;
        }

        int set_agent_max_neighbors(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);

            uint16_t agent_id = luaL_checkinteger(L, 1);
            uint8_t  max_neighbors = (uint8_t)luaL_checkinteger(L, 2);
            crowd::set_agent_max_neighbors(crowd_context, agent_id, max_neighbors);

            return 0;
        }

        int set_agent_time_horizon(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);

            uint16_t agent_id = luaL_checkinteger(L, 1);
            float    time_horizon = (float)luaL_checknumber(L, 2);
            crowd::set_agent_time_horizon(crowd_context, agent_id, time_horizon);

            return 0;
        }

        int set_agent_time_horizon_obstacle(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);

            uint16_t agent_id = luaL_checkinteger(L, 1);
            float    time_horizon_obstacle = (float)luaL_checknumber(L, 2);
            crowd::set_agent_time_horizon_obstacle(crowd_context, agent_id, time_horizon_obstacle);

            return 0;
        }

        int set_agent_nav_mask(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);

            uint16_t agent_id = luaL_checkinteger(L, 1);
            uint8_t  nav_mask = (uint8_t)luaL_checkinteger(L, 2);
            crowd::set_agent_nav_mask(crowd_context, agent_id, nav_mask);

            return 0;
        }

        int set_obstacle_nav_mask(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);

            uint16_t obstacle_id = luaL_checkinteger(L, 1);
            uint8_t  nav_mask = (uint8_t)luaL_checkinteger(L, 2);
            crowd::set_obstacle_nav_mask(crowd_context, obstacle_id, nav_mask);

            return 0;
        }

        int set_circle_obstacle_nav_mask(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);

            uint16_t obstacle_id = luaL_checkinteger(L, 1);
            uint8_t  nav_mask = (uint8_t)luaL_checkinteger(L, 2);
            crowd::set_circle_obstacle_nav_mask(crowd_context, obstacle_id, nav_mask);

            return 0;
        }

        // Agent getters
        int get_agent_position(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 2);
            uint16_t           agent_id = luaL_checkinteger(L, 1);
            const crowd::Vec2& position = crowd::get_agent_position(crowd_context, agent_id);

            lua_pushnumber(L, position.x);
            lua_pushnumber(L, position.y);

            return 2;
        }

        int get_agent_pref_velocity(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 2);
            uint16_t           agent_id = luaL_checkinteger(L, 1);
            const crowd::Vec2& velocity = crowd::get_agent_pref_velocity(crowd_context, agent_id);

            lua_pushnumber(L, velocity.x);
            lua_pushnumber(L, velocity.y);

            return 2;
        }

        int get_agent_velocity(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 2);
            uint16_t           agent_id = luaL_checkinteger(L, 1);
            const crowd::Vec2& velocity = crowd::get_agent_velocity(crowd_context, agent_id);

            lua_pushnumber(L, velocity.x);
            lua_pushnumber(L, velocity.y);

            return 2;
        }
        int get_agent_max_speed(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);
            uint16_t    agent_id = luaL_checkinteger(L, 1);
            const float max_speed = crowd::get_agent_max_speed(crowd_context, agent_id);

            lua_pushnumber(L, max_speed);

            return 1;
        }

        // Agent getters (batch)
        int get_agent_positions(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);
            luaL_checktype(L, 1, LUA_TTABLE); // ids
            const uint32_t count = (uint32_t)luaL_checkinteger(L, 2);
            luaL_checktype(L, 3, LUA_TTABLE); // xs_out
            luaL_checktype(L, 4, LUA_TTABLE); // ys_out

            for (uint32_t i = 0U; i < count; ++i)
            {
                lua_rawgeti(L, 1, (int)(i + 1U));
                const uint16_t agent_id = (uint16_t)lua_tointeger(L, -1);
                lua_pop(L, 1);

                const crowd::Vec2& position = crowd::get_agent_position(crowd_context, agent_id);

                lua_pushnumber(L, position.x);
                lua_rawseti(L, 3, (int)(i + 1U));
                lua_pushnumber(L, position.y);
                lua_rawseti(L, 4, (int)(i + 1U));
            }

            return 0;
        }

        int get_agent_velocities(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);
            luaL_checktype(L, 1, LUA_TTABLE); // ids
            const uint32_t count = (uint32_t)luaL_checkinteger(L, 2);
            luaL_checktype(L, 3, LUA_TTABLE); // vxs_out
            luaL_checktype(L, 4, LUA_TTABLE); // vys_out

            for (uint32_t i = 0U; i < count; ++i)
            {
                lua_rawgeti(L, 1, (int)(i + 1U));
                const uint16_t agent_id = (uint16_t)lua_tointeger(L, -1);
                lua_pop(L, 1);

                const crowd::Vec2& velocity = crowd::get_agent_velocity(crowd_context, agent_id);

                lua_pushnumber(L, velocity.x);
                lua_rawseti(L, 3, (int)(i + 1U));
                lua_pushnumber(L, velocity.y);
                lua_rawseti(L, 4, (int)(i + 1U));
            }

            return 0;
        }

        int get_agent_positions_and_velocities(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);
            luaL_checktype(L, 1, LUA_TTABLE); // ids
            const uint32_t count = (uint32_t)luaL_checkinteger(L, 2);
            luaL_checktype(L, 3, LUA_TTABLE); // xs_out
            luaL_checktype(L, 4, LUA_TTABLE); // ys_out
            luaL_checktype(L, 5, LUA_TTABLE); // vxs_out
            luaL_checktype(L, 6, LUA_TTABLE); // vys_out

            for (uint32_t i = 0U; i < count; ++i)
            {
                lua_rawgeti(L, 1, (int)(i + 1U));
                const uint16_t agent_id = (uint16_t)lua_tointeger(L, -1);
                lua_pop(L, 1);

                const crowd::Vec2& position = crowd::get_agent_position(crowd_context, agent_id);
                const crowd::Vec2& velocity = crowd::get_agent_velocity(crowd_context, agent_id);

                lua_pushnumber(L, position.x);
                lua_rawseti(L, 3, (int)(i + 1U));
                lua_pushnumber(L, position.y);
                lua_rawseti(L, 4, (int)(i + 1U));
                lua_pushnumber(L, velocity.x);
                lua_rawseti(L, 5, (int)(i + 1U));
                lua_pushnumber(L, velocity.y);
                lua_rawseti(L, 6, (int)(i + 1U));
            }

            return 0;
        }

        int get_agent_radius(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);
            uint16_t    agent_id = luaL_checkinteger(L, 1);
            const float radius = crowd::get_agent_radius(crowd_context, agent_id);

            lua_pushnumber(L, radius);

            return 1;
        }

        int get_agent_neighbor_dist(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);
            uint16_t    agent_id = luaL_checkinteger(L, 1);
            const float dist = crowd::get_agent_neighbor_dist(crowd_context, agent_id);

            lua_pushnumber(L, dist);

            return 1;
        }

        int get_agent_max_neighbors(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);
            uint16_t      agent_id = luaL_checkinteger(L, 1);
            const uint8_t neighbors = crowd::get_agent_max_neighbors(crowd_context, agent_id);

            lua_pushinteger(L, neighbors);

            return 1;
        }

        int get_agent_time_horizon(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);
            uint16_t    agent_id = luaL_checkinteger(L, 1);
            const float time_horizon = crowd::get_agent_time_horizon(crowd_context, agent_id);

            lua_pushnumber(L, time_horizon);

            return 1;
        }

        int get_agent_time_horizon_obstacle(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);
            uint16_t    agent_id = luaL_checkinteger(L, 1);
            const float time_horizon_obstacle = crowd::get_agent_time_horizon_obstacle(crowd_context, agent_id);

            lua_pushnumber(L, time_horizon_obstacle);

            return 1;
        }

        int get_agent_nav_mask(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);
            uint16_t      agent_id = luaL_checkinteger(L, 1);
            const uint8_t mask = crowd::get_agent_nav_mask(crowd_context, agent_id);

            lua_pushinteger(L, mask);

            return 1;
        }

        int get_obstacle_nav_mask(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);
            uint16_t      obstacle_id = luaL_checkinteger(L, 1);
            const uint8_t mask = crowd::get_obstacle_nav_mask(crowd_context, obstacle_id);

            lua_pushinteger(L, mask);

            return 1;
        }

        int get_circle_obstacle_nav_mask(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);
            uint16_t      obstacle_id = luaL_checkinteger(L, 1);
            const uint8_t mask = crowd::get_circle_obstacle_nav_mask(crowd_context, obstacle_id);

            lua_pushinteger(L, mask);

            return 1;
        }

        int set_agent_avoidance_weight(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);

            uint16_t agent_id = luaL_checkinteger(L, 1);
            float    weight = (float)luaL_checknumber(L, 2);
            crowd::set_agent_avoidance_weight(crowd_context, agent_id, weight);

            return 0;
        }

        int get_agent_avoidance_weight(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);
            uint16_t    agent_id = luaL_checkinteger(L, 1);
            const float avoidance_weight = crowd::get_agent_avoidance_weight(crowd_context, agent_id);

            lua_pushnumber(L, avoidance_weight);

            return 1;
        }

        int set_agent_avoidance_priority(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);

            uint16_t agent_id = luaL_checkinteger(L, 1);
            uint8_t  priority = (uint8_t)luaL_checkinteger(L, 2);
            crowd::set_agent_avoidance_priority(crowd_context, agent_id, priority);

            return 0;
        }

        int get_agent_avoidance_priority(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);
            uint16_t      agent_id = luaL_checkinteger(L, 1);
            const uint8_t priority = crowd::get_agent_avoidance_priority(crowd_context, agent_id);

            lua_pushinteger(L, priority);

            return 1;
        }

        int set_agent_avoidance_locked(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);

            uint16_t agent_id = luaL_checkinteger(L, 1);
            bool     locked = (bool)lua_toboolean(L, 2);

            crowd::set_agent_avoidance_locked(crowd_context, agent_id, locked);

            return 0;
        }

        int get_agent_avoidance_locked(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);
            uint16_t   agent_id = luaL_checkinteger(L, 1);
            const bool locked = crowd::get_agent_avoidance_locked(crowd_context, agent_id);

            lua_pushboolean(L, locked);

            return 1;
        }

        int set_agent_kinematic(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);

            uint16_t agent_id = luaL_checkinteger(L, 1);
            bool     is_kinematic = (bool)lua_toboolean(L, 2);

            crowd::set_agent_kinematic(crowd_context, agent_id, is_kinematic);

            return 0;
        }

        int get_agent_kinematic(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);
            uint16_t   agent_id = luaL_checkinteger(L, 1);
            const bool is_kinematic = crowd::get_agent_kinematic(crowd_context, agent_id);

            lua_pushboolean(L, is_kinematic);

            return 1;
        }

        int get_agent_num_agent_neighbors(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);
            uint16_t       agent_id = luaL_checkinteger(L, 1);
            const uint16_t agent_neighbors = crowd::get_agent_num_agent_neighbors(crowd_context, agent_id);

            lua_pushinteger(L, agent_neighbors);

            return 1;
        }

        int get_agent_num_obstacle_neighbors(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);
            uint16_t       agent_id = luaL_checkinteger(L, 1);
            const uint16_t obstacle_neighbors = crowd::get_agent_num_obstacle_neighbors(crowd_context, agent_id);

            lua_pushinteger(L, obstacle_neighbors);

            return 1;
        }

        int get_agent_num_circle_obstacle_neighbors(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);
            uint16_t       agent_id = luaL_checkinteger(L, 1);
            const uint16_t circle_obstacle_neighbors = crowd::get_agent_num_circle_obstacle_neighbors(crowd_context, agent_id);

            lua_pushinteger(L, circle_obstacle_neighbors);

            return 1;
        }

        int get_agent_num_orca_lines(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);
            uint16_t       agent_id = luaL_checkinteger(L, 1);
            const uint16_t num_orca_lines = crowd::get_agent_num_orca_lines(crowd_context, agent_id);

            lua_pushinteger(L, num_orca_lines);

            return 1;
        }

        // Indexed neighbor / ORCA-line accessors
        int get_agent_agent_neighbor(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);
            uint16_t       agent_id = luaL_checkinteger(L, 1);
            uint16_t       neighbor_id = luaL_checkinteger(L, 2);
            const uint16_t agent_neighbor = crowd::get_agent_agent_neighbor(crowd_context, agent_id, neighbor_id);

            lua_pushinteger(L, agent_neighbor);

            return 1;
        }

        int get_agent_obstacle_neighbor(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);
            uint16_t       agent_id = luaL_checkinteger(L, 1);
            uint16_t       neighbor_id = luaL_checkinteger(L, 2);
            const uint16_t obstacle_neighbor = crowd::get_agent_obstacle_neighbor(crowd_context, agent_id, neighbor_id);

            lua_pushinteger(L, obstacle_neighbor);

            return 1;
        }

        int get_agent_circle_obstacle_neighbor(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);
            uint16_t       agent_id = luaL_checkinteger(L, 1);
            uint16_t       neighbor_id = luaL_checkinteger(L, 2);
            const uint16_t circle_obstacle_neighbor = crowd::get_agent_circle_obstacle_neighbor(crowd_context, agent_id, neighbor_id);

            lua_pushinteger(L, circle_obstacle_neighbor);

            return 1;
        }

        int get_agent_orca_line(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 4);
            uint16_t       agent_id = luaL_checkinteger(L, 1);
            uint16_t       line_id = luaL_checkinteger(L, 2);
            const OrcaLine orca_line = crowd::get_agent_orca_line(crowd_context, agent_id, line_id);

            lua_pushnumber(L, orca_line.point.x);
            lua_pushnumber(L, orca_line.point.y);
            lua_pushnumber(L, orca_line.direction.x);
            lua_pushnumber(L, orca_line.direction.y);

            return 4;
        }

        // Obstacle vertex accessors
        int get_obstacle_vertex(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 2);
            uint16_t          vertex_id = luaL_checkinteger(L, 1);

            const crowd::Vec2 obstacle_vertex = crowd::get_obstacle_vertex(crowd_context, vertex_id);

            lua_pushnumber(L, obstacle_vertex.x);
            lua_pushnumber(L, obstacle_vertex.y);

            return 2;
        }

        int get_next_obstacle_vertex_id(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);
            uint16_t       vertex_id = luaL_checkinteger(L, 1);

            const uint16_t obstacle_vertex_id = crowd::get_next_obstacle_vertex_id(crowd_context, vertex_id);

            lua_pushinteger(L, obstacle_vertex_id);

            return 1;
        }

        int get_prev_obstacle_vertex_id(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);
            uint16_t       vertex_id = luaL_checkinteger(L, 1);

            const uint16_t obstacle_vertex_id = crowd::get_prev_obstacle_vertex_id(crowd_context, vertex_id);

            lua_pushinteger(L, obstacle_vertex_id);

            return 1;
        }

        int get_num_obstacle_vertices(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);

            lua_pushinteger(L, crowd::get_num_obstacle_vertices(crowd_context));

            return 1;
        }

        // Simulation time
        int set_time_step(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);

            const float time_step = luaL_checknumber(L, 1);

            crowd::set_time_step(crowd_context, time_step);

            return 0;
        }

        int get_time_step(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);

            const float time_step = crowd::get_time_step(crowd_context);

            lua_pushnumber(L, time_step);

            return 1;
        }

        int get_global_time(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 1);

            const float global_time = crowd::get_global_time(crowd_context);

            lua_pushnumber(L, global_time);

            return 1;
        }

        int do_step(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);
            crowd::do_step(crowd_context);
            return 0;
        }

        int do_step_parallel(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);

#ifdef CROWD_HAS_JOBSYSTEM
            if (job_system)
                crowd::do_step_parallel(crowd_context, job_system);
            else
                crowd::do_step(crowd_context);
#else
            crowd::do_step(crowd_context);
#endif

            return 0;
        }

        int query_agents_in_radius(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 2);

            float   x = (float)luaL_checknumber(L, 1);
            float   y = (float)luaL_checknumber(L, 2);
            float   radius = (float)luaL_checknumber(L, 3);
            uint8_t nav_mask = (uint8_t)luaL_optinteger(L, 4, crowd::NAV_MASK_ALL);

            if (agent_query_scratch.Capacity() == 0U)
            {
                dmLogError("query_agents_in_radius: scratch buffer not initialized");
                lua_createtable(L, 0, 0);
                lua_pushinteger(L, 0);
                return 2;
            }

            crowd::AgentNeighbor* results = agent_query_scratch.Begin();
            const uint16_t        count = crowd::query_agents_in_radius(crowd_context, crowd::Vec2(x, y), radius, nav_mask, results, (uint16_t)agent_query_scratch.Capacity());

            lua_createtable(L, count, 0);
            for (uint16_t i = 0U; i < count; ++i)
            {
                lua_pushinteger(L, results[i].agent_id);
                lua_rawseti(L, -2, (int)(i + 1U));
            }

            lua_pushinteger(L, (lua_Integer)count);
            return 2;
        }

        int set_max_agent_query(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);

            uint16_t count = (uint16_t)luaL_checkinteger(L, 1);

            if (!crowd_context)
            {
                dmLogError("Can't set max agent query without a crowd context");
                return 0;
            }

            reset_agent_query_scratch();
            set_agent_query_scratch_capacity(count, "set_max_agent_query");

            return 0;
        }

        int raycast_agents(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 2);

            float   start_x = (float)luaL_checknumber(L, 1);
            float   start_y = (float)luaL_checknumber(L, 2);
            float   end_x = (float)luaL_checknumber(L, 3);
            float   end_y = (float)luaL_checknumber(L, 4);
            float   radius = (float)luaL_checknumber(L, 5);
            uint8_t nav_mask = (uint8_t)luaL_optinteger(L, 6, crowd::NAV_MASK_ALL);

            if (agent_raycast_scratch.Capacity() == 0U)
            {
                dmLogError("raycast_agents: scratch buffer not initialized");
                lua_createtable(L, 0, 0);
                lua_pushinteger(L, 0);
                return 2;
            }

            crowd::AgentRaycastHit* results = agent_raycast_scratch.Begin();
            const uint16_t          count = crowd::raycast_agents(crowd_context, crowd::Vec2(start_x, start_y), crowd::Vec2(end_x, end_y), radius, nav_mask, results, (uint16_t)agent_raycast_scratch.Capacity());

            lua_createtable(L, count, 0);
            for (uint16_t i = 0U; i < count; ++i)
            {
                lua_createtable(L, 0, 2);
                lua_pushinteger(L, results[i].agent_id);
                lua_setfield(L, -2, "id");
                lua_pushnumber(L, results[i].t);
                lua_setfield(L, -2, "t");
                lua_rawseti(L, -2, (int)(i + 1U));
            }

            lua_pushinteger(L, (lua_Integer)count);
            return 2;
        }

        int set_max_raycast_query(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);

            uint16_t count = (uint16_t)luaL_checkinteger(L, 1);

            if (!crowd_context)
            {
                dmLogError("Can't set max raycast query without a crowd context");
                return 0;
            }

            reset_agent_raycast_scratch();
            set_agent_raycast_scratch_capacity(count, "set_max_raycast_query");

            return 0;
        }

        int query_circle_obstacles_in_radius(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 2);

            float   x = (float)luaL_checknumber(L, 1);
            float   y = (float)luaL_checknumber(L, 2);
            float   radius = (float)luaL_checknumber(L, 3);
            uint8_t nav_mask = (uint8_t)luaL_optinteger(L, 4, crowd::NAV_MASK_ALL);

            if (!crowd_context || crowd_context->max_circle_obstacles == 0U)
            {
                lua_createtable(L, 0, 0);
                lua_pushinteger(L, 0);
                return 2;
            }

            if (circle_query_scratch.Capacity() == 0U)
            {
                dmLogError("query_circle_obstacles_in_radius: scratch buffer not initialized");
                lua_createtable(L, 0, 0);
                lua_pushinteger(L, 0);
                return 2;
            }

            crowd::CircleObstacleNeighbor* results = circle_query_scratch.Begin();
            const uint16_t                 count = crowd::query_circle_obstacles_in_radius(crowd_context, crowd::Vec2(x, y), radius, nav_mask, results, (uint16_t)circle_query_scratch.Capacity());

            lua_createtable(L, count, 0);
            for (uint16_t i = 0U; i < count; ++i)
            {
                lua_pushinteger(L, results[i].obs_id);
                lua_rawseti(L, -2, (int)(i + 1U));
            }

            lua_pushinteger(L, (lua_Integer)count);
            return 2;
        }

        int query_circle_obstacles_colliding(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 2);

            float   x = (float)luaL_checknumber(L, 1);
            float   y = (float)luaL_checknumber(L, 2);
            float   radius = (float)luaL_checknumber(L, 3);
            uint8_t nav_mask = (uint8_t)luaL_optinteger(L, 4, crowd::NAV_MASK_ALL);

            if (!crowd_context || crowd_context->max_circle_obstacles == 0U)
            {
                lua_createtable(L, 0, 0);
                lua_pushinteger(L, 0);
                return 2;
            }

            if (circle_query_scratch.Capacity() == 0U)
            {
                dmLogError("query_circle_obstacles_colliding: scratch buffer not initialized");
                lua_createtable(L, 0, 0);
                lua_pushinteger(L, 0);
                return 2;
            }

            crowd::CircleObstacleNeighbor* results = circle_query_scratch.Begin();
            const uint16_t                 count = crowd::query_circle_obstacles_colliding(crowd_context, crowd::Vec2(x, y), radius, nav_mask, results, (uint16_t)circle_query_scratch.Capacity());

            lua_createtable(L, count, 0);
            for (uint16_t i = 0U; i < count; ++i)
            {
                lua_pushinteger(L, results[i].obs_id);
                lua_rawseti(L, -2, (int)(i + 1U));
            }

            lua_pushinteger(L, (lua_Integer)count);
            return 2;
        }

        int set_max_circle_query(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);

            uint16_t count = (uint16_t)luaL_checkinteger(L, 1);

            if (!crowd_context)
            {
                dmLogError("Can't set max circle query without a crowd context");
                return 0;
            }

            reset_circle_query_scratch();

            const uint16_t clamped_count = count > crowd_context->max_circle_obstacles ? crowd_context->max_circle_obstacles : count;

            set_circle_query_scratch_capacity(clamped_count, "set_max_circle_query");

            return 0;
        }

        int raycast_circle_obstacles(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 2);

            float   start_x = (float)luaL_checknumber(L, 1);
            float   start_y = (float)luaL_checknumber(L, 2);
            float   end_x = (float)luaL_checknumber(L, 3);
            float   end_y = (float)luaL_checknumber(L, 4);
            float   radius = (float)luaL_checknumber(L, 5);
            uint8_t nav_mask = (uint8_t)luaL_optinteger(L, 6, crowd::NAV_MASK_ALL);

            if (!crowd_context || crowd_context->max_circle_obstacles == 0U)
            {
                lua_createtable(L, 0, 0);
                lua_pushinteger(L, 0);
                return 2;
            }

            if (circle_raycast_scratch.Capacity() == 0U)
            {
                dmLogError("raycast_circle_obstacles: scratch buffer not initialized");
                lua_createtable(L, 0, 0);
                lua_pushinteger(L, 0);
                return 2;
            }

            crowd::CircleObstacleRaycastHit* results = circle_raycast_scratch.Begin();
            const uint16_t                   count = crowd::raycast_circle_obstacles(crowd_context, crowd::Vec2(start_x, start_y), crowd::Vec2(end_x, end_y), radius, nav_mask, results, (uint16_t)circle_raycast_scratch.Capacity());

            lua_createtable(L, count, 0);
            for (uint16_t i = 0U; i < count; ++i)
            {
                lua_createtable(L, 0, 2);
                lua_pushinteger(L, results[i].obs_id);
                lua_setfield(L, -2, "id");
                lua_pushnumber(L, results[i].t);
                lua_setfield(L, -2, "t");
                lua_rawseti(L, -2, (int)(i + 1U));
            }

            lua_pushinteger(L, (lua_Integer)count);
            return 2;
        }

        int set_max_circle_raycast_query(lua_State* L)
        {
            DM_LUA_STACK_CHECK(L, 0);

            uint16_t count = (uint16_t)luaL_checkinteger(L, 1);

            if (!crowd_context)
            {
                dmLogError("Can't set max circle raycast query without a crowd context");
                return 0;
            }

            reset_circle_raycast_scratch();

            const uint16_t clamped_count = count > crowd_context->max_circle_obstacles ? crowd_context->max_circle_obstacles : count;

            set_circle_raycast_scratch_capacity(clamped_count, "set_max_circle_raycast_query");

            return 0;
        }

        void shutdown()
        {
            reset_gameobject_update(false);

#ifdef CROWD_HAS_JOBSYSTEM
            if (job_system)
            {
                JobSystemDestroy(job_system);
                job_system = NULL;
            }
#endif

            if (crowd_context)
            {
                crowd::destroy_context(crowd_context);
                crowd_context = NULL;
            }

            reset_agent_query_scratch();
            reset_agent_raycast_scratch();
            reset_circle_query_scratch();
            reset_circle_raycast_scratch();

            crowd::unit::shutdown();
        }

        void set_update_frequency(uint8_t frequency)
        {
            game_update.update_frequency = frequency;
        }

        void set_max_time_step(float max_time_step)
        {
            game_update.max_time_step = max_time_step;
        }

        void gameobject_update()
        {
            // If paused or not set
            if (!game_update.update_loop_state || !crowd_context || unit::get_count() == 0)
            {
                return;
            }

            float    step_dt;   // The dt for each step (the game frame)
            uint32_t num_steps; // Number of times to loop over the StepFrame function

            calc_time_step(step_dt, num_steps);

            for (uint32_t i = 0; i < num_steps; ++i)
            {
                crowd::set_time_step(crowd_context, step_dt);

                unit::set_preferred_velocities(crowd_context, step_dt);

#ifdef CROWD_HAS_JOBSYSTEM
                if (job_system)
                    crowd::do_step_parallel(crowd_context, job_system);
                else
                    crowd::do_step(crowd_context);
#else
                crowd::do_step(crowd_context);
#endif
                unit::update(crowd_context, step_dt);
            }
        }

    } // namespace manager

} // namespace crowd
