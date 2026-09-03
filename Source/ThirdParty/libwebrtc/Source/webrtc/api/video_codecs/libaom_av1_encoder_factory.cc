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

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "api/video/video_frame_buffer.h"
#include "api/video_codecs/video_encoder_builders.h"
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

VideoEncoderFactoryInterface::Capabilities
LibaomAv1EncoderFactory::GetEncoderCapabilities() const {
  constexpr int kMaxQp = 63;
  constexpr int kNumBuffers = 8;
  constexpr int kMaxReferences = 3;
  constexpr int kMinEffortLevel = -2;  // Speed 11.
  constexpr int kMaxEffortLevel = 4;   // Speed 5.
  constexpr int kMaxSpatialLayersLimit = 4;
  constexpr int kMaxTemporalLayers = 4;

  std::vector<VideoFrameBuffer::Type> supported_input_formats = {
      VideoFrameBuffer::Type::kI420, VideoFrameBuffer::Type::kNV12};

  std::vector<Rational> supported_scaling_factors = {
      {{.numerator = 2, .denominator = 1},
       {.numerator = 1, .denominator = 1},
       {.numerator = 1, .denominator = 2},
       {.numerator = 1, .denominator = 4},
       {.numerator = 1, .denominator = 8},
       {.numerator = 1, .denominator = 16}}};

  return CapabilitiesBuilder()
      .WithPredictionConstraints(
          [&](VideoEncoderFactoryInterface::Capabilities::PredictionConstraints&
                  p) {
            p.set_num_buffers(kNumBuffers);
            p.set_max_references(kMaxReferences);
            p.set_max_temporal_layers(kMaxTemporalLayers);
            p.set_buffer_space_type(
                VideoEncoderFactoryInterface::Capabilities::
                    PredictionConstraints::BufferSpaceType::kSingleKeyframe);
            p.set_max_spatial_layers(kMaxSpatialLayersLimit);
            p.set_scaling_factors(std::move(supported_scaling_factors));
            p.set_supported_frame_types({FrameType::kKeyframe,
                                         FrameType::kStartFrame,
                                         FrameType::kDeltaFrame});
          })
      .WithInputConstraints(
          [&](VideoEncoderFactoryInterface::Capabilities::InputConstraints& i) {
            i.set_min({.width = 64, .height = 36});
            i.set_max({.width = 3840, .height = 2160});
            i.set_pixel_alignment(1);
            i.set_input_formats(std::move(supported_input_formats));
          })
      .EncodingFormats({{.sub_sampling = EncodingFormat::k420, .bit_depth = 8}})
      .WithBitrateControl(
          [&](VideoEncoderFactoryInterface::Capabilities::BitrateControl& b) {
            b.set_qp_range(0, kMaxQp);
            using enum VideoEncoderFactoryInterface::RateControlMode;
            b.set_rc_modes({kCbr, kCqp});
          })
      .WithPerformance(
          [&](VideoEncoderFactoryInterface::Capabilities::Performance& p) {
            p.set_encode_on_calling_thread(true);
            p.set_min_max_effort_level(kMinEffortLevel, kMaxEffortLevel);
          })
      .Build();
}

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
