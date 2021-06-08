/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_mixer/frame_combiner.h"

#include <cstdint>
#include <initializer_list>
#include <numeric>
#include <string>
#include <type_traits>

#include "api/array_view.h"
#include "audio/utility/audio_frame_operations.h"
#include "modules/audio_mixer/gain_change_calculator.h"
#include "modules/audio_mixer/sine_wave_generator.h"
#include "rtc_base/checks.h"
#include "rtc_base/strings/string_builder.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
using LimiterType = FrameCombiner::LimiterType;
struct FrameCombinerConfig {
  bool use_limiter;
  int sample_rate_hz;
  int number_of_channels;
  float wave_frequency;
};

std::string ProduceDebugText(int sample_rate_hz,
                             int number_of_channels,
                             int number_of_sources) {
  rtc::StringBuilder ss;
  ss << "Sample rate: " << sample_rate_hz << " ,";
  ss << "number of channels: " << number_of_channels << " ,";
  ss << "number of sources: " << number_of_sources;
  return ss.Release();
}

std::string ProduceDebugText(const FrameCombinerConfig& config) {
  rtc::StringBuilder ss;
  ss << "Sample rate: " << config.sample_rate_hz << " ,";
  ss << "number of channels: " << config.number_of_channels << " ,";
  ss << "limiter active: " << (config.use_limiter ? "on" : "off") << " ,";
  ss << "wave frequency: " << config.wave_frequency << " ,";
  return ss.Release();
}

AudioFrame frame1;
AudioFrame frame2;
AudioFrame audio_frame_for_mixing;

void SetUpFrames(int sample_rate_hz, int number_of_channels) {
  for (auto* frame : {&frame1, &frame2}) {
    frame->UpdateFrame(0, nullptr, rtc::CheckedDivExact(sample_rate_hz, 100),
                       sample_rate_hz, AudioFrame::kNormalSpeech,
                       AudioFrame::kVadActive, number_of_channels);
  }
}
}  // namespace

// The limiter requires sample rate divisible by 2000.
TEST(FrameCombiner, BasicApiCallsLimiter) {
  FrameCombiner combiner(true);
  for (const int rate : {8000, 18000, 34000, 48000}) {
    for (const int number_of_channels : {1, 2, 4, 8}) {
      const std::vector<AudioFrame*> all_frames = {&frame1, &frame2};
      SetUpFrames(rate, number_of_channels);

      for (const int number_of_frames : {0, 1, 2}) {
        SCOPED_TRACE(
            ProduceDebugText(rate, number_of_channels, number_of_frames));
        const std::vector<AudioFrame*> frames_to_combine(
            all_frames.begin(), all_frames.begin() + number_of_frames);
        combiner.Combine(frames_to_combine, number_of_channels, rate,
                         frames_to_combine.size(), &audio_frame_for_mixing);
      }
    }
  }
}

// There are DCHECKs in place to check for invalid parameters.
TEST(FrameCombinerDeathTest, DebugBuildCrashesWithManyChannels) {
  FrameCombiner combiner(true);
  for (const int rate : {8000, 18000, 34000, 48000}) {
    for (const int number_of_channels : {10, 20, 21}) {
      if (static_cast<size_t>(rate / 100 * number_of_channels) >
          AudioFrame::kMaxDataSizeSamples) {
        continue;
      }
      const std::vector<AudioFrame*> all_frames = {&frame1, &frame2};
      SetUpFrames(rate, number_of_channels);

      const int number_of_frames = 2;
      SCOPED_TRACE(
          ProduceDebugText(rate, number_of_channels, number_of_frames));
      const std::vector<AudioFrame*> frames_to_combine(
          all_frames.begin(), all_frames.begin() + number_of_frames);
#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
      EXPECT_DEATH(
          combiner.Combine(frames_to_combine, number_of_channels, rate,
                           frames_to_combine.size(), &audio_frame_for_mixing),
          "");
#elif !RTC_DCHECK_IS_ON
      combiner.Combine(frames_to_combine, number_of_channels, rate,
                       frames_to_combine.size(), &audio_frame_for_mixing);
#endif
    }
  }
}

TEST(FrameCombinerDeathTest, DebugBuildCrashesWithHighRate) {
  FrameCombiner combiner(true);
  for (const int rate : {50000, 96000, 128000, 196000}) {
    for (const int number_of_channels : {1, 2, 3}) {
      if (static_cast<size_t>(rate / 100 * number_of_channels) >
          AudioFrame::kMaxDataSizeSamples) {
        continue;
      }
      const std::vector<AudioFrame*> all_frames = {&frame1, &frame2};
      SetUpFrames(rate, number_of_channels);

      const int number_of_frames = 2;
      SCOPED_TRACE(
          ProduceDebugText(rate, number_of_channels, number_of_frames));
      const std::vector<AudioFrame*> frames_to_combine(
          all_frames.begin(), all_frames.begin() + number_of_frames);
#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
      EXPECT_DEATH(
          combiner.Combine(frames_to_combine, number_of_channels, rate,
                           frames_to_combine.size(), &audio_frame_for_mixing),
          "");
#elif !RTC_DCHECK_IS_ON
      combiner.Combine(frames_to_combine, number_of_channels, rate,
                       frames_to_combine.size(), &audio_frame_for_mixing);
#endif
    }
  }
}

// With no limiter, the rate has to be divisible by 100 since we use
// 10 ms frames.
TEST(FrameCombiner, BasicApiCallsNoLimiter) {
  FrameCombiner combiner(false);
  for (const int rate : {8000, 10000, 11000, 32000, 44100}) {
    for (const int number_of_channels : {1, 2, 4, 8}) {
      const std::vector<AudioFrame*> all_frames = {&frame1, &frame2};
      SetUpFrames(rate, number_of_channels);

      for (const int number_of_frames : {0, 1, 2}) {
        SCOPED_TRACE(
            ProduceDebugText(rate, number_of_channels, number_of_frames));
        const std::vector<AudioFrame*> frames_to_combine(
            all_frames.begin(), all_frames.begin() + number_of_frames);
        combiner.Combine(frames_to_combine, number_of_channels, rate,
                         frames_to_combine.size(), &audio_frame_for_mixing);
      }
    }
  }
}

TEST(FrameCombiner, CombiningZeroFramesShouldProduceSilence) {
  FrameCombiner combiner(false);
  for (const int rate : {8000, 10000, 11000, 32000, 44100}) {
    for (const int number_of_channels : {1, 2}) {
      SCOPED_TRACE(ProduceDebugText(rate, number_of_channels, 0));

      const std::vector<AudioFrame*> frames_to_combine;
      combiner.Combine(frames_to_combine, number_of_channels, rate,
                       frames_to_combine.size(), &audio_frame_for_mixing);

      const int16_t* audio_frame_for_mixing_data =
          audio_frame_for_mixing.data();
      const std::vector<int16_t> mixed_data(
          audio_frame_for_mixing_data,
          audio_frame_for_mixing_data + number_of_channels * rate / 100);

      const std::vector<int16_t> expected(number_of_channels * rate / 100, 0);
      EXPECT_EQ(mixed_data, expected);
    }
  }
}

TEST(FrameCombiner, CombiningOneFrameShouldNotChangeFrame) {
  FrameCombiner combiner(false);
  for (const int rate : {8000, 10000, 11000, 32000, 44100}) {
    for (const int number_of_channels : {1, 2, 4, 8, 10}) {
      SCOPED_TRACE(ProduceDebugText(rate, number_of_channels, 1));

      SetUpFrames(rate, number_of_channels);
      int16_t* frame1_data = frame1.mutable_data();
      std::iota(frame1_data, frame1_data + number_of_channels * rate / 100, 0);
      const std::vector<AudioFrame*> frames_to_combine = {&frame1};
      combiner.Combine(frames_to_combine, number_of_channels, rate,
                       frames_to_combine.size(), &audio_frame_for_mixing);

      const int16_t* audio_frame_for_mixing_data =
          audio_frame_for_mixing.data();
      const std::vector<int16_t> mixed_data(
          audio_frame_for_mixing_data,
          audio_frame_for_mixing_data + number_of_channels * rate / 100);

      std::vector<int16_t> expected(number_of_channels * rate / 100);
      std::iota(expected.begin(), expected.end(), 0);
      EXPECT_EQ(mixed_data, expected);
    }
  }
}

// Send a sine wave through the FrameCombiner, and check that the
// difference between input and output varies smoothly. Also check
// that it is inside reasonable bounds. This is to catch issues like
// chromium:695993 and chromium:816875.
TEST(FrameCombiner, GainCurveIsSmoothForAlternatingNumberOfStreams) {
  // Rates are divisible by 2000 when limiter is active.
  std::vector<FrameCombinerConfig> configs = {
      {false, 30100, 2, 50.f},  {false, 16500, 1, 3200.f},
      {true, 8000, 1, 3200.f},  {true, 16000, 1, 50.f},
      {true, 18000, 8, 3200.f}, {true, 10000, 2, 50.f},
  };

  for (const auto& config : configs) {
    SCOPED_TRACE(ProduceDebugText(config));

    FrameCombiner combiner(config.use_limiter);

    constexpr int16_t wave_amplitude = 30000;
    SineWaveGenerator wave_generator(config.wave_frequency, wave_amplitude);

    GainChangeCalculator change_calculator;
    float cumulative_change = 0.f;

    constexpr size_t iterations = 100;

    for (size_t i = 0; i < iterations; ++i) {
      SetUpFrames(config.sample_rate_hz, config.number_of_channels);
      wave_generator.GenerateNextFrame(&frame1);
      AudioFrameOperations::Mute(&frame2);

      std::vector<AudioFrame*> frames_to_combine = {&frame1};
      if (i % 2 == 0) {
        frames_to_combine.push_back(&frame2);
      }
      const size_t number_of_samples =
          frame1.samples_per_channel_ * config.number_of_channels;

      // Ensures limiter is on if 'use_limiter'.
      constexpr size_t number_of_streams = 2;
      combiner.Combine(frames_to_combine, config.number_of_channels,
                       config.sample_rate_hz, number_of_streams,
                       &audio_frame_for_mixing);
      cumulative_change += change_calculator.CalculateGainChange(
          rtc::ArrayView<const int16_t>(frame1.data(), number_of_samples),
          rtc::ArrayView<const int16_t>(audio_frame_for_mixing.data(),
                                        number_of_samples));
    }

    // Check that the gain doesn't vary too much.
    EXPECT_LT(cumulative_change, 10);

    // Check that the latest gain is within reasonable bounds. It
    // should be slightly less that 1.
    EXPECT_LT(0.9f, change_calculator.LatestGain());
    EXPECT_LT(change_calculator.LatestGain(), 1.01f);
  }
}
}  // namespace webrtc
