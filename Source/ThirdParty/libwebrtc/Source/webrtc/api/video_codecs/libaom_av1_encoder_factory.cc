/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/libaom_av1_encoder_factory.h"

#include <array>
#include <map>
#include <memory>
#include <string>

#include "api/video/video_frame_buffer.h"
#include "api/video_codecs/video_encoder_factory_interface.h"
#include "api/video_codecs/video_encoder_interface.h"
#include "api/video_codecs/video_encoding_general.h"
#include "modules/video_coding/codecs/av1/libaom_av1_encoder_v2.h"
#include "rtc_base/numerics/rational.h"

namespace webrtc {

std::string LibaomAv1EncoderFactory::CodecName() const {
  return "AV1";
}

std::string LibaomAv1EncoderFactory::ImplementationName() const {
  return "Libaom";
}

std::map<std::string, std::string> LibaomAv1EncoderFactory::CodecSpecifics()
    const {
  return {};
}

// clang-format off
// The formater and cpplint have conflicting ideas.
VideoEncoderFactoryInterface::Capabilities
LibaomAv1EncoderFactory::GetEncoderCapabilities() const {
  constexpr int kMaxQp = 63;
  constexpr int kNumBuffers = 8;
  constexpr int kMaxReferences = 3;
  constexpr int kMinEffortLevel = -2;  // Speed 11.
  constexpr int kMaxEffortLevel = 4;   // Speed 5.
  constexpr int kMaxSpatialLayersLimit = 4;
  constexpr int kMaxTemporalLayers = 4;

  constexpr std::array<VideoFrameBuffer::Type, 2> kSupportedInputFormats = {
      VideoFrameBuffer::Type::kI420, VideoFrameBuffer::Type::kNV12};

  constexpr std::array<Rational, 7> kSupportedScalingFactors = {
      {{.numerator = 2, .denominator = 1},
       {.numerator = 1, .denominator = 1},
       {.numerator = 1, .denominator = 2},
       {.numerator = 1, .denominator = 4},
       {.numerator = 1, .denominator = 8},
       {.numerator = 1, .denominator = 16}}};

  return {
      .prediction_constraints = {
           .num_buffers = kNumBuffers,
           .max_references = kMaxReferences,
           .max_temporal_layers = kMaxTemporalLayers,
           .buffer_space_type = VideoEncoderFactoryInterface::Capabilities::
               PredictionConstraints::BufferSpaceType::kSingleKeyframe,
           .max_spatial_layers = kMaxSpatialLayersLimit,
           .scaling_factors = {kSupportedScalingFactors.begin(),
                               kSupportedScalingFactors.end()},
           .supported_frame_types = {FrameType::kKeyframe,
                                     FrameType::kStartFrame,
                                     FrameType::kDeltaFrame}},
      .input_constraints = {
              .min = {.width = 64, .height = 36},
              .max = {.width = 3840, .height = 2160},
              .pixel_alignment = 1,
              .input_formats = {kSupportedInputFormats.begin(),
                                kSupportedInputFormats.end()},
          },
      .encoding_formats = {{.sub_sampling = EncodingFormat::k420,
                            .bit_depth = 8}},
      .rate_control = {
           .qp_range = {0, kMaxQp},
           .rc_modes = {VideoEncoderFactoryInterface::RateControlMode::kCbr,
                        VideoEncoderFactoryInterface::RateControlMode::kCqp}},
      .performance = {.encode_on_calling_thread = true,
                      .min_max_effort_level = {kMinEffortLevel,
                                               kMaxEffortLevel}},
  };
}
// clang-format on

std::unique_ptr<VideoEncoderInterface> LibaomAv1EncoderFactory::CreateEncoder(
    const StaticEncoderSettings& settings,
    const std::map<std::string, std::string>& encoder_specific_settings) {
  auto encoder = std::make_unique<LibaomAv1EncoderV2>();
  if (!encoder->InitEncode(settings, encoder_specific_settings)) {
    return nullptr;
  }
  return encoder;
}

}  // namespace webrtc
