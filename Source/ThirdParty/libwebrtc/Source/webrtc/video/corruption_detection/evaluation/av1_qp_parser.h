/*
 * Copyright 2025 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_CORRUPTION_DETECTION_EVALUATION_AV1_QP_PARSER_H_
#define VIDEO_CORRUPTION_DETECTION_EVALUATION_AV1_QP_PARSER_H_

#include <cstdint>
#include <optional>

#include "api/array_view.h"
#include "third_party/libgav1/src/src/buffer_pool.h"
#include "third_party/libgav1/src/src/decoder_state.h"
#include "third_party/libgav1/src/src/obu_parser.h"

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
// To get the QP value for a specific spatial layer use:
//  `operating_point` = #total_num_spatial_layer - wanted_spatial_layer.
// E.g. if the QP for the highest spatial layer is sought use `operating_point`
// = 0.
class Av1QpParser {
 public:
  Av1QpParser()
      : buffer_pool_(/*on_frame_buffer_size_changed=*/nullptr,
                     /*get_frame_buffer=*/nullptr,
                     /*release_frame_buffer=*/nullptr,
                     /*callback_private_data=*/nullptr) {}

  std::optional<uint32_t> Parse(ArrayView<const uint8_t> frame_data,
                                int operating_point = 0);

 private:
  libgav1::BufferPool buffer_pool_;
  libgav1::DecoderState decoder_state_;
  std::optional<libgav1::ObuSequenceHeader> sequence_header_ = std::nullopt;
};

}  // namespace webrtc

#endif  // VIDEO_CORRUPTION_DETECTION_EVALUATION_AV1_QP_PARSER_H_
