![Defold Crowd](/.github/crowd_1160.png?raw=true)

# Defold Crowd Native Extension

> [!IMPORTANT]
> This is the **Standard** version of the Defold Crowd Extension without Job System support. The **Pro** version with Job System support is available only to past and current GitHub Sponsors.


## What is this?

Defold Crowd is a native extension for local crowd avoidance in Defold. It helps many circular agents move through the same 2D space while avoiding each other and static obstacles.

You provide positions, radii, speeds, neighbor settings, and desired movement. The extension computes avoidance velocities and updated positions for the next simulation step.

## What is not?

Defold Crowd is not a complete movement AI system.

- No pathfinding. Plug in your own A*, navmesh, waypoint graph, flow field, or route planner.
- No formations or group movement. Squads, offsets, ranks, selection logic, and coordinated movement belong in your game code.
- No universal arrival solution. Stopping, slowing down, snapping, attacking, queueing, and animation blending differ from game to game.
- No physical collision solver. ORCA computes avoidance velocities; your game may still need placement rules or overlap correction.

## Extra Features

- [Polygonal](./LUA_API.md#polygon-obstacles) and [circle](./LUA_API.md#circle-obstacles) obstacles. Polygon winding rules are documented in the Lua API.
- [Navigation masks](./LUA_API.md#navigation-masks)
- [Avoidance responsibility, Avoidance-locked, Kinematic, Dynamic agents](./LUA_API.md#avoidance-responsibility)
- [JobSystem and parallel simulation](./LUA_API.md#jobsystem-and-parallel-simulation) (Pro version only)


## Documentation

For detailed function descriptions and usage examples:

- [LUA API Reference](./LUA_API.md)
- [C/C++ API Reference](./CPP_API.md)





## Known Limitations

- **Classic ORCA head-on deadlock**: when agents meet in a perfectly symmetric situation, such as two equal agents moving directly toward each other, neither side has a clear reason to choose left or right. They can slow down, stop, or jitter in place. Break the symmetry in your game code with slightly offset goals, lane offsets, varied speed/priority, path waypoints, or a small steering bias.
- **Deadlock when agents spawn at the same position**: ORCA needs a usable separation direction between agents. If multiple agents start at exactly the same coordinates, the solver may not be able to separate them cleanly. Avoid exact overlapping spawns, reserve enough spacing for each radius, scatter spawn points slightly, or run your own overlap correction before stepping the simulation.


## Tools

Usage and Details: [README.md](./tools/README.md)

**SVG to Vertices**

`importsvg.py` converts a polygon-like SVG shape into obstacle vertices for this project.
It is meant for SVG files that describe map borders, obstacle polygons, or simple closed
shapes.

**CORS Server**

`serve_cors.py`  for testing WASM pthread bundles using a local HTTP server with CORS enabled.




## Important performance notes


- `max_neighbors` has a direct cost. Higher values can improve avoidance in dense scenes, but they also increase memory use and per-step work.
- Large `neighbor_distance` values can make agents consider too many distant agents that are not immediate threats.
- Mixed agent radii can be more expensive in dense scenes because larger agents usually need wider neighbor queries.
- For very large crowds, keep Lua-side per-agent work small. Move performance-critical steering, batching, or gameplay movement code to native code when needed.


## Technical features

The extension allocates during context setup and destruction; simulation steps use the memory reserved by the context.

- No C++ STL containers or strings.
- No classes; the core uses `struct` and `namespace`.
- No virtual functions.
- No exceptions or RTTI used by the source.
- No runtime allocation during simulation. Memory is reserved at context creation and released at destruction.
- Uses Defold allocation through `dmMemory::AlignedMalloc` / `dmMemory::AlignedFree`.
- Explicit types only; no `auto`.
- Small C/C++ standard headers only, plus Defold SDK headers.
- Error handling uses result enums instead of exceptions.
- Cache-conscious structs with `DM_ALIGNED(N)`: `Agent` is 64-byte aligned, `Obstacle` is 32-byte aligned.

## License Summary (not legally binding)

- ✅ **Standard** is free for personal, educational, experimental, and **completely free** non-monetized games or apps.
- 💲 **Pro** is paid-access only and requires **at least a one-time payment of $10 USD** via [GitHub Sponsors](https://github.com/sponsors/selimanac).
- 💲 Any monetized game/app using either version requires **at least a one-time payment of $10 USD** via [GitHub Sponsors](https://github.com/sponsors/selimanac).
- 📩 Free releases are welcome to share project info (name, genre, date, links).
- 🔒 The Software is **closed-source**; source access may be granted **upon request**.

See [License](./LICENSE.md)

## Credits

- [RVO2](https://github.com/snape/RVO2)  by [Jamie Snape](https://www.jamiesnape.io/)
