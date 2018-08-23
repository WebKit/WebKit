/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/saturation_protector.h"

#include <algorithm>

#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/gunit.h"

namespace webrtc {
namespace {
float RunOnConstantLevel(int num_iterations,
                         VadWithLevel::LevelAndProbability vad_data,
                         float estimated_level_dbfs,
                         SaturationProtector* saturation_protector) {
  float last_margin = saturation_protector->LastMargin();
  float max_difference = 0.f;
  for (int i = 0; i < num_iterations; ++i) {
    saturation_protector->UpdateMargin(vad_data, estimated_level_dbfs);
    const float new_margin = saturation_protector->LastMargin();
    max_difference =
        std::max(max_difference, std::abs(new_margin - last_margin));
    last_margin = new_margin;
    saturation_protector->DebugDumpEstimate();
  }
  return max_difference;
}
}  // namespace

TEST(AutomaticGainController2SaturationProtector, ProtectorShouldNotCrash) {
  ApmDataDumper apm_data_dumper(0);
  SaturationProtector saturation_protector(&apm_data_dumper);
  VadWithLevel::LevelAndProbability vad_data(1.f, -20.f, -10.f);

  saturation_protector.UpdateMargin(vad_data, -20.f);
  static_cast<void>(saturation_protector.LastMargin());
  saturation_protector.DebugDumpEstimate();
}

// Check that the estimate converges to the ratio between peaks and
// level estimator values after a while.
TEST(AutomaticGainController2SaturationProtector,
     ProtectorEstimatesCrestRatio) {
  ApmDataDumper apm_data_dumper(0);
  SaturationProtector saturation_protector(&apm_data_dumper);

  constexpr float kPeakLevel = -20.f;
  constexpr float kCrestFactor = kInitialSaturationMarginDb + 1.f;
  constexpr float kSpeechLevel = kPeakLevel - kCrestFactor;
  const float kMaxDifference =
      0.5 * std::abs(kInitialSaturationMarginDb - kCrestFactor);

  static_cast<void>(RunOnConstantLevel(
      2000, VadWithLevel::LevelAndProbability(1.f, -90.f, kPeakLevel),
      kSpeechLevel, &saturation_protector));

  EXPECT_NEAR(saturation_protector.LastMargin(), kCrestFactor, kMaxDifference);
}

TEST(AutomaticGainController2SaturationProtector, ProtectorChangesSlowly) {
  ApmDataDumper apm_data_dumper(0);
  SaturationProtector saturation_protector(&apm_data_dumper);

  constexpr float kPeakLevel = -20.f;
  constexpr float kCrestFactor = kInitialSaturationMarginDb - 5.f;
  constexpr float kOtherCrestFactor = kInitialSaturationMarginDb;
  constexpr float kSpeechLevel = kPeakLevel - kCrestFactor;
  constexpr float kOtherSpeechLevel = kPeakLevel - kOtherCrestFactor;

  constexpr int kNumIterations = 1000;
  float max_difference = RunOnConstantLevel(
      kNumIterations, VadWithLevel::LevelAndProbability(1.f, -90.f, kPeakLevel),
      kSpeechLevel, &saturation_protector);

  max_difference =
      std::max(RunOnConstantLevel(
                   kNumIterations,
                   VadWithLevel::LevelAndProbability(1.f, -90.f, kPeakLevel),
                   kOtherSpeechLevel, &saturation_protector),
               max_difference);

  constexpr float kMaxChangeSpeedDbPerSecond = 0.5;  // 1 db / 2 seconds.

  EXPECT_LE(max_difference,
            kMaxChangeSpeedDbPerSecond / 1000 * kFrameDurationMs);
}

TEST(AutomaticGainController2SaturationProtector,
     ProtectorAdaptsToDelayedChanges) {
  ApmDataDumper apm_data_dumper(0);
  SaturationProtector saturation_protector(&apm_data_dumper);

  constexpr int kDelayIterations = kFullBufferSizeMs / kFrameDurationMs;
  constexpr float kInitialSpeechLevelDbfs = -30;
  constexpr float kLaterSpeechLevelDbfs = -15;

  // First run on initial level.
  float max_difference = RunOnConstantLevel(
      kDelayIterations,
      VadWithLevel::LevelAndProbability(
          1.f, -90.f, kInitialSpeechLevelDbfs + kInitialSaturationMarginDb),
      kInitialSpeechLevelDbfs, &saturation_protector);

  // Then peak changes, but not RMS.
  max_difference = std::max(
      RunOnConstantLevel(
          kDelayIterations,
          VadWithLevel::LevelAndProbability(
              1.f, -90.f, kLaterSpeechLevelDbfs + kInitialSaturationMarginDb),
          kInitialSpeechLevelDbfs, &saturation_protector),
      max_difference);

  // Then both change.
  max_difference = std::max(
      RunOnConstantLevel(
          kDelayIterations,
          VadWithLevel::LevelAndProbability(
              1.f, -90.f, kLaterSpeechLevelDbfs + kInitialSaturationMarginDb),
          kLaterSpeechLevelDbfs, &saturation_protector),
      max_difference);

  // The saturation protector expects that the RMS changes roughly
  // 'kFullBufferSizeMs' after peaks change. This is to account for
  // delay introduces by the level estimator. Therefore, the input
  // above is 'normal' and 'expected', and shouldn't influence the
  // margin by much.

  const float total_difference =
      std::abs(saturation_protector.LastMargin() - kInitialSaturationMarginDb);

  EXPECT_LE(total_difference, 0.05f);
  EXPECT_LE(max_difference, 0.01f);
}

}  // namespace webrtc
