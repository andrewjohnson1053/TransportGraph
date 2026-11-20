# Concept Document: Project Transport Graph

## 1. Vision & High Concept
**Project Transport Graph** is a high-performance, top-down 2D vector transport management simulator engineered on a custom C++ architecture. Players build and scale a private passenger and cargo transport company operating across pre-existing city and multi-city regional road networks.

Visualized with a crisp, schematic node-and-edge graph aesthetic inspired by *Rail Route*—while deliberately avoiding rail-signaling puzzles—the game centers on spatial route planning, dynamic congestion and overtaking physics, competitive AI rivals, contract bidding, and a 12-tier business progression system that eventually leads into municipal infrastructure lobbying and selective asset/company acquisitions.

## 2. Core Gameplay Pillars
1. **Schematic Vector Clarity:** Every road, intersection, terminal, depot, and vehicle is rendered as clean, zoom-invariant 2D vector geometry with instant legibility across neighborhood, city, and regional scales.
2. **Dual Passenger & Cargo Logistics:** Players manage unscheduled open-market passenger routes, time-bound corporate/municipal transit tenders, and industrial/perishable freight logistics across a shared road graph.
3. **Living Competitive Ecosystem:** Multiple distinct AI competitor archetypes contest corridors, fares, and tenders, while anti-monopoly friction (>75% market share pressure), periodic new startups, and late-game asset/company buyouts keep the economy dynamic.
4. **12-Tier Multi-Vector Progression:** Progression spans four eras (**Local**, **Metro**, **Regional**, **Conglomerate**) across 12 tiers, tying advancement to capital/net worth, fleet size and diversity, depots/infrastructure, workforce, and reputation.

## 3. Visual & UI Philosophy
* **Road-Class Color Coding (Default Map):** Edges represent pre-existing municipal and regional roads and are colored by infrastructure class—never by line ownership—so players read road geometry and capacity first:
  * *Single-Lane Local Streets:* Thin, muted light gray (low capacity, no overtaking, prone to queueing).
  * *2-Lane Arterial Roads:* Medium solid gray (standard urban throughput).
  * *Multi-Lane Avenues:* Distinct blue (high capacity, supports overtaking and future bus/BRT lanes).
  * *Highways / Expressways:* Thick orange (high speed, long-distance inter-city corridors).
* **On-Demand Overlays:**
  * *Traffic Saturation Heatmap:* Toggleable green-to-yellow-to-red gradient showing live edge utilization vs. capacity.
  * *Competitor Focus Mode:* Selecting an AI rival highlights their active routes, vehicles, and presence without overriding default road-class clarity.
* **Clean Nodes, Rich Inspectors:** Map nodes stay uncluttered (showing only essential passenger/cargo backlog indicators), while detailed market share bars, rider/freight demand, satisfaction, and route economics live inside a dedicated **Corridor & Line Inspector** panel.

## 4. Traffic & Overtaking Philosophy
* **Realistic Bypass vs. True Bottlenecks:** On multi-lane avenues and highways, faster or unobstructed vehicles can overtake slower traffic when passing clearance exists, preventing artificial "stuck behind one slow bus" frustration everywhere.
* **Meaningful Constraints:** Single-lane streets enforce single-file flow and queueing behind slow, loading, or broken-down vehicles unless pull-out bays exist; roadside stops without dedicated pull-out bays can delay following traffic.
* **City Evolution:** Municipal traffic and city AI can introduce temporary roadworks, lane closures, or upgrades, pushing players to adapt routes or lobby at higher tiers.

## 5. Starting Scales, Progression & Late-Game Influence
* **Flexible Starting Scale:** Players can start in a **single city** and expand outward into a **multi-city region**, or start at a **regional scale** and launch urban city services.
* **12-Tier Arc (Eras I–IV):**
  * *Era I (Tiers 1–3 — Local Operator):* Mini-vans, city buses, basic maintenance bay, light delivery trucks, corporate shuttles, first small depot, and positive cash flow.
  * *Era II (Tiers 4–6 — Metropolitan Mobility):* Medium cargo trucks, timetable auto-dispatching, double-decker buses, express routes, second depot, EV charging bays/electric buses, and municipal transit tenders.
  * *Era III (Tiers 7–9 — Regional Integration):* Luxury inter-city coaches, highway toll permits, heavy semi-trucks, cold-chain freight, multi-modal terminals, auto-replacement, and multi-city operations.
  * *Era IV (Tiers 10–12 — Transit Conglomerate):* Lobbying for dedicated bus bays and smart signal priority, articulated buses, lobbying for dedicated BRT lanes, asset/depot acquisitions, selective late-game company buyouts, and metro/light-rail study contracts.