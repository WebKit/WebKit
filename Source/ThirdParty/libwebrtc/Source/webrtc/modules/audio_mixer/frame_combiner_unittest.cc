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
#include <vector>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/rtp_packet_info.h"
#include "api/rtp_packet_infos.h"
#include "api/units/timestamp.h"
#include "audio/utility/audio_frame_operations.h"
#include "modules/audio_mixer/gain_change_calculator.h"
#include "modules/audio_mixer/sine_wave_generator.h"
#include "rtc_base/checks.h"
#include "rtc_base/strings/string_builder.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

using ::testing::ElementsAreArray;
using ::testing::IsEmpty;
using ::testing::UnorderedElementsAreArray;

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

void SetUpFrames(int sample_rate_hz, int number_of_channels) {
  RtpPacketInfo packet_info1(/*ssrc=*/1001, /*csrcs=*/{},
                             /*rtp_timestamp=*/1000,
                             /*receive_time=*/Timestamp::Millis(1));
  RtpPacketInfo packet_info2(/*ssrc=*/4004, /*csrcs=*/{},
                             /*rtp_timestamp=*/1234,
                             /*receive_time=*/Timestamp::Millis(2));
  RtpPacketInfo packet_info3(/*ssrc=*/7007, /*csrcs=*/{},
                             /*rtp_timestamp=*/1333,
                             /*receive_time=*/Timestamp::Millis(2));

  frame1.packet_infos_ = RtpPacketInfos({packet_info1});
  frame2.packet_infos_ = RtpPacketInfos({packet_info2, packet_info3});

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
        AudioFrame audio_frame_for_mixing;
        combiner.Combine(frames_to_combine, number_of_channels, rate,
                         frames_to_combine.size(), &audio_frame_for_mixing);
      }
    }
  }
}

// The RtpPacketInfos field of the mixed packet should contain the union of the
// RtpPacketInfos from the frames that were actually mixed.
TEST(FrameCombiner, ContainsAllRtpPacketInfos) {
  static constexpr int kSampleRateHz = 48000;
  static constexpr int kNumChannels = 1;
  FrameCombiner combiner(true);
  const std::vector<AudioFrame*> all_frames = {&frame1, &frame2};
  SetUpFrames(kSampleRateHz, kNumChannels);

  for (const int number_of_frames : {0, 1, 2}) {
    SCOPED_TRACE(
        ProduceDebugText(kSampleRateHz, kNumChannels, number_of_frames));
    const std::vector<AudioFrame*> frames_to_combine(
        all_frames.begin(), all_frames.begin() + number_of_frames);

    std::vector<RtpPacketInfo> packet_infos;
    for (const auto& frame : frames_to_combine) {
      packet_infos.insert(packet_infos.end(), frame->packet_infos_.begin(),
                          frame->packet_infos_.end());
    }

    AudioFrame audio_frame_for_mixing;
    combiner.Combine(frames_to_combine, kNumChannels, kSampleRateHz,
                     frames_to_combine.size(), &audio_frame_for_mixing);
    EXPECT_THAT(audio_frame_for_mixing.packet_infos_,
                UnorderedElementsAreArray(packet_infos));
  }
}

#if GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
// There are CHECKs in place to check for invalid parameters.
TEST(FrameCombinerDeathTest, BuildCrashesWithManyChannels) {
  FrameCombiner combiner(true);
  for (const int rate : {8000, 18000, 34000, 48000}) {
    for (const int number_of_channels : {10, 20, 21}) {
      if (static_cast<size_t>(rate / 100 * number_of_channels) >
          AudioFrame::kMaxDataSizeSamples) {
        continue;
      }
      const std::vector<AudioFrame*> all_frames = {&frame1, &frame2};
      // With an unsupported channel count, this will crash in
      // `AudioFrame::UpdateFrame`.
      EXPECT_DEATH(SetUpFrames(rate, number_of_channels), "");

      const int number_of_frames = 2;
      SCOPED_TRACE(
          ProduceDebugText(rate, number_of_channels, number_of_frames));
      const std::vector<AudioFrame*> frames_to_combine(
          all_frames.begin(), all_frames.begin() + number_of_frames);
      AudioFrame audio_frame_for_mixing;
      EXPECT_DEATH(
          combiner.Combine(frames_to_combine, number_of_channels, rate,
                           frames_to_combine.size(), &audio_frame_for_mixing),
          "");
    }
  }
}
#endif  // GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

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
      AudioFrame audio_frame_for_mixing;
#if GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
      EXPECT_DEATH(
          combiner.Combine(frames_to_combine, number_of_channels, rate,
                           frames_to_combine.size(), &audio_frame_for_mixing),
          "")
          << "number_of_channels=" << number_of_channels << ", rate=" << rate
          << ", frames to combine=" << frames_to_combine.size();
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
        AudioFrame audio_frame_for_mixing;
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

      AudioFrame audio_frame_for_mixing;

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
      EXPECT_THAT(audio_frame_for_mixing.packet_infos_, IsEmpty());
    }
  }
}

TEST(FrameCombiner, CombiningOneFrameShouldNotChangeFrame) {
  FrameCombiner combiner(false);
  for (const int rate : {8000, 10000, 11000, 32000, 44100}) {
    // kMaxConcurrentChannels is 8.
    for (const int number_of_channels : {1, 2, 4, kMaxConcurrentChannels}) {
      SCOPED_TRACE(ProduceDebugText(rate, number_of_channels, 1));

      AudioFrame audio_frame_for_mixing;

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
      EXPECT_THAT(audio_frame_for_mixing.packet_infos_,
                  ElementsAreArray(frame1.packet_infos_));
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
      AudioFrame audio_frame_for_mixing;
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
