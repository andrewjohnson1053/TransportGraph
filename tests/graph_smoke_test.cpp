#include "sim/Graph.h"

#include <filesystem>
#include <iostream>
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
    if (!graph.add_node({}, &error) || !graph.add_node({}, &error) || !graph.add_node({}, &error))
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
    if (!graph.add_edge(bidirectional, &error))
    {
        std::cerr << "Failed to add bidirectional edge: " << error << '\n';
        return 1;
    }

    sim::Edge one_way;
    one_way.from_node = core::NodeId{1};
    one_way.to_node = core::NodeId{2};
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

    sim::Edge invalid_edge;
    invalid_edge.from_node = core::NodeId{2};
    invalid_edge.to_node = core::NodeId{3};
    if (graph.add_edge(invalid_edge, &error) || error.empty() || graph.edges().size() != 2)
    {
        std::cerr << "Graph accepted an invalid edge endpoint or changed on rejection.\n";
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
