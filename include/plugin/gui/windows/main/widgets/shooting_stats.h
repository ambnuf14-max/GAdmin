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

#ifndef GADMIN_PLUGIN_GUI_WINDOWS_MAIN_WIDGETS_SHOOTING_STATS_H
#define GADMIN_PLUGIN_GUI_WINDOWS_MAIN_WIDGETS_SHOOTING_STATS_H

#include "plugin/gui/widgets/search.h"
#include <cstdint>
#include <optional>

namespace plugin::gui::windows::main::widgets {

/// Widget that renders the shooting views inside the logs frame.
///
/// Reads from the shared `server::shooting` module and provides a sortable shot log,
/// a per-player accuracy dashboard, and a drill-down card for an individual player
/// (output, input, weapons/distance and a feed of their shots).
class shooting_stats final {
public:
    /// Payload marker for the single "Стрельба" entry in the logs submenu.
    struct entry_tag final {}; // struct entry_tag final
private:
    /// The internal shooting views, switched by the in-panel tab bar.
    enum class view : std::uint8_t {
        log,       ///< Sortable log of all shots.
        dashboard  ///< Per-player accuracy dashboard with drill-down cards.
    }; // enum class view : std::uint8_t

    gui::widgets::search log_search = gui::widgets::search("frames::logs::shooting::log_search");
    gui::widgets::search dashboard_search = gui::widgets::search("frames::logs::shooting::dashboard_search");

    std::optional<std::uint16_t> selected_player;

    auto render_log() -> void;
    auto render_dashboard() -> void;
    auto render_player_card(std::uint16_t id) -> void;
public:
    /// Render the shooting panel: a tab bar switching between the shot log and the
    /// per-player statistics dashboard (with drill-down player cards). The widget is
    /// self-contained — it reads from `server::shooting` and uses ImGui directly, so it
    /// can be hosted both in the logs frame and in a standalone window.
    auto render() -> void;
}; // class shooting_stats final

} // namespace plugin::gui::windows::main::widgets

#endif // GADMIN_PLUGIN_GUI_WINDOWS_MAIN_WIDGETS_SHOOTING_STATS_H
