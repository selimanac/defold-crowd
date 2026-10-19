local data                    = require("defold-files.examples.scripts.lib.data")
local spawn_sampling          = require("defold-files.examples.scripts.lib.spawn_sampling")
local const                   = require("defold-files.examples.scripts.lib.const")

local agent                   = {}

-- Agents
local AGENT_SIZE              = 128
local AGENT_MAX_NEIGHBOR      = 4
local AGENT_MAX_SPEED         = 400
local AGENT_TIME_HORIZON      = 1
local AGENT_TIME_HORIZON_OBST = 1

-- Goal arrival settings
local GOAL_RADIUS             = 64
local ARRIVAL_MARGIN          = 2

-- Spawn sampling
local SPAWN_MAX_DISTANCE      = 1024
local SPAWN_EXTRA_DISTANCE    = 100
local SPAWN_PADDING           = 4
local SPAWN_ATTEMPTS          = 30

local function default(value, fallback)
	if value == nil then
		return fallback
	end
	return value
end

function agent.set(index, max_neighbors, max_speed)
	AGENT_MAX_NEIGHBOR[index] = max_neighbors
	AGENT_MAX_SPEED[index] = max_speed
end

function agent.set_spawn_max_distance(val)
	SPAWN_MAX_DISTANCE = val
end

function agent.set_preferred_velocities(dt)
	local ids = data.batch_ids
	local xs  = data.batch_xs
	local ys  = data.batch_ys

	for i = 1, data.agent_count do
		local temp_agent     = data.agents[i]

		local pref_velocity  = vmath.vector3()
		local goal_direction = temp_agent.goal - temp_agent.position
		local distance       = vmath.length(goal_direction)
		local arrival_radius = temp_agent.radius + GOAL_RADIUS + ARRIVAL_MARGIN

		if distance > arrival_radius then
			local remaining_distance = distance - arrival_radius
			local slow_distance      = math.max(temp_agent.radius, temp_agent.max_speed * dt)

			if remaining_distance < slow_distance then
				local speed   = math.min(temp_agent.max_speed, remaining_distance / dt)
				pref_velocity = vmath.normalize(goal_direction) * speed
			else
				pref_velocity = vmath.normalize(goal_direction) * temp_agent.max_speed
			end
		end

		ids[i] = temp_agent.id
		xs[i]  = pref_velocity.x
		ys[i]  = pref_velocity.y
	end

	crowd.set_agent_pref_velocities(ids, xs, ys, data.agent_count)
end

function agent.add(amount, spawn_position, goal_position, run_simulation, mask, is_player, spawn_options, min_spawn_radius, max_spawn_radius)
	amount           = default(amount, 1)
	run_simulation   = default(run_simulation, false)

	mask             = default(mask, nil)
	is_player        = default(is_player, false)
	spawn_options    = default(spawn_options, nil)
	min_spawn_radius = default(min_spawn_radius, 400)
	max_spawn_radius = default(max_spawn_radius, 1024)

	local spawned    = {}

	for i = 1, amount do
		local agent_id = crowd.INVALID_ID


		local agent_name        = tostring(AGENT_SIZE)
		local radius            = AGENT_SIZE / 2
		local max_neighbors     = AGENT_MAX_NEIGHBOR
		local max_speed         = AGENT_MAX_SPEED
		local neighbor_distance = math.max(radius * 8 + 150, max_speed * AGENT_TIME_HORIZON + 2 * radius)

		local current_goal      = goal_position
		local start_pos

		if spawn_options and spawn_options.layout == "opposite_circle" then
			local layout_radius = default(spawn_options.radius, SPAWN_MAX_DISTANCE)
			local angle         = (i - 1) * const.TWO_PI / amount
			local x             = math.cos(angle) * layout_radius
			local y             = math.sin(angle) * layout_radius

			start_pos           = vmath.vector3(x, y, 0)
			current_goal        = vmath.vector3(-x, -y, 0)
		else
			local min_spawn_distance = GOAL_RADIUS + radius + ARRIVAL_MARGIN + SPAWN_EXTRA_DISTANCE
			local sampling_position  = default(spawn_position, goal_position)

			start_pos                = spawn_sampling.random_in_radius(sampling_position, min_spawn_radius, max_spawn_radius)
			--[[
			-- This is an expensive alternative
			start_pos                = spawn_sampling.find_position(sampling_position, radius, spawned, {
			min_distance = min_spawn_distance,
			max_distance = SPAWN_MAX_DISTANCE,
			padding = SPAWN_PADDING,
			attempts = SPAWN_ATTEMPTS
		})]]
		end

		local agent_factory_url = "/agent_factory#agent_" .. agent_name

		if is_player then
			agent_factory_url = "/player_factory#player_" .. agent_name
			start_pos.z = 0.1
		end

		local instance      = factory.create(agent_factory_url, start_pos)
		local sprite_url    = msg.url(instance)
		sprite_url.fragment = "sprite"

		if run_simulation then
			agent_id = crowd.add_gameobject(msg.url(instance), radius, max_speed, max_neighbors, neighbor_distance, AGENT_TIME_HORIZON, AGENT_TIME_HORIZON_OBST)

			if agent_id ~= crowd.INVALID_ID then
				crowd.set_gameobject_goal(agent_id, current_goal.x, current_goal.y)
			end
		else
			agent_id = crowd.add_agent(start_pos.x, start_pos.y, radius, max_speed, max_neighbors, neighbor_distance, AGENT_TIME_HORIZON, AGENT_TIME_HORIZON_OBST)
		end

		if agent_id == crowd.INVALID_ID then
			error("agent.add failed: " .. agent_id, 2)
			return
		end

		if mask then
			crowd.set_agent_nav_mask(agent_id, mask)
		end

		local stop_dist  = const.PLAYER_RADIUS + radius * 2.0
		local slow_dist  = stop_dist + radius * 5.0

		local temp_agent = {
			id         = agent_id,
			rotation   = vmath.quat(),
			goal       = current_goal,
			radius     = radius,
			speed      = max_speed,
			instance   = instance,
			sprite_url = sprite_url,
			position   = start_pos,
			velocity   = vmath.vector3(),
			max_speed  = max_speed,

			stop_dist  = stop_dist,
			slow_dist  = slow_dist,
			--	stop_dist_sq = stop_dist * stop_dist,
			--	slow_dist_sq = slow_dist * slow_dist,
		}

		table.insert(data.agents, temp_agent)
		data.agent_by_id[agent_id] = temp_agent
		data.max_agent_radius = math.max(data.max_agent_radius, radius)
		data.max_agent_speed = math.max(data.max_agent_speed, max_speed)
		table.insert(spawned, {
			position = start_pos,
			radius = radius
		})
	end

	data.agent_count = data.agent_count + amount
end

function agent.update_game_objects()
	local ids = data.batch_ids
	local xs  = data.batch_xs
	local ys  = data.batch_ys
	local vxs = data.batch_vxs
	local vys = data.batch_vys

	for i = 1, data.agent_count do
		ids[i] = data.agents[i].id
	end

	crowd.get_agent_positions_and_velocities(ids, data.agent_count, xs, ys, vxs, vys)

	for i = 1, data.agent_count do
		local temp_agent = data.agents[i]

		temp_agent.position.x, temp_agent.position.y = xs[i], ys[i]
		temp_agent.velocity.x, temp_agent.velocity.y = vxs[i], vys[i]

		if vmath.length_sqr(temp_agent.velocity) > 0.001 then
			local angle = 0

			angle = math.atan2(temp_agent.velocity.y, temp_agent.velocity.x)
			temp_agent.rotation = vmath.quat_rotation_z(angle)

			go.set_rotation(temp_agent.rotation, temp_agent.instance)
		end

		go.set_position(temp_agent.position, temp_agent.instance)
	end
end

return agent
