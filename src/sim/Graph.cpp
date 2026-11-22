#include "sim/Graph.h"

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

Node* Graph::find_node(core::NodeId id) noexcept
{
    return id.is_valid() && id.value() < nodes_.size() ? &nodes_[id.value()] : nullptr;
}

const Node* Graph::find_node(core::NodeId id) const noexcept
{
    return id.is_valid() && id.value() < nodes_.size() ? &nodes_[id.value()] : nullptr;
}

Edge* Graph::find_edge(core::EdgeId id) noexcept
{
    return id.is_valid() && id.value() < edges_.size() ? &edges_[id.value()] : nullptr;
}

const Edge* Graph::find_edge(core::EdgeId id) const noexcept
{
    return id.is_valid() && id.value() < edges_.size() ? &edges_[id.value()] : nullptr;
}
}