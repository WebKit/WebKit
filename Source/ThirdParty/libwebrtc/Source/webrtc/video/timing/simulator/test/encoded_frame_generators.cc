/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/simulator/test/encoded_frame_generators.h"

#include <cstdint>
#include <memory>

#include "api/environment/environment.h"
#include "api/rtp_packet_info.h"
#include "api/rtp_packet_infos.h"
#include "api/units/timestamp.h"
#include "api/video/encoded_frame.h"
#include "rtc_base/checks.h"
#include "test/fake_encoded_frame.h"

namespace webrtc::video_timing_simulator {

namespace {

constexpr int kFrameSizeBytes = 2000;
constexpr int kPayloadType = 96;

}  // namespace

EncodedFrameBuilderGenerator::EncodedFrameBuilderGenerator(
    const Environment& env)
    : env_(env) {}
EncodedFrameBuilderGenerator::~EncodedFrameBuilderGenerator() = default;

EncodedFrameBuilderGenerator::BuilderWithFrameId
EncodedFrameBuilderGenerator::NextEncodedFrameBuilder() {
  test::FakeFrameBuilder builder;
  Timestamp now = env_.clock().CurrentTime();
  builder = builder.Time(rtp_timestamp_)
                .Id(frame_id_)
                .AsLast()
                .SpatialLayer(0)
                .ReceivedTime(now)
                .Size(kFrameSizeBytes)
                .PayloadType(kPayloadType)
                .PacketInfos(RtpPacketInfos(
                    {RtpPacketInfo(kSsrc, /*csrcs=*/{}, rtp_timestamp_, now)}));
  BuilderWithFrameId ret = {.builder = builder, .frame_id = frame_id_};

  // 30 fps.
  rtp_timestamp_ += 3000;
  ++frame_id_;

  return ret;
}

SingleLayerEncodedFrameGenerator::SingleLayerEncodedFrameGenerator(
    const Environment& env)
    : builder_generator_(env) {}
SingleLayerEncodedFrameGenerator::~SingleLayerEncodedFrameGenerator() = default;

std::unique_ptr<EncodedFrame>
SingleLayerEncodedFrameGenerator::NextEncodedFrame() {
  EncodedFrameBuilderGenerator::BuilderWithFrameId builder_and_frame_id =
      builder_generator_.NextEncodedFrameBuilder();
  if (builder_and_frame_id.frame_id == 0) {
    // Keyframe.
    return builder_and_frame_id.builder.Build();
  }
  return builder_and_frame_id.builder.Refs({builder_and_frame_id.frame_id - 1})
      .Build();
}

TemporalLayersEncodedFrameGenerator::TemporalLayersEncodedFrameGenerator(
    const Environment& env)
    : builder_generator_(env) {}
TemporalLayersEncodedFrameGenerator::~TemporalLayersEncodedFrameGenerator() =
    default;

std::unique_ptr<EncodedFrame>
TemporalLayersEncodedFrameGenerator::NextEncodedFrame() {
  EncodedFrameBuilderGenerator::BuilderWithFrameId builder_and_frame_id =
      builder_generator_.NextEncodedFrameBuilder();
  int64_t frame_id = builder_and_frame_id.frame_id;
  if (frame_id == 0) {
    // Keyframe.
    return builder_and_frame_id.builder.Build();
  }
  test::FakeFrameBuilder builder = builder_and_frame_id.builder;
  int64_t mod = frame_id % kNumTemporalLayers;
  if (mod == 0) {
    // The keyframe is handled above.
    RTC_CHECK_GE(frame_id, 4);
    return builder.Refs({frame_id - 4}).Build();
  } else if (mod == 1) {
    RTC_CHECK_GE(frame_id, 1);
    return builder.Refs({frame_id - 1}).Build();
  } else if (mod == 2) {
    RTC_CHECK_GE(frame_id, 2);
    return builder.Refs({frame_id - 2}).Build();
  } else if (mod == 3) {
    RTC_CHECK_GE(frame_id, 3);
    return builder.Refs({frame_id - 1}).Build();
  }
  RTC_CHECK_NOTREACHED();
  return test::FakeFrameBuilder().Build();
}

}  // namespace webrtc::video_timing_simulator
