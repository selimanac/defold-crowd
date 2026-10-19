# defold-crowd-c — Public API Reference

All symbols live in the `crowd` namespace. Include `crowd.h` to access the full API.

```cpp
#include "crowd.h"
```

---

## Types

### `Vec2`

2D float vector used for positions and velocities.

```cpp
struct Vec2 { float x; float y; };
```

| Constructor | Description |
|---|---|
| `Vec2()` | Zero-initialises both components |
| `Vec2(float x, float y)` | Initialises with given components |

---

### `OrcaLine`

A half-plane constraint produced by the ORCA solver, accessible via `get_agent_orca_line()`.

```cpp
struct OrcaLine { Vec2 point; Vec2 direction; };
```

| Field | Description |
|---|---|
| `point` | A point on the boundary line in velocity space |
| `direction` | Unit tangent of the boundary line |

---

### `AgentNeighbor`

One result entry from `query_agents_in_radius()` — an agent id paired with its squared distance from the query point.

```cpp
struct AgentNeighbor { float distance_sq; uint16_t agent_id; };
```

| Field | Description |
|---|---|
| `distance_sq` | Squared distance from the query center to the agent's position |
| `agent_id` | Matching agent's id |

---

### `AgentRaycastHit`

One result entry from `raycast_agents()` — an agent id paired with the normalized position along the segment where it was struck.

```cpp
struct AgentRaycastHit { float t; uint16_t agent_id; uint8_t _pad[2]; };
```

| Field | Description |
|---|---|
| `t` | Normalized position along `start -> end`, in `[0, 1]` |
| `agent_id` | Matching agent's id |

---

### `CircleObstacleNeighbor`

One result entry from `query_circle_obstacles_in_radius()` / `query_circle_obstacles_colliding()` — a circle obstacle id paired with its squared center distance from the query point.

```cpp
struct CircleObstacleNeighbor { float center_dist_sq; uint16_t obs_id; };
```

| Field | Description |
|---|---|
| `center_dist_sq` | Squared distance from the query center to the obstacle's center |
| `obs_id` | Matching circle obstacle's id (current slot index, not a permanent identity) |

---

### `CircleObstacleRaycastHit`

One result entry from `raycast_circle_obstacles()` — a circle obstacle id paired with the normalized position along the segment where it was struck.

```cpp
struct CircleObstacleRaycastHit { float t; uint16_t obs_id; uint8_t _pad[2]; };
```

| Field | Description |
|---|---|
| `t` | Normalized position along `start -> end`, in `[0, 1]` |
| `obs_id` | Matching circle obstacle's id (current slot index, not a permanent identity) |

---

### `CrowdResult`

Return code for operations that can fail.

| Value | Meaning |
|---|---|
| `CROWD_RESULT_OK` | Success |
| `CROWD_RESULT_INVALID_ARGUMENT` | NULL context or out-of-range parameter |
| `CROWD_RESULT_OUT_OF_MEMORY` | Allocation failed |
| `CROWD_RESULT_OBSTACLE_CAPACITY_EXHAUSTED` | Obstacle array is full (BSP split records included) |
| `CROWD_RESULT_OBSTACLE_SCRATCH_EXHAUSTED` | BSP build scratch exhausted |
| `CROWD_RESULT_OBSTACLE_DEGENERATE` | Zero-length edge detected during obstacle add |

```cpp
const char* result_to_string(CrowdResult result);
```

Converts a `CrowdResult` to a human-readable C string.

---

### `CrowdMemoryUsage`

Output struct filled by `calculate_memory_usage()` and `get_memory_usage()`. All `*_bytes` fields report requested allocation size excluding allocator bookkeeping.

| Key fields | Description |
|---|---|
| `max_agents` | Agent capacity |
| `max_obstacles` | Polygon obstacle record capacity (source × 4) |
| `max_circle_obstacles` | Circle obstacle capacity |
| `max_neighbors` | Per-agent neighbor scratch cap |
| `max_orca_line_cap` | Per-agent ORCA line cap |
| `total_bytes` | Sum of all allocation sizes |

---

## Constants

### Nav-mask sentinels

| Constant | Value | Meaning |
|---|---|---|
| `NAV_MASK_NONE` | `0x00` | Agent or obstacle is invisible to all |
| `NAV_MASK_ALL` | `0xFF` | Agent or obstacle is visible to everyone (default) |

Bits 0–7 are free for the game to define. Combine with `|` for multi-layer membership.

### Sentinel IDs

| Constant | Value | Meaning |
|---|---|---|
| `AGENT_ID_NONE` | `0xFFFF` | Invalid / not-found agent id |
| `OBSTACLE_ID_NONE` | `0xFFFF` | Invalid / not-found obstacle id |

### Agent flags (read via `get_agent_avoidance_locked` / `get_agent_kinematic`)

| Constant | Bit | Meaning |
|---|---|---|
| `AGENT_FLAG_KINEMATIC` | 0 | Agent is controlled externally; others avoid it but it does not self-steer |
| `AGENT_FLAG_AVOIDANCE_LOCKED` | 1 | Agent takes zero ORCA responsibility; velocity is zeroed every step |
| `AGENT_FLAG_INACTIVE` | 3 | Agent slot has been removed; ignored in all simulation passes |

### Obstacle flags

| Constant | Bit | Meaning |
|---|---|---|
| `OBSTACLE_FLAG_CONVEX` | 0 | Polygon vertex is convex (set automatically by `add_obstacle`) |
| `OBSTACLE_FLAG_INACTIVE` | 1 | Obstacle has been removed |
| `CIRCLE_OBS_FLAG_INACTIVE` | 0 | Circle obstacle has been removed |

---

## Lifecycle


### `create_context()`

```cpp
CrowdContext* create_context(
    uint16_t max_agents,
    uint16_t max_obstacle_vertices,
    uint16_t max_circle_obstacles,
    uint8_t  max_neighbors,
    uint8_t  max_workers
);
```

Allocates a fixed-capacity simulation context. All backing memory is reserved in one call; nothing is reallocated at runtime.

| Parameter | Type | Description |
|---|---|---|
| `max_agents` | `uint16_t` | Maximum agent capacity |
| `max_obstacle_vertices` | `uint16_t` | Source polygon vertex capacity across all obstacles. Pass `0` to disable polygon obstacle support. Maximum: `(65534 − max_neighbors) / 4` |
| `max_circle_obstacles` | `uint16_t` | Circle obstacle capacity. Pass `0` to disable |
| `max_neighbors` | `uint8_t` | Per-agent neighbor scratch cap (mirrors RVO2 `maxNeighbors`). Must be ≥ 1 |
| `max_workers` | `uint8_t` | Worker thread count for `do_step_parallel`. Must be ≥ 1 |

Returns a new `CrowdContext*` on success, or `NULL` on invalid arguments or allocation failure.

---

### `destroy_context()`

```cpp
void destroy_context(CrowdContext* crowd_context);
```

Frees all memory owned by the context. Passing `NULL` is safe and has no effect.

---

## Memory

### `calculate_memory_usage()`

```cpp
CrowdResult calculate_memory_usage(
    uint16_t          max_agents,
    uint16_t          max_obstacle_vertices,
    uint16_t          max_circle_obstacles,
    uint8_t           max_neighbors,
    uint8_t           max_workers,
    CrowdMemoryUsage* out_usage
);
```

Computes the memory that `create_context()` would reserve for the given parameters without allocating anything.

| Parameter | Type | Description |
|---|---|---|
| `max_agents` … `max_workers` | — | Same parameters as `create_context()` |
| `out_usage` | `CrowdMemoryUsage*` | Receives the breakdown of requested bytes |

---

### `get_memory_usage()`

```cpp
CrowdResult get_memory_usage(const CrowdContext* crowd_context, CrowdMemoryUsage* out_usage);
```

Reports the memory reserved by an existing context.

| Parameter | Type | Description |
|---|---|---|
| `crowd_context` | `const CrowdContext*` | Simulation context |
| `out_usage` | `CrowdMemoryUsage*` | Receives the breakdown of reserved bytes |

---

## Agents

### `add_agent()`

```cpp
uint16_t add_agent(
    CrowdContext* crowd_context,
    Vec2     position,
    float    radius,
    float    max_speed,
    float    neighbor_distance,
    float    time_horizon,
    float    time_horizon_obstacle,
    uint8_t  max_neighbors
);
```

Adds an agent to the simulation. Can be called at any time while slots remain.

| Parameter | Type | Description |
|---|---|---|
| `crowd_context` | `CrowdContext*` | Simulation context |
| `position` | `Vec2` | Initial world-space position |
| `radius` | `float` | Agent radius |
| `max_speed` | `float` | Maximum speed |
| `neighbor_distance` | `float` | Radius within which other agents are considered neighbors |
| `time_horizon` | `float` | Lookahead time for agent–agent avoidance (seconds) |
| `time_horizon_obstacle` | `float` | Lookahead time for obstacle avoidance (seconds) |
| `max_neighbors` | `uint8_t` | Maximum number of agent neighbors this agent tracks per step |

Returns the agent id, or `AGENT_ID_NONE` on capacity overflow.

---

### `remove_agent()`

```cpp
bool remove_agent(CrowdContext* crowd_context, uint16_t agent_id);
```

Marks an agent inactive (`AGENT_FLAG_INACTIVE`). The slot may be reused by a future `add_agent()` call.

| Parameter | Type | Description |
|---|---|---|
| `crowd_context` | `CrowdContext*` | Simulation context |
| `agent_id` | `uint16_t` | Id from `add_agent()` |

Returns `true` on success, `false` if the id is invalid or already inactive.

---

### `get_agent_count()`

```cpp
uint16_t get_agent_count(const CrowdContext* crowd_context);
```

Returns the number of currently active agents.

---

### `set_agent_pref_velocity` / `get_agent_pref_velocity`

```cpp
void  set_agent_pref_velocity(CrowdContext* ctx, uint16_t agent_id, Vec2 pref_velocity);
Vec2  get_agent_pref_velocity(const CrowdContext* ctx, uint16_t agent_id);
```

The velocity the agent would like to have if there were no other agents or obstacles. Set every frame before `do_step()`.

---

### `set_agent_position` / `get_agent_position`

```cpp
void set_agent_position(CrowdContext* ctx, uint16_t agent_id, Vec2 position);
Vec2 get_agent_position(const CrowdContext* ctx, uint16_t agent_id);
```

The agent's current world-space position. `do_step()` writes this each frame; override only for teleport or respawn.

---

### `set_agent_velocity` / `get_agent_velocity`

```cpp
void set_agent_velocity(CrowdContext* ctx, uint16_t agent_id, Vec2 velocity);
Vec2 get_agent_velocity(const CrowdContext* ctx, uint16_t agent_id);
```

The agent's current velocity. `do_step()` sets this to `new_velocity` after the ORCA solve.

---

### `set_agent_max_speed` / `get_agent_max_speed`

```cpp
void  set_agent_max_speed(CrowdContext* ctx, uint16_t agent_id, float max_speed);
float get_agent_max_speed(const CrowdContext* ctx, uint16_t agent_id);
```

Maximum speed used as the ORCA speed-disk radius.

---

### `set_agent_radius` / `get_agent_radius`

```cpp
void  set_agent_radius(CrowdContext* ctx, uint16_t agent_id, float radius);
float get_agent_radius(const CrowdContext* ctx, uint16_t agent_id);
```

Physical radius used for collision geometry and ORCA constraint generation.

---

### `set_agent_neighbor_dist` / `get_agent_neighbor_dist`

```cpp
void  set_agent_neighbor_dist(CrowdContext* ctx, uint16_t agent_id, float neighbor_dist);
float get_agent_neighbor_dist(const CrowdContext* ctx, uint16_t agent_id);
```

Radius within which other agents are included as ORCA neighbors.

---

### `set_agent_max_neighbors` / `get_agent_max_neighbors`

```cpp
void    set_agent_max_neighbors(CrowdContext* ctx, uint16_t agent_id, uint8_t max_neighbors);
uint8_t get_agent_max_neighbors(const CrowdContext* ctx, uint16_t agent_id);
```

Per-agent cap on the number of agent neighbors considered. Pass `0` to disable agent–agent avoidance while keeping obstacle avoidance active.

---

### `set_agent_time_horizon` / `get_agent_time_horizon`

```cpp
void  set_agent_time_horizon(CrowdContext* ctx, uint16_t agent_id, float time_horizon);
float get_agent_time_horizon(const CrowdContext* ctx, uint16_t agent_id);
```

Lookahead time (seconds) for agent–agent ORCA constraints. Larger values produce earlier, gentler avoidance.

---

### `set_agent_time_horizon_obstacle` / `get_agent_time_horizon_obstacle`

```cpp
void  set_agent_time_horizon_obstacle(CrowdContext* ctx, uint16_t agent_id, float time_horizon_obstacle);
float get_agent_time_horizon_obstacle(const CrowdContext* ctx, uint16_t agent_id);
```

Lookahead time (seconds) for static obstacle ORCA constraints.

---

## Polygon Obstacles

### `add_obstacle()`

```cpp
uint16_t add_obstacle(CrowdContext* crowd_context, const Vec2* vertices, uint16_t vertex_count);
```

Adds a closed polygon obstacle. Vertices must be listed in counter-clockwise order. Call `process_obstacles()` afterwards.

| Parameter | Type | Description |
|---|---|---|
| `crowd_context` | `CrowdContext*` | Simulation context |
| `vertices` | `const Vec2*` | Array of CCW polygon vertices |
| `vertex_count` | `uint16_t` | Number of vertices (minimum 2) |

Returns the first obstacle record id (pass to `set_obstacle_nav_mask`, `remove_obstacle`), or `OBSTACLE_ID_NONE` on failure.

---

### `remove_obstacle()`

```cpp
bool remove_obstacle(CrowdContext* crowd_context, uint16_t obstacle_id);
```

Marks all segments of the polygon ring inactive. Call `process_obstacles()` to rebuild the tree.

| Parameter | Type | Description |
|---|---|---|
| `crowd_context` | `CrowdContext*` | Simulation context |
| `obstacle_id` | `uint16_t` | Id from `add_obstacle()` |

Returns `true` on success. BSP split record ids are not valid public obstacle ids.

---

### `process_obstacles()`

```cpp
CrowdResult process_obstacles(CrowdContext* crowd_context);
```

Builds the polygon obstacle KD-tree (BSP). Call once after all obstacles are added, and again after any add or remove.

---

### `get_obstacle_count()`

```cpp
uint16_t get_obstacle_count(const CrowdContext* crowd_context);
```

Returns the total number of obstacle records (including BSP split records).

---

### `get_obstacle_vertex()` / `get_next_obstacle_vertex_id()` / `get_prev_obstacle_vertex_id()`

```cpp
Vec2     get_obstacle_vertex(const CrowdContext* ctx, uint16_t vertex_id);
uint16_t get_next_obstacle_vertex_id(const CrowdContext* ctx, uint16_t vertex_id);
uint16_t get_prev_obstacle_vertex_id(const CrowdContext* ctx, uint16_t vertex_id);
```

Access polygon obstacle geometry by vertex id. Use to iterate the ring for rendering or debugging.

| Parameter | Type | Description |
|---|---|---|
| `ctx` | `const CrowdContext*` | Simulation context |
| `vertex_id` | `uint16_t` | A vertex id (from `add_obstacle()` or neighbor query) |

`get_next_obstacle_vertex_id` and `get_prev_obstacle_vertex_id` return `OBSTACLE_ID_NONE` at open ring ends.

---

## Circle Obstacles

### `add_circle_obstacle()`

```cpp
uint16_t add_circle_obstacle(CrowdContext* crowd_context, Vec2 center, float radius);
```

Adds a circular obstacle. Call `process_circle_obstacles()` afterwards.

| Parameter | Type | Description |
|---|---|---|
| `crowd_context` | `CrowdContext*` | Simulation context |
| `center` | `Vec2` | World-space centre position |
| `radius` | `float` | Circle radius (must be > 0) |

Returns the obstacle id, or `OBSTACLE_ID_NONE` on failure.

---

### `remove_circle_obstacle()`

```cpp
bool remove_circle_obstacle(CrowdContext* crowd_context, uint16_t obstacle_id);
```

Marks a circle obstacle inactive. Call `process_circle_obstacles()` to rebuild the tree.

Returns `false` if the id is out of range or the obstacle is already inactive.

---

### `process_circle_obstacles()`

```cpp
CrowdResult process_circle_obstacles(CrowdContext* crowd_context);
```

Builds the circle obstacle KD-tree. Call after any add or remove.

---

## Nav-Layer Mask

Each agent, polygon obstacle, and circle obstacle carries a `uint8_t nav_mask`. A neighbor is only considered if `(agent.nav_mask & neighbor.nav_mask) != 0`. Defaults to `NAV_MASK_ALL`. See the [Nav-Layer Filtering](#) section in README for the full usage guide.

### `set_agent_nav_mask` / `get_agent_nav_mask`

```cpp
void    set_agent_nav_mask(CrowdContext* ctx, uint16_t agent_id, uint8_t nav_mask);
uint8_t get_agent_nav_mask(const CrowdContext* ctx, uint16_t agent_id);
```

Sets or reads the nav-layer membership mask for an agent.

---

### `set_obstacle_nav_mask` / `get_obstacle_nav_mask`

```cpp
void    set_obstacle_nav_mask(CrowdContext* ctx, uint16_t obstacle_id, uint8_t nav_mask);
uint8_t get_obstacle_nav_mask(const CrowdContext* ctx, uint16_t obstacle_id);
```

Sets or reads the nav-layer mask for a polygon obstacle ring. The setter walks the full ring and updates all segments including any BSP split records. `obstacle_id` must be a source id returned by `add_obstacle()`; BSP split record ids are rejected. Can be called before or after `process_obstacles()`.

---

### `set_circle_obstacle_nav_mask` / `get_circle_obstacle_nav_mask`

```cpp
void    set_circle_obstacle_nav_mask(CrowdContext* ctx, uint16_t obstacle_id, uint8_t nav_mask);
uint8_t get_circle_obstacle_nav_mask(const CrowdContext* ctx, uint16_t obstacle_id);
```

Sets or reads the nav-layer mask for a circle obstacle. Both return `NAV_MASK_NONE` / do nothing if the obstacle is inactive.

---

## Avoidance Tuning

### `set_agent_avoidance_weight` / `get_agent_avoidance_weight`

```cpp
void  set_agent_avoidance_weight(CrowdContext* ctx, uint16_t agent_id, float weight);
float get_agent_avoidance_weight(const CrowdContext* ctx, uint16_t agent_id);
```

Physical mass tier of the agent. Heavier agents take a smaller share of each pairwise ORCA correction. Default `1.0`. Invalid or non-positive values are clamped to `EPSILON`; values above `AVOIDANCE_WEIGHT_MAX` are capped.

---

### `set_agent_avoidance_priority` / `get_agent_avoidance_priority`

```cpp
void    set_agent_avoidance_priority(CrowdContext* ctx, uint16_t agent_id, uint8_t priority);
uint8_t get_agent_avoidance_priority(const CrowdContext* ctx, uint16_t agent_id);
```

Situational urgency layered on top of weight: `effective_weight = weight × (1 + priority)`. Use small integers (`0`–`3`). Default `0`.

---

### `set_agent_avoidance_locked` / `get_agent_avoidance_locked`

```cpp
void set_agent_avoidance_locked(CrowdContext* ctx, uint16_t agent_id, bool locked);
bool get_agent_avoidance_locked(const CrowdContext* ctx, uint16_t agent_id);
```

When locked, the agent's velocity is zeroed each step and it takes zero ORCA responsibility; other agents still avoid it. Use for idle RTS units, parked vehicles, and placed structures.

---

### `set_agent_kinematic` / `get_agent_kinematic`

```cpp
void set_agent_kinematic(CrowdContext* ctx, uint16_t agent_id, bool is_kinematic);
bool get_agent_kinematic(const CrowdContext* ctx, uint16_t agent_id);
```

Kinematic agents are moved externally. `do_step()` zeros their velocity and skips their ORCA pass; other agents still avoid them. Use for scripted movers or game objects whose transform is owned outside the simulation.

---

## Debug / Introspection

### Neighbor and ORCA line counts

```cpp
uint16_t get_agent_num_agent_neighbors(const CrowdContext* ctx, uint16_t agent_id);
uint16_t get_agent_num_obstacle_neighbors(const CrowdContext* ctx, uint16_t agent_id);
uint16_t get_agent_num_circle_obstacle_neighbors(const CrowdContext* ctx, uint16_t agent_id);
uint16_t get_agent_num_orca_lines(const CrowdContext* ctx, uint16_t agent_id);
```

Number of neighbors and ORCA lines recorded for an agent after the last `do_step()`.

---

### Indexed neighbor accessors

```cpp
uint16_t get_agent_agent_neighbor(const CrowdContext* ctx, uint16_t agent_id, uint16_t neighbor_idx);
uint16_t get_agent_obstacle_neighbor(const CrowdContext* ctx, uint16_t agent_id, uint16_t neighbor_idx);
uint16_t get_agent_circle_obstacle_neighbor(const CrowdContext* ctx, uint16_t agent_id, uint16_t neighbor_idx);
```

Returns the id of the neighbor at `neighbor_idx` (0-based, must be < the corresponding count). Use for debug visualisation of neighbor links.

---

### `get_agent_orca_line()`

```cpp
OrcaLine get_agent_orca_line(const CrowdContext* ctx, uint16_t agent_id, uint16_t line_idx);
```

Returns the ORCA half-plane at `line_idx` (0-based, must be < `get_agent_num_orca_lines()`). Use for debug visualisation of velocity constraints.

---

## Simulation

### `set_time_step` / `get_time_step`

```cpp
void  set_time_step(CrowdContext* crowd_context, float time_step);
float get_time_step(const CrowdContext* crowd_context);
```

The integration time step in seconds. Set before each `do_step()` (typically to `dt` from the game loop). Must be > 0.

---

### `get_global_time()`

```cpp
float get_global_time(const CrowdContext* crowd_context);
```

Accumulated simulation time in seconds (sum of all time steps).

---

### `do_step()`

```cpp
void do_step(CrowdContext* crowd_context);
```

Advances the simulation by one time step: rebuilds the agent KD-tree, computes neighbors and ORCA velocities for all active agents, then integrates positions. Requires `time_step > 0`.

---

### `do_step_parallel()`

```cpp
// Pro build only — requires CROWD_HAS_JOBSYSTEM (ENABLE_JOBSYSTEM=ON)
void do_step_parallel(CrowdContext* crowd_context, HJobContext job_system_context);
```

Parallel simulation step using the Defold JobSystem. Distributes neighbor computation and velocity solve across worker threads, then integrates positions. Falls back to `do_step()` for small agent counts (below `PARALLEL_AGENT_THRESHOLD = 500`), timing-enabled builds, or an invalid job context.

| Parameter | Type | Description |
|---|---|---|
| `crowd_context` | `CrowdContext*` | Simulation context |
| `job_system_context` | `HJobContext` | Defold JobSystem context; `max_workers` in the context must match the worker count of this job system |

---

## Spatial Queries

### `query_agents_in_radius()`

```cpp
uint16_t query_agents_in_radius(const CrowdContext* crowd_context, Vec2 center, float radius,
                                 uint8_t nav_mask, AgentNeighbor* out_neighbors, uint16_t max_count);
```

Finds active agents within `radius` of `center`, using the agent KD-tree built by the most recent `do_step()` / `do_step_parallel()`. Pure center-distance test — agent radius is not included in the range check.

| Parameter | Type | Description |
|---|---|---|
| `crowd_context` | `const CrowdContext*` | Simulation context |
| `center` | `Vec2` | Query center |
| `radius` | `float` | Query radius. `<= 0` returns `0` results. |
| `nav_mask` | `uint8_t` | Only agents sharing a layer with this mask are matched (`nav_mask & agent.nav_mask`); `NAV_MASK_NONE` returns `0` results |
| `out_neighbors` | `AgentNeighbor*` | Caller-owned output buffer, capacity `max_count` |
| `max_count` | `uint16_t` | Capacity of `out_neighbors`. If more agents match, only the closest `max_count` are kept. |

Returns the number of entries written to `out_neighbors`, sorted by `distance_sq` ascending.

Requires at least one prior `do_step()` / `do_step_parallel()` call to have built the agent tree; returns `0` on a context that has never stepped. Does not rebuild the tree and does not mutate any shared `CrowdContext` scratch state (`agent_neighbors`, `agent_neighbor_counts`, query timing stats) — safe to call concurrently from multiple threads provided each caller supplies its own `out_neighbors` buffer and the context isn't being mutated at the same time (e.g. not during `do_step_parallel`'s parallel phase).

---

### `raycast_agents()`

```cpp
uint16_t raycast_agents(const CrowdContext* crowd_context, Vec2 start, Vec2 end, float radius,
                         uint8_t nav_mask, AgentRaycastHit* out_hits, uint16_t max_count);
```

Casts a finite segment from `start` to `end` against active agents, using the agent KD-tree built by the most recent `do_step()` / `do_step_parallel()`. Each candidate agent is tested with a combined radius of `radius + agent.radius`, so a `radius` of `0` tests the segment against agents' own radii only. A perpendicular distance exactly equal to the combined radius (tangent) is treated as a miss (strict `<`).

| Parameter | Type | Description |
|---|---|---|
| `crowd_context` | `const CrowdContext*` | Simulation context |
| `start` | `Vec2` | Segment start |
| `end` | `Vec2` | Segment end |
| `radius` | `float` | Extra radius added to each agent's own radius. `< 0` returns `0` results. |
| `nav_mask` | `uint8_t` | Only agents sharing a layer with this mask are matched (`nav_mask & agent.nav_mask`); `NAV_MASK_NONE` returns `0` results |
| `out_hits` | `AgentRaycastHit*` | Caller-owned output buffer, capacity `max_count` |
| `max_count` | `uint16_t` | Capacity of `out_hits`. If more agents match, only the earliest `max_count` hits (by `t`) are kept. |

Returns the number of entries written to `out_hits`, sorted by `t` ascending, with `agent_id` as a deterministic tiebreak for equal `t`. A zero-length segment (`start == end`) degrades to a point-in-circle test.

Requires at least one prior `do_step()` / `do_step_parallel()` call to have built the agent tree; returns `0` on a context that has never stepped. Does not rebuild the tree and does not mutate any shared `CrowdContext` scratch state — safe to call concurrently from multiple threads provided each caller supplies its own `out_hits` buffer and the context isn't being mutated at the same time (e.g. not during `do_step_parallel`'s parallel phase).

---

### `query_circle_obstacles_in_radius()`

```cpp
uint16_t query_circle_obstacles_in_radius(const CrowdContext* crowd_context, Vec2 center, float radius,
                                           uint8_t nav_mask, CircleObstacleNeighbor* out_neighbors, uint16_t max_count);
```

Finds active circle obstacles within `radius` of `center`, using the circle-obstacle KD-tree built by `process_circle_obstacles()`. Pure center-distance test — the obstacle's own radius is not included in the range check. Strict `<` boundary: an obstacle exactly tangent to the query circle (`center_dist_sq == radius * radius`) is not matched.

| Parameter | Type | Description |
|---|---|---|
| `crowd_context` | `const CrowdContext*` | Simulation context |
| `center` | `Vec2` | Query center |
| `radius` | `float` | Query radius. `<= 0` returns `0` results. |
| `nav_mask` | `uint8_t` | Only obstacles sharing a layer with this mask are matched (`nav_mask & obstacle.nav_mask`); `NAV_MASK_NONE` returns `0` results |
| `out_neighbors` | `CircleObstacleNeighbor*` | Caller-owned output buffer, capacity `max_count` |
| `max_count` | `uint16_t` | Capacity of `out_neighbors`. If more obstacles match, only the closest `max_count` are kept. |

Returns the number of entries written to `out_neighbors`, sorted by `center_dist_sq` ascending.

Requires `process_circle_obstacles()` to have been called at least once; returns `0` if the circle-obstacle tree has never been built. Returned `obs_id`s are current slot indices, not permanent identities — a removed obstacle's id can be reassigned to a new obstacle by a later `add_circle_obstacle()` call.

---

### `query_circle_obstacles_colliding()`

```cpp
uint16_t query_circle_obstacles_colliding(const CrowdContext* crowd_context, Vec2 center, float radius,
                                           uint8_t nav_mask, CircleObstacleNeighbor* out_neighbors, uint16_t max_count);
```

Finds active circle obstacles overlapping a query circle of `radius` centered at `center`, using the circle-obstacle KD-tree built by `process_circle_obstacles()`. True circle-vs-circle overlap test: an obstacle matches if `distance(center, obstacle.center) < radius + obstacle.radius`. Strict `<` boundary: an obstacle exactly touching the query circle's edge is not matched.

| Parameter | Type | Description |
|---|---|---|
| `crowd_context` | `const CrowdContext*` | Simulation context |
| `center` | `Vec2` | Query center |
| `radius` | `float` | Query radius. `<= 0` returns `0` results. |
| `nav_mask` | `uint8_t` | Only obstacles sharing a layer with this mask are matched (`nav_mask & obstacle.nav_mask`); `NAV_MASK_NONE` returns `0` results |
| `out_neighbors` | `CircleObstacleNeighbor*` | Caller-owned output buffer, capacity `max_count` |
| `max_count` | `uint16_t` | Capacity of `out_neighbors`. If more obstacles match, only the closest `max_count` (by center distance) are kept. |

Returns the number of entries written to `out_neighbors`, sorted by `center_dist_sq` ascending. Unlike `query_circle_obstacles_in_radius()`, this also matches large obstacles whose center lies outside `radius` but whose edge overlaps the query circle.

Requires `process_circle_obstacles()` to have been called at least once; returns `0` if the circle-obstacle tree has never been built. Returned `obs_id`s are current slot indices, not permanent identities — a removed obstacle's id can be reassigned to a new obstacle by a later `add_circle_obstacle()` call.

---

### `raycast_circle_obstacles()`

```cpp
uint16_t raycast_circle_obstacles(const CrowdContext* crowd_context, Vec2 start, Vec2 end, float radius,
                                   uint8_t nav_mask, CircleObstacleRaycastHit* out_hits, uint16_t max_count);
```

Casts a finite segment from `start` to `end` against active circle obstacles, using the circle-obstacle KD-tree built by `process_circle_obstacles()`. Each candidate obstacle is tested with a combined radius of `radius + obstacle.radius`, so a `radius` of `0` tests the segment against obstacles' own radii only. A perpendicular distance exactly equal to the combined radius (tangent) is treated as a miss (strict `<`).

| Parameter | Type | Description |
|---|---|---|
| `crowd_context` | `const CrowdContext*` | Simulation context |
| `start` | `Vec2` | Segment start |
| `end` | `Vec2` | Segment end |
| `radius` | `float` | Extra radius added to each obstacle's own radius. `< 0` returns `0` results. |
| `nav_mask` | `uint8_t` | Only obstacles sharing a layer with this mask are matched (`nav_mask & obstacle.nav_mask`); `NAV_MASK_NONE` returns `0` results |
| `out_hits` | `CircleObstacleRaycastHit*` | Caller-owned output buffer, capacity `max_count` |
| `max_count` | `uint16_t` | Capacity of `out_hits`. If more obstacles match, only the earliest `max_count` hits (by `t`) are kept. |

Returns the number of entries written to `out_hits`, sorted by `t` ascending, with `obs_id` as a deterministic tiebreak for equal `t`. A zero-length segment (`start == end`) degrades to a point-in-circle test.

Requires `process_circle_obstacles()` to have been called at least once; returns `0` if the circle-obstacle tree has never been built. Does not rebuild the tree and does not mutate any shared `CrowdContext` scratch state — safe to call concurrently from multiple threads provided each caller supplies its own `out_hits` buffer and the context isn't being mutated at the same time. Returned `obs_id`s are current slot indices, not permanent identities — a removed obstacle's id can be reassigned to a new obstacle by a later `add_circle_obstacle()` call.

---

## Build Info

### `CrowdBuildFeature`

Bitmask enum reporting which optional features were compiled into the library.

```cpp
enum CrowdBuildFeature
{
    CROWD_BUILD_FEATURE_JOBSYSTEM = 1U << 0U
};
```

| Value | Meaning |
|---|---|
| `CROWD_BUILD_FEATURE_JOBSYSTEM` | Library was built with `ENABLE_JOBSYSTEM=ON`; `do_step_parallel()` is available |

---

### `get_build_features()`

```cpp
uint32_t get_build_features();
```

Returns a bitmask of `CrowdBuildFeature` values indicating which optional features are present in this build. Use on the extension side to detect the Lite vs Pro variant at runtime before calling `do_step_parallel()`.

```cpp
if (crowd::get_build_features() & crowd::CROWD_BUILD_FEATURE_JOBSYSTEM)
{
    crowd::do_step_parallel(ctx, job_system);
}
else
{
    crowd::do_step(ctx);
}
```
