#include "core/App.h"

#include "core/HitTesting.h"
#include "sim/Graph.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>

namespace core
{
namespace
{
constexpr int initial_window_width = 1280;
constexpr int initial_window_height = 720;
constexpr int development_window_width = 1920;
constexpr int development_window_height = 1080;
constexpr int display_safety_margin_percent = 90;
constexpr std::uint64_t target_frame_duration_ns = 1'000'000'000ULL / 60ULL;
constexpr double nanoseconds_per_second = 1'000'000'000.0;
constexpr double keyboard_pan_speed = 600.0;
constexpr float zoom_per_wheel_unit = 1.1f;
constexpr float default_ui_scale = 1.0f;
constexpr float development_ui_scale = 2.0f;
constexpr float development_visual_scale = 2.0f;
constexpr float minimum_display_scale = 0.5f;
constexpr float maximum_display_scale = 5.0f;
constexpr std::string_view development_display_environment = "TRANSPORT_GRAPH_DEV_DISPLAY";

struct ResolutionPreset
{
    const char* label;
    int width;
    int height;
};

constexpr ResolutionPreset resolution_presets[] = {
    {"1280 x 720", 1280, 720},
    {"1920 x 1080", 1920, 1080},
    {"2560 x 1440", 2560, 1440},
    {"3840 x 2160", 3840, 2160},
};
constexpr const char* resolution_labels[] = {
    "1280 x 720",
    "1920 x 1080",
    "2560 x 1440",
    "3840 x 2160",
};

bool development_display_enabled() noexcept
{
    const char* value = SDL_getenv(development_display_environment.data());
    return value != nullptr && std::string_view{value} == "1";
}

bool key_is_down(const bool* keyboard_state, int key_count, SDL_Scancode key) noexcept
{
    const int index = static_cast<int>(key);
    return keyboard_state != nullptr && index >= 0 && index < key_count && keyboard_state[index];
}

}

App::~App()
{
    shutdown();
}

bool App::load_sample_city()
{
    const char* base_path = SDL_GetBasePath();
    if (base_path == nullptr)
    {
        std::cerr << "Unable to determine executable directory: " << SDL_GetError() << '\n';
        return false;
    }
    const std::filesystem::path map_path =
        std::filesystem::path{base_path} / "assets" / "maps" / "sample_city.json";

    sim::Graph graph;
    std::string error;
    if (!graph.load_from_json(map_path, &error))
    {
        std::cerr << "Sample city map loading failed: " << error << '\n';
        return false;
    }

    map_bounds_min_ = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    map_bounds_max_ = {std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
    render_nodes_.reserve(graph.nodes().size());
    node_picks_.reserve(graph.nodes().size());
    for (const sim::Node& node : graph.nodes())
    {
        render_nodes_.push_back({node.id, node.pos_world, node.type, node.waiting_passengers});
        node_picks_.push_back({node.id, node.pos_world});
        map_bounds_min_.x = std::min(map_bounds_min_.x, node.pos_world.x);
        map_bounds_min_.y = std::min(map_bounds_min_.y, node.pos_world.y);
        map_bounds_max_.x = std::max(map_bounds_max_.x, node.pos_world.x);
        map_bounds_max_.y = std::max(map_bounds_max_.y, node.pos_world.y);
    }

    render_edges_.reserve(graph.edges().size());
    edge_picks_.reserve(graph.edges().size());
    for (const sim::Edge& edge : graph.edges())
    {
        const sim::Node* from = graph.find_node(edge.from_node);
        const sim::Node* to = graph.find_node(edge.to_node);
        if (from == nullptr || to == nullptr)
        {
            std::cerr << "Sample city edge references a missing endpoint.\n";
            return false;
        }
        render_edges_.push_back({edge.id, edge.from_node, edge.to_node, edge.road_class,
                                 edge.base_lanes, edge.length_m, edge.congestion_ratio});
        edge_picks_.push_back({edge.id, from->pos_world, to->pos_world});
    }
    return !render_nodes_.empty() && !render_edges_.empty();
}

bool App::initialize()
{
    if (is_initialized())
    {
        return true;
    }
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == false)
    {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << '\n';
        shutdown();
        return false;
    }
    sdl_initialized_ = true;
    if (!load_sample_city())
    {
        shutdown();
        return false;
    }

    const SDL_DisplayID display = SDL_GetPrimaryDisplay();
    if (display == 0)
    {
        std::cerr << "Unable to determine the primary display: " << SDL_GetError() << '\n';
        shutdown();
        return false;
    }
    SDL_Rect usable_bounds{};
    if (!SDL_GetDisplayUsableBounds(display, &usable_bounds) ||
        usable_bounds.w <= 0 || usable_bounds.h <= 0)
    {
        std::cerr << "Unable to determine usable primary display bounds: " << SDL_GetError() << '\n';
        shutdown();
        return false;
    }

    const bool dev_display = development_display_enabled();
    resolution_index_ = dev_display ? 1 : 0;
    const int requested_width = dev_display ? development_window_width : initial_window_width;
    const int requested_height = dev_display ? development_window_height : initial_window_height;
    const int max_width = std::max(1, usable_bounds.w * display_safety_margin_percent / 100);
    const int max_height = std::max(1, usable_bounds.h * display_safety_margin_percent / 100);
    const int window_width = std::min(requested_width, max_width);
    const int window_height = std::min(requested_height, max_height);

    window_ = SDL_CreateWindow("Transport Graph", window_width, window_height, SDL_WINDOW_RESIZABLE);
    if (window_ == nullptr)
    {
        std::cerr << "SDL window creation failed: " << SDL_GetError() << '\n';
        shutdown();
        return false;
    }
    if (!SDL_SetWindowPosition(window_, SDL_WINDOWPOS_CENTERED_DISPLAY(display),
                               SDL_WINDOWPOS_CENTERED_DISPLAY(display)))
    {
        std::cerr << "SDL window positioning failed: " << SDL_GetError() << '\n';
        shutdown();
        return false;
    }

    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (renderer_ == nullptr)
    {
        std::cerr << "SDL renderer creation failed: " << SDL_GetError() << '\n';
        shutdown();
        return false;
    }
    vector_renderer_.set_renderer(renderer_);
    vsync_enabled_ = SDL_SetRenderVSync(renderer_, 1);
    if (!vsync_enabled_)
    {
        std::cerr << "SDL renderer vsync unavailable; using frame delay: " << SDL_GetError() << '\n';
    }

    if (!update_viewport_size())
    {
        shutdown();
        return false;
    }
    const glm::vec2 map_size = map_bounds_max_ - map_bounds_min_;
    const glm::vec2 viewport = camera_.viewport_size_px();
    const float fit_zoom = 0.8f * std::min(viewport.x / map_size.x, viewport.y / map_size.y);
    if (!camera_.set_center_world((map_bounds_min_ + map_bounds_max_) * 0.5f) ||
        !camera_.set_zoom(fit_zoom))
    {
        std::cerr << "Unable to fit the sample city into the initial view.\n";
        shutdown();
        return false;
    }

    IMGUI_CHECKVERSION();
    if (ImGui::CreateContext() == nullptr)
    {
        std::cerr << "Dear ImGui context creation failed.\n";
        shutdown();
        return false;
    }
    imgui_context_created_ = true;
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    ui_scale_ = dev_display ? development_ui_scale : default_ui_scale;
    style.ScaleAllSizes(ui_scale_);
    style.FontScaleMain = ui_scale_;
    render_options_.visual_scale = dev_display ? development_visual_scale : 1.0f;

    if (!ImGui_ImplSDL3_InitForSDLRenderer(window_, renderer_))
    {
        std::cerr << "Dear ImGui SDL3 backend initialization failed.\n";
        shutdown();
        return false;
    }
    imgui_platform_initialized_ = true;

    if (!ImGui_ImplSDLRenderer3_Init(renderer_))
    {
        std::cerr << "Dear ImGui SDL renderer backend initialization failed.\n";
        shutdown();
        return false;
    }
    imgui_renderer_initialized_ = true;
    runtime_error_ = false;
    running_ = true;
    return true;
}

int App::run()
{
    if (!initialize())
    {
        return 1;
    }

    std::uint64_t previous_frame_start_ns = SDL_GetTicksNS();
    while (running_)
    {
        const std::uint64_t frame_start_ns = SDL_GetTicksNS();
        const double frame_dt = static_cast<double>(frame_start_ns - previous_frame_start_ns) /
                                nanoseconds_per_second;
        previous_frame_start_ns = frame_start_ns;

        process_events();
        if (!running_)
        {
            break;
        }
        update_keyboard_pan(frame_dt);

        const std::size_t step_count = sim_loop_.advance(frame_dt);
        if (simulation_tick_ != nullptr)
        {
            for (std::size_t step = 0; step < step_count; ++step)
            {
                simulation_tick_(sim::SimLoop::fixed_dt, simulation_context_);
            }
        }

        if (!render_frame())
        {
            runtime_error_ = true;
            shutdown();
            return 1;
        }
        if (!vsync_enabled_)
        {
            const std::uint64_t elapsed_ns = SDL_GetTicksNS() - frame_start_ns;
            if (elapsed_ns < target_frame_duration_ns)
            {
                SDL_DelayNS(target_frame_duration_ns - elapsed_ns);
            }
        }
    }
    return runtime_error_ ? 1 : 0;
}

void App::shutdown() noexcept
{
    running_ = false;
    pan_drag_active_ = false;
    if (imgui_renderer_initialized_)
    {
        ImGui_ImplSDLRenderer3_Shutdown();
        imgui_renderer_initialized_ = false;
    }
    if (imgui_platform_initialized_)
    {
        ImGui_ImplSDL3_Shutdown();
        imgui_platform_initialized_ = false;
    }
    if (imgui_context_created_)
    {
        ImGui::DestroyContext();
        imgui_context_created_ = false;
    }
    vector_renderer_.set_renderer(nullptr);
    if (renderer_ != nullptr)
    {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_ != nullptr)
    {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    if (sdl_initialized_)
    {
        SDL_Quit();
        sdl_initialized_ = false;
    }
}

void App::set_simulation_tick_callback(SimulationTick callback, void* context) noexcept
{
    simulation_tick_ = callback;
    simulation_context_ = context;
}

bool App::update_viewport_size() noexcept
{
    int width = 0;
    int height = 0;
    if (renderer_ == nullptr || !SDL_GetRenderOutputSize(renderer_, &width, &height))
    {
        std::cerr << "SDL renderer output size query failed: " << SDL_GetError() << '\n';
        return false;
    }
    if (width <= 0 || height <= 0)
    {
        return true;
    }
    return camera_.set_viewport_size({static_cast<float>(width), static_cast<float>(height)});
}

void App::process_events()
{
    SDL_Event event{};
    while (SDL_PollEvent(&event))
    {
        if (imgui_platform_initialized_)
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
        }

        switch (event.type)
        {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            running_ = false;
            break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            if (!update_viewport_size())
            {
                runtime_error_ = true;
                running_ = false;
            }
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            pan_drag_active_ = false;
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (!ImGui::GetIO().WantCaptureMouse && event.button.button == SDL_BUTTON_LEFT)
            {
                int window_width = 0;
                int window_height = 0;
                int output_width = 0;
                int output_height = 0;
                if (SDL_GetWindowSize(window_, &window_width, &window_height) &&
                    SDL_GetRenderOutputSize(renderer_, &output_width, &output_height) &&
                    window_width > 0 && window_height > 0)
                {
                    const glm::vec2 cursor{
                        event.button.x * static_cast<float>(output_width) / static_cast<float>(window_width),
                        event.button.y * static_cast<float>(output_height) / static_cast<float>(window_height)
                    };
                    update_hover(cursor);
                    if (render_options_.hovered_node)
                    {
                        render_options_.selected_node = render_options_.hovered_node;
                    }
                    else
                    {
                        render_options_.selected_node.reset();
                    }
                }
            }
            else if ((event.button.button == SDL_BUTTON_MIDDLE || event.button.button == SDL_BUTTON_RIGHT) &&
                     !ImGui::GetIO().WantCaptureMouse)
            {
                pan_drag_active_ = true;
            }
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            if (event.button.button == SDL_BUTTON_MIDDLE || event.button.button == SDL_BUTTON_RIGHT)
            {
                pan_drag_active_ = false;
            }
            break;
        case SDL_EVENT_MOUSE_MOTION:
            if (!ImGui::GetIO().WantCaptureMouse)
            {
                int window_width = 0;
                int window_height = 0;
                int output_width = 0;
                int output_height = 0;
                if (SDL_GetWindowSize(window_, &window_width, &window_height) &&
                    SDL_GetRenderOutputSize(renderer_, &output_width, &output_height) &&
                    window_width > 0 && window_height > 0)
                {
                    const float scale_x = static_cast<float>(output_width) / static_cast<float>(window_width);
                    const float scale_y = static_cast<float>(output_height) / static_cast<float>(window_height);
                    update_hover({event.motion.x * scale_x, event.motion.y * scale_y});
                    if (pan_drag_active_)
                    {
                        camera_.pan_by_screen_delta({event.motion.xrel * scale_x, event.motion.yrel * scale_y});
                    }
                }
            }
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            if (!ImGui::GetIO().WantCaptureMouse)
            {
                const float direction = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1.0f : 1.0f;
                const float factor = std::pow(zoom_per_wheel_unit, event.wheel.y * direction);
                int window_width = 0;
                int window_height = 0;
                int output_width = 0;
                int output_height = 0;
                if (std::isfinite(factor) && SDL_GetWindowSize(window_, &window_width, &window_height) &&
                    SDL_GetRenderOutputSize(renderer_, &output_width, &output_height) &&
                    window_width > 0 && window_height > 0)
                {
                    const float scale_x = static_cast<float>(output_width) / static_cast<float>(window_width);
                    const float scale_y = static_cast<float>(output_height) / static_cast<float>(window_height);
                    camera_.zoom_at_cursor(
                        {event.wheel.mouse_x * scale_x, event.wheel.mouse_y * scale_y},
                        factor);
                }
            }
            break;
        case SDL_EVENT_KEY_DOWN:
            if (!event.key.repeat && !ImGui::GetIO().WantCaptureKeyboard)
            {
                switch (event.key.key)
                {
                case SDLK_ESCAPE:
                    running_ = false;
                    break;
                case SDLK_SPACE:
                    sim_loop_.set_paused(!sim_loop_.is_paused());
                    break;
                case SDLK_0:
                    sim_loop_.set_speed(SimulationSpeed::X0);
                    break;
                case SDLK_1:
                    sim_loop_.set_speed(SimulationSpeed::X1);
                    break;
                case SDLK_2:
                    sim_loop_.set_speed(SimulationSpeed::X2);
                    break;
                case SDLK_4:
                    sim_loop_.set_speed(SimulationSpeed::X4);
                    break;
                default:
                    break;
                }
            }
            break;
        default:
            break;
        }
    }
}

void App::update_hover(glm::vec2 cursor_screen_px)
{
    render_options_.hovered_node = find_nearest_node(
        node_picks_, camera_, cursor_screen_px, 11.0f * render_options_.visual_scale);
    render_options_.hovered_edge = render_options_.hovered_node
        ? std::nullopt
        : find_nearest_edge(edge_picks_, camera_, cursor_screen_px,
                            8.0f * render_options_.visual_scale);
}

void App::update_keyboard_pan(double frame_dt)
{
    if (ImGui::GetIO().WantCaptureKeyboard)
    {
        return;
    }

    int key_count = 0;
    const bool* keyboard_state = SDL_GetKeyboardState(&key_count);
    glm::vec2 direction{0.0f, 0.0f};
    if (key_is_down(keyboard_state, key_count, SDL_SCANCODE_LEFT) ||
        key_is_down(keyboard_state, key_count, SDL_SCANCODE_A))
    {
        direction.x -= 1.0f;
    }
    if (key_is_down(keyboard_state, key_count, SDL_SCANCODE_RIGHT) ||
        key_is_down(keyboard_state, key_count, SDL_SCANCODE_D))
    {
        direction.x += 1.0f;
    }
    if (key_is_down(keyboard_state, key_count, SDL_SCANCODE_UP) ||
        key_is_down(keyboard_state, key_count, SDL_SCANCODE_W))
    {
        direction.y -= 1.0f;
    }
    if (key_is_down(keyboard_state, key_count, SDL_SCANCODE_DOWN) ||
        key_is_down(keyboard_state, key_count, SDL_SCANCODE_S))
    {
        direction.y += 1.0f;
    }
    camera_.pan_by_keyboard(direction, std::min(frame_dt, sim::SimLoop::max_frame_dt),
                            static_cast<float>(keyboard_pan_speed));
}

bool App::render_frame()
{
    if (!update_viewport_size())
    {
        return false;
    }
    int output_width = 0;
    int output_height = 0;
    if (!SDL_GetRenderOutputSize(renderer_, &output_width, &output_height))
    {
        std::cerr << "SDL renderer output size query failed: " << SDL_GetError() << '\n';
        return false;
    }
    if (output_width <= 0 || output_height <= 0)
    {
        SDL_Delay(16);
        return true;
    }

    if (!SDL_SetRenderDrawColor(renderer_, 24, 28, 36, 255) || !SDL_RenderClear(renderer_))
    {
        std::cerr << "SDL frame clear failed: " << SDL_GetError() << '\n';
        return false;
    }
    const gfx::RenderScene scene{render_nodes_, render_edges_, {}, {}};
    std::string render_error;
    if (!vector_renderer_.render(scene, camera_, render_options_, render_error))
    {
        std::cerr << "Vector rendering failed: " << render_error << '\n';
        return false;
    }

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    draw_controls();
    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer_);
    if (!SDL_RenderPresent(renderer_))
    {
        std::cerr << "SDL frame presentation failed: " << SDL_GetError() << '\n';
        return false;
    }
    return true;
}

void App::draw_controls()
{
    ImGui::SetNextWindowSize({520.0f * ui_scale_, 420.0f * ui_scale_}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Transport Graph");
    ImGui::Text("Sample city: %zu nodes, %zu roads", render_nodes_.size(), render_edges_.size());

    if (ImGui::Combo("Resolution", &resolution_index_, resolution_labels,
                     static_cast<int>(std::size(resolution_labels))))
    {
        const ResolutionPreset& preset = resolution_presets[resolution_index_];
        if (!SDL_SetWindowSize(window_, preset.width, preset.height))
        {
            std::cerr << "Unable to set window resolution: " << SDL_GetError() << '\n';
        }
    }
    int window_width = 0;
    int window_height = 0;
    if (SDL_GetWindowSize(window_, &window_width, &window_height))
    {
        ImGui::Text("Window size: %d x %d", window_width, window_height);
    }

    float requested_scale = ui_scale_;
    if (ImGui::SliderFloat("Display scale", &requested_scale,
                           minimum_display_scale, maximum_display_scale, "%.2fx"))
    {
        requested_scale = std::clamp(requested_scale, minimum_display_scale, maximum_display_scale);
        ImGuiStyle& style = ImGui::GetStyle();
        style.FontScaleMain = requested_scale;
        ui_scale_ = requested_scale;
        render_options_.visual_scale = requested_scale;
    }

    ImGui::Checkbox("Traffic heatmap", &render_options_.show_heatmap);

    bool paused = sim_loop_.is_paused();
    if (ImGui::Checkbox("Paused", &paused))
    {
        sim_loop_.set_paused(paused);
    }

    int speed = static_cast<int>(static_cast<std::uint8_t>(sim_loop_.speed()));
    constexpr const char* speed_labels[] = {"0x", "1x", "2x", "4x"};
    constexpr int speed_values[] = {0, 1, 2, 4};
    int selected_speed = 0;
    for (int index = 0; index < 4; ++index)
    {
        if (speed == speed_values[index])
        {
            selected_speed = index;
            break;
        }
    }
    if (ImGui::Combo("Simulation speed", &selected_speed, speed_labels, 4))
    {
        sim_loop_.set_speed(static_cast<SimulationSpeed>(speed_values[selected_speed]));
    }

    ImGui::Text("Fixed ticks: %llu", static_cast<unsigned long long>(sim_loop_.total_step_count()));
    ImGui::Text("Camera zoom: %.2f", camera_.zoom());
    if (render_options_.selected_node)
    {
        ImGui::Text("Selected node: %u", render_options_.selected_node->value());
    }
    else if (render_options_.hovered_edge)
    {
        ImGui::Text("Hovered edge: %u", render_options_.hovered_edge->value());
    }
    ImGui::TextUnformatted("Select nodes with left click; pan with middle/right drag or WASD");
    ImGui::TextUnformatted("Zoom: mouse wheel | Pause: Space | Speed: 0, 1, 2, 4");
    ImGui::End();
}
}
