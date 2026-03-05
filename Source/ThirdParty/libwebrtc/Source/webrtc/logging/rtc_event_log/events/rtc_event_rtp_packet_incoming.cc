/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/events/rtc_event_rtp_packet_incoming.h"

#include <memory>

#include "absl/memory/memory.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"

namespace webrtc {

RtcEventRtpPacketIncoming::RtcEventRtpPacketIncoming(
    const RtpPacketReceived& packet)
    : packet_(packet) {}

RtcEventRtpPacketIncoming::~RtcEventRtpPacketIncoming() = default;

std::unique_ptr<RtcEventRtpPacketIncoming> RtcEventRtpPacketIncoming::Copy()
    const {
  return absl::WrapUnique(new RtcEventRtpPacketIncoming(*this));
}

}  // namespace webrtc
