/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "logging/rtc_event_log/events/rtc_event_rtp_packet_outgoing.h"

#include <cstdint>
#include <memory>
#include <optional>

#include "absl/memory/memory.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"

namespace webrtc {

RtcEventRtpPacketOutgoing::RtcEventRtpPacketOutgoing(
    const RtpPacketToSend& packet,
    int probe_cluster_id,
    std::optional<uint16_t> rtx_original_sequence_number)
    : packet_(packet),
      probe_cluster_id_(probe_cluster_id),
      rtx_original_sequence_number_(rtx_original_sequence_number) {}

RtcEventRtpPacketOutgoing::~RtcEventRtpPacketOutgoing() = default;

std::unique_ptr<RtcEventRtpPacketOutgoing> RtcEventRtpPacketOutgoing::Copy()
    const {
  return absl::WrapUnique(new RtcEventRtpPacketOutgoing(*this));
}

}  // namespace webrtc
