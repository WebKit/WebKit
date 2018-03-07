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
#include "modules/rtp_rtcp/source/rtp_format_video_stereo.h"
#include "modules/rtp_rtcp/source/rtp_format_vp8.h"
#include "modules/rtp_rtcp/source/rtp_format_vp9.h"

namespace webrtc {
RtpPacketizer* RtpPacketizer::Create(RtpVideoCodecTypes type,
                                     size_t max_payload_len,
                                     size_t last_packet_reduction_len,
                                     const RTPVideoTypeHeader* rtp_type_header,
                                     FrameType frame_type) {
  switch (type) {
    case kRtpVideoH264:
      RTC_CHECK(rtp_type_header);
      return new RtpPacketizerH264(max_payload_len, last_packet_reduction_len,
                                   rtp_type_header->H264.packetization_mode);
    case kRtpVideoVp8:
      RTC_CHECK(rtp_type_header);
      return new RtpPacketizerVp8(rtp_type_header->VP8, max_payload_len,
                                  last_packet_reduction_len);
    case kRtpVideoVp9:
      RTC_CHECK(rtp_type_header);
      return new RtpPacketizerVp9(rtp_type_header->VP9, max_payload_len,
                                  last_packet_reduction_len);
    case kRtpVideoStereo:
      return new RtpPacketizerStereo(rtp_type_header->stereo, frame_type,
                                     max_payload_len,
                                     last_packet_reduction_len);
    case kRtpVideoGeneric:
      return new RtpPacketizerGeneric(frame_type, max_payload_len,
                                      last_packet_reduction_len);
    case kRtpVideoNone:
      RTC_NOTREACHED();
  }
  return nullptr;
}

RtpDepacketizer* RtpDepacketizer::Create(RtpVideoCodecTypes type) {
  switch (type) {
    case kRtpVideoH264:
      return new RtpDepacketizerH264();
    case kRtpVideoVp8:
      return new RtpDepacketizerVp8();
    case kRtpVideoVp9:
      return new RtpDepacketizerVp9();
    case kRtpVideoStereo:
      return new RtpDepacketizerStereo();
    case kRtpVideoGeneric:
      return new RtpDepacketizerGeneric();
    case kRtpVideoNone:
      assert(false);
  }
  return nullptr;
}
}  // namespace webrtc
