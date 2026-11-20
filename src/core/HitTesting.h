#pragma once

#include "core/Types.h"

#include <glm/vec2.hpp>

#include <optional>
#include <span>

namespace core
{
class Camera2D;

struct NodePick
{
	NodeId id;
	glm::vec2 position_world;
};

struct EdgePick
{
	EdgeId id;
	glm::vec2 from_world;
	glm::vec2 to_world;
};

std::optional<NodeId> find_nearest_node(
	std::span<const NodePick> nodes,
	const Camera2D& camera,
	glm::vec2 cursor_screen_px,
	float pick_radius_px) noexcept;

std::optional<EdgeId> find_nearest_edge(
	std::span<const EdgePick> edges,
	const Camera2D& camera,
	glm::vec2 cursor_screen_px,
	float pick_radius_px) noexcept;
}
