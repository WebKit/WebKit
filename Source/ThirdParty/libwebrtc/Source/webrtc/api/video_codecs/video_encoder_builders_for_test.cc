/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video_codecs/video_encoder_builders_for_test.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "api/units/data_size.h"
#include "api/video_codecs/video_encoder_interface.h"

namespace webrtc {

FrameEncodeSettingsBuilderForTest& FrameEncodeSettingsBuilderForTest::Cbr(
    const VideoEncoderInterface::FrameEncodeSettings::Cbr& cbr_settings) {
  return CbrRateOptions(cbr_settings.duration, cbr_settings.target_bitrate);
}

FrameEncodeSettingsBuilderForTest& FrameEncodeSettingsBuilderForTest::Cqp(
    int target_qp) {
  return CqpRateOptions(target_qp);
}

FrameEncodeSettingsBuilderForTest& FrameEncodeSettingsBuilderForTest::S(
    int spatial_id) {
  return SpatialId(spatial_id);
}

FrameEncodeSettingsBuilderForTest& FrameEncodeSettingsBuilderForTest::T(
    int temporal_id) {
  return TemporalId(temporal_id);
}

FrameEncodeSettingsBuilderForTest& FrameEncodeSettingsBuilderForTest::Res(
    int width,
    int height) {
  return Resolution({.width = width, .height = height});
}

FrameEncodeSettingsBuilderForTest& FrameEncodeSettingsBuilderForTest::Upd(
    std::optional<int> update_buffer) {
  return UpdateBuffer(update_buffer);
}

FrameEncodeSettingsBuilderForTest& FrameEncodeSettingsBuilderForTest::Ref(
    std::vector<int> reference_buffers) {
  return ReferenceBuffers(std::move(reference_buffers));
}

FrameEncodeSettingsBuilderForTest& FrameEncodeSettingsBuilderForTest::Key() {
  return FrameType(VideoEncoderInterface::FrameType::kKeyframe);
}

FrameEncodeSettingsBuilderForTest& FrameEncodeSettingsBuilderForTest::Start() {
  return FrameType(VideoEncoderInterface::FrameType::kStartFrame);
}

FrameEncodeSettingsBuilderForTest& FrameEncodeSettingsBuilderForTest::Delta() {
  return FrameType(VideoEncoderInterface::FrameType::kDeltaFrame);
}

FrameEncodeSettingsBuilderForTest& FrameEncodeSettingsBuilderForTest::Effort(
    int effort_level) {
  return EffortLevel(effort_level);
}

FrameEncodeSettingsBuilderForTest& FrameEncodeSettingsBuilderForTest::Out(
    TestEncodedOutput& out) {
  struct FrameOut : public VideoEncoderInterface::FrameOutput {
    explicit FrameOut(TestEncodedOutput& e) : eo(e) {}
    std::span<uint8_t> GetBitstreamOutputBuffer(DataSize size) override {
      eo.bitstream.resize(size.bytes());
      return eo.bitstream;
    }
    void EncodeComplete(
        const VideoEncoderInterface::EncodeResult& encode_result) override {
      eo.res = encode_result;
    }
    TestEncodedOutput& eo;
  };
  return FrameOutput(std::make_unique<FrameOut>(out));
}

}  // namespace webrtc
