/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/capture_levels_adjuster/audio_samples_scaler.h"

#include <tuple>

#include "modules/audio_processing/test/audio_buffer_tools.h"
#include "rtc_base/strings/string_builder.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

float SampleValueForChannel(int channel) {
  constexpr float kSampleBaseValue = 100.f;
  constexpr float kSampleChannelOffset = 1.f;
  return kSampleBaseValue + channel * kSampleChannelOffset;
}

void PopulateBuffer(AudioBuffer& audio_buffer) {
  for (size_t ch = 0; ch < audio_buffer.num_channels(); ++ch) {
    test::FillBufferChannel(SampleValueForChannel(ch), ch, audio_buffer);
  }
}

constexpr int kNumFramesToProcess = 10;

class AudioSamplesScalerTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<int, int, float>> {
 protected:
  int sample_rate_hz() const { return std::get<0>(GetParam()); }
  int num_channels() const { return std::get<1>(GetParam()); }
  float initial_gain() const { return std::get<2>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(
    AudioSamplesScalerTestSuite,
    AudioSamplesScalerTest,
    ::testing::Combine(::testing::Values(16000, 32000, 48000),
                       ::testing::Values(1, 2, 4),
                       ::testing::Values(0.1f, 1.f, 2.f, 4.f)));

TEST_P(AudioSamplesScalerTest, InitialGainIsRespected) {
  AudioSamplesScaler scaler(initial_gain());

  AudioBuffer audio_buffer(sample_rate_hz(), num_channels(), sample_rate_hz(),
                           num_channels(), sample_rate_hz(), num_channels());

  for (int frame = 0; frame < kNumFramesToProcess; ++frame) {
    PopulateBuffer(audio_buffer);
    scaler.Process(audio_buffer);
    for (int ch = 0; ch < num_channels(); ++ch) {
      for (size_t i = 0; i < audio_buffer.num_frames(); ++i) {
        EXPECT_FLOAT_EQ(audio_buffer.channels_const()[ch][i],
                        initial_gain() * SampleValueForChannel(ch));
      }
    }
  }
}

TEST_P(AudioSamplesScalerTest, VerifyGainAdjustment) {
  const float higher_gain = initial_gain();
  const float lower_gain = higher_gain / 2.f;

  AudioSamplesScaler scaler(lower_gain);

  AudioBuffer audio_buffer(sample_rate_hz(), num_channels(), sample_rate_hz(),
                           num_channels(), sample_rate_hz(), num_channels());

  // Allow the intial, lower, gain to take effect.
  PopulateBuffer(audio_buffer);

  scaler.Process(audio_buffer);

  // Set the new, higher, gain.
  scaler.SetGain(higher_gain);

  // Ensure that the new, higher, gain is achieved gradually over one frame.
  PopulateBuffer(audio_buffer);

  scaler.Process(audio_buffer);
  for (int ch = 0; ch < num_channels(); ++ch) {
    for (size_t i = 0; i < audio_buffer.num_frames() - 1; ++i) {
      EXPECT_LT(audio_buffer.channels_const()[ch][i],
                higher_gain * SampleValueForChannel(ch));
      EXPECT_LE(audio_buffer.channels_const()[ch][i],
                audio_buffer.channels_const()[ch][i + 1]);
    }
    EXPECT_LE(audio_buffer.channels_const()[ch][audio_buffer.num_frames() - 1],
              higher_gain * SampleValueForChannel(ch));
  }

  // Ensure that the new, higher, gain is achieved and stay unchanged.
  for (int frame = 0; frame < kNumFramesToProcess; ++frame) {
    PopulateBuffer(audio_buffer);
    scaler.Process(audio_buffer);

    for (int ch = 0; ch < num_channels(); ++ch) {
      for (size_t i = 0; i < audio_buffer.num_frames(); ++i) {
        EXPECT_FLOAT_EQ(audio_buffer.channels_const()[ch][i],
                        higher_gain * SampleValueForChannel(ch));
      }
    }
  }

  // Set the new, lower, gain.
  scaler.SetGain(lower_gain);

  // Ensure that the new, lower, gain is achieved gradually over one frame.
  PopulateBuffer(audio_buffer);
  scaler.Process(audio_buffer);
  for (int ch = 0; ch < num_channels(); ++ch) {
    for (size_t i = 0; i < audio_buffer.num_frames() - 1; ++i) {
      EXPECT_GT(audio_buffer.channels_const()[ch][i],
                lower_gain * SampleValueForChannel(ch));
      EXPECT_GE(audio_buffer.channels_const()[ch][i],
                audio_buffer.channels_const()[ch][i + 1]);
    }
    EXPECT_GE(audio_buffer.channels_const()[ch][audio_buffer.num_frames() - 1],
              lower_gain * SampleValueForChannel(ch));
  }

  // Ensure that the new, lower, gain is achieved and stay unchanged.
  for (int frame = 0; frame < kNumFramesToProcess; ++frame) {
    PopulateBuffer(audio_buffer);
    scaler.Process(audio_buffer);

    for (int ch = 0; ch < num_channels(); ++ch) {
      for (size_t i = 0; i < audio_buffer.num_frames(); ++i) {
        EXPECT_FLOAT_EQ(audio_buffer.channels_const()[ch][i],
                        lower_gain * SampleValueForChannel(ch));
      }
    }
  }
}

TEST(AudioSamplesScaler, UpwardsClamping) {
  constexpr int kSampleRateHz = 48000;
  constexpr int kNumChannels = 1;
  constexpr float kGain = 10.f;
  constexpr float kMaxClampedSampleValue = 32767.f;
  static_assert(kGain > 1.f, "");

  AudioSamplesScaler scaler(kGain);

  AudioBuffer audio_buffer(kSampleRateHz, kNumChannels, kSampleRateHz,
                           kNumChannels, kSampleRateHz, kNumChannels);

  for (int frame = 0; frame < kNumFramesToProcess; ++frame) {
    for (size_t ch = 0; ch < audio_buffer.num_channels(); ++ch) {
      test::FillBufferChannel(
          kMaxClampedSampleValue - audio_buffer.num_channels() + 1.f + ch, ch,
          audio_buffer);
    }

    scaler.Process(audio_buffer);
    for (int ch = 0; ch < kNumChannels; ++ch) {
      for (size_t i = 0; i < audio_buffer.num_frames(); ++i) {
        EXPECT_FLOAT_EQ(audio_buffer.channels_const()[ch][i],
                        kMaxClampedSampleValue);
      }
    }
  }
}

TEST(AudioSamplesScaler, DownwardsClamping) {
  constexpr int kSampleRateHz = 48000;
  constexpr int kNumChannels = 1;
  constexpr float kGain = 10.f;
  constexpr float kMinClampedSampleValue = -32768.f;
  static_assert(kGain > 1.f, "");

  AudioSamplesScaler scaler(kGain);

  AudioBuffer audio_buffer(kSampleRateHz, kNumChannels, kSampleRateHz,
                           kNumChannels, kSampleRateHz, kNumChannels);

  for (int frame = 0; frame < kNumFramesToProcess; ++frame) {
    for (size_t ch = 0; ch < audio_buffer.num_channels(); ++ch) {
      test::FillBufferChannel(
          kMinClampedSampleValue + audio_buffer.num_channels() - 1.f + ch, ch,
          audio_buffer);
    }

    scaler.Process(audio_buffer);
    for (int ch = 0; ch < kNumChannels; ++ch) {
      for (size_t i = 0; i < audio_buffer.num_frames(); ++i) {
        EXPECT_FLOAT_EQ(audio_buffer.channels_const()[ch][i],
                        kMinClampedSampleValue);
      }
    }
  }
}

}  // namespace
}  // namespace webrtc
