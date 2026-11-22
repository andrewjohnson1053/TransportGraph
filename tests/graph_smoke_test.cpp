#include "sim/Graph.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
class TemporaryMapDirectory
{
public:
    TemporaryMapDirectory() = default;
    TemporaryMapDirectory(const TemporaryMapDirectory&) = delete;
    TemporaryMapDirectory& operator=(const TemporaryMapDirectory&) = delete;

    ~TemporaryMapDirectory()
    {
        if (owns_directory_)
        {
            std::error_code ignored;
            std::filesystem::remove_all(path_, ignored);
        }
    }

    bool create(std::string& error)
    {
        std::error_code filesystem_error;
        const std::filesystem::path temp_root = std::filesystem::temp_directory_path(filesystem_error);
        if (filesystem_error)
        {
            error = "Failed to locate the system temporary directory: " + filesystem_error.message();
            return false;
        }

        static std::atomic<std::uint64_t> sequence{0};
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 128; ++attempt)
        {
            const std::string name = "transport_graph_map_loader_test_" + std::to_string(timestamp) + "_" +
                std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
            const std::filesystem::path candidate = temp_root / name;
            if (std::filesystem::create_directory(candidate, filesystem_error))
            {
                path_ = candidate;
                owns_directory_ = true;
                return true;
            }
            if (filesystem_error && filesystem_error != std::errc::file_exists)
            {
                error = "Failed to create a unique temporary map directory: " + filesystem_error.message();
                return false;
            }
            filesystem_error.clear();
        }

        error = "Unable to create a unique temporary map directory after 128 attempts.";
        return false;
    }

    const std::filesystem::path& path() const noexcept { return path_; }

    bool cleanup(std::string& error)
    {
        if (!owns_directory_)
        {
            return true;
        }
        std::error_code filesystem_error;
        std::filesystem::remove_all(path_, filesystem_error);
        if (filesystem_error)
        {
            error = "Failed to clean up temporary map directory: " + filesystem_error.message();
            return false;
        }
        owns_directory_ = false;
        return true;
    }

private:
    std::filesystem::path path_;
    bool owns_directory_ = false;
};

bool write_map_fixture(const std::filesystem::path& path, const nlohmann::json& document)
{
    std::ofstream output{path};
    if (!output)
    {
        return false;
    }
    output << document.dump(2);
    return output.good();
}

bool rejects_map(sim::Graph& graph, const std::filesystem::path& path,
                 const nlohmann::json& document, const char* expected_error)
{
    if (!write_map_fixture(path, document))
    {
        std::cerr << "Failed to write map fixture.\n";
        return false;
    }

    const auto node_count = graph.nodes().size();
    const auto edge_count = graph.edges().size();
    const auto outgoing = graph.out_edges();
    const auto incoming = graph.in_edges();
    std::string error;
    if (graph.load_from_json(path, &error) || error.find(expected_error) == std::string::npos ||
        graph.nodes().size() != node_count || graph.edges().size() != edge_count ||
        graph.out_edges() != outgoing || graph.in_edges() != incoming)
    {
        std::cerr << "Invalid map was not rejected cleanly (expected: " << expected_error
                  << ", got: " << error << ").\n";
        return false;
    }
    return true;
}

nlohmann::json valid_map_fixture()
{
    return {
        {"nodes", {
            {{"id", 0}, {"x", 1.5}, {"y", -2.0}, {"type", "Intersection"},
             {"waiting_passengers", 4}, {"waiting_cargo_tons", 2.5},
             {"intersection_throughput_per_sec", 12.0}},
            {{"id", 1}, {"x", 101.5}, {"y", -2.0}, {"type", "Terminal"}}
        }},
        {"edges", {
            {{"id", 0}, {"from_node", 0}, {"to_node", 1}, {"bidirectional", true},
             {"road_class", "TwoLane"}, {"base_lanes", 2}, {"length_m", 100.0},
             {"speed_limit_mps", 13.4}, {"active_vehicles", 3}, {"congestion_ratio", 0.4},
             {"effective_speed_mps", 8.0}, {"has_pullout_bays", true}, {"has_bus_lane", true},
             {"has_signal_priority", false}, {"roadwork_active", true}, {"roadwork_speed_mult", 0.5}}
        }}
    };
}
}

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

    TemporaryMapDirectory temp_directory;
    std::string error;
    if (!temp_directory.create(error))
    {
        std::cerr << error << '\n';
        return 1;
    }
    const std::filesystem::path fixture_path = temp_directory.path() / "map.json";
    sim::Graph loaded_graph;
    const nlohmann::json valid_document = valid_map_fixture();
    if (!write_map_fixture(fixture_path, valid_document) ||
        !loaded_graph.load_from_json(fixture_path, &error))
    {
        std::cerr << "Failed to load valid graph map: " << error << '\n';
        return 1;
    }
    if (loaded_graph.nodes().size() != 2 || loaded_graph.edges().size() != 1 ||
        loaded_graph.nodes()[0].id != core::NodeId{0} || loaded_graph.nodes()[1].id != core::NodeId{1} ||
        loaded_graph.edges()[0].id != core::EdgeId{0} ||
        loaded_graph.nodes()[0].waiting_passengers != 4 ||
        loaded_graph.nodes()[0].waiting_cargo_tons != 2.5F ||
        loaded_graph.nodes()[1].type != core::NodeType::Terminal ||
        loaded_graph.edges()[0].road_class != core::RoadClass::TwoLane ||
        loaded_graph.edges()[0].active_vehicles != 3 ||
        !loaded_graph.edges()[0].has_pullout_bays || !loaded_graph.edges()[0].has_bus_lane ||
        !loaded_graph.edges()[0].roadwork_active ||
        loaded_graph.out_edges()[0] != std::vector<core::EdgeId>{core::EdgeId{0}} ||
        loaded_graph.out_edges()[1] != std::vector<core::EdgeId>{core::EdgeId{0}} ||
        loaded_graph.in_edges()[0] != std::vector<core::EdgeId>{core::EdgeId{0}} ||
        loaded_graph.in_edges()[1] != std::vector<core::EdgeId>{core::EdgeId{0}})
    {
        std::cerr << "Loaded graph fields or bidirectional adjacency do not match the map.\n";
        return 1;
    }

    nlohmann::json invalid_document = valid_document;
    invalid_document.erase("nodes");
    if (!rejects_map(loaded_graph, fixture_path, invalid_document, "missing required field 'nodes'")) return 1;
    invalid_document = valid_document;
    invalid_document.erase("edges");
    if (!rejects_map(loaded_graph, fixture_path, invalid_document, "missing required field 'edges'")) return 1;
    invalid_document = valid_document;
    invalid_document["nodes"] = nlohmann::json::array();
    if (!rejects_map(loaded_graph, fixture_path, invalid_document, "non-empty array")) return 1;
    invalid_document = valid_document;
    invalid_document["edges"] = nlohmann::json::array();
    if (!rejects_map(loaded_graph, fixture_path, invalid_document, "non-empty array")) return 1;
    invalid_document = valid_document;
    invalid_document["nodes"][1]["id"] = 0;
    if (!rejects_map(loaded_graph, fixture_path, invalid_document, "duplicate node ID")) return 1;
    invalid_document = valid_document;
    invalid_document["edges"][0]["id"] = 1;
    if (!rejects_map(loaded_graph, fixture_path, invalid_document, "ID must match its array index")) return 1;
    invalid_document = valid_document;
    invalid_document["edges"][0]["to_node"] = 2;
    if (!rejects_map(loaded_graph, fixture_path, invalid_document, "endpoint node ID is out of range")) return 1;
    invalid_document = valid_document;
    invalid_document["edges"][0].erase("to_node");
    if (!rejects_map(loaded_graph, fixture_path, invalid_document, "missing required field 'to_node'")) return 1;
    invalid_document = valid_document;
    invalid_document["edges"][0]["length_m"] = 0.0;
    if (!rejects_map(loaded_graph, fixture_path, invalid_document, "length and speed must be finite and positive")) return 1;
    invalid_document = valid_document;
    invalid_document["edges"][0]["speed_limit_mps"] = -1.0;
    if (!rejects_map(loaded_graph, fixture_path, invalid_document, "length and speed must be finite and positive")) return 1;
    invalid_document = valid_document;
    invalid_document["edges"][0]["base_lanes"] = 0;
    if (!rejects_map(loaded_graph, fixture_path, invalid_document, "lane count must be positive")) return 1;
    invalid_document = valid_document;
    invalid_document["edges"][0]["road_class"] = "Expressway";
    if (!rejects_map(loaded_graph, fixture_path, invalid_document, "invalid road class")) return 1;
    invalid_document = valid_document;
    invalid_document["nodes"][0]["type"] = "Unknown";
    if (!rejects_map(loaded_graph, fixture_path, invalid_document, "invalid node type")) return 1;
    invalid_document = valid_document;
    invalid_document["nodes"][0]["x"] = nullptr;
    if (!rejects_map(loaded_graph, fixture_path, invalid_document, "must be numeric")) return 1;
    {
        std::ofstream non_finite_position{fixture_path};
        non_finite_position << R"({"nodes":[{"id":0,"x":1e999,"y":0,"type":"Intersection"}],"edges":[{"id":0,"from_node":0,"to_node":0,"bidirectional":false,"road_class":"SingleLane","base_lanes":1,"length_m":1,"speed_limit_mps":1}]})";
    }
    if (loaded_graph.load_from_json(fixture_path, &error) || error.empty() ||
        loaded_graph.nodes().size() != 2 || loaded_graph.edges().size() != 1)
    {
        std::cerr << "Non-finite map coordinates were not rejected transactionally.\n";
        return 1;
    }
    invalid_document = valid_document;
    invalid_document["nodes"][0]["waiting_cargo_tons"] = -1.0;
    if (!rejects_map(loaded_graph, fixture_path, invalid_document, "cargo/throughput values")) return 1;

    {
        std::ofstream malformed_json{fixture_path};
        malformed_json << "{invalid json";
    }
    if (loaded_graph.load_from_json(fixture_path, &error) || error.find("Invalid JSON") == std::string::npos ||
        loaded_graph.nodes().size() != 2 || loaded_graph.edges().size() != 1)
    {
        std::cerr << "Malformed JSON was not rejected without changing the existing graph: " << error << '\n';
        return 1;
    }
    if (loaded_graph.load_from_json(temp_directory.path() / "missing.json", &error) || error.empty())
    {
        std::cerr << "Missing map file was not reported.\n";
        return 1;
    }
    if (!temp_directory.cleanup(error))
    {
        std::cerr << error << '\n';
        return 1;
    }

    sim::Graph graph;
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
