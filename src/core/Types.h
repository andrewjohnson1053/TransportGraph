#pragma once

#include <compare>
#include <cstdint>
#include <limits>

namespace core
{
inline constexpr std::uint32_t InvalidId = std::numeric_limits<std::uint32_t>::max();

template <typename Tag>
class Id
{
public:
    constexpr Id() noexcept = default;
    explicit constexpr Id(std::uint32_t value) noexcept : value_(value) {}

    constexpr std::uint32_t value() const noexcept { return value_; }
    constexpr bool is_valid() const noexcept { return value_ != InvalidId; }

    constexpr bool operator==(const Id&) const noexcept = default;
    constexpr auto operator<=>(const Id&) const noexcept = default;

private:
    std::uint32_t value_ = InvalidId;
};

struct NodeIdTag;
struct EdgeIdTag;
struct VehicleIdTag;
struct RouteIdTag;
struct DepotIdTag;
struct WorkerIdTag;
struct ContractIdTag;
struct CompanyIdTag;

using NodeId = Id<NodeIdTag>;
using EdgeId = Id<EdgeIdTag>;
using VehicleId = Id<VehicleIdTag>;
using RouteId = Id<RouteIdTag>;
using DepotId = Id<DepotIdTag>;
using WorkerId = Id<WorkerIdTag>;
using ContractId = Id<ContractIdTag>;
using CompanyId = Id<CompanyIdTag>;

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