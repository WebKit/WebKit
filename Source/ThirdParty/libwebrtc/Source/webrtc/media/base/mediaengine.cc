/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/base/mediaengine.h"

#include "api/video/video_bitrate_allocation.h"
#include "rtc_base/stringencode.h"

namespace cricket {

RtpCapabilities::RtpCapabilities() = default;
RtpCapabilities::~RtpCapabilities() = default;

webrtc::RtpParameters CreateRtpParametersWithOneEncoding() {
  webrtc::RtpParameters parameters;
  webrtc::RtpEncodingParameters encoding;
  parameters.encodings.push_back(encoding);
  return parameters;
}

webrtc::RtpParameters CreateRtpParametersWithEncodings(StreamParams sp) {
  std::vector<uint32_t> primary_ssrcs;
  sp.GetPrimarySsrcs(&primary_ssrcs);
  size_t encoding_count = primary_ssrcs.size();

  std::vector<webrtc::RtpEncodingParameters> encodings(encoding_count);
  for (size_t i = 0; i < encodings.size(); ++i) {
    encodings[i].ssrc = primary_ssrcs[i];
  }
  webrtc::RtpParameters parameters;
  parameters.encodings = encodings;
  parameters.rtcp.cname = sp.cname;
  return parameters;
}

webrtc::RTCError ValidateRtpParameters(
    const webrtc::RtpParameters& old_rtp_parameters,
    const webrtc::RtpParameters& rtp_parameters) {
  using webrtc::RTCErrorType;
  if (rtp_parameters.encodings.size() != old_rtp_parameters.encodings.size()) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INVALID_MODIFICATION,
        "Attempted to set RtpParameters with different encoding count");
  }
  if (rtp_parameters.rtcp != old_rtp_parameters.rtcp) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INVALID_MODIFICATION,
        "Attempted to set RtpParameters with modified RTCP parameters");
  }
  if (rtp_parameters.header_extensions !=
      old_rtp_parameters.header_extensions) {
    LOG_AND_RETURN_ERROR(
        RTCErrorType::INVALID_MODIFICATION,
        "Attempted to set RtpParameters with modified header extensions");
  }

  for (size_t i = 0; i < rtp_parameters.encodings.size(); ++i) {
    if (rtp_parameters.encodings[i].ssrc !=
        old_rtp_parameters.encodings[i].ssrc) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_MODIFICATION,
                           "Attempted to set RtpParameters with modified SSRC");
    }
    if (rtp_parameters.encodings[i].bitrate_priority <= 0) {
      LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_RANGE,
                           "Attempted to set RtpParameters bitrate_priority to "
                           "an invalid number. bitrate_priority must be > 0.");
    }

    if (rtp_parameters.encodings[i].min_bitrate_bps &&
        rtp_parameters.encodings[i].max_bitrate_bps) {
      if (*rtp_parameters.encodings[i].max_bitrate_bps <
          *rtp_parameters.encodings[i].min_bitrate_bps) {
        LOG_AND_RETURN_ERROR(webrtc::RTCErrorType::INVALID_RANGE,
                             "Attempted to set RtpParameters min bitrate "
                             "larger than max bitrate.");
      }
    }
    if (rtp_parameters.encodings[i].num_temporal_layers) {
      if (*rtp_parameters.encodings[i].num_temporal_layers < 1 ||
          *rtp_parameters.encodings[i].num_temporal_layers >
              webrtc::kMaxTemporalStreams) {
        LOG_AND_RETURN_ERROR(RTCErrorType::INVALID_RANGE,
                             "Attempted to set RtpParameters "
                             "num_temporal_layers to an invalid number.");
      }
    }
    if (i > 0 && (rtp_parameters.encodings[i].num_temporal_layers !=
                  rtp_parameters.encodings[i - 1].num_temporal_layers)) {
      LOG_AND_RETURN_ERROR(
          RTCErrorType::INVALID_MODIFICATION,
          "Attempted to set RtpParameters num_temporal_layers "
          "at encoding layer i: " +
              rtc::ToString(i) +
              " to a different value than other encoding layers.");
    }
  }
  return webrtc::RTCError::OK();
}

};  // namespace cricket
