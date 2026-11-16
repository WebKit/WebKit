/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/audio_buffer.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <tuple>
#include <vector>

#include "api/audio/audio_processing.h"
#include "api/audio/audio_view.h"
#include "rtc_base/checks.h"
#include "test/gtest.h"
#include "test/testsupport/rtc_expect_death.h"

namespace webrtc {

namespace {

void ExpectNumChannels(const AudioBuffer& ab, size_t num_channels) {
  EXPECT_EQ(ab.num_channels(), num_channels);
}

void FillChannelWith100HzSine(int channel, float amplitude, AudioBuffer& ab) {
  constexpr float kPi = std::numbers::pi;
  float sample_rate_hz;
  if (ab.num_frames() == 160) {
    sample_rate_hz = 16000.0f;
  } else if (ab.num_frames() == 320) {
    sample_rate_hz = 32000.0f;
  } else {
    sample_rate_hz = 48000.0f;
  }

  constexpr float kFrequencyHz = 100.0f;
  for (size_t i = 0; i < ab.num_frames(); ++i) {
    ab.channels()[channel][i] =
        amplitude * std::sin(2 * kPi * kFrequencyHz / sample_rate_hz * i);
  }
}

void FillChannelWith100HzSine(int sample_rate_hz,
                              int channel,
                              float amplitude,
                              float* const* stacked_data) {
  constexpr float kPi = std::numbers::pi;
  int num_samples_per_channel;
  if (sample_rate_hz == 16000) {
    num_samples_per_channel = 160;
  } else if (sample_rate_hz == 32000) {
    num_samples_per_channel = 320;
  } else {
    num_samples_per_channel = 480;
  }

  constexpr float kFrequencyHz = 100.0f;
  for (int i = 0; i < num_samples_per_channel; ++i) {
    stacked_data[channel][i] =
        amplitude * std::sin(2 * kPi * kFrequencyHz / sample_rate_hz * i);
  }
}

void FillChannelWith100HzSine(int sample_rate_hz,
                              int num_channels,
                              int channel,
                              float amplitude,
                              int16_t* const interleaved_data) {
  constexpr float kPi = std::numbers::pi;
  int num_samples_per_channel;
  if (sample_rate_hz == 16000) {
    num_samples_per_channel = 160;
  } else if (sample_rate_hz == 32000) {
    num_samples_per_channel = 320;
  } else {
    num_samples_per_channel = 480;
  }

  constexpr float kFrequencyHz = 100.0f;
  for (int i = 0; i < num_samples_per_channel; ++i) {
    interleaved_data[channel + i * num_channels] = static_cast<int16_t>(
        amplitude * std::sin(2 * kPi * kFrequencyHz / sample_rate_hz * i));
  }
}

enum class DownmixingSignalVariant {
  kInactive,
  kChannel0Inactive,
  kVeryImbalanced,
  kBalanced
};

}  // namespace

class AudioBufferMonoInputDownmixingTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<
          std::tuple<int,
                     int,
                     AudioProcessing::Config::Pipeline::DownmixMethod>> {
 protected:
  int Rate1() const { return std::get<0>(GetParam()); }
  int Rate2() const { return std::get<1>(GetParam()); }
  AudioProcessing::Config::Pipeline::DownmixMethod DownmixingMethod() const {
    return std::get<2>(GetParam());
  }
};

INSTANTIATE_TEST_SUITE_P(
    AudioBufferTests,
    AudioBufferMonoInputDownmixingTest,
    ::testing::Combine(
        ::testing::Values(16000, 32000, 48000),
        ::testing::Values(16000, 32000, 48000),
        ::testing::Values(
            AudioProcessing::Config::Pipeline::DownmixMethod::kAverageChannels,
            AudioProcessing::Config::Pipeline::DownmixMethod::kAdaptive)));

class AudioBufferStereoInputDownmixingTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<
          std::tuple<int,
                     int,
                     int,
                     AudioProcessing::Config::Pipeline::DownmixMethod,
                     bool,
                     bool,
                     DownmixingSignalVariant>> {
 protected:
  int Rate1() const { return std::get<0>(GetParam()); }
  int Rate2() const { return std::get<1>(GetParam()); }
  size_t NumChannels() const { return std::get<2>(GetParam()); }
  AudioProcessing::Config::Pipeline::DownmixMethod DownmixingMethod() const {
    return std::get<3>(GetParam());
  }
  bool OnlyInitialFrames() const { return std::get<4>(GetParam()); }
  bool UseFloatInterface() const { return std::get<5>(GetParam()); }
  DownmixingSignalVariant SignalVariant() const {
    return std::get<6>(GetParam());
  }
};

INSTANTIATE_TEST_SUITE_P(
    AudioBufferTests,
    AudioBufferStereoInputDownmixingTest,
    ::testing::Combine(
        ::testing::Values(16000, 32000, 48000),
        ::testing::Values(16000, 32000, 48000),
        ::testing::Values(1, 2),
        ::testing::Values(
            AudioProcessing::Config::Pipeline::DownmixMethod::kAverageChannels,
            AudioProcessing::Config::Pipeline::DownmixMethod::kAdaptive),
        ::testing::Values(false, true),
        ::testing::Values(false, true),
        ::testing::Values(DownmixingSignalVariant::kInactive,
                          DownmixingSignalVariant::kChannel0Inactive,
                          DownmixingSignalVariant::kVeryImbalanced,
                          DownmixingSignalVariant::kBalanced)

            ));

class AudioBufferChannelCountAndTwoRatesTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<int, int, int>> {
 protected:
  int Rate1() const { return std::get<0>(GetParam()); }
  int Rate2() const { return std::get<1>(GetParam()); }
  int NumChannels() const { return std::get<2>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(
    AudioBufferTests,
    AudioBufferChannelCountAndTwoRatesTest,
    ::testing::Combine(::testing::Values(16000, 32000, 48000),
                       ::testing::Values(16000, 32000, 48000),
                       ::testing::Values(1, 2)));

class AudioBufferChannelCountAndOneRateTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<int, int>> {
 protected:
  int Rate() const { return std::get<0>(GetParam()); }
  int NumChannels() const { return std::get<1>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(
    AudioBufferTests,
    AudioBufferChannelCountAndOneRateTest,
    ::testing::Combine(::testing::Values(16000, 32000, 48000),
                       ::testing::Values(1, 2)));

TEST(AudioBufferTest, SetNumChannelsSetsChannelBuffersNumChannels) {
  constexpr size_t kSampleRateHz = 48000u;
  AudioBuffer ab(kSampleRateHz, 2, kSampleRateHz, 2, kSampleRateHz, 2);
  ExpectNumChannels(ab, 2);
  ab.set_num_channels(1);
  ExpectNumChannels(ab, 1);
  ab.RestoreNumChannels();
  ExpectNumChannels(ab, 2);
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST(AudioBufferDeathTest, SetNumChannelsDeathTest) {
  constexpr size_t kSampleRateHz = 48000u;
  AudioBuffer ab(kSampleRateHz, 1, kSampleRateHz, 1, kSampleRateHz, 1);
  RTC_EXPECT_DEATH(ab.set_num_channels(2), "num_channels");
}
#endif

TEST_P(AudioBufferChannelCountAndOneRateTest, CopyWithoutResampling) {
  AudioBuffer ab1(Rate(), NumChannels(), Rate(), NumChannels(), Rate(),
                  NumChannels());
  AudioBuffer ab2(Rate(), NumChannels(), Rate(), NumChannels(), Rate(),
                  NumChannels());
  // Fill first buffer.
  for (size_t ch = 0; ch < ab1.num_channels(); ++ch) {
    for (size_t i = 0; i < ab1.num_frames(); ++i) {
      ab1.channels()[ch][i] = i + ch;
    }
  }
  // Copy to second buffer.
  ab1.CopyTo(&ab2);
  // Verify content of second buffer.
  for (size_t ch = 0; ch < ab2.num_channels(); ++ch) {
    for (size_t i = 0; i < ab2.num_frames(); ++i) {
      EXPECT_EQ(ab2.channels()[ch][i], i + ch);
    }
  }
}

TEST_P(AudioBufferChannelCountAndTwoRatesTest, CopyWithResampling) {
  AudioBuffer ab1(Rate1(), NumChannels(), Rate1(), NumChannels(), Rate2(),
                  NumChannels());
  AudioBuffer ab2(Rate2(), NumChannels(), Rate2(), NumChannels(), Rate2(),
                  NumChannels());
  float energy_ab1 = 0.0f;
  float energy_ab2 = 0.0f;
  // Put a sine and compute energy of first buffer.
  for (size_t ch = 0; ch < ab1.num_channels(); ++ch) {
    FillChannelWith100HzSine(ch, 1.0f, ab1);

    for (size_t i = 0; i < ab1.num_frames(); ++i) {
      energy_ab1 += ab1.channels()[ch][i] * ab1.channels()[ch][i];
    }
  }
  // Copy to second buffer.
  ab1.CopyTo(&ab2);
  // Compute energy of second buffer.
  for (size_t ch = 0; ch < ab2.num_channels(); ++ch) {
    for (size_t i = 0; i < ab2.num_frames(); ++i) {
      energy_ab2 += ab2.channels()[ch][i] * ab2.channels()[ch][i];
    }
  }
  // Verify that energies match.
  EXPECT_NEAR(energy_ab1, energy_ab2 * Rate1() / Rate2(), .04f * energy_ab1);
}

TEST_P(AudioBufferChannelCountAndOneRateTest, DeinterleavedView) {
  AudioBuffer ab(Rate(), NumChannels(), Rate(), NumChannels(), Rate(),
                 NumChannels());
  // Fill the buffer with data.
  for (size_t ch = 0; ch < ab.num_channels(); ++ch) {
    FillChannelWith100HzSine(ch, 1.0f, ab);
  }

  // Verify that the DeinterleavedView correctly maps to channels.
  DeinterleavedView<float> view = ab.view();
  ASSERT_EQ(view.num_channels(), ab.num_channels());
  float* const* channels = ab.channels();
  for (size_t c = 0; c < view.num_channels(); ++c) {
    MonoView<float> channel = view[c];
    EXPECT_EQ(SamplesPerChannel(channel), ab.num_frames());
    for (size_t s = 0; s < SamplesPerChannel(channel); ++s) {
      ASSERT_EQ(channel[s], channels[c][s]);
    }
  }
}

TEST_P(AudioBufferMonoInputDownmixingTest, MonoCaptureStacked) {
  AudioProcessing::Config::Pipeline::DownmixMethod downmixing_method =
      DownmixingMethod();
  int num_frames_per_channel = Rate1() / 100;
  std::vector<float> audio_data(num_frames_per_channel);
  std::array<float*, 1> audio = {audio_data.data()};
  AudioBuffer ab(Rate1(), 1, Rate2(), 1, Rate2(), downmixing_method);
  float energy_input = 0.0f;
  float energy_ab = 0.0f;
  // Put a sine and compute energy of first buffer (compensating for the
  // internal S16 format in AudioBuffer..
  FillChannelWith100HzSine(Rate1(), 0, 0.7f, audio.data());
  for (int i = 0; i < num_frames_per_channel; ++i) {
    energy_input += audio[0][i] * 32768.0f * audio[0][i] * 32768.0f;
    ;
  }

  // Copy to audio buffer.
  StreamConfig stream_config(Rate1(), 1);
  ab.CopyFrom(audio.data(), stream_config);

  // Verify that the channel count is correct.
  EXPECT_EQ(ab.num_channels(), 1u);

  // Compute energy of audio buffer.
  for (size_t i = 0; i < ab.num_frames(); ++i) {
    energy_ab += ab.channels()[0][i] * ab.channels()[0][i];
  }
  // Verify that energies match.
  EXPECT_NEAR(energy_input, energy_ab * Rate1() / Rate2(), .04f * energy_input);
}

TEST_P(AudioBufferMonoInputDownmixingTest, MonoCaptureInterleaved) {
  AudioProcessing::Config::Pipeline::DownmixMethod downmixing_method =
      DownmixingMethod();
  int num_frames_per_channel = Rate1() / 100;
  std::vector<int16_t> audio(num_frames_per_channel);
  AudioBuffer ab(Rate1(), 1, Rate2(), 1, Rate2(), downmixing_method);
  float energy_input = 0.0f;
  float energy_ab = 0.0f;
  // Put a sine and compute energy of first buffer (compensating for the
  // internal S16 format in AudioBuffer).
  FillChannelWith100HzSine(Rate1(), 1, 0, 0.7f, audio.data());
  for (int i = 0; i < num_frames_per_channel; ++i) {
    energy_input += audio[i] * 32768.0f * audio[i] * 32768.0f;
  }

  // Copy to audio buffer.
  StreamConfig stream_config(Rate1(), 1);
  ab.CopyFrom(audio.data(), stream_config);

  // Verify that the channel count is correct.
  EXPECT_EQ(ab.num_channels(), 1u);

  // Compute energy of audio buffer.
  for (size_t i = 0; i < ab.num_frames(); ++i) {
    energy_ab += ab.channels()[0][i] * ab.channels()[0][i];
  }
  // Verify that energies match.
  EXPECT_NEAR(energy_input, energy_ab * Rate1() / Rate2(), .04f * energy_input);
}

TEST_P(AudioBufferStereoInputDownmixingTest, StereoCapture) {
  AudioProcessing::Config::Pipeline::DownmixMethod downmixing_method =
      DownmixingMethod();
  int num_frames_per_channel = Rate1() / 100;
  AudioBuffer ab(Rate1(), 2, Rate2(), NumChannels(), Rate2(),
                 downmixing_method);
  float energy_ab_ch0 = 0.0f;
  float energy_ab_ch1 = 0.0f;
  float energy_input_ch0 = 0.0f;
  float energy_input_ch1 = 0.0f;
  float energy_input_average = 0.0f;

  const int num_frames_to_process = OnlyInitialFrames() ? 10 : 250;

  float amplitude0;
  float amplitude1;
  switch (SignalVariant()) {
    case DownmixingSignalVariant::kInactive:
      amplitude0 = 0.0001f;
      amplitude1 = 0.0002f;
      break;
    case DownmixingSignalVariant::kChannel0Inactive:
      amplitude0 = 0.0001f;
      amplitude1 = 0.8f;
      break;
    case DownmixingSignalVariant::kVeryImbalanced:
      amplitude0 = 0.01f;
      amplitude1 = 0.8f;
      break;
    case DownmixingSignalVariant::kBalanced:
      amplitude0 = 0.7f;
      amplitude1 = 0.8f;
      break;
  }

  if (UseFloatInterface()) {
    std::vector<float> audio_data(2 * num_frames_per_channel);
    std::array<float*, 2> audio = {&audio_data[0],
                                   &audio_data[num_frames_per_channel]};

    // Put a sine and compute energy of first buffer (compensating for the
    // internal S16 format in AudioBuffer).
    FillChannelWith100HzSine(Rate1(), 0, amplitude0, audio.data());
    FillChannelWith100HzSine(Rate1(), 1, amplitude1, audio.data());
    for (int i = 0; i < num_frames_per_channel; ++i) {
      energy_input_ch0 += (audio[0][i] * audio[0][i]) * 32768.0f * 32768.0f;
      energy_input_ch1 += (audio[1][i] * audio[1][i]) * 32768.0f * 32768.0f;
      float average = (audio[0][i] + audio[1][i]) * 32768.0f * 0.5f;
      energy_input_average += average * average;
    }

    // Copy to audio buffer.
    StreamConfig stream_config(Rate1(), 2);
    for (int k = 0; k < num_frames_to_process; ++k) {
      ab.CopyFrom(audio.data(), stream_config);

      // Verify that the channel count is correct.
      EXPECT_EQ(ab.num_channels(), NumChannels());
    }
  } else {
    std::vector<int16_t> audio(2 * num_frames_per_channel);
    // Put a sine and compute energy of first buffer (compensating for the
    // internal S16 format in AudioBuffer).
    FillChannelWith100HzSine(Rate1(), 2, 0, amplitude0, audio.data());
    FillChannelWith100HzSine(Rate1(), 2, 1, amplitude1, audio.data());
    for (int i = 0; i < num_frames_per_channel; ++i) {
      energy_input_ch0 += (audio[2 * i] * audio[2 * i]);
      energy_input_ch1 += (audio[2 * i + 1] * audio[2 * i + 1]);
      float average = (audio[2 * i] + audio[2 * i + 1]) * 0.5f;
      energy_input_average += average * average;
    }

    // Copy to audio buffer.
    StreamConfig stream_config(Rate1(), 2);
    for (int k = 0; k < num_frames_to_process; ++k) {
      ab.CopyFrom(audio.data(), stream_config);

      // Verify that the channel count is correct.
      EXPECT_EQ(ab.num_channels(), NumChannels());
    }
  }

  // Compute energy of audio buffer.
  for (size_t i = 0; i < ab.num_frames(); ++i) {
    energy_ab_ch0 += ab.channels()[0][i] * ab.channels()[0][i];
  }
  if (ab.num_channels() == 2) {
    for (size_t i = 0; i < ab.num_frames(); ++i) {
      energy_ab_ch1 += ab.channels()[1][i] * ab.channels()[1][i];
    }
  }

  // Verify that energies match.
  if (downmixing_method ==
      AudioProcessing::Config::Pipeline::DownmixMethod::kAverageChannels) {
    if (NumChannels() == 1) {
      EXPECT_NEAR(energy_input_average, energy_ab_ch0 * Rate1() / Rate2(),
                  .04f * energy_input_average);
    } else {
      EXPECT_NEAR(energy_input_ch0, energy_ab_ch0 * Rate1() / Rate2(),
                  .04f * energy_input_average);
      EXPECT_NEAR(energy_input_ch1, energy_ab_ch1 * Rate1() / Rate2(),
                  .04f * energy_input_average);
    }
    return;
  }
  ASSERT_EQ(downmixing_method,
            AudioProcessing::Config::Pipeline::DownmixMethod::kAdaptive);

  if (OnlyInitialFrames()) {
    EXPECT_NEAR(energy_input_average, energy_ab_ch0 * Rate1() / Rate2(),
                .04f * energy_input_average);
    if (NumChannels() == 2) {
      EXPECT_NEAR(energy_input_average, energy_ab_ch1 * Rate1() / Rate2(),
                  .04f * energy_input_average);
    }
    return;
  }

  switch (SignalVariant()) {
    case DownmixingSignalVariant::kInactive:
      EXPECT_NEAR(energy_input_average, energy_ab_ch0 * Rate1() / Rate2(),
                  .04f * energy_input_average);
      if (NumChannels() == 2) {
        EXPECT_NEAR(energy_input_average, energy_ab_ch1 * Rate1() / Rate2(),
                    .04f * energy_input_average);
      }
      break;
    case DownmixingSignalVariant::kChannel0Inactive:
      EXPECT_NEAR(energy_input_average, energy_ab_ch0 * Rate1() / Rate2(),
                  .04f * energy_input_average);
      if (NumChannels() == 2) {
        EXPECT_NEAR(energy_input_average, energy_ab_ch1 * Rate1() / Rate2(),
                    .04f * energy_input_average);
      }
      break;
    case DownmixingSignalVariant::kVeryImbalanced:
      EXPECT_NEAR(energy_input_ch1, energy_ab_ch0 * Rate1() / Rate2(),
                  .04f * energy_input_ch1);
      if (NumChannels() == 2) {
        EXPECT_NEAR(energy_input_ch1, energy_ab_ch1 * Rate1() / Rate2(),
                    .04f * energy_input_ch1);
      }
      break;
    case DownmixingSignalVariant::kBalanced:
      EXPECT_NEAR(energy_input_ch0, energy_ab_ch0 * Rate1() / Rate2(),
                  .04f * energy_input_ch0);
      if (NumChannels() == 2) {
        EXPECT_NEAR(energy_input_ch1, energy_ab_ch1 * Rate1() / Rate2(),
                    .04f * energy_input_ch0);
      }
      break;
  }
}

}  // namespace webrtc
