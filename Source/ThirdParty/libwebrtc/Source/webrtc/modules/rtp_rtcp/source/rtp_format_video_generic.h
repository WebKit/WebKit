/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VIDEO_GENERIC_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VIDEO_GENERIC_H_

#include <string>

#include "webrtc/base/constructormagic.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_format.h"
#include "webrtc/typedefs.h"

namespace webrtc {
namespace RtpFormatVideoGeneric {
static const uint8_t kKeyFrameBit = 0x01;
static const uint8_t kFirstPacketBit = 0x02;
}  // namespace RtpFormatVideoGeneric

class RtpPacketizerGeneric : public RtpPacketizer {
 public:
  // Initialize with payload from encoder.
  // The payload_data must be exactly one encoded generic frame.
  RtpPacketizerGeneric(FrameType frametype,
                       size_t max_payload_len,
                       size_t last_packet_reduction_len);

  virtual ~RtpPacketizerGeneric();

  // Returns total number of packets to be generated.
  size_t SetPayloadData(const uint8_t* payload_data,
                        size_t payload_size,
                        const RTPFragmentationHeader* fragmentation) override;

  // Get the next payload with generic payload header.
  // Write payload and set marker bit of the |packet|.
  // Returns true on success, false otherwise.
  bool NextPacket(RtpPacketToSend* packet) override;

  ProtectionType GetProtectionType() override;

  StorageType GetStorageType(uint32_t retransmission_settings) override;

  std::string ToString() override;

 private:
  const uint8_t* payload_data_;
  size_t payload_size_;
  const size_t max_payload_len_;
  const size_t last_packet_reduction_len_;
  FrameType frame_type_;
  size_t payload_len_per_packet_;
  uint8_t generic_header_;
  // Number of packets yet to be retrieved by NextPacket() call.
  size_t num_packets_left_;
  // Number of packets, which will be 1 byte more than the rest.
  size_t num_larger_packets_;

  RTC_DISALLOW_COPY_AND_ASSIGN(RtpPacketizerGeneric);
};

// Depacketizer for generic codec.
class RtpDepacketizerGeneric : public RtpDepacketizer {
 public:
  virtual ~RtpDepacketizerGeneric() {}

  bool Parse(ParsedPayload* parsed_payload,
             const uint8_t* payload_data,
             size_t payload_data_length) override;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VIDEO_GENERIC_H_
