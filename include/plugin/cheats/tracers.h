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

#ifndef GADMIN_PLUGIN_CHEATS_TRACERS_H
#define GADMIN_PLUGIN_CHEATS_TRACERS_H

#include "plugin/cheats/base.h"
#include "plugin/types/simple.h"
#include <chrono>

namespace plugin::cheats {

/// Cheat for bullet tracing functionality.
///
/// Renders all bullets' trajectories and whether they hit the target. The shots are
/// collected and enriched by the shared `server::shooting` module; this cheat is only
/// a configurable view over its records (colors, line thickness, endpoint dot, and the
/// shooter / weapon labels).
///
/// Only functions when the user is authenticated via `/alogin` and the cheat is enabled
/// in the configuration.
class tracers final : public basic_cheat {
private:
    gui::hotkey hotkey;

    /// Records captured before this point are hidden (set by the clear hotkey). The shared
    /// buffer is left intact so the shotlog window is not affected.
    std::chrono::steady_clock::time_point hidden_before;

    auto hotkey_callback(gui::hotkey& hotkey) -> void;
public:
    auto render(types::not_null<gui_initializer*> child) -> void override;
    auto register_hotkeys(types::not_null<gui::hotkey_handler*> handler) -> void override;

    explicit tracers();
}; // class tracers final : public basic_cheat

} // namespace plugin::cheats

#endif // GADMIN_PLUGIN_CHEATS_TRACERS_H
