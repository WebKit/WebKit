/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/audio_processing/capture_mixer/remixing_logic.h"

#include <array>

#include "modules/audio_processing/capture_mixer/channel_content_remixer.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr int kNumFramesSinceActivityForInactive = 101;
constexpr int kNumFramesSinceActivityForActive = 99;
constexpr int kAverageEnergyForInactive = 10.0f;
constexpr int kAverageEnergyForActive = 10000.0f;

constexpr int kSilentChannelsModeExitFrames = 1001;
constexpr int kSaturatedChannelsModeExitFrames = 301;
constexpr int kImbalancedChannelsModeExitFrames = 301;

TEST(RemixingLogicTest, InitialState) {
  RemixingLogic logic(480, RemixingLogic::Settings(true, true, true));
  std::array<float, 2> average_energies = {1.0f, 1.0f};
  std::array<int, 2> num_frames_since_activity = {0, 0};
  std::array<float, 2> saturation_factors = {1.0f, 1.0f};
  // The initial mixing is kUseAverage, but since both channels are active and
  // balanced, it will switch to kUseBothChannels.
  EXPECT_EQ(
      logic.SelectStereoChannelMixing(
          average_energies, num_frames_since_activity, saturation_factors),
      StereoMixingVariant::kUseBothChannels);
}

TEST(RemixingLogicTest, InactiveChannels) {
  RemixingLogic logic(480, RemixingLogic::Settings(true, true, true));
  std::array<float, 2> average_energies = {1.0f, 1.0f};
  std::array<int, 2> num_frames_since_activity = {
      kNumFramesSinceActivityForInactive, kNumFramesSinceActivityForInactive};
  std::array<float, 2> saturation_factors = {1.0f, 1.0f};
  EXPECT_EQ(
      logic.SelectStereoChannelMixing(
          average_energies, num_frames_since_activity, saturation_factors),
      StereoMixingVariant::kUseAverage);
}

TEST(RemixingLogicTest, BalancedActiveNotSaturated) {
  RemixingLogic logic(480, RemixingLogic::Settings(true, true, true));
  std::array<float, 2> average_energies = {1.0f, 1.0f};
  std::array<int, 2> num_frames_since_activity = {
      kNumFramesSinceActivityForActive, kNumFramesSinceActivityForActive};
  std::array<float, 2> saturation_factors = {1.0f, 1.0f};

  EXPECT_EQ(
      logic.SelectStereoChannelMixing(
          average_energies, num_frames_since_activity, saturation_factors),
      StereoMixingVariant::kUseBothChannels);
}

class RemixingLogicParamTest : public ::testing::TestWithParam<int> {
 protected:
  int AffectedChannel() const { return GetParam(); }
  int OtherChannel() const { return 1 - GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(ChannelPermutations,
                         RemixingLogicParamTest,
                         ::testing::Values(0, 1));

TEST_P(RemixingLogicParamTest, OneChannelSilent) {
  RemixingLogic logic(480, RemixingLogic::Settings(true, true, true));
  std::array<float, 2> average_energies;
  average_energies[AffectedChannel()] = kAverageEnergyForInactive;
  average_energies[OtherChannel()] = kAverageEnergyForActive;
  std::array<int, 2> num_frames_since_activity;
  num_frames_since_activity[AffectedChannel()] =
      kNumFramesSinceActivityForInactive;
  num_frames_since_activity[OtherChannel()] = kNumFramesSinceActivityForActive;
  std::array<float, 2> saturation_factors = {1.0f, 1.0f};

  // Enters kSilentChannel mode, uses kUseAverage.
  EXPECT_EQ(
      logic.SelectStereoChannelMixing(
          average_energies, num_frames_since_activity, saturation_factors),
      StereoMixingVariant::kUseAverage);

  // Stays in kSilentChannel mode.
  num_frames_since_activity[AffectedChannel()] =
      kNumFramesSinceActivityForActive;
  average_energies[AffectedChannel()] = kAverageEnergyForActive;
  for (int i = 0; i < kSilentChannelsModeExitFrames - 1; ++i) {
    EXPECT_EQ(
        logic.SelectStereoChannelMixing(
            average_energies, num_frames_since_activity, saturation_factors),
        StereoMixingVariant::kUseAverage);
  }

  // Exits kSilentChannel mode.
  EXPECT_EQ(
      logic.SelectStereoChannelMixing(
          average_energies, num_frames_since_activity, saturation_factors),
      StereoMixingVariant::kUseBothChannels);
}

TEST_P(RemixingLogicParamTest, LargelyImbalancedChannels) {
  RemixingLogic logic(480, RemixingLogic::Settings(true, true, true));
  std::array<float, 2> average_energies;
  average_energies[AffectedChannel()] = 51.0f;
  average_energies[OtherChannel()] = 1.0f;
  std::array<int, 2> num_frames_since_activity = {
      kNumFramesSinceActivityForActive, kNumFramesSinceActivityForActive};
  std::array<float, 2> saturation_factors = {1.0f, 1.0f};

  const auto expected_variant = AffectedChannel() == 0
                                    ? StereoMixingVariant::kUseChannel0
                                    : StereoMixingVariant::kUseChannel1;

  // Enters kImbalancedChannels mode, uses louder channel.
  EXPECT_EQ(
      logic.SelectStereoChannelMixing(
          average_energies, num_frames_since_activity, saturation_factors),
      expected_variant);

  // Stays in kImbalancedChannels mode.
  average_energies[AffectedChannel()] = 1.0f;
  for (int i = 0; i < kImbalancedChannelsModeExitFrames - 1; ++i) {
    EXPECT_EQ(
        logic.SelectStereoChannelMixing(
            average_energies, num_frames_since_activity, saturation_factors),
        expected_variant);
  }

  // Exits kImbalancedChannels mode.
  EXPECT_EQ(
      logic.SelectStereoChannelMixing(
          average_energies, num_frames_since_activity, saturation_factors),
      StereoMixingVariant::kUseBothChannels);
}

TEST_P(RemixingLogicParamTest, ModeratelyImbalancedAndSaturated) {
  RemixingLogic logic(480, RemixingLogic::Settings(true, true, true));
  std::array<float, 2> average_energies;
  average_energies[AffectedChannel()] = 5000.0f;
  average_energies[OtherChannel()] = 1000.0f;
  std::array<int, 2> num_frames_since_activity = {
      kNumFramesSinceActivityForActive, kNumFramesSinceActivityForActive};
  std::array<float, 2> saturation_factors;
  saturation_factors[AffectedChannel()] = 0.81f;
  saturation_factors[OtherChannel()] = 0.09f;

  const auto expected_variant = AffectedChannel() == 0
                                    ? StereoMixingVariant::kUseChannel1
                                    : StereoMixingVariant::kUseChannel0;

  // Enters kSaturatedChannel mode, uses less saturated channel.
  EXPECT_EQ(
      logic.SelectStereoChannelMixing(
          average_energies, num_frames_since_activity, saturation_factors),
      expected_variant);

  // Stays in kSaturatedChannel mode.
  average_energies = {1000.0f, 1000.0f};
  saturation_factors = {0.09f, 0.09f};
  for (int i = 0; i < kSaturatedChannelsModeExitFrames - 1; ++i) {
    EXPECT_EQ(
        logic.SelectStereoChannelMixing(
            average_energies, num_frames_since_activity, saturation_factors),
        expected_variant);
  }

  // Exits kSaturatedChannel mode.
  EXPECT_EQ(
      logic.SelectStereoChannelMixing(
          average_energies, num_frames_since_activity, saturation_factors),
      StereoMixingVariant::kUseBothChannels);
}

TEST_P(RemixingLogicParamTest, PrecedenceSilentOverImbalanced) {
  RemixingLogic logic(480, RemixingLogic::Settings(true, true, true));
  std::array<float, 2> average_energies;
  average_energies[AffectedChannel()] = kAverageEnergyForInactive;
  average_energies[OtherChannel()] = kAverageEnergyForActive;
  std::array<int, 2> num_frames_since_activity;
  num_frames_since_activity[AffectedChannel()] =
      kNumFramesSinceActivityForInactive;
  num_frames_since_activity[OtherChannel()] = kNumFramesSinceActivityForActive;
  std::array<float, 2> saturation_factors = {0.09f, 0.09f};

  EXPECT_EQ(
      logic.SelectStereoChannelMixing(
          average_energies, num_frames_since_activity, saturation_factors),
      StereoMixingVariant::kUseAverage);
}

}  // namespace
}  // namespace webrtc
