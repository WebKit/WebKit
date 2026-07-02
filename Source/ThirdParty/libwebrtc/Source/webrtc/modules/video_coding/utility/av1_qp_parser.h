/*
 * Copyright 2025 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_UTILITY_AV1_QP_PARSER_H_
#define MODULES_VIDEO_CODING_UTILITY_AV1_QP_PARSER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <span>

namespace webrtc {

// In RTC we expect AV1 to be configured with `AOM_USAGE_REALTIME`, see
// webrtc/modules/video_coding/codecs/av1/libaom_av1_encoder.cc. In this
// mode AV1 is expected to only have one "temporal" frame per temporal unit.
// Hence, in this implementation we do not take into consideration scenarios
// such as having multiple frames in one temporal unit, as specified
// in https://norkin.org/research/av1_decoder_model/index.html Fig 2.
//
// Although, in scalable encoding mode, AV1 can have several spatial layers in
// one temporal unit. But these must be placed in one temporal unit as described
// in AV1 documentation 7.5.
//
// Note that the parser is stateful - each decoded frame of the bitstream
// must be passed to the parser, starting with a keyframe, in order for the
// state to be valid.
class Av1QpParser {
 public:
  struct Settings {
    Settings() : use_average_qp(false) {}
    // When false, the frame global QP (from the frame header) is returned.
    // When true, the full frame is parsed and the true average QP is returned.
    bool use_average_qp;
  };

  virtual ~Av1QpParser() = default;

  static std::unique_ptr<Av1QpParser> Create(Settings settings = Settings());

  // Parse the bitstream and return the base frame QP for the highest
  // available spatial layer of the frame.
  virtual std::optional<uint32_t> Parse(
      std::span<const uint8_t> frame_data) = 0;

  // Parse the bitstream and return the base frame QP for the given operating
  // point. The mapping to spatial layers is:
  //  `operating_point` = #total_num_spatial_layer - wanted_spatial_layer.
  virtual std::optional<uint32_t> Parse(std::span<const uint8_t> frame_data,
                                        int operating_point) = 0;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_UTILITY_AV1_QP_PARSER_H_
