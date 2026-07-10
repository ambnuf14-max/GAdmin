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
/// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
/// GNU General Public License for more details.
///
/// You should have received a copy of the GNU General Public License
/// along with this program.  If not, see <https://www.gnu.org/licenses/>.
///
/// SPDX-License-Identifier: GPL-3.0-only

#ifndef GADMIN_PLUGIN_GUI_WINDOWS_SPECTATOR_SHOTLOG_H
#define GADMIN_PLUGIN_GUI_WINDOWS_SPECTATOR_SHOTLOG_H

#include "plugin/gui/base/window.h"
#include "plugin/gui/gui.h"
#include "plugin/types/not_null.h"
#include "plugin/types/simple.h"
#include <imgui.h>

namespace plugin::gui::windows {

/// Compact spectator shooting-log overlay.
///
/// Shown only while spectating a player (`/sp`); displays the spectated player's live
/// accuracy and a short feed of their most recent shots (weapon, target/miss, distance),
/// read from the shared `server::shooting` module.
class spectator_shotlog final : public window {
private:
    static constexpr float font_size = 15;

    ImFont* bold_font = nullptr;
    ImFont* regular_font = nullptr;
public:
    inline auto get_id() const -> types::zstring_t override;
    inline auto get_name() const -> types::zstring_t override;

    auto render() -> void override;

    /// Create instance of the current window.
    ///
    /// @param child[in] Valid pointer to the GUI initializer.
    /// @return          Unique pointer to window.
    static auto create(types::not_null<gui_initializer*> child) noexcept -> window_ptr_t;

    /// Construct the window.
    ///
    /// @param child[in] Valid pointer to the GUI initializer.
    explicit spectator_shotlog(types::not_null<gui_initializer*> child)
        : window(child, get_id()),
          bold_font(child->fonts->bold),
          regular_font(child->fonts->regular) {}
}; // class spectator_shotlog final : public window

} // namespace plugin::gui::windows

inline auto plugin::gui::windows::spectator_shotlog::get_id() const -> types::zstring_t {
    return "windows::spectator_shotlog";
}

inline auto plugin::gui::windows::spectator_shotlog::get_name() const -> types::zstring_t {
    return "Лог стрельбы в /sp";
}

#endif // GADMIN_PLUGIN_GUI_WINDOWS_SPECTATOR_SHOTLOG_H
