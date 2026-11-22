# Technical & Functional Specifications (`SPECIFICATIONS.md`)

## 1. Engine & Module Architecture
The project is structured in C++20 with strict separation between simulation, graph routing, vector rendering, UI, and persistence:

* **`src/core/` (`core::`)**:
  * Fixed-timestep accumulator loop (`SimLoop`), application lifecycle (`App`), 2D orthographic pan/zoom camera (`Camera2D`), coordinate transforms (`screen_to_world`, `world_to_screen`), hit-testing, and shared POD types/IDs (`Types.h`).
* **`src/sim/` (`sim::`)**:
  * Data-oriented contiguous pools (`std::vector`) for `Node`, `Edge`, `Vehicle`, `Route`, `Depot`, `Worker`, `Contract`, and `Company`.
  * Dynamic congestion, overtaking/queueing, passenger/cargo demand & route choice, wear/fuel/charging, workforce shifts/fatigue, AI archetypes, anti-monopoly/startups/buyouts, 12-tier progression, and municipal lobbying.
* **`src/graph/` or `src/sim/Graph.*` (`graph::`)**:
  * Flat graph storage, CSR/flat adjacency lists, dynamic edge travel-time weight calculation, `A*` / Dijkstra pathfinding, and path cache invalidation.
* **`src/gfx/` (`gfx::`)**:
  * Batched 2D vector rendering via SDL3 / GPU/shader pipeline: variable-width road edges, road-class palette, traffic saturation heatmap, node markers & queue badges, vehicle icons, and competitor highlight overlays.
* **`src/ui/` (`ui::`)**:
  * `Dear ImGui` HUD, Corridor & Line Inspector (market share, demand, satisfaction), Route & Timetable Editor, Fleet/Depot/Workforce management, Contract Board, Lobbying panel, and performance/debug counters.

## 2. Core Data Models & Invariants

### 2.1 Graph Primitives
* **`Node`:**
  * `uint32_t id` (index in `nodes` pool), `glm::vec2 pos_world`, `NodeType` (`Intersection`, `Stop`, `Terminal`, `DepotAnchor`, `RegionalGateway`).
  * `uint32_t waiting_passengers`, `float waiting_cargo_tons`, `float intersection_throughput_per_sec`.
  * *Invariant:* `pos_world` is finite (`!isnan`, `!isinf`); `id == index`.
* **`Edge` (Pre-Existing Road Segment):**
  * `uint32_t id`, `uint32_t from_node`, `uint32_t to_node`, `bool bidirectional`.
  * `RoadClass` (`SingleLane`, `TwoLane`, `MultiLane`, `Highway`), `uint8_t base_lanes`, `float length_m`, `float speed_limit_mps`.
  * Dynamic state: `uint32_t active_vehicles`, `float congestion_ratio`, `float effective_speed_mps`, `bool has_pullout_bays`, `bool has_bus_lane`, `bool has_signal_priority`, `bool roadwork_active`, `float roadwork_speed_mult`.
  * *Invariant:* `from_node < nodes.size()`, `to_node < nodes.size()`, `length_m > 0.0f`, `speed_limit_mps > 0.0f`, `base_lanes >= 1`.

### 2.2 JSON Map Format
Map files contain non-empty `nodes` and `edges` arrays. Node and edge `id` values are unsigned integers matching their array indices. Each node requires `id`, finite numeric `x` and `y`, and a `type` string (`Intersection`, `Stop`, `Terminal`, `DepotAnchor`, or `RegionalGateway`). Optional node fields are `waiting_passengers`, `waiting_cargo_tons`, and `intersection_throughput_per_sec`; numeric values must be finite and non-negative.

Each edge requires `id`, `from_node`, `to_node`, `bidirectional`, `road_class` (`SingleLane`, `TwoLane`, `MultiLane`, or `Highway`), `base_lanes`, positive `length_m`, and positive `speed_limit_mps`. Optional fields are `active_vehicles`, `congestion_ratio`, `effective_speed_mps`, `has_pullout_bays`, `has_bus_lane`, `has_signal_priority`, `roadwork_active`, and positive `roadwork_speed_mult`. Endpoints must reference existing node IDs; dynamic numeric values must be finite and non-negative. Omitted optional fields use the defaults in the graph data structures.

### 2.3 Fleet, Depots & Workforce
* **`Vehicle`:**
  * `uint32_t id`, `uint32_t company_id`, `uint32_t route_id`, `uint32_t home_depot_id`.
  * `VehicleClass` (`MiniVan`, `CityBus`, `DoubleDecker`, `ArticulatedBus`, `LightTruck`, `MediumTruck`, `SemiTruck`).
  * `Powertrain` (`ICE`, `Electric`), `uint32_t passenger_cap`, `float cargo_cap_tons`, `float max_speed_mps`.
  * Runtime state: `uint32_t current_edge_id`, `float edge_dist_m`, `float current_speed_mps`, `float energy_pct` (`0..100`), `float wear_pct` (`0..100`), `uint32_t onboard_passengers`, `float onboard_cargo_tons`, `VehicleState` (`EnRoute`, `Dwelling`, `Queued`, `ToDepot`, `Servicing`, `BrokenDown`).
* **`Depot`:**
  * `uint32_t id`, `uint32_t company_id`, `uint32_t node_id`, `uint32_t parking_slots`, `uint32_t service_bays`, `uint32_t ev_chargers`, `uint32_t fuel_pumps`.
* **`Worker`:**
  * `uint32_t id`, `uint32_t company_id`, `uint32_t depot_id`, `WorkerRole` (`Driver`, `Mechanic`, `Dispatcher`), `float hourly_wage`, `float shift_hours_worked`, `float fatigue_0_to_100`, `bool on_duty`.

### 2.4 Routes, Contracts & Companies
* **`Route`:**
  * `uint32_t id`, `uint32_t company_id`, `RouteKind` (`PrivatePassenger`, `ContractPassenger`, `Cargo`), `std::vector<uint32_t> node_sequence`, `std::vector<uint32_t> edge_sequence`.
  * `float ticket_fare`, `float target_headway_sec`, `float time_since_last_dispatch_sec`, `bool active`.
  * Telemetry: `uint32_t riders_carried`, `float tons_carried`, `float avg_satisfaction`, `double period_revenue`, `double period_cost`.
* **`Contract`:**
  * `uint32_t id`, `ContractKind` (`CorporateShuttle`, `MunicipalTransit`, `CargoFreight`, `ColdChain`), `uint8_t min_tier`, `float min_reputation`, `double payout_per_period`, `float sla_target_pct`, `float current_sla_pct`, `uint32_t assigned_company_id`.
* **`Company`:**
  * `uint32_t id`, `bool is_player`, `AIArchetype` (`None`, `LowCostOperator`, `RegionalGiant`, `GreenPioneer`, `LogisticsHeavyweight`, `SuburbanExpress`).
  * `double cash`, `double net_worth`, `float reputation` (`0..100`), `uint8_t tier` (`1..12`), `float regional_market_share` (`0..1`), `float anti_monopoly_tax_rate`.

## 3. Simulation Formulas & Rules
* **Edge Travel Time:**
  * Base speed is `min(vehicle_max_speed_mps, edge.speed_limit_mps)`.
  * Congestion does not slow an edge through saturation `0.7`; above that, use `max(0.15, 1 / (1 + 2 * (s - 0.7)^2))` as a speed multiplier.
  * When roadwork is active, multiply speed by `roadwork_speed_mult`. A bus lane or signal priority independently multiplies travel time by `0.9`.
  * Invalid edge metrics or non-positive/non-finite vehicle speed return positive infinity, indicating an unusable edge weight.
* **Congestion Saturation & Speed:**
  * Define edge capacity `cap` from `base_lanes` and `RoadClass` (plus `has_bus_lane` / `roadwork_speed_mult`).
  * Saturation `s = static_cast<float>(active_vehicles) / std::max(1.0f, cap)`.
  * When `s <= 0.7f`, speed stays near `min(vehicle.max_speed_mps, edge.speed_limit_mps) * edge.roadwork_speed_mult`. Above `s > 0.7f`, apply a smooth non-linear slowdown curve down to a minimum crawl floor so deadlocks cannot freeze time.
* **Overtaking & Queueing:**
  * On `MultiLane` and `Highway` edges (when not blocked by active roadworks), a vehicle moving faster than a vehicle ahead with lower relative saturation may overtake, updating its relative position/lane state rather than getting permanently trapped behind a slow/stopping vehicle.
  * On `SingleLane` edges, vehicles follow in order and queue behind stopped, loading, or broken-down vehicles unless `has_pullout_bays` is true at a stop.
* **Passenger & Cargo Choice:**
  * Passenger utility scores candidate routes/modes using generalized cost: weighted combination of `ticket_fare`, expected `wait_time`, `in_vehicle_time`, and comfort/vehicle-age penalty (`1.0 - comfort`).
* **Anti-Monopoly, Startups & Buyouts:**
  * If a company's market share exceeds `0.75f` (75%), progressive regulatory friction/tax applies and competitor spawn probability increases.
  * Late-game tiers unlock depot/vehicle/route asset purchases and selective company buyouts, while periodic new AI entrants prevent permanent market stagnation.
* **12-Tier Progression Gates:**
  * Advancement from Tier `T` to `T + 1` (`1 <= T < 12`) requires satisfying all criteria for that tier across finances, fleet size/types, depots/infrastructure, workforce, and reputation.