/*
 * Copyright 2025 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "api/environment/environment.h"
#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "api/video/corruption_detection/corruption_detection_filter_settings.h"
#include "api/video/corruption_detection/frame_instrumentation_data.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_type.h"
#include "api/video_codecs/scalability_mode.h"
#include "rtc_base/ref_counted_object.h"
#include "test/create_test_environment.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "video/corruption_detection/frame_instrumentation_generator_impl.h"
#include "video/corruption_detection/utils.h"

namespace webrtc {
namespace {
using ::testing::DoubleEq;
using ::testing::ElementsAre;
using ::testing::Pointwise;

constexpr int kDefaultScaledWidth = 4;
constexpr int kDefaultScaledHeight = 4;

scoped_refptr<I420Buffer> MakeDefaultI420FrameBuffer() {
  // Create an I420 frame of size 4x4.
  const int kDefaultLumaWidth = 4;
  const int kDefaultLumaHeight = 4;
  const int kDefaultChromaWidth = 2;
  const int kDefaultPixelValue = 30;
  std::vector<uint8_t> kDefaultYContent(16, kDefaultPixelValue);
  std::vector<uint8_t> kDefaultUContent(4, kDefaultPixelValue);
  std::vector<uint8_t> kDefaultVContent(4, kDefaultPixelValue);

  return I420Buffer::Copy(kDefaultLumaWidth, kDefaultLumaHeight,
                          kDefaultYContent.data(), kDefaultLumaWidth,
                          kDefaultUContent.data(), kDefaultChromaWidth,
                          kDefaultVContent.data(), kDefaultChromaWidth);
}

scoped_refptr<I420Buffer> MakeI420FrameBufferWithDifferentPixelValues() {
  // Create an I420 frame of size 4x4.
  const int kDefaultLumaWidth = 4;
  const int kDefaultLumaHeight = 4;
  const int kDefaultChromaWidth = 2;
  std::vector<uint8_t> kDefaultYContent = {1, 2,  3,  4,  5,  6,  7,  8,
                                           9, 10, 11, 12, 13, 14, 15, 16};
  std::vector<uint8_t> kDefaultUContent = {17, 18, 19, 20};
  std::vector<uint8_t> kDefaultVContent = {21, 22, 23, 24};

  return I420Buffer::Copy(kDefaultLumaWidth, kDefaultLumaHeight,
                          kDefaultYContent.data(), kDefaultLumaWidth,
                          kDefaultUContent.data(), kDefaultChromaWidth,
                          kDefaultVContent.data(), kDefaultChromaWidth);
}

TEST(FrameInstrumentationGeneratorTest,
     ReturnsNothingWhenNoFramesHaveBeenProvided) {
  const Environment env = CreateTestEnvironment();
  FrameInstrumentationGeneratorImpl generator(
      &env, VideoCodecType::kVideoCodecGeneric, ScalabilityMode::kL1T1);

  EXPECT_FALSE(generator.OnEncodedImage(EncodedImage()).has_value());
}

TEST(FrameInstrumentationGeneratorTest,
     ReturnsNothingWhenNoFrameWithTheSameTimestampIsProvided) {
  const Environment env = CreateTestEnvironment();
  FrameInstrumentationGeneratorImpl generator(
      &env, VideoCodecType::kVideoCodecGeneric, ScalabilityMode::kL1T1);
  VideoFrame frame = VideoFrame::Builder()
                         .set_video_frame_buffer(MakeDefaultI420FrameBuffer())
                         .set_rtp_timestamp(1)
                         .build();
  EncodedImage encoded_image;
  encoded_image.SetRtpTimestamp(2);

  generator.OnCapturedFrame(frame);

  EXPECT_FALSE(generator.OnEncodedImage(encoded_image).has_value());
}

TEST(FrameInstrumentationGeneratorTest,
     ReturnsNothingWhenTheFirstFrameOfASpatialOrSimulcastLayerIsNotAKeyFrame) {
  const Environment env = CreateTestEnvironment();
  FrameInstrumentationGeneratorImpl generator(
      &env, VideoCodecType::kVideoCodecGeneric, ScalabilityMode::kL1T1);
  VideoFrame frame = VideoFrame::Builder()
                         .set_video_frame_buffer(MakeDefaultI420FrameBuffer())
                         .set_rtp_timestamp(1)
                         .build();

  // Delta frame with no preceding key frame.
  EncodedImage encoded_image;
  encoded_image.SetRtpTimestamp(1);
  encoded_image.set_frame_type(VideoFrameType::kVideoFrameDelta);
  encoded_image.SetSpatialIndex(0);
  encoded_image.SetSimulcastIndex(0);

  generator.OnCapturedFrame(frame);

  // The first frame of a spatial or simulcast layer is not a key frame.
  EXPECT_FALSE(generator.OnEncodedImage(encoded_image).has_value());
}

TEST(FrameInstrumentationGeneratorTest,
     ReturnsNothingWhenQpIsUnsetAndNotParseable) {
  const Environment env = CreateTestEnvironment();
  FrameInstrumentationGeneratorImpl generator(
      &env, VideoCodecType::kVideoCodecGeneric, ScalabilityMode::kL1T1);
  VideoFrame frame = VideoFrame::Builder()
                         .set_video_frame_buffer(MakeDefaultI420FrameBuffer())
                         .set_rtp_timestamp(1)
                         .build();

  // Frame where QP is unset and QP is not parseable from the encoded data.
  EncodedImage encoded_image;
  encoded_image.SetRtpTimestamp(1);
  encoded_image.set_frame_type(VideoFrameType::kVideoFrameKey);

  generator.OnCapturedFrame(frame);

  EXPECT_FALSE(generator.OnEncodedImage(encoded_image).has_value());
}

#if GTEST_HAS_DEATH_TEST
TEST(FrameInstrumentationGeneratorTest, FailsWhenCodecIsUnsupported) {
  const Environment env = CreateTestEnvironment();
  // No available mapping from codec to filter parameters.
  FrameInstrumentationGeneratorImpl generator(
      &env, VideoCodecType::kVideoCodecGeneric, ScalabilityMode::kL1T1);
  VideoFrame frame = VideoFrame::Builder()
                         .set_video_frame_buffer(MakeDefaultI420FrameBuffer())
                         .set_rtp_timestamp(1)
                         .build();
  EncodedImage encoded_image;
  encoded_image.SetRtpTimestamp(1);
  encoded_image.set_frame_type(VideoFrameType::kVideoFrameKey);
  encoded_image.qp_ = 10;

  generator.OnCapturedFrame(frame);

  EXPECT_DEATH(generator.OnEncodedImage(encoded_image),
               "Codec type Generic is not supported");
}
#endif  // GTEST_HAS_DEATH_TEST

TEST(FrameInstrumentationGeneratorTest,
     ReturnsInstrumentationDataForVP8KeyFrameWithQpSet) {
  const Environment env = CreateTestEnvironment();
  FrameInstrumentationGeneratorImpl generator(
      &env, VideoCodecType::kVideoCodecVP8, ScalabilityMode::kL1T1);
  VideoFrame frame = VideoFrame::Builder()
                         .set_video_frame_buffer(MakeDefaultI420FrameBuffer())
                         .set_rtp_timestamp(1)
                         .build();
  // VP8 key frame with QP set.
  EncodedImage encoded_image;
  encoded_image.SetRtpTimestamp(1);
  encoded_image.set_frame_type(VideoFrameType::kVideoFrameKey);
  encoded_image.qp_ = 10;
  encoded_image._encodedWidth = kDefaultScaledWidth;
  encoded_image._encodedHeight = kDefaultScaledHeight;

  generator.OnCapturedFrame(frame);
  std::optional<FrameInstrumentationData> frame_instrumentation_data =
      generator.OnEncodedImage(encoded_image);

  ASSERT_TRUE(frame_instrumentation_data.has_value());
  EXPECT_EQ(frame_instrumentation_data->sequence_index(), 0);
  EXPECT_NE(frame_instrumentation_data->std_dev(), 0.0);
  EXPECT_NE(frame_instrumentation_data->luma_error_threshold(), 0);
  EXPECT_NE(frame_instrumentation_data->chroma_error_threshold(), 0);
  EXPECT_FALSE(frame_instrumentation_data->sample_values().empty());
}

TEST(FrameInstrumentationGeneratorTest,
     ReturnsInstrumentationDataWhenQpIsParseable) {
  const Environment env = CreateTestEnvironment();
  FrameInstrumentationGeneratorImpl generator(
      &env, VideoCodecType::kVideoCodecVP8, ScalabilityMode::kL1T1);
  VideoFrame frame = VideoFrame::Builder()
                         .set_video_frame_buffer(MakeDefaultI420FrameBuffer())
                         .set_rtp_timestamp(1)
                         .build();

  // VP8 key frame with parseable QP.
  constexpr uint8_t kCodedFrameVp8Qp25[] = {
      0x10, 0x02, 0x00, 0x9d, 0x01, 0x2a, 0x10, 0x00, 0x10, 0x00,
      0x02, 0x47, 0x08, 0x85, 0x85, 0x88, 0x85, 0x84, 0x88, 0x0c,
      0x82, 0x00, 0x0c, 0x0d, 0x60, 0x00, 0xfe, 0xfc, 0x5c, 0xd0};
  scoped_refptr<EncodedImageBuffer> encoded_image_buffer =
      EncodedImageBuffer::Create(kCodedFrameVp8Qp25,
                                 sizeof(kCodedFrameVp8Qp25));
  EncodedImage encoded_image;
  encoded_image.SetRtpTimestamp(1);
  encoded_image.set_frame_type(VideoFrameType::kVideoFrameKey);
  encoded_image.SetEncodedData(encoded_image_buffer);
  encoded_image._encodedWidth = kDefaultScaledWidth;
  encoded_image._encodedHeight = kDefaultScaledHeight;

  generator.OnCapturedFrame(frame);
  std::optional<FrameInstrumentationData> frame_instrumentation_data =
      generator.OnEncodedImage(encoded_image);

  ASSERT_TRUE(frame_instrumentation_data.has_value());
  EXPECT_EQ(frame_instrumentation_data->sequence_index(), 0);
  EXPECT_NE(frame_instrumentation_data->std_dev(), 0.0);
  EXPECT_NE(frame_instrumentation_data->luma_error_threshold(), 0);
  EXPECT_NE(frame_instrumentation_data->chroma_error_threshold(), 0);
  EXPECT_FALSE(frame_instrumentation_data->sample_values().empty());
}

TEST(FrameInstrumentationGeneratorTest,
     ReturnsInstrumentationDataForUpperLayerOfAnSvcKeyFrame) {
  const Environment env = CreateTestEnvironment();
  FrameInstrumentationGeneratorImpl generator(
      &env, VideoCodecType::kVideoCodecVP9, ScalabilityMode::kL3T1);
  VideoFrame frame = VideoFrame::Builder()
                         .set_video_frame_buffer(MakeDefaultI420FrameBuffer())
                         .set_rtp_timestamp(1)
                         .build();
  EncodedImage encoded_image1;
  encoded_image1.SetRtpTimestamp(1);
  encoded_image1.set_frame_type(VideoFrameType::kVideoFrameKey);
  encoded_image1.SetSpatialIndex(0);
  encoded_image1.qp_ = 10;
  encoded_image1._encodedWidth = kDefaultScaledWidth;
  encoded_image1._encodedHeight = kDefaultScaledHeight;

  // Delta frame that is an upper layer of an SVC key frame.
  EncodedImage encoded_image2;
  encoded_image2.SetRtpTimestamp(1);
  encoded_image2.set_frame_type(VideoFrameType::kVideoFrameDelta);
  encoded_image2.SetSpatialIndex(1);
  encoded_image2.qp_ = 10;
  encoded_image2._encodedWidth = kDefaultScaledWidth;
  encoded_image2._encodedHeight = kDefaultScaledHeight;

  generator.OnCapturedFrame(frame);
  generator.OnEncodedImage(encoded_image1);
  std::optional<FrameInstrumentationData> frame_instrumentation_data =
      generator.OnEncodedImage(encoded_image2);

  ASSERT_TRUE(frame_instrumentation_data.has_value());
  EXPECT_EQ(frame_instrumentation_data->sequence_index(), 0);
  EXPECT_NE(frame_instrumentation_data->std_dev(), 0.0);
  EXPECT_NE(frame_instrumentation_data->luma_error_threshold(), 0);
  EXPECT_NE(frame_instrumentation_data->chroma_error_threshold(), 0);
  EXPECT_FALSE(frame_instrumentation_data->sample_values().empty());
}

TEST(FrameInstrumentationGeneratorTest,
     ReturnsNothingWhenNotEnoughTimeHasPassedSinceLastSampledFrame) {
  const Environment env = CreateTestEnvironment();
  FrameInstrumentationGeneratorImpl generator(
      &env, VideoCodecType::kVideoCodecVP8, ScalabilityMode::kL1T1);
  VideoFrame frame1 = VideoFrame::Builder()
                          .set_video_frame_buffer(MakeDefaultI420FrameBuffer())
                          .set_rtp_timestamp(1)
                          .build();
  VideoFrame frame2 = VideoFrame::Builder()
                          .set_video_frame_buffer(MakeDefaultI420FrameBuffer())
                          .set_rtp_timestamp(2)
                          .build();
  EncodedImage encoded_image1;
  encoded_image1.SetRtpTimestamp(1);
  encoded_image1.set_frame_type(VideoFrameType::kVideoFrameKey);
  encoded_image1.SetSpatialIndex(0);
  encoded_image1.qp_ = 10;
  encoded_image1._encodedWidth = kDefaultScaledWidth;
  encoded_image1._encodedHeight = kDefaultScaledHeight;

  // Delta frame that is too recent in comparison to the last sampled frame:
  // passed time < 90'000.
  EncodedImage encoded_image2;
  encoded_image2.SetRtpTimestamp(2);
  encoded_image2.set_frame_type(VideoFrameType::kVideoFrameDelta);
  encoded_image2.SetSpatialIndex(0);
  encoded_image2.qp_ = 10;
  encoded_image2._encodedWidth = kDefaultScaledWidth;
  encoded_image2._encodedHeight = kDefaultScaledHeight;

  generator.OnCapturedFrame(frame1);
  generator.OnCapturedFrame(frame2);
  generator.OnEncodedImage(encoded_image1);

  ASSERT_FALSE(generator.OnEncodedImage(encoded_image2).has_value());
}

TEST(FrameInstrumentationGeneratorTest,
     ReturnsInstrumentationDataForUpperLayerOfASecondSvcKeyFrame) {
  const Environment env = CreateTestEnvironment();
  FrameInstrumentationGeneratorImpl generator(
      &env, VideoCodecType::kVideoCodecVP9, ScalabilityMode::kL3T1);
  VideoFrame frame1 = VideoFrame::Builder()
                          .set_video_frame_buffer(MakeDefaultI420FrameBuffer())
                          .set_rtp_timestamp(1)
                          .build();
  VideoFrame frame2 = VideoFrame::Builder()
                          .set_video_frame_buffer(MakeDefaultI420FrameBuffer())
                          .set_rtp_timestamp(2)
                          .build();
  for (const VideoFrame& frame : {frame1, frame2}) {
    EncodedImage encoded_image1;
    encoded_image1.SetRtpTimestamp(frame.rtp_timestamp());
    encoded_image1.set_frame_type(VideoFrameType::kVideoFrameKey);
    encoded_image1.SetSpatialIndex(0);
    encoded_image1.qp_ = 10;
    encoded_image1._encodedWidth = kDefaultScaledWidth;
    encoded_image1._encodedHeight = kDefaultScaledHeight;

    EncodedImage encoded_image2;
    encoded_image2.SetRtpTimestamp(frame.rtp_timestamp());
    encoded_image2.set_frame_type(VideoFrameType::kVideoFrameDelta);
    encoded_image2.SetSpatialIndex(1);
    encoded_image2.qp_ = 10;
    encoded_image2._encodedWidth = kDefaultScaledWidth;
    encoded_image2._encodedHeight = kDefaultScaledHeight;

    generator.OnCapturedFrame(frame);

    std::optional<FrameInstrumentationData> data1 =
        generator.OnEncodedImage(encoded_image1);

    std::optional<FrameInstrumentationData> data2 =
        generator.OnEncodedImage(encoded_image2);

    ASSERT_TRUE(data1.has_value());
    ASSERT_TRUE(data2.has_value());

    EXPECT_TRUE(data1->holds_upper_bits());
    EXPECT_TRUE(data2->holds_upper_bits());
  }
}

TEST(FrameInstrumentationGeneratorTest,
     SvcLayersSequenceIndicesIncreaseIndependentOnEachother) {
  const Environment env = CreateTestEnvironment();
  FrameInstrumentationGeneratorImpl generator(
      &env, VideoCodecType::kVideoCodecVP9, ScalabilityMode::kL3T1);
  VideoFrame frame1 =
      VideoFrame::Builder()
          .set_video_frame_buffer(MakeI420FrameBufferWithDifferentPixelValues())
          .set_rtp_timestamp(1)
          .build();
  VideoFrame frame2 =
      VideoFrame::Builder()
          .set_video_frame_buffer(MakeI420FrameBufferWithDifferentPixelValues())
          .set_rtp_timestamp(2)
          .build();
  for (const VideoFrame& frame : {frame1, frame2}) {
    EncodedImage encoded_image1;
    encoded_image1.SetRtpTimestamp(frame.rtp_timestamp());
    encoded_image1.set_frame_type(VideoFrameType::kVideoFrameKey);
    encoded_image1.SetSpatialIndex(0);
    encoded_image1.qp_ = 10;
    encoded_image1._encodedWidth = kDefaultScaledWidth;
    encoded_image1._encodedHeight = kDefaultScaledHeight;

    EncodedImage encoded_image2;
    encoded_image2.SetRtpTimestamp(frame.rtp_timestamp());
    encoded_image2.set_frame_type(VideoFrameType::kVideoFrameDelta);
    encoded_image2.SetSpatialIndex(1);
    encoded_image2.qp_ = 10;
    encoded_image2._encodedWidth = kDefaultScaledWidth;
    encoded_image2._encodedHeight = kDefaultScaledHeight;

    generator.OnCapturedFrame(frame);

    std::optional<FrameInstrumentationData> data1 =
        generator.OnEncodedImage(encoded_image1);

    std::optional<FrameInstrumentationData> data2 =
        generator.OnEncodedImage(encoded_image2);

    ASSERT_TRUE(data1.has_value());
    ASSERT_TRUE(data2.has_value());

    EXPECT_TRUE(data1->holds_upper_bits());
    EXPECT_TRUE(data2->holds_upper_bits());

    EXPECT_EQ(data1->sequence_index(), data2->sequence_index());

    // In the test the frames have equal frame buffers so the sample values
    // should be equal.
    EXPECT_THAT(data1->sample_values(),
                Pointwise(DoubleEq(), data2->sample_values()));
  }
}

TEST(FrameInstrumentationGeneratorTest,
     OutputsDeltaFrameInstrumentationDataForSimulcast) {
  const Environment env = CreateTestEnvironment();
  FrameInstrumentationGeneratorImpl generator(
      &env, VideoCodecType::kVideoCodecVP9, ScalabilityMode::kL3T1);
  bool has_found_delta_frame = false;
  // 34 frames is the minimum number of frames to be able to sample a delta
  // frame.
  for (int i = 0; i < 34; ++i) {
    VideoFrame frame = VideoFrame::Builder()
                           .set_video_frame_buffer(MakeDefaultI420FrameBuffer())
                           .set_rtp_timestamp(i)
                           .build();
    EncodedImage encoded_image1;
    encoded_image1.SetRtpTimestamp(frame.rtp_timestamp());
    encoded_image1.set_frame_type(i == 0 ? VideoFrameType::kVideoFrameKey
                                         : VideoFrameType::kVideoFrameDelta);
    encoded_image1.SetSimulcastIndex(0);
    encoded_image1.qp_ = 10;
    encoded_image1._encodedWidth = kDefaultScaledWidth;
    encoded_image1._encodedHeight = kDefaultScaledHeight;

    EncodedImage encoded_image2;
    encoded_image2.SetRtpTimestamp(frame.rtp_timestamp());
    encoded_image2.set_frame_type(i == 0 ? VideoFrameType::kVideoFrameKey
                                         : VideoFrameType::kVideoFrameDelta);
    encoded_image2.SetSimulcastIndex(1);
    encoded_image2.qp_ = 10;
    encoded_image2._encodedWidth = kDefaultScaledWidth;
    encoded_image2._encodedHeight = kDefaultScaledHeight;

    generator.OnCapturedFrame(frame);

    std::optional<FrameInstrumentationData> data1 =
        generator.OnEncodedImage(encoded_image1);

    std::optional<FrameInstrumentationData> data2 =
        generator.OnEncodedImage(encoded_image2);

    if (i == 0) {
      ASSERT_TRUE(data1.has_value());
      ASSERT_TRUE(data2.has_value());

      EXPECT_TRUE(data1->holds_upper_bits());
      EXPECT_TRUE(data2->holds_upper_bits());
    } else if (data1.has_value() || data2.has_value()) {
      if (data1.has_value()) {
        EXPECT_FALSE(data1->holds_upper_bits());
      }
      if (data2.has_value()) {
        EXPECT_FALSE(data2->holds_upper_bits());
      }
      has_found_delta_frame = true;
    }
  }
  EXPECT_TRUE(has_found_delta_frame);
}

TEST(FrameInstrumentationGeneratorTest,
     SequenceIndexIncreasesCorrectlyAtNewKeyFrame) {
  const Environment env = CreateTestEnvironment();
  FrameInstrumentationGeneratorImpl generator(
      &env, VideoCodecType::kVideoCodecVP8, ScalabilityMode::kL1T1);
  VideoFrame frame1 =
      VideoFrame::Builder()
          .set_video_frame_buffer(MakeI420FrameBufferWithDifferentPixelValues())
          .set_rtp_timestamp(1)
          .build();
  VideoFrame frame2 =
      VideoFrame::Builder()
          .set_video_frame_buffer(MakeI420FrameBufferWithDifferentPixelValues())
          .set_rtp_timestamp(2)
          .build();
  EncodedImage encoded_image1;
  encoded_image1.SetRtpTimestamp(1);
  encoded_image1.set_frame_type(VideoFrameType::kVideoFrameKey);
  encoded_image1.qp_ = 10;
  encoded_image1._encodedWidth = kDefaultScaledWidth;
  encoded_image1._encodedHeight = kDefaultScaledHeight;

  // Delta frame that is an upper layer of an SVC key frame.
  EncodedImage encoded_image2;
  encoded_image2.SetRtpTimestamp(2);
  encoded_image2.set_frame_type(VideoFrameType::kVideoFrameKey);
  encoded_image2.qp_ = 10;
  encoded_image2._encodedWidth = kDefaultScaledWidth;
  encoded_image2._encodedHeight = kDefaultScaledHeight;

  generator.OnCapturedFrame(frame1);
  generator.OnCapturedFrame(frame2);

  ASSERT_EQ(GetSpatialLayerId(encoded_image1),
            GetSpatialLayerId(encoded_image2));
  generator.SetHaltonSequenceIndex(0b0010'1010,
                                   GetSpatialLayerId(encoded_image1));

  std::optional<FrameInstrumentationData> data1 =
      generator.OnEncodedImage(encoded_image1);
  std::optional<FrameInstrumentationData> data2 =
      generator.OnEncodedImage(encoded_image2);

  ASSERT_TRUE(data1.has_value());
  ASSERT_TRUE(data2.has_value());

  EXPECT_EQ(data1->sequence_index(), 0b0000'1000'0000);
  EXPECT_EQ(data2->sequence_index(), 0b0001'0000'0000);
}

TEST(FrameInstrumentationGeneratorTest,
     SequenceIndexThatWouldOverflowTo15BitsIncreasesCorrectlyAtNewKeyFrame) {
  const Environment env = CreateTestEnvironment();
  FrameInstrumentationGeneratorImpl generator(
      &env, VideoCodecType::kVideoCodecVP8, ScalabilityMode::kL1T1);
  VideoFrame frame1 =
      VideoFrame::Builder()
          .set_video_frame_buffer(MakeI420FrameBufferWithDifferentPixelValues())
          .set_rtp_timestamp(1)
          .build();
  VideoFrame frame2 =
      VideoFrame::Builder()
          .set_video_frame_buffer(MakeI420FrameBufferWithDifferentPixelValues())
          .set_rtp_timestamp(2)
          .build();
  EncodedImage encoded_image1;
  encoded_image1.SetRtpTimestamp(1);
  encoded_image1.set_frame_type(VideoFrameType::kVideoFrameKey);
  encoded_image1.qp_ = 10;
  encoded_image1._encodedWidth = kDefaultScaledWidth;
  encoded_image1._encodedHeight = kDefaultScaledHeight;
  encoded_image1.SetSimulcastIndex(0);

  EncodedImage encoded_image2;
  encoded_image2.SetRtpTimestamp(2);
  encoded_image2.set_frame_type(VideoFrameType::kVideoFrameKey);
  encoded_image2.qp_ = 10;
  encoded_image2._encodedWidth = kDefaultScaledWidth;
  encoded_image2._encodedHeight = kDefaultScaledHeight;
  encoded_image2.SetSimulcastIndex(0);

  generator.OnCapturedFrame(frame1);
  generator.OnCapturedFrame(frame2);

  ASSERT_EQ(GetSpatialLayerId(encoded_image1),
            GetSpatialLayerId(encoded_image2));
  generator.SetHaltonSequenceIndex(0b11'1111'1111'1111,
                                   GetSpatialLayerId(encoded_image1));
  std::optional<FrameInstrumentationData> data1 =
      generator.OnEncodedImage(encoded_image1);
  std::optional<FrameInstrumentationData> data2 =
      generator.OnEncodedImage(encoded_image2);

  ASSERT_TRUE(data1.has_value());
  ASSERT_TRUE(data2.has_value());

  EXPECT_EQ(data1->sequence_index(), 0);
  EXPECT_EQ(data2->sequence_index(), 0b1000'0000);
}

TEST(FrameInstrumentationGeneratorTest,
     SequenceIndexIncreasesCorrectlyAtNewKeyFrameAlreadyZeroes) {
  const Environment env = CreateTestEnvironment();
  FrameInstrumentationGeneratorImpl generator(
      &env, VideoCodecType::kVideoCodecVP8, ScalabilityMode::kL1T1);
  VideoFrame frame1 =
      VideoFrame::Builder()
          .set_video_frame_buffer(MakeI420FrameBufferWithDifferentPixelValues())
          .set_rtp_timestamp(1)
          .build();
  VideoFrame frame2 =
      VideoFrame::Builder()
          .set_video_frame_buffer(MakeI420FrameBufferWithDifferentPixelValues())
          .set_rtp_timestamp(2)
          .build();
  EncodedImage encoded_image1;
  encoded_image1.SetRtpTimestamp(1);
  encoded_image1.set_frame_type(VideoFrameType::kVideoFrameKey);
  encoded_image1.qp_ = 10;
  encoded_image1._encodedWidth = kDefaultScaledWidth;
  encoded_image1._encodedHeight = kDefaultScaledHeight;

  // Delta frame that is an upper layer of an SVC key frame.
  EncodedImage encoded_image2;
  encoded_image2.SetRtpTimestamp(2);
  encoded_image2.set_frame_type(VideoFrameType::kVideoFrameKey);
  encoded_image2.qp_ = 10;
  encoded_image2._encodedWidth = kDefaultScaledWidth;
  encoded_image2._encodedHeight = kDefaultScaledHeight;

  generator.OnCapturedFrame(frame1);
  generator.OnCapturedFrame(frame2);

  ASSERT_EQ(GetSpatialLayerId(encoded_image1),
            GetSpatialLayerId(encoded_image2));
  generator.SetHaltonSequenceIndex(0b1000'0000,
                                   GetSpatialLayerId(encoded_image1));

  std::optional<FrameInstrumentationData> data1 =
      generator.OnEncodedImage(encoded_image1);
  std::optional<FrameInstrumentationData> data2 =
      generator.OnEncodedImage(encoded_image2);

  ASSERT_TRUE(data1.has_value());
  ASSERT_TRUE(data2.has_value());

  EXPECT_EQ(data1->sequence_index(), 0b0000'1000'0000);
  EXPECT_EQ(data2->sequence_index(), 0b0001'0000'0000);
}
TEST(FrameInstrumentationGeneratorTest,
     SequenceIndexThatWouldOverflowTo15BitsIncreasesCorrectlyAtNewDeltaFrame) {
  const Environment env = CreateTestEnvironment();
  FrameInstrumentationGeneratorImpl generator(
      &env, VideoCodecType::kVideoCodecVP8, ScalabilityMode::kL1T1);
  generator.OnCapturedFrame(
      VideoFrame::Builder()
          .set_video_frame_buffer(MakeI420FrameBufferWithDifferentPixelValues())
          .set_rtp_timestamp(1)
          .build());
  EncodedImage encoded_image;
  encoded_image.SetRtpTimestamp(1);
  encoded_image.set_frame_type(VideoFrameType::kVideoFrameDelta);
  encoded_image.qp_ = 10;
  encoded_image._encodedWidth = kDefaultScaledWidth;
  encoded_image._encodedHeight = kDefaultScaledHeight;
  encoded_image.SetSimulcastIndex(0);

  constexpr int kMaxSequenceIndex = 0b11'1111'1111'1111;

  generator.SetHaltonSequenceIndex(kMaxSequenceIndex,
                                   GetSpatialLayerId(encoded_image));
  std::optional<FrameInstrumentationData> data =
      generator.OnEncodedImage(encoded_image);

  ASSERT_TRUE(data.has_value());
  EXPECT_EQ(data->sequence_index(), kMaxSequenceIndex);

  // Loop until we get a new delta frame.
  bool has_found_delta_frame = false;
  for (int i = 0; i < 34; ++i) {
    generator.OnCapturedFrame(
        VideoFrame::Builder()
            .set_video_frame_buffer(
                MakeI420FrameBufferWithDifferentPixelValues())
            .set_rtp_timestamp(i + 2)
            .build());

    encoded_image.SetRtpTimestamp(i + 2);

    std::optional<FrameInstrumentationData> frame_instrumentation_data =
        generator.OnEncodedImage(encoded_image);
    if (frame_instrumentation_data.has_value()) {
      has_found_delta_frame = true;
      EXPECT_EQ(frame_instrumentation_data->sequence_index(), 0);
      break;
    }
  }
  EXPECT_TRUE(has_found_delta_frame);
}

TEST(FrameInstrumentationGeneratorTest, GetterAndSetterOperatesAsExpected) {
  const Environment env = CreateTestEnvironment();
  FrameInstrumentationGeneratorImpl generator(
      &env, VideoCodecType::kVideoCodecVP8, ScalabilityMode::kL1T1);
  // `std::nullopt` when uninitialized.
  EXPECT_FALSE(generator.GetHaltonSequenceIndex(1).has_value());

  // Zero is a valid index.
  generator.SetHaltonSequenceIndex(0, 1);
  std::optional<int> index = generator.GetHaltonSequenceIndex(1);
  EXPECT_TRUE(index.has_value());
  EXPECT_EQ(*index, 0);

#if GTEST_HAS_DEATH_TEST
  // Negative values are not allowed to be set.
  EXPECT_DEATH(generator.SetHaltonSequenceIndex(-2, 1),
               "Index must be non-negative");
  index = generator.GetHaltonSequenceIndex(1);
  EXPECT_TRUE(index.has_value());
  EXPECT_EQ(*index, 0);

  // Values requiring more than 15 bits are not allowed.
  EXPECT_DEATH(generator.SetHaltonSequenceIndex(0x4000, 1),
               "Index must not be larger than 0x3FFF");
  index = generator.GetHaltonSequenceIndex(1);
  EXPECT_TRUE(index.has_value());
  EXPECT_EQ(*index, 0);
#endif  // GTEST_HAS_DEATH_TEST
}

TEST(FrameInstrumentationGeneratorTest, QueuesAtMostThreeInputFrames) {
  const Environment env = CreateTestEnvironment();
  auto generator = std::make_unique<FrameInstrumentationGeneratorImpl>(
      &env, VideoCodecType::kVideoCodecVP8, ScalabilityMode::kL1T1);

  bool frames_destroyed[4] = {};
  class TestBuffer : public I420Buffer {
   public:
    TestBuffer(int width, int height, bool* frame_destroyed_indicator)
        : I420Buffer(width, height),
          frame_destroyed_indicator_(frame_destroyed_indicator) {}

   private:
    friend class RefCountedObject<TestBuffer>;
    ~TestBuffer() override { *frame_destroyed_indicator_ = true; }

    bool* frame_destroyed_indicator_;
  };

  // Insert four frames, the first one should expire and be released.
  for (int i = 0; i < 4; ++i) {
    generator->OnCapturedFrame(
        VideoFrame::Builder()
            .set_video_frame_buffer(make_ref_counted<TestBuffer>(
                kDefaultScaledWidth, kDefaultScaledHeight,
                &frames_destroyed[i]))
            .set_rtp_timestamp(1 + (33 * i))
            .build());
  }

  EXPECT_THAT(frames_destroyed, ElementsAre(true, true, false, false));

  generator.reset();
  EXPECT_THAT(frames_destroyed, ElementsAre(true, true, true, true));
}

TEST(FrameInstrumentationGeneratorTest,
     UsesFilterSettingsFromFrameWhenAvailable) {
  const Environment env = CreateTestEnvironment();
  FrameInstrumentationGeneratorImpl generator(
      &env, VideoCodecType::kVideoCodecVP8, ScalabilityMode::kL1T1);
  VideoFrame frame = VideoFrame::Builder()
                         .set_video_frame_buffer(MakeDefaultI420FrameBuffer())
                         .set_rtp_timestamp(1)
                         .build();
  // No QP needed when frame provides filter settings.
  EncodedImage encoded_image;
  encoded_image.SetRtpTimestamp(1);
  encoded_image.set_frame_type(VideoFrameType::kVideoFrameKey);
  encoded_image._encodedWidth = kDefaultScaledWidth;
  encoded_image._encodedHeight = kDefaultScaledHeight;
  encoded_image.set_corruption_detection_filter_settings(
      CorruptionDetectionFilterSettings{.std_dev = 1.0,
                                        .luma_error_threshold = 2,
                                        .chroma_error_threshold = 3});

  generator.OnCapturedFrame(frame);
  std::optional<FrameInstrumentationData> frame_instrumentation_data =
      generator.OnEncodedImage(encoded_image);

  ASSERT_TRUE(frame_instrumentation_data.has_value());
  EXPECT_EQ(frame_instrumentation_data->std_dev(), 1.0);
  EXPECT_EQ(frame_instrumentation_data->luma_error_threshold(), 2);
  EXPECT_EQ(frame_instrumentation_data->chroma_error_threshold(), 3);
}

TEST(FrameInstrumentationGeneratorTest, UsesFrameSelectorWhenEnabled) {
  // We wish to verify that the frame selector is used when enabled.
  // The default behavior of the frame selector is to sample key frames and
  // randomly sample delta frames (uniform distribution).
  // By setting the upper and lower bound of the distribution to 0, we can force
  // the frame selector to sample every frame.
  // Since the default behavior of the frame instrumentation generator (when
  // frame selector is not used) is to sample key frames and then wait for at
  // least 34 frames before sampling again, we can distinguish the two behaviors
  // by checking if the second frame (a delta frame) is sampled.
  const Environment env = CreateTestEnvironment(
      {.field_trials = "WebRTC-CorruptionDetectionFrameSelector/"
                       "enabled:true,low_overhead_lower_bound:0ms,low_overhead_"
                       "upper_bound:0ms,"
                       "high_overhead_lower_bound:0ms,high_overhead_upper_"
                       "bound:0ms/"});
  FrameInstrumentationGeneratorImpl generator(
      &env, VideoCodecType::kVideoCodecVP8, ScalabilityMode::kL1T1);

  VideoFrame frame1 = VideoFrame::Builder()
                          .set_video_frame_buffer(MakeDefaultI420FrameBuffer())
                          .set_rtp_timestamp(1)
                          .build();
  EncodedImage encoded_image1;
  encoded_image1.SetRtpTimestamp(1);
  encoded_image1.SetFrameType(VideoFrameType::kVideoFrameKey);
  encoded_image1.qp_ = 10;
  encoded_image1._encodedWidth = kDefaultScaledWidth;
  encoded_image1._encodedHeight = kDefaultScaledHeight;

  VideoFrame frame2 = VideoFrame::Builder()
                          .set_video_frame_buffer(MakeDefaultI420FrameBuffer())
                          .set_rtp_timestamp(2)
                          .build();
  EncodedImage encoded_image2;
  encoded_image2.SetRtpTimestamp(2);
  encoded_image2.SetFrameType(VideoFrameType::kVideoFrameDelta);
  encoded_image2.qp_ = 10;
  encoded_image2._encodedWidth = kDefaultScaledWidth;
  encoded_image2._encodedHeight = kDefaultScaledHeight;

  generator.OnCapturedFrame(frame1);
  EXPECT_TRUE(generator.OnEncodedImage(encoded_image1).has_value());

  generator.OnCapturedFrame(frame2);
  EXPECT_TRUE(generator.OnEncodedImage(encoded_image2).has_value());
}

}  // namespace
}  // namespace webrtc
