/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/video_encoder_factory.h"

#include <optional>
#include <string>

#include "absl/algorithm/container.h"
#include "api/video/resolution.h"
#include "api/video_codecs/scalability_mode.h"
#include "api/video_codecs/sdp_video_format.h"

namespace webrtc {

VideoEncoderFactory::CodecSupport VideoEncoderFactory::QueryCodecSupport(
    const SdpVideoFormat& format,
    std::optional<std::string> scalability_mode,
    std::optional<Resolution> resolution) const {
  // Default implementation, query for supported formats and check if the
  // specified format is supported. If a scalability mode is specified, check
  // that it is present in the matching format's scalability_modes list.
  for (const auto& supported_format : GetSupportedFormats()) {
    if (supported_format.IsSameCodec(format)) {
      if (!scalability_mode.has_value()) {
        return {.is_supported = true};
      } else {
        // Unable to use existing scalability mode parser due to circular
        // dependency between api/video_codecs and modules/video_coding.
        return {.is_supported = absl::c_any_of(
                    supported_format.scalability_modes,
                    [&](ScalabilityMode supported_mode) {
                      return ScalabilityModeToString(supported_mode) ==
                             *scalability_mode;
                    })};
      }
    }
  }
  return {.is_supported = false};
}

}  // namespace webrtc
