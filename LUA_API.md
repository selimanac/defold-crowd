# Defold Crowd Lua API Reference

The `crowd` native extension exposes a fixed-capacity, two-dimensional ORCA crowd simulation to Defold Lua.

## Conventions and workflows

- Call `crowd.init()` before every other function. Calling it again destroys and replaces the existing context, agents, obstacles, game-object tracking, and worker state.
- Agent, polygon-obstacle, and circle-obstacle IDs are Lua numbers backed by unsigned 16-bit values. `crowd.INVALID_ID` (`65535`) is the invalid ID sentinel returned by add and indexed-access functions on failure.
- Indexed neighbor and ORCA-line accessors use zero-based indexes even though ordinary Lua arrays are one-based.
- The simulation is fixed-capacity. Capacities are chosen by `crowd.init()` and do not grow at runtime.
- Polygon and circle obstacles are not used by the solver until their corresponding `process_*` function rebuilds the search tree.
- Most setters and getters require a valid, active ID and do not return a status. Keep IDs returned by the add functions and stop using them after removal.
- The manual workflow uses `crowd.add_agent()`, sets preferred velocities and the time step, calls `crowd.do_step()` or `crowd.do_step_parallel()`, and reads positions.
- The managed game-object workflow uses `crowd.init_gameobjects()`, `crowd.add_gameobject()`, goals, and `crowd.run_simulation(true)`. The extension then advances the simulation and writes transforms during its native update. Do not also step the same context manually.
- When driving hundreds or thousands of manual agents, prefer the batch functions in [Batch agent access](#batch-agent-access) over repeated single-agent calls — each batch call crosses the Lua/native boundary once instead of once per agent.

### Manual-agent example

```lua
function init(self)
    crowd.init(128, 16, 0, 0)
    self.agent_id = crowd.add_agent(0, 0, 12, 120, 16, 160, 2.0, 2.0)
end

function update(self, dt)
    crowd.set_agent_pref_velocity(self.agent_id, 120, 0)
    crowd.set_time_step(dt)
    crowd.do_step()

    local x, y = crowd.get_agent_position(self.agent_id)
    go.set_position(vmath.vector3(x, y, 0))
end

function final(self)
    crowd.destroy()
end
```

### Managed game-object example

```lua
function init(self)
    crowd.init(128, 16, 0, 0, 4)
    crowd.init_gameobjects()
    self.agent_id = crowd.add_gameobject(msg.url(), 12, 120, 16, 160, 2.0, 2.0)
    crowd.set_gameobject_goal(self.agent_id, 600, 200)
    crowd.run_simulation(true)
end
```

## Constants

### `crowd.INVALID_ID`

```lua
crowd.INVALID_ID  -- 65535
```

Invalid ID sentinel. Every function that returns an agent ID, polygon obstacle ID, or circle obstacle ID returns this value when the operation fails — capacity exhausted, invalid input, or slot unavailable.

```lua
local agent_id = crowd.add_agent(x, y, radius, max_speed, max_neighbors,
    neighbor_distance, time_horizon, time_horizon_obstacle)
if agent_id == crowd.INVALID_ID then
    -- no agent slot available
end
```

## Lifecycle

### `crowd.init()`

```lua
crowd.init(max_agents, max_neighbors [, max_obstacle_vertices] [, max_circle_obstacles ] [, max_workers])
```

Creates the fixed-capacity simulation context and replaces any existing context. In a JobSystem-enabled build, `max_workers > 1` requests background workers; the main thread counts as one worker. Builds without JobSystem support and platforms without thread support remain single-threaded.

| Name | Type | Description |
|---|---|---|
| `max_agents` | `number` | Maximum active agent slots, within the unsigned 16-bit range. |
| `max_neighbors` | `number` | Context-wide per-agent neighbor scratch capacity, within `1`–`255`. |
| `max_obstacle_vertices` | `number` or `nil` | Maximum source polygon vertices; defaults to `0`, which disables polygon obstacles. Maximum: `(65534 - max_neighbors) / 4`. |
| `max_circle_obstacles` | `number` or `nil` | Maximum circle obstacles; defaults to `0`, which disables circle obstacles. |
| `max_workers` | `number` or `nil` | Simulation lanes including the main thread, `1`–`255`; defaults to `1`. |
| Return | — | No values. Context-creation failure is logged. |

Initialize once during setup. All existing IDs become invalid if this function is called again. Also allocates the `crowd.query_agents_in_radius()` result buffer, sized to `max_agents` by default — see `crowd.set_max_agent_query()` to use a different capacity. Also allocates the `crowd.raycast_agents()` result buffer, sized to `max_agents` by default — see `crowd.set_max_raycast_query()` to use a different capacity. Also allocates the `crowd.query_circle_obstacles_in_radius()` / `crowd.query_circle_obstacles_colliding()` result buffer, sized to `max_circle_obstacles` by default — see `crowd.set_max_circle_query()` to use a different capacity. Also allocates the `crowd.raycast_circle_obstacles()` result buffer, sized to `max_circle_obstacles` by default — see `crowd.set_max_circle_raycast_query()` to use a different capacity.

### `crowd.destroy()`

```lua
crowd.destroy()
```

Stops managed updates and releases the context, agents, obstacles, game-object tracking records, and JobSystem resources owned by the extension.

| Name | Type | Description |
|---|---|---|
| Return | — | No values. Calling it with no active context is safe. |

Call from the owning script's `final()` when the crowd is no longer needed.



## Agents

### `crowd.add_agent()`

```lua
local agent_id = crowd.add_agent(x, y, radius, max_speed, max_neighbors,
    neighbor_distance, time_horizon, time_horizon_obstacle)
```

Adds a manually controlled agent to the simulation.

| Name | Type | Description |
|---|---|---|
| `x` | `number` | Initial world-space X coordinate. |
| `y` | `number` | Initial world-space Y coordinate. |
| `radius` | `number` | Collision radius. |
| `max_speed` | `number` | Maximum speed used by the ORCA solver. |
| `max_neighbors` | `number` | Per-agent neighbor cap, not exceeding the context capacity. Use `0` for unilateral agent avoidance. |
| `neighbor_distance` | `number` | Radius in which other agents can become neighbors. |
| `time_horizon` | `number` | Agent-agent avoidance lookahead in seconds. |
| `time_horizon_obstacle` | `number` | Obstacle-avoidance lookahead in seconds. |
| Return | `number` | Agent ID, or `crowd.INVALID_ID` when no slot is available. |

Requires an active context. A `max_neighbors` value of `0` makes this agent ignore other agents in its own solve while other agents can still avoid it; obstacle avoidance remains active.

**Choosing `neighbor_distance`**

`neighbor_distance` is the radius of the search circle used to find nearby agents and obstacles each step. Only objects within this distance can be added to the neighbor list — no ORCA constraint is ever generated for a pair that never meets inside each other's search circles.

Too small and agents don't see each other in time, producing late or jittery avoidance and potential collisions at higher speeds. Too large and agents try to react to many distant agents that are not actually a threat, which increases per-step work and can make movement feel unnatural.

Two starting formulas:

**Speed-based** — good when agent speeds vary significantly:

```lua
neighbor_distance = max_speed * 3 + 2 * radius
```

The first term is how far the agent travels in roughly 3 seconds at full speed — enough lookahead for the solver to plan a smooth correction before a collision. The second term accounts for the physical reach of both agents: two agents of the same radius first touch when they are `2 * radius` apart.

**Size-based** — good when agents have different radii and you want proportional awareness zones:

```lua
neighbor_distance = radius * 8 + 150
```

This keeps the search circle roughly 8 body-widths wide, scaled to the agent's size. A flat constant is added as a minimum baseline so very small agents still see across a reasonable area.

**Practical rules:**

- The minimum that avoids most collisions at walking speed is approximately `max_speed * time_horizon + 2 * radius`.
- If weight, priority, locked, or kinematic agents are not having their intended effect on a pair, confirm that pair actually falls inside each other's `neighbor_distance` by checking `crowd.get_agent_num_agent_neighbors()`.
- Increasing `neighbor_distance` without also increasing `max_neighbors` has limited effect once the neighbor list fills up with closer agents.

### `crowd.remove_agent()`

```lua
local removed = crowd.remove_agent(agent_id)
```

Marks an agent inactive and makes its slot available for reuse.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| Return | `boolean` | `true` on success; `false` for an invalid or inactive ID. |

For managed agents, the native tracking record is removed on the next update. The Defold game object remains alive.

### `crowd.get_agent_count()`

```lua
local count = crowd.get_agent_count()
```

Returns the number of agents currently participating in the simulation.

| Name | Type | Description |
|---|---|---|
| Return | `number` | Number of currently active agents. |

Requires an active context.


## Managed game objects

> **This is a reference implementation, not a production system.**
>
> The managed game-object API is included to demonstrate how the native side drives Defold transforms — goals, preferred velocity, stepping, and position write-back. It intentionally covers only the simplest case: a single collection of agents all moving toward fixed world-space goals.
>
> It does not handle pathfinding, formation movement, arrival behaviours, group selection, agent priorities set from Lua, dynamic obstacles, or any other gameplay logic your project will likely need. If you use it as-is you will quickly hit its limits.
>
> Treat it as a starting point and a code reference. For real projects, extend or replace the native C/C++ side — the managed game-object code in `crowd_manager.cpp`  and `unit.cpp` shows exactly how the update loop, preferred velocity, stepping, and transform write-back are wired together, and is the intended entry point for customisation.

Managed game objects are Defold instances in the calling script's collection. Their X/Y position and planar rotation are written by the native update loop. Their initial Z position is retained. The game object itself remains owned by Defold and is never deleted by the crowd extension.

### `crowd.init_gameobjects()`

```lua
crowd.init_gameobjects()
```

Allocates game-object tracking records using the agent capacity selected by `crowd.init()`.

| Name | Type | Description |
|---|---|---|
| Return | — | No values. Invalid initialization is logged. |

Requires an active context. It is rejected while any tracking records are active so existing agent-to-instance mappings are preserved. It does not start simulation; call `crowd.run_simulation(true)` after setup.

### `crowd.add_gameobject()`

```lua
local agent_id = crowd.add_gameobject(instance, radius, max_speed, max_neighbors,
    neighbor_distance, time_horizon, time_horizon_obstacle)
```

Creates an agent at the game object's current X/Y position and associates that agent with the instance. The instance must belong to the calling script's collection.

| Name | Type | Description |
|---|---|---|
| `instance` | `url` | Game-object URL, commonly `go.get_id()`. |
| `radius` | `number` | Agent collision radius. |
| `max_speed` | `number` | Maximum movement speed. |
| `max_neighbors` | `number` | Per-agent neighbor limit, not exceeding the context capacity. |
| `neighbor_distance` | `number` | Maximum distance for agent-neighbor queries. |
| `time_horizon` | `number` | Agent-agent avoidance lookahead in seconds. |
| `time_horizon_obstacle` | `number` | Obstacle-avoidance lookahead in seconds. |
| Return | `number` | Agent ID, or `crowd.INVALID_ID` if the context, agent slot, or tracking record is unavailable. |

Requires `crowd.init_gameobjects()`. The game object's initial Z coordinate is stored and preserved during native transform updates.

### `crowd.set_gameobject_goal()`

```lua
crowd.set_gameobject_goal(agent_id, x, y)
```

Assigns a world-space goal to a tracked game object. The managed update derives the preferred velocity that moves the agent toward this goal.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | ID returned by `crowd.add_gameobject()`. |
| `x` | `number` | Goal X coordinate. |
| `y` | `number` | Goal Y coordinate. |
| Return | — | No values. Invalid or untracked IDs are logged. |

### `crowd.remove_gameobject_goal()`

```lua
crowd.remove_gameobject_goal(agent_id)
```

Clears the tracked game object's current movement goal.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | ID returned by `crowd.add_gameobject()`. |
| Return | — | No values. Invalid or untracked IDs are logged. |

The agent remains active and tracked. Use `crowd.remove_agent()` to remove it from the simulation.

### `crowd.get_gameobject_count()`

```lua
local count = crowd.get_gameobject_count()
```

Returns the number of dense native tracking records.

| Name | Type | Description |
|---|---|---|
| Return | `number` | Tracked record count, including records awaiting deferred removal. |

When `crowd.remove_agent()` targets a managed agent, tracking stops through deferred native cleanup on the next update. The Defold game object is not deleted.

### `crowd.run_simulation()`

```lua
crowd.run_simulation(enabled)
```

Enables or pauses native simulation and automatic transform updates for managed game objects.

| Name | Type | Description |
|---|---|---|
| `enabled` | `boolean` | `true` to run; `false` to pause. |
| Return | — | No values. |

Changing the state resets native frame timing and its fixed-step accumulator, so paused time is not processed when simulation resumes.


## Agent motion and properties

### `crowd.set_agent_pref_velocity()`

```lua
crowd.set_agent_pref_velocity(agent_id, x, y)
```

Sets the velocity the agent would choose without avoidance constraints. Manual simulations normally set it before every step.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| `x` | `number` | Preferred X velocity. |
| `y` | `number` | Preferred Y velocity. |
| Return | — | No values. |

### `crowd.get_agent_pref_velocity()`

```lua
local x, y = crowd.get_agent_pref_velocity(agent_id)
```

Returns the agent's current preferred velocity.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| Return 1 | `number` | Preferred X velocity. |
| Return 2 | `number` | Preferred Y velocity. |

### `crowd.set_agent_position()`

```lua
crowd.set_agent_position(agent_id, x, y)
```

Overrides an agent's current world-space position. Use this for teleports or respawns; normal stepping updates position automatically.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| `x` | `number` | New X coordinate. |
| `y` | `number` | New Y coordinate. |
| Return | — | No values. |

### `crowd.get_agent_position()`

```lua
local x, y = crowd.get_agent_position(agent_id)
```

Returns the agent's current simulated position.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| Return 1 | `number` | Current X coordinate. |
| Return 2 | `number` | Current Y coordinate. |

### `crowd.set_agent_velocity()`

```lua
crowd.set_agent_velocity(agent_id, x, y)
```

Overrides the agent's current velocity. The next simulation step replaces it with the solver's new velocity.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| `x` | `number` | X velocity. |
| `y` | `number` | Y velocity. |
| Return | — | No values. |

### `crowd.get_agent_velocity()`

```lua
local x, y = crowd.get_agent_velocity(agent_id)
```

Returns the velocity produced or stored by the latest update.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| Return 1 | `number` | Current X velocity. |
| Return 2 | `number` | Current Y velocity. |

### `crowd.set_agent_max_speed()`

```lua
crowd.set_agent_max_speed(agent_id, max_speed)
```

Sets the maximum speed used as the radius of the ORCA velocity constraint disk.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| `max_speed` | `number` | New maximum speed. |
| Return | — | No values. |

### `crowd.get_agent_max_speed()`

```lua
local max_speed = crowd.get_agent_max_speed(agent_id)
```

Returns the agent's current maximum speed.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| Return | `number` | Current maximum speed. |

### `crowd.set_agent_radius()`

```lua
crowd.set_agent_radius(agent_id, radius)
```

Sets the physical radius used for neighbor geometry and ORCA constraints.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| `radius` | `number` | New collision radius. |
| Return | — | No values. |

### `crowd.get_agent_radius()`

```lua
local radius = crowd.get_agent_radius(agent_id)
```

Returns the agent's current collision radius.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| Return | `number` | Current collision radius. |

### `crowd.set_agent_neighbor_dist()`

```lua
crowd.set_agent_neighbor_dist(agent_id, neighbor_distance)
```

Sets the maximum distance at which other agents are considered for agent-agent avoidance.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| `neighbor_distance` | `number` | Neighbor query distance. |
| Return | — | No values. |

### `crowd.get_agent_neighbor_dist()`

```lua
local neighbor_distance = crowd.get_agent_neighbor_dist(agent_id)
```

Returns the agent's current neighbor query distance.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| Return | `number` | Current neighbor query distance. |

### `crowd.set_agent_max_neighbors()`

```lua
crowd.set_agent_max_neighbors(agent_id, max_neighbors)
```

Sets the maximum number of agent neighbors retained for the agent each step.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| `max_neighbors` | `number` | New cap, from `0` through the context's `max_neighbors`. |
| Return | — | No values. |

Use `0` when an agent should ignore agent-agent ORCA in its own solve while remaining visible to other agents. Static-obstacle avoidance is unaffected.

### `crowd.get_agent_max_neighbors()`

```lua
local max_neighbors = crowd.get_agent_max_neighbors(agent_id)
```

Returns the agent's current agent-neighbor cap.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| Return | `number` | Current per-agent neighbor cap. |

### `crowd.set_agent_time_horizon()`

```lua
crowd.set_agent_time_horizon(agent_id, time_horizon)
```

Sets the agent-agent avoidance lookahead in seconds. Larger positive values generally produce earlier and gentler avoidance.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| `time_horizon` | `number` | New agent-agent time horizon. |
| Return | — | No values. |

### `crowd.get_agent_time_horizon()`

```lua
local time_horizon = crowd.get_agent_time_horizon(agent_id)
```

Returns the agent-agent avoidance lookahead.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| Return | `number` | Current agent-agent time horizon in seconds. |

### `crowd.set_agent_time_horizon_obstacle()`

```lua
crowd.set_agent_time_horizon_obstacle(agent_id, time_horizon_obstacle)
```

Sets the static-obstacle avoidance lookahead in seconds.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| `time_horizon_obstacle` | `number` | New obstacle time horizon. |
| Return | — | No values. |

### `crowd.get_agent_time_horizon_obstacle()`

```lua
local time_horizon_obstacle = crowd.get_agent_time_horizon_obstacle(agent_id)
```

Returns the static-obstacle avoidance lookahead.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| Return | `number` | Current obstacle time horizon in seconds. |

## Batch agent access

Calling a single-agent getter or setter once per agent is fine for small counts, but with
hundreds or thousands of manual agents the repeated Lua↔native calls become the actual cost —
not the field access itself. These batch functions read or write position, velocity, and
preferred velocity for many agents in one native call.

All batch functions use flat, one-based, parallel Lua arrays instead of an array of tables
(e.g. `ids = {1, 2, 3}`, `xs = {10, 20, 30}`, index `i` of every array describes the same
agent) and take an explicit `count` rather than relying on `#ids`, so you can reuse
fixed-size buffer tables across frames without resizing. Per-agent semantics are identical
to the corresponding single-agent function, including silently skipping invalid or inactive
IDs.

> [!IMPORTANT]
> **Reusing scratch buffers.** The getters write results into the `xs_out`/`ys_out`/... tables
> you pass in — they never allocate or return new tables. Create these buffer tables once
> (e.g. during `init`) and pass the same tables in every frame. Allocating fresh tables every
> update instead would create garbage for the Lua GC to collect on every single frame.
> `defold-files/examples/scripts/lib/data.lua` (buffer tables) and `lib/agent.lua`
> (`set_preferred_velocities` / `update_game_objects`) show the full reference pattern.

### `crowd.set_agent_positions()`

```lua
crowd.set_agent_positions(ids, xs, ys, count)
```

Sets world-space position for many agents in one call.

| Name | Type | Description |
|---|---|---|
| `ids` | `number[]` | One-based array of at least `count` active agent IDs. |
| `xs` | `number[]` | One-based array of new X coordinates, aligned with `ids`. |
| `ys` | `number[]` | One-based array of new Y coordinates, aligned with `ids`. |
| `count` | `number` | Number of entries to process from `ids`/`xs`/`ys`. |
| Return | — | No values. |

Equivalent to calling `crowd.set_agent_position()` for each of the first `count` entries, but
crosses the Lua/native boundary once instead of once per agent.

### `crowd.set_agent_velocities()`

```lua
crowd.set_agent_velocities(ids, xs, ys, count)
```

Overrides velocity for many agents in one call.

| Name | Type | Description |
|---|---|---|
| `ids` | `number[]` | One-based array of at least `count` active agent IDs. |
| `xs` | `number[]` | One-based array of new X velocities, aligned with `ids`. |
| `ys` | `number[]` | One-based array of new Y velocities, aligned with `ids`. |
| `count` | `number` | Number of entries to process from `ids`/`xs`/`ys`. |
| Return | — | No values. |

Equivalent to calling `crowd.set_agent_velocity()` for each of the first `count` entries.

### `crowd.set_agent_pref_velocities()`

```lua
crowd.set_agent_pref_velocities(ids, xs, ys, count)
```

Sets preferred velocity for many agents in one call. Manual simulations normally call this
before every step.

| Name | Type | Description |
|---|---|---|
| `ids` | `number[]` | One-based array of at least `count` active agent IDs. |
| `xs` | `number[]` | One-based array of preferred X velocities, aligned with `ids`. |
| `ys` | `number[]` | One-based array of preferred Y velocities, aligned with `ids`. |
| `count` | `number` | Number of entries to process from `ids`/`xs`/`ys`. |
| Return | — | No values. |

Equivalent to calling `crowd.set_agent_pref_velocity()` for each of the first `count` entries.

### `crowd.set_agent_positions_and_velocities()`

```lua
crowd.set_agent_positions_and_velocities(ids, xs, ys, vxs, vys, count)
```

Sets both position and velocity for many agents in one call.

| Name | Type | Description |
|---|---|---|
| `ids` | `number[]` | One-based array of at least `count` active agent IDs. |
| `xs` | `number[]` | One-based array of new X coordinates, aligned with `ids`. |
| `ys` | `number[]` | One-based array of new Y coordinates, aligned with `ids`. |
| `vxs` | `number[]` | One-based array of new X velocities, aligned with `ids`. |
| `vys` | `number[]` | One-based array of new Y velocities, aligned with `ids`. |
| `count` | `number` | Number of entries to process from all arrays. |
| Return | — | No values. |

Equivalent to calling `crowd.set_agent_position()` followed by `crowd.set_agent_velocity()`
for each of the first `count` entries, in a single native call.

### `crowd.get_agent_positions()`

```lua
crowd.get_agent_positions(ids, count, xs_out, ys_out)
```

Reads position for many agents in one call.

| Name | Type | Description |
|---|---|---|
| `ids` | `number[]` | One-based array of at least `count` active agent IDs. |
| `count` | `number` | Number of agents to read. |
| `xs_out` | `number[]` | Table filled with X coordinates, indexed `1..count`. |
| `ys_out` | `number[]` | Table filled with Y coordinates, indexed `1..count`. |
| Return | — | No values; results are written into `xs_out`/`ys_out`. |

Writes results into `xs_out`/`ys_out` in place; does not allocate new tables. See
"Reusing scratch buffers" above.

### `crowd.get_agent_velocities()`

```lua
crowd.get_agent_velocities(ids, count, vxs_out, vys_out)
```

Reads velocity for many agents in one call.

| Name | Type | Description |
|---|---|---|
| `ids` | `number[]` | One-based array of at least `count` active agent IDs. |
| `count` | `number` | Number of agents to read. |
| `vxs_out` | `number[]` | Table filled with X velocities, indexed `1..count`. |
| `vys_out` | `number[]` | Table filled with Y velocities, indexed `1..count`. |
| Return | — | No values; results are written into `vxs_out`/`vys_out`. |

Writes results into `vxs_out`/`vys_out` in place; does not allocate new tables. See
"Reusing scratch buffers" above.

### `crowd.get_agent_positions_and_velocities()`

```lua
crowd.get_agent_positions_and_velocities(ids, count, xs_out, ys_out, vxs_out, vys_out)
```

Reads both position and velocity for many agents in one call.

| Name | Type | Description |
|---|---|---|
| `ids` | `number[]` | One-based array of at least `count` active agent IDs. |
| `count` | `number` | Number of agents to read. |
| `xs_out` | `number[]` | Table filled with X coordinates, indexed `1..count`. |
| `ys_out` | `number[]` | Table filled with Y coordinates, indexed `1..count`. |
| `vxs_out` | `number[]` | Table filled with X velocities, indexed `1..count`. |
| `vys_out` | `number[]` | Table filled with Y velocities, indexed `1..count`. |
| Return | — | No values; results are written into the four output tables. |

Writes results into the four output tables in place; does not allocate new tables. This is
the batched equivalent of calling `crowd.get_agent_position()` and
`crowd.get_agent_velocity()` per agent — the common per-frame read when syncing game-object
transforms from the simulation. See "Reusing scratch buffers" above.

### Batch example

```lua
function init(self)
    crowd.init(2000, 16, 0, 0)

    -- Scratch buffers, created once and reused every frame
    self.ids = {}
    self.xs, self.ys   = {}, {}
    self.vxs, self.vys = {}, {}

    self.agent_count = 0
    for i = 1, 2000 do
        local id = crowd.add_agent(i * 2, 0, 12, 120, 16, 160, 2.0, 2.0)
        self.agent_count = self.agent_count + 1
        self.ids[self.agent_count] = id
    end
end

function update(self, dt)
    -- Set every agent's preferred velocity in one call
    for i = 1, self.agent_count do
        self.xs[i], self.ys[i] = 120, 0
    end
    crowd.set_agent_pref_velocities(self.ids, self.xs, self.ys, self.agent_count)

    crowd.set_time_step(dt)
    crowd.do_step_parallel()

    -- Read every agent's position and velocity back in one call
    crowd.get_agent_positions_and_velocities(self.ids, self.agent_count,
        self.xs, self.ys, self.vxs, self.vys)
end
```

## Polygon obstacles

Each polygon is treated as a closed ring. Vertex winding determines which side is blocked: use counter-clockwise (CCW) order for a solid obstacle that agents remain outside, and clockwise (CW) order for an enclosing boundary that keeps agents inside.

### `crowd.add_obstacle()`

```lua
local obstacle_id = crowd.add_obstacle(vertices)
```

Adds a polygon obstacle to the source records. It is not visible to agents until `crowd.process_obstacles()` succeeds.

| Name | Type | Description |
|---|---|---|
| `vertices` | `table[]` | One-based array of at least two `{ x = number, y = number }` vertices. Use CCW order for a solid obstacle or CW order for an enclosing boundary. |
| Return | `number` | First source vertex ID, or `crowd.INVALID_ID` on invalid geometry or capacity failure. |

```lua
local wall_id = crowd.add_obstacle({
    { x = 100, y = 100 },
    { x = 300, y = 100 },
    { x = 300, y = 140 },
    { x = 100, y = 140 },
})
crowd.process_obstacles()
```

### `crowd.remove_obstacle()`

```lua
local removed = crowd.remove_obstacle(obstacle_id)
```

Marks every segment in a source polygon ring inactive.

| Name | Type | Description |
|---|---|---|
| `obstacle_id` | `number` | Source ID returned by `crowd.add_obstacle()`; BSP split IDs are not valid here. |
| Return | `boolean` | `true` on success; `false` for an invalid or inactive source obstacle. |

Call `crowd.process_obstacles()` after removal to rebuild the search tree.

### `crowd.process_obstacles()`

```lua
crowd.process_obstacles()
```

Rebuilds the polygon obstacle BSP/KD search tree from active source obstacles.

| Name | Type | Description |
|---|---|---|
| Return | — | No values. Processing failure is logged. |

Call after a batch of additions or removals, rather than after every individual change.

### `crowd.get_obstacle_count()`

```lua
local count = crowd.get_obstacle_count()
```

| Name | Type | Description |
|---|---|---|
| Return | `number` | Number of internal polygon obstacle records, including BSP split records. |

This is an internal record count, not necessarily the number of source polygons.

### `crowd.get_num_obstacle_vertices()`

```lua
local count = crowd.get_num_obstacle_vertices()
```

| Name | Type | Description |
|---|---|---|
| Return | `number` | Number of source polygon obstacle vertices originally passed to `crowd.add_obstacle()`. Does not include internal BSP split records. |

Use this to know the original input size rather than the internal expanded count returned by `crowd.get_obstacle_count()`.

### `crowd.get_obstacle_vertex()`

```lua
local x, y = crowd.get_obstacle_vertex(vertex_id)
```

Reads a polygon obstacle vertex for rendering or debug inspection.

| Name | Type | Description |
|---|---|---|
| `vertex_id` | `number` | Valid source or neighbor-query vertex ID. |
| Return 1 | `number` | Vertex X coordinate. |
| Return 2 | `number` | Vertex Y coordinate. |

### `crowd.get_next_obstacle_vertex_id()`

```lua
local next_id = crowd.get_next_obstacle_vertex_id(vertex_id)
```

Returns the next linked vertex in the polygon ring.

| Name | Type | Description |
|---|---|---|
| `vertex_id` | `number` | Valid polygon vertex ID. |
| Return | `number` | Next vertex ID in the ring, or `crowd.INVALID_ID` for an invalid vertex. |

### `crowd.get_prev_obstacle_vertex_id()`

```lua
local previous_id = crowd.get_prev_obstacle_vertex_id(vertex_id)
```

Returns the previous linked vertex in the polygon ring.

| Name | Type | Description |
|---|---|---|
| `vertex_id` | `number` | Valid polygon vertex ID. |
| Return | `number` | Previous vertex ID in the ring, or `crowd.INVALID_ID` for an invalid vertex. |

## Circle obstacles

### `crowd.add_circle_obstacle()`

```lua
local obstacle_id = crowd.add_circle_obstacle(x, y, radius)
```

Adds a circular obstacle. It is not visible to agents until `crowd.process_circle_obstacles()` succeeds.

| Name | Type | Description |
|---|---|---|
| `x` | `number` | Center X coordinate. |
| `y` | `number` | Center Y coordinate. |
| `radius` | `number` | Positive circle radius. |
| Return | `number` | Circle obstacle ID, or `crowd.INVALID_ID` on invalid input or capacity failure. |

### `crowd.remove_circle_obstacle()`

```lua
local removed = crowd.remove_circle_obstacle(obstacle_id)
```

Marks a circle obstacle inactive.

| Name | Type | Description |
|---|---|---|
| `obstacle_id` | `number` | ID returned by `crowd.add_circle_obstacle()`. |
| Return | `boolean` | `true` on success; `false` for an invalid or already inactive obstacle. |

Call `crowd.process_circle_obstacles()` after removal.

### `crowd.process_circle_obstacles()`

```lua
crowd.process_circle_obstacles()
```

Rebuilds the circle-obstacle search tree from active circles.

| Name | Type | Description |
|---|---|---|
| Return | — | No values. Processing failure is logged. |

## Navigation masks

A navigation mask is an 8-bit number (0–255) assigned to every agent, polygon obstacle, and circle obstacle. It controls which objects can see and react to each other.

Two objects only interact when they share at least one layer in common. If they share no layer, they are completely invisible to each other — no avoidance constraints are generated between them, and agents pass through each other as if they were on separate simulation layers.

**Special values**

| Mask | Value | Meaning |
|---|---|---|
| `0` | `0x00` | Invisible to everything |
| `255` | `0xFF` | Interacts with everyone (default for all new agents and obstacles) |

All new agents and obstacles start with mask `255`, so existing simulations that never touch nav masks behave exactly as before.

**Defining your own layers**

All 8 bits are free for your game to define. A common pattern:

```lua
local LAYER_GROUND = 2 ^ 0  -- bit 0 = 1
local LAYER_AIR    = 2 ^ 1  -- bit 1 = 2
local LAYER_WATER  = 2 ^ 2  -- bit 2 = 4
```

Set a single layer to make an agent belong to exactly one category. Combine layers with bitwise OR (add the values in Lua) for multi-layer membership:

```lua
-- Amphibious unit — lives on ground AND water
local LAYER_AMPHIBIOUS = LAYER_GROUND + LAYER_WATER  -- 5

crowd.set_agent_nav_mask(amphibious_id, LAYER_AMPHIBIOUS)
```

**What the mask filters**

The mask is checked before anything is added to an agent's neighbor list. It applies to all three neighbor types: other agents, polygon obstacles, and circle obstacles. An agent with `LAYER_AIR` set and a ground-only polygon wall will never receive an obstacle ORCA line from that wall.

**Common patterns**

```lua
local LAYER_GROUND = 2 ^ 0
local LAYER_AIR    = 2 ^ 1

-- Ground agents avoid each other and ground obstacles
crowd.set_agent_nav_mask(infantry_id, LAYER_GROUND)
crowd.set_obstacle_nav_mask(wall_id, LAYER_GROUND)

-- Flying agents ignore ground agents and ground walls entirely
crowd.set_agent_nav_mask(drone_id, LAYER_AIR)

-- A shared obstacle visible to both layers (e.g. a tower)
crowd.set_obstacle_nav_mask(tower_id, LAYER_GROUND + LAYER_AIR)
```

Lua bindings accept numeric masks; pass whole integer values in the `0`–`255` range.

### `crowd.set_agent_nav_mask()`

```lua
crowd.set_agent_nav_mask(agent_id, nav_mask)
```

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| `nav_mask` | `number` | Navigation layer bitmask, `0`–`255`. |
| Return | — | No values. |

The mask filters agent-agent, polygon-obstacle, and circle-obstacle neighbor discovery.

### `crowd.get_agent_nav_mask()`

```lua
local nav_mask = crowd.get_agent_nav_mask(agent_id)
```

Returns the agent's navigation-layer membership mask.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| Return | `number` | Current navigation mask. |

### `crowd.set_obstacle_nav_mask()`

```lua
crowd.set_obstacle_nav_mask(obstacle_id, nav_mask)
```

Sets the mask for an entire source polygon ring, including BSP split records.

| Name | Type | Description |
|---|---|---|
| `obstacle_id` | `number` | Source ID returned by `crowd.add_obstacle()`. |
| `nav_mask` | `number` | Navigation layer bitmask, `0`–`255`. |
| Return | — | No values. Invalid or inactive IDs have no effect. |

This can be called before or after `crowd.process_obstacles()`. Source IDs are required; neighbor queries may return split-record IDs that are not accepted here.

### `crowd.get_obstacle_nav_mask()`

```lua
local nav_mask = crowd.get_obstacle_nav_mask(obstacle_id)
```

Returns the navigation-layer mask shared by the source polygon ring.

| Name | Type | Description |
|---|---|---|
| `obstacle_id` | `number` | Source polygon obstacle ID. |
| Return | `number` | Current ring mask, or `0` when the source obstacle is invalid or inactive. |

### `crowd.set_circle_obstacle_nav_mask()`

```lua
crowd.set_circle_obstacle_nav_mask(obstacle_id, nav_mask)
```

| Name | Type | Description |
|---|---|---|
| `obstacle_id` | `number` | Active circle obstacle ID. |
| `nav_mask` | `number` | Navigation layer bitmask, `0`–`255`. |
| Return | — | No values. Invalid or inactive IDs have no effect. |

### `crowd.get_circle_obstacle_nav_mask()`

```lua
local nav_mask = crowd.get_circle_obstacle_nav_mask(obstacle_id)
```

Returns the circle obstacle's navigation-layer mask.

| Name | Type | Description |
|---|---|---|
| `obstacle_id` | `number` | Circle obstacle ID. |
| Return | `number` | Current circle mask, or `0` when the obstacle is invalid or inactive. |

## Avoidance responsibility

Weight and priority are soft right-of-way controls. They change how a pairwise ORCA correction is divided between two agents. They do not apply a physical push and do not make an agent unstoppable in dense crowds.

**How it works**

When two nearby agents need to avoid each other, the solver works out a velocity correction for both. That correction is split between them based on *responsibility* — a number from 0% to 100% that says how much of the correction each agent should absorb.

An agent with a **higher effective weight** absorbs a **smaller share** — it moves less out of the way. The other agent picks up the rest. Equal weights mean a 50/50 split.

The effective weight of an agent is its `avoidance_weight` multiplied by `(1 + avoidance_priority)`. A unit with weight `1` and priority `2` has an effective weight of `3`. A unit with weight `10` and priority `0` has an effective weight of `10`. The agent with the higher effective weight is treated as "heavier" and yields less.

Responsibility is always kept between 5% and 95%. This prevents the correction from landing entirely on one agent, which can cause the solver to produce an invalid velocity.

**Effective weight ceiling (saturation)**

The effective weight is capped at an internal maximum (`AVOIDANCE_WEIGHT_MAX`). Once `weight × (1 + priority)` reaches this ceiling, further increases have no additional effect. An agent with weight `20` and priority `20` (effective `420`) behaves identically to one whose effective weight already sits at the cap — the agent has maximum right-of-way, but more does not mean more dominant. This is the *saturated* state.

In practice the ceiling is reached at moderate values. An agent with weight `16` may already be at or near the cap, so adding `priority = 20` on top of `weight = 20` yields the same visual result as a more modest combination that already hits the ceiling. Stay in the recommended starting ranges below and use small priority values (`0`–`3`) to remain in the useful, linear region of the scale.

**Weight vs priority — the key difference**

`avoidance_weight` describes what an entity *is* — a permanent property of the unit class. Set it once and leave it.

`avoidance_priority` describes what an entity *is doing right now* — a temporary gameplay state. Set it when the state begins and reset it to `0` when the state ends.

```lua
-- Permanent unit tiers
crowd.set_agent_avoidance_weight(infantry_id, 1)
crowd.set_agent_avoidance_weight(horse_id,    4)
crowd.set_agent_avoidance_weight(tank_id,     10)

-- Temporary urgency: selected infantry gets right-of-way
crowd.set_agent_avoidance_priority(selected_id, 2)  -- effective weight = 1*(1+2) = 3
-- Reset when deselected
crowd.set_agent_avoidance_priority(selected_id, 0)
```

**Starting values**

| Unit / state | Weight | Priority |
|---|---:|---:|
| Infantry / human | 1 | 0 |
| Selected infantry | 1 | 1–2 |
| Dog / fast creature | 2–4 | 0 |
| Horse / cavalry | 4–8 | 0–2 |
| Cart / wagon | 6–10 | 0 |
| Tank / boss | 10–20 | 0–2 |

Use small priority values (`0`–`3`). Large values saturate the responsibility clamp quickly and have no additional effect.

**Avoidance-locked agents**

`crowd.set_agent_avoidance_locked(agent_id, true)` hard-locks an agent at zero velocity. It skips ORCA computation entirely and takes 0% responsibility for every pairwise correction. Moving agents must absorb 100% of the correction when near a locked agent. The locked agent remains in the KD-tree so movers still see and avoid it.

Use this for idle RTS units, parked vehicles, or placed structures — anything that should act as a dynamic obstacle but never move.

**Kinematic agents**

Kinematic agents are externally controlled. They are not solved by ORCA and are not integrated by the simulation, but they remain visible to dynamic agents while retained as neighbors.

`crowd.set_agent_kinematic(agent_id, true)` applies the same zero-responsibility semantics as a locked agent: dynamic movers take 100% of every pairwise correction. Additionally, the simulation zeroes the kinematic agent's velocity and skips its position integration entirely — `do_step()` never moves it.

Move kinematic agents from Lua using `crowd.set_agent_position()` each frame.

Use this for scripted movers, path-following agents, or game objects whose transform is owned by an animation or cutscene system outside the crowd.

**Dynamic agents**

Dynamic agents are simulation-controlled. They solve ORCA, choose a new velocity, and are integrated by `do_step()`.

Use dynamic agents for units whose movement should be driven by the crowd simulation. Use kinematic agents for units whose position is owned by gameplay, animation, physics, or another external system.

Some units should never be slowed down or deflected by other agents — projectiles, a tank pushing through infantry, a cavalry charge. For these, weight and priority are the wrong tool: even a very high weight still allows some small yield (responsibility is clamped to at most 95%).

The correct approach is to set the agent's `max_neighbors` to `0`. With no agent neighbors, the solver generates no agent-agent ORCA constraints for that unit — it ignores all other agents when computing its own velocity and moves exactly where it wants to go. Other agents still see it in the spatial tree and route around it normally.

Obstacle avoidance is unaffected: an agent with `max_neighbors = 0` still avoids walls and circle obstacles if the context has them.

```lua
local bullet_id = crowd.add_agent(x, y, radius, max_speed,
    0,                   -- max_neighbors = 0: ignore other agents in own solve
    neighbor_distance, time_horizon, time_horizon_obstacle)
crowd.set_agent_avoidance_weight(bullet_id, 20)  -- others yield heavily to it
```

Use this for: projectiles, tanks pushing through a crowd, any unit that should never slow down for agents.


> [!IMPORTANT]
> Neither mode guarantees physical separation. ORCA only computes avoidance velocities from the configured neighbors, radii, horizons, and timestep; your game may still need placement rules, collision handling, or correction logic.

**Choosing the right control**

| Goal | Use |
|---|---|
| Permanent unit hierarchy (infantry vs horse vs tank) | `avoidance_weight` |
| Temporary urgency (selected, charging, fleeing, escorting) | `avoidance_priority` |
| Agent is idle and should block others but not move | `set_agent_avoidance_locked(id, true)` |
| Agent is moved externally each frame | `set_agent_kinematic(id, true)` |
| Agent ignores others but others still avoid it | `max_neighbors = 0` |

Ensure important agent pairs are within `neighbor_distance` and `max_neighbors` of each other. If two agents never appear in each other's neighbor list, no ORCA line is created and weight or priority has no effect on that pair.

### `crowd.set_agent_avoidance_weight()`

```lua
crowd.set_agent_avoidance_weight(agent_id, weight)
```

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| `weight` | `number` | Persistent avoidance weight; default is `1.0`. |
| Return | — | No values. |

Larger weights make the agent take a smaller share of pairwise correction. Non-positive or invalid weights are clamped to a small positive value; extreme weights are capped.

### `crowd.get_agent_avoidance_weight()`

```lua
local weight = crowd.get_agent_avoidance_weight(agent_id)
```

Returns the sanitized persistent responsibility weight.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| Return | `number` | Current sanitized avoidance weight. |

### `crowd.set_agent_avoidance_priority()`

```lua
crowd.set_agent_avoidance_priority(agent_id, priority)
```

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| `priority` | `number` | Temporary unsigned priority, `0`–`255`; default is `0`. |
| Return | — | No values. |

Use small values such as `0`–`3`; high values quickly reach the responsibility clamp. Reset temporary priority when the gameplay state ends.

### `crowd.get_agent_avoidance_priority()`

```lua
local priority = crowd.get_agent_avoidance_priority(agent_id)
```

Returns the agent's temporary avoidance priority.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| Return | `number` | Current avoidance priority. |

### `crowd.set_agent_avoidance_locked()`

```lua
crowd.set_agent_avoidance_locked(agent_id, locked)
```

Locks or unlocks avoidance movement.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| `locked` | `boolean` | `true` to lock the agent. |
| Return | — | No values. |

A locked agent keeps zero velocity, skips its own ORCA solve, and takes zero responsibility. It remains in the agent tree, so other agents avoid it. Use this for idle units, parked vehicles, or agent-shaped static blockers.

### `crowd.get_agent_avoidance_locked()`

```lua
local locked = crowd.get_agent_avoidance_locked(agent_id)
```

Reports whether the agent is acting as an avoidance-locked blocker.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| Return | `boolean` | `true` when avoidance movement is locked. |

### `crowd.set_agent_kinematic()`

```lua
crowd.set_agent_kinematic(agent_id, is_kinematic)
```

Marks an agent as externally controlled or returns it to normal simulation integration.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| `is_kinematic` | `boolean` | `true` to exclude it from its own solve and integration. |
| Return | — | No values. |

Kinematic agents remain visible to other agents but have their velocity zeroed and are not moved by `do_step()`. Update their positions externally with `crowd.set_agent_position()`.

### `crowd.get_agent_kinematic()`

```lua
local is_kinematic = crowd.get_agent_kinematic(agent_id)
```

Reports whether simulation integration is disabled for this agent.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| Return | `boolean` | `true` when the agent is kinematic. |

## Debug and introspection

Neighbor counts, neighbor IDs, and ORCA lines describe the latest completed simulation step. They are primarily intended for visualization, diagnostics, and tuning.

### `crowd.get_agent_num_agent_neighbors()`

```lua
local count = crowd.get_agent_num_agent_neighbors(agent_id)
```

Returns the number of agent neighbors retained for the latest step.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| Return | `number` | Agent-neighbor count from the latest step. |

### `crowd.get_agent_num_obstacle_neighbors()`

```lua
local count = crowd.get_agent_num_obstacle_neighbors(agent_id)
```

Returns the number of polygon obstacle neighbors retained for the latest step.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| Return | `number` | Polygon-obstacle neighbor count from the latest step. |

### `crowd.get_agent_num_circle_obstacle_neighbors()`

```lua
local count = crowd.get_agent_num_circle_obstacle_neighbors(agent_id)
```

Returns the number of circle obstacle neighbors retained for the latest step.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| Return | `number` | Circle-obstacle neighbor count from the latest step. |

### `crowd.get_agent_num_orca_lines()`

```lua
local count = crowd.get_agent_num_orca_lines(agent_id)
```

Returns the number of ORCA constraint lines generated during the latest step.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| Return | `number` | ORCA constraint-line count from the latest step. |

### `crowd.get_agent_agent_neighbor()`

```lua
local neighbor_agent_id = crowd.get_agent_agent_neighbor(agent_id, neighbor_idx)
```

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| `neighbor_idx` | `number` | Zero-based index less than `crowd.get_agent_num_agent_neighbors(agent_id)`. |
| Return | `number` | Neighbor agent ID, or `crowd.INVALID_ID` for an invalid index. |

### `crowd.get_agent_obstacle_neighbor()`

```lua
local vertex_id = crowd.get_agent_obstacle_neighbor(agent_id, neighbor_idx)
```

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| `neighbor_idx` | `number` | Zero-based index less than `crowd.get_agent_num_obstacle_neighbors(agent_id)`. |
| Return | `number` | Polygon obstacle vertex ID, or `crowd.INVALID_ID` for an invalid index. |

The returned ID may be a BSP split record. It is valid for geometry inspection but not as a source ID for removal or mask setters.

### `crowd.get_agent_circle_obstacle_neighbor()`

```lua
local obstacle_id = crowd.get_agent_circle_obstacle_neighbor(agent_id, neighbor_idx)
```

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| `neighbor_idx` | `number` | Zero-based index less than `crowd.get_agent_num_circle_obstacle_neighbors(agent_id)`. |
| Return | `number` | Circle obstacle ID, or `crowd.INVALID_ID` for an invalid index. |

### `crowd.get_agent_orca_line()`

```lua
local point_x, point_y, direction_x, direction_y =
    crowd.get_agent_orca_line(agent_id, line_idx)
```

Returns one ORCA half-plane boundary from the latest step.

| Name | Type | Description |
|---|---|---|
| `agent_id` | `number` | Active agent ID. |
| `line_idx` | `number` | Zero-based index less than `crowd.get_agent_num_orca_lines(agent_id)`. |
| Return 1 | `number` | Line point X coordinate. |
| Return 2 | `number` | Line point Y coordinate. |
| Return 3 | `number` | Line direction X component. |
| Return 4 | `number` | Line direction Y component. |

## Simulation

### `crowd.set_time_step()`

```lua
crowd.set_time_step(time_step)
```

Sets the integration time step used by the next manual simulation step.

| Name | Type | Description |
|---|---|---|
| `time_step` | `number` | Positive duration in seconds, normally Defold's `dt`. |
| Return | — | No values. |

Set this before every `crowd.do_step()` or `crowd.do_step_parallel()` when using the manual workflow. Managed game-object simulation sets it internally.

### `crowd.get_time_step()`

```lua
local time_step = crowd.get_time_step()
```

Returns the time step currently stored in the simulation context.

| Name | Type | Description |
|---|---|---|
| Return | `number` | Current integration time step in seconds. |

### `crowd.get_global_time()`

```lua
local global_time = crowd.get_global_time()
```

Returns accumulated simulated time since the current context was initialized.

| Name | Type | Description |
|---|---|---|
| Return | `number` | Accumulated simulation time in seconds. |

Global time advances by the configured time step after each completed simulation step.

### `crowd.do_step()`

```lua
crowd.do_step()
```

Advances the simulation by one time step: rebuilds the agent tree, computes neighbors, generates and solves ORCA constraints, and integrates active agent positions.

| Name | Type | Description |
|---|---|---|
| Return | — | No values. |

Requires an active context and a positive time step. Set manual agents' preferred velocities before calling it.

### `crowd.do_step_parallel()`

```lua
crowd.do_step_parallel()
```

Advances the simulation using the Defold JobSystem when the extension was built with JobSystem support and `crowd.init()` successfully created workers.

| Name | Type | Description |
|---|---|---|
| Return | — | No values. |

The binding transparently falls back to `crowd.do_step()` when JobSystem support or a valid job context is unavailable. The core may also choose the sequential path for small crowds or timing-enabled builds. The same time-step and preferred-velocity requirements as `crowd.do_step()` apply.

## Spatial queries

### `crowd.query_agents_in_radius()`

```lua
local agent_ids, count = crowd.query_agents_in_radius(x, y, radius [, nav_mask])
```

Finds active agents within `radius` of `(x, y)`, using the agent KD-tree built by the most recent `crowd.do_step()` / `crowd.do_step_parallel()`. Pure center-distance test; agent radius is not included.

| Name | Type | Description |
|---|---|---|
| `x` | `number` | Query center X |
| `y` | `number` | Query center Y |
| `radius` | `number` | Query radius. `0` or negative returns no results. |
| `nav_mask` | `number` or `nil` | Navigation layer bitmask, `0`–`255`; defaults to `255` (interacts with everyone) if omitted. |
| Return 1 | `number[]` | One-based array of agent IDs within radius, closest first. Capped at the query result buffer's capacity (see below). Empty table if none match. |
| Return 2 | `number` | Number of entries written to `agent_ids` (same value as `#agent_ids`). |

Requires the agent tree to have been built at least once — on a context that has never stepped, returns an empty table and a count of `0`.

```lua
local nearby, count = crowd.query_agents_in_radius(400, 300, 150)
for i = 1, count do
    local agent_id = nearby[i]
    -- e.g. highlight, select, or apply an area-of-effect to agent_id
end
```

### `crowd.set_max_agent_query()`

```lua
crowd.set_max_agent_query(count)
```

Resizes the shared result buffer used by `crowd.query_agents_in_radius()`. `crowd.init()` sizes this buffer to `max_agents` by default (a query can never return more results than there are agents); call this only if a different capacity is needed. Must be called after `crowd.init()`. The buffer is allocated once per call to this function (or once at `crowd.init()` if never called) — not per query.

| Name | Type | Description |
|---|---|---|
| `count` | `number` | New result buffer capacity |

### `crowd.raycast_agents()`

```lua
local hits, count = crowd.raycast_agents(start_x, start_y, end_x, end_y, radius [, nav_mask])
```

Casts a finite segment from `(start_x, start_y)` to `(end_x, end_y)` against active agents, using the agent KD-tree built by the most recent `crowd.do_step()` / `crowd.do_step_parallel()`. Each candidate agent is tested with a combined radius of `radius + agent.radius`, so a `radius` of `0` tests the segment against agents' own radii only.

| Name | Type | Description |
|---|---|---|
| `start_x` | `number` | Segment start X |
| `start_y` | `number` | Segment start Y |
| `end_x` | `number` | Segment end X |
| `end_y` | `number` | Segment end Y |
| `radius` | `number` | Extra radius added to each agent's own radius. Negative returns no results. |
| `nav_mask` | `number` or `nil` | Navigation layer bitmask, `0`–`255`; defaults to `255` (interacts with everyone) if omitted. |
| Return 1 | `table[]` | One-based array of `{id = number, t = number}` tables, sorted by `t` (normalized position along the segment, `0`–`1`) ascending. Capped at the query result buffer's capacity (see below). Empty table if none match. |
| Return 2 | `number` | Number of entries written to `hits` (same value as `#hits`). |

Requires the agent tree to have been built at least once — on a context that has never stepped, returns an empty table and a count of `0`.

```lua
local hits, count = crowd.raycast_agents(0, 0, 800, 0, 0)
for i = 1, count do
    local hit = hits[i]
    -- hit.id is the agent id, hit.t is where along the segment it was struck
end
```

### `crowd.set_max_raycast_query()`

```lua
crowd.set_max_raycast_query(count)
```

Resizes the shared result buffer used by `crowd.raycast_agents()`. `crowd.init()` sizes this buffer to `max_agents` by default (a query can never return more results than there are agents); call this only if a different capacity is needed. Must be called after `crowd.init()`. The buffer is allocated once per call to this function (or once at `crowd.init()` if never called) — not per query.

| Name | Type | Description |
|---|---|---|
| `count` | `number` | New result buffer capacity |

### `crowd.query_circle_obstacles_in_radius()`

```lua
local obstacle_ids, count = crowd.query_circle_obstacles_in_radius(x, y, radius [, nav_mask])
```

Finds active circle obstacles within `radius` of `(x, y)`, using the circle-obstacle KD-tree built by `crowd.process_circle_obstacles()`. Pure center-distance test; the obstacle's own radius is not included — an obstacle exactly tangent to the query circle (`center_dist == radius`) is **not** included (strict `<`).

| Name | Type | Description |
|---|---|---|
| `x` | `number` | Query center X |
| `y` | `number` | Query center Y |
| `radius` | `number` | Query radius. `0` or negative returns no results. |
| `nav_mask` | `number` or `nil` | Navigation layer bitmask, `0`–`255`; defaults to `255` (interacts with everyone) if omitted. |
| Return 1 | `number[]` | One-based array of circle obstacle IDs within radius, closest first. Capped at the query result buffer's capacity (see `crowd.set_max_circle_query()`). Empty table if none match. |
| Return 2 | `number` | Number of entries written to `obstacle_ids` (same value as `#obstacle_ids`). |

Requires `crowd.process_circle_obstacles()` to have been called at least once — on a context whose circle-obstacle tree has never been built, returns an empty table and a count of `0`. Returned obstacle IDs are current slot indices, not permanent identities: a removed obstacle's ID can be reassigned to a new obstacle by a later `crowd.add_circle_obstacle()` call.

```lua
local nearby, count = crowd.query_circle_obstacles_in_radius(400, 300, 150)
for i = 1, count do
    local obstacle_id = nearby[i]
    -- e.g. highlight, or check line-of-sight against obstacle_id
end
```

### `crowd.query_circle_obstacles_colliding()`

```lua
local obstacle_ids, count = crowd.query_circle_obstacles_colliding(x, y, radius [, nav_mask])
```

Finds active circle obstacles overlapping a query circle of `radius` centered at `(x, y)`, using the circle-obstacle KD-tree built by `crowd.process_circle_obstacles()`. True circle-vs-circle overlap test: an obstacle matches if `distance(query center, obstacle center) < radius + obstacle.radius` — strict `<`, so an obstacle exactly touching the query circle's edge is **not** included.

| Name | Type | Description |
|---|---|---|
| `x` | `number` | Query center X |
| `y` | `number` | Query center Y |
| `radius` | `number` | Query radius. `0` or negative returns no results. |
| `nav_mask` | `number` or `nil` | Navigation layer bitmask, `0`–`255`; defaults to `255` (interacts with everyone) if omitted. |
| Return 1 | `number[]` | One-based array of circle obstacle IDs overlapping the query circle, ascending by center distance. Capped at the query result buffer's capacity (see `crowd.set_max_circle_query()`). Empty table if none match. |
| Return 2 | `number` | Number of entries written to `obstacle_ids` (same value as `#obstacle_ids`). |

Requires `crowd.process_circle_obstacles()` to have been called at least once — on a context whose circle-obstacle tree has never been built, returns an empty table and a count of `0`. Returned obstacle IDs are current slot indices, not permanent identities: a removed obstacle's ID can be reassigned to a new obstacle by a later `crowd.add_circle_obstacle()` call.

Unlike `crowd.query_circle_obstacles_in_radius()`, this also matches large obstacles whose center lies outside `radius` but whose edge overlaps the query circle.

### `crowd.set_max_circle_query()`

```lua
crowd.set_max_circle_query(count)
```

Resizes the shared result buffer used by `crowd.query_circle_obstacles_in_radius()` and `crowd.query_circle_obstacles_colliding()`. `crowd.init()` sizes this buffer to `max_circle_obstacles` by default; call this only if a different capacity is needed. The requested count is clamped to `max_circle_obstacles` — a query can never return more matches than obstacles that exist. Must be called after `crowd.init()`.

| Name | Type | Description |
|---|---|---|
| `count` | `number` | New result buffer capacity (clamped to `max_circle_obstacles`) |

### `crowd.raycast_circle_obstacles()`

```lua
local hits, count = crowd.raycast_circle_obstacles(start_x, start_y, end_x, end_y, radius [, nav_mask])
```

Casts a finite segment from `(start_x, start_y)` to `(end_x, end_y)` against active circle obstacles, using the circle-obstacle KD-tree built by `crowd.process_circle_obstacles()`. Each candidate obstacle is tested with a combined radius of `radius + obstacle.radius`, so a `radius` of `0` tests the segment against obstacles' own radii only.

| Name | Type | Description |
|---|---|---|
| `start_x` | `number` | Segment start X |
| `start_y` | `number` | Segment start Y |
| `end_x` | `number` | Segment end X |
| `end_y` | `number` | Segment end Y |
| `radius` | `number` | Extra radius added to each obstacle's own radius. Negative returns no results. |
| `nav_mask` | `number` or `nil` | Navigation layer bitmask, `0`–`255`; defaults to `255` (interacts with everyone) if omitted. |
| Return 1 | `table[]` | One-based array of `{id = number, t = number}` tables, sorted by `t` (normalized position along the segment, `0`–`1`) ascending, with obstacle id as a deterministic tiebreak. Capped at the query result buffer's capacity (see below). Empty table if none match. |
| Return 2 | `number` | Number of entries written to `hits` (same value as `#hits`). |

Requires `crowd.process_circle_obstacles()` to have been called at least once — on a context whose circle-obstacle tree has never been built, returns an empty table and a count of `0`. Returned obstacle IDs are current slot indices, not permanent identities: a removed obstacle's ID can be reassigned to a new obstacle by a later `crowd.add_circle_obstacle()` call.

```lua
local hits, count = crowd.raycast_circle_obstacles(0, 0, 800, 0, 0)
for i = 1, count do
    local hit = hits[i]
    -- hit.id is the obstacle id, hit.t is where along the segment it was struck
end
```

### `crowd.set_max_circle_raycast_query()`

```lua
crowd.set_max_circle_raycast_query(count)
```

Resizes the shared result buffer used by `crowd.raycast_circle_obstacles()`. `crowd.init()` sizes this buffer to `max_circle_obstacles` by default (a query can never return more results than there are circle obstacles); call this only if a different capacity is needed. The requested count is clamped to `max_circle_obstacles`. Must be called after `crowd.init()`.

| Name | Type | Description |
|---|---|---|
| `count` | `number` | New result buffer capacity (clamped to `max_circle_obstacles`) |

---

## JobSystem and parallel simulation

This section explains how the library uses background threads, when parallel dispatch actually runs, and what happens when it cannot.

### Workers and lanes

`max_workers` in `crowd.init()` sets the total number of simulation lanes, **including the main thread**. Passing `max_workers = 3` requests 1 main thread + 2 background worker threads.

Background workers are created internally at init time. The actual number of workers created may be less than requested (the OS decides). If that happens, the library uses however many were actually created. The main thread always counts as one lane regardless.

Example:

```lua
crowd.init(1000, 16, 0, 0, 3)
-- Requests 3 lanes: main thread + 2 background workers.
-- If the OS only creates 1 background worker, the library runs with 2 lanes total.
```

### Parallel dispatch threshold

Even with workers available, `crowd.do_step_parallel()` only dispatches jobs when the active agent count is **500 or more**.

Below 500 agents, the overhead of splitting work into chunks, dispatching jobs, and waiting for them to finish costs more time than the parallelism saves. The sequential path is faster for small crowds. This threshold was determined by benchmarking agent counts from 100 to 2500 on Apple Silicon; re-benchmark if targeting other hardware.

In practice: use `crowd.do_step()` for crowds below 500 agents, and `crowd.do_step_parallel()` for crowds at or above 500.

### Fallback behaviour

The library falls back to single-threaded `crowd.do_step()` automatically in each of these cases:

| Condition | Result |
|---|---|
| Build without JobSystem support | Always single-threaded |
| `max_workers = 1` (default) | Single-threaded, no workers created |
| Worker creation fails at runtime | Single-threaded, error logged |
| Workers created but count is 0 | Single-threaded, workers destroyed |
| Agent count below 500 | Sequential path chosen automatically |

No Lua-side handling is required. `crowd.do_step_parallel()` always produces the correct simulation result; it just may run on fewer threads than requested.

### Platform limitation: HTML5 (WebAssembly)

On HTML5 builds, background threads require the browser to have `SharedArrayBuffer` enabled and the extension to be compiled with Emscripten pthreads support.

If pthreads support is not compiled in, the library detects this at init time and stays single-threaded regardless of `max_workers`. This is transparent — no error is raised and `crowd.do_step_parallel()` works correctly as a single-threaded step.

If you target HTML5 and want parallel simulation, ensure your Defold project builds with wasm_pthread-web (WebAssembly + Thread support).
