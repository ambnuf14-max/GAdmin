/// GAdmin - Plugin simplifying the work of administrators on the Gambit-RP
/// Copyright (C) 2023-2026 The Contributors.
///
/// This program is free software: you can redistribute it and/or modify
/// it under the terms of the GNU General Public License as published by
/// the Free Software Foundation, either version 3 of the License, or
/// (at your option) any later version.
///
/// This program is distributed in the hope that it will be useful,
/// but WITHOUT ANY WARRANTY; without even the implied warranty of
/// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
/// GNU General Public License for more details.
///
/// You should have received a copy of the GNU General Public License
/// along with this program. If not, see <https://www.gnu.org/licenses/>.
///
/// SPDX-License-Identifier: GPL-3.0-only

#ifndef GADMIN_PLUGIN_GAME_CAMERA_H
#define GADMIN_PLUGIN_GAME_CAMERA_H

#include "plugin/types/simple.h"
#include "plugin/types/address.h"
#include "plugin/types/signatures.h"
#include <cstdint>

namespace plugin {
namespace signatures {

using lerp_fov = types::signatures::thiscall_t<void, std::uintptr_t, float, float, float, bool>;
using find_fov = types::signatures::thiscall_t<float, std::uintptr_t>;
using matrix_invert = types::signatures::stdcall_t<void, std::uintptr_t, std::uintptr_t, std::uintptr_t>;
using take_control_no_entity = types::signatures::thiscall_t<void, std::uintptr_t, const types::vector_3d*, short, int>;
using set_cam_position_for_fixed_mode = types::signatures::thiscall_t<void, std::uintptr_t, const types::vector_3d*, const types::vector_3d*>;
using restore_camera = types::signatures::thiscall_t<void, std::uintptr_t>;
using set_camera_behind_player = types::signatures::thiscall_t<void, std::uintptr_t>;

} // namespace signatures

namespace game {

/// Provides methods for in-game camera operations and accessing properties.
class camera final {
private:
    struct active_camera final {
        types::vector_3d front;
        types::vector_3d source;
    }; // struct active_camera final
    
    static inline types::address<types::vector_3d> active_camera_coordinates = 0xB6F338;
    static inline types::address<std::uintptr_t> instance_address = 0xB6F028;
    static inline types::offset<std::uint8_t> active_camera_index_offset = 0x59;
    static inline types::address<signatures::lerp_fov> lerp_fov_address = 0x50D280;
    static inline types::address<signatures::find_fov> find_fov_address = 0x50AD20;
    static inline types::address<signatures::matrix_invert> matrix_invert_address = 0x76795B;
    static inline types::address<types::vector_3d> front_camera_address = 0xB6F32C;
    static inline types::address<types::vector_3d> source_camera_address = 0xB6F338;
    static inline types::address<signatures::take_control_no_entity> take_control_no_entity_address = 0x50C8B0;
    static inline types::address<signatures::set_cam_position_for_fixed_mode> set_cam_position_for_fixed_mode_address = 0x50BEC0;
    static inline types::address<signatures::restore_camera> restore_camera_address = 0x50BAB0;
    static inline types::address<signatures::set_camera_behind_player> set_camera_behind_player_address = 0x50BD40;

    static auto get_active_camera() noexcept -> active_camera;
public:
    /// Get the FOV from the active camera.
    /// 
    /// @return Active camera's FOV.
    static auto get_fov() noexcept -> float;

    /// Linearly interpolate the field of view.
    ///
    /// @param zoom_in_factor[in]  Factor to zoom in.
    /// @param zoom_out_factor[in] Factor to zoom out.
    /// @param time_limit[in]      Time limit for the interpolation.
    /// @param ease[in]            Whether to ease the interpolation.
    static auto lerp_fov(float zoom_in_factor, float zoom_out_factor, float time_limit, bool ease) noexcept -> void;
    
    /// Get the active camera coordinates.
    /// 
    /// @return Active camera coordinates.
    static auto get_active_coordinates() noexcept -> types::vector_3d;

    /// Get the point at which the camera is looking.
    /// 
    /// @return Point at which the camera is looking.
    static auto get_active_point_at() noexcept -> types::vector_3d;

    /// Convert screen coordinates to the world coordinates (2D => 3D).
    ///
    /// @param x[in], y[in] Coordinates on the screen.
    /// @param depth[in]    Depth on the screen.
    /// @return             Converted world coordinates.
    static auto screen_to_coordinates(float x, float y, float depth) noexcept -> types::vector_3d;

    /// Take control of the camera in fixed mode, looking at the given world point.
    /// Combine with `set_fixed_position` to pin the camera at a chosen position.
    ///
    /// @param look_at[in] World point the camera should look at.
    static auto take_control_fixed(const types::vector_3d& look_at) noexcept -> void;

    /// Set the camera position used by the fixed mode (where the camera sits).
    ///
    /// @param source[in] World position to place the camera at.
    static auto set_fixed_position(const types::vector_3d& source) noexcept -> void;

    /// Restore the camera from fixed mode with a jump cut (release the takeover).
    static auto restore() noexcept -> void;

    /// Set the camera directly behind the player (the default follow camera).
    static auto set_behind_player() noexcept -> void;
private:
    static constexpr std::uintptr_t camera_size = 0x238;
}; // class camera final

} // namespace game
} // namespace plugin

#endif // GADMIN_PLUGIN_GAME_CAMERA_H
