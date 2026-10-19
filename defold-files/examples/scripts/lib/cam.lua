local const     = require "defold-files.examples.scripts.lib.const"

local cam       = {}

local ZOOM_STEP = 0.01
local MIN_ZOOM  = 0.01

local function zoom_camera(delta)
	local zoom = camera.get_orthographic_zoom(const.CAMERA) + delta
	zoom = math.max(MIN_ZOOM, zoom)
	camera.set_orthographic_zoom(const.CAMERA, zoom)
end

function cam.input(action_id)
	if action_id == const.TRIGGER.MOUSE_WHEEL_UP then
		zoom_camera(ZOOM_STEP)
	end
	if action_id == const.TRIGGER.MOUSE_WHEEL_DOWN then
		zoom_camera(-ZOOM_STEP)
	end
end

function cam.set_zoom_step(value)
	ZOOM_STEP = value
end

return cam
