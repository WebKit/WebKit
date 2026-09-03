/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VIDEO_ENCODER_BUILDERS_FOR_TEST_H_
#define API_VIDEO_CODECS_VIDEO_ENCODER_BUILDERS_FOR_TEST_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/video/resolution.h"
#include "api/video_codecs/video_encoder_builders.h"
#include "api/video_codecs/video_encoder_interface.h"

namespace webrtc {

struct TestEncodedOutput {
  std::vector<uint8_t> bitstream;
  VideoEncoderInterface::EncodeResult res;
};

class FrameEncodeSettingsBuilderForTest : public FrameEncodeSettingsBuilder {
 public:
  FrameEncodeSettingsBuilderForTest() = default;

  // Implicit conversion!
  operator VideoEncoderInterface::FrameEncodeSettings() { return Build(); }

  // Overridden base class methods returning FrameEncodeSettingsBuilderForTest&
  FrameEncodeSettingsBuilderForTest& CqpRateOptions(int target_qp) {
    FrameEncodeSettingsBuilder::CqpRateOptions(target_qp);
    return *this;
  }
  FrameEncodeSettingsBuilderForTest& CbrRateOptions(TimeDelta duration,
                                                    DataRate target_bitrate) {
    FrameEncodeSettingsBuilder::CbrRateOptions(duration, target_bitrate);
    return *this;
  }
  FrameEncodeSettingsBuilderForTest& FrameType(
      VideoEncoderInterface::FrameType frame_type) {
    FrameEncodeSettingsBuilder::FrameType(frame_type);
    return *this;
  }
  FrameEncodeSettingsBuilderForTest& TemporalId(int temporal_id) {
    FrameEncodeSettingsBuilder::TemporalId(temporal_id);
    return *this;
  }
  FrameEncodeSettingsBuilderForTest& SpatialId(int spatial_id) {
    FrameEncodeSettingsBuilder::SpatialId(spatial_id);
    return *this;
  }
  FrameEncodeSettingsBuilderForTest& Resolution(webrtc::Resolution resolution) {
    FrameEncodeSettingsBuilder::Resolution(resolution);
    return *this;
  }
  FrameEncodeSettingsBuilderForTest& ReferenceBuffers(
      std::vector<int> reference_buffers) {
    FrameEncodeSettingsBuilder::ReferenceBuffers(std::move(reference_buffers));
    return *this;
  }
  FrameEncodeSettingsBuilderForTest& UpdateBuffer(
      std::optional<int> update_buffer) {
    FrameEncodeSettingsBuilder::UpdateBuffer(update_buffer);
    return *this;
  }
  FrameEncodeSettingsBuilderForTest& EffortLevel(int effort_level) {
    FrameEncodeSettingsBuilder::EffortLevel(effort_level);
    return *this;
  }
  FrameEncodeSettingsBuilderForTest& FrameOutput(
      std::unique_ptr<VideoEncoderInterface::FrameOutput> frame_output) {
    FrameEncodeSettingsBuilder::FrameOutput(std::move(frame_output));
    return *this;
  }

  // Shorthand methods!
  FrameEncodeSettingsBuilderForTest& Cbr(
      const VideoEncoderInterface::FrameEncodeSettings::Cbr& cbr_settings);
  FrameEncodeSettingsBuilderForTest& Cqp(int target_qp);
  FrameEncodeSettingsBuilderForTest& S(int spatial_id);
  FrameEncodeSettingsBuilderForTest& T(int temporal_id);
  FrameEncodeSettingsBuilderForTest& Res(int width, int height);
  FrameEncodeSettingsBuilderForTest& Upd(std::optional<int> update_buffer);
  FrameEncodeSettingsBuilderForTest& Ref(std::vector<int> reference_buffers);
  FrameEncodeSettingsBuilderForTest& Key();
  FrameEncodeSettingsBuilderForTest& Start();
  FrameEncodeSettingsBuilderForTest& Delta();
  FrameEncodeSettingsBuilderForTest& Effort(int effort_level);
  FrameEncodeSettingsBuilderForTest& Out(TestEncodedOutput& out);
};

}  // namespace webrtc

#endif  // API_VIDEO_CODECS_VIDEO_ENCODER_BUILDERS_FOR_TEST_H_
