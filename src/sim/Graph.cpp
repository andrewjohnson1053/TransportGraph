#include "sim/Graph.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace sim
{
namespace
{
constexpr float congestion_slowdown_threshold = 0.7F;
constexpr float congestion_slowdown_curve = 2.0F;
constexpr float minimum_congested_speed_multiplier = 0.15F;
constexpr double bus_lane_travel_time_multiplier = 0.9;
constexpr double signal_priority_travel_time_multiplier = 0.9;

void set_error(std::string* error, std::string message)
{
    if (error != nullptr)
    {
        *error = std::move(message);
    }
}

bool finite_non_negative(float value) noexcept
{
    return std::isfinite(value) && value >= 0.0F;
}

bool valid_node(const Node& node) noexcept
{
    return std::isfinite(node.pos_world.x) && std::isfinite(node.pos_world.y) &&
           node.waiting_cargo_tons >= 0.0F && std::isfinite(node.waiting_cargo_tons) &&
           finite_non_negative(node.intersection_throughput_per_sec);
}

bool valid_edge(const Edge& edge) noexcept
{
    return edge.base_lanes >= 1 && std::isfinite(edge.length_m) && edge.length_m > 0.0F &&
           std::isfinite(edge.speed_limit_mps) && edge.speed_limit_mps > 0.0F &&
           finite_non_negative(edge.congestion_ratio) &&
           finite_non_negative(edge.effective_speed_mps) &&
           std::isfinite(edge.roadwork_speed_mult) && edge.roadwork_speed_mult > 0.0F;
}

const nlohmann::json& required_field(const nlohmann::json& object, std::string_view key,
                                     std::string_view context)
{
    if (!object.is_object())
    {
        throw std::runtime_error(std::string{context} + " must be an object.");
    }
    const auto field = object.find(key);
    if (field == object.end())
    {
        throw std::runtime_error(std::string{context} + " is missing required field '" +
                                 std::string{key} + "'.");
    }
    return *field;
}

std::uint32_t read_uint32(const nlohmann::json& value, std::string_view field_name)
{
    if (value.is_number_unsigned())
    {
        const std::uint64_t parsed = value.get<std::uint64_t>();
        if (parsed <= std::numeric_limits<std::uint32_t>::max())
        {
            return static_cast<std::uint32_t>(parsed);
        }
    }
    else if (value.is_number_integer())
    {
        const std::int64_t parsed = value.get<std::int64_t>();
        if (parsed >= 0 && static_cast<std::uint64_t>(parsed) <= std::numeric_limits<std::uint32_t>::max())
        {
            return static_cast<std::uint32_t>(parsed);
        }
    }
    throw std::runtime_error("Field '" + std::string{field_name} + "' must be an unsigned 32-bit integer.");
}

float read_float(const nlohmann::json& value, std::string_view field_name)
{
    if (!value.is_number())
    {
        throw std::runtime_error("Field '" + std::string{field_name} + "' must be numeric.");
    }
    const double parsed = value.get<double>();
    if (!std::isfinite(parsed) || parsed < -std::numeric_limits<float>::max() ||
        parsed > std::numeric_limits<float>::max())
    {
        throw std::runtime_error("Field '" + std::string{field_name} + "' must be finite and representable as a float.");
    }
    return static_cast<float>(parsed);
}

bool read_bool(const nlohmann::json& value, std::string_view field_name)
{
    if (!value.is_boolean())
    {
        throw std::runtime_error("Field '" + std::string{field_name} + "' must be a boolean.");
    }
    return value.get<bool>();
}

std::string read_string(const nlohmann::json& value, std::string_view field_name)
{
    if (!value.is_string())
    {
        throw std::runtime_error("Field '" + std::string{field_name} + "' must be a string.");
    }
    return value.get<std::string>();
}

std::uint32_t optional_uint32(const nlohmann::json& object, std::string_view key,
                              std::uint32_t fallback, std::string_view context)
{
    const auto field = object.find(key);
    return field == object.end() ? fallback : read_uint32(*field, std::string{context} + "." + std::string{key});
}

float optional_float(const nlohmann::json& object, std::string_view key, float fallback,
                     std::string_view context)
{
    const auto field = object.find(key);
    return field == object.end() ? fallback : read_float(*field, std::string{context} + "." + std::string{key});
}

bool optional_bool(const nlohmann::json& object, std::string_view key, bool fallback,
                   std::string_view context)
{
    const auto field = object.find(key);
    return field == object.end() ? fallback : read_bool(*field, std::string{context} + "." + std::string{key});
}

core::NodeType parse_node_type(std::string_view value, std::string_view context)
{
    if (value == "Intersection") return core::NodeType::Intersection;
    if (value == "Stop") return core::NodeType::Stop;
    if (value == "Terminal") return core::NodeType::Terminal;
    if (value == "DepotAnchor") return core::NodeType::DepotAnchor;
    if (value == "RegionalGateway") return core::NodeType::RegionalGateway;
    throw std::runtime_error(std::string{context} + " has invalid node type '" + std::string{value} + "'.");
}

core::RoadClass parse_road_class(std::string_view value, std::string_view context)
{
    if (value == "SingleLane") return core::RoadClass::SingleLane;
    if (value == "TwoLane") return core::RoadClass::TwoLane;
    if (value == "MultiLane") return core::RoadClass::MultiLane;
    if (value == "Highway") return core::RoadClass::Highway;
    throw std::runtime_error(std::string{context} + " has invalid road class '" + std::string{value} + "'.");
}
}

float compute_edge_travel_time(const Edge& edge, float vehicle_max_speed_mps) noexcept
{
    if (!valid_edge(edge) || !std::isfinite(vehicle_max_speed_mps) || vehicle_max_speed_mps <= 0.0F)
    {
        return std::numeric_limits<float>::infinity();
    }

    const double base_speed = std::min(static_cast<double>(edge.speed_limit_mps),
                                       static_cast<double>(vehicle_max_speed_mps));
    const double excess_congestion = std::max(
        0.0, static_cast<double>(edge.congestion_ratio) - congestion_slowdown_threshold);
    const double congestion_speed_multiplier = std::max(
        static_cast<double>(minimum_congested_speed_multiplier),
        1.0 / (1.0 + congestion_slowdown_curve * excess_congestion * excess_congestion));
    const double roadwork_multiplier = edge.roadwork_active ? edge.roadwork_speed_mult : 1.0;

    double travel_time = static_cast<double>(edge.length_m) /
        (base_speed * congestion_speed_multiplier * roadwork_multiplier);
    if (edge.has_bus_lane)
    {
        travel_time *= bus_lane_travel_time_multiplier;
    }
    if (edge.has_signal_priority)
    {
        travel_time *= signal_priority_travel_time_multiplier;
    }

    if (!std::isfinite(travel_time) || travel_time <= 0.0 ||
        travel_time > static_cast<double>(std::numeric_limits<float>::max()))
    {
        return std::numeric_limits<float>::infinity();
    }
    return static_cast<float>(travel_time);
}

bool Graph::add_node(Node node, std::string* error)
{
    if (error != nullptr)
    {
        error->clear();
    }
    if (nodes_.size() >= core::InvalidId)
    {
        set_error(error, "Cannot add node: node ID capacity exhausted.");
        return false;
    }

    const auto index = static_cast<std::uint32_t>(nodes_.size());
    if (node.id.is_valid() && node.id.value() != index)
    {
        set_error(error, "Cannot add node: supplied ID does not match the next pool index.");
        return false;
    }
    if (!valid_node(node))
    {
        set_error(error, "Cannot add node: position must be finite and cargo/throughput values must be finite and non-negative.");
        return false;
    }

    node.id = core::NodeId{index};
    nodes_.push_back(node);
    out_edges_.emplace_back();
    in_edges_.emplace_back();
    return true;
}

bool Graph::add_edge(Edge edge, std::string* error)
{
    if (error != nullptr)
    {
        error->clear();
    }
    if (edges_.size() >= core::InvalidId)
    {
        set_error(error, "Cannot add edge: edge ID capacity exhausted.");
        return false;
    }
    if (!edge.from_node.is_valid() || edge.from_node.value() >= nodes_.size() ||
        !edge.to_node.is_valid() || edge.to_node.value() >= nodes_.size())
    {
        set_error(error, "Cannot add edge: endpoint node ID is out of range.");
        return false;
    }
    if (!valid_edge(edge))
    {
        set_error(error, "Cannot add edge: lane count must be positive, length and speed must be finite and positive, and dynamic numeric values must be finite and non-negative.");
        return false;
    }

    const auto index = static_cast<std::uint32_t>(edges_.size());
    if (edge.id.is_valid() && edge.id.value() != index)
    {
        set_error(error, "Cannot add edge: supplied ID does not match the next pool index.");
        return false;
    }

    edge.id = core::EdgeId{index};
    edges_.push_back(edge);
    out_edges_[edge.from_node.value()].push_back(edge.id);
    in_edges_[edge.to_node.value()].push_back(edge.id);
    if (edge.bidirectional && edge.from_node != edge.to_node)
    {
        out_edges_[edge.to_node.value()].push_back(edge.id);
        in_edges_[edge.from_node.value()].push_back(edge.id);
    }
    return true;
}

bool Graph::load_from_json(const std::filesystem::path& path, std::string* error)
{
    if (error != nullptr)
    {
        error->clear();
    }

    std::ifstream input{path};
    if (!input)
    {
        set_error(error, ("Cannot open map file '" + path.string() + "'.").c_str());
        return false;
    }

    try
    {
        const nlohmann::json document = nlohmann::json::parse(input);
        if (input.bad())
        {
            set_error(error, ("Failed while reading map file '" + path.string() + "'.").c_str());
            return false;
        }

        const auto& nodes_json = required_field(document, "nodes", "Map");
        const auto& edges_json = required_field(document, "edges", "Map");
        if (!nodes_json.is_array() || nodes_json.empty())
        {
            throw std::runtime_error("Map field 'nodes' must be a non-empty array.");
        }
        if (!edges_json.is_array() || edges_json.empty())
        {
            throw std::runtime_error("Map field 'edges' must be a non-empty array.");
        }
        if (nodes_json.size() >= core::InvalidId || edges_json.size() >= core::InvalidId)
        {
            throw std::runtime_error("Map exceeds the maximum supported node or edge ID count.");
        }

        Graph candidate;
        std::unordered_set<std::uint32_t> node_ids;
        node_ids.reserve(nodes_json.size());
        for (std::size_t index = 0; index < nodes_json.size(); ++index)
        {
            const auto& node_json = nodes_json[index];
            const std::string context = "Node at index " + std::to_string(index);
            Node node;
            const std::uint32_t node_id = read_uint32(required_field(node_json, "id", context), context + ".id");
            if (!node_ids.insert(node_id).second)
            {
                throw std::runtime_error(context + " has duplicate node ID " + std::to_string(node_id) + ".");
            }
            if (node_id != index)
            {
                throw std::runtime_error(context + " ID must match its array index.");
            }
            node.id = core::NodeId{node_id};
            node.pos_world.x = read_float(required_field(node_json, "x", context), context + ".x");
            node.pos_world.y = read_float(required_field(node_json, "y", context), context + ".y");
            node.type = parse_node_type(
                read_string(required_field(node_json, "type", context), context + ".type"), context);
            node.waiting_passengers = optional_uint32(node_json, "waiting_passengers", 0, context);
            node.waiting_cargo_tons = optional_float(node_json, "waiting_cargo_tons", 0.0F, context);
            node.intersection_throughput_per_sec = optional_float(
                node_json, "intersection_throughput_per_sec", 0.0F, context);

            std::string insertion_error;
            if (!candidate.add_node(node, &insertion_error))
            {
                throw std::runtime_error(context + ": " + insertion_error);
            }
        }

        std::unordered_set<std::uint32_t> edge_ids;
        edge_ids.reserve(edges_json.size());
        for (std::size_t index = 0; index < edges_json.size(); ++index)
        {
            const auto& edge_json = edges_json[index];
            const std::string context = "Edge at index " + std::to_string(index);
            Edge edge;
            const std::uint32_t edge_id = read_uint32(required_field(edge_json, "id", context), context + ".id");
            if (!edge_ids.insert(edge_id).second)
            {
                throw std::runtime_error(context + " has duplicate edge ID " + std::to_string(edge_id) + ".");
            }
            if (edge_id != index)
            {
                throw std::runtime_error(context + " ID must match its array index.");
            }
            edge.id = core::EdgeId{edge_id};
            edge.from_node = core::NodeId{read_uint32(
                required_field(edge_json, "from_node", context), context + ".from_node")};
            edge.to_node = core::NodeId{read_uint32(
                required_field(edge_json, "to_node", context), context + ".to_node")};
            edge.bidirectional = read_bool(
                required_field(edge_json, "bidirectional", context), context + ".bidirectional");
            edge.road_class = parse_road_class(
                read_string(required_field(edge_json, "road_class", context), context + ".road_class"), context);
            const std::uint32_t lanes = read_uint32(
                required_field(edge_json, "base_lanes", context), context + ".base_lanes");
            if (lanes > std::numeric_limits<std::uint8_t>::max())
            {
                throw std::runtime_error(context + " lane count exceeds the supported range.");
            }
            edge.base_lanes = static_cast<std::uint8_t>(lanes);
            edge.length_m = read_float(required_field(edge_json, "length_m", context), context + ".length_m");
            edge.speed_limit_mps = read_float(
                required_field(edge_json, "speed_limit_mps", context), context + ".speed_limit_mps");
            edge.active_vehicles = optional_uint32(edge_json, "active_vehicles", 0, context);
            edge.congestion_ratio = optional_float(edge_json, "congestion_ratio", 0.0F, context);
            edge.effective_speed_mps = optional_float(edge_json, "effective_speed_mps", 0.0F, context);
            edge.has_pullout_bays = optional_bool(edge_json, "has_pullout_bays", false, context);
            edge.has_bus_lane = optional_bool(edge_json, "has_bus_lane", false, context);
            edge.has_signal_priority = optional_bool(edge_json, "has_signal_priority", false, context);
            edge.roadwork_active = optional_bool(edge_json, "roadwork_active", false, context);
            edge.roadwork_speed_mult = optional_float(edge_json, "roadwork_speed_mult", 1.0F, context);

            std::string insertion_error;
            if (!candidate.add_edge(edge, &insertion_error))
            {
                throw std::runtime_error(context + ": " + insertion_error);
            }
        }

        *this = std::move(candidate);
        return true;
    }
    catch (const std::runtime_error& exception)
    {
        if (error != nullptr)
        {
            *error = "Invalid map file '" + path.string() + "': " + exception.what();
        }
        return false;
    }
    catch (const nlohmann::json::exception& exception)
    {
        if (error != nullptr)
        {
            *error = "Invalid JSON in map file '" + path.string() + "': " + exception.what();
        }
        return false;
    }
}

bool Graph::rebuild_adjacency(std::string* error)
{
    if (error != nullptr)
    {
        error->clear();
    }

    std::vector<std::vector<core::EdgeId>> out_edges(nodes_.size());
    std::vector<std::vector<core::EdgeId>> in_edges(nodes_.size());
    for (std::size_t index = 0; index < nodes_.size(); ++index)
    {
        if (!nodes_[index].id.is_valid() || nodes_[index].id.value() != index)
        {
            set_error(error, "Cannot rebuild adjacency: node ID does not match its pool index.");
            return false;
        }
        if (!valid_node(nodes_[index]))
        {
            set_error(error, "Cannot rebuild adjacency: node contains invalid position or numeric values.");
            return false;
        }
    }
    for (std::size_t index = 0; index < edges_.size(); ++index)
    {
        const Edge& edge = edges_[index];
        if (!edge.id.is_valid() || edge.id.value() != index)
        {
            set_error(error, "Cannot rebuild adjacency: edge ID does not match its pool index.");
            return false;
        }
        if (!edge.from_node.is_valid() || edge.from_node.value() >= nodes_.size() ||
            !edge.to_node.is_valid() || edge.to_node.value() >= nodes_.size())
        {
            set_error(error, "Cannot rebuild adjacency: edge endpoint node ID is out of range.");
            return false;
        }
        if (!valid_edge(edge))
        {
            set_error(error, "Cannot rebuild adjacency: edge contains invalid lane, length, speed, or dynamic numeric values.");
            return false;
        }

        out_edges[edge.from_node.value()].push_back(edge.id);
        in_edges[edge.to_node.value()].push_back(edge.id);
        if (edge.bidirectional && edge.from_node != edge.to_node)
        {
            out_edges[edge.to_node.value()].push_back(edge.id);
            in_edges[edge.from_node.value()].push_back(edge.id);
        }
    }

    out_edges_.swap(out_edges);
    in_edges_.swap(in_edges);
    return true;
}

const Node* Graph::find_node(core::NodeId id) const noexcept
{
    return id.is_valid() && id.value() < nodes_.size() ? &nodes_[id.value()] : nullptr;
}

const Edge* Graph::find_edge(core::EdgeId id) const noexcept
{
    return id.is_valid() && id.value() < edges_.size() ? &edges_[id.value()] : nullptr;
}
}