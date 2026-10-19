local data            = {}

data.agent_count      = 0
data.agents           = {}
data.agent_by_id      = {}
data.max_agent_radius = 0
data.max_agent_speed  = 0
data.spawn_points     = {}

-- Scratch buffers for the batch crowd.* calls,
data.batch_ids        = {}
data.batch_xs         = {}
data.batch_ys         = {}
data.batch_vxs        = {}
data.batch_vys        = {}

-- player
data.player           = {
	id        = crowd.INVALID_ID,
	position  = vmath.vector3(),
	direction = vmath.vector3(),
	rotation  = vmath.quat(),
	angle     = 0,
	speed     = 0
}
return data
