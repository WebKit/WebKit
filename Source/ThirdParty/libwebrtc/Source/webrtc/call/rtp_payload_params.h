/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_RTP_PAYLOAD_PARAMS_H_
#define CALL_RTP_PAYLOAD_PARAMS_H_

#include <map>
#include <vector>

#include "api/video_codecs/video_encoder.h"
#include "call/rtp_config.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/rtp_rtcp/source/rtp_video_header.h"

namespace webrtc {

class RTPFragmentationHeader;
class RtpRtcp;

// State for setting picture id and tl0 pic idx, for VP8 and VP9
// TODO(nisse): Make these properties not codec specific.
class RtpPayloadParams final {
 public:
  RtpPayloadParams(const uint32_t ssrc, const RtpPayloadState* state);
  ~RtpPayloadParams();

  RTPVideoHeader GetRtpVideoHeader(
      const EncodedImage& image,
      const CodecSpecificInfo* codec_specific_info);

  uint32_t ssrc() const;

  RtpPayloadState state() const;

 private:
  void Set(RTPVideoHeader* rtp_video_header, bool first_frame_in_picture);

  const uint32_t ssrc_;
  RtpPayloadState state_;
};
}  // namespace webrtc
#endif  // CALL_RTP_PAYLOAD_PARAMS_H_
