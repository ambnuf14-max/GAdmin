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

#ifndef GADMIN_PLUGIN_GUI_WINDOWS_SHOOTING_H
#define GADMIN_PLUGIN_GUI_WINDOWS_SHOOTING_H

#include "plugin/gui/base/window.h"
#include "plugin/gui/gui.h"
#include "plugin/gui/hotkey.h"
#include "plugin/gui/windows/main/widgets/shooting_stats.h"
#include "plugin/types/not_null.h"
#include "plugin/types/simple.h"

namespace plugin::gui::windows {

/// Standalone shooting menu window.
///
/// Hosts the same shooting panel as the logs frame (shot log, accuracy dashboard and
/// per-player cards) in a free-floating window that can be toggled by a hotkey and used
/// outside spectator mode. Reads from the shared `server::shooting` module.
class shooting final : public window {
private:
    bool opened = false;
    gui::hotkey toggle_hotkey;
    main::widgets::shooting_stats panel;

    auto toggle_hotkey_callback(gui::hotkey& hotkey) -> void;
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
    explicit shooting(types::not_null<gui_initializer*> child);
}; // class shooting final : public window

} // namespace plugin::gui::windows

inline auto plugin::gui::windows::shooting::get_id() const -> types::zstring_t {
    return "windows::shooting";
}

inline auto plugin::gui::windows::shooting::get_name() const -> types::zstring_t {
    return "Меню стрельбы";
}

#endif // GADMIN_PLUGIN_GUI_WINDOWS_SHOOTING_H
