
/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/capture_mixer/channel_content_remixer.h"

#include <algorithm>
#include <cstddef>
#include <tuple>
#include <vector>

#include "api/array_view.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr float kSampleValue0 = 100.0f;
constexpr float kSampleValue1 = 200.0f;
constexpr float kSampleValueAverage = (kSampleValue0 + kSampleValue1) / 2.0f;

void PopulateChannels(std::vector<float>& channel0,
                      std::vector<float>& channel1) {
  std::fill(channel0.begin(), channel0.end(), kSampleValue0);
  std::fill(channel1.begin(), channel1.end(), kSampleValue1);
}

void VerifyCrossFade(float value_begin,
                     float value_end,
                     ArrayView<const float> channel_data) {
  const float one_by_num_samples_per_channel = 1.0f / channel_data.size();
  for (size_t k = 0; k < channel_data.size(); ++k) {
    const float expected_value =
        value_begin * (1.0f - k * one_by_num_samples_per_channel) +
        value_end * k * one_by_num_samples_per_channel;
    EXPECT_NEAR(channel_data[k], expected_value, 1e-3);
  }
}

void VerifyConstantValue(float expected_value,
                         ArrayView<const float> channel_data) {
  for (size_t k = 0; k < channel_data.size(); ++k) {
    EXPECT_NEAR(channel_data[k], expected_value, 1e-3);
  }
}

bool Remix(int num_output_channels,
           StereoMixingVariant mixing_variant,
           ChannelContentRemixer& mixer,
           std::vector<float>& channel0,
           std::vector<float>& channel1) {
  PopulateChannels(channel0, channel1);
  return mixer.Mix(num_output_channels, mixing_variant, channel0, channel1);
}

}  // namespace

class ChannelContentRemixerAllCombinationsTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<int,
                                                      int,
                                                      int,
                                                      StereoMixingVariant,
                                                      StereoMixingVariant,
                                                      StereoMixingVariant>> {};

INSTANTIATE_TEST_SUITE_P(
    ChannelContentMixerTests,
    ChannelContentRemixerAllCombinationsTest,
    ::testing::Combine(::testing::Values(16000, 32000, 48000),
                       ::testing::Values(1, 2),
                       ::testing::Values(1, 4, 7),
                       ::testing::Values(StereoMixingVariant::kUseBothChannels,
                                         StereoMixingVariant::kUseChannel0,
                                         StereoMixingVariant::kUseChannel1,
                                         StereoMixingVariant::kUseAverage),
                       ::testing::Values(StereoMixingVariant::kUseBothChannels,
                                         StereoMixingVariant::kUseChannel0,
                                         StereoMixingVariant::kUseChannel1,
                                         StereoMixingVariant::kUseAverage),
                       ::testing::Values(StereoMixingVariant::kUseBothChannels,
                                         StereoMixingVariant::kUseChannel0,
                                         StereoMixingVariant::kUseChannel1,
                                         StereoMixingVariant::kUseAverage)));

TEST_P(ChannelContentRemixerAllCombinationsTest, MixingMultiplexing) {
  const int sample_rate_hz = std::get<0>(GetParam());
  const int num_output_channels = std::get<1>(GetParam());
  const int num_frames_for_crossfade = std::get<2>(GetParam());
  const StereoMixingVariant mixing1 = std::get<3>(GetParam());
  const StereoMixingVariant mixing2 = std::get<4>(GetParam());
  const StereoMixingVariant mixing3 = std::get<5>(GetParam());

  const size_t num_samples_per_channel = sample_rate_hz / 100;

  std::vector<float> ch0(num_samples_per_channel);
  std::vector<float> ch1(num_samples_per_channel);

  ChannelContentRemixer mixer(num_samples_per_channel,
                              num_frames_for_crossfade);

  constexpr int kNumFramesToProcess = 10;
  ASSERT_GT(kNumFramesToProcess, num_frames_for_crossfade);
  bool crossfade_completed = false;
  for (int k = 0; k < kNumFramesToProcess; ++k) {
    crossfade_completed = Remix(num_output_channels, mixing1, mixer, ch0, ch1);
  }
  EXPECT_TRUE(crossfade_completed);
  for (int k = 0; k < kNumFramesToProcess; ++k) {
    crossfade_completed = Remix(num_output_channels, mixing2, mixer, ch0, ch1);
  }
  EXPECT_TRUE(crossfade_completed);
  for (int k = 0; k < kNumFramesToProcess; ++k) {
    crossfade_completed = Remix(num_output_channels, mixing3, mixer, ch0, ch1);
  }
  EXPECT_TRUE(crossfade_completed);
}

class ChannelContentRemixerParametrizedTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<int, int>> {};

INSTANTIATE_TEST_SUITE_P(
    ChannelContentMixerTests,
    ChannelContentRemixerParametrizedTest,
    ::testing::Combine(::testing::Values(16000, 32000, 48000),
                       ::testing::Values(1, 2)));

TEST_P(ChannelContentRemixerParametrizedTest, InitialState) {
  const int sample_rate_hz = std::get<0>(GetParam());
  const int num_output_channels = std::get<1>(GetParam());
  const size_t num_samples_per_channel = sample_rate_hz / 100;
  std::vector<float> ch0(num_samples_per_channel);
  std::vector<float> ch1(num_samples_per_channel);

  ChannelContentRemixer mixer(num_samples_per_channel,
                              /*num_frames_for_crossfade=*/1);

  // Initial state: kUseAverage
  // kUseAverage -> kUseAverage
  Remix(num_output_channels, StereoMixingVariant::kUseAverage, mixer, ch0, ch1);

  VerifyConstantValue(kSampleValueAverage, ch0);
  if (num_output_channels == 2) {
    VerifyConstantValue(kSampleValueAverage, ch1);
  }
}

TEST_P(ChannelContentRemixerParametrizedTest, CrossfadeDuration) {
  const int sample_rate_hz = std::get<0>(GetParam());
  const int num_output_channels = std::get<1>(GetParam());
  const size_t num_samples_per_channel = sample_rate_hz / 100;
  std::vector<float> ch0(num_samples_per_channel);
  std::vector<float> ch1(num_samples_per_channel);

  for (int num_frames_for_crossfade = 1; num_frames_for_crossfade < 10;
       ++num_frames_for_crossfade) {
    ChannelContentRemixer mixer(num_samples_per_channel,
                                num_frames_for_crossfade);

    // Initial state: kUseAverage
    // kUseAverage -> kUseBothChannels
    for (int j = 0; j < num_frames_for_crossfade - 1; ++j) {
      EXPECT_FALSE(Remix(num_output_channels,
                         StereoMixingVariant::kUseBothChannels, mixer, ch0,
                         ch1));
    }
    EXPECT_TRUE(Remix(num_output_channels,
                      StereoMixingVariant::kUseBothChannels, mixer, ch0, ch1));

    EXPECT_TRUE(Remix(num_output_channels,
                      StereoMixingVariant::kUseBothChannels, mixer, ch0, ch1));
    VerifyConstantValue(kSampleValue0, ch0);
    if (num_output_channels == 2) {
      VerifyConstantValue(kSampleValue1, ch1);
    }
  }
}

TEST_P(ChannelContentRemixerParametrizedTest, StartingWithAverageMixing) {
  const int sample_rate_hz = std::get<0>(GetParam());
  const int num_output_channels = std::get<1>(GetParam());
  const size_t num_samples_per_channel = sample_rate_hz / 100;

  std::vector<float> ch0(num_samples_per_channel);
  std::vector<float> ch1(num_samples_per_channel);

  ChannelContentRemixer mixer(num_samples_per_channel,
                              /*num_frames_for_crossfade=*/1);

  // Initial state: kUseAverage
  // kUseAverage -> kUseAverage
  // Note that the initial mode is to use the average.
  Remix(num_output_channels, StereoMixingVariant::kUseAverage, mixer, ch0, ch1);

  VerifyConstantValue(kSampleValueAverage, ch0);
  if (num_output_channels == 2) {
    VerifyConstantValue(kSampleValueAverage, ch1);
  }

  // kUseAverage -> kUseChannel0
  Remix(num_output_channels, StereoMixingVariant::kUseAverage, mixer, ch0, ch1);
  Remix(num_output_channels, StereoMixingVariant::kUseChannel0, mixer, ch0,
        ch1);
  VerifyCrossFade(kSampleValueAverage, kSampleValue0, ch0);
  if (num_output_channels == 2) {
    VerifyCrossFade(kSampleValueAverage, kSampleValue0, ch1);
  }

  // kUseAverage -> kUseChannel1
  Remix(num_output_channels, StereoMixingVariant::kUseAverage, mixer, ch0, ch1);
  Remix(num_output_channels, StereoMixingVariant::kUseChannel1, mixer, ch0,
        ch1);
  VerifyCrossFade(kSampleValueAverage, kSampleValue1, ch0);
  if (num_output_channels == 2) {
    VerifyCrossFade(kSampleValueAverage, kSampleValue1, ch1);
  }

  // kUseAverage -> kUseBothChannels
  Remix(num_output_channels, StereoMixingVariant::kUseAverage, mixer, ch0, ch1);
  Remix(num_output_channels, StereoMixingVariant::kUseBothChannels, mixer, ch0,
        ch1);
  VerifyCrossFade(kSampleValueAverage, kSampleValue0, ch0);
  if (num_output_channels == 2) {
    VerifyCrossFade(kSampleValueAverage, kSampleValue1, ch1);
  }
}

TEST_P(ChannelContentRemixerParametrizedTest, StartingWithChannel0Mixing) {
  const int sample_rate_hz = std::get<0>(GetParam());
  const int num_output_channels = std::get<1>(GetParam());
  const size_t num_samples_per_channel = sample_rate_hz / 100;

  std::vector<float> ch0(num_samples_per_channel);
  std::vector<float> ch1(num_samples_per_channel);

  ChannelContentRemixer mixer(num_samples_per_channel,
                              /*num_frames_for_crossfade=*/1);

  // Initial state: kUseAverage
  // kUseChannel0 -> kUseAverage
  Remix(num_output_channels, StereoMixingVariant::kUseChannel0, mixer, ch0,
        ch1);
  Remix(num_output_channels, StereoMixingVariant::kUseAverage, mixer, ch0, ch1);

  VerifyCrossFade(kSampleValue0, kSampleValueAverage, ch0);
  if (num_output_channels == 2) {
    VerifyCrossFade(kSampleValue0, kSampleValueAverage, ch1);
  }
  // kUseChannel0 -> kUseChannel0
  Remix(num_output_channels, StereoMixingVariant::kUseChannel0, mixer, ch0,
        ch1);
  Remix(num_output_channels, StereoMixingVariant::kUseChannel0, mixer, ch0,
        ch1);

  VerifyConstantValue(kSampleValue0, ch0);
  if (num_output_channels == 2) {
    VerifyConstantValue(kSampleValue0, ch1);
  }

  // kUseChannel0 -> kUseChannel1
  Remix(num_output_channels, StereoMixingVariant::kUseChannel0, mixer, ch0,
        ch1);
  Remix(num_output_channels, StereoMixingVariant::kUseChannel1, mixer, ch0,
        ch1);
  VerifyCrossFade(kSampleValue0, kSampleValue1, ch0);
  if (num_output_channels == 2) {
    VerifyCrossFade(kSampleValue0, kSampleValue1, ch1);
  }

  // kUseChannel0 -> kUseBothChannels
  Remix(num_output_channels, StereoMixingVariant::kUseChannel0, mixer, ch0,
        ch1);
  Remix(num_output_channels, StereoMixingVariant::kUseBothChannels, mixer, ch0,
        ch1);

  VerifyConstantValue(kSampleValue0, ch0);
  if (num_output_channels == 2) {
    VerifyCrossFade(kSampleValue0, kSampleValue1, ch1);
  }
}

TEST_P(ChannelContentRemixerParametrizedTest, StartingWithChannel1Mixing) {
  const int sample_rate_hz = std::get<0>(GetParam());
  const int num_output_channels = std::get<1>(GetParam());
  const size_t num_samples_per_channel = sample_rate_hz / 100;

  std::vector<float> ch0(num_samples_per_channel);
  std::vector<float> ch1(num_samples_per_channel);

  ChannelContentRemixer mixer(num_samples_per_channel,
                              /*num_frames_for_crossfade=*/1);

  // Initial state: kUseAverage
  // kUseChannel1 -> kUseAverage
  Remix(num_output_channels, StereoMixingVariant::kUseChannel1, mixer, ch0,
        ch1);
  Remix(num_output_channels, StereoMixingVariant::kUseAverage, mixer, ch0, ch1);

  VerifyCrossFade(kSampleValue1, kSampleValueAverage, ch0);
  if (num_output_channels == 2) {
    VerifyCrossFade(kSampleValue1, kSampleValueAverage, ch1);
  }

  // kUseChannel1 -> kUseChannel0
  Remix(num_output_channels, StereoMixingVariant::kUseChannel1, mixer, ch0,
        ch1);
  Remix(num_output_channels, StereoMixingVariant::kUseChannel0, mixer, ch0,
        ch1);

  VerifyCrossFade(kSampleValue1, kSampleValue0, ch0);
  if (num_output_channels == 2) {
    VerifyCrossFade(kSampleValue1, kSampleValue0, ch1);
  }

  // kUseChannel1 -> kUseChannel1
  Remix(num_output_channels, StereoMixingVariant::kUseChannel1, mixer, ch0,
        ch1);
  Remix(num_output_channels, StereoMixingVariant::kUseChannel1, mixer, ch0,
        ch1);
  VerifyConstantValue(kSampleValue1, ch1);
  if (num_output_channels == 2) {
    VerifyConstantValue(kSampleValue1, ch1);
  }

  // kUseChannel1 -> kUseBothChannels
  Remix(num_output_channels, StereoMixingVariant::kUseChannel1, mixer, ch0,
        ch1);
  Remix(num_output_channels, StereoMixingVariant::kUseBothChannels, mixer, ch0,
        ch1);

  VerifyCrossFade(kSampleValue1, kSampleValue0, ch0);
  if (num_output_channels == 2) {
    VerifyConstantValue(kSampleValue1, ch1);
  }
}

TEST_P(ChannelContentRemixerParametrizedTest, StartingWithBothChannelsMixing) {
  const int sample_rate_hz = std::get<0>(GetParam());
  const int num_output_channels = std::get<1>(GetParam());
  const size_t num_samples_per_channel = sample_rate_hz / 100;

  std::vector<float> ch0(num_samples_per_channel);
  std::vector<float> ch1(num_samples_per_channel);

  ChannelContentRemixer mixer(num_samples_per_channel,
                              /*num_frames_for_crossfade=*/1);

  // Initial state: kUseAverage
  // kUseBothChannels -> kUseAverage
  Remix(num_output_channels, StereoMixingVariant::kUseBothChannels, mixer, ch0,
        ch1);
  Remix(num_output_channels, StereoMixingVariant::kUseAverage, mixer, ch0, ch1);

  VerifyCrossFade(kSampleValue0, kSampleValueAverage, ch0);
  if (num_output_channels == 2) {
    VerifyCrossFade(kSampleValue1, kSampleValueAverage, ch1);
  }

  // kUseBothChannels -> kUseChannel0
  Remix(num_output_channels, StereoMixingVariant::kUseBothChannels, mixer, ch0,
        ch1);
  Remix(num_output_channels, StereoMixingVariant::kUseChannel0, mixer, ch0,
        ch1);

  VerifyConstantValue(kSampleValue0, ch0);
  if (num_output_channels == 2) {
    VerifyCrossFade(kSampleValue1, kSampleValue0, ch1);
  }

  // kUseBothChannels -> kUseChannel1
  Remix(num_output_channels, StereoMixingVariant::kUseBothChannels, mixer, ch0,
        ch1);
  Remix(num_output_channels, StereoMixingVariant::kUseChannel1, mixer, ch0,
        ch1);

  VerifyCrossFade(kSampleValue0, kSampleValue1, ch0);
  if (num_output_channels == 2) {
    VerifyConstantValue(kSampleValue1, ch1);
  }

  // kUseBothChannels -> kUseBothChannels
  Remix(num_output_channels, StereoMixingVariant::kUseBothChannels, mixer, ch0,
        ch1);
  Remix(num_output_channels, StereoMixingVariant::kUseBothChannels, mixer, ch0,
        ch1);

  VerifyConstantValue(kSampleValue0, ch0);
  if (num_output_channels == 2) {
    VerifyConstantValue(kSampleValue1, ch1);
  }
}

}  // namespace webrtc
