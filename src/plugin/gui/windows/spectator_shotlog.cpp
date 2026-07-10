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

#include "plugin/gui/windows/spectator_shotlog.h"
#include "plugin/gui/style.h"
#include "plugin/server/shooting.h"
#include "plugin/server/spectator.h"
#include "plugin/samp/player.h"
#include "plugin/game/weapon.h"
#include "plugin/game/game.h"
#include "plugin/plugin.h"
#include <string>
#include <format>
#include <utility>
#include <algorithm>

namespace {

/// The weapon name, or an empty string for an out-of-range weapon ID.
auto weapon_name(std::uint8_t weapon_id) -> plugin::types::zstring_t {
    return (weapon_id <= std::to_underlying(plugin::game::weapon::parachute))
        ? plugin::game::weapon_names[weapon_id] : "";
}

/// Format a player as `nickname[id]`, or `[id]` if the nickname is unknown.
auto format_player(const std::string& nickname, std::uint16_t id) -> std::string {
    return nickname.empty() ? std::format("[{}]", id) : std::format("{}[{}]", nickname, id);
}

/// A color usable as text, falling back to the default text color for white/transparent.
auto safe_text_color(const plugin::types::color& color) -> ImU32 {
    return (*color == 0xFFFFFFFF || *color == 0) ? ImGui::GetColorU32(ImGuiCol_Text) : *color;
}

} // namespace

auto plugin::gui::windows::spectator_shotlog::render() -> void {
    auto& window_configuration = (*configuration)["windows"]["spectator_shotlog"];

    if (!window_configuration["use"] || !server::spectator::can_render())
        return;

    std::size_t max_count = window_configuration["max_count"];
    bool show_misses = window_configuration["show_misses"];
    std::uint16_t spectated_id = server::spectator::id;

    const auto& statistics = server::shooting::statistics();
    const auto& records = server::shooting::records();

    const server::shooting::player_statistics* spectated_stats = nullptr;

    if (auto it = statistics.find(spectated_id); it != statistics.end())
        spectated_stats = &it->second;

    std::uint32_t shots_fired = (spectated_stats != nullptr) ? spectated_stats->shots_fired : 0;
    std::uint32_t hits_dealt = (spectated_stats != nullptr) ? spectated_stats->hits_dealt : 0;

    float accuracy = (shots_fired != 0) ? static_cast<float>(hits_dealt) / shots_fired : 0.0f;

    auto [ size_x, size_y ] = game::get_screen_resolution();

    ImGui::SetNextWindowPos({ size_x / 1.36f, size_y / 4.2f }, ImGuiCond_FirstUseEver);
    ImGui::Begin(get_id(), nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize
                                    | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    {
        ImGui::PushFont(bold_font, font_size);
        {
            types::color spectated_color = server::spectator::player.get_color();

            if (*spectated_color == 0xFFFFFFFF || *spectated_color == 0)
                spectated_color = ImGui::GetColorU32(ImGuiCol_Text);

            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(*spectated_color), "%s[%u]",
                               server::spectator::nickname.c_str(), spectated_id);

            ImGui::SameLine();
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(*style::get_current_accent_colors().green),
                               "%.0f%%", accuracy * 100.0f);

            ImGui::SameLine();
            ImGui::TextDisabled("(%u/%u)", hits_dealt, shots_fired);

            if (spectated_stats != nullptr) {
                server::shooting::suspicion_level level = spectated_stats->suspicion();

                if (level != server::shooting::suspicion_level::none) {
                    ImU32 warning_color = (level == server::shooting::suspicion_level::high)
                        ? *style::get_current_accent_colors().red : IM_COL32(255, 170, 0, 255);

                    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(warning_color),
                                       "[!] Подозрительная стрельба (%.0f%% невозможных)",
                                       spectated_stats->implausibility_rate() * 100.0f);
                }
            }
        }
        ImGui::PopFont();

        ImGui::PushFont(regular_font, font_size);
        {
            // Filter: the whole stream zone, or shots from/to the spectated player.
            static constexpr plugin::types::zstring_t filter_items[] = { "Зона стрима", "От наблюдаемого", "К наблюдаемому" };
            int filter_mode = std::clamp(static_cast<int>(window_configuration["filter_mode"]), 0, 2);

            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 9.0f);
            if (ImGui::Combo("##spectator_shotlog_filter", &filter_mode, filter_items, IM_ARRAYSIZE(filter_items)))
                window_configuration["filter_mode"] = filter_mode;

            std::size_t shown = 0;

            if (ImGui::BeginTable("windows::spectator_shotlog::table", 5,
                                  ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoBordersInBody))
            {
                for (const auto& record : records) {
                    if (shown >= max_count)
                        break;

                    bool passes_filter = (filter_mode == 1) ? (record.shooter_id == spectated_id)
                                       : (filter_mode == 2) ? (record.is_hit && record.target_id == spectated_id)
                                       : true; // 0: whole stream zone

                    if (!passes_filter)
                        continue;

                    // The "to player" filter only ever matches hits, so misses are dropped there anyway.
                    if (!record.is_hit && !show_misses && filter_mode != 2)
                        continue;

                    shown++;
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextDisabled("%s", record.timestamp.c_str());

                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(safe_text_color(record.shooter_color)), "%s",
                                       format_player(record.shooter_nickname, record.shooter_id).c_str());

                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextUnformatted(weapon_name(record.weapon_id));

                    ImGui::TableSetColumnIndex(3);
                    if (record.is_hit && record.target_id != samp::player::id_none) {
                        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(*style::get_current_accent_colors().green),
                                           "→ %s", format_player(record.target_nickname, record.target_id).c_str());

                        if (record.analysis == server::shooting::hit_analysis::implausible) {
                            ImGui::SameLine();
                            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(*style::get_current_accent_colors().red), "[!]");
                        }
                    } else {
                        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(*style::get_current_accent_colors().red), "мимо");
                    }

                    ImGui::TableSetColumnIndex(4);
                    ImGui::Text("%.0f m", record.distance);
                }

                ImGui::EndTable();
            }

            if (shown == 0)
                ImGui::TextDisabled("Нет выстрелов");
        }
        ImGui::PopFont();
    }
    ImGui::End();
}

auto plugin::gui::windows::spectator_shotlog::create(types::not_null<gui_initializer*> child) noexcept -> window_ptr_t {
    return std::make_unique<spectator_shotlog>(child);
}
