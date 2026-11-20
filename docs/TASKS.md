# Detailed Implementation Tasks & Subtasks (`TASKS.md`)

> **Agent Handoff Note:** Each subtask below is scoped to be small, testable, and verifiable. Complete one subtask at a time, verify compilation (`0` warnings) and tests, and update `[ ]` to `[x]` as each item is completed.

---

## Milestone 1: Project Bootstrap, Fixed Loop & Vector Renderer

### Task 1.1: Build System, Dependencies & Directory Wiring
* [ ] **Subtask 1.1.1:** Verify `CMakeLists.txt`, `CMakePresets.json`, and `vcpkg.json` configure cleanly in Visual Studio Community (`windows-x64-debug` and `windows-x64-release`) with `SDL3`, `glm`, `nlohmann_json`, and `imgui`.
* [ ] **Subtask 1.1.2:** Enforce C++20 and strict warning flags (`/W4 /permissive-` on MSVC; `-Wall -Wextra -Wpedantic` on GCC/Clang) across all targets.
* [ ] **Subtask 1.1.3:** Configure post-build asset copying so `assets/` is always available next to the built executable.
* [x] **Subtask 1.1.4:** Create `src/core/Types.h` with core ID aliases (`NodeId`, `EdgeId`, `VehicleId`, `RouteId`, `DepotId`, `WorkerId`, `ContractId`, `CompanyId`), `InvalidId` sentinel (`UINT32_MAX`), and fundamental enums (`RoadClass`, `NodeType`, `VehicleClass`, `RouteKind`, `AIArchetype`).

### Task 1.2: Fixed-Timestep Simulation Loop & 2D Camera
* [x] **Subtask 1.2.1:** Implement `SimLoop` (`src/sim/SimLoop.h`) with fixed step `fixed_dt` (e.g., `1.0 / 60.0`), `max_frame_dt` clamp (e.g., `0.25s`) to prevent spiral-of-death, pause flag, and simulation speed multiplier (`0x`, `1x`, `2x`, `4x`).
* [x] **Subtask 1.2.2:** Implement `Camera2D` (`src/core/Camera2D.h/.cpp`) storing `glm::vec2 center_world`, `float zoom`, `float min_zoom`, `float max_zoom`, and `glm::vec2 viewport_size_px`.
* [x] **Subtask 1.2.3:** Add `screen_to_world(vec2 screen_px)` and `world_to_screen(vec2 world_pos)` transforms, cursor-anchored zoom around the mouse pointer, and middle/right-drag + keyboard panning.
* [x] **Subtask 1.2.4:** Implement `find_nearest_node` and `find_nearest_edge` world-space hit-testing with zoom-aware pick radius for selection and route building.
* [x] **Subtask 1.2.5:** Wire `App` (`src/core/App.h/.cpp`) and `src/main.cpp` to initialize SDL3 window/renderer, Dear ImGui SDL3 bindings, input polling, fixed sim ticks, and rendering, shutting down cleanly without leaks.

### Task 1.3: Schematic Vector Graph & Vehicle Renderer
* [ ] **Subtask 1.3.1:** Implement batched line/quad drawing utilities in `VectorRenderer` (`src/gfx/VectorRenderer.h/.cpp`) applying world-to-screen transforms and zoom-scaled thicknesses.
* [ ] **Subtask 1.3.2:** Render road edges in Default Mode using distinct colors and widths for `SingleLane` (thin light gray), `TwoLane` (medium gray), `MultiLane` (blue), and `Highway` (thick orange).
* [ ] **Subtask 1.3.3:** Implement Traffic Saturation Heatmap mode (`bool show_heatmap`) interpolating edge colors from green (`0.0`) through yellow (`0.7`) to red (`>= 1.0+` congestion).
* [ ] **Subtask 1.3.4:** Render nodes as crisp schematic markers (`Intersection`, `Stop`, `Terminal`, `DepotAnchor`, `RegionalGateway`) with hover/selection rings and clean numeric queue badges (no per-node pie charts).
* [ ] **Subtask 1.3.5:** Render vehicles along edges from their `(current_edge_id, edge_dist_m)` coordinates and add Competitor Focus Mode (`selected_rival_id`) highlighting a chosen rival's lines and vehicles without changing base road-class colors.

---

## Milestone 2: Road Graph, Pathfinding & Traffic Simulation

### Task 2.1: Contiguous Graph Storage & JSON Map Loader
* [ ] **Subtask 2.1.1:** Implement `Graph` (`src/sim/Graph.h/.cpp`) storing flat `std::vector<Node>` and `std::vector<Edge>` pools and precomputed outgoing/incoming edge index lists (`out_edges`, `in_edges`) per node.
* [ ] **Subtask 2.1.2:** Write `load_from_json(const std::filesystem::path& path)` in `Graph` parsing nodes and edges, validating node indices, lengths, and lane/speed fields, and returning clear error messages on invalid maps.
* [ ] **Subtask 2.1.3:** Author a rich `assets/maps/sample_city.json` containing single-lane streets, 2-lane arterials, multi-lane avenues, highway segments, stops, terminals, and depot anchors.
* [ ] **Subtask 2.1.4:** Add unit/smoke test coverage in `tests/graph_smoke_test.cpp` verifying graph loading, adjacency integrity, and invalid-map rejection.

### Task 2.2: Dynamic Edge Weights & Pathfinding
* [ ] **Subtask 2.2.1:** Implement `compute_edge_travel_time(const Edge& e, float vehicle_max_speed_mps)` factoring in `length_m`, `speed_limit_mps`, `congestion_ratio`, `has_bus_lane`, `has_signal_priority`, and `roadwork_speed_mult`.
* [ ] **Subtask 2.2.2:** Implement `find_shortest_path(NodeId start, NodeId goal, float vehicle_max_speed_mps)` using `A*` (with Euclidean heuristic scaled by max speed) returning ordered `std::vector<NodeId>` and `std::vector<EdgeId>`.
* [ ] **Subtask 2.2.3:** Add path cache keys/versioning so cached routes invalidate when edge congestion crosses thresholds or city roadworks toggle.

### Task 2.3: Congestion, Overtaking, Stop Bays & City Roadworks
* [ ] **Subtask 2.3.1:** Recalculate `Edge::active_vehicles` and `Edge::congestion_ratio` cleanly each tick as vehicles enter and leave edges.
* [ ] **Subtask 2.3.2:** Implement the non-linear congestion speed curve (`s <= 0.7` smooth flow; `s > 0.7` progressive slowdown down to minimum speed floor).
* [ ] **Subtask 2.3.3:** Implement overtaking logic on `MultiLane` and `Highway` edges when a trailing vehicle can safely pass a slower vehicle ahead, while keeping `SingleLane` edges strictly queued/following.
* [ ] **Subtask 2.3.4:** Model stop dwell times, dedicated pull-out bay bypass vs. standard roadside stop delay, and scheduled/random city AI roadwork events.

---

## Milestone 3: Fleet, Workforce, Depots & Dual Logistics

### Task 3.1: Vehicle Classes, Wear, Energy & Depots
* [ ] **Subtask 3.1.1:** Define catalog stats for `MiniVan`, `CityBus`, `DoubleDecker`, `ArticulatedBus`, `LightTruck`, `MediumTruck`, and `SemiTruck` (purchase price, capacity, top speed, fuel/kWh rate, comfort, maintenance cost).
* [ ] **Subtask 3.1.2:** Update vehicle `energy_pct` and `wear_pct` during simulation ticks; trigger breakdown states when `wear_pct` reaches critical thresholds or random wear checks fail.
* [ ] **Subtask 3.1.3:** Implement `Depot` ownership, parking slot limits, refueling/EV charging queues, and mechanic repair bays that return serviced/repaired vehicles to active duty.

### Task 3.2: Workforce, Shifts & Fatigue
* [ ] **Subtask 3.2.1:** Implement `Worker` pools (`Driver`, `Mechanic`, `Dispatcher`) attached to depots with wages, shift timers, and `fatigue_0_to_100`.
* [ ] **Subtask 3.2.2:** Require available drivers before dispatching routes/vehicles and apply fatigue penalties (reduced efficiency/speed or elevated incident risk) when fatigued.

### Task 3.3: Passenger & Cargo Demand + Corridor Inspector UI
* [ ] **Subtask 3.3.1:** Implement origin-destination passenger trip generation and cargo shipment generation with time-of-day demand curves.
* [ ] **Subtask 3.3.2:** Implement passenger/cargo route & operator selection based on fare, wait time/frequency, travel duration, and comfort/age.
* [ ] **Subtask 3.3.3:** Build `InspectorUI` (`src/ui/InspectorUI.h/.cpp`) displaying Corridor/Line metrics (daily riders/tons, market share breakdown, satisfaction, revenue vs. operating cost) and route creation/editing controls.

---

## Milestone 4: AI Rivals, Contracts, Anti-Monopoly, 12-Tier Progression & Lobbying

### Task 4.1: AI Competitor Archetypes & Market Ecosystem
* [ ] **Subtask 4.1.1:** Implement `Company` records and distinct AI behavior profiles for `LowCostOperator`, `RegionalGiant`, `GreenPioneer`, `LogisticsHeavyweight`, and `SuburbanExpress`.
* [ ] **Subtask 4.1.2:** Implement periodic AI decisions for opening/tuning routes, adjusting ticket fares, purchasing vehicles/depots, and bidding on contracts.
* [ ] **Subtask 4.1.3:** Implement anti-monopoly tax/regulatory friction when market share exceeds `0.75f`, periodic new AI startup spawns, and late-game asset/depot purchases and company buyouts.

### Task 4.2: Contract Bidding & SLA Enforcement
* [ ] **Subtask 4.2.1:** Define contract templates (`CorporateShuttle`, `MunicipalTransit`, `CargoFreight`, `ColdChain`) with tier/reputation requirements, SLA targets, payouts, and penalties.
* [ ] **Subtask 4.2.2:** Build the Contract Board UI and bid evaluation logic comparing player and AI bids.
* [ ] **Subtask 4.2.3:** Track live SLA compliance per contract and apply period payouts, bonuses, or breach penalties and reputation shifts.

### Task 4.3: 12-Tier Progression, Lobbying, Regional Maps & Persistence
* [ ] **Subtask 4.3.1:** Implement all 12 tiers across Eras I–IV (`T1`–`T12`) with multi-metric advancement checks (cash/net worth, fleet size/types, depots/infrastructure, workforce, reputation) and feature/vehicle/contract unlocks.
* [ ] **Subtask 4.3.2:** Implement lobbying actions (`BusPulloutBays`, `SmartSignalPriority`, `DedicatedBrtLanes`, `MetroRailStudy`) with cost, approval rules, and direct graph/edge/node effects.
* [ ] **Subtask 4.3.3:** Add multi-city regional map support (enabling both city-to-region and region-to-city expansion) and full JSON save/load serialization (`save_game` / `load_game`).