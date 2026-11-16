/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/corruption_detection/corruption_detection_settings_generator.h"

#include "api/video/corruption_detection/corruption_detection_filter_settings.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::AllOf;
using ::testing::DoubleEq;
using ::testing::DoubleNear;
using ::testing::Eq;
using ::testing::Field;

namespace webrtc {

TEST(CorruptionDetectionSettingsGenerator, ExponentialFunctionStdDev) {
  CorruptionDetectionSettingsGenerator settings_generator(
      CorruptionDetectionSettingsGenerator::ExponentialFunctionParameters{
          .scale = 0.006,
          .exponent_factor = 0.01857465,
          .exponent_offset = -4.26470513},
      CorruptionDetectionSettingsGenerator::ErrorThresholds{},
      CorruptionDetectionSettingsGenerator::TransientParameters{});

  // 0.006 * e^(0.01857465 * 20 + 4.26470513) ~= 0.612
  CorruptionDetectionFilterSettings settings =
      settings_generator.OnFrame(/*is_keyframe=*/true, /*qp=*/20);
  EXPECT_THAT(settings.std_dev, DoubleNear(0.612, 0.01));

  // 0.006 * e^(0.01857465 * 20 + 4.26470513) ~= 1.886
  settings = settings_generator.OnFrame(/*is_keyframe=*/true, /*qp=*/80);
  EXPECT_THAT(settings.std_dev, DoubleNear(1.886, 0.01));
}

TEST(CorruptionDetectionSettingsGenerator, ExponentialFunctionThresholds) {
  CorruptionDetectionSettingsGenerator settings_generator(
      CorruptionDetectionSettingsGenerator::ExponentialFunctionParameters{
          .scale = 0.006,
          .exponent_factor = 0.01857465,
          .exponent_offset = -4.26470513},
      CorruptionDetectionSettingsGenerator::ErrorThresholds{.luma = 5,
                                                            .chroma = 6},
      CorruptionDetectionSettingsGenerator::TransientParameters{});

  CorruptionDetectionFilterSettings settings =
      settings_generator.OnFrame(/*is_keyframe=*/true, /*qp=*/20);
  EXPECT_EQ(settings.chroma_error_threshold, 6);
  EXPECT_EQ(settings.luma_error_threshold, 5);
}

TEST(CorruptionDetectionSettingsGenerator, RationalFunctionStdDev) {
  CorruptionDetectionSettingsGenerator settings_generator(
      CorruptionDetectionSettingsGenerator::RationalFunctionParameters{
          .numerator_factor = -5.5, .denumerator_term = -97, .offset = -1},
      CorruptionDetectionSettingsGenerator::ErrorThresholds{},
      CorruptionDetectionSettingsGenerator::TransientParameters{});

  // (20 * -5.5) / (20 - 97) - 1 ~= 0.429
  CorruptionDetectionFilterSettings settings =
      settings_generator.OnFrame(/*is_keyframe=*/true, /*qp=*/20);
  EXPECT_THAT(settings.std_dev, DoubleNear(0.429, 0.01));

  // (40 * -5.5) / (40 - 97) - 1 ~= 2.860
  settings = settings_generator.OnFrame(/*is_keyframe=*/true, /*qp=*/40);
  EXPECT_THAT(settings.std_dev, DoubleNear(2.860, 0.01));
}

TEST(CorruptionDetectionSettingsGenerator, RationalFunctionThresholds) {
  CorruptionDetectionSettingsGenerator settings_generator(
      CorruptionDetectionSettingsGenerator::RationalFunctionParameters{
          .numerator_factor = -5.5, .denumerator_term = -97, .offset = -1},
      CorruptionDetectionSettingsGenerator::ErrorThresholds{.luma = 5,
                                                            .chroma = 6},
      CorruptionDetectionSettingsGenerator::TransientParameters{});

  CorruptionDetectionFilterSettings settings =
      settings_generator.OnFrame(/*is_keyframe=*/true, /*qp=*/20);
  EXPECT_EQ(settings.chroma_error_threshold, 6);
  EXPECT_EQ(settings.luma_error_threshold, 5);
}

TEST(CorruptionDetectionSettingsGenerator, TransientStdDevOffset) {
  CorruptionDetectionSettingsGenerator settings_generator(
      // (1 * qp) / (qp - 0) + 1 = 2, for all values of qp.
      CorruptionDetectionSettingsGenerator::RationalFunctionParameters{
          .numerator_factor = 1, .denumerator_term = 0, .offset = 1},
      CorruptionDetectionSettingsGenerator::ErrorThresholds{},
      // Two frames with adjusted settings, including the keyframe.
      // Adjust the keyframe std_dev by 2.
      CorruptionDetectionSettingsGenerator::TransientParameters{
          .keyframe_stddev_offset = 2.0,
          .keyframe_offset_duration_frames = 2,
      });

  EXPECT_THAT(settings_generator.OnFrame(/*is_keyframe=*/true, /*qp=*/1),
              Field(&CorruptionDetectionFilterSettings::std_dev,
                    DoubleNear(4.0, 0.001)));

  // Second frame has std_dev ofset interpolated halfway between keyframe
  // (2.0 + 2.0) and default (2.0) => 3.0
  EXPECT_THAT(settings_generator.OnFrame(/*is_keyframe=*/false, /*qp=*/1),
              Field(&CorruptionDetectionFilterSettings::std_dev,
                    DoubleNear(3.0, 0.001)));

  EXPECT_THAT(settings_generator.OnFrame(/*is_keyframe=*/false, /*qp=*/1),
              Field(&CorruptionDetectionFilterSettings::std_dev,
                    DoubleNear(2.0, 0.001)));

  EXPECT_THAT(settings_generator.OnFrame(/*is_keyframe=*/false, /*qp=*/1),
              Field(&CorruptionDetectionFilterSettings::std_dev,
                    DoubleNear(2.0, 0.001)));
}

TEST(CorruptionDetectionSettingsGenerator, TransientThresholdOffsets) {
  CorruptionDetectionSettingsGenerator settings_generator(
      CorruptionDetectionSettingsGenerator::RationalFunctionParameters{
          .numerator_factor = 1, .denumerator_term = 0, .offset = 1},
      CorruptionDetectionSettingsGenerator::ErrorThresholds{.luma = 2,
                                                            .chroma = 3},
      // Two frames with adjusted settings, including the keyframe.
      // Adjust the error thresholds by 2.
      CorruptionDetectionSettingsGenerator::TransientParameters{
          .keyframe_threshold_offset = 2,
          .keyframe_offset_duration_frames = 2,
      });

  EXPECT_THAT(
      settings_generator.OnFrame(/*is_keyframe=*/true, /*qp=*/1),
      AllOf(Field(&CorruptionDetectionFilterSettings::chroma_error_threshold,
                  Eq(5)),
            Field(&CorruptionDetectionFilterSettings::luma_error_threshold,
                  Eq(4))));

  // Second frame has offset interpolated halfway between keyframe and default.
  EXPECT_THAT(
      settings_generator.OnFrame(/*is_keyframe=*/false, /*qp=*/1),
      AllOf(Field(&CorruptionDetectionFilterSettings::chroma_error_threshold,
                  Eq(4)),
            Field(&CorruptionDetectionFilterSettings::luma_error_threshold,
                  Eq(3))));

  EXPECT_THAT(
      settings_generator.OnFrame(/*is_keyframe=*/false, /*qp=*/1),
      AllOf(Field(&CorruptionDetectionFilterSettings::chroma_error_threshold,
                  Eq(3)),
            Field(&CorruptionDetectionFilterSettings::luma_error_threshold,
                  Eq(2))));

  EXPECT_THAT(
      settings_generator.OnFrame(/*is_keyframe=*/false, /*qp=*/1),
      AllOf(Field(&CorruptionDetectionFilterSettings::chroma_error_threshold,
                  Eq(3)),
            Field(&CorruptionDetectionFilterSettings::luma_error_threshold,
                  Eq(2))));
}

TEST(CorruptionDetectionSettingsGenerator, StdDevUpperBound) {
  CorruptionDetectionSettingsGenerator settings_generator(
      // (1 * qp) / (qp - 0) + 41 = 42, for all values of qp.
      CorruptionDetectionSettingsGenerator::RationalFunctionParameters{
          .numerator_factor = 1, .denumerator_term = 0, .offset = 41},
      CorruptionDetectionSettingsGenerator::ErrorThresholds{},
      CorruptionDetectionSettingsGenerator::TransientParameters{});

  // `std_dev` capped at max 40.0, which is the limit for the protocol.
  EXPECT_THAT(
      settings_generator.OnFrame(/*is_keyframe=*/true, /*qp=*/1),
      Field(&CorruptionDetectionFilterSettings::std_dev, DoubleEq(40.0)));
}

TEST(CorruptionDetectionSettingsGenerator, StdDevLowerBound) {
  CorruptionDetectionSettingsGenerator settings_generator(
      // (1 * qp) / (qp - 0) + 1 = 2, for all values of qp.
      CorruptionDetectionSettingsGenerator::RationalFunctionParameters{
          .numerator_factor = 1, .denumerator_term = 0, .offset = 1},
      CorruptionDetectionSettingsGenerator::ErrorThresholds{},
      CorruptionDetectionSettingsGenerator::TransientParameters{
          .std_dev_lower_bound = 5.0});

  // `std_dev` capped at lower bound of 5.0.
  EXPECT_THAT(
      settings_generator.OnFrame(/*is_keyframe=*/true, /*qp=*/1),
      Field(&CorruptionDetectionFilterSettings::std_dev, DoubleEq(5.0)));
}

TEST(CorruptionDetectionSettingsGenerator, TreatsLargeQpChangeAsKeyFrame) {
  CorruptionDetectionSettingsGenerator settings_generator(
      CorruptionDetectionSettingsGenerator::RationalFunctionParameters{
          .numerator_factor = 1, .denumerator_term = 0, .offset = 1},
      CorruptionDetectionSettingsGenerator::ErrorThresholds{.luma = 2,
                                                            .chroma = 3},
      // Two frames with adjusted settings, including the keyframe.
      // Adjust the error thresholds by 2.
      CorruptionDetectionSettingsGenerator::TransientParameters{
          .max_qp = 100,
          .keyframe_threshold_offset = 2,
          .keyframe_offset_duration_frames = 1,
          .large_qp_change_threshold = 20});

  // +2 offset due to keyframe.
  EXPECT_THAT(
      settings_generator.OnFrame(/*is_keyframe=*/true, /*qp=*/10),
      Field(&CorruptionDetectionFilterSettings::luma_error_threshold, Eq(4)));

  // Back to normal.
  EXPECT_THAT(
      settings_generator.OnFrame(/*is_keyframe=*/false, /*qp=*/10),
      Field(&CorruptionDetectionFilterSettings::luma_error_threshold, Eq(2)));

  // Large change in qp, treat as keyframe => add +2 offset.
  EXPECT_THAT(
      settings_generator.OnFrame(/*is_keyframe=*/false, /*qp=*/30),
      Field(&CorruptionDetectionFilterSettings::luma_error_threshold, Eq(4)));

  // Back to normal.
  EXPECT_THAT(
      settings_generator.OnFrame(/*is_keyframe=*/false, /*qp=*/30),
      Field(&CorruptionDetectionFilterSettings::luma_error_threshold, Eq(2)));
}

}  // namespace webrtc
