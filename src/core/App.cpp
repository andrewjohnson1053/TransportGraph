#include "core/App.h"

#include "core/HitTesting.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>

namespace core
{
namespace
{
constexpr int initial_window_width = 1280;
constexpr int initial_window_height = 720;
constexpr std::uint64_t target_frame_duration_ns = 1'000'000'000ULL / 60ULL;
constexpr double nanoseconds_per_second = 1'000'000'000.0;
constexpr double keyboard_pan_speed = 600.0;
constexpr float zoom_per_wheel_unit = 1.1f;
constexpr float default_ui_scale = 1.25f;

bool key_is_down(const bool* keyboard_state, int key_count, SDL_Scancode key) noexcept
{
    const int index = static_cast<int>(key);
    return keyboard_state != nullptr && index >= 0 && index < key_count && keyboard_state[index];
}

struct DemoSceneView
{
    gfx::RenderScene render_scene;
    std::span<const NodePick> node_picks;
    std::span<const EdgePick> edge_picks;
};

const DemoSceneView& demo_scene()
{
    static const std::array<gfx::RenderNode, 8> nodes{{
        {NodeId{0}, {-450.0f, -250.0f}, NodeType::Intersection, 0},
        {NodeId{1}, {-150.0f, -250.0f}, NodeType::Stop, 18},
        {NodeId{2}, {150.0f, -250.0f}, NodeType::Intersection, 0},
        {NodeId{3}, {450.0f, -250.0f}, NodeType::Terminal, 42},
        {NodeId{4}, {450.0f, 100.0f}, NodeType::DepotAnchor, 0},
        {NodeId{5}, {150.0f, 100.0f}, NodeType::RegionalGateway, 0},
        {NodeId{6}, {-150.0f, 100.0f}, NodeType::Stop, 7},
        {NodeId{7}, {-450.0f, 100.0f}, NodeType::Intersection, 0}
    }};
    static const std::array<gfx::RenderEdge, 9> edges{{
        {EdgeId{0}, NodeId{0}, NodeId{1}, RoadClass::SingleLane, 1, 300.0f, 0.1f},
        {EdgeId{1}, NodeId{1}, NodeId{2}, RoadClass::TwoLane, 2, 300.0f, 0.45f},
        {EdgeId{2}, NodeId{2}, NodeId{3}, RoadClass::MultiLane, 4, 300.0f, 0.76f},
        {EdgeId{3}, NodeId{3}, NodeId{4}, RoadClass::Highway, 6, 350.0f, 1.15f},
        {EdgeId{4}, NodeId{4}, NodeId{5}, RoadClass::TwoLane, 2, 300.0f, 0.2f},
        {EdgeId{5}, NodeId{5}, NodeId{6}, RoadClass::MultiLane, 4, 300.0f, 0.55f},
        {EdgeId{6}, NodeId{6}, NodeId{7}, RoadClass::SingleLane, 1, 300.0f, 0.15f},
        {EdgeId{7}, NodeId{7}, NodeId{0}, RoadClass::TwoLane, 2, 350.0f, 0.3f},
        {EdgeId{8}, NodeId{1}, NodeId{6}, RoadClass::SingleLane, 1, 350.0f, 0.85f}
    }};
    static const std::array<NodePick, 8> node_picks{{
        {NodeId{0}, {-450.0f, -250.0f}}, {NodeId{1}, {-150.0f, -250.0f}},
        {NodeId{2}, {150.0f, -250.0f}}, {NodeId{3}, {450.0f, -250.0f}},
        {NodeId{4}, {450.0f, 100.0f}}, {NodeId{5}, {150.0f, 100.0f}},
        {NodeId{6}, {-150.0f, 100.0f}}, {NodeId{7}, {-450.0f, 100.0f}}
    }};
    static const std::array<EdgePick, 9> edge_picks{{
        {EdgeId{0}, {-450.0f, -250.0f}, {-150.0f, -250.0f}},
        {EdgeId{1}, {-150.0f, -250.0f}, {150.0f, -250.0f}},
        {EdgeId{2}, {150.0f, -250.0f}, {450.0f, -250.0f}},
        {EdgeId{3}, {450.0f, -250.0f}, {450.0f, 100.0f}},
        {EdgeId{4}, {450.0f, 100.0f}, {150.0f, 100.0f}},
        {EdgeId{5}, {150.0f, 100.0f}, {-150.0f, 100.0f}},
        {EdgeId{6}, {-150.0f, 100.0f}, {-450.0f, 100.0f}},
        {EdgeId{7}, {-450.0f, 100.0f}, {-450.0f, -250.0f}},
        {EdgeId{8}, {-150.0f, -250.0f}, {-150.0f, 100.0f}}
    }};
    static const std::array<core::EdgeId, 4> rival_edges{{
        core::EdgeId{1}, core::EdgeId{2}, core::EdgeId{3}, core::EdgeId{4}
    }};
    static const std::array<gfx::RenderRoute, 1> routes{{
        {core::CompanyId{1}, std::span<const core::EdgeId>{rival_edges}}
    }};
    static const std::array<gfx::RenderVehicle, 4> vehicles{{
        {VehicleId{0}, CompanyId{1}, EdgeId{1}, 115.0f},
        {VehicleId{1}, CompanyId{1}, EdgeId{2}, 185.0f},
        {VehicleId{2}, CompanyId{2}, EdgeId{5}, 95.0f},
        {VehicleId{3}, CompanyId{1}, EdgeId{3}, 210.0f}
    }};
    static const gfx::RenderScene scene{
        std::span<const gfx::RenderNode>{nodes}, std::span<const gfx::RenderEdge>{edges},
        std::span<const gfx::RenderRoute>{routes}, std::span<const gfx::RenderVehicle>{vehicles}
    };
    static const DemoSceneView view{
        scene, std::span<const NodePick>{node_picks}, std::span<const EdgePick>{edge_picks}
    };
    return view;
}
}

App::~App()
{
    shutdown();
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

    window_ = SDL_CreateWindow(
        "Transport Graph",
        initial_window_width,
        initial_window_height,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_MAXIMIZED);
    if (window_ == nullptr)
    {
        std::cerr << "SDL window creation failed: " << SDL_GetError() << '\n';
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
    style.ScaleAllSizes(default_ui_scale);
    style.FontScaleMain = default_ui_scale;
    ui_scale_ = default_ui_scale;

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
    const DemoSceneView& view = demo_scene();
    render_options_.hovered_node = find_nearest_node(view.node_picks, camera_, cursor_screen_px, 11.0f);
    render_options_.hovered_edge = render_options_.hovered_node
        ? std::nullopt
        : find_nearest_edge(view.edge_picks, camera_, cursor_screen_px, 8.0f);
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
    std::string render_error;
    if (!vector_renderer_.render(demo_scene().render_scene, camera_, render_options_, render_error))
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
    ImGui::SetNextWindowSize({520.0f, 420.0f}, ImGuiCond_FirstUseEver);
    ImGui::Begin("Transport Graph");
    ImGui::TextUnformatted("Schematic city preview");

    float requested_ui_scale = ui_scale_;
    if (ImGui::SliderFloat("UI scale", &requested_ui_scale, 0.8f, 2.0f, "%.2fx"))
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(requested_ui_scale / ui_scale_);
        style.FontScaleMain = requested_ui_scale;
        ui_scale_ = requested_ui_scale;
    }

    ImGui::Checkbox("Traffic heatmap", &render_options_.show_heatmap);
    int rival_selection = render_options_.selected_rival_id ? 1 : 0;
    constexpr const char* rival_labels[] = {"No rival focus", "Focus company 1"};
    if (ImGui::Combo("Rival focus", &rival_selection, rival_labels, 2))
    {
        render_options_.selected_rival_id = rival_selection == 0
            ? std::nullopt
            : std::optional<CompanyId>{CompanyId{1}};
    }

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
