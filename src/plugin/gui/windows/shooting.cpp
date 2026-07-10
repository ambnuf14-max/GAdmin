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

#include "plugin/gui/windows/shooting.h"
#include "plugin/server/user.h"
#include "plugin/game/game.h"
#include "plugin/plugin.h"
#include <functional>

auto plugin::gui::windows::shooting::toggle_hotkey_callback(gui::hotkey&) -> void {
    opened ^= true;
}

auto plugin::gui::windows::shooting::render() -> void {
    auto& window_configuration = (*configuration)["windows"]["shooting"];

    if (!opened || !window_configuration["use"] || !server::user::is_on_alogin())
        return;

    float frame_height = ImGui::GetFrameHeight();
    auto [ size_x, size_y ] = game::get_screen_resolution();

    ImGui::SetNextWindowPos({ size_x / 2.0f, size_y / 2.0f }, ImGuiCond_FirstUseEver, { 0.5f, 0.5f });
    ImGui::SetNextWindowSize({ frame_height * 30, frame_height * 16 }, ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Меню стрельбы###windows::shooting", &opened))
        panel.render();

    ImGui::End();
}

auto plugin::gui::windows::shooting::create(types::not_null<gui_initializer*> child) noexcept -> window_ptr_t {
    return std::make_unique<shooting>(child);
}

plugin::gui::windows::shooting::shooting(types::not_null<gui_initializer*> child)
    : window(child, get_id())
{
    toggle_hotkey = gui::hotkey("Открыть меню стрельбы",
                                gui::key_bind({ 'K', 0 }, { .alt = true }, gui::bind_condition::on_alogin))
        .with_callback(std::bind(&shooting::toggle_hotkey_callback, this, std::placeholders::_1));

    child->hotkey_handler->add(toggle_hotkey);
}
