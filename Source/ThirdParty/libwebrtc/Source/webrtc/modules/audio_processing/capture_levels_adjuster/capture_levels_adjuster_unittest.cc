/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/capture_levels_adjuster/capture_levels_adjuster.h"

#include <algorithm>
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

float ComputeExpectedSignalGainAfterApplyPreLevelAdjustment(
    bool emulated_analog_mic_gain_enabled,
    int emulated_analog_mic_gain_level,
    float pre_gain) {
  if (!emulated_analog_mic_gain_enabled) {
    return pre_gain;
  }
  return pre_gain * std::min(emulated_analog_mic_gain_level, 255) / 255.f;
}

float ComputeExpectedSignalGainAfterApplyPostLevelAdjustment(
    bool emulated_analog_mic_gain_enabled,
    int emulated_analog_mic_gain_level,
    float pre_gain,
    float post_gain) {
  return post_gain * ComputeExpectedSignalGainAfterApplyPreLevelAdjustment(
                         emulated_analog_mic_gain_enabled,
                         emulated_analog_mic_gain_level, pre_gain);
}

constexpr int kNumFramesToProcess = 10;

class CaptureLevelsAdjusterTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<
          std::tuple<int, int, bool, int, float, float>> {
 protected:
  int sample_rate_hz() const { return std::get<0>(GetParam()); }
  int num_channels() const { return std::get<1>(GetParam()); }
  bool emulated_analog_mic_gain_enabled() const {
    return std::get<2>(GetParam());
  }
  int emulated_analog_mic_gain_level() const { return std::get<3>(GetParam()); }
  float pre_gain() const { return std::get<4>(GetParam()); }
  float post_gain() const { return std::get<5>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(
    CaptureLevelsAdjusterTestSuite,
    CaptureLevelsAdjusterTest,
    ::testing::Combine(::testing::Values(16000, 32000, 48000),
                       ::testing::Values(1, 2, 4),
                       ::testing::Values(false, true),
                       ::testing::Values(21, 255),
                       ::testing::Values(0.1f, 1.f, 4.f),
                       ::testing::Values(0.1f, 1.f, 4.f)));

TEST_P(CaptureLevelsAdjusterTest, InitialGainIsInstantlyAchieved) {
  CaptureLevelsAdjuster adjuster(emulated_analog_mic_gain_enabled(),
                                 emulated_analog_mic_gain_level(), pre_gain(),
                                 post_gain());

  AudioBuffer audio_buffer(sample_rate_hz(), num_channels(), sample_rate_hz(),
                           num_channels(), sample_rate_hz(), num_channels());

  const float expected_signal_gain_after_pre_gain =
      ComputeExpectedSignalGainAfterApplyPreLevelAdjustment(
          emulated_analog_mic_gain_enabled(), emulated_analog_mic_gain_level(),
          pre_gain());
  const float expected_signal_gain_after_post_level_adjustment =
      ComputeExpectedSignalGainAfterApplyPostLevelAdjustment(
          emulated_analog_mic_gain_enabled(), emulated_analog_mic_gain_level(),
          pre_gain(), post_gain());

  for (int frame = 0; frame < kNumFramesToProcess; ++frame) {
    PopulateBuffer(audio_buffer);
    adjuster.ApplyPreLevelAdjustment(audio_buffer);
    EXPECT_FLOAT_EQ(adjuster.GetPreAdjustmentGain(),
                    expected_signal_gain_after_pre_gain);

    for (int ch = 0; ch < num_channels(); ++ch) {
      for (size_t i = 0; i < audio_buffer.num_frames(); ++i) {
        EXPECT_FLOAT_EQ(
            audio_buffer.channels_const()[ch][i],
            expected_signal_gain_after_pre_gain * SampleValueForChannel(ch));
      }
    }
    adjuster.ApplyPostLevelAdjustment(audio_buffer);
    for (int ch = 0; ch < num_channels(); ++ch) {
      for (size_t i = 0; i < audio_buffer.num_frames(); ++i) {
        EXPECT_FLOAT_EQ(audio_buffer.channels_const()[ch][i],
                        expected_signal_gain_after_post_level_adjustment *
                            SampleValueForChannel(ch));
      }
    }
  }
}

TEST_P(CaptureLevelsAdjusterTest, NewGainsAreAchieved) {
  const int lower_emulated_analog_mic_gain_level =
      emulated_analog_mic_gain_level();
  const float lower_pre_gain = pre_gain();
  const float lower_post_gain = post_gain();
  const int higher_emulated_analog_mic_gain_level =
      std::min(lower_emulated_analog_mic_gain_level * 2, 255);
  const float higher_pre_gain = lower_pre_gain * 2.f;
  const float higher_post_gain = lower_post_gain * 2.f;

  CaptureLevelsAdjuster adjuster(emulated_analog_mic_gain_enabled(),
                                 lower_emulated_analog_mic_gain_level,
                                 lower_pre_gain, lower_post_gain);

  AudioBuffer audio_buffer(sample_rate_hz(), num_channels(), sample_rate_hz(),
                           num_channels(), sample_rate_hz(), num_channels());

  const float expected_signal_gain_after_pre_gain =
      ComputeExpectedSignalGainAfterApplyPreLevelAdjustment(
          emulated_analog_mic_gain_enabled(),
          higher_emulated_analog_mic_gain_level, higher_pre_gain);
  const float expected_signal_gain_after_post_level_adjustment =
      ComputeExpectedSignalGainAfterApplyPostLevelAdjustment(
          emulated_analog_mic_gain_enabled(),
          higher_emulated_analog_mic_gain_level, higher_pre_gain,
          higher_post_gain);

  adjuster.SetPreGain(higher_pre_gain);
  adjuster.SetPostGain(higher_post_gain);
  adjuster.SetAnalogMicGainLevel(higher_emulated_analog_mic_gain_level);

  PopulateBuffer(audio_buffer);
  adjuster.ApplyPreLevelAdjustment(audio_buffer);
  adjuster.ApplyPostLevelAdjustment(audio_buffer);
  EXPECT_EQ(adjuster.GetAnalogMicGainLevel(),
            higher_emulated_analog_mic_gain_level);

  for (int frame = 1; frame < kNumFramesToProcess; ++frame) {
    PopulateBuffer(audio_buffer);
    adjuster.ApplyPreLevelAdjustment(audio_buffer);
    EXPECT_FLOAT_EQ(adjuster.GetPreAdjustmentGain(),
                    expected_signal_gain_after_pre_gain);
    for (int ch = 0; ch < num_channels(); ++ch) {
      for (size_t i = 0; i < audio_buffer.num_frames(); ++i) {
        EXPECT_FLOAT_EQ(
            audio_buffer.channels_const()[ch][i],
            expected_signal_gain_after_pre_gain * SampleValueForChannel(ch));
      }
    }

    adjuster.ApplyPostLevelAdjustment(audio_buffer);
    for (int ch = 0; ch < num_channels(); ++ch) {
      for (size_t i = 0; i < audio_buffer.num_frames(); ++i) {
        EXPECT_FLOAT_EQ(audio_buffer.channels_const()[ch][i],
                        expected_signal_gain_after_post_level_adjustment *
                            SampleValueForChannel(ch));
      }
    }

    EXPECT_EQ(adjuster.GetAnalogMicGainLevel(),
              higher_emulated_analog_mic_gain_level);
  }
}

}  // namespace
}  // namespace webrtc
