#pragma once

#include "core/Camera2D.h"
#include "core/HitTesting.h"
#include "gfx/VectorRenderer.h"
#include "sim/SimLoop.h"

#include <cstdint>
#include <vector>

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
    bool load_sample_city();
    bool update_viewport_size() noexcept;
    void process_events();
    void update_hover(glm::vec2 cursor_screen_px);
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
    float ui_scale_ = 1.0f;
    int resolution_index_ = 0;
    SimulationTick simulation_tick_ = nullptr;
    void* simulation_context_ = nullptr;
    std::vector<gfx::RenderNode> render_nodes_;
    std::vector<gfx::RenderEdge> render_edges_;
    std::vector<NodePick> node_picks_;
    std::vector<EdgePick> edge_picks_;
    glm::vec2 map_bounds_min_{0.0f, 0.0f};
    glm::vec2 map_bounds_max_{0.0f, 0.0f};
    Camera2D camera_;
    sim::SimLoop sim_loop_;
    gfx::VectorRenderer vector_renderer_;
    gfx::RenderOptions render_options_;
};
}
