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

#include "plugin/server/shooting.h"
#include "plugin/samp/events/synchronization.h" // IWYU pragma: keep
#include "plugin/samp/player.h"
#include "plugin/samp/core/player_pool.h"
#include "plugin/samp/core/remote_player.h"
#include "plugin/samp/core/ped.h"
#include "plugin/samp/core/user.h"
#include "plugin/game/ped.h"
#include "plugin/server/user.h"
#include "plugin/common_utils.h"
#include "plugin/game/weapon.h"
#include "plugin/log.h"
#include <algorithm>
#include <utility>

namespace {

/// Body sphere radius (meters) plus slack for movement between the shot and our position
/// sample. A hit landing farther than this from the target's real body is implausible.
constexpr float plausibility_radius = 2.5f;

/// Get a streamed player's current body position. Returns false if the player is not
/// streamed in (then the hit cannot be geometrically verified).
auto get_streamed_position(std::uint16_t id, plugin::types::vector_3d& out) -> bool {
    auto remote = plugin::samp::player_pool::get_remote_player(id);

    if (!remote)
        return false;

    plugin::samp::ped samp_ped = remote->get_ped();

    if (!samp_ped.is_available())
        return false;

    out = samp_ped.get_game_ped().get_position();
    return true;
}

} // namespace

auto plugin::server::shooting::player_statistics::accuracy() const noexcept -> float {
    return (shots_fired != 0) ? static_cast<float>(hits_dealt) / shots_fired : 0.0f;
}

auto plugin::server::shooting::player_statistics::average_distance() const noexcept -> float {
    return (shots_fired != 0) ? static_cast<float>(distance_sum / shots_fired) : 0.0f;
}

auto plugin::server::shooting::player_statistics::implausibility_rate() const noexcept -> float {
    return (checked_hits != 0) ? static_cast<float>(implausible_hits) / checked_hits : 0.0f;
}

auto plugin::server::shooting::player_statistics::suspicion() const noexcept -> suspicion_level {
    float rate = implausibility_rate();

    if (checked_hits >= 4 && rate >= 0.34f)
        return suspicion_level::high;

    if ((checked_hits >= 4 && rate >= 0.15f) || (shots_fired >= 25 && accuracy() >= 0.90f))
        return suspicion_level::elevated;

    return suspicion_level::none;
}

auto plugin::server::shooting::process_shot(std::uint16_t shooter_id, std::uint8_t hit_type, std::uint16_t hit_id,
                                            const types::vector_3d& origin, const types::vector_3d& hit,
                                            const types::vector_3d& offset, std::uint8_t weapon_id,
                                            types::zstring_t source) -> void
{
    if (!user::is_on_alogin())
        return;

    shot_record record;

    record.time = std::chrono::steady_clock::now();
    record.timestamp = common_utils::get_current_timestamp();
    record.shooter_id = shooter_id;
    record.target_id = samp::player::id_none;
    record.weapon_id = weapon_id;
    record.origin = origin;
    record.hit = hit;
    record.offset = offset;
    record.distance = origin.get_distance_to(hit);
    record.is_hit = static_cast<bullet_hit_type>(hit_type) == bullet_hit_type::player;

    if (samp::player shooter(shooter_id); shooter) {
        record.shooter_nickname = shooter.nickname;
        record.shooter_color = shooter.get_color();
    }

    if (record.is_hit && hit_id != samp::player::id_none) {
        record.target_id = hit_id;

        if (samp::player target(hit_id); target)
            record.target_nickname = target.nickname;

        // Geometric plausibility: compare the reported hit point against the target's real
        // streamed position. A hit far from the target's body is likely fabricated (silent aim
        // / position faking). When the target is not streamed, the verdict stays `not_checked`.
        if (types::vector_3d target_position; get_streamed_position(hit_id, target_position)) {
            record.analysis_distance = record.hit.get_distance_to(target_position);
            record.analysis = (record.analysis_distance > plausibility_radius)
                ? hit_analysis::implausible : hit_analysis::plausible;
        }
    }

    // Accumulate per-player statistics (shots/hits/accuracy, not HP damage).
    player_statistics& shooter_stats = player_stats[record.shooter_id];
    shooter_stats.id = record.shooter_id;
    shooter_stats.color = record.shooter_color;

    if (!record.shooter_nickname.empty())
        shooter_stats.nickname = record.shooter_nickname;

    shooter_stats.shots_fired++;
    shooter_stats.distance_sum += record.distance;
    shooter_stats.distance_max = std::max(shooter_stats.distance_max, record.distance);

    weapon_statistics& weapon_stats = shooter_stats.weapons[record.weapon_id];
    weapon_stats.shots++;

    if (record.is_hit) {
        shooter_stats.hits_dealt++;
        weapon_stats.hits++;

        if (record.target_id != samp::player::id_none) {
            shooter_stats.dealt_to[record.target_id]++;

            player_statistics& victim_stats = player_stats[record.target_id];
            victim_stats.id = record.target_id;
            victim_stats.hits_taken++;
            victim_stats.taken_from[record.shooter_id]++;

            if (!record.target_nickname.empty())
                victim_stats.nickname = record.target_nickname;
        }

        if (record.analysis != hit_analysis::not_checked) {
            shooter_stats.checked_hits++;

            if (record.analysis == hit_analysis::implausible)
                shooter_stats.implausible_hits++;
        }
    }

    log::info("[shotdbg] {}: shooter={} raw_hit_type={} is_hit={} hit_id={} weapon={}({}) dist={:.1f} analysis={} adist={:.1f}",
              source, shooter_id, hit_type, record.is_hit, hit_id,
              (weapon_id <= std::to_underlying(game::weapon::parachute)) ? game::weapon_names[weapon_id] : "?",
              weapon_id, record.distance, std::to_underlying(record.analysis), record.analysis_distance);

    buffer.push_front(std::move(record));

    while (buffer.size() > capacity)
        buffer.pop_back();
}

auto plugin::server::shooting::on_event(const samp::event_info& event) -> bool {
    if (!(event == samp::event_id::bullet_synchronization))
        return true;

    // Incoming bullets describe other players' shots; outgoing bullets are the local player's
    // own shots (no `player_id` in the packet — the shooter is the local SA:MP user).
    if (event == samp::event_type::incoming_packet) {
        auto packet = event.create<samp::event_id::bullet_synchronization, samp::event_type::incoming_packet>();
        process_shot(packet.player_id, packet.hit_type, packet.hit_id, packet.origin, packet.hit, packet.offset, packet.weapon_id, "incoming");
    } else if (event == samp::event_type::outgoing_packet) {
        auto packet = event.create<samp::event_id::bullet_synchronization, samp::event_type::outgoing_packet>();
        process_shot(samp::user::get_id(), packet.hit_type, packet.hit_id, packet.origin, packet.hit, packet.offset, packet.weapon_id, "outgoing");
    }

    return true;
}
