local const                 = {}

const.CAMERA                = msg.url("main:/camera#camera")
const.TRIGGER               = {
	KEY_SPACE         = hash("key_space"),
	KEY_UP            = hash("key_up"),
	KEY_DOWN          = hash("key_down"),
	KEY_LEFT          = hash("key_left"),
	KEY_RIGHT         = hash("key_right"),
	MOUSE_WHEEL_UP    = hash("mouse_wheel_up"),
	MOUSE_WHEEL_DOWN  = hash("mouse_wheel_down"),
	MOUSE_BUTTON_LEFT = hash("mouse_button_left")
}
const.TWO_PI                = math.pi * 2

const.WALL_COLOR            = vmath.vector4(0.5, 0.0, 1.0, 1.0)
const.CORNER_COLOR          = vmath.vector4(0.4, 0.4, 0.4, 1.0)
const.EDGE_COLOR            = vmath.vector4(0.0, 1.0, 1.0, 1.0)
const.VERTEX_COLOR          = vmath.vector4(1.0, 1.0, 0.0, 1.0)

const.AGENT_ACCELERATION    = 600
const.AGENT_DECELERATION    = 800

const.SPAWN_POINT_COUNT     = 4

-- Player
const.PLAYER_RADIUS         = 64
const.PLAYER_SOLID_MARGIN   = 4
const.PLAYER_MAX_SPEED      = 1000
const.PLAYER_ACCELERATION   = 800
const.PLAYER_DECELERATION   = 900
const.PLAYER_ROTATION_SPEED = 200
const.RND_ID                = rnd.new()

return const
