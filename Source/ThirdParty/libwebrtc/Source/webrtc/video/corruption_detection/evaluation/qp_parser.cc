/*
 * Copyright 2025 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "video/corruption_detection/evaluation/qp_parser.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>

#include "api/video/video_codec_type.h"
#include "modules/video_coding/utility/av1_qp_parser.h"
#include "rtc_base/checks.h"

namespace webrtc {

GenericQpParser::GenericQpParser() : av1_parser_(Av1QpParser::Create()) {}
GenericQpParser::~GenericQpParser() = default;

std::optional<uint32_t> GenericQpParser::Parse(
    VideoCodecType codec_type,
    size_t spatial_idx,
    std::span<const uint8_t> frame_data,
    int operating_point) {
  if (codec_type != kVideoCodecAV1) {
    return non_av1_parsers_.Parse(codec_type, spatial_idx, frame_data.data(),
                                  frame_data.size());
  }

  RTC_DCHECK_EQ(codec_type, kVideoCodecAV1);
  return av1_parser_->Parse(frame_data, operating_point);
}

}  // namespace webrtc
