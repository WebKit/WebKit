/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_RTPPARAMETERS_H_
#define WEBRTC_API_RTPPARAMETERS_H_

#include <string>
#include <vector>

#include "webrtc/base/optional.h"

namespace webrtc {

// These structures are defined as part of the RtpSender interface.
// See http://w3c.github.io/webrtc-pc/#rtcrtpsender-interface for details.
struct RtpEncodingParameters {
  rtc::Optional<uint32_t> ssrc;
  bool active = true;
  int max_bitrate_bps = -1;

  bool operator==(const RtpEncodingParameters& o) const {
    return ssrc == o.ssrc && active == o.active &&
           max_bitrate_bps == o.max_bitrate_bps;
  }
  bool operator!=(const RtpEncodingParameters& o) const {
    return !(*this == o);
  }
};

struct RtpCodecParameters {
  int payload_type;
  std::string mime_type;
  int clock_rate;
  int channels = 1;
  // TODO(deadbeef): Add sdpFmtpLine field.

  bool operator==(const RtpCodecParameters& o) const {
    return payload_type == o.payload_type && mime_type == o.mime_type &&
           clock_rate == o.clock_rate && channels == o.channels;
  }
  bool operator!=(const RtpCodecParameters& o) const { return !(*this == o); }
};

struct RtpParameters {
  std::vector<RtpEncodingParameters> encodings;
  std::vector<RtpCodecParameters> codecs;

  bool operator==(const RtpParameters& o) const {
    return encodings == o.encodings && codecs == o.codecs;
  }
  bool operator!=(const RtpParameters& o) const { return !(*this == o); }
};

}  // namespace webrtc

#endif  // WEBRTC_API_RTPPARAMETERS_H_
