/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/events/rtc_event_rtcp_packet_incoming.h"

#include <cstdint>
#include <memory>

#include "absl/memory/memory.h"
#include "api/array_view.h"
#include "api/rtc_event_log/rtc_event.h"

namespace webrtc {

RtcEventRtcpPacketIncoming::RtcEventRtcpPacketIncoming(
    rtc::ArrayView<const uint8_t> packet)
    : packet_(packet.data(), packet.size()) {}

RtcEventRtcpPacketIncoming::RtcEventRtcpPacketIncoming(
    const RtcEventRtcpPacketIncoming& other)
    : RtcEvent(other.timestamp_us_),
      packet_(other.packet_.data(), other.packet_.size()) {}

RtcEventRtcpPacketIncoming::~RtcEventRtcpPacketIncoming() = default;

std::unique_ptr<RtcEventRtcpPacketIncoming> RtcEventRtcpPacketIncoming::Copy()
    const {
  return absl::WrapUnique<RtcEventRtcpPacketIncoming>(
      new RtcEventRtcpPacketIncoming(*this));
}

}  // namespace webrtc
