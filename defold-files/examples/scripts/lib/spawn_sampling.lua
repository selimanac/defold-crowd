local const = require "defold-files.examples.scripts.lib.const"

local M = {}


function M.random_point_in_annulus(center, min_radius, max_radius)
	local angle = rnd.double(const.RND_ID) * const.TWO_PI
	local min_radius_sq = min_radius * min_radius
	local max_radius_sq = max_radius * max_radius
	local distance = math.sqrt(min_radius_sq + rnd.double(const.RND_ID) * (max_radius_sq - min_radius_sq))

	return vmath.vector3(
		center.x + math.cos(angle) * distance,
		center.y + math.sin(angle) * distance,
		0
	)
end

function M.has_clearance(position, radius, spawned, padding)
	for i = 1, #spawned do
		local other = spawned[i]
		local min_distance = radius + other.radius + padding
		local delta = position - other.position

		if vmath.length_sqr(delta) < min_distance * min_distance then
			return false
		end
	end

	return true
end

---Find a spawn position using bounded Poisson-like rejection sampling.
function M.find_position(center, radius, spawned, config)
	local fallback = M.random_point_in_annulus(center, config.min_distance, config.max_distance)

	if M.has_clearance(fallback, radius, spawned, config.padding) then
		return fallback
	end

	for i = 2, config.attempts do
		local position = M.random_point_in_annulus(center, config.min_distance, config.max_distance)

		if M.has_clearance(position, radius, spawned, config.padding) then
			return position
		end

		fallback = position
	end

	return fallback
end

local function random_point(center, min_radius, max_radius)
	local angle = rnd.double(const.RND_ID) * const.TWO_PI
	local min_radius_sq = min_radius * min_radius
	local max_radius_sq = max_radius * max_radius
	local distance = math.sqrt(min_radius_sq + rnd.double(const.RND_ID) * (max_radius_sq - min_radius_sq))

	return vmath.vector3(
		center.x + math.cos(angle) * distance,
		center.y + math.sin(angle) * distance,
		0
	)
end

function M.random_in_radius(center, min_radius, max_radius)
	return random_point(center, min_radius, max_radius)
end

return M
