/*
 * Copyright 2025 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/utility/av1_qp_parser.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "third_party/libgav1/src/src/buffer_pool.h"
#include "third_party/libgav1/src/src/decoder_state.h"
#include "third_party/libgav1/src/src/gav1/decoder.h"
#include "third_party/libgav1/src/src/gav1/status_code.h"
#include "third_party/libgav1/src/src/obu_parser.h"

namespace webrtc {
namespace {

class Av1QpHeaderParser : public Av1QpParser {
 public:
  Av1QpHeaderParser()
      : buffer_pool_(/*on_frame_buffer_size_changed=*/nullptr,
                     /*get_frame_buffer=*/nullptr,
                     /*release_frame_buffer=*/nullptr,
                     /*callback_private_data=*/nullptr) {}

  std::optional<uint32_t> Parse(std::span<const uint8_t> frame_data) override {
    return Parse(frame_data, 0);
  }

  std::optional<uint32_t> Parse(std::span<const uint8_t> frame_data,
                                int operating_point) override {
    libgav1::RefCountedBufferPtr curr_frame;
    libgav1::ObuParser parser(frame_data.data(), frame_data.size(),
                              operating_point, &buffer_pool_, &decoder_state_);
    std::optional<uint32_t> highest_acceptable_spatial_layers_qp;

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
      // frame_header.quantizer.base_index returns 0 if based on
      // `operating_point` we are not interested in higher spatial layer's QP
      // values.
      if (frame_header.quantizer.base_index != 0) {
        highest_acceptable_spatial_layers_qp =
            frame_header.quantizer.base_index;
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

 private:
  libgav1::BufferPool buffer_pool_;
  libgav1::DecoderState decoder_state_;
  std::optional<libgav1::ObuSequenceHeader> sequence_header_ = std::nullopt;
};

class Av1QpAverageParser : public Av1QpParser {
 public:
  Av1QpAverageParser() = default;

  std::optional<uint32_t> Parse(std::span<const uint8_t> frame_data) override {
    return Parse(frame_data, 0);
  }

  std::optional<uint32_t> Parse(std::span<const uint8_t> frame_data,
                                int operating_point) override {
    if (!decoder_ || operating_point_ != operating_point) {
      libgav1::DecoderSettings settings;
      settings.parse_only = true;
      settings.operating_point = operating_point;
      decoder_ = std::make_unique<libgav1::Decoder>();
      if (decoder_->Init(&settings) != libgav1::kStatusOk) {
        decoder_.reset();
        return std::nullopt;
      }
      operating_point_ = operating_point;
    }

    if (decoder_->EnqueueFrame(frame_data.data(), frame_data.size(), 0,
                               nullptr) != libgav1::kStatusOk) {
      return std::nullopt;
    }

    const libgav1::DecoderBuffer* buffer = nullptr;
    if (decoder_->DequeueFrame(&buffer) != libgav1::kStatusOk) {
      return std::nullopt;
    }

    std::vector<int> qps = decoder_->GetFramesMeanQpInTemporalUnit();
    if (qps.empty()) {
      return std::nullopt;
    }
    return qps.back();
  }

 private:
  std::unique_ptr<libgav1::Decoder> decoder_;
  int operating_point_ = -1;
};

}  // namespace

std::unique_ptr<Av1QpParser> Av1QpParser::Create(Settings settings) {
  if (settings.use_average_qp) {
    return std::make_unique<Av1QpAverageParser>();
  }
  return std::make_unique<Av1QpHeaderParser>();
}

}  // namespace webrtc
