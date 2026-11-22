#pragma once

#include "core/Camera2D.h"
#include "core/Types.h"

#include <glm/vec2.hpp>

#include <optional>
#include <span>
#include <string>

struct SDL_Renderer;

namespace gfx
{
struct Color
{
    unsigned char r = 255;
    unsigned char g = 255;
    unsigned char b = 255;
    unsigned char a = 255;
};

struct RenderNode
{
    core::NodeId id;
    glm::vec2 position_world{0.0f, 0.0f};
    core::NodeType type = core::NodeType::Intersection;
    std::uint32_t waiting_passengers = 0;
};

struct RenderEdge
{
    core::EdgeId id;
    core::NodeId from_node;
    core::NodeId to_node;
    core::RoadClass road_class = core::RoadClass::SingleLane;
    std::uint8_t base_lanes = 1;
    float length_m = 0.0f;
    float congestion_ratio = 0.0f;
};

struct RenderRoute
{
    core::CompanyId company_id;
    std::span<const core::EdgeId> edge_ids;
};

struct RenderVehicle
{
    core::VehicleId id;
    core::CompanyId company_id;
    core::EdgeId current_edge_id;
    float edge_dist_m = 0.0f;
};

struct RenderScene
{
    std::span<const RenderNode> nodes;
    std::span<const RenderEdge> edges;
    std::span<const RenderRoute> routes;
    std::span<const RenderVehicle> vehicles;
};

struct RenderOptions
{
    bool show_heatmap = false;
    float visual_scale = 1.0f;
    std::optional<core::NodeId> hovered_node;
    std::optional<core::NodeId> selected_node;
    std::optional<core::EdgeId> hovered_edge;
    std::optional<core::CompanyId> selected_rival_id;
};

Color road_class_color(core::RoadClass road_class) noexcept;
float road_class_width_px(core::RoadClass road_class) noexcept;
Color heatmap_color(float saturation) noexcept;
Color company_color(core::CompanyId company_id) noexcept;
std::optional<glm::vec2> vehicle_world_position(
    const RenderScene& scene,
    const RenderVehicle& vehicle) noexcept;

class VectorRenderer
{
public:
    void set_renderer(SDL_Renderer* renderer) noexcept { renderer_ = renderer; }
    bool render(const RenderScene& scene, const core::Camera2D& camera,
                const RenderOptions& options, std::string& error);

private:
    SDL_Renderer* renderer_ = nullptr;
};
}