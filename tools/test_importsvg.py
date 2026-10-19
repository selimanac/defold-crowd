#!/usr/bin/env python3
import contextlib
import importsvg
import io
import xml.etree.ElementTree as ET


IDENTITY = (1.0, 0.0, 0.0, 1.0, 0.0, 0.0)


def assert_clean(points, epsilon):
    for i, point in enumerate(points):
        next_point = points[(i + 1) % len(points)]
        assert not importsvg.points_close(point, next_point, epsilon)

        if len(points) > 2:
            prev_point = points[i - 1]
            assert not importsvg.points_close(prev_point, next_point, epsilon)


def load_svg_points(path, origin="raylib", winding="cw", precision=1):
    root = ET.parse(path).getroot()
    shapes = importsvg.collect_shapes(root, IDENTITY, 8)
    viewport = importsvg.svg_viewport(root)
    points = importsvg.convert_origin(importsvg.choose_shape(shapes, None), origin, viewport)
    points = importsvg.enforce_winding(points, winding)
    epsilon = max(1e-4, 0.5 * pow(10.0, -precision))
    return importsvg.cleanup_points(points, epsilon), epsilon


def test_border_curve_flattening():
    points, epsilon = load_svg_points("tools/assets/border.svg")
    assert len(points) > 35
    assert importsvg.signed_area(points) < 0.0
    assert_clean(points, epsilon)


def test_existing_assets_still_import():
    assets = [
        "tools/assets/dia.svg",
        "tools/assets/poly.svg",
        "tools/assets/rect.svg",
        "tools/assets/star.svg",
        "tools/assets/tri.svg",
        "examples/resources/obs1.svg",
    ]

    for asset in assets:
        points, epsilon = load_svg_points(asset)
        assert len(points) >= 3
        assert_clean(points, epsilon)


def test_defold_origin_flips_y():
    points, _epsilon = load_svg_points("tools/assets/tri.svg", origin="defold")
    assert importsvg.points_close(points[0], (244.0, 488.0))
    assert importsvg.points_close(points[1], (488.0, 0.0))
    assert importsvg.points_close(points[2], (0.0, 0.0))


def test_position_offset_centers_svg():
    points = importsvg.apply_position([(0.0, 0.0), (100.0, 50.0)], [0.0, 0.0, 100.0, 50.0], 200.0, 100.0)
    assert importsvg.points_close(points[0], (150.0, 75.0))
    assert importsvg.points_close(points[1], (250.0, 125.0))


def test_position_requires_both_axes():
    try:
        importsvg.apply_position([(0.0, 0.0)], [0.0, 0.0, 100.0, 50.0], 200.0, None)
    except ValueError as err:
        assert "--position-x and --position-y" in str(err)
        return

    raise AssertionError("single-axis position did not fail")


def capture_emit(origin):
    output = io.StringIO()
    with contextlib.redirect_stdout(output):
        importsvg.emit_points([(1.0, 2.0)], "arena", 1, "cw", origin)
    return output.getvalue()


def test_raylib_output_is_cpp():
    output = capture_emit("raylib")
    assert "static crowd::Vec2 arena[1];" in output
    assert "arena[0] = crowd::Vec2(1.0f,  2.0f);" in output


def test_defold_output_is_lua_table():
    output = capture_emit("defold")
    assert "-- total_points = 1" in output
    assert "local arena = {" in output
    assert "\t{ x = 1.0, y = 2.0 }," in output
    assert output.rstrip().endswith("}")
    assert "static crowd::Vec2" not in output
    assert "crowd::Vec2" not in output


def test_relative_cubic_path():
    points = importsvg.path_points("M0,0 c10,0 10,10 20,10 z", 4)
    points = importsvg.cleanup_points(points)
    assert len(points) == 5
    assert importsvg.points_close(points[-1], (20.0, 10.0))


def test_unsupported_command_fails():
    try:
        importsvg.path_points("M0,0 Q10,10 20,0 Z", 8)
    except ValueError as err:
        assert "Unsupported path command" in str(err)
        return

    raise AssertionError("unsupported command did not fail")


def main():
    test_border_curve_flattening()
    test_existing_assets_still_import()
    test_defold_origin_flips_y()
    test_position_offset_centers_svg()
    test_position_requires_both_axes()
    test_raylib_output_is_cpp()
    test_defold_output_is_lua_table()
    test_relative_cubic_path()
    test_unsupported_command_fails()
    print("All importsvg tests passed.")


if __name__ == "__main__":
    main()
