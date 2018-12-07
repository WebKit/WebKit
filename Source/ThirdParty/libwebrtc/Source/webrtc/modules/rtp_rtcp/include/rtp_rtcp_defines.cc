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
#include "modules/rtp_rtcp/source/rtp_packet.h"

#include <ctype.h>
#include <string.h>
#include <algorithm>
#include <type_traits>

#include "api/array_view.h"

namespace webrtc {

StreamDataCounters::StreamDataCounters() : first_packet_time_ms(-1) {}

constexpr size_t StreamId::kMaxSize;

// Check if passed character is a "token-char" from RFC 4566.
static bool IsTokenChar(char ch) {
  return ch == 0x21 || (ch >= 0x23 && ch <= 0x27) || ch == 0x2a || ch == 0x2b ||
         ch == 0x2d || ch == 0x2e || (ch >= 0x30 && ch <= 0x39) ||
         (ch >= 0x41 && ch <= 0x5a) || (ch >= 0x5e && ch <= 0x7e);
}

bool StreamId::IsLegalMidName(rtc::ArrayView<const char> name) {
  return (name.size() <= kMaxSize && name.size() > 0 &&
          std::all_of(name.data(), name.data() + name.size(), IsTokenChar));
}

bool StreamId::IsLegalRsidName(rtc::ArrayView<const char> name) {
  return (name.size() <= kMaxSize && name.size() > 0 &&
          std::all_of(name.data(), name.data() + name.size(), isalnum));
}

void StreamId::Set(const char* data, size_t size) {
  // If |data| contains \0, the stream id size might become less than |size|.
  RTC_CHECK_LE(size, kMaxSize);
  memcpy(value_, data, size);
  if (size < kMaxSize)
    value_[size] = 0;
}

// StreamId is used as member of RTPHeader that is sometimes copied with memcpy
// and thus assume trivial destructibility.
static_assert(std::is_trivially_destructible<StreamId>::value, "");

PayloadUnion::PayloadUnion(const AudioPayload& payload) : payload_(payload) {}
PayloadUnion::PayloadUnion(const VideoPayload& payload) : payload_(payload) {}
PayloadUnion::PayloadUnion(const PayloadUnion&) = default;
PayloadUnion::PayloadUnion(PayloadUnion&&) = default;
PayloadUnion::~PayloadUnion() = default;

PayloadUnion& PayloadUnion::operator=(const PayloadUnion&) = default;
PayloadUnion& PayloadUnion::operator=(PayloadUnion&&) = default;

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
      payload_size(payload_size),
      unacknowledged_data(0),
      local_net_id(local_net_id),
      remote_net_id(remote_net_id),
      pacing_info(pacing_info) {}

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
