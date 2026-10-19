#include "unit.h"

#include "crowd.h"
#include "crowd/math.h"
#include "crowd/types.h"
#include "dmsdk/dlib/array.h"
#include "dmsdk/dlib/vmath.h"
#include "dmsdk/gameobject/gameobject.h"

namespace crowd
{
    namespace unit
    {
        static const uint16_t    UNIT_INDEX_NONE = 0xFFFFU;

        static dmArray<Unit>     units;
        static dmArray<uint16_t> unit_lookup;
        static dmArray<uint16_t> pending_deletes;
        static bool              initialized = false;
        static const float       ARRIVAL_RADIUS = 2.0;

        static bool              validate_gameobject(CrowdContext* crowd_context, const Unit* unit)
        {
            if (dmGameObject::GetInstanceFromIdentifier(unit->collection, unit->identifier) != 0)
            {
                return true;
            }

            dmLogError("Game object was deleted without calling crowd.remove_agent(). Identifier: %s - Agent ID: %u. Auto-removing.", dmHashReverseSafe64(unit->identifier), (uint32_t)unit->agent_id);
            mark_for_delete(unit->agent_id);
            crowd::remove_agent(crowd_context, unit->agent_id);
            return false;
        }

        const char* result_to_string(UnitResult result)
        {
            switch (result)
            {
                case UNIT_RESULT_OK:
                    return "OK";
                case UNIT_RESULT_NOT_INITIALIZED:
                    return "not initialized";
                case UNIT_RESULT_INVALID_AGENT_ID:
                    return "invalid agent ID";
                case UNIT_RESULT_FULL:
                    return "unit capacity exhausted";
                case UNIT_RESULT_ALREADY_EXISTS:
                    return "unit already exists";
                case UNIT_RESULT_NOT_FOUND:
                    return "unit not found";
            }
            return "unknown";
        }

        void shutdown()
        {
            units.SetSize(0U);
            units.SetCapacity(0U);

            unit_lookup.SetSize(0U);
            unit_lookup.SetCapacity(0U);

            pending_deletes.SetSize(0U);
            pending_deletes.SetCapacity(0U);

            initialized = false;
        }

        void init(uint16_t max_units)
        {
            shutdown();

            units.SetCapacity(max_units);

            unit_lookup.SetCapacity(max_units);
            unit_lookup.SetSize(max_units);

            for (uint32_t i = 0U; i < max_units; ++i)
            {
                unit_lookup[i] = UNIT_INDEX_NONE;
            }

            pending_deletes.SetCapacity(max_units);
            initialized = true;
        }

        UnitResult add(const Unit& unit)
        {
            if (!initialized)
            {
                return UNIT_RESULT_NOT_INITIALIZED;
            }

            if (unit.agent_id >= unit_lookup.Size())
            {
                return UNIT_RESULT_INVALID_AGENT_ID;
            }

            if (unit_lookup[unit.agent_id] != UNIT_INDEX_NONE)
            {
                return UNIT_RESULT_ALREADY_EXISTS;
            }

            if (units.Full())
            {
                return UNIT_RESULT_FULL;
            }

            Unit stored_unit = unit;
            stored_unit.pending_delete = false;

            const uint16_t dense_index = (uint16_t)units.Size();
            units.Push(stored_unit);
            unit_lookup[stored_unit.agent_id] = dense_index;

            return UNIT_RESULT_OK;
        }

        UnitResult remove(uint16_t agent_id)
        {
            if (!initialized)
            {
                return UNIT_RESULT_NOT_INITIALIZED;
            }

            if (agent_id >= unit_lookup.Size())
            {
                return UNIT_RESULT_INVALID_AGENT_ID;
            }

            const uint16_t dense_index = unit_lookup[agent_id];
            if (dense_index == UNIT_INDEX_NONE)
            {
                return UNIT_RESULT_NOT_FOUND;
            }

            const uint16_t moved_agent_id = units.Back().agent_id;
            units.EraseSwap(dense_index);

            if (dense_index < units.Size())
            {
                unit_lookup[moved_agent_id] = dense_index;
            }

            unit_lookup[agent_id] = UNIT_INDEX_NONE;
            return UNIT_RESULT_OK;
        }

        Unit* get(uint16_t agent_id)
        {
            if (!initialized || agent_id >= unit_lookup.Size())
            {
                return NULL;
            }

            const uint16_t dense_index = unit_lookup[agent_id];
            if (dense_index == UNIT_INDEX_NONE)
            {
                return NULL;
            }

            return &units[dense_index];
        }

        Unit* get_at(uint32_t dense_index)
        {
            if (!initialized || dense_index >= units.Size())
            {
                return NULL;
            }

            return &units[dense_index];
        }

        uint32_t get_count()
        {
            return initialized ? units.Size() : 0U;
        }

        UnitResult set_goal(uint16_t agent_id, const crowd::Vec2& goal)
        {
            if (!initialized)
            {
                return UNIT_RESULT_NOT_INITIALIZED;
            }

            if (agent_id >= unit_lookup.Size())
            {
                return UNIT_RESULT_INVALID_AGENT_ID;
            }

            Unit* unit = get(agent_id);
            if (!unit)
            {
                return UNIT_RESULT_NOT_FOUND;
            }

            unit->goal = goal;
            unit->has_goal = 1U;
            return UNIT_RESULT_OK;
        }

        UnitResult clear_goal(uint16_t agent_id)
        {
            if (!initialized)
            {
                return UNIT_RESULT_NOT_INITIALIZED;
            }

            if (agent_id >= unit_lookup.Size())
            {
                return UNIT_RESULT_INVALID_AGENT_ID;
            }

            Unit* unit = get(agent_id);
            if (!unit)
            {
                return UNIT_RESULT_NOT_FOUND;
            }

            unit->has_goal = 0U;
            return UNIT_RESULT_OK;
        }

        UnitResult mark_for_delete(uint16_t agent_id)
        {
            if (!initialized)
            {
                return UNIT_RESULT_NOT_INITIALIZED;
            }

            if (agent_id >= unit_lookup.Size())
            {
                return UNIT_RESULT_INVALID_AGENT_ID;
            }

            Unit* unit = get(agent_id);
            if (!unit)
            {
                return UNIT_RESULT_NOT_FOUND;
            }

            if (unit->pending_delete != false)
            {
                return UNIT_RESULT_OK;
            }

            unit->pending_delete = true;
            pending_deletes.Push(agent_id);
            return UNIT_RESULT_OK;
        }

        bool pop_pending_delete(uint16_t* agent_id)
        {
            if (!initialized || !agent_id)
            {
                return false;
            }

            while (!pending_deletes.Empty())
            {
                const uint16_t pending_agent_id = pending_deletes.Back();
                pending_deletes.Pop();

                Unit* unit = get(pending_agent_id);
                if (unit && unit->pending_delete != false)
                {
                    *agent_id = pending_agent_id;
                    return true;
                }
            }

            return false;
        }

        void set_preferred_velocities(CrowdContext* crowd_context, float step_dt)
        {
            for (uint32_t i = 0U; i < unit::get_count(); ++i)
            {
                const unit::Unit* current_unit = unit::get_at(i);

                if (current_unit->pending_delete != false)
                {
                    continue;
                }

                if (!validate_gameobject(crowd_context, current_unit))
                {
                    continue;
                }

                Vec2 pref_velocity(0.0f, 0.0f);

                if (current_unit->has_goal != false)
                {
                    const Vec2  goal_direction = math::subtract(current_unit->goal, crowd::get_agent_position(crowd_context, current_unit->agent_id));
                    const float distance = math::abs(goal_direction);

                    if (distance > ARRIVAL_RADIUS)
                    {
                        const float agent_max_speed = crowd::get_agent_max_speed(crowd_context, current_unit->agent_id);

                        if (distance < (agent_max_speed * step_dt))
                        {
                            pref_velocity = math::multiply(math::normalize(goal_direction), distance / step_dt);
                        }
                        else
                        {
                            pref_velocity = math::multiply(math::normalize(goal_direction), agent_max_speed);
                        }
                    }
                }

                crowd::set_agent_pref_velocity(crowd_context, current_unit->agent_id, pref_velocity);
            }
        }

        void update(CrowdContext* crowd_context, float step_dt)
        {
            uint16_t pending_agent_id = AGENT_ID_NONE;
            while (crowd::unit::pop_pending_delete(&pending_agent_id))
            {
                crowd::unit::remove(pending_agent_id);
            }

            for (uint32_t i = 0U; i < unit::get_count(); ++i)
            {
                unit::Unit* current_unit = unit::get_at(i);

                if (current_unit->pending_delete != false)
                {
                    continue;
                }

                if (!validate_gameobject(crowd_context, current_unit))
                {
                    continue;
                }

                const Vec2 position = crowd::get_agent_position(crowd_context, current_unit->agent_id);
                const Vec2 velocity = crowd::get_agent_velocity(crowd_context, current_unit->agent_id);

                if (math::abs_sq(velocity) > 0.001f)
                {
                    current_unit->rotation = atan2f(velocity.y, velocity.x);
                    dmGameObject::SetRotation(current_unit->instance, dmVMath::QuatFromAngle(2U, current_unit->rotation));
                }

                dmGameObject::SetPosition(current_unit->instance, dmVMath::Point3(position.x, position.y, current_unit->position_z));
            }
        }
    } // namespace unit
} // namespace crowd
