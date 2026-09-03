/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/video_encoder_builders.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

#include "api/function_view.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/video/resolution.h"
#include "api/video_codecs/video_encoder_factory_interface.h"
#include "api/video_codecs/video_encoder_interface.h"
#include "api/video_codecs/video_encoding_general.h"
#include "rtc_base/numerics/rational.h"

namespace webrtc {

// =============================================================================
// FrameEncodeSettingsBuilder implementation
// =============================================================================

FrameEncodeSettingsBuilder::FrameEncodeSettingsBuilder() {
  class IgnoredOutput : public VideoEncoderInterface::FrameOutput {
   public:
    std::span<uint8_t> GetBitstreamOutputBuffer(DataSize size) override {
      unread_.resize(size.bytes());
      return unread_;
    }
    void EncodeComplete(
        const VideoEncoderInterface::EncodeResult& /* encode_result */)
        override {}

   private:
    std::vector<uint8_t> unread_;
  };

  FrameOutput(std::make_unique<IgnoredOutput>());
}

FrameEncodeSettingsBuilder& FrameEncodeSettingsBuilder::CqpRateOptions(
    int target_qp) {
  settings_.set_cqp_options(target_qp);
  return *this;
}

FrameEncodeSettingsBuilder& FrameEncodeSettingsBuilder::CbrRateOptions(
    TimeDelta duration,
    DataRate target_bitrate) {
  settings_.set_cbr_options(duration, target_bitrate);
  return *this;
}

FrameEncodeSettingsBuilder& FrameEncodeSettingsBuilder::FrameType(
    VideoEncoderInterface::FrameType frame_type) {
  settings_.set_frame_type(frame_type);
  return *this;
}

FrameEncodeSettingsBuilder& FrameEncodeSettingsBuilder::TemporalId(
    int temporal_id) {
  settings_.set_temporal_id(temporal_id);
  return *this;
}

FrameEncodeSettingsBuilder& FrameEncodeSettingsBuilder::SpatialId(
    int spatial_id) {
  settings_.set_spatial_id(spatial_id);
  return *this;
}

FrameEncodeSettingsBuilder& FrameEncodeSettingsBuilder::Resolution(
    webrtc::Resolution resolution) {
  settings_.set_resolution(resolution);
  return *this;
}

FrameEncodeSettingsBuilder& FrameEncodeSettingsBuilder::ReferenceBuffers(
    std::vector<int> reference_buffers) {
  settings_.set_reference_buffers(std::move(reference_buffers));
  return *this;
}

FrameEncodeSettingsBuilder& FrameEncodeSettingsBuilder::UpdateBuffer(
    std::optional<int> update_buffer) {
  settings_.set_update_buffer(update_buffer);
  return *this;
}

FrameEncodeSettingsBuilder& FrameEncodeSettingsBuilder::EffortLevel(
    int effort_level) {
  settings_.set_effort_level(effort_level);
  return *this;
}

FrameEncodeSettingsBuilder& FrameEncodeSettingsBuilder::FrameOutput(
    std::unique_ptr<VideoEncoderInterface::FrameOutput> frame_output) {
  settings_.set_frame_output(std::move(frame_output));
  return *this;
}

VideoEncoderInterface::FrameEncodeSettings FrameEncodeSettingsBuilder::Build() {
  return std::move(settings_);
}

FrameEncodeSettingsBuilder::operator VideoEncoderInterface::
    FrameEncodeSettings() {
  return Build();
}

// =============================================================================
// StaticEncoderSettingsBuilder implementation
// =============================================================================

StaticEncoderSettingsBuilder& StaticEncoderSettingsBuilder::MaxEncodeDimensions(
    webrtc::Resolution max_encode_dimensions) {
  settings_.set_max_encode_dimensions(max_encode_dimensions);
  return *this;
}

StaticEncoderSettingsBuilder& StaticEncoderSettingsBuilder::EncodingFormat(
    webrtc::EncodingFormat encoding_format) {
  settings_.set_encoding_format(encoding_format);
  return *this;
}

StaticEncoderSettingsBuilder& StaticEncoderSettingsBuilder::RcMode(
    std::variant<VideoEncoderFactoryInterface::StaticEncoderSettings::Cqp,
                 VideoEncoderFactoryInterface::StaticEncoderSettings::Cbr>
        rc_mode) {
  settings_.set_rc_mode(std::move(rc_mode));
  return *this;
}

StaticEncoderSettingsBuilder& StaticEncoderSettingsBuilder::CqpRcMode() {
  settings_.set_rc_mode(
      VideoEncoderFactoryInterface::StaticEncoderSettings::Cqp());
  return *this;
}

StaticEncoderSettingsBuilder& StaticEncoderSettingsBuilder::CbrRcMode(
    TimeDelta max_buffer_size,
    TimeDelta target_buffer_size) {
  settings_.set_rc_mode(
      VideoEncoderFactoryInterface::StaticEncoderSettings::Cbr{
          .max_buffer_size = max_buffer_size,
          .target_buffer_size = target_buffer_size});
  return *this;
}

StaticEncoderSettingsBuilder& StaticEncoderSettingsBuilder::MaxNumberOfThreads(
    int max_number_of_threads) {
  settings_.set_max_number_of_threads(max_number_of_threads);
  return *this;
}

VideoEncoderFactoryInterface::StaticEncoderSettings
StaticEncoderSettingsBuilder::Build() {
  return settings_;
}

// =============================================================================
// PredictionConstraintsBuilder implementation
// =============================================================================

PredictionConstraintsBuilder& PredictionConstraintsBuilder::NumBuffers(
    int num_buffers) {
  constraints_.set_num_buffers(num_buffers);
  return *this;
}

PredictionConstraintsBuilder& PredictionConstraintsBuilder::MaxReferences(
    int max_references) {
  constraints_.set_max_references(max_references);
  return *this;
}

PredictionConstraintsBuilder& PredictionConstraintsBuilder::MaxTemporalLayers(
    int max_temporal_layers) {
  constraints_.set_max_temporal_layers(max_temporal_layers);
  return *this;
}

PredictionConstraintsBuilder& PredictionConstraintsBuilder::BufferSpaceType(
    VideoEncoderFactoryInterface::Capabilities::PredictionConstraints::
        BufferSpaceType buffer_space_type) {
  constraints_.set_buffer_space_type(buffer_space_type);
  return *this;
}

PredictionConstraintsBuilder& PredictionConstraintsBuilder::MaxSpatialLayers(
    int max_spatial_layers) {
  constraints_.set_max_spatial_layers(max_spatial_layers);
  return *this;
}

PredictionConstraintsBuilder& PredictionConstraintsBuilder::ScalingFactors(
    std::vector<Rational> scaling_factors) {
  constraints_.set_scaling_factors(std::move(scaling_factors));
  return *this;
}

PredictionConstraintsBuilder& PredictionConstraintsBuilder::SupportedFrameTypes(
    std::vector<VideoEncoderInterface::FrameType> supported_frame_types) {
  constraints_.set_supported_frame_types(std::move(supported_frame_types));
  return *this;
}

VideoEncoderFactoryInterface::Capabilities::PredictionConstraints
PredictionConstraintsBuilder::Build() {
  return constraints_;
}

// =============================================================================
// CapabilitiesBuilder implementation
// =============================================================================

CapabilitiesBuilder& CapabilitiesBuilder::WithPredictionConstraints(
    FunctionView<void(
        VideoEncoderFactoryInterface::Capabilities::PredictionConstraints&)>
        f) {
  VideoEncoderFactoryInterface::Capabilities::PredictionConstraints
      constraints = capabilities_.prediction_constraints();
  f(constraints);
  capabilities_.set_prediction_constraints(std::move(constraints));
  return *this;
}

CapabilitiesBuilder& CapabilitiesBuilder::WithInputConstraints(
    FunctionView<void(
        VideoEncoderFactoryInterface::Capabilities::InputConstraints&)> f) {
  VideoEncoderFactoryInterface::Capabilities::InputConstraints constraints =
      capabilities_.input_constraints();
  f(constraints);
  capabilities_.set_input_constraints(std::move(constraints));
  return *this;
}

CapabilitiesBuilder& CapabilitiesBuilder::WithBitrateControl(
    FunctionView<
        void(VideoEncoderFactoryInterface::Capabilities::BitrateControl&)> f) {
  VideoEncoderFactoryInterface::Capabilities::BitrateControl bitrate_control =
      capabilities_.bitrate_control();
  f(bitrate_control);
  capabilities_.set_bitrate_control(std::move(bitrate_control));
  return *this;
}

CapabilitiesBuilder& CapabilitiesBuilder::WithPerformance(
    FunctionView<void(VideoEncoderFactoryInterface::Capabilities::Performance&)>
        f) {
  VideoEncoderFactoryInterface::Capabilities::Performance performance =
      capabilities_.performance();
  f(performance);
  capabilities_.set_performance(std::move(performance));
  return *this;
}

CapabilitiesBuilder& CapabilitiesBuilder::EncodingFormats(
    std::vector<EncodingFormat> encoding_formats) {
  capabilities_.set_encoding_formats(std::move(encoding_formats));
  return *this;
}

VideoEncoderFactoryInterface::Capabilities CapabilitiesBuilder::Build() {
  return capabilities_;
}

}  // namespace webrtc
