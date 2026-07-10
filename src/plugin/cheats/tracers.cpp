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

#include "plugin/cheats/tracers.h"
#include "plugin/gui/icon.h"
#include "plugin/gui/notify.h"
#include "plugin/server/shooting.h"
#include "plugin/server/spectator.h"
#include "plugin/server/user.h"
#include "plugin/game/game.h"
#include "plugin/game/weapon.h"
#include "plugin/plugin.h"
#include "plugin/types/color.h"
#include <format>
#include <string>
#include <utility>

auto plugin::cheats::tracers::hotkey_callback(gui::hotkey&) -> void {
    hidden_before = std::chrono::steady_clock::now();
    gui::notify::send(gui::notification("Трассера удалены", "Трассера на экране успешно скрыты!", ICON_INFO));
}

auto plugin::cheats::tracers::render(types::not_null<gui_initializer*>) -> void {
    auto cheat_configuration = (*configuration)["cheats"]["tracers"];

    if (!cheat_configuration["use"] || !server::user::is_on_alogin() || game::is_menu_opened())
        return;

    const auto& records = server::shooting::records();

    if (records.empty())
        return;

    auto now = std::chrono::steady_clock::now();
    auto time_to_hide = std::chrono::seconds(cheat_configuration["seconds_to_hide"]);
    bool only_from_spectator = cheat_configuration["only_from_spectator"];
    bool show_endpoint = cheat_configuration["show_endpoint"];
    bool show_shooter_label = cheat_configuration["show_shooter_label"];
    bool show_weapon_distance_label = cheat_configuration["show_weapon_distance_label"];
    float line_thickness = cheat_configuration["line_thickness"];
    types::color hit_color = cheat_configuration["hit_color"];
    types::color miss_color = cheat_configuration["miss_color"];

    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();

    // Draw a string with a one-pixel black shadow for readability over any background.
    auto draw_label = [draw_list](float x, float y, ImU32 color, const std::string& text) {
        draw_list->AddText({ x + 1.0f, y + 1.0f }, IM_COL32_BLACK, text.c_str());
        draw_list->AddText({ x, y }, color, text.c_str());
    };

    for (const auto& record : records) {
        if (now - record.time >= time_to_hide || record.time < hidden_before)
            continue;

        if (only_from_spectator && (!server::spectator::is_active() || record.shooter_id != server::spectator::id))
            continue;

        auto [ origin_x, origin_y, origin_z ] = game::convert_3d_coords_to_screen(record.origin);
        auto [ target_x, target_y, target_z ] = game::convert_3d_coords_to_screen(record.hit);

        if (origin_z <= 0.0f || target_z <= 0.0f)
            continue;

        types::color color = (record.is_hit) ? hit_color : miss_color;

        draw_list->AddLine({ origin_x, origin_y }, { target_x, target_y }, *color, line_thickness);

        if (show_endpoint)
            draw_list->AddCircleFilled({ target_x + 1.5f, target_y + 1.5f }, 3, *color, 16);

        if (show_shooter_label && !record.shooter_nickname.empty())
            draw_label(origin_x, origin_y, *color, std::format("{}[{}]", record.shooter_nickname, record.shooter_id));

        if (show_weapon_distance_label && record.weapon_id <= std::to_underlying(game::weapon::parachute))
            draw_label(target_x + 6.0f, target_y, *color, std::format("{} {:.0f}m", game::weapon_names[record.weapon_id], record.distance));
    }
}

auto plugin::cheats::tracers::register_hotkeys(types::not_null<gui::hotkey_handler*> handler) -> void {
    handler->add(hotkey);
}

plugin::cheats::tracers::tracers() {
    hotkey = gui::hotkey("Удалить трассера", gui::key_bind({ 'C', 0 }, { .alt = true }, gui::bind_condition::on_alogin))
        .with_callback(std::bind(&tracers::hotkey_callback, this, std::placeholders::_1));
}
