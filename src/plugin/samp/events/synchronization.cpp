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

#include "plugin/samp/events/synchronization.h"
#include "plugin/log.h"
#include <algorithm>
#include <cstdint>
#include <format>
#include <string>

namespace {

/// Temporary diagnostic: dump the raw bytes and read offset of a bullet stream so the
/// exact packet layout (and our read alignment) can be verified from gadmin.log.
auto dump_bullet_stream(plugin::types::zstring_t tag, plugin::samp::bit_stream* stream) -> void {
    int total_bytes = stream->get_number_of_bytes_used();
    int read_offset_bits = stream->get_read_offset();
    const auto* data = reinterpret_cast<const unsigned char*>(stream->get_data_ptr());

    std::string hex;

    for (int i = 0, count = std::min(total_bytes, 48); i < count; i++)
        hex += std::format("{:02X} ", data[i]);

    plugin::log::info("[shotdbg] {} raw: bytes={} read_offset_bits={} data=[ {}]", tag, total_bytes, read_offset_bits, hex);
}

} // namespace

plugin::samp::common_synchronization_info::common_synchronization_info(bit_stream* stream, int skip_bytes, bool check_keys) {
    stream->ignore_bytes(0x1);

    player_id = stream->read<std::uint16_t>();

    if (skip_bytes != 0)
        stream->ignore_bytes(skip_bytes);

    if (check_keys) {
        if (stream->read<bool>())
            left_right_keys = stream->read<std::uint16_t>();

        if (stream->read<bool>())
            up_down_keys = stream->read<std::uint16_t>();
    } else {
        left_right_keys = stream->read<std::uint16_t>();
        up_down_keys = stream->read<std::uint16_t>();
    }

    keys_data = stream->read<std::uint16_t>();
}

plugin::samp::event<plugin::samp::event_id::bullet_synchronization, plugin::samp::event_type::incoming_packet>::event(bit_stream* stream) {
    dump_bullet_stream("incoming bullet", stream);
    stream->read_into(player_id, hit_type, hit_id, origin, hit, offset, weapon_id);
    log::info("[shotdbg] incoming parsed: player={} hit_type={} hit_id={} weapon={} origin=({:.1f},{:.1f},{:.1f}) hit=({:.1f},{:.1f},{:.1f}) offset=({:.2f},{:.2f},{:.2f})",
              player_id, hit_type, hit_id, weapon_id, origin.x, origin.y, origin.z, hit.x, hit.y, hit.z, offset.x, offset.y, offset.z);
}

plugin::samp::event<plugin::samp::event_id::bullet_synchronization, plugin::samp::event_type::outgoing_packet>::event(bit_stream* stream) {
    // Layout: [packet id][hit_type][hit_id][origin][hit][offset][weapon_id]. No `player_id`
    // for outgoing bullets (the server knows the sender). Skip the leading packet id byte.
    dump_bullet_stream("outgoing bullet", stream);
    stream->ignore_bytes(0x1);
    stream->read_into(hit_type, hit_id, origin, hit, offset, weapon_id);
    log::info("[shotdbg] outgoing parsed: hit_type={} hit_id={} weapon={} origin=({:.1f},{:.1f},{:.1f}) hit=({:.1f},{:.1f},{:.1f}) offset=({:.2f},{:.2f},{:.2f})",
              hit_type, hit_id, weapon_id, origin.x, origin.y, origin.z, hit.x, hit.y, hit.z, offset.x, offset.y, offset.z);
}
