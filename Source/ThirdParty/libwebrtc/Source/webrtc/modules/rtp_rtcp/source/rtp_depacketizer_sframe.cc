/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_depacketizer_sframe.h"

#include <bitset>
#include <cstddef>
#include <memory>
#include <utility>

#include "api/rtc_error.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/sframe_descriptor.h"
#include "modules/rtp_rtcp/source/sframe_rtp_packet_received.h"

namespace webrtc {

RTCErrorOr<std::unique_ptr<SframeRtpPacketReceived>>
ParseSframeRtpPacketOrError(const RtpPacketReceived& packet) {
  if (packet.payload_size() < SframeDescriptor::kSize) {
    return RTCError::InvalidParameter(
        "RTP payload too short for Sframe descriptor");
  }

  const std::bitset<8> bits(packet.payload()[0]);
  SframeDescriptor descriptor;
  descriptor.start = bits[SframeDescriptor::kSBit];
  descriptor.end = bits[SframeDescriptor::kEBit];
  descriptor.encryption_level = bits[SframeDescriptor::kTBit]
                                    ? SframeEncryptionLevel::kPacket
                                    : SframeEncryptionLevel::kFrame;

  auto stripped = std::make_unique<RtpPacketReceived>(packet);

  const size_t padding_size = stripped->padding_size();
  stripped->SetPadding(0);
  stripped->SetPayload(packet.payload().subspan(SframeDescriptor::kSize));
  stripped->SetPadding(padding_size);
  return std::make_unique<SframeRtpPacketReceived>(std::move(stripped),
                                                   descriptor);
}

}  // namespace webrtc
