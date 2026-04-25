/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/events/rtc_event_rtcp_packet_outgoing.h"

#include <cstdint>
#include <memory>

#include "absl/memory/memory.h"
#include "api/array_view.h"
#include "api/rtc_event_log/rtc_event.h"

namespace webrtc {

RtcEventRtcpPacketOutgoing::RtcEventRtcpPacketOutgoing(
    ArrayView<const uint8_t> packet)
    : packet_(packet.data(), packet.size()) {}

RtcEventRtcpPacketOutgoing::RtcEventRtcpPacketOutgoing(
    const RtcEventRtcpPacketOutgoing& other)
    : RtcEvent(other), packet_(other.packet_.data(), other.packet_.size()) {}

RtcEventRtcpPacketOutgoing::~RtcEventRtcpPacketOutgoing() = default;

std::unique_ptr<RtcEventRtcpPacketOutgoing> RtcEventRtcpPacketOutgoing::Copy()
    const {
  return absl::WrapUnique(new RtcEventRtcpPacketOutgoing(*this));
}

}  // namespace webrtc
