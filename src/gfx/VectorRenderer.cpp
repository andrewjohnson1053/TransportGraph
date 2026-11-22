#include "gfx/VectorRenderer.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace gfx
{
namespace
{
constexpr float pi = 3.14159265358979323846f;
constexpr int circle_segments = 20;
constexpr float node_radius_px = 6.0f;
constexpr float badge_pixel_px = 1.35f;

bool finite(glm::vec2 value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

SDL_Vertex make_vertex(glm::vec2 position, Color color) noexcept
{
    return {{position.x, position.y},
            {static_cast<float>(color.r) / 255.0f,
             static_cast<float>(color.g) / 255.0f,
             static_cast<float>(color.b) / 255.0f,
             static_cast<float>(color.a) / 255.0f},
            {0.0f, 0.0f}};
}

class GeometryBatch
{
public:
    void clear() noexcept
    {
        vertices_.clear();
        indices_.clear();
    }

    void triangle(glm::vec2 a, glm::vec2 b, glm::vec2 c, Color color)
    {
        const int base = static_cast<int>(vertices_.size());
        vertices_.push_back(make_vertex(a, color));
        vertices_.push_back(make_vertex(b, color));
        vertices_.push_back(make_vertex(c, color));
        indices_.insert(indices_.end(), {base, base + 1, base + 2});
    }

    void quad(glm::vec2 a, glm::vec2 b, glm::vec2 c, glm::vec2 d, Color color)
    {
        const int base = static_cast<int>(vertices_.size());
        vertices_.push_back(make_vertex(a, color));
        vertices_.push_back(make_vertex(b, color));
        vertices_.push_back(make_vertex(c, color));
        vertices_.push_back(make_vertex(d, color));
        indices_.insert(indices_.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    }

    void line(glm::vec2 start, glm::vec2 end, float width, Color color)
    {
        const glm::vec2 delta = end - start;
        const float length = std::hypot(delta.x, delta.y);
        if (!std::isfinite(length) || length <= 1.0e-4f || !std::isfinite(width) || width <= 0.0f)
        {
            return;
        }
        const glm::vec2 normal{-delta.y * (width * 0.5f / length), delta.x * (width * 0.5f / length)};
        quad(start + normal, end + normal, end - normal, start - normal, color);
    }

    void filled_circle(glm::vec2 center, float radius, Color color)
    {
        for (int segment = 0; segment < circle_segments; ++segment)
        {
            const float a0 = 2.0f * pi * static_cast<float>(segment) / static_cast<float>(circle_segments);
            const float a1 = 2.0f * pi * static_cast<float>(segment + 1) / static_cast<float>(circle_segments);
            triangle(center,
                     center + glm::vec2{std::cos(a0), std::sin(a0)} * radius,
                     center + glm::vec2{std::cos(a1), std::sin(a1)} * radius,
                     color);
        }
    }

    void ring(glm::vec2 center, float radius, float width, Color color)
    {
        for (int segment = 0; segment < circle_segments; ++segment)
        {
            const float a0 = 2.0f * pi * static_cast<float>(segment) / static_cast<float>(circle_segments);
            const float a1 = 2.0f * pi * static_cast<float>(segment + 1) / static_cast<float>(circle_segments);
            const glm::vec2 direction0{std::cos(a0), std::sin(a0)};
            const glm::vec2 direction1{std::cos(a1), std::sin(a1)};
            quad(center + direction0 * (radius - width * 0.5f),
                 center + direction1 * (radius - width * 0.5f),
                 center + direction1 * (radius + width * 0.5f),
                 center + direction0 * (radius + width * 0.5f), color);
        }
    }

    bool draw(SDL_Renderer* renderer, std::string& error) const
    {
        if (vertices_.empty())
        {
            return true;
        }
        if (vertices_.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
            indices_.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        {
            error = "Vector render batch exceeds SDL geometry limits.";
            return false;
        }
        if (!SDL_RenderGeometry(renderer, nullptr, vertices_.data(), static_cast<int>(vertices_.size()),
                                indices_.data(), static_cast<int>(indices_.size())))
        {
            error = std::string{"SDL_RenderGeometry failed: "} + SDL_GetError();
            return false;
        }
        return true;
    }

private:
    std::vector<SDL_Vertex> vertices_;
    std::vector<int> indices_;
};

template <typename IdType>
struct IdHash
{
    std::size_t operator()(IdType id) const noexcept
    {
        return static_cast<std::size_t>(id.value());
    }
};

class SceneLookup
{
public:
    bool build(const RenderScene& scene, std::string& error)
    {
        nodes_.reserve(scene.nodes.size());
        edges_.reserve(scene.edges.size());
        vehicles_.reserve(scene.vehicles.size());
        for (const RenderNode& node : scene.nodes)
        {
            if (!node.id.is_valid())
            {
                error = "Render scene contains a node with an invalid ID.";
                return false;
            }
            if (!nodes_.emplace(node.id, &node).second)
            {
                error = "Render scene contains duplicate node IDs.";
                return false;
            }
        }
        for (const RenderEdge& edge : scene.edges)
        {
            if (!edge.id.is_valid())
            {
                error = "Render scene contains an edge with an invalid ID.";
                return false;
            }
            if (!edges_.emplace(edge.id, &edge).second)
            {
                error = "Render scene contains duplicate edge IDs.";
                return false;
            }
        }
        for (const RenderVehicle& vehicle : scene.vehicles)
        {
            if (!vehicle.id.is_valid())
            {
                error = "Render scene contains a vehicle with an invalid ID.";
                return false;
            }
            if (!vehicles_.emplace(vehicle.id, &vehicle).second)
            {
                error = "Render scene contains duplicate vehicle IDs.";
                return false;
            }
        }
        return true;
    }

    const RenderNode* find_node(core::NodeId id) const noexcept
    {
        const auto it = nodes_.find(id);
        return it == nodes_.end() ? nullptr : it->second;
    }

    const RenderEdge* find_edge(core::EdgeId id) const noexcept
    {
        const auto it = edges_.find(id);
        return it == edges_.end() ? nullptr : it->second;
    }

private:
    std::unordered_map<core::NodeId, const RenderNode*, IdHash<core::NodeId>> nodes_;
    std::unordered_map<core::EdgeId, const RenderEdge*, IdHash<core::EdgeId>> edges_;
    std::unordered_map<core::VehicleId, const RenderVehicle*, IdHash<core::VehicleId>> vehicles_;
};

Color node_color(core::NodeType type) noexcept
{
    switch (type)
    {
    case core::NodeType::Intersection: return {205, 213, 224, 255};
    case core::NodeType::Stop: return {91, 196, 132, 255};
    case core::NodeType::Terminal: return {255, 190, 83, 255};
    case core::NodeType::DepotAnchor: return {190, 133, 255, 255};
    case core::NodeType::RegionalGateway: return {83, 205, 232, 255};
    }
    return {255, 0, 255, 255};
}

void append_node_marker(GeometryBatch& batch, const RenderNode& node, glm::vec2 screen, float scale)
{
    const Color color = node_color(node.type);
    const auto at = [screen, scale](glm::vec2 offset) { return screen + offset * scale; };
    switch (node.type)
    {
    case core::NodeType::Intersection:
        batch.filled_circle(screen, node_radius_px * 0.65f * scale, color);
        break;
    case core::NodeType::Stop:
        batch.quad(at({-4.0f, -4.0f}), at({4.0f, -4.0f}), at({4.0f, 4.0f}), at({-4.0f, 4.0f}), color);
        break;
    case core::NodeType::Terminal:
        batch.triangle(at({0.0f, -6.0f}), at({6.0f, 0.0f}), at({0.0f, 6.0f}), color);
        batch.triangle(at({0.0f, -6.0f}), at({0.0f, 6.0f}), at({-6.0f, 0.0f}), color);
        break;
    case core::NodeType::DepotAnchor:
        batch.quad(at({-5.0f, -5.0f}), at({5.0f, -5.0f}), at({5.0f, 5.0f}), at({-5.0f, 5.0f}), color);
        batch.quad(at({-2.0f, -2.0f}), at({2.0f, -2.0f}), at({2.0f, 2.0f}), at({-2.0f, 2.0f}), {34, 37, 46, 255});
        break;
    case core::NodeType::RegionalGateway:
        batch.triangle(at({0.0f, -6.0f}), at({6.0f, 5.0f}), at({-6.0f, 5.0f}), color);
        break;
    }
}

using Glyph = std::array<unsigned char, 5>;

Glyph glyph_for(char character) noexcept
{
    switch (character)
    {
    case '0': return {7, 5, 5, 5, 7};
    case '1': return {2, 6, 2, 2, 7};
    case '2': return {7, 1, 7, 4, 7};
    case '3': return {7, 1, 7, 1, 7};
    case '4': return {5, 5, 7, 1, 1};
    case '5': return {7, 4, 7, 1, 7};
    case '6': return {7, 4, 7, 5, 7};
    case '7': return {7, 1, 1, 2, 2};
    case '8': return {7, 5, 7, 5, 7};
    case '9': return {7, 5, 7, 1, 7};
    case '+': return {0, 2, 7, 2, 0};
    default: return {0, 0, 0, 0, 0};
    }
}

void append_backlog_badge(GeometryBatch& batch, glm::vec2 screen, std::uint32_t waiting)
{
    if (waiting == 0)
    {
        return;
    }

    std::array<char, 16> text_buffer{};
    const auto converted = std::to_chars(text_buffer.data(), text_buffer.data() + text_buffer.size(), waiting);
    std::string_view text{text_buffer.data(), static_cast<std::size_t>(converted.ptr - text_buffer.data())};
    if (text.size() > 3)
    {
        text = "999+";
    }
    const float glyph_width = 3.0f * badge_pixel_px;
    const float gap = badge_pixel_px;
    const float height = 13.0f;
    const float width = std::max(height, static_cast<float>(text.size()) * glyph_width +
        static_cast<float>(text.size() - 1) * gap + 6.0f);
    const glm::vec2 center = screen + glm::vec2{10.0f, -11.0f};
    const glm::vec2 top_left = center + glm::vec2{-width * 0.5f, -height * 0.5f};
    const float radius = height * 0.5f;
    batch.quad({top_left.x + radius, top_left.y}, {top_left.x + width - radius, top_left.y},
               {top_left.x + width - radius, top_left.y + height},
               {top_left.x + radius, top_left.y + height}, {19, 28, 40, 245});
    batch.filled_circle({top_left.x + radius, center.y}, radius, {19, 28, 40, 245});
    batch.filled_circle({top_left.x + width - radius, center.y}, radius, {19, 28, 40, 245});
    for (std::size_t index = 0; index < text.size(); ++index)
    {
        const Glyph glyph = glyph_for(text[index]);
        const float x_base = top_left.x + 3.0f + static_cast<float>(index) * (glyph_width + gap);
        for (std::size_t row = 0; row < glyph.size(); ++row)
        {
            for (std::size_t column = 0; column < 3; ++column)
            {
                if ((glyph[row] & static_cast<unsigned char>(1U << (2U - column))) == 0)
                {
                    continue;
                }
                const float x = x_base + static_cast<float>(column) * badge_pixel_px;
                const float y = top_left.y + 2.0f + static_cast<float>(row) * badge_pixel_px;
                batch.quad({x, y}, {x + badge_pixel_px, y},
                           {x + badge_pixel_px, y + badge_pixel_px}, {x, y + badge_pixel_px},
                           {255, 255, 255, 255});
            }
        }
    }
}

bool edge_screen_points(const SceneLookup& lookup, const RenderEdge& edge, const core::Camera2D& camera,
                        glm::vec2& start, glm::vec2& end) noexcept
{
    const RenderNode* from = lookup.find_node(edge.from_node);
    const RenderNode* to = lookup.find_node(edge.to_node);
    if (from == nullptr || to == nullptr)
    {
        return false;
    }
    start = camera.world_to_screen(from->position_world);
    end = camera.world_to_screen(to->position_world);
    return finite(start) && finite(end);
}

bool valid_scene(const RenderScene& scene, const SceneLookup& lookup, std::string& error)
{
    for (const RenderNode& node : scene.nodes)
    {
        if (!finite(node.position_world))
        {
            error = "Render scene contains a node with a non-finite position.";
            return false;
        }
    }
    for (const RenderEdge& edge : scene.edges)
    {
        if (!edge.from_node.is_valid() || !edge.to_node.is_valid() || edge.base_lanes == 0 ||
            road_class_width_px(edge.road_class) <= 0.0f || !std::isfinite(edge.length_m) ||
            edge.length_m <= 0.0f || !std::isfinite(edge.congestion_ratio) || edge.congestion_ratio < 0.0f ||
            lookup.find_node(edge.from_node) == nullptr || lookup.find_node(edge.to_node) == nullptr)
        {
            error = "Render scene contains an edge with invalid endpoints or numeric properties.";
            return false;
        }
    }
    for (const RenderRoute& route : scene.routes)
    {
        if (!route.company_id.is_valid())
        {
            error = "Render scene contains a route with an invalid company ID.";
            return false;
        }
        for (const core::EdgeId edge_id : route.edge_ids)
        {
            if (!edge_id.is_valid() || lookup.find_edge(edge_id) == nullptr)
            {
                error = "Render scene contains a route that references an invalid edge.";
                return false;
            }
        }
    }
    for (const RenderVehicle& vehicle : scene.vehicles)
    {
        if (!vehicle.company_id.is_valid() || !std::isfinite(vehicle.edge_dist_m) ||
            lookup.find_edge(vehicle.current_edge_id) == nullptr)
        {
            error = "Render scene contains a vehicle with an invalid edge or position.";
            return false;
        }
    }
    return true;
}
}

Color road_class_color(core::RoadClass road_class) noexcept
{
    switch (road_class)
    {
    case core::RoadClass::SingleLane: return {177, 187, 201, 255};
    case core::RoadClass::TwoLane: return {137, 151, 170, 255};
    case core::RoadClass::MultiLane: return {71, 151, 224, 255};
    case core::RoadClass::Highway: return {246, 151, 74, 255};
    }
    return {255, 0, 255, 255};
}

float road_class_width_px(core::RoadClass road_class) noexcept
{
    switch (road_class)
    {
    case core::RoadClass::SingleLane: return 1.8f;
    case core::RoadClass::TwoLane: return 3.0f;
    case core::RoadClass::MultiLane: return 4.2f;
    case core::RoadClass::Highway: return 6.0f;
    }
    return 0.0f;
}

Color heatmap_color(float saturation) noexcept
{
    if (!std::isfinite(saturation))
    {
        return {255, 0, 255, 255};
    }
    constexpr Color green{50, 190, 96, 255};
    constexpr Color yellow{246, 213, 70, 255};
    constexpr Color red{239, 74, 70, 255};
    const float value = std::clamp(saturation, 0.0f, 1.0f);
    const Color from = value <= 0.7f ? green : yellow;
    const Color to = value <= 0.7f ? yellow : red;
    const float range_start = value <= 0.7f ? 0.0f : 0.7f;
    const float range = value <= 0.7f ? 0.7f : 0.3f;
    const float amount = (value - range_start) / range;
    const auto interpolate = [amount](unsigned char a, unsigned char b)
    {
        return static_cast<unsigned char>(std::lround(static_cast<float>(a) +
                                                       (static_cast<float>(b) - a) * amount));
    };
    return {interpolate(from.r, to.r), interpolate(from.g, to.g), interpolate(from.b, to.b), 255};
}

Color company_color(core::CompanyId company_id) noexcept
{
    constexpr std::array<Color, 6> palette{{
        {255, 210, 95, 255}, {81, 200, 245, 255}, {244, 115, 130, 255},
        {151, 224, 123, 255}, {198, 145, 255, 255}, {255, 155, 78, 255}
    }};
    return company_id.is_valid() ? palette[company_id.value() % palette.size()] : Color{255, 0, 255, 255};
}

namespace
{
std::optional<glm::vec2> vehicle_world_position(const SceneLookup& lookup, const RenderVehicle& vehicle) noexcept
{
    const RenderEdge* edge = lookup.find_edge(vehicle.current_edge_id);
    if (edge == nullptr || !std::isfinite(vehicle.edge_dist_m) || !std::isfinite(edge->length_m) ||
        edge->length_m <= 0.0f)
    {
        return std::nullopt;
    }
    const RenderNode* from = lookup.find_node(edge->from_node);
    const RenderNode* to = lookup.find_node(edge->to_node);
    if (from == nullptr || to == nullptr || !finite(from->position_world) || !finite(to->position_world))
    {
        return std::nullopt;
    }
    const float distance = std::clamp(vehicle.edge_dist_m, 0.0f, edge->length_m);
    const float progress = distance / edge->length_m;
    const glm::vec2 position = from->position_world + (to->position_world - from->position_world) * progress;
    return finite(position) ? std::optional<glm::vec2>{position} : std::nullopt;
}
}

std::optional<glm::vec2> vehicle_world_position(const RenderScene& scene, const RenderVehicle& vehicle) noexcept
{
    const auto edge_it = std::find_if(scene.edges.begin(), scene.edges.end(),
                                      [&vehicle](const RenderEdge& edge) {
                                          return edge.id == vehicle.current_edge_id;
                                      });
    if (edge_it == scene.edges.end() || !std::isfinite(vehicle.edge_dist_m) ||
        !std::isfinite(edge_it->length_m) || edge_it->length_m <= 0.0f)
    {
        return std::nullopt;
    }
    const auto from_it = std::find_if(scene.nodes.begin(), scene.nodes.end(),
                                      [&edge_it](const RenderNode& node) { return node.id == edge_it->from_node; });
    const auto to_it = std::find_if(scene.nodes.begin(), scene.nodes.end(),
                                    [&edge_it](const RenderNode& node) { return node.id == edge_it->to_node; });
    if (from_it == scene.nodes.end() || to_it == scene.nodes.end() ||
        !finite(from_it->position_world) || !finite(to_it->position_world))
    {
        return std::nullopt;
    }
    const float distance = std::clamp(vehicle.edge_dist_m, 0.0f, edge_it->length_m);
    const float progress = distance / edge_it->length_m;
    const glm::vec2 position = from_it->position_world + (to_it->position_world - from_it->position_world) * progress;
    return finite(position) ? std::optional<glm::vec2>{position} : std::nullopt;
}

bool VectorRenderer::render(const RenderScene& scene, const core::Camera2D& camera,
                            const RenderOptions& options, std::string& error)
{
    error.clear();
    if (renderer_ == nullptr)
    {
        error = "VectorRenderer has no SDL renderer.";
        return false;
    }
    if (!std::isfinite(camera.zoom()) || camera.zoom() <= 0.0f || !finite(camera.center_world()) ||
        !finite(camera.viewport_size_px()))
    {
        error = "Camera contains invalid values for rendering.";
        return false;
    }
    SceneLookup lookup;
    if (!lookup.build(scene, error) || !valid_scene(scene, lookup, error))
    {
        return false;
    }

    const float zoom_scale = std::clamp(camera.zoom(), 0.75f, 2.0f);
    GeometryBatch batch;
    for (const RenderEdge& edge : scene.edges)
    {
        glm::vec2 start;
        glm::vec2 end;
        if (!edge_screen_points(lookup, edge, camera, start, end))
        {
            error = "Unable to transform edge endpoints to screen coordinates.";
            return false;
        }
        const Color color = options.show_heatmap ? heatmap_color(edge.congestion_ratio) : road_class_color(edge.road_class);
        const float width = road_class_width_px(edge.road_class) * zoom_scale;
        batch.line(start, end, width, color);
        if (options.hovered_edge == edge.id)
        {
            batch.line(start, end, width + 4.0f * zoom_scale, {255, 255, 255, 95});
            batch.line(start, end, width, color);
        }
    }
    if (options.selected_rival_id)
    {
        const Color route_color = company_color(*options.selected_rival_id);
        for (const RenderRoute& route : scene.routes)
        {
            if (route.company_id != *options.selected_rival_id)
            {
                continue;
            }
            for (const core::EdgeId edge_id : route.edge_ids)
            {
                const RenderEdge* edge = lookup.find_edge(edge_id);
                glm::vec2 start;
                glm::vec2 end;
                if (edge != nullptr && edge_screen_points(lookup, *edge, camera, start, end))
                {
                    batch.line(start, end, 2.0f * zoom_scale, route_color);
                }
            }
        }
    }
    if (!batch.draw(renderer_, error)) return false;

    batch.clear();
    for (const RenderNode& node : scene.nodes)
    {
        const glm::vec2 screen = camera.world_to_screen(node.position_world);
        if (!finite(screen))
        {
            error = "Unable to transform node position to screen coordinates.";
            return false;
        }
        if (options.selected_node == node.id)
        {
            batch.ring(screen, 11.0f * zoom_scale, 2.0f * zoom_scale, {255, 220, 100, 255});
        }
        if (options.hovered_node == node.id)
        {
            batch.ring(screen, 8.5f * zoom_scale, 1.5f * zoom_scale, {255, 255, 255, 230});
        }
        append_node_marker(batch, node, screen, zoom_scale);
        append_backlog_badge(batch, screen, node.waiting_passengers);
    }
    if (!batch.draw(renderer_, error)) return false;

    batch.clear();
    for (const RenderVehicle& vehicle : scene.vehicles)
    {
        const RenderEdge* edge = lookup.find_edge(vehicle.current_edge_id);
        const RenderNode* from = edge == nullptr ? nullptr : lookup.find_node(edge->from_node);
        const RenderNode* to = edge == nullptr ? nullptr : lookup.find_node(edge->to_node);
        const auto world_position = vehicle_world_position(lookup, vehicle);
        if (edge == nullptr || from == nullptr || to == nullptr || !world_position)
        {
            error = "Unable to locate a vehicle on its current edge.";
            return false;
        }
        const glm::vec2 screen = camera.world_to_screen(*world_position);
        const glm::vec2 direction = camera.world_to_screen(to->position_world) - camera.world_to_screen(from->position_world);
        const float length = std::hypot(direction.x, direction.y);
        if (!finite(screen) || !std::isfinite(length) || length <= 1.0e-4f)
        {
            continue;
        }
        const glm::vec2 forward = direction / length;
        const glm::vec2 side{-forward.y, forward.x};
        Color color = company_color(vehicle.company_id);
        if (options.selected_rival_id && vehicle.company_id != *options.selected_rival_id)
        {
            color = {102, 110, 124, 180};
        }
        batch.triangle(screen + forward * (7.0f * zoom_scale),
                       screen - forward * (5.0f * zoom_scale) + side * (4.5f * zoom_scale),
                       screen - forward * (5.0f * zoom_scale) - side * (4.5f * zoom_scale),
                       color);
        if (options.selected_rival_id && vehicle.company_id == *options.selected_rival_id)
        {
            batch.ring(screen, 9.0f * zoom_scale, 1.5f * zoom_scale, color);
        }
    }
    return batch.draw(renderer_, error);
}
}