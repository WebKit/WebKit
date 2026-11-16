/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/capture_mixer/capture_mixer.h"

#include <cstddef>
#include <tuple>
#include <vector>

#include "test/gtest.h"

namespace webrtc {
namespace {

void PopulateChannels(float amplitude0,
                      float amplitude1,
                      float dc_level0,
                      float dc_level1,
                      std::vector<float>& channel0,
                      std::vector<float>& channel1) {
  for (size_t k = 0; k < channel0.size(); ++k) {
    channel0[k] = amplitude0 * (k % 2 == 0 ? 1 : -1) + dc_level0;
    channel1[k] = amplitude1 * (k % 2 == 1 ? 1 : -1) + dc_level1;
  }
}

bool IsFakeStereoWithSingleChannelContent(
    const std::vector<float>& reference_channel,
    const std::vector<float>& channel0,
    const std::vector<float>& channel1) {
  for (size_t k = 0; k < channel0.size(); ++k) {
    if (reference_channel[k] != channel1[k] && channel0[k] != channel1[k]) {
      return false;
    }
  }
  return true;
}

bool ChannelContainsCorrectContent(const std::vector<float>& reference_channel,
                                   const std::vector<float>& channel) {
  for (size_t k = 0; k < channel.size(); ++k) {
    if (channel[k] != reference_channel[k]) {
      return false;
    }
  }
  return true;
}

bool IsFakeStereoWithAverageChannelContent(
    const std::vector<float>& reference_channel0,
    const std::vector<float>& reference_channel1,
    const std::vector<float>& channel0,
    const std::vector<float>& channel1) {
  for (size_t k = 0; k < channel0.size(); ++k) {
    float average = (reference_channel0[k] + reference_channel1[k]) / 2.0f;
    if (channel0[k] != average || channel1[k] != average) {
      return false;
    }
  }
  return true;
}

bool ChannelContainsAverageContent(const std::vector<float>& reference_channel0,
                                   const std::vector<float>& reference_channel1,
                                   const std::vector<float>& channel) {
  for (size_t k = 0; k < channel.size(); ++k) {
    float average = (reference_channel0[k] + reference_channel1[k]) / 2.0f;
    if (channel[k] != average) {
      return false;
    }
  }
  return true;
}

bool IsTrueStereoWithCorrectContent(
    const std::vector<float>& reference_channel0,
    const std::vector<float>& reference_channel1,
    const std::vector<float>& channel0,
    const std::vector<float>& channel1) {
  for (size_t k = 0; k < channel0.size(); ++k) {
    if (reference_channel0[k] != channel0[k]) {
      return false;
    }
    if (reference_channel1[k] != channel1[k]) {
      return false;
    }
  }
  return true;
}

}  // namespace

class CaptureMixerRemixerTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<int, int, float>> {};

INSTANTIATE_TEST_SUITE_P(
    CaptureMixerMixerTests,
    CaptureMixerRemixerTest,
    ::testing::Combine(::testing::Values(16000, 32000, 48000),
                       ::testing::Values(1, 2),
                       ::testing::Values(0.0f, -5.1f, 10.7f)));

TEST_P(CaptureMixerRemixerTest, InitiallyFakeStereo) {
  const int sample_rate_hz = std::get<0>(GetParam());
  const int num_output_channels = std::get<1>(GetParam());
  const int dc_level = std::get<2>(GetParam());

  const size_t num_samples_per_channel = sample_rate_hz / 100;
  std::vector<float> ch0(num_samples_per_channel);
  std::vector<float> ch1(num_samples_per_channel);
  std::vector<float> ch_original0;
  std::vector<float> ch_original1;

  CaptureMixer mixer(num_samples_per_channel);

  constexpr float kAmplitude0 = 100.0f;
  constexpr float kAmplitude1 = 200.0f;
  constexpr int kNumFramesToProcess = 30;
  for (int k = 0; k < kNumFramesToProcess; ++k) {
    PopulateChannels(kAmplitude0, kAmplitude1, dc_level, dc_level, ch0, ch1);
    ch_original0 = ch0;
    ch_original1 = ch1;
    mixer.Mix(num_output_channels, ch0, ch1);
    if (num_output_channels == 1) {
      EXPECT_TRUE(
          ChannelContainsAverageContent(ch_original0, ch_original1, ch0));
    } else {
      EXPECT_TRUE(IsFakeStereoWithAverageChannelContent(
          ch_original0, ch_original1, ch0, ch1));
    }
  }
}

TEST_P(CaptureMixerRemixerTest, EventuallyTrueStereo) {
  const int sample_rate_hz = std::get<0>(GetParam());
  const int num_output_channels = std::get<1>(GetParam());
  const int dc_level = std::get<2>(GetParam());

  const size_t num_samples_per_channel = sample_rate_hz / 100;
  std::vector<float> ch0(num_samples_per_channel);
  std::vector<float> ch1(num_samples_per_channel);
  std::vector<float> ch_original0;
  std::vector<float> ch_original1;

  CaptureMixer mixer(num_samples_per_channel);

  constexpr float kAmplitude0 = 180.0f;
  constexpr float kAmplitude1 = 200.0f;
  constexpr int kNumFramesToProcess = 300;
  for (int k = 0; k < kNumFramesToProcess; ++k) {
    PopulateChannels(kAmplitude0, kAmplitude1, dc_level, dc_level, ch0, ch1);
    ch_original0 = ch0;
    ch_original1 = ch1;
    mixer.Mix(num_output_channels, ch0, ch1);
  }
  if (num_output_channels == 1) {
    EXPECT_TRUE(ChannelContainsCorrectContent(ch_original0, ch0));
  } else {
    EXPECT_TRUE(
        IsTrueStereoWithCorrectContent(ch_original0, ch_original1, ch0, ch1));
  }
}

class CaptureMixerRemixerImpairedChannelTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<int, int, float, int>> {};

INSTANTIATE_TEST_SUITE_P(
    CaptureMixerMixerTests,
    CaptureMixerRemixerImpairedChannelTest,
    ::testing::Combine(::testing::Values(16000, 32000, 48000),
                       ::testing::Values(1, 2),
                       ::testing::Values(0.0f, -5.1f, 10.7f),
                       ::testing::Values(0, 1)));

TEST_P(CaptureMixerRemixerImpairedChannelTest, LargeChannelPowerImbalance) {
  const int sample_rate_hz = std::get<0>(GetParam());
  const int num_output_channels = std::get<1>(GetParam());
  const int dc_level = std::get<2>(GetParam());
  const int impaired_channel_index = std::get<3>(GetParam());

  const size_t num_samples_per_channel = sample_rate_hz / 100;
  std::vector<float> ch0(num_samples_per_channel);
  std::vector<float> ch1(num_samples_per_channel);
  std::vector<float> ch_original0;
  std::vector<float> ch_original1;

  CaptureMixer mixer(num_samples_per_channel);

  constexpr float kSmallerAmplitude = 190.0f;
  constexpr float kLargerAmplitude = 4000.0f;
  const float kAmplitude0 =
      impaired_channel_index == 0 ? kSmallerAmplitude : kLargerAmplitude;
  const float kAmplitude1 =
      impaired_channel_index == 1 ? kSmallerAmplitude : kLargerAmplitude;
  constexpr int kNumFramesToProcess = 300;
  for (int k = 0; k < kNumFramesToProcess; ++k) {
    PopulateChannels(kAmplitude0, kAmplitude1, dc_level, dc_level, ch0, ch1);
    ch_original0 = ch0;
    ch_original1 = ch1;
    mixer.Mix(num_output_channels, ch0, ch1);
  }
  if (num_output_channels == 1) {
    EXPECT_TRUE(ChannelContainsCorrectContent(
        impaired_channel_index == 0 ? ch_original1 : ch_original0, ch0));
  } else {
    EXPECT_TRUE(IsFakeStereoWithSingleChannelContent(
        impaired_channel_index == 0 ? ch_original1 : ch_original0, ch0, ch1));
  }
}

TEST_P(CaptureMixerRemixerImpairedChannelTest, SmallChannelPowerImbalance) {
  const int sample_rate_hz = std::get<0>(GetParam());
  const int num_output_channels = std::get<1>(GetParam());
  const int dc_level = std::get<2>(GetParam());
  const int impaired_channel_index = std::get<3>(GetParam());

  const size_t num_samples_per_channel = sample_rate_hz / 100;
  std::vector<float> ch0(num_samples_per_channel);
  std::vector<float> ch1(num_samples_per_channel);
  std::vector<float> ch_original0;
  std::vector<float> ch_original1;

  CaptureMixer mixer(num_samples_per_channel);

  constexpr float kSmallerAmplitude = 3000.0f;
  constexpr float kLargerAmplitude = 4000.0f;
  const float kAmplitude0 =
      impaired_channel_index == 0 ? kSmallerAmplitude : kLargerAmplitude;
  const float kAmplitude1 =
      impaired_channel_index == 1 ? kSmallerAmplitude : kLargerAmplitude;
  constexpr int kNumFramesToProcess = 300;
  for (int k = 0; k < kNumFramesToProcess; ++k) {
    PopulateChannels(kAmplitude0, kAmplitude1, dc_level, dc_level, ch0, ch1);
    ch_original0 = ch0;
    ch_original1 = ch1;
    mixer.Mix(num_output_channels, ch0, ch1);
  }
  if (num_output_channels == 1) {
    EXPECT_TRUE(ChannelContainsCorrectContent(ch_original0, ch0));
  } else {
    EXPECT_TRUE(
        IsTrueStereoWithCorrectContent(ch_original0, ch_original1, ch0, ch1));
  }
}

TEST_P(CaptureMixerRemixerImpairedChannelTest, InactiveChannel) {
  const int sample_rate_hz = std::get<0>(GetParam());
  const int num_output_channels = std::get<1>(GetParam());
  const int dc_level = std::get<2>(GetParam());
  const int inactive_channel_index = std::get<3>(GetParam());

  const size_t num_samples_per_channel = sample_rate_hz / 100;
  std::vector<float> ch0(num_samples_per_channel);
  std::vector<float> ch1(num_samples_per_channel);
  std::vector<float> ch_original0;
  std::vector<float> ch_original1;

  CaptureMixer mixer(num_samples_per_channel);

  constexpr float kLargerAmplitude = 500;
  constexpr float kSmallerAmplitude = 40.0f;
  const float kAmplitude0 =
      inactive_channel_index == 1 ? kLargerAmplitude : kSmallerAmplitude;
  const float kAmplitude1 =
      inactive_channel_index == 0 ? kLargerAmplitude : kSmallerAmplitude;
  constexpr int kNumFramesToProcess = 300;
  for (int k = 0; k < kNumFramesToProcess; ++k) {
    PopulateChannels(kAmplitude0, kAmplitude1, dc_level, dc_level, ch0, ch1);
    ch_original0 = ch0;
    ch_original1 = ch1;
    mixer.Mix(num_output_channels, ch0, ch1);
  }

  std::vector<float> ch_average(ch_original0.size());
  for (size_t k = 0; k < ch_original0.size(); ++k) {
    ch_average[k] = (ch_original0[k] + ch_original1[k]) * 0.5f;
  }

  if (num_output_channels == 1) {
    EXPECT_TRUE(ChannelContainsCorrectContent(ch_average, ch0));
  } else {
    EXPECT_TRUE(IsFakeStereoWithSingleChannelContent(ch_average, ch0, ch1));
  }
}

}  // namespace webrtc
