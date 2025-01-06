/*
 * Copyright 2024 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/corruption_detection_message.h"

#include <optional>
#include <vector>

#include "test/gtest.h"

namespace webrtc {
namespace {

TEST(CorruptionDetectionMessageTest, FailsToCreateWhenSequenceIndexIsTooLarge) {
  EXPECT_EQ(CorruptionDetectionMessage::Builder()
                .WithSequenceIndex(0b1000'0000)
                .Build(),
            std::nullopt);
}

TEST(CorruptionDetectionMessageTest, FailsToCreateWhenSequenceIndexIsTooSmall) {
  EXPECT_EQ(CorruptionDetectionMessage::Builder().WithSequenceIndex(-1).Build(),
            std::nullopt);
}

TEST(CorruptionDetectionMessageTest, FailsToCreateWhenStddevIsTooLarge) {
  EXPECT_EQ(CorruptionDetectionMessage::Builder().WithStdDev(45.0).Build(),
            std::nullopt);
}

TEST(CorruptionDetectionMessageTest, FailsToCreateWhenStddevIsTooSmall) {
  EXPECT_EQ(CorruptionDetectionMessage::Builder().WithStdDev(-1.0).Build(),
            std::nullopt);
}

TEST(CorruptionDetectionMessageTest,
     FailsToCreateWhenLumaErrorThresholdIsTooLarge) {
  EXPECT_EQ(
      CorruptionDetectionMessage::Builder().WithLumaErrorThreshold(16).Build(),
      std::nullopt);
}

TEST(CorruptionDetectionMessageTest,
     FailsToCreateWhenLumaErrorThresholdIsTooSmall) {
  EXPECT_EQ(
      CorruptionDetectionMessage::Builder().WithLumaErrorThreshold(-1).Build(),
      std::nullopt);
}

TEST(CorruptionDetectionMessageTest,
     FailsToCreateWhenChromaErrorThresholdIsTooLarge) {
  EXPECT_EQ(CorruptionDetectionMessage::Builder()
                .WithChromaErrorThreshold(16)
                .Build(),
            std::nullopt);
}

TEST(CorruptionDetectionMessageTest,
     FailsToCreateWhenChromaErrorThresholdIsTooSmall) {
  EXPECT_EQ(CorruptionDetectionMessage::Builder()
                .WithChromaErrorThreshold(-1)
                .Build(),
            std::nullopt);
}

TEST(CorruptionDetectionMessageTest,
     FailsToCreateWhenTooManySamplesAreSpecified) {
  const std::vector<double> kSampleValues = {1.0,  2.0,  3.0,  4.0, 5.0,
                                             6.0,  7.0,  8.0,  9.0, 10.0,
                                             11.0, 12.0, 13.0, 14.0};

  EXPECT_EQ(CorruptionDetectionMessage::Builder()
                .WithSampleValues(kSampleValues)
                .Build(),
            std::nullopt);
}

TEST(CorruptionDetectionMessageTest, FailsToCreateWhenSampleValueIsTooLarge) {
  const std::vector<double> kSampleValues = {255.1};

  EXPECT_EQ(CorruptionDetectionMessage::Builder()
                .WithSampleValues(kSampleValues)
                .Build(),
            std::nullopt);
}

TEST(CorruptionDetectionMessageTest, FailsToCreateWhenSampleValueIsTooSmall) {
  const std::vector<double> kSampleValues = {-0.1};

  EXPECT_EQ(CorruptionDetectionMessage::Builder()
                .WithSampleValues(kSampleValues)
                .Build(),
            std::nullopt);
}

TEST(CorruptionDetectionMessageTest,
     CreatesDefaultWhenNoParametersAreSpecified) {
  EXPECT_NE(CorruptionDetectionMessage::Builder().Build(), std::nullopt);
}

TEST(CorruptionDetectionMessageTest, CreatesWhenValidParametersAreSpecified) {
  const std::vector<double> kSampleValues = {1.0, 2.0, 3.0, 4.0,  5.0,  6.0,
                                             7.0, 8.0, 9.0, 10.0, 11.0, 12.0};

  EXPECT_NE(CorruptionDetectionMessage::Builder()
                .WithSequenceIndex(0b0111'1111)
                .WithInterpretSequenceIndexAsMostSignificantBits(true)
                .WithStdDev(40.0)
                .WithLumaErrorThreshold(15)
                .WithChromaErrorThreshold(15)
                .WithSampleValues(kSampleValues)
                .Build(),
            std::nullopt);
}

}  // namespace
}  // namespace webrtc
