---Defold Crowd Extension
---C-style ORCA crowd simulation with polygon and circle obstacle support.
---@class crowd
---@field INVALID_ID number Invalid ID sentinel (65535). Returned by add and lookup functions on failure.
crowd = {}
crowd.INVALID_ID = 65535

---@class CrowdObstacleVertex
---@field x number X coordinate of the obstacle vertex
---@field y number Y coordinate of the obstacle vertex

---Initialize the crowd simulation. Replaces any existing simulation context.
---@param max_agents number Maximum number of agent slots
---@param max_neighbors number Maximum number of agent neighbors
---@param max_obstacle_vertices number|nil Maximum source polygon vertices (optional, default: 0)
---@param max_circle_obstacles number|nil Maximum circle obstacles (optional, default: 0)
---@param max_workers number|nil Maximum simulation lanes including the main thread, from 1 to 255 (optional, default: 1)
function crowd.init(max_agents, max_neighbors, max_obstacle_vertices, max_circle_obstacles, max_workers) end

---Destroy the crowd simulation and release its resources.
function crowd.destroy() end

---Add an agent to the simulation.
---@param x number Initial X coordinate
---@param y number Initial Y coordinate
---@param radius number Agent radius
---@param max_speed number Maximum movement speed
---@param max_neighbors number Maximum neighbors considered by this agent
---@param neighbor_distance number Maximum neighbor query distance
---@param time_horizon number Agent collision-avoidance time horizon
---@param time_horizon_obstacle number Obstacle collision-avoidance time horizon
---@return number agent_id Agent ID, or crowd.INVALID_ID if capacity is exhausted
function crowd.add_agent(x, y, radius, max_speed, max_neighbors, neighbor_distance, time_horizon, time_horizon_obstacle) end

---Remove an agent and make its slot available for reuse.
---For game-object-backed agents, native tracking is removed on the next update; the Defold game object remains owned by the caller and is not deleted.
---@param agent_id number Agent ID
---@return boolean removed True when the agent was removed
function crowd.remove_agent(agent_id) end

---Initialize native game-object tracking after `crowd.init()`.
---Calling this while game objects are already tracked is rejected so their mappings are preserved.
---This does not start simulation; call `crowd.run_simulation(true)` when setup is complete.
function crowd.init_gameobjects() end

---Add a game object and its crowd agent. The instance URL must refer to a game object in the same collection.
---The native update loop writes the simulated XY position and rotation while preserving the initial Z position.
---@param instance url Game-object URL in the calling script's collection
---@param radius number Agent radius
---@param max_speed number Maximum movement speed
---@param max_neighbors number Maximum neighbors considered by this agent
---@param neighbor_distance number Maximum neighbor query distance
---@param time_horizon number Agent collision-avoidance time horizon
---@param time_horizon_obstacle number Obstacle collision-avoidance time horizon
---@return number agent_id Agent ID, or crowd.INVALID_ID if the crowd agent or tracking record could not be added
function crowd.add_gameobject(instance, radius, max_speed, max_neighbors, neighbor_distance, time_horizon, time_horizon_obstacle) end

---Set the movement goal for a tracked game object. Invalid IDs are logged by the extension.
---@param agent_id number Agent ID returned by `crowd.add_gameobject()`
---@param x number Goal X coordinate
---@param y number Goal Y coordinate
function crowd.set_gameobject_goal(agent_id, x, y) end

---Clear the movement goal for a tracked game object. Invalid IDs are logged by the extension.
---@param agent_id number Agent ID returned by `crowd.add_gameobject()`
function crowd.remove_gameobject_goal(agent_id) end

---Get the number of dense game-object tracking records.
---Records awaiting deferred removal remain counted until the next native update. Removing an agent stops tracking but does not delete its Defold game object.
---@return number gameobject_count Number of tracked records, including records awaiting deferred removal
function crowd.get_gameobject_count() end

---Enable or pause native simulation and automatic tracked game-object transform updates.
---Changing the state resets native frame timing, so paused time is not processed when simulation resumes.
---@param enabled boolean True to run simulation; false to pause it
function crowd.run_simulation(enabled) end

---Add a closed polygon obstacle. Call `crowd.process_obstacles()` afterwards.
---@param vertices CrowdObstacleVertex[] Ordered obstacle vertices
---@return number obstacle_id First obstacle vertex ID, or crowd.INVALID_ID on failure
function crowd.add_obstacle(vertices) end

---Remove a polygon obstacle. Call `crowd.process_obstacles()` afterwards.
---@param obstacle_id number First vertex ID returned by `crowd.add_obstacle()`
---@return boolean removed True when the obstacle was removed
function crowd.remove_obstacle(obstacle_id) end

---Rebuild the polygon obstacle search tree after adding or removing obstacles.
function crowd.process_obstacles() end

---Add a circle obstacle. Call `crowd.process_circle_obstacles()` afterwards.
---@param x number Center X coordinate
---@param y number Center Y coordinate
---@param radius number Circle radius
---@return number obstacle_id Circle obstacle ID, or crowd.INVALID_ID on failure
function crowd.add_circle_obstacle(x, y, radius) end

---Remove a circle obstacle. Call `crowd.process_circle_obstacles()` afterwards.
---@param obstacle_id number Circle obstacle ID
---@return boolean removed True when the obstacle was removed
function crowd.remove_circle_obstacle(obstacle_id) end

---Rebuild the circle obstacle search tree after adding or removing obstacles.
function crowd.process_circle_obstacles() end

---Get the number of allocated agent slots.
---@return number agent_count Number of allocated agent slots
function crowd.get_agent_count() end

---Get the number of internal polygon obstacle records.
---@return number obstacle_count Number of polygon obstacle records
function crowd.get_obstacle_count() end

---Set an agent's preferred velocity.
---@param agent_id number Agent ID
---@param x number Preferred velocity X component
---@param y number Preferred velocity Y component
function crowd.set_agent_pref_velocity(agent_id, x, y) end

---Set an agent's position.
---@param agent_id number Agent ID
---@param x number Position X coordinate
---@param y number Position Y coordinate
function crowd.set_agent_position(agent_id, x, y) end

---Set an agent's current velocity.
---@param agent_id number Agent ID
---@param x number Velocity X component
---@param y number Velocity Y component
function crowd.set_agent_velocity(agent_id, x, y) end

---Set an agent's maximum speed.
---@param agent_id number Agent ID
---@param max_speed number Maximum movement speed
function crowd.set_agent_max_speed(agent_id, max_speed) end

---Set an agent's radius.
---@param agent_id number Agent ID
---@param radius number Agent radius
function crowd.set_agent_radius(agent_id, radius) end

---Set an agent's neighbor query distance.
---@param agent_id number Agent ID
---@param neighbor_distance number Maximum neighbor query distance
function crowd.set_agent_neighbor_dist(agent_id, neighbor_distance) end

---Set the maximum number of neighbors considered by an agent.
---@param agent_id number Agent ID
---@param max_neighbors number Maximum neighbor count
function crowd.set_agent_max_neighbors(agent_id, max_neighbors) end

---Set an agent's collision-avoidance time horizon.
---@param agent_id number Agent ID
---@param time_horizon number Agent time horizon
function crowd.set_agent_time_horizon(agent_id, time_horizon) end

---Set an agent's obstacle collision-avoidance time horizon.
---@param agent_id number Agent ID
---@param time_horizon_obstacle number Obstacle time horizon
function crowd.set_agent_time_horizon_obstacle(agent_id, time_horizon_obstacle) end

---Set an agent's navigation mask.
---@param agent_id number Agent ID
---@param nav_mask number Navigation mask (0-255)
function crowd.set_agent_nav_mask(agent_id, nav_mask) end

---Set a polygon obstacle's navigation mask.
---@param obstacle_id number First vertex ID of the polygon obstacle
---@param nav_mask number Navigation mask (0-255)
function crowd.set_obstacle_nav_mask(obstacle_id, nav_mask) end

---Set a circle obstacle's navigation mask.
---@param obstacle_id number Circle obstacle ID
---@param nav_mask number Navigation mask (0-255)
function crowd.set_circle_obstacle_nav_mask(obstacle_id, nav_mask) end

---Set an agent's avoidance weight.
---@param agent_id number Agent ID
---@param weight number Avoidance responsibility weight
function crowd.set_agent_avoidance_weight(agent_id, weight) end

---Set an agent's avoidance priority.
---@param agent_id number Agent ID
---@param priority number Avoidance priority (0-255)
function crowd.set_agent_avoidance_priority(agent_id, priority) end

---Lock or unlock an agent's avoidance movement.
---@param agent_id number Agent ID
---@param locked boolean True to lock avoidance movement
function crowd.set_agent_avoidance_locked(agent_id, locked) end

---Set whether an agent is kinematic.
---@param agent_id number Agent ID
---@param is_kinematic boolean True to exclude the agent from simulation integration
function crowd.set_agent_kinematic(agent_id, is_kinematic) end

---Set agent positions in batch. Equivalent to calling `crowd.set_agent_position()` for
---each of the first `count` entries, but crosses the Lua/native boundary once.
---@param ids number[] Agent IDs
---@param xs number[] Position X coordinates
---@param ys number[] Position Y coordinates
---@param count number Number of entries to process
function crowd.set_agent_positions(ids, xs, ys, count) end

---Set agent velocities in batch. Equivalent to calling `crowd.set_agent_velocity()` for
---each of the first `count` entries, but crosses the Lua/native boundary once.
---@param ids number[] Agent IDs
---@param xs number[] Velocity X components
---@param ys number[] Velocity Y components
---@param count number Number of entries to process
function crowd.set_agent_velocities(ids, xs, ys, count) end

---Set agent preferred velocities in batch. Equivalent to calling
---`crowd.set_agent_pref_velocity()` for each of the first `count` entries, but crosses the
---Lua/native boundary once.
---@param ids number[] Agent IDs
---@param xs number[] Preferred velocity X components
---@param ys number[] Preferred velocity Y components
---@param count number Number of entries to process
function crowd.set_agent_pref_velocities(ids, xs, ys, count) end

---Set agent positions and velocities in batch. Equivalent to calling
---`crowd.set_agent_position()` and `crowd.set_agent_velocity()` for each of the first
---`count` entries, but crosses the Lua/native boundary once.
---@param ids number[] Agent IDs
---@param xs number[] Position X coordinates
---@param ys number[] Position Y coordinates
---@param vxs number[] Velocity X components
---@param vys number[] Velocity Y components
---@param count number Number of entries to process
function crowd.set_agent_positions_and_velocities(ids, xs, ys, vxs, vys, count) end

---Get an agent's position.
---@param agent_id number Agent ID
---@return number x Position X coordinate
---@return number y Position Y coordinate
function crowd.get_agent_position(agent_id) end

---Get an agent's preferred velocity.
---@param agent_id number Agent ID
---@return number x Preferred velocity X component
---@return number y Preferred velocity Y component
function crowd.get_agent_pref_velocity(agent_id) end

---Get an agent's current velocity.
---@param agent_id number Agent ID
---@return number x Velocity X component
---@return number y Velocity Y component
function crowd.get_agent_velocity(agent_id) end

---Get an agent's maximum speed.
---@param agent_id number Agent ID
---@return number max_speed Maximum movement speed
function crowd.get_agent_max_speed(agent_id) end

---Get an agent's radius.
---@param agent_id number Agent ID
---@return number radius Agent radius
function crowd.get_agent_radius(agent_id) end

---Get an agent's neighbor query distance.
---@param agent_id number Agent ID
---@return number neighbor_distance Maximum neighbor query distance
function crowd.get_agent_neighbor_dist(agent_id) end

---Get an agent's maximum neighbor count.
---@param agent_id number Agent ID
---@return number max_neighbors Maximum neighbor count
function crowd.get_agent_max_neighbors(agent_id) end

---Get an agent's collision-avoidance time horizon.
---@param agent_id number Agent ID
---@return number time_horizon Agent time horizon
function crowd.get_agent_time_horizon(agent_id) end

---Get an agent's obstacle collision-avoidance time horizon.
---@param agent_id number Agent ID
---@return number time_horizon_obstacle Obstacle time horizon
function crowd.get_agent_time_horizon_obstacle(agent_id) end

---Get an agent's navigation mask.
---@param agent_id number Agent ID
---@return number nav_mask Navigation mask
function crowd.get_agent_nav_mask(agent_id) end

---Get a polygon obstacle's navigation mask.
---@param obstacle_id number First vertex ID of the polygon obstacle
---@return number nav_mask Navigation mask
function crowd.get_obstacle_nav_mask(obstacle_id) end

---Get a circle obstacle's navigation mask.
---@param obstacle_id number Circle obstacle ID
---@return number nav_mask Navigation mask
function crowd.get_circle_obstacle_nav_mask(obstacle_id) end

---Get an agent's avoidance weight.
---@param agent_id number Agent ID
---@return number weight Avoidance responsibility weight
function crowd.get_agent_avoidance_weight(agent_id) end

---Get an agent's avoidance priority.
---@param agent_id number Agent ID
---@return number priority Avoidance priority
function crowd.get_agent_avoidance_priority(agent_id) end

---Check whether an agent's avoidance movement is locked.
---@param agent_id number Agent ID
---@return boolean locked True when avoidance movement is locked
function crowd.get_agent_avoidance_locked(agent_id) end

---Check whether an agent is kinematic.
---@param agent_id number Agent ID
---@return boolean is_kinematic True when the agent is kinematic
function crowd.get_agent_kinematic(agent_id) end

---Get the number of agent neighbors found during the latest simulation step.
---@param agent_id number Agent ID
---@return number neighbor_count Agent neighbor count
function crowd.get_agent_num_agent_neighbors(agent_id) end

---Get the number of polygon obstacle neighbors found during the latest simulation step.
---@param agent_id number Agent ID
---@return number neighbor_count Polygon obstacle neighbor count
function crowd.get_agent_num_obstacle_neighbors(agent_id) end

---Get the number of circle obstacle neighbors found during the latest simulation step.
---@param agent_id number Agent ID
---@return number neighbor_count Circle obstacle neighbor count
function crowd.get_agent_num_circle_obstacle_neighbors(agent_id) end

---Get the number of ORCA lines generated for an agent during the latest simulation step.
---@param agent_id number Agent ID
---@return number line_count ORCA line count
function crowd.get_agent_num_orca_lines(agent_id) end

---Get agent positions in batch. Writes results into `xs_out`/`ys_out` in place instead of
---allocating new tables.
---@param ids number[] Agent IDs
---@param count number Number of entries to process
---@param xs_out number[] Output table for X coordinates
---@param ys_out number[] Output table for Y coordinates
function crowd.get_agent_positions(ids, count, xs_out, ys_out) end

---Get agent velocities in batch. Writes results into `vxs_out`/`vys_out` in place instead
---of allocating new tables.
---@param ids number[] Agent IDs
---@param count number Number of entries to process
---@param vxs_out number[] Output table for velocity X components
---@param vys_out number[] Output table for velocity Y components
function crowd.get_agent_velocities(ids, count, vxs_out, vys_out) end

---Get agent positions and velocities in batch. Writes results into `xs_out`/`ys_out`/
---`vxs_out`/`vys_out` in place instead of allocating new tables.
---@param ids number[] Agent IDs
---@param count number Number of entries to process
---@param xs_out number[] Output table for X coordinates
---@param ys_out number[] Output table for Y coordinates
---@param vxs_out number[] Output table for velocity X components
---@param vys_out number[] Output table for velocity Y components
function crowd.get_agent_positions_and_velocities(ids, count, xs_out, ys_out, vxs_out, vys_out) end

---Get an agent neighbor ID by zero-based neighbor index.
---@param agent_id number Agent ID
---@param neighbor_idx number Zero-based neighbor index
---@return number neighbor_agent_id Neighbor agent ID, or crowd.INVALID_ID when the index is invalid
function crowd.get_agent_agent_neighbor(agent_id, neighbor_idx) end

---Get a polygon obstacle neighbor ID by zero-based neighbor index.
---@param agent_id number Agent ID
---@param neighbor_idx number Zero-based neighbor index
---@return number obstacle_id Obstacle vertex ID, or crowd.INVALID_ID when the index is invalid
function crowd.get_agent_obstacle_neighbor(agent_id, neighbor_idx) end

---Get a circle obstacle neighbor ID by zero-based neighbor index.
---@param agent_id number Agent ID
---@param neighbor_idx number Zero-based neighbor index
---@return number obstacle_id Circle obstacle ID, or crowd.INVALID_ID when the index is invalid
function crowd.get_agent_circle_obstacle_neighbor(agent_id, neighbor_idx) end

---Get an ORCA line by zero-based line index.
---@param agent_id number Agent ID
---@param line_idx number Zero-based ORCA line index
---@return number point_x ORCA line point X coordinate
---@return number point_y ORCA line point Y coordinate
---@return number direction_x ORCA line direction X component
---@return number direction_y ORCA line direction Y component
function crowd.get_agent_orca_line(agent_id, line_idx) end

---Get a polygon obstacle vertex position.
---@param vertex_id number Obstacle vertex ID
---@return number x Vertex X coordinate
---@return number y Vertex Y coordinate
function crowd.get_obstacle_vertex(vertex_id) end

---Get the next vertex ID in a polygon obstacle ring.
---@param vertex_id number Obstacle vertex ID
---@return number next_vertex_id Next vertex ID, or crowd.INVALID_ID when the vertex is invalid
function crowd.get_next_obstacle_vertex_id(vertex_id) end

---Get the previous vertex ID in a polygon obstacle ring.
---@param vertex_id number Obstacle vertex ID
---@return number previous_vertex_id Previous vertex ID, or crowd.INVALID_ID when the vertex is invalid
function crowd.get_prev_obstacle_vertex_id(vertex_id) end

---Get the number of source polygon obstacle vertices.
---Unlike `crowd.get_obstacle_count()`, this excludes internal BSP split records and counts only the vertices originally passed to `crowd.add_obstacle()`.
---@return number vertex_count Number of source polygon obstacle vertices
function crowd.get_num_obstacle_vertices() end

---Set the simulation time step.
---@param time_step number Time step in seconds
function crowd.set_time_step(time_step) end

---Get the simulation time step.
---@return number time_step Time step in seconds
function crowd.get_time_step() end

---Get the elapsed simulation time.
---@return number global_time Elapsed simulation time in seconds
function crowd.get_global_time() end

---Advance the simulation by one time step.
function crowd.do_step() end

---Advance the simulation by one time step.
function crowd.do_step_parallel() end

---Find active agents within `radius` of (x, y), using the current agent KD-tree.
---Pure center-distance test; agent radius is not included. Returns closest first,
---capped at the query result buffer's capacity (`max_agents` by default, see
---`crowd.set_max_agent_query`).
---@param x number Query center X
---@param y number Query center Y
---@param radius number Query radius
---@param nav_mask number|nil Navigation mask (defaults to NAV_MASK_ALL if omitted)
---@return number[] agent_ids Agent ids within radius, ascending by distance
---@return number count Number of ids written to agent_ids
function crowd.query_agents_in_radius(x, y, radius, nav_mask) end

---Resize the shared result buffer used by `crowd.query_agents_in_radius`.
---Defaults to `max_agents` (set in `crowd.init`); call this to use a different
---capacity. Must be called after `crowd.init`.
---@param count number New result buffer capacity
function crowd.set_max_agent_query(count) end

---Cast a finite segment from (start_x, start_y) to (end_x, end_y) against active agents,
---using the current agent KD-tree. Each candidate agent is tested with a combined radius
---of `radius + agent.radius`, so a `radius` of 0 tests the segment against agents' own
---radii only. Returns hits sorted by `t` (normalized position along the segment, in [0,1])
---ascending, with agent id as a deterministic tiebreak, capped at the query result buffer's
---capacity (`max_agents` by default, see `crowd.set_max_raycast_query`).
---@param start_x number Segment start X
---@param start_y number Segment start Y
---@param end_x number Segment end X
---@param end_y number Segment end Y
---@param radius number Extra radius added to each agent's own radius (0 for a bare segment)
---@param nav_mask number|nil Navigation mask (defaults to NAV_MASK_ALL if omitted)
---@return table[] hits Array of `{id: number, t: number}`, sorted by `t` ascending
---@return number count Number of hits written to hits
function crowd.raycast_agents(start_x, start_y, end_x, end_y, radius, nav_mask) end

---Resize the shared result buffer used by `crowd.raycast_agents`.
---Defaults to `max_agents` (set in `crowd.init`); call this to use a different
---capacity. Must be called after `crowd.init`.
---@param count number New result buffer capacity
function crowd.set_max_raycast_query(count) end

---Find active circle obstacles within `radius` of (x, y), using the circle-obstacle KD-tree.
---Pure center-distance test; the obstacle's own radius is not included. An obstacle exactly
---tangent to the query circle (center_dist == radius) is NOT included (strict `<`).
---Requires `crowd.process_circle_obstacles()` to have been called at least once — returns
---an empty table if the circle-obstacle tree has not been built yet. Returns closest first,
---capped at the query result buffer's capacity (`max_circle_obstacles` by default, see
---`crowd.set_max_circle_query`). Returned obstacle ids are current slot indices, not permanent
---identities: a removed obstacle's id can be reassigned to a new obstacle by a later
---`crowd.add_circle_obstacle()` call.
---@param x number Query center X
---@param y number Query center Y
---@param radius number Query radius
---@param nav_mask number|nil Navigation mask (defaults to NAV_MASK_ALL if omitted)
---@return number[] obstacle_ids Circle obstacle ids within radius, ascending by distance
---@return number count Number of ids written to obstacle_ids
function crowd.query_circle_obstacles_in_radius(x, y, radius, nav_mask) end

---Find active circle obstacles overlapping a query circle of `radius` centered at (x, y),
---using the circle-obstacle KD-tree. True circle-vs-circle overlap test: an obstacle matches
---if `distance(query center, obstacle center) < radius + obstacle.radius` (strict `<` — an
---obstacle exactly touching the query circle's edge is NOT included). Requires
---`crowd.process_circle_obstacles()` to have been called at least once — returns an empty
---table if the circle-obstacle tree has not been built yet. Returns closest first (by center
---distance), capped at the query result buffer's capacity (`max_circle_obstacles` by default,
---see `crowd.set_max_circle_query`). Returned obstacle ids are current slot indices, not
---permanent identities: a removed obstacle's id can be reassigned to a new obstacle by a later
---`crowd.add_circle_obstacle()` call.
---@param x number Query center X
---@param y number Query center Y
---@param radius number Query radius
---@param nav_mask number|nil Navigation mask (defaults to NAV_MASK_ALL if omitted)
---@return number[] obstacle_ids Circle obstacle ids overlapping the query circle, ascending by center distance
---@return number count Number of ids written to obstacle_ids
function crowd.query_circle_obstacles_colliding(x, y, radius, nav_mask) end

---Resize the shared result buffer used by `crowd.query_circle_obstacles_in_radius` and
---`crowd.query_circle_obstacles_colliding`. Defaults to `max_circle_obstacles` (set in
---`crowd.init`); call this to use a different capacity. The requested count is clamped to
---`max_circle_obstacles`. Must be called after `crowd.init`.
---@param count number New result buffer capacity
function crowd.set_max_circle_query(count) end

---Cast a finite segment from (start_x, start_y) to (end_x, end_y) against active circle
---obstacles, using the circle-obstacle KD-tree. Each candidate obstacle is tested with a
---combined radius of `radius + obstacle.radius`, so a `radius` of 0 tests the segment against
---obstacles' own radii only. Requires `crowd.process_circle_obstacles()` to have been called
---at least once — returns an empty table if the circle-obstacle tree has not been built yet.
---Returns hits sorted by `t` (normalized position along the segment, in [0,1]) ascending,
---with obstacle id as a deterministic tiebreak, capped at the query result buffer's capacity
---(`max_circle_obstacles` by default, see `crowd.set_max_circle_raycast_query`). Returned
---obstacle ids are current slot indices, not permanent identities: a removed obstacle's id can
---be reassigned to a new obstacle by a later `crowd.add_circle_obstacle()` call.
---@param start_x number Segment start X
---@param start_y number Segment start Y
---@param end_x number Segment end X
---@param end_y number Segment end Y
---@param radius number Extra radius added to each obstacle's own radius (0 for a bare segment)
---@param nav_mask number|nil Navigation mask (defaults to NAV_MASK_ALL if omitted)
---@return table[] hits Array of `{id: number, t: number}`, sorted by `t` ascending
---@return number count Number of hits written to hits
function crowd.raycast_circle_obstacles(start_x, start_y, end_x, end_y, radius, nav_mask) end

---Resize the shared result buffer used by `crowd.raycast_circle_obstacles`.
---Defaults to `max_circle_obstacles` (set in `crowd.init`); call this to use a different
---capacity. The requested count is clamped to `max_circle_obstacles`. Must be called after
---`crowd.init`.
---@param count number New result buffer capacity
function crowd.set_max_circle_raycast_query(count) end
