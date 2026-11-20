#include "core/Camera2D.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace core
{
bool Camera2D::is_finite(glm::vec2 value) noexcept
{
    return std::isfinite(value.x) && std::isfinite(value.y);
}

bool Camera2D::set_center_world(glm::vec2 center) noexcept
{
    if (!is_finite(center))
    {
        return false;
    }
    center_world_ = center;
    return true;
}

bool Camera2D::set_zoom_bounds(float minimum, float maximum) noexcept
{
    if (!std::isfinite(minimum) || !std::isfinite(maximum) || minimum <= 0.0f || minimum > maximum)
    {
        return false;
    }
    min_zoom_ = minimum;
    max_zoom_ = maximum;
    zoom_ = std::clamp(zoom_, min_zoom_, max_zoom_);
    return true;
}

bool Camera2D::set_zoom(float zoom) noexcept
{
    if (!std::isfinite(zoom) || zoom <= 0.0f)
    {
        return false;
    }
    zoom_ = std::clamp(zoom, min_zoom_, max_zoom_);
    return true;
}

bool Camera2D::set_viewport_size(glm::vec2 size_px) noexcept
{
    if (!is_finite(size_px) || size_px.x <= 0.0f || size_px.y <= 0.0f)
    {
        return false;
    }
    viewport_size_px_ = size_px;
    return true;
}

glm::vec2 Camera2D::screen_to_world(glm::vec2 screen_px) const noexcept
{
    if (!is_finite(screen_px))
    {
        return center_world_;
    }
    const glm::vec2 world = center_world_ + (screen_px - viewport_size_px_ * 0.5f) / zoom_;
    return is_finite(world) ? world : center_world_;
}

glm::vec2 Camera2D::world_to_screen(glm::vec2 world_pos) const noexcept
{
    if (!is_finite(world_pos))
    {
        return viewport_size_px_ * 0.5f;
    }
    const glm::vec2 screen = (world_pos - center_world_) * zoom_ + viewport_size_px_ * 0.5f;
    return is_finite(screen) ? screen : viewport_size_px_ * 0.5f;
}

bool Camera2D::zoom_at_cursor(glm::vec2 cursor_screen_px, float zoom_factor) noexcept
{
    if (!is_finite(cursor_screen_px) || !std::isfinite(zoom_factor) || zoom_factor <= 0.0f)
    {
        return false;
    }

    const double proposed_zoom = static_cast<double>(zoom_) * zoom_factor;
    if (!std::isfinite(proposed_zoom))
    {
        return false;
    }
    const float bounded_zoom = static_cast<float>(std::clamp(
        proposed_zoom,
        static_cast<double>(min_zoom_),
        static_cast<double>(max_zoom_)));
    const double offset_x = static_cast<double>(cursor_screen_px.x) - viewport_size_px_.x * 0.5;
    const double offset_y = static_cast<double>(cursor_screen_px.y) - viewport_size_px_.y * 0.5;
    const double adjusted_x = static_cast<double>(center_world_.x) + offset_x / zoom_ - offset_x / bounded_zoom;
    const double adjusted_y = static_cast<double>(center_world_.y) + offset_y / zoom_ - offset_y / bounded_zoom;
    if (!std::isfinite(adjusted_x) || !std::isfinite(adjusted_y) ||
        std::abs(adjusted_x) > std::numeric_limits<float>::max() ||
        std::abs(adjusted_y) > std::numeric_limits<float>::max())
    {
        return false;
    }

    zoom_ = bounded_zoom;
    center_world_ = {static_cast<float>(adjusted_x), static_cast<float>(adjusted_y)};
    return true;
}

void Camera2D::pan_by_screen_delta(glm::vec2 delta_px) noexcept
{
    if (!is_finite(delta_px))
    {
        return;
    }
    const glm::vec2 adjusted_center = center_world_ - delta_px / zoom_;
    if (is_finite(adjusted_center))
    {
        center_world_ = adjusted_center;
    }
}

void Camera2D::pan_by_keyboard(glm::vec2 direction, double delta_seconds, float world_units_per_second) noexcept
{
    if (!is_finite(direction) || !std::isfinite(delta_seconds) || delta_seconds <= 0.0 ||
        !std::isfinite(world_units_per_second) || world_units_per_second <= 0.0f)
    {
        return;
    }

    const double magnitude = std::hypot(static_cast<double>(direction.x), static_cast<double>(direction.y));
    if (!std::isfinite(magnitude) || magnitude <= std::numeric_limits<double>::epsilon())
    {
        return;
    }
    const double distance = delta_seconds * world_units_per_second;
    const double scale = distance / magnitude;
    const double x = static_cast<double>(center_world_.x) + direction.x * scale;
    const double y = static_cast<double>(center_world_.y) + direction.y * scale;
    if (std::isfinite(x) && std::isfinite(y) &&
        std::abs(x) <= std::numeric_limits<float>::max() && std::abs(y) <= std::numeric_limits<float>::max())
    {
        center_world_ = {static_cast<float>(x), static_cast<float>(y)};
    }
}
}