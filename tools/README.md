# Tools

## SVG Import Tool

### Purpose

`importsvg.py` converts a polygon-like SVG shape into obstacle vertices for this project.
It is meant for SVG files that describe map borders, obstacle polygons, or simple closed
shapes.

Supported SVG elements:

- `rect`
- `path`
- `polygon`
- `polyline`

The default output is C++ for raylib examples:

```cpp
static crowd::Vec2 arena[3];
arena[0] = crowd::Vec2(0.0f,  488.0f);
arena[1] = crowd::Vec2(488.0f,  488.0f);
arena[2] = crowd::Vec2(244.0f,  0.0f);
```

When `--origin defold` is selected, the tool flips Y into Defold coordinates and emits
copy-paste-ready Lua table output:

```lua
-- total_points = 3
local arena = {
	{ x = 244.0, y = 488.0 },
	{ x = 488.0, y = 0.0 },
	{ x = 0.0, y = 0.0 },
}
```

### Quick Start

Run the tool from the repository root:

```bash
python3 tools/importsvg.py tools/assets/tri.svg
```

Show all CLI options:

```bash
python3 tools/importsvg.py --help
```

Run the tests:

```bash
python3 tools/test_importsvg.py
```

### Output Modes

Output mode is selected with `--origin`.

`--origin raylib` is the default. It keeps SVG coordinates as top-left origin with Y
going down, and emits C++ `crowd::Vec2` assignment lines.

```bash
python3 tools/importsvg.py tools/assets/tri.svg --origin raylib --precision 1
```

`--origin defold` flips Y so the output uses bottom-left origin with Y going up, and
emits a Lua table.

```bash
python3 tools/importsvg.py tools/assets/tri.svg --origin defold --precision 1
```

### Positioning

Most SVGs are authored from a local zero-based coordinate space. Use `--position-x` and
`--position-y` when the imported shape should be centered at a specific screen or world
position.

Both flags must be supplied together. If neither flag is supplied, the original converted
SVG coordinates are emitted without an extra position offset.

The position offset is:

```text
x = vertex.x + (position.x - (svg_width / 2))
y = vertex.y + (position.y - (svg_height / 2))
```

Example: emit a Defold Lua table centered at `(640, 360)`:

```bash
python3 tools/importsvg.py tools/assets/tri.svg --origin defold --position-x 640 --position-y 360 --precision 1
```

Example output:

```lua
-- total_points = 3
local arena = {
	{ x = 640.0, y = 604.0 },
	{ x = 884.0, y = 116.0 },
	{ x = 396.0, y = 116.0 },
}
```

### Winding

Winding controls the emitted vertex order after coordinate conversion.

`--winding cw` is the default. Use it for clockwise Crowd boundary vertices, usually for a
negative/map-boundary obstacle.

```bash
python3 tools/importsvg.py tools/assets/border.svg --winding cw
```

Use `--winding ccw` for counterclockwise internal or normal obstacle vertices.

```bash
python3 tools/importsvg.py tools/assets/rect.svg --winding ccw
```

The tool prints a summary line to stderr containing vertex count, origin, winding, and
signed area.

### Shape Selection

If an SVG contains multiple polygon-like shapes, the tool uses the shape with the largest
absolute signed area by default.

Use `--shape-index` to select a specific collected shape instead:

```bash
python3 tools/importsvg.py tools/assets/poly.svg --shape-index 0
```

Indexes are zero-based and refer to the polygon-like shapes collected from the SVG.

### Curve Flattening

Cubic Bezier path segments are converted to line segments. The default is `8` line
segments per cubic curve.

Increase `--curve-steps` for smoother curves:

```bash
python3 tools/importsvg.py tools/assets/border.svg --curve-steps 16
```

Lower values emit fewer vertices:

```bash
python3 tools/importsvg.py tools/assets/border.svg --curve-steps 4
```

### Examples

Basic C++ output:

```bash
python3 tools/importsvg.py tools/assets/tri.svg
```

C++ output with a custom variable name and two decimal places:

```bash
python3 tools/importsvg.py tools/assets/tri.svg --var triangle_obstacle --precision 2
```

Defold Lua table output:

```bash
python3 tools/importsvg.py tools/assets/tri.svg --origin defold
```

Defold Lua table output centered at `(640, 360)`:

```bash
python3 tools/importsvg.py tools/assets/tri.svg --origin defold --position-x 640 --position-y 360
```

Choose counterclockwise winding for an internal obstacle:

```bash
python3 tools/importsvg.py tools/assets/obst_rect_256.svg --origin defold --winding ccw
```

Choose a specific shape from a multi-shape SVG:

```bash
python3 tools/importsvg.py tools/assets/poly.svg --shape-index 0
```

Flatten cubic curves with more detail:

```bash
python3 tools/importsvg.py tools/assets/border.svg --curve-steps 16 --precision 1
```

### Testing

Run the importer tests:

```bash
python3 tools/test_importsvg.py
```

Raylib/C++ smoke command:

```bash
python3 tools/importsvg.py tools/assets/tri.svg --origin raylib --precision 1
```

Defold/Lua positioned smoke command:

```bash
python3 tools/importsvg.py tools/assets/tri.svg --origin defold --position-x 640 --position-y 360 --precision 1
```

### Notes and Limitations

- The SVG must have a `viewBox` or numeric `width` and `height`.
- Path support covers `M`, `L`, `H`, `V`, `C`, and `Z` commands.
- Cubic `C` curves are flattened; quadratic, smooth, and arc commands are not supported.
- Supported transforms are `matrix`, `translate`, `scale`, and `rotate`.
- Duplicate and immediate backtracking points are removed during cleanup.
- `--position-x` and `--position-y` must be used together.
- `--var` controls the C++ array name and the Defold Lua local variable name.

## CORS Bundle Server

`serve_cors.py` serves a Defold HTML5 or `wasm_pthread-web` bundle with the CORS and
cross-origin isolation headers needed by pthread-enabled WebAssembly builds.

Start the server from the repository root:

```bash
python3 tools/serve_cors.py defold-bundle/wasm_pthread-web/defold-crowd-pro-dev
```

Run on a custom port:

```bash
python3 tools/serve_cors.py defold-bundle/wasm_pthread-web/defold-crowd-pro-dev --port 8008
```

Bind to all interfaces for LAN access:

```bash
python3 tools/serve_cors.py defold-bundle/wasm_pthread-web/defold-crowd-pro-dev --host 0.0.0.0 --port 8008
```

Show all CLI options:

```bash
python3 tools/serve_cors.py --help
```

The server adds:

- `Cross-Origin-Opener-Policy: same-origin`
- `Cross-Origin-Embedder-Policy: require-corp`
- `Cross-Origin-Resource-Policy: cross-origin`
- `Access-Control-Allow-Origin: *`
- `Access-Control-Allow-Methods: GET, HEAD, OPTIONS`
- `Access-Control-Allow-Headers: *`
- `Cache-Control: no-store`
