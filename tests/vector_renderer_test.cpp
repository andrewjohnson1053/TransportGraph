#include "gfx/VectorRenderer.h"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>

namespace
{
bool check(bool condition, const char* message)
{
	if (!condition)
	{
		std::cerr << "FAILED: " << message << '\n';
	}
	return condition;
}
}

int main()
{
	bool passed = true;
	const gfx::Color single = gfx::road_class_color(core::RoadClass::SingleLane);
	const gfx::Color arterial = gfx::road_class_color(core::RoadClass::TwoLane);
	const gfx::Color avenue = gfx::road_class_color(core::RoadClass::MultiLane);
	const gfx::Color highway = gfx::road_class_color(core::RoadClass::Highway);
	passed = check(single.r != arterial.r && arterial.r != avenue.r && avenue.r != highway.r,
				   "each road class has a distinct default color") && passed;
	passed = check(gfx::road_class_width_px(core::RoadClass::SingleLane) <
					   gfx::road_class_width_px(core::RoadClass::TwoLane) &&
					   gfx::road_class_width_px(core::RoadClass::TwoLane) <
					   gfx::road_class_width_px(core::RoadClass::MultiLane) &&
					   gfx::road_class_width_px(core::RoadClass::MultiLane) <
					   gfx::road_class_width_px(core::RoadClass::Highway),
				   "road widths increase with road class") && passed;

	const gfx::Color low = gfx::heatmap_color(0.0f);
	const gfx::Color midpoint = gfx::heatmap_color(0.7f);
	const gfx::Color full = gfx::heatmap_color(1.0f);
	const gfx::Color over_capacity = gfx::heatmap_color(1.7f);
	passed = check(low.g > low.r && low.g > low.b, "low saturation is green") && passed;
	passed = check(midpoint.r > midpoint.b && midpoint.g > midpoint.b,
				   "0.7 saturation is yellow") && passed;
	passed = check(full.r > full.g && full.r > full.b, "full saturation is red") && passed;
	passed = check(over_capacity.r == full.r && over_capacity.g == full.g && over_capacity.b == full.b,
				   "saturation above capacity clamps to the red maximum") && passed;
	passed = check(gfx::heatmap_color(std::numeric_limits<float>::quiet_NaN()).r == 255,
				   "invalid saturation is visible as an error color") && passed;

	const core::NodeId from_id{0};
	const core::NodeId to_id{1};
	const core::EdgeId edge_id{0};
	const std::array<gfx::RenderNode, 2> nodes{{
		gfx::RenderNode{from_id, {0.0f, 0.0f}, core::NodeType::Intersection, 0},
		gfx::RenderNode{to_id, {100.0f, 50.0f}, core::NodeType::Stop, 12}
	}};
	const std::array<gfx::RenderEdge, 1> edges{{
		gfx::RenderEdge{edge_id, from_id, to_id, core::RoadClass::TwoLane, 2, 200.0f, 0.4f}
	}};
	const gfx::RenderScene scene{nodes, edges, {}, {}};
	const gfx::RenderVehicle halfway{core::VehicleId{0}, core::CompanyId{1}, edge_id, 100.0f};
	const auto position = gfx::vehicle_world_position(scene, halfway);
	passed = check(position && std::abs(position->x - 50.0f) < 1.0e-5f &&
					   std::abs(position->y - 25.0f) < 1.0e-5f,
				   "vehicle world position interpolates by edge distance") && passed;

	const gfx::RenderVehicle before_edge{core::VehicleId{1}, core::CompanyId{1}, edge_id, -10.0f};
	const auto start_position = gfx::vehicle_world_position(scene, before_edge);
	passed = check(start_position && *start_position == nodes[0].position_world,
				   "vehicle distance clamps to the start of its edge") && passed;
	const gfx::RenderVehicle past_edge{core::VehicleId{2}, core::CompanyId{1}, edge_id, 250.0f};
	const auto end_position = gfx::vehicle_world_position(scene, past_edge);
	passed = check(end_position && *end_position == nodes[1].position_world,
				   "vehicle distance clamps to the end of its edge") && passed;
	const gfx::RenderVehicle invalid_edge{core::VehicleId{3}, core::CompanyId{1}, core::EdgeId{9}, 10.0f};
	passed = check(!gfx::vehicle_world_position(scene, invalid_edge), "missing vehicle edge is rejected") && passed;

	const gfx::Color company_a = gfx::company_color(core::CompanyId{2});
	const gfx::Color company_b = gfx::company_color(core::CompanyId{3});
	passed = check(company_a.r != company_b.r || company_a.g != company_b.g || company_a.b != company_b.b,
				   "different companies receive distinct highlight colors") && passed;

	if (passed)
	{
		std::cout << "Vector renderer tests passed.\n";
		return 0;
	}
	return 1;
}
