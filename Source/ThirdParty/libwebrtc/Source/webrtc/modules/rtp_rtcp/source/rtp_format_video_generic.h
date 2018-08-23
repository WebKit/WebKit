/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VIDEO_GENERIC_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VIDEO_GENERIC_H_

#include <string>

#include "common_types.h"  // NOLINT(build/include)
#include "modules/rtp_rtcp/source/rtp_format.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {
namespace RtpFormatVideoGeneric {
static const uint8_t kKeyFrameBit = 0x01;
static const uint8_t kFirstPacketBit = 0x02;
// If this bit is set, there will be an extended header contained in this
// packet. This was added later so old clients will not send this.
static const uint8_t kExtendedHeaderBit = 0x04;
}  // namespace RtpFormatVideoGeneric

class RtpPacketizerGeneric : public RtpPacketizer {
 public:
  // Initialize with payload from encoder.
  // The payload_data must be exactly one encoded generic frame.
  RtpPacketizerGeneric(const RTPVideoHeader& rtp_video_header,
                       FrameType frametype,
                       size_t max_payload_len,
                       size_t last_packet_reduction_len);

  ~RtpPacketizerGeneric() override;

  // Returns total number of packets to be generated.
  size_t SetPayloadData(const uint8_t* payload_data,
                        size_t payload_size,
                        const RTPFragmentationHeader* fragmentation) override;

  // Get the next payload with generic payload header.
  // Write payload and set marker bit of the |packet|.
  // Returns true on success, false otherwise.
  bool NextPacket(RtpPacketToSend* packet) override;

  std::string ToString() override;

 private:
  const absl::optional<uint16_t> picture_id_;
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

  void WriteExtendedHeader(uint8_t* out_ptr);

  RTC_DISALLOW_COPY_AND_ASSIGN(RtpPacketizerGeneric);
};

// Depacketizer for generic codec.
class RtpDepacketizerGeneric : public RtpDepacketizer {
 public:
  ~RtpDepacketizerGeneric() override;

  bool Parse(ParsedPayload* parsed_payload,
             const uint8_t* payload_data,
             size_t payload_data_length) override;
};
}  // namespace webrtc
#endif  // MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_VIDEO_GENERIC_H_
