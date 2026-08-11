/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <optional>
#include <utility>

#include "api/video/i420_buffer.h"  // IWYU pragma: keep
#include "api/video_codecs/libaom_av1_encoder_factory.h"
#include "api/video_codecs/simple_encoder_wrapper.h"
#include "api/video_codecs/video_encoder_factory_interface.h"
#include "api/video_codecs/video_encoding_general.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/frame_reader.h"

namespace webrtc {

using ::testing::Eq;
using ::testing::Gt;
using ::testing::IsEmpty;
using ::testing::Ne;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::UnorderedElementsAre;
using PredictionConstraints =
    VideoEncoderFactoryInterface::Capabilities::PredictionConstraints;

namespace {

std::unique_ptr<test::FrameReader> CreateFrameReader() {
  return CreateY4mFrameReader(
      test::ResourcePath("reference_video_640x360_30fps", "y4m"),
      test::YuvFrameReaderImpl::RepeatMode::kPingPong);
}

TEST(SimpleEncoderWrapper, SupportedSvcModesOnlyL1T1) {
  PredictionConstraints constraints = {
      .num_buffers = 2,
      .max_references = 2,
      .max_temporal_layers = 1,
      .buffer_space_type =
          PredictionConstraints::BufferSpaceType::kSingleKeyframe,
      .max_spatial_layers = 1,
      .scaling_factors = {{.numerator = 1, .denominator = 1}},
  };

  EXPECT_THAT(SimpleEncoderWrapper::SupportedWebrtcSvcModes(constraints),
              UnorderedElementsAre("L1T1"));
}

TEST(SimpleEncoderWrapper, SupportedSvcModesUpToL1T3) {
  PredictionConstraints constraints = {
      .num_buffers = 8,
      .max_references = 1,
      .max_temporal_layers = 3,
      .buffer_space_type =
          PredictionConstraints::BufferSpaceType::kSingleKeyframe,
      .max_spatial_layers = 1,
      .scaling_factors = {{.numerator = 1, .denominator = 1},
                          {.numerator = 1, .denominator = 2}},
  };

  EXPECT_THAT(SimpleEncoderWrapper::SupportedWebrtcSvcModes(constraints),
              UnorderedElementsAre("L1T1", "L1T2", "L1T3"));
}

TEST(SimpleEncoderWrapper, SupportedSvcModesUpToL3T3Key) {
  PredictionConstraints constraints = {
      .num_buffers = 8,
      .max_references = 2,
      .max_temporal_layers = 3,
      .buffer_space_type =
          PredictionConstraints::BufferSpaceType::kSingleKeyframe,
      .max_spatial_layers = 3,
      .scaling_factors = {{.numerator = 1, .denominator = 1},
                          {.numerator = 1, .denominator = 2}},
  };

  EXPECT_THAT(
      SimpleEncoderWrapper::SupportedWebrtcSvcModes(constraints),
      UnorderedElementsAre("L1T1", "L1T2", "L1T3", "L2T1", "L2T1_KEY", "L2T2",
                           "L2T2_KEY", "L2T3", "L2T3_KEY", "L3T1", "L3T1_KEY",
                           "L3T2", "L3T2_KEY", "L3T3", "L3T3_KEY", "S2T1",
                           "S2T2", "S2T3", "S3T1", "S3T2", "S3T3"));
}

TEST(SimpleEncoderWrapper, SupportedSvcModesUpToS3T3) {
  PredictionConstraints constraints = {
      .num_buffers = 8,
      .max_references = 2,
      .max_temporal_layers = 3,
      .buffer_space_type =
          PredictionConstraints::BufferSpaceType::kMultiInstance,
      .max_spatial_layers = 3,
      .scaling_factors = {{.numerator = 1, .denominator = 1},
                          {.numerator = 1, .denominator = 2}},
  };

  EXPECT_THAT(SimpleEncoderWrapper::SupportedWebrtcSvcModes(constraints),
              UnorderedElementsAre("L1T1", "L1T2", "L1T3", "S2T1", "S2T2",
                                   "S2T3", "S3T1", "S3T2", "S3T3"));
}

TEST(SimpleEncoderWrapper, SupportedSvcModesUpToL3T3KeyWithHScaling) {
  PredictionConstraints constraints = {
      .num_buffers = 8,
      .max_references = 2,
      .max_temporal_layers = 3,
      .buffer_space_type =
          PredictionConstraints::BufferSpaceType::kSingleKeyframe,
      .max_spatial_layers = 3,
      .scaling_factors = {{.numerator = 1, .denominator = 1},
                          {.numerator = 1, .denominator = 2},
                          {.numerator = 2, .denominator = 3}},
  };

  EXPECT_THAT(
      SimpleEncoderWrapper::SupportedWebrtcSvcModes(constraints),
      UnorderedElementsAre(
          "L1T1", "L1T2", "L1T3", "L2T1", "L2T1h", "L2T1_KEY", "L2T1h_KEY",
          "L2T2", "L2T2h", "L2T2_KEY", "L2T2h_KEY", "L2T3", "L2T3h", "L2T3_KEY",
          "L2T3h_KEY", "L3T1", "L3T1h", "L3T1_KEY", "L3T1h_KEY", "L3T2",
          "L3T2h", "L3T2_KEY", "L3T2h_KEY", "L3T3", "L3T3h", "L3T3_KEY",
          "L3T3h_KEY", "S2T1", "S2T1h", "S2T2", "S2T2h", "S2T3", "S2T3h",
          "S3T1", "S3T1h", "S3T2", "S3T2h", "S3T3", "S3T3h"));
}

// TD: The encoder wrapper shouldn't really use an actual encoder
// implementation for testing, but hey, this is just a PoC.
TEST(SimpleEncoderWrapper, EncodeL1T1) {
  auto encoder = LibaomAv1EncoderFactory().CreateEncoder(
      {.max_encode_dimensions = {.width = 1080, .height = 720},
       .encoding_format = {.sub_sampling = EncodingFormat::k420,
                           .bit_depth = 8},
       .rc_mode = VideoEncoderFactoryInterface::StaticEncoderSettings::Cqp(),
       .max_number_of_threads = 1},
      {});

  std::unique_ptr<SimpleEncoderWrapper> simple_encoder =
      SimpleEncoderWrapper::Create(std::move(encoder), "L1T1");

  ASSERT_THAT(simple_encoder, NotNull());

  simple_encoder->SetEncodeQp(30);
  simple_encoder->SetEncodeFps(15);
  auto frame_reader = CreateFrameReader();

  int num_callbacks = 0;
  simple_encoder->Encode(
      frame_reader->PullFrame(), /*force_keyframe=*/true,
      [&](const SimpleEncoderWrapper::EncodeResult& result) {
        ++num_callbacks;
        ASSERT_THAT(result.oh_no, Eq(false));
        EXPECT_THAT(result.dependency_structure, Ne(std::nullopt));
        EXPECT_THAT(result.bitstream_data, Not(IsEmpty()));
        EXPECT_THAT(result.frame_type, Eq(FrameType::kKeyframe));
        EXPECT_THAT(result.generic_frame_info.spatial_id, Eq(0));
        EXPECT_THAT(result.generic_frame_info.temporal_id, Eq(0));
      });

  simple_encoder->Encode(
      frame_reader->PullFrame(), /*force_keyframe=*/false,
      [&](const SimpleEncoderWrapper::EncodeResult& result) {
        ++num_callbacks;
        ASSERT_THAT(result.oh_no, Eq(false));
        EXPECT_THAT(result.dependency_structure, Eq(std::nullopt));
        EXPECT_THAT(result.bitstream_data, Not(IsEmpty()));
        EXPECT_THAT(result.frame_type, Eq(FrameType::kDeltaFrame));
        EXPECT_THAT(result.generic_frame_info.spatial_id, Eq(0));
        EXPECT_THAT(result.generic_frame_info.temporal_id, Eq(0));
      });
}

TEST(SimpleEncoderWrapper, EncodeL2T2_KEY) {
  auto encoder = LibaomAv1EncoderFactory().CreateEncoder(
      {.max_encode_dimensions = {.width = 1080, .height = 720},
       .encoding_format = {.sub_sampling = EncodingFormat::k420,
                           .bit_depth = 8},
       .rc_mode = VideoEncoderFactoryInterface::StaticEncoderSettings::Cqp(),
       .max_number_of_threads = 1},
      {});

  std::unique_ptr<SimpleEncoderWrapper> simple_encoder =
      SimpleEncoderWrapper::Create(std::move(encoder), "L2T2_KEY");

  ASSERT_THAT(simple_encoder, NotNull());

  simple_encoder->SetEncodeQp(30);
  simple_encoder->SetEncodeFps(15);
  auto frame_reader = CreateFrameReader();

  int num_callbacks = 0;
  simple_encoder->Encode(
      frame_reader->PullFrame(), /*force_keyframe=*/true,
      [&](const SimpleEncoderWrapper::EncodeResult& result) {
        ASSERT_THAT(result.oh_no, Eq(false));
        if (result.generic_frame_info.spatial_id == 0) {
          ++num_callbacks;
          EXPECT_THAT(result.dependency_structure, Ne(std::nullopt));
          EXPECT_THAT(result.bitstream_data, Not(IsEmpty()));
          EXPECT_THAT(result.frame_type, Eq(FrameType::kKeyframe));
          EXPECT_THAT(result.generic_frame_info.temporal_id, Eq(0));
        } else if (result.generic_frame_info.spatial_id == 1) {
          ++num_callbacks;
          EXPECT_THAT(result.dependency_structure, Eq(std::nullopt));
          EXPECT_THAT(result.bitstream_data, Not(IsEmpty()));
          EXPECT_THAT(result.frame_type, Eq(FrameType::kDeltaFrame));
          EXPECT_THAT(result.generic_frame_info.temporal_id, Eq(0));
        }
      });

  simple_encoder->Encode(
      frame_reader->PullFrame(), /*force_keyframe=*/false,
      [&](const SimpleEncoderWrapper::EncodeResult& result) {
        ASSERT_THAT(result.oh_no, Eq(false));
        if (result.generic_frame_info.spatial_id == 0) {
          ++num_callbacks;
          EXPECT_THAT(result.dependency_structure, Eq(std::nullopt));
          EXPECT_THAT(result.bitstream_data, Not(IsEmpty()));
          EXPECT_THAT(result.frame_type, Eq(FrameType::kDeltaFrame));
          EXPECT_THAT(result.generic_frame_info.temporal_id, Eq(1));
        } else if (result.generic_frame_info.spatial_id == 1) {
          ++num_callbacks;
          EXPECT_THAT(result.dependency_structure, Eq(std::nullopt));
          EXPECT_THAT(result.bitstream_data, Not(IsEmpty()));
          EXPECT_THAT(result.frame_type, Eq(FrameType::kDeltaFrame));
          EXPECT_THAT(result.generic_frame_info.temporal_id, Eq(1));
        }
      });

  EXPECT_THAT(num_callbacks, Eq(4));
}

TEST(SimpleEncoderWrapper, EncodeL1T3ForceKeyframe) {
  auto encoder = LibaomAv1EncoderFactory().CreateEncoder(
      {.max_encode_dimensions = {.width = 1080, .height = 720},
       .encoding_format = {.sub_sampling = EncodingFormat::k420,
                           .bit_depth = 8},
       .rc_mode = VideoEncoderFactoryInterface::StaticEncoderSettings::Cqp(),
       .max_number_of_threads = 1},
      {});

  std::unique_ptr<SimpleEncoderWrapper> simple_encoder =
      SimpleEncoderWrapper::Create(std::move(encoder), "L1T3");

  ASSERT_THAT(simple_encoder, NotNull());

  simple_encoder->SetEncodeQp(30);
  simple_encoder->SetEncodeFps(15);
  auto frame_reader = CreateFrameReader();

  int num_callbacks = 0;
  simple_encoder->Encode(
      frame_reader->PullFrame(), /*force_keyframe=*/true,
      [&](const SimpleEncoderWrapper::EncodeResult& result) {
        ++num_callbacks;
        ASSERT_THAT(result.oh_no, Eq(false));
        EXPECT_THAT(result.frame_type, Eq(FrameType::kKeyframe));
        EXPECT_THAT(result.generic_frame_info.temporal_id, Eq(0));
      });

  simple_encoder->Encode(
      frame_reader->PullFrame(), /*force_keyframe=*/false,
      [&](const SimpleEncoderWrapper::EncodeResult& result) {
        ++num_callbacks;
        ASSERT_THAT(result.oh_no, Eq(false));
        EXPECT_THAT(result.frame_type, Eq(FrameType::kDeltaFrame));
        EXPECT_THAT(result.generic_frame_info.temporal_id, Eq(2));
      });

  simple_encoder->Encode(
      frame_reader->PullFrame(), /*force_keyframe=*/false,
      [&](const SimpleEncoderWrapper::EncodeResult& result) {
        ++num_callbacks;
        ASSERT_THAT(result.oh_no, Eq(false));
        EXPECT_THAT(result.frame_type, Eq(FrameType::kDeltaFrame));
        EXPECT_THAT(result.generic_frame_info.temporal_id, Eq(1));
      });

  simple_encoder->Encode(
      frame_reader->PullFrame(), /*force_keyframe=*/true,
      [&](const SimpleEncoderWrapper::EncodeResult& result) {
        ++num_callbacks;
        ASSERT_THAT(result.oh_no, Eq(false));
        EXPECT_THAT(result.frame_type, Eq(FrameType::kKeyframe));
        EXPECT_THAT(result.generic_frame_info.temporal_id, Eq(0));
      });

  simple_encoder->Encode(
      frame_reader->PullFrame(), /*force_keyframe=*/false,
      [&](const SimpleEncoderWrapper::EncodeResult& result) {
        ++num_callbacks;
        ASSERT_THAT(result.oh_no, Eq(false));
        EXPECT_THAT(result.frame_type, Eq(FrameType::kDeltaFrame));
        EXPECT_THAT(result.generic_frame_info.temporal_id, Eq(2));
      });

  EXPECT_THAT(num_callbacks, Eq(5));
}

}  // namespace
}  // namespace webrtc
