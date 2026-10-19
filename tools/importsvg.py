#!/usr/bin/env python3
import argparse
import math
import re
import sys
import xml.etree.ElementTree as ET


NUMBER = r"[-+]?(?:\d*\.\d+|\d+)(?:[eE][-+]?\d+)?"
PATH_TOKEN_RE = re.compile(rf"[AaCcHhLlMmQqSsTtVvZz]|{NUMBER}")
TRANSFORM_RE = re.compile(r"([a-zA-Z]+)\s*\(([^)]*)\)")


def local_name(tag):
    return tag.rsplit("}", 1)[-1]


def parse_numbers(text):
    return [float(value) for value in re.findall(NUMBER, text or "")]


def parse_length(value):
    values = parse_numbers(value)
    return values[0] if values else None


def svg_viewport(root):
    view_box = parse_numbers(root.attrib.get("viewBox", ""))
    if len(view_box) == 4:
        return view_box

    width = parse_length(root.attrib.get("width"))
    height = parse_length(root.attrib.get("height"))

    if width is None or height is None:
        raise ValueError("Defold output needs an SVG viewBox or numeric width/height so Y can be flipped.")

    return [0.0, 0.0, width, height]


def convert_origin(points, origin, viewport):
    if origin == "raylib":
        return points

    min_x, min_y, _width, height = viewport
    top_y = min_y + height
    return [(x - min_x, top_y - y) for x, y in points]


def mat_mul(a, b):
    return (
        a[0] * b[0] + a[2] * b[1],
        a[1] * b[0] + a[3] * b[1],
        a[0] * b[2] + a[2] * b[3],
        a[1] * b[2] + a[3] * b[3],
        a[0] * b[4] + a[2] * b[5] + a[4],
        a[1] * b[4] + a[3] * b[5] + a[5],
    )


def transform_point(matrix, point):
    x, y = point
    a, b, c, d, e, f = matrix
    return (a * x + c * y + e, b * x + d * y + f)


def parse_transform(transform):
    matrix = (1.0, 0.0, 0.0, 1.0, 0.0, 0.0)

    for name, args in TRANSFORM_RE.findall(transform or ""):
        values = parse_numbers(args)
        name = name.lower()

        if name == "matrix" and len(values) == 6:
            next_matrix = tuple(values)
        elif name == "translate":
            tx = values[0] if values else 0.0
            ty = values[1] if len(values) > 1 else 0.0
            next_matrix = (1.0, 0.0, 0.0, 1.0, tx, ty)
        elif name == "scale":
            sx = values[0] if values else 1.0
            sy = values[1] if len(values) > 1 else sx
            next_matrix = (sx, 0.0, 0.0, sy, 0.0, 0.0)
        elif name == "rotate":
            angle = math.radians(values[0] if values else 0.0)
            cos_a = math.cos(angle)
            sin_a = math.sin(angle)
            rotation = (cos_a, sin_a, -sin_a, cos_a, 0.0, 0.0)

            if len(values) >= 3:
                cx, cy = values[1], values[2]
                next_matrix = mat_mul(
                    mat_mul((1.0, 0.0, 0.0, 1.0, cx, cy), rotation),
                    (1.0, 0.0, 0.0, 1.0, -cx, -cy),
                )
            else:
                next_matrix = rotation
        else:
            raise ValueError(f"Unsupported SVG transform: {name}({args})")

        matrix = mat_mul(matrix, next_matrix)

    return matrix


def signed_area(points):
    area = 0.0
    for i, (x1, y1) in enumerate(points):
        x2, y2 = points[(i + 1) % len(points)]
        area += x1 * y2 - x2 * y1
    return area * 0.5


def points_close(a, b, epsilon=1e-4):
    return abs(a[0] - b[0]) <= epsilon and abs(a[1] - b[1]) <= epsilon


def cleanup_points(points, epsilon=1e-4):
    cleaned = []

    for point in points:
        if cleaned and points_close(cleaned[-1], point, epsilon):
            continue

        if len(cleaned) >= 2 and points_close(cleaned[-2], point, epsilon):
            cleaned.pop()
            continue

        cleaned.append(point)

    if len(cleaned) > 1 and points_close(cleaned[0], cleaned[-1], epsilon):
        cleaned.pop()

    return cleaned


def dedupe_closed(points, epsilon=1e-4):
    return cleanup_points(points, epsilon)


def parse_points_attr(points_text):
    values = parse_numbers(points_text)
    if len(values) % 2 != 0:
        raise ValueError("Odd number of values in points attribute")
    return list(zip(values[0::2], values[1::2]))


def rect_points(element):
    x = float(element.attrib.get("x", 0.0))
    y = float(element.attrib.get("y", 0.0))
    width = float(element.attrib.get("width", 0.0))
    height = float(element.attrib.get("height", 0.0))
    return [
        (x, y),
        (x + width, y),
        (x + width, y + height),
        (x, y + height),
    ]


def cubic_point(p0, p1, p2, p3, t):
    inv_t = 1.0 - t
    a = inv_t * inv_t * inv_t
    b = 3.0 * inv_t * inv_t * t
    c = 3.0 * inv_t * t * t
    d = t * t * t
    return (
        a * p0[0] + b * p1[0] + c * p2[0] + d * p3[0],
        a * p0[1] + b * p1[1] + c * p2[1] + d * p3[1],
    )


def append_cubic(points, p0, p1, p2, p3, curve_steps):
    for step in range(1, curve_steps + 1):
        t = float(step) / float(curve_steps)
        points.append(cubic_point(p0, p1, p2, p3, t))


def path_points(path_data, curve_steps=8):
    tokens = PATH_TOKEN_RE.findall(path_data or "")
    points = []
    cursor = (0.0, 0.0)
    start = None
    command = None
    i = 0

    def is_command(token):
        return len(token) == 1 and token.isalpha()

    def read_float():
        nonlocal i
        if i >= len(tokens) or is_command(tokens[i]):
            raise ValueError("Expected path number")
        value = float(tokens[i])
        i += 1
        return value

    def read_point(absolute):
        x = read_float()
        y = read_float()
        return (x, y) if absolute else (cursor[0] + x, cursor[1] + y)

    while i < len(tokens):
        if is_command(tokens[i]):
            command = tokens[i]
            i += 1

        if command is None:
            raise ValueError("Path data starts without a command")

        absolute = command.isupper()
        op = command.upper()

        if op == "M":
            cursor = read_point(absolute)
            start = cursor
            points.append(cursor)
            command = "L" if absolute else "l"

            while i < len(tokens) and not is_command(tokens[i]):
                cursor = read_point(absolute)
                points.append(cursor)
        elif op == "L":
            while i < len(tokens) and not is_command(tokens[i]):
                cursor = read_point(absolute)
                points.append(cursor)
        elif op == "H":
            while i < len(tokens) and not is_command(tokens[i]):
                x = read_float()
                cursor = (x, cursor[1]) if absolute else (cursor[0] + x, cursor[1])
                points.append(cursor)
        elif op == "V":
            while i < len(tokens) and not is_command(tokens[i]):
                y = read_float()
                cursor = (cursor[0], y) if absolute else (cursor[0], cursor[1] + y)
                points.append(cursor)
        elif op == "C":
            while i < len(tokens) and not is_command(tokens[i]):
                p0 = cursor
                p1 = read_point(absolute)
                p2 = read_point(absolute)
                p3 = read_point(absolute)
                append_cubic(points, p0, p1, p2, p3, curve_steps)
                cursor = p3
        elif op == "Z":
            if start is not None:
                points.append(start)
                cursor = start
            command = None
        else:
            raise ValueError(
                f"Unsupported path command '{command}'. Convert curves to lines/polygons before importing."
            )

    return cleanup_points(points)


def collect_shapes(element, parent_matrix, curve_steps):
    matrix = mat_mul(parent_matrix, parse_transform(element.attrib.get("transform")))
    tag = local_name(element.tag)
    shapes = []

    if tag == "rect":
        shapes.append([transform_point(matrix, point) for point in rect_points(element)])
    elif tag in ("polygon", "polyline"):
        shapes.append([transform_point(matrix, point) for point in parse_points_attr(element.attrib.get("points", ""))])
    elif tag == "path":
        shapes.append([transform_point(matrix, point) for point in path_points(element.attrib.get("d", ""), curve_steps)])

    for child in element:
        shapes.extend(collect_shapes(child, matrix, curve_steps))

    return shapes


def choose_shape(shapes, index):
    closed_shapes = [dedupe_closed(shape) for shape in shapes if len(dedupe_closed(shape)) >= 3]
    if not closed_shapes:
        raise ValueError("No polygon-like SVG shape found")

    if index is not None:
        return closed_shapes[index]

    return max(closed_shapes, key=lambda shape: abs(signed_area(shape)))


def enforce_winding(points, winding):
    # RVO2 evaluates winding in raw x/y math coordinates. For a negative
    # obstacle used as a map boundary, RVO2 wants clockwise: signed area < 0.
    area = signed_area(points)
    should_be_positive = winding == "ccw"

    if (area > 0.0) != should_be_positive:
        return list(reversed(points))

    return points


def apply_position(points, viewport, position_x, position_y):
    if position_x is None and position_y is None:
        return points

    if position_x is None or position_y is None:
        raise ValueError("--position-x and --position-y must be supplied together")

    _min_x, _min_y, width, height = viewport
    offset_x = position_x - width * 0.5
    offset_y = position_y - height * 0.5
    return [(x + offset_x, y + offset_y) for x, y in points]


def emit_cpp(points, var_name, precision, winding, origin):
    print(f"// --- Coordinate Origin: {origin} ---")

    if winding == "cw":
        print("// --- Generated Crowd Boundary Vertices (clockwise: use for negative/map-boundary obstacle) ---")
    else:
        print("// --- Generated Crowd Obstacle Vertices (counterclockwise: use for normal/internal obstacle) ---")

    total_points = len(points)
    print(f"static crowd::Vec2 {var_name}[{total_points}];")

    for i, (x, y) in enumerate(points):
        print(f"{var_name}[{i}] = crowd::Vec2({x:.{precision}f}f,  {y:.{precision}f}f);")


def emit_lua(points, var_name, precision, winding, origin):
    print(f"-- --- Coordinate Origin: {origin} ---")

    if winding == "cw":
        print("-- --- Generated Crowd Boundary Vertices (clockwise: use for negative/map-boundary obstacle) ---")
    else:
        print("-- --- Generated Crowd Obstacle Vertices (counterclockwise: use for normal/internal obstacle) ---")

    total_points = len(points)
    print(f"-- total_points = {total_points}")
    print(f"local {var_name} = {{")

    for x, y in points:
        print(f"\t{{ x = {x:.{precision}f}, y = {y:.{precision}f} }},")

    print("}")


def emit_points(points, var_name, precision, winding, origin):
    if origin == "defold":
        emit_lua(points, var_name, precision, winding, origin)
        return

    emit_cpp(points, var_name, precision, winding, origin)


def main():
    parser = argparse.ArgumentParser(
        description="Convert an SVG rect/path/polygon/polyline boundary to RVO2 Vector2 vertices."
    )
    parser.add_argument("svg", help="SVG file to import")
    parser.add_argument("--shape-index", type=int, help="Use a specific polygon-like shape instead of the largest")
    parser.add_argument("--origin", choices=("raylib", "defold"), default="raylib", help="Coordinate origin to emit: raylib is top-left/Y-down, defold is bottom-left/Y-up")
    parser.add_argument("--winding", choices=("cw", "ccw"), default="cw", help="Output winding in RVO2 raw x/y coordinates")
    parser.add_argument("--var", default="arena", help="C++ vector variable name")
    parser.add_argument("--precision", type=int, default=1, help="Decimal places in generated output")
    parser.add_argument("--curve-steps", type=int, default=8, help="Line segments emitted per cubic Bezier curve")
    parser.add_argument("--position-x", type=float, help="Center imported SVG at this X position")
    parser.add_argument("--position-y", type=float, help="Center imported SVG at this Y position")
    args = parser.parse_args()

    if args.curve_steps < 1:
        raise ValueError("--curve-steps must be >= 1")
    if (args.position_x is None) != (args.position_y is None):
        raise ValueError("--position-x and --position-y must be supplied together")

    root = ET.parse(args.svg).getroot()
    shapes = collect_shapes(root, (1.0, 0.0, 0.0, 1.0, 0.0, 0.0), args.curve_steps)
    viewport = svg_viewport(root)
    points = convert_origin(choose_shape(shapes, args.shape_index), args.origin, viewport)
    points = enforce_winding(points, args.winding)
    output_epsilon = max(1e-4, 0.5 * math.pow(10.0, -args.precision))
    points = cleanup_points(points, output_epsilon)
    points = apply_position(points, viewport, args.position_x, args.position_y)

    emit_points(points, args.var, args.precision, args.winding, args.origin)
    comment_prefix = "--" if args.origin == "defold" else "//"
    print(
        f"\n{comment_prefix} vertices={len(points)} origin={args.origin} winding={args.winding} signed_area={signed_area(points):.{args.precision}f}",
        file=sys.stderr,
    )


if __name__ == "__main__":
    main()
