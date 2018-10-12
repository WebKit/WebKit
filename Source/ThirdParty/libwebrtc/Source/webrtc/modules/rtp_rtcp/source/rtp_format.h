/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H_
#define MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H_

#include <memory>
#include <string>
#include <vector>

#include "api/array_view.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/include/module_common_types.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {
class RtpPacketToSend;

class RtpPacketizer {
 public:
  struct PayloadSizeLimits {
    int max_payload_len = 1200;
    int first_packet_reduction_len = 0;
    int last_packet_reduction_len = 0;
  };
  static std::unique_ptr<RtpPacketizer> Create(
      VideoCodecType type,
      rtc::ArrayView<const uint8_t> payload,
      PayloadSizeLimits limits,
      // Codec-specific details.
      const RTPVideoHeader& rtp_video_header,
      FrameType frame_type,
      const RTPFragmentationHeader* fragmentation);

  virtual ~RtpPacketizer() = default;

  // Returns number of remaining packets to produce by the packetizer.
  virtual size_t NumPackets() const = 0;

  // Get the next payload with payload header.
  // Write payload and set marker bit of the |packet|.
  // Returns true on success, false otherwise.
  virtual bool NextPacket(RtpPacketToSend* packet) = 0;

  // Split payload_len into sum of integers with respect to |limits|.
  // Returns empty vector on failure.
  static std::vector<int> SplitAboutEqually(int payload_len,
                                            const PayloadSizeLimits& limits);
};

// TODO(sprang): Update the depacketizer to return a std::unqie_ptr with a copy
// of the parsed payload, rather than just a pointer into the incoming buffer.
// This way we can move some parsing out from the jitter buffer into here, and
// the jitter buffer can just store that pointer rather than doing a copy there.
class RtpDepacketizer {
 public:
  struct ParsedPayload {
    RTPVideoHeader& video_header() { return video; }
    const RTPVideoHeader& video_header() const { return video; }
    RTPVideoHeader video;

    const uint8_t* payload;
    size_t payload_length;
    FrameType frame_type;
  };

  static RtpDepacketizer* Create(VideoCodecType type);

  virtual ~RtpDepacketizer() {}

  // Parses the RTP payload, parsed result will be saved in |parsed_payload|.
  virtual bool Parse(ParsedPayload* parsed_payload,
                     const uint8_t* payload_data,
                     size_t payload_data_length) = 0;
};
}  // namespace webrtc
#endif  // MODULES_RTP_RTCP_SOURCE_RTP_FORMAT_H_
