#pragma once

#include "core/Camera2D.h"
#include "sim/SimLoop.h"

#include <cstdint>

struct SDL_Renderer;
struct SDL_Window;

namespace core
{
class App
{
public:
    using SimulationTick = void (*)(double fixed_dt, void* context);

    App() = default;
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    bool initialize();
    int run();
    void shutdown() noexcept;

    bool is_initialized() const noexcept { return window_ != nullptr && renderer_ != nullptr; }
    void set_simulation_tick_callback(SimulationTick callback, void* context = nullptr) noexcept;

    Camera2D& camera() noexcept { return camera_; }
    const Camera2D& camera() const noexcept { return camera_; }
    sim::SimLoop& sim_loop() noexcept { return sim_loop_; }
    const sim::SimLoop& sim_loop() const noexcept { return sim_loop_; }

private:
    bool update_viewport_size() noexcept;
    void process_events();
    void update_keyboard_pan(double frame_dt);
    bool render_frame();
    void draw_controls();

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    bool sdl_initialized_ = false;
    bool imgui_context_created_ = false;
    bool imgui_platform_initialized_ = false;
    bool imgui_renderer_initialized_ = false;
    bool vsync_enabled_ = false;
    bool running_ = false;
    bool runtime_error_ = false;
    bool pan_drag_active_ = false;
    SimulationTick simulation_tick_ = nullptr;
    void* simulation_context_ = nullptr;
    Camera2D camera_;
    sim::SimLoop sim_loop_;
};
}
