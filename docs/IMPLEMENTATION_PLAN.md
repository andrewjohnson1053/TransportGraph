# Transport Graph - Detailed Implementation Plan

## Working Model

Use one new agent per phase, in sequence. Each agent should own a single phase, read the relevant existing docs and source files, implement only that phase, run verification, update task checkboxes, and finish with a concise handoff.

Do not launch all phase agents in parallel unless their dependencies are already complete. The project has several foundational chains: build and types before application code, graph before routing, routing before vehicles and traffic, and vehicles before fleet and economy. UI work should consume completed model APIs rather than defining parallel duplicate state.

A parent worker or maintainer should remain responsible for:

- reviewing each phase handoff before the next phase starts
- resolving cross-phase API changes
- preventing unrelated changes
- updating `TASKS.md`
- running a final full build/test pass

A single worker is reasonable only for small implementation sessions or if one agent can retain the full project context. For this repository, sequential phase agents are more reliable because they provide narrow scope, verifiable checkpoints, and reduced context overload.

## Rules for Every Phase

Before implementation:

1. Read `docs/CONCEPT.md`, `docs/REQUIREMENTS.md`, `docs/SPECIFICATIONS.md`, and `docs/TASKS.md`.
2. Read the source files, headers, build files, map, and tests relevant to the phase.
3. Inspect existing uncommitted changes and preserve unrelated work.
4. Confirm the phase's dependencies are complete and build successfully.
5. Add or update tests before considering the implementation complete when practical.

During implementation:

- Use C++20 and existing namespaces (`core`, `sim`, `graph`, `gfx`, `ui`).
- Keep entity data in contiguous pools and use stable `uint32_t` IDs.
- Avoid heap allocations inside inner per-tick loops.
- Use fixed-timestep simulation updates.
- Guard against empty routes, disconnected graphs, invalid indices, division by zero, NaN, and infinity.
- Preserve deterministic update order and make seeded behavior deterministic.
- Use existing data structures and public APIs instead of introducing parallel state.
- Use `nlohmann::json` for JSON data and serialization.
- Keep default values centralized and configurable where practical.
- Do not mark `TASKS.md` items complete unless behavior and verification are complete.

For every implementation change:

- Make the smallest complete change.
- Build the relevant target with `/W4 /permissive-` under MSVC or strict GCC/Clang flags.
- Run the smallest relevant test suite, then any smoke tests affected by the change.
- Fix all warnings introduced by the change.
- Preserve existing behavior unrelated to the phase.

When finishing:

- State changed files.
- State tests run and their results.
- State build configurations verified.
- State any unresolved assumptions or follow-up decisions.
- Mark only this phase's completed subtasks in `TASKS.md`.

## Assumptions and Defaults

These assumptions should remain easy to change:

- The first playable scale is one city with later support for regional expansion.
- The sample city is the initial integration map.
- All unspecified numeric gameplay formulas use documented constants or tunable configuration constants.
- Vector geometry and ImGui controls are sufficient for the initial implementation.
- One fixed simulation tick is `1.0 / 60.0` seconds unless an explicit compatibility need requires otherwise.
- Frame delta is clamped to `0.25` seconds.
- The simulation supports pause and `0x`, `1x`, `2x`, and `4x` speeds.
- On high-resolution displays, the game starts in a resizable 1920x1080 window with UI and map visuals scaled to 2x by default.
- A deterministic seed is persisted in simulation state and used for all random decisions.

Unspecified formulas should not be hard-coded into separate implementations. Prefer named constants or a small configuration structure in `Graph`, simulation, economy, or application code.

## Phase 0: Repository and Build Baseline

### Objective

Establish a clean build and test baseline before implementing engine behavior.

### Work

- Verify `CMakeLists.txt`, `CMakePresets.json`, and `vcpkg.json` configure in Visual Studio 2026.
- Verify dependencies: SDL3, GLM, nlohmann-json, and ImGui SDL3/renderer bindings.
- Verify debug and release presets:
  - `windows-x64-debug`
  - `windows-x64-release`
- Verify CMake configures C++20 with C++ extensions disabled.
- Add `/W4 /permissive-` to all MSVC targets.
- Add `-Wall -Wextra -Wpedantic` to GCC/Clang targets.
- Verify asset copying for `assets/` on post-build.
- Decide whether the existing smoke test or an additional test framework is the initial test harness.
- Add test registration with CTest if not already complete.
- Fix empty or placeholder CMake target configuration that prevents clean testing.

### Deliverables

- Clean debug and release builds.
- Passing baseline test target.
- Verified executable asset layout.
- Documented build and test commands in source comments or existing project documentation.

### Tests

- Configure and build with the debug preset.
- Configure and build with the release preset.
- Run CTest.
- Launch the executable and confirm it can initialize without an immediate crash.
- Confirm `assets/maps/sample_city.json` exists beside the executable.

### Completion criteria

- No configure errors.
- No compile warnings attributable to project code.
- All existing tests pass or the agent explicitly documents any pre-existing unrelated failure.
- Asset copying works in a clean build directory.

### Relevant task IDs

- 1.1.1–1.1.3

## Phase 1: Core Types and Application Framework

### Objective

Create the foundational IDs, enums, fixed loop, application lifecycle, and shared camera primitives.

### Work

#### Core types

Create `src/core/Types.h` with:

- `NodeId`, `EdgeId`, `VehicleId`, `RouteId`, `DepotId`, `WorkerId`, `ContractId`, `CompanyId`
- `InvalidId = UINT32_MAX`
- `RoadClass`
- `NodeType`
- `VehicleClass`
- `RouteKind`
- `AIArchetype`
- Additional runtime enums required by the implemented types

Use strong alias types or the smallest existing style that prevents index misuse. All pool indexes must remain stable, and nodes and edges must enforce `id == index` where specified.

#### Fixed simulation loop

Complete `src/sim/SimLoop.h` with:

- `fixed_dt = 1.0f / 60.0f`
- frame delta clamp
- accumulator behavior
- pause state
- `0x`, `1x`, `2x`, `4x` simulation speed
- deterministic fixed-tick scheduling
- overflow and large-frame protections

Provide public methods suitable for testing:

- advance by frame delta
- inspect number of simulation steps
- reset accumulator
- set pause
- set speed

#### Camera

Complete `src/core/Camera2D.h/.cpp` with:

- `center_world`
- `zoom`
- minimum and maximum zoom
- viewport size
- screen/world coordinate conversion
- cursor-centered zoom
- drag panning
- keyboard panning
- zoom bounds
- finite value guards

The camera should support direct unit tests for transform round trips and zoom anchoring.

#### Application

Complete `src/core/App.h/.cpp` and `src/main.cpp` with:

- SDL3 window initialization
- SDL3 renderer initialization
- ImGui SDL3/renderer backends
- event polling
- camera input handling
- fixed-timestep dispatch
- pause/speed controls
- clean shutdown
- resource release

Do not implement graph simulation internals in `App`. The application should coordinate systems only.

### Tests

- SimLoop emits the correct number of ticks for frame delta and fixed dt.
- Large frame delta is clamped.
- Pause prevents simulation ticks.
- Speed changes adjust tick count predictably.
- Screen-to-world/world-to-screen conversion round trips within floating-point tolerance.
- Cursor-centered zoom keeps the world point under the pointer fixed.
- Invalid camera values do not produce NaN transforms.

### Completion criteria

- Application initializes and closes cleanly.
- Fixed loop is testable without SDL initialization.
- Camera transforms and zoom behavior pass tests.
- `Types.h` defines every core ID and enum needed by subsequent work.

### Relevant task IDs

- 1.1.4
- 1.2.1–1.2.5

## Phase 2: Graph Storage and JSON Map Loading

### Objective

Create the core graph model and validated city map loader.

### Work

Complete `src/sim/Graph.h/.cpp` and the sample map.

Implement:

- Contiguous `std::vector<Node>` and `std::vector<Edge>` pools.
- `out_edges` and `in_edges` per node.
- Stable ID-to-index lookup assumptions.
- Bidirectional and one-way edge support.
- Node and edge invariant validation.
- `load_from_json(const std::filesystem::path&)`.
- Clear error return or status reporting.
- Edge rebuild when edge state changes.

Node fields:

- ID
- world position
- node type
- waiting passengers
- waiting cargo tons
- intersection throughput

Edge fields:

- ID
- endpoints
- bidirectional flag
- road class
- base lane count
- length
- speed limit
- dynamic vehicles/congestion/effective speed
- pull-out bays
- bus lane
- signal priority
- roadwork state and multiplier

Map JSON validation must reject:

- Missing nodes or edges
- Duplicate node IDs
- Out-of-range endpoints
- Missing endpoints
- Zero or negative length
- Negative or zero speed limit where invalid
- Lane count below one
- Invalid road/node enum names
- NaN or infinite positions
- Invalid edge references

Implement bidirectional adjacency by creating or resolving outgoing and incoming traversal entries consistently. One-way edges must not appear in the reverse direction.

Expand `assets/maps/sample_city.json` to demonstrate:

- At least one of every road class
- Stops
- Terminal
- Depot anchor
- Regional gateway
- Pull-out bays
- One-way and bidirectional examples
- A small connected city network suitable for routing and rendering

### Tests

- Load a valid city map.
- Verify node ID/index equality.
- Verify node and edge counts.
- Verify adjacency completeness.
- Verify reverse traversal for bidirectional edges.
- Verify no reverse traversal for one-way edges.
- Reject every invalid map class above.
- Verify finite and positive invariant values.

### Completion criteria

- Graph loading is deterministic.
- Invalid maps fail with an actionable error.
- Adjacency lists match edge directionality.
- Sample city provides connected routing coverage.

### Relevant task IDs

- 2.1.1–2.1.4

## Phase 3: Schematic Rendering and Hit Testing

### Objective

Render the graph and expose interactive camera and selection behavior.

### Work

Complete `src/gfx/VectorRenderer.h/.cpp` and App integration.

Implement:

- Batched line/quad primitives.
- World-to-screen transform use.
- Zoom-scaled road and node thickness.
- Road-class colors:
  - SingleLane: thin muted light gray
  - TwoLane: medium gray
  - MultiLane: blue
  - Highway: thick orange
- Traffic heatmap mode:
  - green at low saturation
  - yellow around `0.7`
  - red at `1.0+`
- Node markers for every `NodeType`.
- Hover and selection rings.
- Numeric waiting backlog badges.
- Optional edge labels or debug labels only if they do not disturb clean map defaults.
- Competitor focus overlay hook for later rival rendering.

Implement hit testing:

- `find_nearest_node`
- `find_nearest_edge`
- zoom-aware pick radius
- deterministic selection when candidates overlap
- line-segment distance for world-space edge selection

Implement a simple debug/vehicle rendering hook so later vehicle code can render by `(edge_id, edge_dist_m)`.

### Tests

- Nearest node returns the correct node in one and overlapping candidates.
- Nearest edge chooses the closest segment.
- Pick radius is larger in world units for distant zoom levels as intended.
- No hit when the cursor is outside the radius.
- Zoomed node and edge thickness remain legible.

### Completion criteria

- A loaded map renders in default mode.
- Road classes are visually distinct.
- Heatmap toggle changes edge colors predictably.
- Hit testing works under pan and zoom.
- No per-node clutter beyond lightweight numeric backlog badges.

### Relevant task IDs

- 1.2.3–1.2.4
- 1.3.1–1.3.5

## Phase 4: Dynamic Weights and Pathfinding

### Objective

Add dynamic fastest-path routing and path cache invalidation.

### Work

Implement:

- Edge capacity from road class and lane count.
- Optional capacity modifiers for bus lanes.
- `compute_edge_travel_time` using:
  - length
  - speed limit
  - congestion ratio
  - vehicle maximum speed
  - bus lane perk
  - signal priority perk
  - roadwork multiplier
- A* or Dijkstra pathfinding.
- Euclidean heuristic for A* scaled consistently with vehicle maximum speed.
- Ordered node and edge sequences.
- Valid empty and disconnected-path behavior.
- Positive and finite travel-time guards.
- Path cache by start node, goal node, vehicle speed/vehicle class, and graph road version.
- Cache invalidation when congestion crosses configured thresholds or road state changes.
- Separate invalidation for bus lane and signal-priority changes.

Avoid floating-point cache churn from minor congestion updates. Define threshold bands or explicit versions.

### Tests

- Direct edge path.
- Alternate shortest path.
- Bidirectional traversal.
- One-way dead end.
- Empty graph.
- Invalid node IDs.
- Disconnected graph.
- Congestion changing path choice.
- Roadwork changing path choice.
- Cache hit and invalidation behavior.
- Travel time decreases with a faster vehicle only where legitimate.

### Completion criteria

- Pathfinding never loops or crashes on invalid graph input.
- Dynamic weights are used consistently.
- Cache invalidation is observable and testable.
- Returned node and edge sequences correspond exactly.

### Relevant task IDs

- 2.2.1–2.2.3

## Phase 5: Vehicle Motion, Congestion, and Lane Behavior

### Objective

Add vehicles to the graph and implement live fixed-timestep traffic behavior.

### Work

Create or complete simulation pools and runtime behavior for:

- Vehicle identity and company/route/depot ownership
- Current edge
- Edge distance
- Current speed
- State
- Passenger and cargo loads

Implement:

- Active vehicle count recalculation each tick.
- Congestion ratio calculation.
- Effective edge speed calculation.
- Smooth low-congestion speed behavior through `0.7` saturation.
- Progressive slowdown above `0.7`.
- Minimum crawl floor preventing deadlocks.
- Safe vehicle movement across edge boundaries.
- Route-edge transition and route completion behavior.
- Single-lane strict queue/follow behavior.
- Queueing behind stopped, loading, broken, or roadwork-blocked vehicles.
- Multi-lane/highway overtaking when clearance exists.
- Relative position updates and lane state representation.
- No overtaking through active roadwork or prohibited edges.
- Stop dwell behavior.
- Dedicated pull-out bay bypass.
- Standard roadside stop delay.
- Random and scheduled city roadwork events.
- Roadwork activation/deactivation updating edge speed and path cache versions.

Use named constants or a small traffic configuration structure for:

- Congestion thresholds
- Crawl floor
- Dwell duration
- Overtaking clearance
- Roadwork duration ranges
- Random seed behavior

### Tests

- Vehicle advances along an edge.
- Vehicle transitions at exact endpoint distances.
- Congestion ratio reaches and respects capacity.
- Speed curve remains finite and monotonic.
- Minimum speed never freezes the tick loop.
- Single-lane vehicles maintain no unsafe overlap.
- Multi-lane vehicles can overtake in permitted conditions.
- Roadwork blocks overtaking.
- Stop dwell holds the vehicle for the expected duration.
- Pull-out bay prevents following traffic delay.

### Completion criteria

- Vehicles can move deterministically on the sample city.
- Congestion changes edge effective speed.
- Lane behavior matches road class constraints.
- Roadworks modify traffic and paths.
- No per-vehicle heap allocation in the inner update loop.

### Relevant task IDs

- 2.3.1–2.3.4
- 3.1.2 where wear/breakdown behavior is integrated

## Phase 6: Vehicle Catalog, Depots, Maintenance, and Workforce

### Objective

Implement operational fleet and depot systems.

### Work

### Vehicle catalog

Define catalog data for:

- MiniVan
- CityBus
- DoubleDecker
- ArticulatedBus
- LightTruck
- MediumTruck
- SemiTruck

Each entry should define:

- purchase price
- passenger and cargo capacity
- top speed
- comfort
- ICE or electric consumption
- maintenance cost
- breakdown sensitivity
- tier unlock
- age or condition defaults

No single vehicle instance should duplicate immutable catalog values unless required for runtime specialization.

### Vehicle runtime

Implement:

- Wear increase over distance/time
- Energy/fuel decrease
- Battery or fuel warning state
- Low-resource routing to depot
- Critical wear breakdown risk
- Random but seeded breakdown checks
- Broken-down state
- Towing or return-to-depot behavior
- Scheduled maintenance
- Refueling/charging queue
- Mechanic repair queue
- Return to duty after service

### Depots

Implement:

- Depot ownership
- Depot node attachment
- Parking slots
- Service bays
- EV chargers
- Fuel pumps
- Capacity and queue occupancy
- Parking limit enforcement
- Asset purchase hooks for later phases

### Workforce

Implement:

- Worker IDs and company/depot ownership
- Roles: Driver, Mechanic, Dispatcher
- Hourly wage
- Shift time
- Fatigue
- On-duty state
- Fatigue decay during rest
- Fatigue penalties
- Driver requirement before dispatch
- Mechanic availability for repair
- Dispatcher availability for timetable automation

### Tests

- Catalog lookup for every vehicle class.
- Wear increases and remains bounded.
- Energy reaches zero safely.
- Critical wear creates breakdown risk without invalid state transitions.
- Depot rejects vehicles past parking capacity.
- Refuel/charging queue is FIFO or explicitly documented if priority-based.
- Service bay returns a vehicle to active duty.
- Dispatch rejects an unstaffed vehicle.
- Fatigue rises during duty and falls during rest.
- Fatigue penalties alter dispatch or reliability consistently.

### Completion criteria

- Fleet and depot state is represented by contiguous pools.
- Every state transition has a valid predecessor and exit path.
- Staff availability affects operations.
- Maintenance can restore wear and energy.

### Relevant task IDs

- 3.1.1–3.1.3
- 3.2.1–3.2.2

## Phase 7: Passenger/Cargo Demand and Route Operations

### Objective

Make routes meaningful to passengers and freight.

### Work

Implement:

- Route create/edit/toggle/delete operations.
- Route kinds: private passenger, contract passenger, cargo.
- Node sequences and edge sequences.
- Ticket fare.
- Target headway.
- Time since last dispatch.
- Vehicle assignment.
- Depot assignment.
- Active state.
- Revenue and operating cost telemetry.
- Riders and tons carried.
- Average satisfaction.

Implement demand:

- Origin-destination passenger generation.
- Cargo shipment generation.
- Time-of-day passenger demand curves.
- Freight demand periods.
- Route and mode utility calculations.
- Fare, wait, travel duration, comfort, and age weighting.
- Cargo deadline or cold-chain priority hooks.

Implement allocation:

- Rider boarding at stops.
- Cargo pickup and unloading.
- Capacity enforcement.
- Queueing of waiting passengers and cargo.
- Route selection for each shipment or trip.
- Satisfaction updates from wait, crowding, and reliability.

Implement dispatch:

- Available vehicle selection.
- Available driver selection.
- Depot readiness.
- Headway enforcement.
- Empty route and disconnected route guards.

Implement UI:

- Corridor/Line Inspector metrics.
- Daily riders or tons.
- Market share breakdown.
- Satisfaction.
- Revenue and operating cost.
- Route controls and route sequence editing.
- Fare and frequency controls.
- Vehicle/depot assignment controls.

### Tests

- Demand is bounded and deterministic for a seed.
- Empty graph produces no demand allocation crash.
- Passenger selection favors each utility component in controlled test cases.
- Over-capacity riders are left waiting or allocated later.
- Cargo weight never exceeds capacity.
- Route dispatch respects headway.
- Driver absence prevents dispatch.
- Revenue and cost telemetry do not become negative unintentionally.
- Inspector receives the correct route telemetry.

### Completion criteria

- A player can operate at least one passenger route.
- Demand appears at stops or terminals.
- Vehicles pick up and deliver riders/cargo.
- Route economics are visible and testable.

### Relevant task IDs

- 3.3.1–3.3.3
- 4.1.2 partly: AI route tuning depends on route APIs

## Phase 8: AI Competitors and Market Ecosystem

### Objective

Create rival companies and living market behavior.

### Work

Implement company pools:

- Player and AI company flags
- Archetype
- Cash
- Net worth
- Reputation
- Tier
- Regional market share
- Anti-monopoly tax

Implement archetypes:

- LowCostOperator
- RegionalGiant
- GreenPioneer
- LogisticsHeavyweight
- SuburbanExpress

For each archetype define:

- Route preference
- Fleet preference
- Fare strategy
- Depot strategy
- Contract preference
- Risk tolerance
- Decision interval

Implement periodic AI decisions:

- Open and close routes
- Adjust fare and headway
- Purchase vehicles
- Acquire depot slots or depots
- Bid on contracts
- Shift to cargo or passenger markets
- Respond to congestion and route profitability

Implement market ecosystem:

- Market share calculation
- Anti-monopoly tax above `75%`
- Progressive regulatory friction
- Startup spawn interval
- Startup archetype selection
- Startup entry condition
- Late-game asset purchase
- Late-game vehicle, route, or depot purchase
- Selective company buyout eligibility and valuation

Keep all AI decisions deterministic for a persisted seed. Decision intervals should be fixed-tick or simulated-period based, not frame-rate dependent.

### Tests

- Market share sums remain valid.
- Anti-monopoly tax activates above 75% and remains off below it.
- Friction changes increase consistently with market share.
- Each archetype has a valid behavior profile.
- AI cannot dispatch without a vehicle and driver.
- Startup spawn respects configured interval and cap.
- Asset purchase modifies cash and ownership correctly.
- Buyout removes or consolidates the target state without dangling IDs.

### Completion criteria

- At least one rival actively operates after simulation begins.
- Rival routes and vehicles are visible and selectable in competitor focus mode.
- Anti-monopoly and startup behavior is observable.
- AI decisions are deterministic and testable.

### Relevant task IDs

- 4.1.1–4.1.3

## Phase 9: Contracts and SLA Enforcement

### Objective

Add timed contracts and contract-board operations.

### Work

Define contract templates:

- CorporateShuttle
- MunicipalTransit
- CargoFreight
- ColdChain

Each contract should expose:

- Tier requirement
- Reputation requirement
- Payout per period
- SLA target
- Current SLA
- Assigned company
- Contract duration
- Breach penalty
- Optional bonus

Implement bidding:

- Bid data entry and player submission
- AI bid evaluation
- Cost, reliability, reputation, and archetype factors
- Deterministic tie-breaking
- Award to the winning company
- Contract activation and assignment

Implement SLA:

- Period delivery or service measurement
- Live SLA percentage
- Payout and bonus evaluation
- Breach penalties
- Reputation changes
- Contract expiration and renewal status
- Cold-chain or deadline penalties if applicable

Build Contract Board UI:

- Available contracts
- Requirements
- Player and AI bid status
- SLA status
- Payout and penalty preview
- Active and expired contracts

### Tests

- Tier/reputation requirements reject invalid bidders.
- AI and player bid evaluation is deterministic.
- Highest valid score wins with stable tie-breaking.
- SLA at/above target receives expected payout.
- SLA below target receives penalty.
- Reputation remains bounded.
- Contract assignment updates route or company ownership correctly.
- Contract expiration cannot double-payout or double-penalize.

### Completion criteria

- Player can place a valid bid.
- Contract award follows a deterministic bid rule.
- Active contract performance changes payout and reputation.
- Contract UI reflects current status.

### Relevant task IDs

- 4.2.1–4.2.3

## Phase 10: Tier Progression and Lobbying

### Objective

Implement progression, unlocks, and high-tier municipal influence.

### Work

Create tier table data for T1–T12:

- Era
- Cash/net worth threshold
- Fleet count/type requirements
- Depot/infrastructure requirements
- Workforce requirements
- Reputation requirement
- Market and contract requirements if applicable
- Unlocks

Implement progression checks:

- Evaluate every metric as a conjunction of requirements.
- Return the next eligible tier.
- Prevent tier skipping unless explicitly defined.
- Emit advancement notification.
- Preserve safe behavior at Tier 12.
- Use configured metrics and avoid scattered tier-number conditions.

Implement unlock categories:

- Vehicle classes
- Routes and auto-dispatch
- Timetable features
- Express routes
- Contract categories
- Auto-replacement
- Regional operations
- Toll permits
- Cold-chain freight
- Lobbying
- Acquisitions
- Articulated buses
- BRT and rail study features

Implement lobbying actions:

- Bus pull-out bay
- Smart signal priority
- Dedicated BRT lane
- Metro/light-rail study

For each action track:

- Tier requirement
- Reputation requirement
- Cost
- Approval chance or approval threshold
- Cooldown
- Success state
- Graph effect
- Ongoing maintenance or reputation effect

Implement graph effects:

- Add `has_pullout_bays` to selected stops.
- Add signal priority to selected edges or nodes.
- Set `has_bus_lane` on approved edges.
- Expose approved study contract or unlock state.

### Tests

- Tier 1 has valid initial unlocks.
- Advancement requires every metric.
- One unmet metric prevents advancement.
- Tier table covers T1–T12 exactly once.
- Tier unlock data matches Era and concept requirements.
- Lobbying rejects insufficient tier/cash/reputation.
- Successful action modifies graph state exactly once.
- Failed or rejected action has no graph side effects.
- Tier 12 cannot advance beyond 12.

### Completion criteria

- The player can advance when all metrics are satisfied.
- Locks prevent unavailable vehicles and features.
- Lobbying actions apply deterministic graph effects.

### Relevant task IDs

- 4.3.1–4.3.2
- 5.1–5.3

## Phase 11: Regional Maps and Expansion

### Objective

Support city-to-region and region-to-city progression.

### Work

- Add regional map JSON format or an explicit city/region map schema.
- Add regional gateways and inter-city edges.
- Support loading a city or regional map.
- Support switching or merging connected city maps if that is the chosen implementation model.
- Define a stable coordinate system across map boundaries.
- Validate regional cross-links.
- Prevent duplicate or conflicting node IDs across connected maps.
- Add highway travel and toll permit hooks.
- Ensure pathfinding works over all loaded regions.
- Implement city and regional corridor selection.
- Integrate regional market share by corridor or by map region.
- Add regional map data to save/load state.

Use this phase to decide and document:

- Whether regional maps are a second full graph or an expanded graph.
- Whether city services can operate from regional starts.
- Whether disconnected regions are selectable simulation areas or initially locked content.

### Tests

- City map loads with regional gateway but no accidental cross-region assumptions.
- Regional map connects two cities through a valid highway.
- Cross-city endpoints have valid node references.
- Inter-city path cost is finite.
- Duplicate IDs are rejected if map parts are merged.
- Regional gateway cannot become an ordinary intersection by deserialization.
- Region state survives save/load.

### Completion criteria

- A city can expand to at least one additional city.
- A regional start can launch local city services.
- Regional routes are pathfindable and visible.

### Relevant task IDs

- FR-01.2
- 4.3.3

## Phase 12: Persistence

### Objective

Add versioned full-state JSON save/load.

### Work

Implement serialization for:

- Graph nodes and dynamic edge properties
- Graph roadwork state
- Vehicles and runtime state
- Routes and telemetry
- Depots
- Workers
- Contracts
- Companies and AI state
- Cash and net worth
- Tier and unlock state
- Lobbying state
- Regional map identity and loaded map parts
- Random seed
- Simulation time and accumulated period state
- Player settings relevant to simulation state

Implement:

- Save format version field.
- Deterministic field order where practical.
- Atomic write: temporary file followed by rename or equivalent safe replacement.
- Load from `.json` or a project-defined save extension.
- Clear error reporting for missing/corrupt files.
- Version and schema validation.
- Migration or an explicit unsupported-version error.
- ID reference validation after load.
- Pool compaction or stable-ID checks.
- Cache rebuilding after graph load.
- Path cache invalidation after graph modification.

Never silently normalize invalid save data. Report the validation failure and preserve or leave the existing session unchanged.

### Tests

- Save/load round trip for an empty simulation.
- Save/load round trip for a populated graph.
- Save/load while vehicles are on edges.
- Save/load with active contracts and depots.
- Save/load with AI companies and routes.
- Corrupt JSON fails safely.
- Unsupported version fails safely.
- Dangling route, vehicle, edge, depot, or company references fail validation.
- Reload produces deterministic simulation for a fixed seed and tick count.
- A save can be loaded after graph roadwork changes.

### Completion criteria

- Player-facing save/load is usable.
- Loading never leaves dangling IDs or invalid graph state.
- A fixed seed and identical update sequence produce matching state.

### Relevant task IDs

- FR-06.1–FR-06.2
- 4.3.3

## Phase 13: Fleet/Depot/Workforce UI and Operational Inspector

### Objective

Expose all core operations in a usable UI.

### Work

Complete `src/ui/InspectorUI.h/.cpp` and add coordinated operational panels:

- Corridor and line inspector
- Fleet view
- Depot view
- Workforce view
- Contract board
- Lobbying panel
- Simulation controls
- Save/load controls
- Performance/debug counters

UI requirements:

- Keep the map uncluttered.
- Show only concise backlog values on nodes.
- Keep detailed economics in inspectors.
- Use stable item IDs for repeated lists.
- Avoid mutating pools directly from UI without a simulation API.
- Queue route or economy edits for validation rather than partially applying invalid changes.
- Provide clear validation errors.
- Highlight the selected route, node, edge, rival, or vehicle.

### Tests

- UI smoke test initializes ImGui backends or a UI abstraction when possible.
- Invalid route node sequences cannot be committed.
- Invalid fare, headway, and capacity values are blocked or clearly reported.
- Selected entities remain valid after pool updates.
- Save/load controls call the same persistence API used by tests.

### Completion criteria

- Player can create and operate a route through UI.
- Player can view depot, fleet, worker, contract, lobbying, and progression state.
- UI does not corrupt graph or simulation pools.

### Relevant task IDs

- FR-06.1
- 3.3.3
- 4.2.2
- 4.3.1–4.3.2

## Phase 14: Playable Vertical Slice and Final Verification

### Objective

Integrate all completed systems into a playable city demonstration and verify the complete acceptance surface.

### Starting scenario

Create one deterministic starting state containing:

- A complete sample city map
- A player depot
- A small player fleet
- A staffed workforce
- At least one passenger route
- At least one cargo route or cargo shipment opportunity
- One active rival company
- One available contract
- Initial Tier 1 unlock state
- Starting cash and a documented goal state

### Player flow

The demo should allow a player to:

1. Pan and zoom the city.
2. Select nodes and edges.
3. Create or edit a route.
4. Assign a driver and vehicle.
5. Dispatch vehicles and observe traffic.
6. Toggle congestion heatmap.
7. Select a rival and inspect their activity.
8. View corridor/line economics.
9. Purchase or service fleet/depots as unlocked.
10. Bid on a contract.
11. Observe SLA, payout, and reputation changes.
12. Save and reload.
13. Pause and change simulation speed.

### Performance verification

- Target 60 FPS at standard city scale.
- Test hundreds of active vehicles if content permits.
- Measure fixed-tick and rendering costs with the debug and release builds.
- Remove avoidable per-tick allocations.
- Review cache invalidation and pathfinding frequency.
- Check for pool erase/shift patterns in hot loops.
- Confirm frame delta clamping prevents simulation spiral.

### Final tests

- Clean debug build.
- Clean release build.
- CTest or project test suite passes.
- Graph map and save/load smoke tests pass.
- Fixed-loop tests pass.
- Pathfinding tests pass.
- Traffic tests pass.
- Fleet/workforce tests pass.
- Demand/route tests pass.
- AI and contract tests pass.
- Progression/lobbying tests pass.
- Regional smoke test passes if data is available.
- Save/load round-trip test passes.
- 10-minute playable simulation does not leak or corrupt state.

### Completion criteria

- The complete city vertical slice starts from a clean build.
- The player can complete the documented player flow.
- Save/load is reliable.
- No critical crash or invalid-state path remains.
- Debug and release builds meet the warning and test requirements.

### Relevant task IDs

- All remaining tasks
- NFR-01–NFR-04

## Cross-Phase Acceptance Checklist

Before the project is considered implementation-complete:

- [ ] CMake debug and release configurations build cleanly.
- [ ] C++20 and strict warning flags are active for every target.
- [ ] Assets are copied to the executable directory.
- [ ] `Types.h` is complete and consistent.
- [ ] The fixed loop is deterministic and frame-rate independent.
- [ ] Camera transform round trips are accurate.
- [ ] Graph JSON validation is complete.
- [ ] Default road rendering and heatmap rendering are implemented.
- [ ] Dijkstra/A* dynamic pathfinding passes tests.
- [ ] Congestion and lane behavior follow road class constraints.
- [ ] Vehicles experience wear and resource use.
- [ ] Depots and workforce enforce operational limits.
- [ ] Passenger and cargo routes generate telemetry and satisfaction.
- [ ] AI archetypes operate independently and deterministically.
- [ ] Contract bids and SLA payouts function.
- [ ] Anti-monopoly behavior activates above 75% share.
- [ ] Tiers 1–12 have complete progression data.
- [ ] Lobbying modifies graph infrastructure.
- [ ] Regional expansion is supported.
- [ ] Full save/load survives a populated session.
- [ ] Unit and smoke tests cover each major subsystem.
- [ ] The playable city demo completes a save/load cycle.
- [ ] 60 FPS is targeted with hundreds of active vehicles in a standard map.
- [ ] No heap allocations are intentionally placed in hot inner loops.
- [ ] Simulation results are deterministic for a fixed seed.
- [ ] No map, save, route, or pool state can produce NaN or invalid indices silently.
- [ ] `TASKS.md` accurately reflects implementation status.

## Recommended Agent Handoff Format

Each phase agent should finish with:

1. **Phase:** phase name and task IDs.
2. **Completed:** concise implementation summary.
3. **Changed files:** paths and purpose.
4. **Tests added:** test names and coverage.
5. **Verification:** build presets and test commands with pass/fail.
6. **Dependencies satisfied:** APIs or data available to the next phase.
7. **Known limitations:** explicit temporary gaps or assumptions.
8. **Recommended next phase:** the next implementation phase.
9. **Task checkboxes updated:** list of checked task IDs.

This format prevents the next worker from rebuilding work or guessing about public APIs.
