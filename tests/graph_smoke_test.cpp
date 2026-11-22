#include "sim/Graph.h"

#include <filesystem>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

int main()
{
    static_assert(std::is_same_v<decltype(std::declval<sim::Graph&>().find_node(core::NodeId{})),
                                 const sim::Node*>);
    static_assert(std::is_same_v<decltype(std::declval<sim::Graph&>().find_edge(core::EdgeId{})),
                                 const sim::Edge*>);

    const std::filesystem::path sampleMap{"assets/maps/sample_city.json"};
    if (!std::filesystem::is_regular_file(sampleMap))
    {
        std::cerr << "Missing sample map asset: " << sampleMap << '\n';
        return 1;
    }

    sim::Graph graph;
    std::string error;
    sim::Node node0;
    sim::Node node1;
    node1.pos_world = {10.0F, 0.0F};
    sim::Node node2;
    node2.pos_world = {20.0F, 0.0F};
    if (!graph.add_node(node0, &error) || !graph.add_node(node1, &error) ||
        !graph.add_node(node2, &error))
    {
        std::cerr << "Failed to add graph nodes: " << error << '\n';
        return 1;
    }
    if (graph.nodes().size() != 3 || graph.nodes()[0].id != core::NodeId{0} ||
        graph.nodes()[1].id != core::NodeId{1} || graph.nodes()[2].id != core::NodeId{2})
    {
        std::cerr << "Node IDs do not match their pool indices.\n";
        return 1;
    }

    sim::Edge bidirectional;
    bidirectional.from_node = core::NodeId{0};
    bidirectional.to_node = core::NodeId{1};
    bidirectional.bidirectional = true;
    bidirectional.base_lanes = 2;
    bidirectional.length_m = 100.0F;
    bidirectional.speed_limit_mps = 13.4F;
    if (!graph.add_edge(bidirectional, &error))
    {
        std::cerr << "Failed to add bidirectional edge: " << error << '\n';
        return 1;
    }

    sim::Edge one_way;
    one_way.from_node = core::NodeId{1};
    one_way.to_node = core::NodeId{2};
    one_way.base_lanes = 1;
    one_way.length_m = 80.0F;
    one_way.speed_limit_mps = 11.2F;
    if (!graph.add_edge(one_way, &error))
    {
        std::cerr << "Failed to add one-way edge: " << error << '\n';
        return 1;
    }

    const auto outgoing_before = graph.out_edges();
    const auto incoming_before = graph.in_edges();
    if (outgoing_before.size() != 3 || incoming_before.size() != 3 ||
        outgoing_before[0] != std::vector<core::EdgeId>{core::EdgeId{0}} ||
        outgoing_before[1] != std::vector<core::EdgeId>{core::EdgeId{0}, core::EdgeId{1}} ||
        !outgoing_before[2].empty() ||
        incoming_before[0] != std::vector<core::EdgeId>{core::EdgeId{0}} ||
        incoming_before[1] != std::vector<core::EdgeId>{core::EdgeId{0}} ||
        incoming_before[2] != std::vector<core::EdgeId>{core::EdgeId{1}})
    {
        std::cerr << "Graph adjacency does not match edge directionality.\n";
        return 1;
    }

    sim::Node invalid_node;
    invalid_node.pos_world = {std::numeric_limits<float>::quiet_NaN(), 0.0F};
    if (graph.add_node(invalid_node, &error) || error.empty() || graph.nodes().size() != 3)
    {
        std::cerr << "Graph accepted a node with invalid position or changed on rejection.\n";
        return 1;
    }
    invalid_node = {};
    invalid_node.waiting_cargo_tons = -1.0F;
    if (graph.add_node(invalid_node, &error) || error.empty() || graph.nodes().size() != 3)
    {
        std::cerr << "Graph accepted a node with negative cargo or changed on rejection.\n";
        return 1;
    }
    invalid_node = {};
    invalid_node.intersection_throughput_per_sec = std::numeric_limits<float>::infinity();
    if (graph.add_node(invalid_node, &error) || error.empty() || graph.nodes().size() != 3)
    {
        std::cerr << "Graph accepted a node with non-finite throughput or changed on rejection.\n";
        return 1;
    }

    sim::Edge invalid_edge;
    invalid_edge.from_node = core::NodeId{2};
    invalid_edge.to_node = core::NodeId{3};
    if (graph.add_edge(invalid_edge, &error) || error.empty() || graph.edges().size() != 2)
    {
        std::cerr << "Graph accepted an invalid edge endpoint or changed on rejection.\n";
        return 1;
    }

    sim::Edge invalid_metrics = bidirectional;
    invalid_metrics.id = core::EdgeId{};
    invalid_metrics.length_m = 0.0F;
    if (graph.add_edge(invalid_metrics, &error) || error.empty() || graph.edges().size() != 2)
    {
        std::cerr << "Graph accepted an edge with invalid length or changed on rejection.\n";
        return 1;
    }
    invalid_metrics = bidirectional;
    invalid_metrics.speed_limit_mps = std::numeric_limits<float>::infinity();
    if (graph.add_edge(invalid_metrics, &error) || error.empty() || graph.edges().size() != 2)
    {
        std::cerr << "Graph accepted an edge with non-finite speed or changed on rejection.\n";
        return 1;
    }
    invalid_metrics = bidirectional;
    invalid_metrics.base_lanes = 0;
    if (graph.add_edge(invalid_metrics, &error) || error.empty() || graph.edges().size() != 2)
    {
        std::cerr << "Graph accepted an edge with zero lanes or changed on rejection.\n";
        return 1;
    }
    if (!graph.rebuild_adjacency(&error) || graph.out_edges() != outgoing_before ||
        graph.in_edges() != incoming_before)
    {
        std::cerr << "Rebuilt graph adjacency differs from the original adjacency: " << error << '\n';
        return 1;
    }
    if (graph.find_node(core::NodeId{3}) != nullptr || graph.find_edge(core::EdgeId{2}) != nullptr)
    {
        std::cerr << "Graph lookup returned an out-of-range object.\n";
        return 1;
    }

    return 0;
}
