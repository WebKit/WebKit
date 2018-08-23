/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtp_format.h"

#include <utility>

#include "modules/rtp_rtcp/source/rtp_format_h264.h"
#include "modules/rtp_rtcp/source/rtp_format_video_generic.h"
#include "modules/rtp_rtcp/source/rtp_format_vp8.h"
#include "modules/rtp_rtcp/source/rtp_format_vp9.h"

namespace webrtc {
RtpPacketizer* RtpPacketizer::Create(VideoCodecType type,
                                     size_t max_payload_len,
                                     size_t last_packet_reduction_len,
                                     const RTPVideoHeader* rtp_video_header,
                                     FrameType frame_type) {
  RTC_CHECK(type == kVideoCodecGeneric || rtp_video_header);
  switch (type) {
    case kVideoCodecH264: {
      const auto& h264 =
          absl::get<RTPVideoHeaderH264>(rtp_video_header->video_type_header);
      return new RtpPacketizerH264(max_payload_len, last_packet_reduction_len,
                                   h264.packetization_mode);
    }
    case kVideoCodecVP8:
      return new RtpPacketizerVp8(rtp_video_header->vp8(), max_payload_len,
                                  last_packet_reduction_len);
    case kVideoCodecVP9: {
      const auto& vp9 =
          absl::get<RTPVideoHeaderVP9>(rtp_video_header->video_type_header);
      return new RtpPacketizerVp9(vp9, max_payload_len,
                                  last_packet_reduction_len);
    }
    case kVideoCodecGeneric:
      RTC_CHECK(rtp_video_header);
      return new RtpPacketizerGeneric(*rtp_video_header, frame_type,
                                      max_payload_len,
                                      last_packet_reduction_len);
    default:
      RTC_NOTREACHED();
  }
  return nullptr;
}

RtpDepacketizer* RtpDepacketizer::Create(VideoCodecType type) {
  switch (type) {
    case kVideoCodecH264:
      return new RtpDepacketizerH264();
    case kVideoCodecVP8:
      return new RtpDepacketizerVp8();
    case kVideoCodecVP9:
      return new RtpDepacketizerVp9();
    default:
      return new RtpDepacketizerGeneric();
  }
}
}  // namespace webrtc
