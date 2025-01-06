/*
 * Copyright 2024 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/corruption_detection_extension.h"

#include <cstddef>
#include <cstdint>
#include <optional>

#include "common_video/corruption_detection_message.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::DoubleEq;
using ::testing::ElementsAre;

TEST(CorruptionDetectionExtensionTest, ValueSizeIs1UnlessSamplesAreSpecified) {
  const std::optional<CorruptionDetectionMessage> kMessage =
      CorruptionDetectionMessage::Builder()
          .WithSequenceIndex(0b0110'1111)
          .WithInterpretSequenceIndexAsMostSignificantBits(true)
          .WithStdDev(8.0)
          .WithSampleValues({})
          .Build();

  ASSERT_NE(kMessage, std::nullopt);
  EXPECT_EQ(CorruptionDetectionExtension::ValueSize(*kMessage), size_t{1});
}

TEST(CorruptionDetectionExtensionTest,
     GivenSamplesTheValueSizeIsTheSumOfTheNumberOfSamplesPlus3) {
  const double kSampleValues[] = {1.0, 2.0, 3.0, 4.0};
  const std::optional<CorruptionDetectionMessage> kMessage =
      CorruptionDetectionMessage::Builder()
          .WithSampleValues(kSampleValues)
          .Build();

  ASSERT_NE(kMessage, std::nullopt);
  EXPECT_EQ(CorruptionDetectionExtension::ValueSize(*kMessage), size_t{7});
}

TEST(CorruptionDetectionExtensionTest,
     WritesMandatoryWhenEnoughMemoryIsAllocatedWithoutSamples) {
  const std::optional<CorruptionDetectionMessage> kMessage =
      CorruptionDetectionMessage::Builder()
          .WithSequenceIndex(0b0110'1111)
          .WithInterpretSequenceIndexAsMostSignificantBits(true)
          .Build();
  uint8_t data[] = {0};

  ASSERT_NE(kMessage, std::nullopt);
  EXPECT_TRUE(CorruptionDetectionExtension::Write(data, *kMessage));
  EXPECT_THAT(data, ElementsAre(0b1110'1111));
}

TEST(CorruptionDetectionExtensionTest,
     FailsToWriteWhenTooMuchMemoryIsAllocatedWithoutSamples) {
  const std::optional<CorruptionDetectionMessage> kMessage =
      CorruptionDetectionMessage::Builder()
          .WithSequenceIndex(0b0110'1111)
          .WithInterpretSequenceIndexAsMostSignificantBits(true)
          .Build();
  uint8_t data[] = {0, 0, 0};

  ASSERT_NE(kMessage, std::nullopt);
  EXPECT_FALSE(CorruptionDetectionExtension::Write(data, *kMessage));
}

TEST(CorruptionDetectionExtensionTest,
     FailsToWriteWhenTooMuchMemoryIsAllocatedWithSamples) {
  const double kSampleValues[] = {1.0};
  const std::optional<CorruptionDetectionMessage> kMessage =
      CorruptionDetectionMessage::Builder()
          .WithSequenceIndex(0b0110'1111)
          .WithInterpretSequenceIndexAsMostSignificantBits(true)
          .WithStdDev(8.0)
          .WithSampleValues(kSampleValues)
          .Build();
  uint8_t data[] = {0, 0, 0, 0, 0};

  ASSERT_NE(kMessage, std::nullopt);
  EXPECT_FALSE(CorruptionDetectionExtension::Write(data, *kMessage));
}

TEST(CorruptionDetectionExtensionTest,
     WritesEverythingWhenEnoughMemoryIsAllocatedWithSamples) {
  const double kSampleValues[] = {1.0};
  const std::optional<CorruptionDetectionMessage> kMessage =
      CorruptionDetectionMessage::Builder()
          .WithSequenceIndex(0b0110'1111)
          .WithInterpretSequenceIndexAsMostSignificantBits(true)
          .WithStdDev(8.0)
          .WithSampleValues(kSampleValues)
          .Build();
  uint8_t data[] = {0, 0, 0, 0};

  ASSERT_NE(kMessage, std::nullopt);
  EXPECT_TRUE(CorruptionDetectionExtension::Write(data, *kMessage));
  EXPECT_THAT(data, ElementsAre(0b1110'1111, 51, 0, 1));
}

TEST(CorruptionDetectionExtensionTest,
     WritesEverythingToExtensionWhenUpperBitsAreUsedForSequenceIndex) {
  const double kSampleValues[] = {1.0, 2.0, 3.0,  4.0,  5.0,  6.0, 7.0,
                                  8.0, 9.0, 10.0, 11.0, 12.0, 13.0};
  const std::optional<CorruptionDetectionMessage> kMessage =
      CorruptionDetectionMessage::Builder()
          .WithSequenceIndex(0b0110'1111)
          .WithInterpretSequenceIndexAsMostSignificantBits(true)
          .WithStdDev(34.5098)  // 220 / (255.0 * 40.0)
          .WithLumaErrorThreshold(0b1110)
          .WithChromaErrorThreshold(0b1111)
          .WithSampleValues(kSampleValues)
          .Build();
  uint8_t data[16];

  ASSERT_NE(kMessage, std::nullopt);
  EXPECT_TRUE(CorruptionDetectionExtension::Write(data, *kMessage));
  EXPECT_THAT(data, ElementsAre(0b1110'1111, 220, 0b1110'1111, 1, 2, 3, 4, 5, 6,
                                7, 8, 9, 10, 11, 12, 13));
}

TEST(CorruptionDetectionExtensionTest,
     WritesEverythingToExtensionWhenLowerBitsAreUsedForSequenceIndex) {
  const double kSampleValues[] = {1.0, 2.0, 3.0,  4.0,  5.0,  6.0, 7.0,
                                  8.0, 9.0, 10.0, 11.0, 12.0, 13.0};
  const std::optional<CorruptionDetectionMessage> kMessage =
      CorruptionDetectionMessage::Builder()
          .WithSequenceIndex(0b0110'1111)
          .WithInterpretSequenceIndexAsMostSignificantBits(false)
          .WithStdDev(34.5098)  // 220 / (255.0 * 40.0)
          .WithLumaErrorThreshold(0b1110)
          .WithChromaErrorThreshold(0b1111)
          .WithSampleValues(kSampleValues)
          .Build();
  uint8_t data[16];

  ASSERT_NE(kMessage, std::nullopt);
  EXPECT_TRUE(CorruptionDetectionExtension::Write(data, *kMessage));
  EXPECT_THAT(data, ElementsAre(0b0110'1111, 220, 0b1110'1111, 1, 2, 3, 4, 5, 6,
                                7, 8, 9, 10, 11, 12, 13));
}

TEST(CorruptionDetectionExtensionTest, TruncatesSampleValuesWhenWriting) {
  const double kSampleValues[] = {1.4, 2.5, 3.6};
  const std::optional<CorruptionDetectionMessage> kMessage =
      CorruptionDetectionMessage::Builder()
          .WithSampleValues(kSampleValues)
          .Build();
  uint8_t data[6];

  ASSERT_NE(kMessage, std::nullopt);
  EXPECT_TRUE(CorruptionDetectionExtension::Write(data, *kMessage));
  EXPECT_THAT(data, ElementsAre(0, 0, 0, 1, 2, 3));
}

TEST(CorruptionDetectionExtensionTest, ParsesMandatoryFieldsFromExtension) {
  CorruptionDetectionMessage message;
  const uint8_t kData[] = {0b1110'1111};

  EXPECT_TRUE(CorruptionDetectionExtension::Parse(kData, &message));
  EXPECT_EQ(message.sequence_index(), 0b0110'1111);
  EXPECT_TRUE(message.interpret_sequence_index_as_most_significant_bits());
  EXPECT_THAT(message.std_dev(), DoubleEq(0.0));
  EXPECT_EQ(message.luma_error_threshold(), 0);
  EXPECT_EQ(message.chroma_error_threshold(), 0);
  EXPECT_THAT(message.sample_values(), ElementsAre());
}

TEST(CorruptionDetectionExtensionTest, FailsToParseWhenGivenTooFewFields) {
  CorruptionDetectionMessage message;
  const uint8_t kData[] = {0b1110'1111, 8, 0};

  EXPECT_FALSE(CorruptionDetectionExtension::Parse(kData, &message));
}

TEST(CorruptionDetectionExtensionTest,
     ParsesEverythingFromExtensionWhenUpperBitsAreUsedForSequenceIndex) {
  CorruptionDetectionMessage message;
  const uint8_t kSampleValues[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
  const uint8_t kData[] = {0b1100'0100,       220,
                           0b1110'1111,       kSampleValues[0],
                           kSampleValues[1],  kSampleValues[2],
                           kSampleValues[3],  kSampleValues[4],
                           kSampleValues[5],  kSampleValues[6],
                           kSampleValues[7],  kSampleValues[8],
                           kSampleValues[9],  kSampleValues[10],
                           kSampleValues[11], kSampleValues[12]};

  EXPECT_TRUE(CorruptionDetectionExtension::Parse(kData, &message));
  EXPECT_EQ(message.sequence_index(), 0b0100'0100);
  EXPECT_TRUE(message.interpret_sequence_index_as_most_significant_bits());
  EXPECT_THAT(message.std_dev(),
              DoubleEq(34.509803921568626));  // 220 / (255.0 * 40.0)
  EXPECT_EQ(message.luma_error_threshold(), 0b1110);
  EXPECT_EQ(message.chroma_error_threshold(), 0b1111);
  EXPECT_EQ(message.sample_values().size(), sizeof(kSampleValues));
  for (size_t i = 0; i < sizeof(kSampleValues); ++i) {
    EXPECT_EQ(message.sample_values()[i], kSampleValues[i]);
  }
}

TEST(CorruptionDetectionExtensionTest,
     ParsesEverythingFromExtensionWhenLowerBitsAreUsedForSequenceIndex) {
  CorruptionDetectionMessage message;
  const uint8_t kSampleValues[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
  const uint8_t kData[] = {0b0100'0100,       220,
                           0b1110'1111,       kSampleValues[0],
                           kSampleValues[1],  kSampleValues[2],
                           kSampleValues[3],  kSampleValues[4],
                           kSampleValues[5],  kSampleValues[6],
                           kSampleValues[7],  kSampleValues[8],
                           kSampleValues[9],  kSampleValues[10],
                           kSampleValues[11], kSampleValues[12]};

  EXPECT_TRUE(CorruptionDetectionExtension::Parse(kData, &message));
  EXPECT_EQ(message.sequence_index(), 0b0100'0100);
  EXPECT_FALSE(message.interpret_sequence_index_as_most_significant_bits());
  EXPECT_THAT(message.std_dev(),
              DoubleEq(34.509803921568626));  // 220 / (255.0 * 40.0)
  EXPECT_EQ(message.luma_error_threshold(), 0b1110);
  EXPECT_EQ(message.chroma_error_threshold(), 0b1111);
  EXPECT_EQ(message.sample_values().size(), sizeof(kSampleValues));
  for (size_t i = 0; i < sizeof(kSampleValues); ++i) {
    EXPECT_EQ(message.sample_values()[i], kSampleValues[i]);
  }
}

TEST(CorruptionDetectionExtensionTest, FailsToParseWhenGivenNullptrAsOutput) {
  const uint8_t kData[] = {0, 0, 0};

  EXPECT_FALSE(CorruptionDetectionExtension::Parse(kData, nullptr));
}

TEST(CorruptionDetectionExtensionTest,
     FailsToParseWhenTooManySamplesAreSpecified) {
  CorruptionDetectionMessage message;
  uint8_t data[17];

  EXPECT_FALSE(CorruptionDetectionExtension::Parse(data, &message));
}

}  // namespace
}  // namespace webrtc
