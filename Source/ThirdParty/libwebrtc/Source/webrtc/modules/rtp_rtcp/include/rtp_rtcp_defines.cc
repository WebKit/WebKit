/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"

#include <ctype.h>
#include <string.h>

#include <type_traits>

#include "absl/algorithm/container.h"
#include "api/array_view.h"
#include "modules/rtp_rtcp/source/rtp_packet.h"

namespace webrtc {

namespace {
constexpr size_t kMidRsidMaxSize = 16;

// Check if passed character is a "token-char" from RFC 4566.
bool IsTokenChar(char ch) {
  return ch == 0x21 || (ch >= 0x23 && ch <= 0x27) || ch == 0x2a || ch == 0x2b ||
         ch == 0x2d || ch == 0x2e || (ch >= 0x30 && ch <= 0x39) ||
         (ch >= 0x41 && ch <= 0x5a) || (ch >= 0x5e && ch <= 0x7e);
}
}  // namespace

bool IsLegalMidName(absl::string_view name) {
  return (name.size() <= kMidRsidMaxSize && !name.empty() &&
          absl::c_all_of(name, IsTokenChar));
}

bool IsLegalRsidName(absl::string_view name) {
  return (name.size() <= kMidRsidMaxSize && !name.empty() &&
          absl::c_all_of(name, isalnum));
}

StreamDataCounters::StreamDataCounters() : first_packet_time_ms(-1) {}

PacketFeedback::PacketFeedback(int64_t arrival_time_ms,
                               uint16_t sequence_number)
    : PacketFeedback(-1,
                     arrival_time_ms,
                     kNoSendTime,
                     sequence_number,
                     0,
                     0,
                     0,
                     PacedPacketInfo()) {}

PacketFeedback::PacketFeedback(int64_t arrival_time_ms,
                               int64_t send_time_ms,
                               uint16_t sequence_number,
                               size_t payload_size,
                               const PacedPacketInfo& pacing_info)
    : PacketFeedback(-1,
                     arrival_time_ms,
                     send_time_ms,
                     sequence_number,
                     payload_size,
                     0,
                     0,
                     pacing_info) {}

PacketFeedback::PacketFeedback(int64_t creation_time_ms,
                               uint16_t sequence_number,
                               size_t payload_size,
                               uint16_t local_net_id,
                               uint16_t remote_net_id,
                               const PacedPacketInfo& pacing_info)
    : PacketFeedback(creation_time_ms,
                     kNotReceived,
                     kNoSendTime,
                     sequence_number,
                     payload_size,
                     local_net_id,
                     remote_net_id,
                     pacing_info) {}

PacketFeedback::PacketFeedback(int64_t creation_time_ms,
                               int64_t arrival_time_ms,
                               int64_t send_time_ms,
                               uint16_t sequence_number,
                               size_t payload_size,
                               uint16_t local_net_id,
                               uint16_t remote_net_id,
                               const PacedPacketInfo& pacing_info)
    : creation_time_ms(creation_time_ms),
      arrival_time_ms(arrival_time_ms),
      send_time_ms(send_time_ms),
      sequence_number(sequence_number),
      long_sequence_number(0),
      payload_size(payload_size),
      unacknowledged_data(0),
      local_net_id(local_net_id),
      remote_net_id(remote_net_id),
      pacing_info(pacing_info),
      ssrc(0),
      rtp_sequence_number(0) {}

PacketFeedback::PacketFeedback(const PacketFeedback&) = default;
PacketFeedback& PacketFeedback::operator=(const PacketFeedback&) = default;
PacketFeedback::~PacketFeedback() = default;

bool PacketFeedback::operator==(const PacketFeedback& rhs) const {
  return arrival_time_ms == rhs.arrival_time_ms &&
         send_time_ms == rhs.send_time_ms &&
         sequence_number == rhs.sequence_number &&
         payload_size == rhs.payload_size && pacing_info == rhs.pacing_info;
}

void RtpPacketCounter::AddPacket(const RtpPacket& packet) {
  ++packets;
  header_bytes += packet.headers_size();
  padding_bytes += packet.padding_size();
  payload_bytes += packet.payload_size();
}

}  // namespace webrtc
