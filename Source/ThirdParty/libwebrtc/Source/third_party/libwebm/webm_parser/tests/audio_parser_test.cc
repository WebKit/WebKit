// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "src/audio_parser.h"

#include "gtest/gtest.h"

#include "test_utils/element_parser_test.h"
#include "webm/id.h"

using webm::Audio;
using webm::AudioParser;
using webm::ElementParserTest;
using webm::Id;

namespace {

class AudioParserTest : public ElementParserTest<AudioParser, Id::kAudio> {};

TEST_F(AudioParserTest, DefaultParse) {
  ParseAndVerify();

  const Audio audio = parser_.value();

  EXPECT_FALSE(audio.sampling_frequency.is_present());
  EXPECT_EQ(8000, audio.sampling_frequency.value());

  EXPECT_FALSE(audio.output_frequency.is_present());
  EXPECT_EQ(8000, audio.output_frequency.value());

  EXPECT_FALSE(audio.channels.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(1), audio.channels.value());

  EXPECT_FALSE(audio.bit_depth.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), audio.bit_depth.value());
}

TEST_F(AudioParserTest, DefaultValues) {
  SetReaderData({
      0xB5,  // ID = 0x85 (SamplingFrequency).
      0x40, 0x00,  // Size = 0.

      0x78, 0xB5,  // ID = 0x78B5 (OutputSamplingFrequency).
      0x80,  // Size = 0.

      0x9F,  // ID = 0x9F (Channels).
      0x40, 0x00,  // Size = 0.

      0x62, 0x64,  // ID = 0x6264 (BitDepth).
      0x80,  // Size = 0.
  });

  ParseAndVerify();

  const Audio audio = parser_.value();

  EXPECT_TRUE(audio.sampling_frequency.is_present());
  EXPECT_EQ(8000, audio.sampling_frequency.value());

  EXPECT_TRUE(audio.output_frequency.is_present());
  EXPECT_EQ(8000, audio.output_frequency.value());

  EXPECT_TRUE(audio.channels.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(1), audio.channels.value());

  EXPECT_TRUE(audio.bit_depth.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), audio.bit_depth.value());
}

TEST_F(AudioParserTest, CustomValues) {
  SetReaderData({
      0xB5,  // ID = 0x85 (SamplingFrequency).
      0x84,  // Size = 4.
      0x3F, 0x80, 0x00, 0x00,  // Body (value = 1.0f).

      0x78, 0xB5,  // ID = 0x78B5 (OutputSamplingFrequency).
      0x84,  // Size = 4.
      0x3F, 0xDD, 0xB3, 0xD7,  // Body (value = 1.73205077648162841796875f).

      0x9F,  // ID = 0x9F (Channels).
      0x10, 0x00, 0x00, 0x01,  // Size = 1.
      0x02,  // Body (value = 2).

      0x62, 0x64,  // ID = 0x6264 (BitDepth).
      0x10, 0x00, 0x00, 0x01,  // Size = 1.
      0x01,  // Body (value = 1).
  });

  ParseAndVerify();

  const Audio audio = parser_.value();

  EXPECT_TRUE(audio.sampling_frequency.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(1), audio.sampling_frequency.value());

  EXPECT_TRUE(audio.output_frequency.is_present());
  EXPECT_EQ(1.73205077648162841796875, audio.output_frequency.value());

  EXPECT_TRUE(audio.channels.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(2), audio.channels.value());

  EXPECT_TRUE(audio.bit_depth.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(1), audio.bit_depth.value());
}

TEST_F(AudioParserTest, AbsentOutputSamplingFrequency) {
  SetReaderData({
      0xB5,  // ID = 0x85 (SamplingFrequency).
      0x84,  // Size = 4.
      0x3F, 0x80, 0x00, 0x00,  // Body (value = 1.0f).
  });

  ParseAndVerify();

  const Audio audio = parser_.value();

  EXPECT_TRUE(audio.sampling_frequency.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(1), audio.sampling_frequency.value());

  EXPECT_FALSE(audio.output_frequency.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(1), audio.output_frequency.value());

  EXPECT_FALSE(audio.channels.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(1), audio.channels.value());

  EXPECT_FALSE(audio.bit_depth.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), audio.bit_depth.value());
}

TEST_F(AudioParserTest, DefaultOutputSamplingFrequency) {
  SetReaderData({
      0xB5,  // ID = 0x85 (SamplingFrequency).
      0x84,  // Size = 4.
      0x3F, 0x80, 0x00, 0x00,  // Body (value = 1.0f).

      0x78, 0xB5,  // ID = 0x78B5 (OutputSamplingFrequency).
      0x10, 0x00, 0x00, 0x00,  // Size = 0.
  });

  ParseAndVerify();

  const Audio audio = parser_.value();

  EXPECT_TRUE(audio.sampling_frequency.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(1), audio.sampling_frequency.value());

  EXPECT_TRUE(audio.output_frequency.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(1), audio.output_frequency.value());

  EXPECT_FALSE(audio.channels.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(1), audio.channels.value());

  EXPECT_FALSE(audio.bit_depth.is_present());
  EXPECT_EQ(static_cast<std::uint64_t>(0), audio.bit_depth.value());
}

}  // namespace
