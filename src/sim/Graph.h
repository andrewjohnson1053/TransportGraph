#pragma once

#include "core/Types.h"

#include <glm/vec2.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace sim
{
struct Node
{
    core::NodeId id{};
    glm::vec2 pos_world{0.0F, 0.0F};
    core::NodeType type = core::NodeType::Intersection;
    std::uint32_t waiting_passengers = 0;
    float waiting_cargo_tons = 0.0F;
    float intersection_throughput_per_sec = 0.0F;
};

struct Edge
{
    core::EdgeId id{};
    core::NodeId from_node{};
    core::NodeId to_node{};
    bool bidirectional = false;
    core::RoadClass road_class = core::RoadClass::SingleLane;
    std::uint8_t base_lanes = 1;
    float length_m = 0.0F;
    float speed_limit_mps = 0.0F;
    std::uint32_t active_vehicles = 0;
    float congestion_ratio = 0.0F;
    float effective_speed_mps = 0.0F;
    bool has_pullout_bays = false;
    bool has_bus_lane = false;
    bool has_signal_priority = false;
    bool roadwork_active = false;
    float roadwork_speed_mult = 1.0F;
};

class Graph
{
public:
    bool add_node(Node node, std::string* error = nullptr);
    bool add_edge(Edge edge, std::string* error = nullptr);
    bool rebuild_adjacency(std::string* error = nullptr);

    const std::vector<Node>& nodes() const noexcept { return nodes_; }
    const std::vector<Edge>& edges() const noexcept { return edges_; }
    const std::vector<std::vector<core::EdgeId>>& out_edges() const noexcept { return out_edges_; }
    const std::vector<std::vector<core::EdgeId>>& in_edges() const noexcept { return in_edges_; }

    Node* find_node(core::NodeId id) noexcept;
    const Node* find_node(core::NodeId id) const noexcept;
    Edge* find_edge(core::EdgeId id) noexcept;
    const Edge* find_edge(core::EdgeId id) const noexcept;

private:
    std::vector<Node> nodes_;
    std::vector<Edge> edges_;
    std::vector<std::vector<core::EdgeId>> out_edges_;
    std::vector<std::vector<core::EdgeId>> in_edges_;
};
}