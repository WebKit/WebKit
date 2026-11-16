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

#include <vector>

#include "api/video/video_frame_buffer.h"
#include "api/video_codecs/video_encoder_factory_interface.h"
#include "api/video_codecs/video_encoder_interface.h"
#include "api/video_codecs/video_encoding_general.h"
#include "rtc_base/numerics/rational.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::NotNull;
using ::testing::UnorderedElementsAre;

using Capabilities = VideoEncoderFactoryInterface::Capabilities;
using PredictionConstraints = Capabilities::PredictionConstraints;
using BufferSpaceType = PredictionConstraints::BufferSpaceType;
using InputConstraints = Capabilities::InputConstraints;
using BitrateControl = Capabilities::BitrateControl;
using RateControlMode = VideoEncoderFactoryInterface::RateControlMode;
using CbrSetting = VideoEncoderFactoryInterface::CbrSetting;
using Performance = Capabilities::Performance;
using FrameType = VideoEncoderInterface::FrameType;
using StaticEncoderSettings =
    VideoEncoderFactoryInterface::StaticEncoderSettings;

TEST(LibaomAv1EncoderFactory, CodecName) {
  EXPECT_THAT(LibaomAv1EncoderFactory().CodecName(), Eq("AV1"));
}

TEST(LibaomAv1EncoderFactory, ImplementationName) {
  EXPECT_THAT(LibaomAv1EncoderFactory().ImplementationName(), Eq("Libaom"));
}

TEST(LibaomAv1EncoderFactory, CodecSpecifics) {
  EXPECT_THAT(LibaomAv1EncoderFactory().CodecSpecifics(), IsEmpty());
}

TEST(LibaomAv1EncoderFactory, ReportsCorrectCapabilities) {
  LibaomAv1EncoderFactory factory;
  Capabilities capabilities = factory.GetEncoderCapabilities();

  // Prediction Constraints
  const PredictionConstraints& pc = capabilities.prediction_constraints();
  EXPECT_EQ(pc.num_buffers(), 8);
  EXPECT_EQ(pc.max_references(), 3);
  EXPECT_EQ(pc.max_temporal_layers(), 4);
  EXPECT_EQ(pc.buffer_space_type(), BufferSpaceType::kSingleKeyframe);
  EXPECT_EQ(pc.max_spatial_layers(), 4);

  // Scaling Factors
  const std::vector<Rational>& scaling_factors = pc.scaling_factors();
  ASSERT_EQ(scaling_factors.size(), 6u);
  EXPECT_EQ(scaling_factors[0].numerator, 16);
  EXPECT_EQ(scaling_factors[0].denominator, 1);
  EXPECT_EQ(scaling_factors[1].numerator, 8);
  EXPECT_EQ(scaling_factors[1].denominator, 1);
  EXPECT_EQ(scaling_factors[2].numerator, 4);
  EXPECT_EQ(scaling_factors[2].denominator, 1);
  EXPECT_EQ(scaling_factors[3].numerator, 2);
  EXPECT_EQ(scaling_factors[3].denominator, 1);
  EXPECT_EQ(scaling_factors[4].numerator, 1);
  EXPECT_EQ(scaling_factors[4].denominator, 1);
  EXPECT_EQ(scaling_factors[5].numerator, 1);
  EXPECT_EQ(scaling_factors[5].denominator, 2);

  // Supported Frame Types
  EXPECT_THAT(pc.supported_frame_types(),
              UnorderedElementsAre(FrameType::kKeyframe, FrameType::kStartFrame,
                                   FrameType::kDeltaFrame));

  // Input Constraints
  const InputConstraints& ic = capabilities.input_constraints();
  EXPECT_EQ(ic.min().width, 64);
  EXPECT_EQ(ic.min().height, 36);
  EXPECT_EQ(ic.max().width, 3840);
  EXPECT_EQ(ic.max().height, 2160);
  EXPECT_EQ(ic.pixel_alignment(), 1);

  const std::vector<VideoFrameBuffer::Type>& input_formats = ic.input_formats();
  ASSERT_EQ(input_formats.size(), 2u);
  EXPECT_EQ(input_formats[0], VideoFrameBuffer::Type::kI420);
  EXPECT_EQ(input_formats[1], VideoFrameBuffer::Type::kNV12);

  // Encoding Formats
  const std::vector<EncodingFormat>& enc_formats =
      capabilities.encoding_formats();
  ASSERT_EQ(enc_formats.size(), 1u);
  EXPECT_EQ(enc_formats[0].sub_sampling, EncodingFormat::SubSampling::k420);
  EXPECT_EQ(enc_formats[0].bit_depth, 8);

  // Bitrate Control
  const BitrateControl& bc = capabilities.bitrate_control();
  EXPECT_EQ(bc.min_qp(), 0);
  EXPECT_EQ(bc.max_qp(), 255);

  EXPECT_THAT(bc.rc_modes(), UnorderedElementsAre(RateControlMode::kCbr,
                                                  RateControlMode::kCqp));

  EXPECT_THAT(bc.supported_cbr_settings(),
              UnorderedElementsAre(CbrSetting::kBufferSizes,
                                   CbrSetting::kMaxIntraBitrateFactor));

  // Performance
  const Performance& perf = capabilities.performance();
  EXPECT_TRUE(perf.encode_on_calling_thread());
  EXPECT_EQ(perf.min_max_effort_level().first, -2);
  EXPECT_EQ(perf.min_max_effort_level().second, 4);
}

TEST(LibaomAv1EncoderFactory, CreateEncoder) {
  LibaomAv1EncoderFactory factory;
  Capabilities capabilities = factory.GetEncoderCapabilities();
  StaticEncoderSettings settings;
  settings.set_max_encode_dimensions({.width = 1280, .height = 720});
  settings.set_encoding_format(capabilities.encoding_formats()[0]);
  settings.set_rc_mode(StaticEncoderSettings::Cqp{});
  EXPECT_THAT(factory.CreateEncoder(settings, {}), NotNull());
}

}  // namespace
}  // namespace webrtc
