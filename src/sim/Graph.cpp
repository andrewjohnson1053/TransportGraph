#include "sim/Graph.h"

#include <cmath>
#include <limits>
#include <utility>

namespace sim
{
namespace
{
void set_error(std::string* error, const char* message)
{
    if (error != nullptr)
    {
        *error = message;
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