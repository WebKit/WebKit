/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VIDEO_ENCODER_BUILDERS_H_
#define API_VIDEO_CODECS_VIDEO_ENCODER_BUILDERS_H_

#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "api/function_view.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/video/resolution.h"
#include "api/video_codecs/video_encoder_factory_interface.h"
#include "api/video_codecs/video_encoder_interface.h"
#include "api/video_codecs/video_encoding_general.h"
#include "rtc_base/numerics/rational.h"

namespace webrtc {

class FrameEncodeSettingsBuilder {
 public:
  FrameEncodeSettingsBuilder();

  FrameEncodeSettingsBuilder& CqpRateOptions(int target_qp);
  FrameEncodeSettingsBuilder& CbrRateOptions(TimeDelta duration,
                                             DataRate target_bitrate);
  FrameEncodeSettingsBuilder& FrameType(
      VideoEncoderInterface::FrameType frame_type);
  FrameEncodeSettingsBuilder& TemporalId(int temporal_id);
  FrameEncodeSettingsBuilder& SpatialId(int spatial_id);
  FrameEncodeSettingsBuilder& Resolution(Resolution resolution);
  FrameEncodeSettingsBuilder& ReferenceBuffers(
      std::vector<int> reference_buffers);
  FrameEncodeSettingsBuilder& UpdateBuffer(std::optional<int> update_buffer);
  FrameEncodeSettingsBuilder& EffortLevel(int effort_level);
  FrameEncodeSettingsBuilder& FrameOutput(
      std::unique_ptr<VideoEncoderInterface::FrameOutput> frame_output);

  VideoEncoderInterface::FrameEncodeSettings Build();

  explicit operator VideoEncoderInterface::FrameEncodeSettings();

 private:
  VideoEncoderInterface::FrameEncodeSettings settings_;
};

class StaticEncoderSettingsBuilder {
 public:
  StaticEncoderSettingsBuilder() = default;

  StaticEncoderSettingsBuilder& MaxEncodeDimensions(
      Resolution max_encode_dimensions);
  StaticEncoderSettingsBuilder& EncodingFormat(EncodingFormat encoding_format);
  StaticEncoderSettingsBuilder& RcMode(
      std::variant<VideoEncoderFactoryInterface::StaticEncoderSettings::Cqp,
                   VideoEncoderFactoryInterface::StaticEncoderSettings::Cbr>
          rc_mode);
  StaticEncoderSettingsBuilder& CqpRcMode();
  StaticEncoderSettingsBuilder& CbrRcMode(TimeDelta max_buffer_size,
                                          TimeDelta target_buffer_size);
  StaticEncoderSettingsBuilder& MaxNumberOfThreads(int max_number_of_threads);

  VideoEncoderFactoryInterface::StaticEncoderSettings Build();

 private:
  VideoEncoderFactoryInterface::StaticEncoderSettings settings_;
};

class PredictionConstraintsBuilder {
 public:
  PredictionConstraintsBuilder() = default;

  PredictionConstraintsBuilder& NumBuffers(int num_buffers);
  PredictionConstraintsBuilder& MaxReferences(int max_references);
  PredictionConstraintsBuilder& MaxTemporalLayers(int max_temporal_layers);
  PredictionConstraintsBuilder& BufferSpaceType(
      VideoEncoderFactoryInterface::Capabilities::PredictionConstraints::
          BufferSpaceType buffer_space_type);
  PredictionConstraintsBuilder& MaxSpatialLayers(int max_spatial_layers);
  PredictionConstraintsBuilder& ScalingFactors(
      std::vector<Rational> scaling_factors);
  PredictionConstraintsBuilder& SupportedFrameTypes(
      std::vector<VideoEncoderInterface::FrameType> supported_frame_types);

  VideoEncoderFactoryInterface::Capabilities::PredictionConstraints Build();

 private:
  VideoEncoderFactoryInterface::Capabilities::PredictionConstraints
      constraints_;
};

class CapabilitiesBuilder {
 public:
  CapabilitiesBuilder() = default;

  CapabilitiesBuilder& WithPredictionConstraints(
      FunctionView<void(
          VideoEncoderFactoryInterface::Capabilities::PredictionConstraints&)>
          f);
  CapabilitiesBuilder& WithInputConstraints(
      FunctionView<void(
          VideoEncoderFactoryInterface::Capabilities::InputConstraints&)> f);
  CapabilitiesBuilder& WithBitrateControl(
      FunctionView<
          void(VideoEncoderFactoryInterface::Capabilities::BitrateControl&)> f);
  CapabilitiesBuilder& WithPerformance(
      FunctionView<
          void(VideoEncoderFactoryInterface::Capabilities::Performance&)> f);

  CapabilitiesBuilder& EncodingFormats(
      std::vector<EncodingFormat> encoding_formats);

  VideoEncoderFactoryInterface::Capabilities Build();

 private:
  VideoEncoderFactoryInterface::Capabilities capabilities_;
};

}  // namespace webrtc

#endif  // API_VIDEO_CODECS_VIDEO_ENCODER_BUILDERS_H_
