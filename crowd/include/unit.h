#pragma once

#include "crowd/types.h"
#include <cstdint>
#include <dmsdk/sdk.h>

namespace crowd
{
    namespace unit
    {
        struct Unit
        {
            dmGameObject::HInstance   instance;
            dmGameObject::HCollection collection;
            dmhash_t                  identifier;

            crowd::Vec2               goal;
            float                     position_z;
            float                     rotation;

            uint16_t                  agent_id;
            bool                      pending_delete;
            bool                      has_goal;
        };

        enum UnitResult
        {
            UNIT_RESULT_OK,
            UNIT_RESULT_NOT_INITIALIZED,
            UNIT_RESULT_INVALID_AGENT_ID,
            UNIT_RESULT_FULL,
            UNIT_RESULT_ALREADY_EXISTS,
            UNIT_RESULT_NOT_FOUND
        };

        const char* result_to_string(UnitResult result);

        void        init(uint16_t max_units);
        void        shutdown();

        UnitResult  add(const Unit& unit);
        UnitResult  remove(uint16_t agent_id);

        Unit*       get(uint16_t agent_id);
        Unit*       get_at(uint32_t dense_index);
        uint32_t    get_count();

        UnitResult  set_goal(uint16_t agent_id, const crowd::Vec2& goal);
        UnitResult  clear_goal(uint16_t agent_id);

        UnitResult  mark_for_delete(uint16_t agent_id);
        bool        pop_pending_delete(uint16_t* agent_id);

        // Update
        void set_preferred_velocities(CrowdContext* crowd_context, float step_dt);
        void update(CrowdContext* crowd_context, float step_dt);

    } // namespace unit
} // namespace crowd
