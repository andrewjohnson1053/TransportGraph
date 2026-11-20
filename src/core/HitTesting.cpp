#include "core/HitTesting.h"

#include "core/Camera2D.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace core
{
namespace
{
bool is_finite(glm::vec2 value) noexcept
{
	return std::isfinite(value.x) && std::isfinite(value.y);
}

bool project_to_screen(glm::vec2 world, const Camera2D& camera, glm::dvec2& screen) noexcept
{
	if (!is_finite(world))
	{
		return false;
	}

	const glm::vec2 center = camera.center_world();
	const glm::vec2 viewport = camera.viewport_size_px();
	const double zoom = camera.zoom();
	const double x = (static_cast<double>(world.x) - center.x) * zoom + viewport.x * 0.5;
	const double y = (static_cast<double>(world.y) - center.y) * zoom + viewport.y * 0.5;
	if (!std::isfinite(x) || !std::isfinite(y))
	{
		return false;
	}

	screen = {x, y};
	return true;
}

double distance_squared(glm::dvec2 first, glm::dvec2 second) noexcept
{
	const double dx = first.x - second.x;
	const double dy = first.y - second.y;
	return dx * dx + dy * dy;
}

double distance_squared_to_segment(glm::dvec2 point, glm::dvec2 start, glm::dvec2 end) noexcept
{
	const double dx = end.x - start.x;
	const double dy = end.y - start.y;
	const double length_squared = dx * dx + dy * dy;
	if (length_squared == 0.0)
	{
		return distance_squared(point, start);
	}

	const double projection = std::clamp(
		((point.x - start.x) * dx + (point.y - start.y) * dy) / length_squared,
		0.0,
		1.0);
	const glm::dvec2 closest{start.x + projection * dx, start.y + projection * dy};
	return distance_squared(point, closest);
}

bool valid_query(const Camera2D& camera, glm::vec2 cursor, float radius) noexcept
{
	return is_finite(cursor) && std::isfinite(radius) && radius > 0.0f &&
		   std::isfinite(camera.zoom()) && camera.zoom() > 0.0f &&
		   is_finite(camera.center_world()) && is_finite(camera.viewport_size_px());
}
}

std::optional<NodeId> find_nearest_node(
	std::span<const NodePick> nodes,
	const Camera2D& camera,
	glm::vec2 cursor_screen_px,
	float pick_radius_px) noexcept
{
	if (!valid_query(camera, cursor_screen_px, pick_radius_px))
	{
		return std::nullopt;
	}

	const glm::dvec2 cursor{cursor_screen_px.x, cursor_screen_px.y};
	const double radius_squared = static_cast<double>(pick_radius_px) * pick_radius_px;
	double nearest_distance_squared = radius_squared;
	std::optional<NodeId> nearest;
	for (const NodePick& node : nodes)
	{
		if (!node.id.is_valid())
		{
			continue;
		}
		glm::dvec2 screen;
		if (!project_to_screen(node.position_world, camera, screen))
		{
			continue;
		}

		const double candidate_distance_squared = distance_squared(cursor, screen);
		if (candidate_distance_squared < nearest_distance_squared ||
			(candidate_distance_squared == nearest_distance_squared &&
			 (!nearest || node.id.value() < nearest->value())))
		{
			nearest_distance_squared = candidate_distance_squared;
			nearest = node.id;
		}
	}
	return nearest;
}

std::optional<EdgeId> find_nearest_edge(
	std::span<const EdgePick> edges,
	const Camera2D& camera,
	glm::vec2 cursor_screen_px,
	float pick_radius_px) noexcept
{
	if (!valid_query(camera, cursor_screen_px, pick_radius_px))
	{
		return std::nullopt;
	}

	const glm::dvec2 cursor{cursor_screen_px.x, cursor_screen_px.y};
	const double radius_squared = static_cast<double>(pick_radius_px) * pick_radius_px;
	double nearest_distance_squared = radius_squared;
	std::optional<EdgeId> nearest;
	for (const EdgePick& edge : edges)
	{
		if (!edge.id.is_valid())
		{
			continue;
		}
		glm::dvec2 start;
		glm::dvec2 end;
		if (!project_to_screen(edge.from_world, camera, start) ||
			!project_to_screen(edge.to_world, camera, end))
		{
			continue;
		}

		const double candidate_distance_squared = distance_squared_to_segment(cursor, start, end);
		if (candidate_distance_squared < nearest_distance_squared ||
			(candidate_distance_squared == nearest_distance_squared &&
			 (!nearest || edge.id.value() < nearest->value())))
		{
			nearest_distance_squared = candidate_distance_squared;
			nearest = edge.id;
		}
	}
	return nearest;
}
}
