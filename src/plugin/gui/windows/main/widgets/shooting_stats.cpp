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

#include "plugin/gui/windows/main/widgets/shooting_stats.h"
#include "plugin/gui/widgets/button.h"
#include "plugin/gui/style.h"
#include "plugin/server/shooting.h"
#include "plugin/samp/player.h"
#include "plugin/game/weapon.h"
#include "plugin/plugin.h"
#include <algorithm>
#include <vector>
#include <string>
#include <format>
#include <utility>
#include <cfloat>

namespace {

using shot_record = plugin::server::shooting::shot_record;
using player_statistics = plugin::server::shooting::player_statistics;

/// Resolve a player's nickname from the collected statistics, falling back to the SA:MP
/// player pool and finally to an empty string.
auto resolve_nickname(std::uint16_t id) -> std::string {
    const auto& stats = plugin::server::shooting::statistics();

    if (auto it = stats.find(id); it != stats.end() && !it->second.nickname.empty())
        return it->second.nickname;

    if (plugin::samp::player player(id); player && !player.nickname.empty())
        return player.nickname;

    return "";
}

/// Format a player as `nickname[id]`, or `[id]` if the nickname is unknown.
auto format_player(std::uint16_t id) -> std::string {
    std::string nickname = resolve_nickname(id);
    return nickname.empty() ? std::format("[{}]", id) : std::format("{}[{}]", nickname, id);
}

/// The weapon name, or an empty string for an out-of-range weapon ID.
auto weapon_name(std::uint8_t weapon_id) -> plugin::types::zstring_t {
    return (weapon_id <= std::to_underlying(plugin::game::weapon::parachute))
        ? plugin::game::weapon_names[weapon_id] : "";
}

/// A color usable as text: the player's clist, falling back to the default text color
/// for an unknown or plain white/transparent color.
auto render_safe_color(const plugin::types::color& color) -> plugin::types::color {
    return (*color == 0xFFFFFFFF || *color == 0)
        ? plugin::types::color(ImGui::GetColorU32(ImGuiCol_Text)) : color;
}

/// Display color for a suspicion level.
auto suspicion_color(plugin::server::shooting::suspicion_level level) -> ImU32 {
    using level_t = plugin::server::shooting::suspicion_level;

    switch (level) {
        case level_t::high:     return *plugin::gui::style::get_current_accent_colors().red;
        case level_t::elevated: return IM_COL32(255, 170, 0, 255);
        default:                return ImGui::GetColorU32(ImGuiCol_TextDisabled);
    }
}

/// Short label for a suspicion level.
auto suspicion_label(plugin::server::shooting::suspicion_level level) -> plugin::types::zstring_t {
    using level_t = plugin::server::shooting::suspicion_level;

    switch (level) {
        case level_t::high:     return "Высокая";
        case level_t::elevated: return "Подозрит.";
        default:                return "—";
    }
}

/// Human-readable reasons behind a player's suspicion level (empty if nothing abnormal).
auto suspicion_reasons(const player_statistics& stats) -> std::vector<std::string> {
    std::vector<std::string> reasons;

    if (stats.checked_hits != 0 && stats.implausible_hits != 0)
        reasons.push_back(std::format("Невозможных попаданий: {}/{} ({:.0f}%)",
                          stats.implausible_hits, stats.checked_hits, stats.implausibility_rate() * 100.0f));

    if (stats.shots_fired >= 25 && stats.accuracy() >= 0.90f)
        reasons.push_back(std::format("Аномальная точность: {:.0f}% на {} выстрелах",
                          stats.accuracy() * 100.0f, stats.shots_fired));

    return reasons;
}

/// Sortable columns of the shot log table, used as the columns' user IDs.
enum class log_column : ImGuiID {
    time, shooter, target, weapon, distance, result
}; // enum class log_column : ImGuiID

/// Sortable columns of the dashboard table, used as the columns' user IDs.
enum class dashboard_column : ImGuiID {
    player, shots, hits, accuracy, taken, suspicion
}; // enum class dashboard_column : ImGuiID

/// Render a `nickname[id]` cell colored by the player's clist.
auto render_player_cell(std::uint16_t id, const std::string& nickname, const plugin::types::color& color) -> void {
    ImGui::PushStyleColor(ImGuiCol_Text, *render_safe_color(color));

    if (nickname.empty())
        ImGui::Text("[%u]", id);
    else
        ImGui::Text("%s[%u]", nickname.c_str(), id);

    ImGui::PopStyleColor();
}

/// Sort the shot log view in place according to the table's sort specs.
auto sort_log(std::vector<const shot_record*>& view, const ImGuiTableSortSpecs* specs) -> void {
    std::ranges::sort(view, [specs](const shot_record* a, const shot_record* b) {
        for (int n = 0; n < specs->SpecsCount; n++) {
            const ImGuiTableColumnSortSpecs& spec = specs->Specs[n];
            int delta = 0;

            switch (static_cast<log_column>(spec.ColumnUserID)) {
                case log_column::time:     delta = (a->time < b->time) ? -1 : (a->time > b->time) ? 1 : 0; break;
                case log_column::shooter:  delta = a->shooter_nickname.compare(b->shooter_nickname); break;
                case log_column::target:   delta = a->target_nickname.compare(b->target_nickname); break;
                case log_column::weapon:   delta = static_cast<int>(a->weapon_id) - static_cast<int>(b->weapon_id); break;
                case log_column::distance: delta = (a->distance < b->distance) ? -1 : (a->distance > b->distance) ? 1 : 0; break;
                default: break;
            }

            if (delta != 0)
                return (spec.SortDirection == ImGuiSortDirection_Ascending) ? (delta < 0) : (delta > 0);
        }

        return a->time > b->time; // stable fallback: newest first
    });
}

/// Sort the dashboard view in place according to the table's sort specs.
auto sort_dashboard(std::vector<const player_statistics*>& view, const ImGuiTableSortSpecs* specs) -> void {
    std::ranges::sort(view, [specs](const player_statistics* a, const player_statistics* b) {
        for (int n = 0; n < specs->SpecsCount; n++) {
            const ImGuiTableColumnSortSpecs& spec = specs->Specs[n];
            float delta = 0.0f;

            switch (static_cast<dashboard_column>(spec.ColumnUserID)) {
                case dashboard_column::player:   delta = static_cast<float>(a->nickname.compare(b->nickname)); break;
                case dashboard_column::shots:    delta = static_cast<float>(a->shots_fired) - b->shots_fired; break;
                case dashboard_column::hits:     delta = static_cast<float>(a->hits_dealt) - b->hits_dealt; break;
                case dashboard_column::accuracy: delta = a->accuracy() - b->accuracy(); break;
                case dashboard_column::taken:    delta = static_cast<float>(a->hits_taken) - b->hits_taken; break;
                case dashboard_column::suspicion:
                    delta = static_cast<float>(std::to_underlying(a->suspicion())) - std::to_underlying(b->suspicion());
                    break;
            }

            if (delta != 0.0f)
                return (spec.SortDirection == ImGuiSortDirection_Ascending) ? (delta < 0.0f) : (delta > 0.0f);
        }

        return a->shots_fired > b->shots_fired; // stable fallback: most active first
    });
}

} // namespace

auto plugin::gui::windows::main::widgets::shooting_stats::render_log() -> void {
    const auto& records = server::shooting::records();

    float region_avail_x = ImGui::GetContentRegionAvail().x;
    float clear_button_height = ImGui::GetFrameHeight();

    log_search.render(region_avail_x, "Поиск");

    std::vector<const server::shooting::shot_record*> view;
    view.reserve(records.size());

    for (const auto& record : records) {
        if (!log_search.contains("{} {} {}", record.shooter_nickname, record.target_nickname, weapon_name(record.weapon_id)))
            continue;

        view.push_back(&record);
    }

    constexpr ImGuiTableFlags table_flags = ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY
        | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_BordersInnerH;

    float table_height = ImGui::GetContentRegionAvail().y - clear_button_height - ImGui::GetStyle().ItemSpacing.y;

    if (ImGui::BeginTable("frames::logs::shooting::log", 6, table_flags, { region_avail_x, table_height })) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Время", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortDescending,
                                0.0f, static_cast<ImGuiID>(log_column::time));
        ImGui::TableSetupColumn("Стрелок", 0, 0.0f, static_cast<ImGuiID>(log_column::shooter));
        ImGui::TableSetupColumn("Цель", 0, 0.0f, static_cast<ImGuiID>(log_column::target));
        ImGui::TableSetupColumn("Оружие", 0, 0.0f, static_cast<ImGuiID>(log_column::weapon));
        ImGui::TableSetupColumn("Дистанция", 0, 0.0f, static_cast<ImGuiID>(log_column::distance));
        ImGui::TableSetupColumn("Результат", ImGuiTableColumnFlags_NoSort, 0.0f, static_cast<ImGuiID>(log_column::result));
        ImGui::TableHeadersRow();

        if (const ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs(); specs && specs->SpecsCount > 0)
            sort_log(view, specs);

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(view.size()));

        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                const auto& record = *view[i];
                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(record.timestamp.c_str());

                ImGui::TableSetColumnIndex(1);
                render_player_cell(record.shooter_id, record.shooter_nickname, record.shooter_color);

                ImGui::TableSetColumnIndex(2);
                if (record.is_hit && record.target_id != samp::player::id_none)
                    ImGui::Text("%s[%u]", record.target_nickname.c_str(), record.target_id);
                else
                    ImGui::TextUnformatted("—");

                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(weapon_name(record.weapon_id));

                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%.0f m", record.distance);

                ImGui::TableSetColumnIndex(5);
                types::color result_color = (record.is_hit)
                    ? gui::style::get_current_accent_colors().green
                    : gui::style::get_current_accent_colors().red;

                ImGui::PushStyleColor(ImGuiCol_Text, *result_color);
                ImGui::TextUnformatted((record.is_hit) ? "Попадание" : "Промах");
                ImGui::PopStyleColor();

                if (record.analysis == server::shooting::hit_analysis::implausible) {
                    ImGui::SameLine();
                    ImGui::PushStyleColor(ImGuiCol_Text, *gui::style::get_current_accent_colors().red);
                    ImGui::TextUnformatted("[!]");
                    ImGui::PopStyleColor();

                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Невозможное попадание: пуля в %.1f м от реального тела цели.\n"
                                          "Возможен silent aim / подмена позиции.", record.analysis_distance);
                }
            }
        }

        ImGui::EndTable();
    }

    if (gui::widgets::button("Очистить##frames::logs::shooting::log_clear", { region_avail_x, clear_button_height }).render())
        server::shooting::clear();
}

auto plugin::gui::windows::main::widgets::shooting_stats::render_dashboard() -> void {
    const auto& statistics = server::shooting::statistics();

    float region_avail_x = ImGui::GetContentRegionAvail().x;
    float reset_button_height = ImGui::GetFrameHeight();

    dashboard_search.render(region_avail_x, "Поиск по нику");

    std::vector<const server::shooting::player_statistics*> view;
    view.reserve(statistics.size());

    for (const auto& [ id, stats ] : statistics) {
        if (!dashboard_search.contains("{}", format_player(id)))
            continue;

        view.push_back(&stats);
    }

    constexpr ImGuiTableFlags table_flags = ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY
        | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerH;

    float table_height = ImGui::GetContentRegionAvail().y - reset_button_height - ImGui::GetStyle().ItemSpacing.y;

    if (ImGui::BeginTable("frames::logs::shooting::dashboard", 6, table_flags, { region_avail_x, table_height })) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Игрок", ImGuiTableColumnFlags_WidthStretch, 0.0f, static_cast<ImGuiID>(dashboard_column::player));
        ImGui::TableSetupColumn("Выстрелы", ImGuiTableColumnFlags_DefaultSort | ImGuiTableColumnFlags_PreferSortDescending
                                | ImGuiTableColumnFlags_WidthFixed, 0.0f, static_cast<ImGuiID>(dashboard_column::shots));
        ImGui::TableSetupColumn("Попадания", ImGuiTableColumnFlags_WidthFixed, 0.0f, static_cast<ImGuiID>(dashboard_column::hits));
        ImGui::TableSetupColumn("Точность", ImGuiTableColumnFlags_WidthStretch, 0.0f, static_cast<ImGuiID>(dashboard_column::accuracy));
        ImGui::TableSetupColumn("Получил", ImGuiTableColumnFlags_WidthFixed, 0.0f, static_cast<ImGuiID>(dashboard_column::taken));
        ImGui::TableSetupColumn("Подозрит.", ImGuiTableColumnFlags_WidthFixed, 0.0f, static_cast<ImGuiID>(dashboard_column::suspicion));
        ImGui::TableHeadersRow();

        if (const ImGuiTableSortSpecs* specs = ImGui::TableGetSortSpecs(); specs && specs->SpecsCount > 0)
            sort_dashboard(view, specs);

        for (const auto* stats : view) {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, *render_safe_color(stats->color));
            {
                std::string row_id = std::format("{}##dashboard_row_{}", format_player(stats->id), stats->id);
                if (ImGui::Selectable(row_id.c_str(), false, ImGuiSelectableFlags_SpanAllColumns))
                    selected_player = stats->id;
            }
            ImGui::PopStyleColor();

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%u", stats->shots_fired);

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%u", stats->hits_dealt);

            ImGui::TableSetColumnIndex(3);
            std::string accuracy_text = std::format("{:.0f}%", stats->accuracy() * 100.0f);
            ImGui::ProgressBar(stats->accuracy(), { -FLT_MIN, 0.0f }, accuracy_text.c_str());

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%u", stats->hits_taken);

            ImGui::TableSetColumnIndex(5);
            server::shooting::suspicion_level level = stats->suspicion();
            ImGui::PushStyleColor(ImGuiCol_Text, suspicion_color(level));
            ImGui::TextUnformatted(suspicion_label(level));
            ImGui::PopStyleColor();

            if (level != server::shooting::suspicion_level::none && ImGui::IsItemHovered()) {
                std::string tooltip;

                for (const auto& reason : suspicion_reasons(*stats))
                    tooltip += "• " + reason + "\n";

                tooltip += "Это повод заспектить, не доказательство.";
                ImGui::SetTooltip("%s", tooltip.c_str());
            }
        }

        ImGui::EndTable();
    }

    if (gui::widgets::button("Сбросить статистику##frames::logs::shooting::reset", { region_avail_x, reset_button_height }).render())
        server::shooting::reset_statistics();
}

auto plugin::gui::windows::main::widgets::shooting_stats::render_player_card(std::uint16_t id) -> void {
    const auto& statistics = server::shooting::statistics();

    float frame_height = ImGui::GetFrameHeight();

    if (gui::widgets::button("← Назад##frames::logs::shooting::back", { ImGui::GetFrameHeight() * 4, frame_height }).render()) {
        selected_player.reset();
        return;
    }

    auto it = statistics.find(id);

    if (it == statistics.end()) {
        selected_player.reset();
        return;
    }

    const server::shooting::player_statistics& stats = it->second;

    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, *render_safe_color(stats.color));
    ImGui::TextUnformatted(format_player(id).c_str());
    ImGui::PopStyleColor();

    if (!ImGui::BeginChild("frames::logs::shooting::card", { 0.0f, 0.0f }, ImGuiChildFlags_AlwaysUseWindowPadding))
        return ImGui::EndChild();

    ImGui::SeparatorText("Нанёс");
    {
        ImGui::Text("Выстрелы: %u   Попадания: %u", stats.shots_fired, stats.hits_dealt);
        std::string accuracy_text = std::format("Точность: {:.0f}%", stats.accuracy() * 100.0f);
        ImGui::ProgressBar(stats.accuracy(), { -FLT_MIN, 0.0f }, accuracy_text.c_str());

        if (!stats.dealt_to.empty() && ImGui::BeginTable("card_dealt_to", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
            ImGui::TableSetupColumn("Кому попал");
            ImGui::TableSetupColumn("Попаданий", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableHeadersRow();

            for (const auto& [ target_id, hits ] : stats.dealt_to) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(format_player(target_id).c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%u", hits);
            }

            ImGui::EndTable();
        }
    }

    ImGui::SeparatorText("Получил");
    {
        ImGui::Text("Попаданий получено: %u", stats.hits_taken);

        if (!stats.taken_from.empty() && ImGui::BeginTable("card_taken_from", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
            ImGui::TableSetupColumn("Кто попал");
            ImGui::TableSetupColumn("Попаданий", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableHeadersRow();

            for (const auto& [ shooter_id, hits ] : stats.taken_from) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(format_player(shooter_id).c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%u", hits);
            }

            ImGui::EndTable();
        }
    }

    ImGui::SeparatorText("Оружие и дистанция");
    {
        ImGui::Text("Средняя дистанция: %.0f m   Максимальная: %.0f m", stats.average_distance(), stats.distance_max);

        if (!stats.weapons.empty() && ImGui::BeginTable("card_weapons", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
            ImGui::TableSetupColumn("Оружие");
            ImGui::TableSetupColumn("Выстрелы", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Попадания", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Точность", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableHeadersRow();

            for (const auto& [ weapon_id, weapon_stats ] : stats.weapons) {
                float weapon_accuracy = (weapon_stats.shots != 0)
                    ? static_cast<float>(weapon_stats.hits) / weapon_stats.shots : 0.0f;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(weapon_name(weapon_id));
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%u", weapon_stats.shots);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%u", weapon_stats.hits);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%.0f%%", weapon_accuracy * 100.0f);
            }

            ImGui::EndTable();
        }
    }

    ImGui::SeparatorText("Анализ (анти-чит)");
    {
        server::shooting::suspicion_level level = stats.suspicion();

        ImGui::TextUnformatted("Подозрительность:");
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, suspicion_color(level));
        ImGui::TextUnformatted(suspicion_label(level));
        ImGui::PopStyleColor();

        ImGui::Text("Проверено попаданий: %u из %u", stats.checked_hits, stats.hits_dealt);
        ImGui::Text("Невозможных попаданий: %u (%.0f%%)", stats.implausible_hits, stats.implausibility_rate() * 100.0f);

        for (const auto& reason : suspicion_reasons(stats))
            ImGui::BulletText("%s", reason.c_str());

        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled));
        ImGui::TextWrapped("Проверка работает только для застримленных целей. Хорошо сделанный silent aim, "
                           "кладущий пулю в реальную позицию цели, геометрией не ловится — флаг это повод "
                           "заспектить, а не доказательство.");
        ImGui::PopStyleColor();
    }

    ImGui::SeparatorText("Лента выстрелов");
    {
        const auto& records = server::shooting::records();

        if (ImGui::BeginTable("card_feed", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("Время", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Оружие", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Результат");
            ImGui::TableSetupColumn("Дистанция", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableHeadersRow();

            for (const auto& record : records) {
                if (record.shooter_id != id)
                    continue;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(record.timestamp.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(weapon_name(record.weapon_id));

                ImGui::TableSetColumnIndex(2);
                if (record.is_hit && record.target_id != samp::player::id_none) {
                    ImGui::PushStyleColor(ImGuiCol_Text, *gui::style::get_current_accent_colors().green);
                    ImGui::Text("→ %s", format_player(record.target_id).c_str());
                    ImGui::PopStyleColor();
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Text, *gui::style::get_current_accent_colors().red);
                    ImGui::TextUnformatted("мимо");
                    ImGui::PopStyleColor();
                }

                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%.0f m", record.distance);
            }

            ImGui::EndTable();
        }
    }

    ImGui::EndChild();
}

auto plugin::gui::windows::main::widgets::shooting_stats::render() -> void {
    if (!ImGui::BeginTabBar("frames::logs::shooting::tabs"))
        return;

    if (ImGui::BeginTabItem("Лог выстрелов")) {
        render_log();
        ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Статистика")) {
        if (selected_player.has_value())
            render_player_card(*selected_player);
        else
            render_dashboard();

        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
}
