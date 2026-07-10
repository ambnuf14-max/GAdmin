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

#ifndef GADMIN_PLUGIN_SERVER_SHOOTING_H
#define GADMIN_PLUGIN_SERVER_SHOOTING_H

#include "plugin/samp/events/synchronization.h"
#include "plugin/samp/events/event.h"
#include "plugin/types/color.h"
#include "plugin/types/simple.h"
#include <chrono>
#include <cstdint>
#include <deque>
#include <map>
#include <string>

namespace plugin::server {

/// Shared source of truth for incoming bullet shots.
///
/// Captures the `bullet_synchronization` packet once, enriches it with the shooter's
/// nickname/color, the target, weapon and distance, and stores the result both as a
/// bounded rolling log (`records`) and as per-player aggregated statistics
/// (`statistics`). The tracers cheat and the shotlog UI read from this module instead
/// of parsing the packet themselves, so the enrichment is done only once per shot.
///
/// Collection happens only while the user is authenticated via `/alogin`.
///
/// Note: the SA:MP bullet packet carries no HP damage, so the statistics are based on
/// shots/hits/accuracy, not on dealt health.
class shooting final {
public:
    /// Bullet hit types as transmitted in the SA:MP bullet packet.
    enum class bullet_hit_type : std::uint8_t {
        none = 0,         ///< Bullet hit nothing.
        player = 1,       ///< Bullet hit a player.
        vehicle = 2,      ///< Bullet hit a vehicle.
        object = 3,       ///< Bullet hit a map object.
        player_object = 4 ///< Bullet hit a player-attached object.
    }; // enum class bullet_hit_type : std::uint8_t

    /// Per-shot anti-cheat verdict for a player hit (geometric plausibility).
    enum class hit_analysis : std::uint8_t {
        not_checked, ///< Not a player hit, or the target was not streamed (cannot judge).
        plausible,   ///< The reported hit lands on/near the target's actual body.
        implausible  ///< The reported hit is far from the target's real position (likely faked).
    }; // enum class hit_analysis : std::uint8_t

    /// Overall per-player suspicion level derived from the accumulated statistics.
    enum class suspicion_level : std::uint8_t {
        none,     ///< Nothing abnormal.
        elevated, ///< Some implausible hits or unusually high accuracy — worth spectating.
        high      ///< A large share of impossible-geometry hits.
    }; // enum class suspicion_level : std::uint8_t

    /// A single enriched shot record (newest records are stored at the front).
    struct shot_record final {
        std::chrono::steady_clock::time_point time; ///< When the shot was captured (for tracer TTL and sorting).
        std::uint16_t shooter_id = 0;               ///< Shooter's ID.
        std::uint16_t target_id = 0;                ///< Target's ID, `samp::player::id_none` if none.
        std::string shooter_nickname;               ///< Shooter's nickname (empty if unknown).
        std::string target_nickname;                ///< Target's nickname (empty if none or unknown).
        std::string timestamp;                      ///< Capture time in `HH:MM:SS` format for display.
        types::color shooter_color;                 ///< Shooter's clist color.
        std::uint8_t weapon_id = 0;                 ///< Weapon's ID the bullet was fired from.
        types::vector_3d origin;                    ///< Bullet's start coordinates.
        types::vector_3d hit;                       ///< Bullet's end coordinates.
        types::vector_3d offset;                    ///< Bullet's hit offset relative to the hit entity.
        float distance = 0.0f;                      ///< Distance between origin and hit.
        bool is_hit = false;                        ///< Whether the bullet hit a player.
        hit_analysis analysis = hit_analysis::not_checked; ///< Geometric plausibility verdict.
        float analysis_distance = 0.0f;             ///< Distance from `hit` to the target's real body (meters).
    }; // struct shot_record final

    /// Aggregated shots/hits for a single weapon (per player).
    struct weapon_statistics final {
        std::uint32_t shots = 0; ///< Shots fired with the weapon.
        std::uint32_t hits = 0;  ///< Shots that hit a target.
    }; // struct weapon_statistics final

    /// Per-player aggregated shooting statistics, accumulated over the session.
    struct player_statistics final {
        std::uint16_t id = 0;       ///< Player's ID.
        std::string nickname;       ///< Player's last known nickname (empty if unknown).
        types::color color;         ///< Player's last known clist color.

        std::uint32_t shots_fired = 0; ///< Total shots fired by the player.
        std::uint32_t hits_dealt = 0;  ///< Shots by the player that hit a target.
        std::uint32_t hits_taken = 0;  ///< Hits the player received as a target.

        std::uint32_t checked_hits = 0;     ///< Player hits that could be geometrically verified (target streamed).
        std::uint32_t implausible_hits = 0; ///< Of the checked hits, those that landed far from the target's body.

        double distance_sum = 0.0;  ///< Sum of all the player's shot distances.
        float distance_max = 0.0f;  ///< Longest shot distance by the player.

        std::map<std::uint8_t, weapon_statistics> weapons;  ///< Per-weapon shots/hits (output).
        std::map<std::uint16_t, std::uint32_t> dealt_to;    ///< Target ID -> hits the player landed on them.
        std::map<std::uint16_t, std::uint32_t> taken_from;  ///< Shooter ID -> hits the player received from them.

        /// Output accuracy in the `[0; 1]` range (`hits_dealt / shots_fired`).
        ///
        /// @return Accuracy, or `0` if no shots were fired.
        auto accuracy() const noexcept -> float;

        /// Average shot distance.
        ///
        /// @return Average distance, or `0` if no shots were fired.
        auto average_distance() const noexcept -> float;

        /// Fraction of verified hits that were geometrically implausible (`[0; 1]`). Computed only
        /// over hits that could be checked, so it never inflates from unverifiable shots.
        ///
        /// @return Implausibility rate, or `0` if no hits could be checked.
        auto implausibility_rate() const noexcept -> float;

        /// Overall suspicion level derived from implausible-hit rate and accuracy on a large
        /// enough sample. Conservative thresholds: a lead to spectate, never a verdict.
        ///
        /// @return Suspicion level.
        auto suspicion() const noexcept -> suspicion_level;
    }; // struct player_statistics final
private:
    /// Maximum amount of stored log records (a superset that serves both consumers' views).
    static constexpr std::size_t capacity = 256;

    static inline std::deque<shot_record> buffer;
    static inline std::map<std::uint16_t, player_statistics> player_stats;

    /// Enrich, analyze and record a single shot. Shared by incoming (other players') and
    /// outgoing (the local player's own) bullet packets.
    static auto process_shot(std::uint16_t shooter_id, std::uint8_t hit_type, std::uint16_t hit_id,
                             const types::vector_3d& origin, const types::vector_3d& hit,
                             const types::vector_3d& offset, std::uint8_t weapon_id,
                             types::zstring_t source) -> void;
public:
    /// Get all collected shot records (newest first).
    ///
    /// @return Reference to the records buffer.
    static inline auto records() noexcept -> const std::deque<shot_record>&;

    /// Get per-player aggregated statistics keyed by player ID.
    ///
    /// @return Reference to the statistics map.
    static inline auto statistics() noexcept -> const std::map<std::uint16_t, player_statistics>&;

    /// Clear the rolling log of shot records (does not touch the statistics).
    static inline auto clear() noexcept -> void;

    /// Reset all accumulated per-player statistics (does not touch the log).
    static inline auto reset_statistics() noexcept -> void;

    /// Handle SA:MP event for the shooting collector.
    ///
    /// @param event[in] SA:MP event information.
    /// @return          Whether the event should continue processing.
    static auto on_event(const samp::event_info& event) -> bool;
}; // class shooting final

} // namespace plugin::server

inline auto plugin::server::shooting::records() noexcept -> const std::deque<shot_record>& {
    return buffer;
}

inline auto plugin::server::shooting::statistics() noexcept -> const std::map<std::uint16_t, player_statistics>& {
    return player_stats;
}

inline auto plugin::server::shooting::clear() noexcept -> void {
    buffer.clear();
}

inline auto plugin::server::shooting::reset_statistics() noexcept -> void {
    player_stats.clear();
}

#endif // GADMIN_PLUGIN_SERVER_SHOOTING_H
