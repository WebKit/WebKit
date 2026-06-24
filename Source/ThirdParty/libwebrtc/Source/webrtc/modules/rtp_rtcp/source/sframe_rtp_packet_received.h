/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_SFRAME_RTP_PACKET_RECEIVED_H_
#define MODULES_RTP_RTCP_SOURCE_SFRAME_RTP_PACKET_RECEIVED_H_

#include <cstdint>
#include <memory>
#include <utility>

#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/sframe_descriptor.h"

namespace webrtc {

// Wraps an RtpPacketReceived whose 1-byte SFrame payload descriptor has
// already been parsed and stripped from the payload.  Carries the parsed
// SFrameDescriptor alongside the packet so the SFramePacketBuffer can use
// S/E/T bits for assembly without re-parsing.
class SframeRtpPacketReceived {
 public:
  SframeRtpPacketReceived(std::unique_ptr<RtpPacketReceived> packet,
                          const SFrameDescriptor& descriptor)
      : packet_(std::move(packet)), descriptor_(descriptor) {}

  const RtpPacketReceived& packet() const { return *packet_; }
  RtpPacketReceived& packet() { return *packet_; }
  std::unique_ptr<RtpPacketReceived> TakePacket() { return std::move(packet_); }

  const SFrameDescriptor& descriptor() const { return descriptor_; }

  // Forwarding accessors for common RTP fields.
  uint16_t SequenceNumber() const { return packet_->SequenceNumber(); }
  uint32_t Timestamp() const { return packet_->Timestamp(); }
  uint8_t PayloadType() const { return packet_->PayloadType(); }

 private:
  std::unique_ptr<RtpPacketReceived> packet_;
  SFrameDescriptor descriptor_;
};

}  // namespace webrtc

#endif  // MODULES_RTP_RTCP_SOURCE_SFRAME_RTP_PACKET_RECEIVED_H_
