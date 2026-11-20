#include "core/Camera2D.h"
#include "core/Types.h"
#include "sim/SimLoop.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <type_traits>

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

bool near(float actual, float expected, float tolerance = 1.0e-4f)
{
	return std::abs(actual - expected) <= tolerance;
}

bool test_types()
{
	static_assert(sizeof(core::NodeId) == sizeof(std::uint32_t));
	static_assert(sizeof(core::EdgeId) == sizeof(std::uint32_t));
	static_assert(sizeof(core::VehicleId) == sizeof(std::uint32_t));
	static_assert(sizeof(core::RouteId) == sizeof(std::uint32_t));
	static_assert(sizeof(core::DepotId) == sizeof(std::uint32_t));
	static_assert(sizeof(core::WorkerId) == sizeof(std::uint32_t));
	static_assert(sizeof(core::ContractId) == sizeof(std::uint32_t));
	static_assert(sizeof(core::CompanyId) == sizeof(std::uint32_t));
	static_assert(!std::is_same_v<core::NodeId, core::EdgeId>);
	static_assert(!std::is_same_v<core::VehicleId, core::RouteId>);
	static_assert(!std::is_convertible_v<core::NodeId, core::EdgeId>);
	static_assert(!std::is_convertible_v<core::EdgeId, core::NodeId>);
	static_assert(!std::is_assignable_v<core::NodeId&, core::EdgeId>);
	static_assert(std::is_constructible_v<core::NodeId, std::uint32_t>);
	static_assert(!std::is_convertible_v<std::uint32_t, core::NodeId>);

	const core::NodeId invalid_node;
	const core::NodeId node{7};
	return check(core::InvalidId == std::numeric_limits<std::uint32_t>::max(), "InvalidId uses the uint32 maximum") &&
		   check(!invalid_node.is_valid(), "default ID represents an invalid pool index") &&
		   check(node.is_valid() && node.value() == 7, "ID explicitly stores and exposes its pool index");
}

bool test_sim_loop()
{
	sim::SimLoop loop;
	bool passed = check(loop.advance(sim::SimLoop::fixed_dt * 3.0) == 3, "fixed timestep emits the expected count") &&
				  check(loop.last_step_count() == 3 && loop.total_step_count() == 3, "step counters are observable");

	sim::SimLoop clamped;
	passed = check(clamped.advance(1.0) == 15, "frame delta is clamped to 0.25 seconds") && passed;
	passed = check(clamped.advance(std::numeric_limits<double>::quiet_NaN()) == 0, "NaN frame delta is ignored") && passed;
	passed = check(clamped.advance(std::numeric_limits<double>::infinity()) == 0, "infinite frame delta is ignored") && passed;
	passed = check(clamped.advance(-1.0) == 0, "negative frame delta is ignored") && passed;

	sim::SimLoop sped_up;
	passed = check(sped_up.set_speed(core::SimulationSpeed::X2), "2x speed is accepted") && passed;
	passed = check(sped_up.advance(sim::SimLoop::fixed_dt * 0.5) == 1, "2x speed doubles accumulated simulation time") && passed;
	passed = check(sped_up.set_speed(core::SimulationSpeed::X4), "4x speed is accepted") && passed;
	sped_up.reset_accumulator();
	passed = check(sped_up.advance(sim::SimLoop::max_frame_dt) == sim::SimLoop::max_steps_per_frame,
				   "4x speed is bounded to 60 steps per frame") && passed;

	sim::SimLoop paused;
	passed = check(paused.advance(sim::SimLoop::fixed_dt * 0.5) == 0, "partial timestep accumulates") && passed;
	paused.set_paused(true);
	passed = check(paused.advance(sim::SimLoop::fixed_dt * 2.0) == 0, "pause prevents simulation ticks") && passed;
	paused.set_paused(false);
	passed = check(paused.advance(sim::SimLoop::fixed_dt * 0.5) == 0, "pause clears stale accumulated time") && passed;
	passed = check(paused.advance(sim::SimLoop::fixed_dt * 0.5) == 1, "simulation resumes from a clean accumulator") && passed;
	passed = check(paused.set_speed(core::SimulationSpeed::X0), "0x speed is accepted") && passed;
	passed = check(paused.advance(sim::SimLoop::fixed_dt * 4.0) == 0, "0x speed prevents simulation ticks") && passed;
	passed = check(!paused.set_speed(static_cast<core::SimulationSpeed>(3)), "unsupported speed is rejected") && passed;
	return passed;
}

bool test_camera()
{
	core::Camera2D camera;
	bool passed = check(camera.set_viewport_size({800.0f, 600.0f}), "valid viewport size is accepted");
	passed = check(camera.set_center_world({12.0f, -7.0f}), "finite camera center is accepted") && passed;
	passed = check(camera.set_zoom(2.5f), "valid zoom is accepted") && passed;

	const glm::vec2 world{24.5f, 16.25f};
	const glm::vec2 round_trip = camera.screen_to_world(camera.world_to_screen(world));
	passed = check(near(round_trip.x, world.x) && near(round_trip.y, world.y), "camera transforms round-trip") && passed;

	const glm::vec2 cursor{137.0f, 221.0f};
	const glm::vec2 anchored_before = camera.screen_to_world(cursor);
	passed = check(camera.zoom_at_cursor(cursor, 1.7f), "cursor-centered zoom succeeds") && passed;
	const glm::vec2 anchored_after = camera.screen_to_world(cursor);
	passed = check(near(anchored_before.x, anchored_after.x) && near(anchored_before.y, anchored_after.y),
				   "cursor-centered zoom preserves the world point under the cursor") && passed;

	passed = check(camera.set_zoom_bounds(0.5f, 2.0f), "valid zoom bounds are accepted") && passed;
	passed = check(camera.zoom() == 2.0f, "changing zoom bounds clamps the current zoom") && passed;
	passed = check(!camera.set_zoom(std::numeric_limits<float>::quiet_NaN()), "NaN zoom is rejected") && passed;
	passed = check(!camera.set_viewport_size({0.0f, 600.0f}), "zero viewport dimension is rejected") && passed;
	passed = check(!camera.set_center_world({std::numeric_limits<float>::infinity(), 0.0f}),
					 "infinite camera center is rejected") && passed;

	  core::Camera2D overflow_camera;
	  passed = check(overflow_camera.set_viewport_size({800.0f, 600.0f}) && overflow_camera.set_zoom(0.05f),
					 "camera accepts finite zoom inputs for overflow protection") && passed;
	  const float old_zoom = overflow_camera.zoom();
	  const glm::vec2 old_center = overflow_camera.center_world();
	  passed = check(!overflow_camera.zoom_at_cursor({std::numeric_limits<float>::max(), 300.0f}, 2.0f),
					 "cursor zoom rejects an unrepresentable camera center") && passed;
	  passed = check(overflow_camera.zoom() == old_zoom && overflow_camera.center_world() == old_center,
					 "failed cursor zoom leaves camera state unchanged") && passed;

	  const glm::vec2 center_before_drag = camera.center_world();
	  camera.pan_by_screen_delta({20.0f, -10.0f});
	const glm::vec2 center_after_drag = camera.center_world();
	passed = check(near(center_after_drag.x, center_before_drag.x - 10.0f) &&
					   near(center_after_drag.y, center_before_drag.y + 5.0f),
				   "drag panning scales by zoom") && passed;
	camera.pan_by_keyboard({1.0f, 0.0f}, 2.0, 3.0f);
	passed = check(near(camera.center_world().x, center_after_drag.x + 6.0f), "keyboard panning uses elapsed time") && passed;
	camera.pan_by_keyboard({0.0f, 1.0f}, 1.0, std::numeric_limits<float>::infinity());

	const glm::vec2 invalid_world = camera.screen_to_world({std::numeric_limits<float>::quiet_NaN(), 0.0f});
	const glm::vec2 invalid_screen = camera.world_to_screen({std::numeric_limits<float>::infinity(), 0.0f});
	passed = check(std::isfinite(invalid_world.x) && std::isfinite(invalid_world.y) &&
					   std::isfinite(invalid_screen.x) && std::isfinite(invalid_screen.y),
				   "invalid coordinates produce finite transform results") && passed;
	return passed;
}
}

int main()
{
	const bool passed = test_types() && test_sim_loop() && test_camera();
	if (passed)
	{
		std::cout << "Core primitive tests passed.\n";
		return 0;
	}
	return 1;
}
