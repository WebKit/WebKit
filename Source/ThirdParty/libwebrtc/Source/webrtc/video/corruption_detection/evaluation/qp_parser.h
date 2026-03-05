/*
 * Copyright 2025 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_CORRUPTION_DETECTION_EVALUATION_QP_PARSER_H_
#define VIDEO_CORRUPTION_DETECTION_EVALUATION_QP_PARSER_H_

#include <cstddef>
#include <cstdint>
#include <optional>

#include "api/array_view.h"
#include "api/video/video_codec_type.h"
#include "modules/video_coding/utility/qp_parser.h"
#include "video/corruption_detection/evaluation/av1_qp_parser.h"

namespace webrtc {

// This class is a wrapper around `Av1QpParser` and `QpParser`. It is used to
// parse the QP value for any of the following codecs: AV1, VP8, VP9, H264 and
// H265.
class GenericQpParser {
 public:
  std::optional<uint32_t> Parse(VideoCodecType codec_type,
                                size_t spatial_idx,
                                ArrayView<const uint8_t> frame_data,
                                int operating_point = 0);

 private:
  Av1QpParser av1_parser_;
  QpParser non_av1_parsers_;
};

}  // namespace webrtc

#endif  // VIDEO_CORRUPTION_DETECTION_EVALUATION_QP_PARSER_H_
