# Requirements Document (`REQUIREMENTS.md`)

## 1. Functional Requirements

### FR-01: Map Loading & Graph Topology
* **FR-01.1:** The engine shall load city and multi-city regional network definitions from JSON map files (`assets/maps/*.json`), validating node indices, edge endpoints, positive lengths, and speed limits.
* **FR-01.2:** The graph system shall support both single-city starting maps and multi-city regional maps (supporting progression from city to region and region to city services).
* **FR-01.3:** The city simulation shall support dynamic road state changes (such as temporary roadworks and infrastructure upgrades) that update edge properties at runtime.

### FR-02: Camera & Schematic Vector Rendering
* **FR-02.1:** The camera shall support smooth panning and cursor-centered zooming with configurable zoom bounds (`min_zoom`, `max_zoom`) and accurate screen-to-world / world-to-screen conversion.
* **FR-02.2:** In default map mode, the renderer shall color-code road edges by `RoadClass` (`SingleLane`, `TwoLane`, `MultiLane`, `Highway`) with distinct stroke weights/styles.
* **FR-02.3:** The renderer shall provide a toggleable Traffic Saturation Heatmap overlay transitioning smoothly from green to yellow to red based on congestion saturation.
* **FR-02.4:** Selecting an AI rival shall highlight that rival's active routes, vehicles, and nodes without altering the underlying road-class colors.
* **FR-02.5:** Map nodes shall remain visually clean (showing lightweight backlog numbers rather than per-node pie charts), with market share, demand, and satisfaction displayed in the Corridor/Line Inspector UI.

### FR-03: Routing, Traffic & Vehicle Physics
* **FR-03.1:** The routing engine shall compute shortest/fastest paths across the road graph using dynamic edge weights (distance, speed limit, congestion, lane/signal perks, roadworks).
* **FR-03.2:** Vehicles shall advance along edges using fixed-timestep updates, respecting edge speed limits, vehicle top speeds, and congestion curves.
* **FR-03.3:** Vehicles on multi-lane and highway edges shall be able to overtake slower vehicles when conditions allow, whereas single-lane edges shall enforce queueing/following behavior.
* **FR-03.4:** Vehicles shall experience wear and fuel/battery consumption, require depot servicing/refueling/charging, and incur higher breakdown risk at high wear or low maintenance.
* **FR-03.5:** Workforce mechanics shall track drivers, mechanics, and dispatchers, wages, shift hours, and fatigue, preventing unstaffed dispatches and applying fatigue effects.

### FR-04: Demand, Pricing, Contracts & Competition
* **FR-04.1:** The simulation shall generate passenger demand and cargo shipments and allocate riders/cargo based on fare, frequency/wait time, travel time, and comfort.
* **FR-04.2:** Players shall be able to create, edit, toggle, and delete routes, set fares/frequencies, and assign vehicles and depots.
* **FR-04.3:** The system shall simulate AI competitor archetypes (`LowCostOperator`, `RegionalGiant`, `GreenPioneer`, `LogisticsHeavyweight`, `SuburbanExpress`) operating lines, setting prices, and bidding on contracts.
* **FR-04.4:** The system shall support contract bidding, SLA tracking, periodic payouts, and breach penalties across corporate, municipal, and freight/cold-chain contracts.
* **FR-04.5:** The market system shall enforce anti-monopoly pressure above 75% market share, spawn periodic new AI startups, and allow late-game asset and company buyouts.

### FR-05: 12-Tier Progression & Lobbying
* **FR-05.1:** The progression system shall track Tiers 1–12 across Eras I–IV (Local, Metro, Regional, Conglomerate), evaluating multi-metric thresholds (finances, fleet, infrastructure, workforce, reputation).
* **FR-05.2:** Advancing tiers shall unlock specific vehicle classes, operational features (auto-dispatch, express routes, auto-replacement), contract categories, and lobbying actions.
* **FR-05.3:** High-tier lobbying actions shall allow petitioning/funding dedicated bus pull-out bays, smart traffic signal priority, dedicated BRT lanes, and metro/light-rail study contracts.

### FR-06: Save / Load & UI Inspectors
* **FR-06.1:** The UI shall provide a Corridor & Line Inspector, Fleet/Depot/Workforce views, Contract Board, Lobbying controls, and simulation speed/pause controls.
* **FR-06.2:** The application shall support saving and loading simulation state (graph modifications, routes, fleet, finances, contracts, tier, and AI state) to JSON.

## 2. Non-Functional Requirements
* **NFR-01 (Frame Rate & Tick Budget):** Maintain 60 FPS target at standard map scales with hundreds of active vehicles and full city/regional graphs; keep simulation tick cost predictable via fixed timestep (`dt`) and frame-delta clamping.
* **NFR-02 (Memory & Cache Efficiency):** Store entities in contiguous `std::vector` pools using flat `uint32_t` IDs, avoiding per-tick heap allocations inside inner simulation and rendering loops.
* **NFR-03 (Numerical Safety & Determinism):** Guard against division by zero, empty routes, disconnected graphs, and negative/NaN values; make simulation deterministic for a given seed and deterministic update order.
* **NFR-04 (Build & Code Quality):** Build cleanly with CMake and C++20 under MSVC `/W4 /permissive-` and Clang/GCC `-Wall -Wextra -Wpedantic` with zero warnings, and pass automated unit/smoke tests.