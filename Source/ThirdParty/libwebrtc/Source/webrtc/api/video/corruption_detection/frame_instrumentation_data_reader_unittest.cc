/*
 * Copyright 2025 The WebRTC project authors. All rights reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/corruption_detection/frame_instrumentation_data_reader.h"

#include <optional>
#include <vector>

#include "api/transport/rtp/corruption_detection_message.h"
#include "api/video/corruption_detection/frame_instrumentation_data.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::ElementsAre;
using ::testing::Eq;

namespace webrtc {

TEST(FrameInstrumentationDataReaderTest, AcceptsMsbFromStart) {
  FrameInstrumentationDataReader reader;
  std::optional<FrameInstrumentationData> data = reader.ParseMessage(
      *CorruptionDetectionMessage::Builder()
           .WithSequenceIndex(1)
           .WithInterpretSequenceIndexAsMostSignificantBits(true)
           .Build());

  EXPECT_TRUE(data.has_value());
  EXPECT_THAT(data->sequence_index(), Eq(1 << 7));
}

TEST(FrameInstrumentationDataReaderTest, RejectsLsbFromStart) {
  FrameInstrumentationDataReader reader;
  std::optional<FrameInstrumentationData> data = reader.ParseMessage(
      *CorruptionDetectionMessage::Builder()
           .WithSequenceIndex(1)
           .WithInterpretSequenceIndexAsMostSignificantBits(false)
           .Build());

  EXPECT_FALSE(data.has_value());
}

TEST(FrameInstrumentationDataReaderTest,
     IgnorePreviousSequenceIndexWhenSetToUpdateTheMostSignificantBits) {
  FrameInstrumentationDataReader reader;

  // Prime with sequence index 11 << 7.
  EXPECT_TRUE(
      reader
          .ParseMessage(
              *CorruptionDetectionMessage::Builder()
                   .WithSequenceIndex(11)
                   .WithInterpretSequenceIndexAsMostSignificantBits(true)
                   .Build())
          .has_value());

  // New absolute value 12 << 7 take precedence.
  std::optional<FrameInstrumentationData> data = reader.ParseMessage(
      *CorruptionDetectionMessage::Builder()
           .WithSequenceIndex(12)
           .WithInterpretSequenceIndexAsMostSignificantBits(true)
           .Build());

  ASSERT_TRUE(data.has_value());
  EXPECT_THAT(data->sequence_index(), Eq(12 << 7));
}

TEST(FrameInstrumentationDataReaderTest,
     UseMessageSequenceIndexWhenHigherThanPrevious) {
  FrameInstrumentationDataReader reader;

  // Prime with sequence index 11 << 7.
  EXPECT_TRUE(
      reader
          .ParseMessage(
              *CorruptionDetectionMessage::Builder()
                   .WithSequenceIndex(11)
                   .WithInterpretSequenceIndexAsMostSignificantBits(true)
                   .Build())
          .has_value());

  std::optional<FrameInstrumentationData> data = reader.ParseMessage(
      *CorruptionDetectionMessage::Builder()
           .WithSequenceIndex(12)
           .WithInterpretSequenceIndexAsMostSignificantBits(false)
           .Build());

  ASSERT_TRUE(data.has_value());
  EXPECT_EQ(data->sequence_index(), (11 << 7) + 12);
}

TEST(FrameInstrumentationDataReaderTest, HandlesMsbRollOver) {
  FrameInstrumentationDataReader reader;

  // Prime with sequence index 11 << 7.
  EXPECT_THAT(
      reader
          .ParseMessage(
              *CorruptionDetectionMessage::Builder()
                   .WithSequenceIndex(11)
                   .WithInterpretSequenceIndexAsMostSignificantBits(true)
                   .Build())
          ->sequence_index(),
      Eq(11 << 7));

  // Bump index by 100.
  EXPECT_THAT(
      reader
          .ParseMessage(
              *CorruptionDetectionMessage::Builder()
                   .WithSequenceIndex(100)
                   .WithInterpretSequenceIndexAsMostSignificantBits(false)
                   .Build())
          ->sequence_index(),
      Eq((11 << 7) + 100));

  // Bumping it again so that LSB = 1, MSB should increment to 12.
  EXPECT_THAT(
      reader
          .ParseMessage(
              *CorruptionDetectionMessage::Builder()
                   .WithSequenceIndex(1)
                   .WithInterpretSequenceIndexAsMostSignificantBits(false)
                   .Build())
          ->sequence_index(),
      Eq((12 << 7) + 1));
}

TEST(FrameInstrumentationDataReaderTest,
     IgnoreIndexUpdateWhenTheLowerBitsSuppliedAreTheSameAsInThePreviousIndex) {
  FrameInstrumentationDataReader reader;

  // Prime with sequence index 11 << 7.
  EXPECT_THAT(
      reader
          .ParseMessage(
              *CorruptionDetectionMessage::Builder()
                   .WithSequenceIndex(11)
                   .WithInterpretSequenceIndexAsMostSignificantBits(true)
                   .Build())
          ->sequence_index(),
      Eq(11 << 7));

  // LSB = 0, meaning it's the same sequence again - no increment.
  EXPECT_THAT(
      reader
          .ParseMessage(
              *CorruptionDetectionMessage::Builder()
                   .WithSequenceIndex(0)
                   .WithInterpretSequenceIndexAsMostSignificantBits(false)
                   .Build())
          ->sequence_index(),
      Eq(11 << 7));
}

TEST(FrameInstrumentationDataReaderTest, MaximumRollover) {
  FrameInstrumentationDataReader reader;

  // Prime with sequence index 11 << 7.
  EXPECT_THAT(
      reader
          .ParseMessage(
              *CorruptionDetectionMessage::Builder()
                   .WithSequenceIndex(11)
                   .WithInterpretSequenceIndexAsMostSignificantBits(true)
                   .Build())
          ->sequence_index(),
      Eq(11 << 7));

  // Bump index by 1.
  EXPECT_THAT(
      reader
          .ParseMessage(
              *CorruptionDetectionMessage::Builder()
                   .WithSequenceIndex(1)
                   .WithInterpretSequenceIndexAsMostSignificantBits(false)
                   .Build())
          ->sequence_index(),
      Eq((11 << 7) + 1));

  // Setting the LSB to one lower than current => maximum +127 jump.
  EXPECT_THAT(
      reader
          .ParseMessage(
              *CorruptionDetectionMessage::Builder()
                   .WithSequenceIndex(0)
                   .WithInterpretSequenceIndexAsMostSignificantBits(false)
                   .Build())
          ->sequence_index(),
      Eq(12 << 7));
}

TEST(FrameInstrumentationDataReaderTest, RolloverWithSamples) {
  FrameInstrumentationDataReader reader;

  // Prime with sequence index 11 << 7.
  EXPECT_THAT(
      reader
          .ParseMessage(
              *CorruptionDetectionMessage::Builder()
                   .WithSequenceIndex(11)
                   .WithInterpretSequenceIndexAsMostSignificantBits(true)
                   .Build())
          ->sequence_index(),
      Eq(11 << 7));

  // Bump index by one, but include 4 samples values, which count as index too.
  std::vector<double> sample_values = {1.0, 2.0, 3.0, 4.0};
  EXPECT_THAT(
      reader
          .ParseMessage(
              *CorruptionDetectionMessage::Builder()
                   .WithSequenceIndex(1)
                   .WithInterpretSequenceIndexAsMostSignificantBits(false)
                   .WithSampleValues(sample_values)
                   .Build())
          ->sequence_index(),
      Eq((11 << 7) + 1));

  // Set the LSB to 4, which is one less than the previous seen sequence, this
  // counts as a wraparound.
  EXPECT_THAT(
      reader
          .ParseMessage(
              *CorruptionDetectionMessage::Builder()
                   .WithSequenceIndex(4)
                   .WithInterpretSequenceIndexAsMostSignificantBits(false)
                   .Build())
          ->sequence_index(),
      Eq((12 << 7) + 4));
}

TEST(FrameInstrumentationDataReaderTest, ConvertAllFields) {
  std::vector<double> sample_values = {1.0, 2.0, 3.0, 4.0, 5.0};
  std::optional<CorruptionDetectionMessage> message =
      CorruptionDetectionMessage::Builder()
          .WithSequenceIndex(11)
          .WithInterpretSequenceIndexAsMostSignificantBits(true)
          .WithStdDev(1.2)
          .WithLumaErrorThreshold(10)
          .WithChromaErrorThreshold(10)
          .WithSampleValues(sample_values)
          .Build();
  ASSERT_TRUE(message.has_value());

  FrameInstrumentationDataReader reader;
  std::optional<FrameInstrumentationData> data = reader.ParseMessage(*message);

  ASSERT_TRUE(data.has_value());
  EXPECT_EQ(data->sequence_index(), 11 << 7);
  EXPECT_NEAR(data->std_dev(), 1.2, 0.024);  // ~2%
  EXPECT_EQ(data->luma_error_threshold(), 10);
  EXPECT_EQ(data->chroma_error_threshold(), 10);
  EXPECT_THAT(data->sample_values(), ElementsAre(1.0, 2.0, 3.0, 4.0, 5.0));
}

}  // namespace webrtc
