#pragma once

#include <glm/vec2.hpp>

namespace core
{
class Camera2D
{
public:
    Camera2D() = default;

    glm::vec2 center_world() const noexcept { return center_world_; }
    float zoom() const noexcept { return zoom_; }
    float min_zoom() const noexcept { return min_zoom_; }
    float max_zoom() const noexcept { return max_zoom_; }
    glm::vec2 viewport_size_px() const noexcept { return viewport_size_px_; }

    bool set_center_world(glm::vec2 center) noexcept;
    bool set_zoom_bounds(float minimum, float maximum) noexcept;
    bool set_zoom(float zoom) noexcept;
    bool set_viewport_size(glm::vec2 size_px) noexcept;

    glm::vec2 screen_to_world(glm::vec2 screen_px) const noexcept;
    glm::vec2 world_to_screen(glm::vec2 world_pos) const noexcept;
    bool zoom_at_cursor(glm::vec2 cursor_screen_px, float zoom_factor) noexcept;
    void pan_by_screen_delta(glm::vec2 delta_px) noexcept;
    void pan_by_keyboard(glm::vec2 direction, double delta_seconds, float world_units_per_second) noexcept;

private:
    static bool is_finite(glm::vec2 value) noexcept;

    glm::vec2 center_world_{0.0f, 0.0f};
    float zoom_ = 1.0f;
    float min_zoom_ = 0.05f;
    float max_zoom_ = 20.0f;
    glm::vec2 viewport_size_px_{1280.0f, 720.0f};
};
}