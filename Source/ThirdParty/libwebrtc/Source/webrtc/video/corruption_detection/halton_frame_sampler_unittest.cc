/*
 * Copyright 2024 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "video/corruption_detection/halton_frame_sampler.h"

#include <cstdint>
#include <vector>

#include "api/scoped_refptr.h"
#include "api/video/i420_buffer.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::DoubleEq;
using ::testing::DoubleNear;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Not;

using Coordinates = HaltonFrameSampler::Coordinates;

// Defaults for sampling tests.
const int kDefaultScaledWidth = 4;
const int kDefaultScaledHeight = 4;
const double kDefaultStdDevGaussianBlur = 0.02;

#if GTEST_HAS_DEATH_TEST
// Defaults for blurring tests.
const int kDefaultWidth = 4;
const int kDefaultHeight = 4;
const int kDefaultStride = 4;
const uint8_t kDefaultData[kDefaultWidth * kDefaultHeight] = {
    20, 196, 250, 115, 139, 39, 99, 197, 21, 166, 254, 28, 227, 54, 64, 46};
const int kDefaultRow = 3;
const int kDefaultColumn = 2;
const double kDefaultStdDev = 1.12;
#endif  // GTEST_HAS_DEATH_TEST

scoped_refptr<I420Buffer> MakeDefaultI420FrameBuffer() {
  // Create an I420 frame of size 4x4.
  const int kDefaultLumaWidth = 4;
  const int kDefaultLumaHeight = 4;
  const int kDefaultChromaWidth = 2;
  const uint8_t kDefaultYContent[16] = {20, 196, 250, 115, 139, 39, 99, 197,
                                        21, 166, 254, 28,  227, 54, 64, 46};
  const uint8_t kDefaultUContent[4] = {156, 203, 36, 128};
  const uint8_t kDefaultVContent[4] = {112, 2, 0, 24};

  return I420Buffer::Copy(kDefaultLumaWidth, kDefaultLumaHeight,
                          kDefaultYContent, kDefaultLumaWidth, kDefaultUContent,
                          kDefaultChromaWidth, kDefaultVContent,
                          kDefaultChromaWidth);
}

std::vector<Coordinates> MakeDefaultSampleCoordinates() {
  // Coordinates in all planes.
  return {{.row = 0.2, .column = 0.7},
          {.row = 0.5, .column = 0.9},
          {.row = 0.3, .column = 0.7},
          {.row = 0.8, .column = 0.4}};
}

TEST(GaussianFilteringTest, ShouldReturnFilteredValueWhenInputIsValid) {
  const int kWidth = 4;
  const int kHeight = 4;
  const int kStride = 4;
  const uint8_t kData[kWidth * kHeight] = {20, 196, 250, 115, 139, 39, 99, 197,
                                           21, 166, 254, 28,  227, 54, 64, 46};
  const int kRow = 3;
  const int kColumn = 2;
  const double kStdDev = 1.12;

  EXPECT_THAT(GetFilteredElement(kWidth, kHeight, kStride, kData, kRow, kColumn,
                                 kStdDev),
              DoubleEq(103.9558797428683));
}

TEST(GaussianFilteringTest,
     ShouldReturnOriginalValueWhenNoFilteringIsRequested) {
  const int kWidth = 4;
  const int kHeight = 4;
  const int kStride = 4;
  const uint8_t kData[kWidth * kHeight] = {20, 196, 250, 115, 139, 39, 99, 197,
                                           21, 166, 254, 28,  227, 54, 64, 46};
  const int kRow = 3;
  const int kColumn = 2;
  const double kStdDev = 0.0;

  EXPECT_THAT(GetFilteredElement(kWidth, kHeight, kStride, kData, kRow, kColumn,
                                 kStdDev),
              DoubleEq(64.0));
}

#if GTEST_HAS_DEATH_TEST
TEST(GaussianFilteringTest, ShouldCrashWhenRowIsNegative) {
  EXPECT_DEATH(
      GetFilteredElement(kDefaultWidth, kDefaultHeight, kDefaultStride,
                         kDefaultData, -1, kDefaultColumn, kDefaultStdDev),
      _);
}

TEST(GaussianFilteringTest, ShouldCrashWhenRowIsOutOfRange) {
  EXPECT_DEATH(
      GetFilteredElement(kDefaultWidth, 4, kDefaultStride, kDefaultData, 4,
                         kDefaultColumn, kDefaultStdDev),
      _);
}

TEST(GaussianFilteringTest, ShouldCrashWhenColumnIsNegative) {
  EXPECT_DEATH(
      GetFilteredElement(kDefaultWidth, kDefaultHeight, kDefaultStride,
                         kDefaultData, kDefaultRow, -1, kDefaultStdDev),
      _);
}

TEST(GaussianFilteringTest, ShouldCrashWhenColumnIsOutOfRange) {
  EXPECT_DEATH(GetFilteredElement(4, kDefaultHeight, kDefaultStride,
                                  kDefaultData, kDefaultRow, 4, kDefaultStdDev),
               _);
}

TEST(GaussianFilteringTest, ShouldCrashWhenStrideIsSmallerThanWidth) {
  EXPECT_DEATH(GetFilteredElement(4, kDefaultHeight, 3, kDefaultData,
                                  kDefaultRow, kDefaultColumn, kDefaultStdDev),
               _);
}

TEST(GaussianFilteringTest, ShouldCrashWhenStdDevIsNegative) {
  EXPECT_DEATH(
      GetFilteredElement(kDefaultWidth, kDefaultHeight, kDefaultStride,
                         kDefaultData, kDefaultRow, kDefaultColumn, -1.0),
      _);
}

TEST(HaltonFrameSamplerTest, FrameIsNotSampledWhenTimestampsAreEqual) {
  HaltonFrameSampler halton_frame_sampler;

  EXPECT_THAT(
      halton_frame_sampler.GetSampleCoordinatesForFrameIfFrameShouldBeSampled(
          /*is_key_frame=*/false, /*rtp_timestamp=*/0, /*num_samples=*/1),
      Not(IsEmpty()));
  EXPECT_DEATH(
      halton_frame_sampler.GetSampleCoordinatesForFrameIfFrameShouldBeSampled(
          /*is_key_frame=*/false, /*rtp_timestamp=*/0, /*num_samples=*/1),
      _);
}
#endif  // GTEST_HAS_DEATH_TEST

TEST(HaltonFrameSamplerGaussianFilteringTest,
     ShouldReturnEmptyListGivenInvalidInputNoFrameBuffer) {
  const std::vector<Coordinates> kDefaultSampleCoordinates =
      MakeDefaultSampleCoordinates();

  EXPECT_THAT(GetSampleValuesForFrame(nullptr, kDefaultSampleCoordinates,
                                      kDefaultScaledWidth, kDefaultScaledHeight,
                                      kDefaultStdDevGaussianBlur),
              IsEmpty());
}

TEST(HaltonFrameSamplerGaussianFilteringTest,
     ShouldReturnEmptyListGivenInvalidInputNoCoordinates) {
  const scoped_refptr<I420Buffer> kDefaultI420Buffer =
      MakeDefaultI420FrameBuffer();

  EXPECT_THAT(
      GetSampleValuesForFrame(kDefaultI420Buffer, {}, kDefaultScaledWidth,
                              kDefaultScaledHeight, kDefaultStdDevGaussianBlur),
      IsEmpty());
}

TEST(HaltonFrameSamplerGaussianFilteringTest,
     ShouldReturnEmptyListGivenInvalidInputOutOfRangeCoordinates) {
  const scoped_refptr<I420Buffer> kDefaultI420Buffer =
      MakeDefaultI420FrameBuffer();
  const std::vector<Coordinates> kSampleCoordinates = {
      {.row = 0.2, .column = 0.7},
      {.row = 0.5, .column = 1.0},
      {.row = 0.3, .column = 0.7},
      {.row = 0.8, .column = 0.4}};

  EXPECT_THAT(GetSampleValuesForFrame(kDefaultI420Buffer, kSampleCoordinates,
                                      kDefaultScaledWidth, kDefaultScaledHeight,
                                      kDefaultStdDevGaussianBlur),
              IsEmpty());
}

TEST(HaltonFrameSamplerGaussianFilteringTest,
     ShouldReturnEmptyListGivenInvalidInputWidthZero) {
  const scoped_refptr<I420Buffer> kDefaultI420Buffer =
      MakeDefaultI420FrameBuffer();
  const std::vector<Coordinates> kDefaultSampleCoordinates =
      MakeDefaultSampleCoordinates();

  EXPECT_THAT(
      GetSampleValuesForFrame(kDefaultI420Buffer, kDefaultSampleCoordinates, 0,
                              kDefaultScaledHeight, kDefaultStdDevGaussianBlur),
      IsEmpty());
}

TEST(HaltonFrameSamplerGaussianFilteringTest,
     ShouldReturnEmptyListGivenInvalidInputHeightZero) {
  const scoped_refptr<I420Buffer> kDefaultI420Buffer =
      MakeDefaultI420FrameBuffer();
  const std::vector<Coordinates> kDefaultSampleCoordinates =
      MakeDefaultSampleCoordinates();

  EXPECT_THAT(GetSampleValuesForFrame(
                  kDefaultI420Buffer, kDefaultSampleCoordinates,
                  kDefaultScaledWidth, 0, kDefaultStdDevGaussianBlur),
              IsEmpty());
}

TEST(HaltonFrameSamplerGaussianFilteringTest,
     ShouldReturnEmptyListGivenInvalidInputStdDevNegative) {
  const scoped_refptr<I420Buffer> kDefaultI420Buffer =
      MakeDefaultI420FrameBuffer();
  const std::vector<Coordinates> kDefaultSampleCoordinates =
      MakeDefaultSampleCoordinates();

  EXPECT_THAT(
      GetSampleValuesForFrame(kDefaultI420Buffer, kDefaultSampleCoordinates,
                              kDefaultScaledWidth, kDefaultScaledHeight, -1.0),
      IsEmpty());
}

TEST(HaltonFrameSamplerGaussianFilteringTest,
     ShouldReturnGivenValueWhenStdDevZero) {
  // 4x4 i420 frame data.
  const int kLumaWidth = 4;
  const int kLumaHeight = 4;
  const int kChromaWidth = 2;
  const uint8_t kYContent[16] = {20, 196, 250, 115, 139, 39, 99, 197,
                                 21, 166, 254, 28,  227, 54, 64, 46};
  const uint8_t kUContent[4] = {156, 203, 36, 128};
  const uint8_t kVContent[4] = {112, 2, 0, 24};
  const scoped_refptr<I420Buffer> kI420Buffer =
      I420Buffer::Copy(kLumaWidth, kLumaHeight, kYContent, kLumaWidth,
                       kUContent, kChromaWidth, kVContent, kChromaWidth);

  // Coordinates in all planes.
  const std::vector<Coordinates> kSampleCoordinates = {
      {.row = 0.2, .column = 0.7},
      {.row = 0.5, .column = 0.9},
      {.row = 0.3, .column = 0.7},
      {.row = 0.8, .column = 0.4}};

  // No scaling.
  const int kScaledWidth = kLumaWidth;
  const int kScaledHeight = kLumaHeight;

  EXPECT_THAT(
      GetSampleValuesForFrame(kI420Buffer, kSampleCoordinates, kScaledWidth,
                              kScaledHeight, 0.0),
      ElementsAre(AllOf(Field(&FilteredSample::value, DoubleEq(156.0)),
                        Field(&FilteredSample::plane, ImagePlane::kChroma)),
                  AllOf(Field(&FilteredSample::value, DoubleEq(2.0)),
                        Field(&FilteredSample::plane, ImagePlane::kChroma)),
                  AllOf(Field(&FilteredSample::value, DoubleEq(36.0)),
                        Field(&FilteredSample::plane, ImagePlane::kChroma)),
                  AllOf(Field(&FilteredSample::value, DoubleEq(64.0)),
                        Field(&FilteredSample::plane, ImagePlane::kLuma))));
}

TEST(HaltonFrameSamplerGaussianFilteringTest,
     ShouldReturnGivenValueWhenNoScalingOrFilteringIsDefined) {
  // 4x4 i420 frame data.
  const int kLumaWidth = 4;
  const int kLumaHeight = 4;
  const int kChromaWidth = 2;
  const uint8_t kYContent[16] = {20, 196, 250, 115, 139, 39, 99, 197,
                                 21, 166, 254, 28,  227, 54, 64, 46};
  const uint8_t kUContent[4] = {156, 203, 36, 128};
  const uint8_t kVContent[4] = {112, 2, 0, 24};
  const scoped_refptr<I420Buffer> kI420Buffer =
      I420Buffer::Copy(kLumaWidth, kLumaHeight, kYContent, kLumaWidth,
                       kUContent, kChromaWidth, kVContent, kChromaWidth);

  // Coordinates in all planes.
  const std::vector<Coordinates> kSampleCoordinates = {
      {.row = 0.2, .column = 0.7},
      {.row = 0.5, .column = 0.9},
      {.row = 0.3, .column = 0.7},
      {.row = 0.8, .column = 0.4}};

  // No scaling.
  const int kScaledWidth = kLumaWidth;
  const int kScaledHeight = kLumaHeight;

  // No filtering.
  const double kStdDevGaussianBlur = 0.02;

  EXPECT_THAT(
      GetSampleValuesForFrame(kI420Buffer, kSampleCoordinates, kScaledWidth,
                              kScaledHeight, kStdDevGaussianBlur),
      ElementsAre(AllOf(Field(&FilteredSample::value, DoubleEq(156.0)),
                        Field(&FilteredSample::plane, ImagePlane::kChroma)),
                  AllOf(Field(&FilteredSample::value, DoubleEq(2.0)),
                        Field(&FilteredSample::plane, ImagePlane::kChroma)),
                  AllOf(Field(&FilteredSample::value, DoubleEq(36.0)),
                        Field(&FilteredSample::plane, ImagePlane::kChroma)),
                  AllOf(Field(&FilteredSample::value, DoubleEq(64.0)),
                        Field(&FilteredSample::plane, ImagePlane::kLuma))));
}

TEST(HaltonFrameSamplerGaussianFilteringTest,
     ShouldScaleTheFrameWhenScalingIsRequested) {
  // 4x4 i420 frame data.
  const int kLumaWidth = 4;
  const int kLumaHeight = 4;
  const int kChromaWidth = 2;
  const uint8_t kYContent[16] = {20, 196, 250, 115, 139, 39, 99, 197,
                                 21, 166, 254, 28,  227, 54, 64, 46};
  const uint8_t kUContent[4] = {156, 203, 36, 128};
  const uint8_t kVContent[4] = {112, 2, 0, 24};
  const scoped_refptr<I420Buffer> kI420Buffer =
      I420Buffer::Copy(kLumaWidth, kLumaHeight, kYContent, kLumaWidth,
                       kUContent, kChromaWidth, kVContent, kChromaWidth);

  // Coordinates in all planes.
  const std::vector<Coordinates> kSampleCoordinates = {
      {.row = 0.2, .column = 0.7},
      {.row = 0.5, .column = 0.9},
      {.row = 0.3, .column = 0.7},
      {.row = 0.8, .column = 0.4}};

  // With scaling.
  const int kScaledWidth = 10;
  const int kScaledHeight = 10;

  // No filtering.
  const double kStdDevGaussianBlur = 0.02;

  EXPECT_THAT(
      GetSampleValuesForFrame(kI420Buffer, kSampleCoordinates, kScaledWidth,
                              kScaledHeight, kStdDevGaussianBlur),
      ElementsAre(AllOf(Field(&FilteredSample::value, DoubleEq(96.0)),
                        Field(&FilteredSample::plane, ImagePlane::kChroma)),
                  AllOf(Field(&FilteredSample::value, DoubleEq(30.0)),
                        Field(&FilteredSample::plane, ImagePlane::kChroma)),
                  AllOf(Field(&FilteredSample::value, DoubleEq(66.0)),
                        Field(&FilteredSample::plane, ImagePlane::kChroma)),
                  AllOf(Field(&FilteredSample::value, DoubleNear(127.0, 1.0)),
                        Field(&FilteredSample::plane, ImagePlane::kLuma))));
}

TEST(HaltonFrameSamplerGaussianFilteringTest,
     ShouldReturnFilteredValuesWhenFilteringIsRequested) {
  // 4x4 i420 frame data.
  const int kLumaWidth = 4;
  const int kLumaHeight = 4;
  const int kChromaWidth = 2;
  const uint8_t kYContent[16] = {20, 196, 250, 115, 139, 39, 99, 197,
                                 21, 166, 254, 28,  227, 54, 64, 46};
  const uint8_t kUContent[4] = {156, 203, 36, 128};
  const uint8_t kVContent[4] = {112, 2, 0, 24};
  const scoped_refptr<I420Buffer> kI420Buffer =
      I420Buffer::Copy(kLumaWidth, kLumaHeight, kYContent, kLumaWidth,
                       kUContent, kChromaWidth, kVContent, kChromaWidth);

  // Coordinates in all (YUV) planes.
  const std::vector<Coordinates> kSampleCoordinates = {
      {.row = 0.2, .column = 0.7},
      {.row = 0.5, .column = 0.9},
      {.row = 0.3, .column = 0.7},
      {.row = 0.8, .column = 0.4}};

  // No scaling.
  const int kScaledWidth = kLumaWidth;
  const int kScaledHeight = kLumaHeight;

  // With filtering (kernel size 2x2).
  const double kStdDevGaussianBlur = 1.12;

  EXPECT_THAT(
      GetSampleValuesForFrame(kI420Buffer, kSampleCoordinates, kScaledWidth,
                              kScaledHeight, kStdDevGaussianBlur),
      ElementsAre(
          AllOf(Field(&FilteredSample::value, DoubleEq(133.93909141543787)),
                Field(&FilteredSample::plane, ImagePlane::kChroma)),
          AllOf(Field(&FilteredSample::value, DoubleEq(33.40054269066487)),
                Field(&FilteredSample::plane, ImagePlane::kChroma)),
          AllOf(Field(&FilteredSample::value, DoubleEq(113.8901872847041)),
                Field(&FilteredSample::plane, ImagePlane::kChroma)),
          AllOf(Field(&FilteredSample::value, DoubleEq(103.9558797428683)),
                Field(&FilteredSample::plane, ImagePlane::kLuma))));
}

TEST(HaltonFrameSamplerTest, CoordinatesFollowsHaltonSequence) {
  HaltonFrameSampler halton_frame_sampler;
  const int kNumSamples = 1;
  EXPECT_THAT(halton_frame_sampler.GetSampleCoordinatesForFrame(kNumSamples),
              ElementsAre(AllOf(Field(&Coordinates::row, DoubleEq(0.0)),
                                Field(&Coordinates::column, DoubleEq(0.0)))));
  EXPECT_THAT(
      halton_frame_sampler.GetSampleCoordinatesForFrame(kNumSamples),
      ElementsAre(AllOf(Field(&Coordinates::row, DoubleEq(1.0 / 2)),
                        Field(&Coordinates::column, DoubleEq(1.0 / 3)))));
  EXPECT_THAT(
      halton_frame_sampler.GetSampleCoordinatesForFrame(kNumSamples),
      ElementsAre(AllOf(Field(&Coordinates::row, DoubleEq(1.0 / 4)),
                        Field(&Coordinates::column, DoubleEq(2.0 / 3)))));
  EXPECT_THAT(
      halton_frame_sampler.GetSampleCoordinatesForFrame(kNumSamples),
      ElementsAre(AllOf(Field(&Coordinates::row, DoubleEq(3.0 / 4)),
                        Field(&Coordinates::column, DoubleEq(1.0 / 9)))));
  EXPECT_THAT(
      halton_frame_sampler.GetSampleCoordinatesForFrame(kNumSamples),
      ElementsAre(AllOf(Field(&Coordinates::row, DoubleEq(1.0 / 8)),
                        Field(&Coordinates::column, DoubleEq(4.0 / 9)))));
  EXPECT_THAT(
      halton_frame_sampler.GetSampleCoordinatesForFrame(kNumSamples),
      ElementsAre(AllOf(Field(&Coordinates::row, DoubleEq(5.0 / 8)),
                        Field(&Coordinates::column, DoubleEq(7.0 / 9)))));
  EXPECT_THAT(
      halton_frame_sampler.GetSampleCoordinatesForFrame(kNumSamples),
      ElementsAre(AllOf(Field(&Coordinates::row, DoubleEq(3.0 / 8)),
                        Field(&Coordinates::column, DoubleEq(2.0 / 9)))));
}

TEST(HaltonFrameSamplerTest, GeneratesMultipleSamplesWhenRequested) {
  HaltonFrameSampler halton_frame_sampler;
  EXPECT_THAT(
      halton_frame_sampler.GetSampleCoordinatesForFrame(3),
      ElementsAre(AllOf(Field(&Coordinates::row, DoubleEq(0.0)),
                        Field(&Coordinates::column, DoubleEq(0.0))),
                  AllOf(Field(&Coordinates::row, DoubleEq(1.0 / 2)),
                        Field(&Coordinates::column, DoubleEq(1.0 / 3))),
                  AllOf(Field(&Coordinates::row, DoubleEq(1.0 / 4)),
                        Field(&Coordinates::column, DoubleEq(2.0 / 3)))));
}

TEST(HaltonFrameSamplerTest, ShouldChangeIndexWhenRequestedTo) {
  HaltonFrameSampler halton_frame_sampler;
  halton_frame_sampler.SetCurrentIndex(1);
  EXPECT_EQ(halton_frame_sampler.GetCurrentIndex(), 1);
  EXPECT_THAT(
      halton_frame_sampler.GetSampleCoordinatesForFrame(1),
      ElementsAre(AllOf(Field(&Coordinates::row, DoubleEq(1.0 / 2)),
                        Field(&Coordinates::column, DoubleEq(1.0 / 3)))));
}

TEST(HaltonFrameSamplerTest, FirstFrameIsSampled) {
  HaltonFrameSampler halton_frame_sampler;
  EXPECT_THAT(
      halton_frame_sampler.GetSampleCoordinatesForFrameIfFrameShouldBeSampled(
          /*is_key_frame=*/false, /*rtp_timestamp=*/0, /*num_samples=*/1),
      Not(IsEmpty()));
}

TEST(HaltonFrameSamplerTest,
     DeltaFrameFollowingSampledFrameWithTooShortTimeDeltaIsNotSampled) {
  HaltonFrameSampler halton_frame_sampler;
  halton_frame_sampler.GetSampleCoordinatesForFrameIfFrameShouldBeSampled(
      /*is_key_frame=*/false, /*rtp_timestamp=*/0, /*num_samples=*/1);
  EXPECT_THAT(
      halton_frame_sampler.GetSampleCoordinatesForFrameIfFrameShouldBeSampled(
          /*is_key_frame=*/false, /*rtp_timestamp=*/1, /*num_samples=*/1),
      IsEmpty());
}

TEST(HaltonFrameSamplerTest,
     DeltaFramesAreSampledBasedOnHowManyFramesHasPassedSinceLastSampledFrame) {
  HaltonFrameSampler halton_frame_sampler;
  uint32_t rtp_timestamp = 0;
  const int kNumSamples = 1;

  // The number of frames between each sample is defined as
  //   33 - mod(number_of_sampled_frames, 8)
  // so the following gets get coverage for [26, 33] two times.
  for (int iterations = 0; iterations < 2; ++iterations) {
    for (int num_sampled_frames = 0; num_sampled_frames < 8;
         ++num_sampled_frames) {
      EXPECT_THAT(halton_frame_sampler
                      .GetSampleCoordinatesForFrameIfFrameShouldBeSampled(
                          /*is_key_frame=*/false, rtp_timestamp, kNumSamples),
                  Not(IsEmpty()));
      ++rtp_timestamp;
      for (int num_unsampled_frames = 1;
           num_unsampled_frames < 33 - num_sampled_frames;
           ++num_unsampled_frames) {
        EXPECT_THAT(halton_frame_sampler
                        .GetSampleCoordinatesForFrameIfFrameShouldBeSampled(
                            /*is_key_frame=*/false, rtp_timestamp, kNumSamples),
                    IsEmpty());
        ++rtp_timestamp;
      }
    }
  }
}

TEST(HaltonFrameSamplerTest, KeyFrameIsSampled) {
  HaltonFrameSampler halton_frame_sampler;
  EXPECT_THAT(
      halton_frame_sampler.GetSampleCoordinatesForFrameIfFrameShouldBeSampled(
          /*is_key_frame=*/false, /*rtp_timestamp=*/0, /*num_samples=*/1),
      Not(IsEmpty()));
  EXPECT_THAT(
      halton_frame_sampler.GetSampleCoordinatesForFrameIfFrameShouldBeSampled(
          /*is_key_frame=*/true, /*rtp_timestamp=*/1, /*num_samples=*/1),
      Not(IsEmpty()));
}

TEST(HaltonFrameSamplerTest,
     SampleFramesWhenEnoughTimeHasPassedSinceLastSampledFrame) {
  HaltonFrameSampler halton_frame_sampler;
  const uint32_t kRtpTimestamp = 0;
  const int kNumSamples = 1;
  const uint32_t kSufficientDuration = 90'000;
  const uint32_t kTooShortDuration = 1;
  halton_frame_sampler.GetSampleCoordinatesForFrameIfFrameShouldBeSampled(
      /*is_key_frame=*/false, kRtpTimestamp, kNumSamples);
  EXPECT_THAT(
      halton_frame_sampler.GetSampleCoordinatesForFrameIfFrameShouldBeSampled(
          /*is_key_frame=*/false, kRtpTimestamp + kSufficientDuration,
          kNumSamples),
      Not(IsEmpty()));
  EXPECT_THAT(
      halton_frame_sampler.GetSampleCoordinatesForFrameIfFrameShouldBeSampled(
          /*is_key_frame=*/false,
          kRtpTimestamp + kSufficientDuration + kTooShortDuration, kNumSamples),
      IsEmpty());
  EXPECT_THAT(
      halton_frame_sampler.GetSampleCoordinatesForFrameIfFrameShouldBeSampled(
          /*is_key_frame=*/false, kRtpTimestamp + 2 * kSufficientDuration,
          kNumSamples),
      Not(IsEmpty()));
}

TEST(HaltonFrameSamplerTest,
     FrameIsNotSampledWhenTooShortTimeHasPassedSinceLastSampledFrame) {
  HaltonFrameSampler halton_frame_sampler;
  const uint32_t kRtpTimestamp = 0;
  const uint32_t kTooShortDuration = 90'000 - 1;
  halton_frame_sampler.GetSampleCoordinatesForFrameIfFrameShouldBeSampled(
      /*is_key_frame=*/false, kRtpTimestamp, /*num_samples=*/1);
  EXPECT_THAT(
      halton_frame_sampler.GetSampleCoordinatesForFrameIfFrameShouldBeSampled(
          /*is_key_frame=*/false, kRtpTimestamp + kTooShortDuration,
          /*num_samples=*/1),
      IsEmpty());
}

TEST(HaltonFrameSamplerTest,
     SampleFramesWhenEnoughTimeWithWraparoundHasPassedSinceLastSampledFrame) {
  HaltonFrameSampler halton_frame_sampler;

  // Time delta = 90'000.
  EXPECT_THAT(
      halton_frame_sampler.GetSampleCoordinatesForFrameIfFrameShouldBeSampled(
          /*is_key_frame=*/false, /*rtp_timestamp=*/0xFFFE'A071,
          /*num_samples=*/1),
      Not(IsEmpty()));
  EXPECT_THAT(
      halton_frame_sampler.GetSampleCoordinatesForFrameIfFrameShouldBeSampled(
          /*is_key_frame=*/false, /*rtp_timestamp=*/1, /*num_samples=*/1),
      Not(IsEmpty()));
}

TEST(
    HaltonFrameSamplerTest,
    FrameIsNotSampledWhenTooShortTimeDeltaWithWraparoundSinceLastSampledFrame) {
  HaltonFrameSampler halton_frame_sampler;

  // Time delta = 89'999.
  EXPECT_THAT(
      halton_frame_sampler.GetSampleCoordinatesForFrameIfFrameShouldBeSampled(
          /*is_key_frame=*/false, /*rtp_timestamp=*/0xFFFE'A072,
          /*num_samples=*/1),
      Not(IsEmpty()));
  EXPECT_THAT(
      halton_frame_sampler.GetSampleCoordinatesForFrameIfFrameShouldBeSampled(
          /*is_key_frame=*/false, /*rtp_timestamp=*/1, /*num_samples=*/1),
      IsEmpty());
}

}  // namespace
}  // namespace webrtc
