/*
 * Copyright 2025 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "video/corruption_detection/evaluation/av1_qp_parser.h"

#include <cstdint>
#include <optional>

#include "api/array_view.h"
#include "third_party/libgav1/src/src/buffer_pool.h"
#include "third_party/libgav1/src/src/gav1/status_code.h"
#include "third_party/libgav1/src/src/obu_parser.h"

namespace webrtc {

std::optional<uint32_t> Av1QpParser::Parse(ArrayView<const uint8_t> frame_data,
                                           int operating_point) {
  libgav1::RefCountedBufferPtr curr_frame;
  libgav1::ObuParser parser(frame_data.data(), frame_data.size(),
                            operating_point, &buffer_pool_, &decoder_state_);
  uint8_t highest_acceptable_spatial_layers_qp;

  // Since the temporal unit can have more than 1 frame in scalable coding, we
  // go through all the frame's.
  while (parser.HasData()) {
    // If the frame is not a keyframe, the `parser` must know the information
    // from `sequence_header` to parse the OBU properly.
    if (sequence_header_) {
      parser.set_sequence_header(*sequence_header_);
    }
    if (parser.ParseOneFrame(&curr_frame) != libgav1::kStatusOk) {
      return std::nullopt;
    }

    // Get QP from frame header.
    const auto& frame_header = parser.frame_header();
    // frame_header.quantizer.base_index returns 0 if based on `operating_point`
    // we are not interested in higher spatial layer's QP values.
    if (frame_header.quantizer.base_index != 0) {
      highest_acceptable_spatial_layers_qp = frame_header.quantizer.base_index;
    }

    // Update the state for the next frame.
    if (parser.sequence_header_changed()) {
      sequence_header_ = parser.sequence_header();
    }
    decoder_state_.UpdateReferenceFrames(curr_frame,
                                         frame_header.refresh_frame_flags);
  }

  return highest_acceptable_spatial_layers_qp;
}

}  // namespace webrtc
