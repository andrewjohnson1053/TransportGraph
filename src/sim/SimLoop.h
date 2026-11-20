#pragma once

#include "core/Types.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace sim
{
class SimLoop
{
public:
    static constexpr double fixed_dt = 1.0 / 60.0;
    static constexpr double max_frame_dt = 0.25;
    static constexpr std::size_t max_steps_per_frame = 60;

    std::size_t advance(double frame_dt) noexcept
    {
        last_step_count_ = 0;
        if (paused_ || speed_ == core::SimulationSpeed::X0)
        {
            accumulator_ = 0.0;
            return last_step_count_;
        }
        if (!std::isfinite(frame_dt) || frame_dt <= 0.0)
        {
            return last_step_count_;
        }

        const double scaled_dt = std::min(frame_dt, max_frame_dt) * speed_multiplier();
        accumulator_ += scaled_dt;
        if (!std::isfinite(accumulator_))
        {
            accumulator_ = 0.0;
            return last_step_count_;
        }

        constexpr double epsilon = fixed_dt * 1.0e-10;
        while (accumulator_ + epsilon >= fixed_dt && last_step_count_ < max_steps_per_frame)
        {
            accumulator_ -= fixed_dt;
            ++last_step_count_;
        }

        if (accumulator_ < 0.0 && accumulator_ > -epsilon)
        {
            accumulator_ = 0.0;
        }
        if (last_step_count_ == max_steps_per_frame && accumulator_ >= fixed_dt)
        {
            accumulator_ = std::fmod(accumulator_, fixed_dt);
        }

        if (total_step_count_ > std::numeric_limits<std::uint64_t>::max() - last_step_count_)
        {
            total_step_count_ = std::numeric_limits<std::uint64_t>::max();
        }
        else
        {
            total_step_count_ += last_step_count_;
        }
        return last_step_count_;
    }

    void reset_accumulator() noexcept
    {
        accumulator_ = 0.0;
        last_step_count_ = 0;
    }

    void set_paused(bool paused) noexcept
    {
        paused_ = paused;
        if (paused_)
        {
            reset_accumulator();
        }
    }

    bool set_speed(core::SimulationSpeed speed) noexcept
    {
        switch (speed)
        {
        case core::SimulationSpeed::X0:
        case core::SimulationSpeed::X1:
        case core::SimulationSpeed::X2:
        case core::SimulationSpeed::X4:
            speed_ = speed;
            if (speed_ == core::SimulationSpeed::X0)
            {
                reset_accumulator();
            }
            return true;
        }
        return false;
    }

    bool is_paused() const noexcept { return paused_; }
    core::SimulationSpeed speed() const noexcept { return speed_; }
    std::size_t last_step_count() const noexcept { return last_step_count_; }
    std::uint64_t total_step_count() const noexcept { return total_step_count_; }
    double accumulator() const noexcept { return accumulator_; }
    double interpolation_alpha() const noexcept { return accumulator_ / fixed_dt; }

private:
    double speed_multiplier() const noexcept
    {
        return static_cast<double>(static_cast<std::uint8_t>(speed_));
    }

    double accumulator_ = 0.0;
    std::size_t last_step_count_ = 0;
    std::uint64_t total_step_count_ = 0;
    core::SimulationSpeed speed_ = core::SimulationSpeed::X1;
    bool paused_ = false;
};
}