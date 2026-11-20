#pragma once

#include <cstdint>
#include <limits>

namespace core
{
using NodeId = std::uint32_t;
using EdgeId = std::uint32_t;
using VehicleId = std::uint32_t;
using RouteId = std::uint32_t;
using DepotId = std::uint32_t;
using WorkerId = std::uint32_t;
using ContractId = std::uint32_t;
using CompanyId = std::uint32_t;

inline constexpr std::uint32_t InvalidId = std::numeric_limits<std::uint32_t>::max();

enum class RoadClass : std::uint8_t
{
    SingleLane,
    TwoLane,
    MultiLane,
    Highway
};

enum class NodeType : std::uint8_t
{
    Intersection,
    Stop,
    Terminal,
    DepotAnchor,
    RegionalGateway
};

enum class VehicleClass : std::uint8_t
{
    MiniVan,
    CityBus,
    DoubleDecker,
    ArticulatedBus,
    LightTruck,
    MediumTruck,
    SemiTruck
};
enum class RouteKind : std::uint8_t
{
    PrivatePassenger,
    ContractPassenger,
    Cargo
};
enum class AIArchetype : std::uint8_t
{
    None,
    LowCostOperator,
    RegionalGiant,
    GreenPioneer,
    LogisticsHeavyweight,
    SuburbanExpress
};
enum class Powertrain : std::uint8_t
{
    ICE,
    Electric
};
enum class VehicleState : std::uint8_t
{
    EnRoute,
    Dwelling,
    Queued,
    ToDepot,
    Servicing,
    BrokenDown
};
enum class WorkerRole : std::uint8_t
{
    Driver,
    Mechanic,
    Dispatcher
};
enum class ContractKind : std::uint8_t
{
    CorporateShuttle,
    MunicipalTransit,
    CargoFreight,
    ColdChain
};
enum class SimulationSpeed : std::uint8_t
{
    X0 = 0,
    X1 = 1,
    X2 = 2,
    X4 = 4
};
}