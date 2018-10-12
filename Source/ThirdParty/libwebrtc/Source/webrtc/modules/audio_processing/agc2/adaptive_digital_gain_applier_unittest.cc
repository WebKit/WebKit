/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/adaptive_digital_gain_applier.h"

#include <algorithm>

#include "common_audio/include/audio_util.h"
#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/agc2/vector_float_frame.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/gunit.h"

namespace webrtc {
namespace {
// Constants used in place of estimated noise levels.
constexpr float kNoNoiseDbfs = -90.f;
constexpr float kWithNoiseDbfs = -20.f;
constexpr VadWithLevel::LevelAndProbability kVadSpeech(1.f, -20.f, 0.f);

// Runs gain applier and returns the applied gain in linear scale.
float RunOnConstantLevel(int num_iterations,
                         VadWithLevel::LevelAndProbability vad_data,
                         float input_level_dbfs,
                         AdaptiveDigitalGainApplier* gain_applier) {
  float gain_linear = 0.f;

  for (int i = 0; i < num_iterations; ++i) {
    VectorFloatFrame fake_audio(1, 1, 1.f);
    SignalWithLevels signal_with_levels(fake_audio.float_frame_view());
    signal_with_levels.input_level_dbfs = input_level_dbfs;
    signal_with_levels.input_noise_level_dbfs = kNoNoiseDbfs;
    signal_with_levels.vad_result = vad_data;
    signal_with_levels.limiter_audio_level_dbfs = -2.f;
    signal_with_levels.estimate_is_confident = true;
    gain_applier->Process(signal_with_levels);
    gain_linear = fake_audio.float_frame_view().channel(0)[0];
  }
  return gain_linear;
}

// Returns 'SignalWithLevels' for typical GainApplier behavior. Voice on, no
// noise, low limiter, confident level.
SignalWithLevels TestSignalWithLevel(AudioFrameView<float> float_frame) {
  SignalWithLevels result(float_frame);
  result.input_level_dbfs = -1;
  result.input_noise_level_dbfs = kNoNoiseDbfs;
  result.vad_result = kVadSpeech;
  result.estimate_is_confident = true;
  result.limiter_audio_level_dbfs = -2.f;
  return result;
}

}  // namespace

TEST(AutomaticGainController2AdaptiveGainApplier, GainApplierShouldNotCrash) {
  static_assert(
      std::is_trivially_destructible<VadWithLevel::LevelAndProbability>::value,
      "");
  ApmDataDumper apm_data_dumper(0);
  AdaptiveDigitalGainApplier gain_applier(&apm_data_dumper);

  // Make one call with reasonable audio level values and settings.
  VectorFloatFrame fake_audio(2, 480, 10000.f);
  auto signal_with_level = TestSignalWithLevel(fake_audio.float_frame_view());
  signal_with_level.input_level_dbfs = -5.0;
  gain_applier.Process(signal_with_level);
}

// Check that the output is -kHeadroom dBFS.
TEST(AutomaticGainController2AdaptiveGainApplier, TargetLevelIsReached) {
  ApmDataDumper apm_data_dumper(0);
  AdaptiveDigitalGainApplier gain_applier(&apm_data_dumper);

  constexpr float initial_level_dbfs = -5.f;

  const float applied_gain =
      RunOnConstantLevel(200, kVadSpeech, initial_level_dbfs, &gain_applier);

  EXPECT_NEAR(applied_gain, DbToRatio(-kHeadroomDbfs - initial_level_dbfs),
              0.1f);
}

// Check that the output is -kHeadroom dBFS
TEST(AutomaticGainController2AdaptiveGainApplier, GainApproachesMaxGain) {
  ApmDataDumper apm_data_dumper(0);
  AdaptiveDigitalGainApplier gain_applier(&apm_data_dumper);

  constexpr float initial_level_dbfs = -kHeadroomDbfs - kMaxGainDb - 10.f;
  // A few extra frames for safety.
  constexpr int kNumFramesToAdapt =
      static_cast<int>(kMaxGainDb / kMaxGainChangePerFrameDb) + 10;

  const float applied_gain = RunOnConstantLevel(
      kNumFramesToAdapt, kVadSpeech, initial_level_dbfs, &gain_applier);
  EXPECT_NEAR(applied_gain, DbToRatio(kMaxGainDb), 0.1f);

  const float applied_gain_db = 20.f * std::log10(applied_gain);
  EXPECT_NEAR(applied_gain_db, kMaxGainDb, 0.1f);
}

TEST(AutomaticGainController2AdaptiveGainApplier, GainDoesNotChangeFast) {
  ApmDataDumper apm_data_dumper(0);
  AdaptiveDigitalGainApplier gain_applier(&apm_data_dumper);

  constexpr float initial_level_dbfs = -25.f;
  // A few extra frames for safety.
  constexpr int kNumFramesToAdapt =
      static_cast<int>(initial_level_dbfs / kMaxGainChangePerFrameDb) + 10;

  const float kMaxChangePerFrameLinear = DbToRatio(kMaxGainChangePerFrameDb);

  float last_gain_linear = 1.f;
  for (int i = 0; i < kNumFramesToAdapt; ++i) {
    SCOPED_TRACE(i);
    VectorFloatFrame fake_audio(1, 1, 1.f);
    auto signal_with_level = TestSignalWithLevel(fake_audio.float_frame_view());
    signal_with_level.input_level_dbfs = initial_level_dbfs;
    gain_applier.Process(signal_with_level);
    float current_gain_linear = fake_audio.float_frame_view().channel(0)[0];
    EXPECT_LE(std::abs(current_gain_linear - last_gain_linear),
              kMaxChangePerFrameLinear);
    last_gain_linear = current_gain_linear;
  }

  // Check that the same is true when gain decreases as well.
  for (int i = 0; i < kNumFramesToAdapt; ++i) {
    SCOPED_TRACE(i);
    VectorFloatFrame fake_audio(1, 1, 1.f);
    auto signal_with_level = TestSignalWithLevel(fake_audio.float_frame_view());
    signal_with_level.input_level_dbfs = 0.f;
    gain_applier.Process(signal_with_level);
    float current_gain_linear = fake_audio.float_frame_view().channel(0)[0];
    EXPECT_LE(std::abs(current_gain_linear - last_gain_linear),
              kMaxChangePerFrameLinear);
    last_gain_linear = current_gain_linear;
  }
}

TEST(AutomaticGainController2AdaptiveGainApplier, GainIsRampedInAFrame) {
  ApmDataDumper apm_data_dumper(0);
  AdaptiveDigitalGainApplier gain_applier(&apm_data_dumper);

  constexpr float initial_level_dbfs = -25.f;
  constexpr int num_samples = 480;

  VectorFloatFrame fake_audio(1, num_samples, 1.f);
  auto signal_with_level = TestSignalWithLevel(fake_audio.float_frame_view());
  signal_with_level.input_level_dbfs = initial_level_dbfs;
  gain_applier.Process(signal_with_level);
  float maximal_difference = 0.f;
  float current_value = 1.f * DbToRatio(kInitialAdaptiveDigitalGainDb);
  for (const auto& x : fake_audio.float_frame_view().channel(0)) {
    const float difference = std::abs(x - current_value);
    maximal_difference = std::max(maximal_difference, difference);
    current_value = x;
  }

  const float kMaxChangePerFrameLinear = DbToRatio(kMaxGainChangePerFrameDb);
  const float kMaxChangePerSample = kMaxChangePerFrameLinear / num_samples;

  EXPECT_LE(maximal_difference, kMaxChangePerSample);
}

TEST(AutomaticGainController2AdaptiveGainApplier, NoiseLimitsGain) {
  ApmDataDumper apm_data_dumper(0);
  AdaptiveDigitalGainApplier gain_applier(&apm_data_dumper);

  constexpr float initial_level_dbfs = -25.f;
  constexpr int num_samples = 480;
  constexpr int num_initial_frames =
      kInitialAdaptiveDigitalGainDb / kMaxGainChangePerFrameDb;
  constexpr int num_frames = 50;

  ASSERT_GT(kWithNoiseDbfs, kMaxNoiseLevelDbfs) << "kWithNoiseDbfs is too low";

  for (int i = 0; i < num_initial_frames + num_frames; ++i) {
    VectorFloatFrame fake_audio(1, num_samples, 1.f);
    auto signal_with_level = TestSignalWithLevel(fake_audio.float_frame_view());
    signal_with_level.input_level_dbfs = initial_level_dbfs;
    signal_with_level.input_noise_level_dbfs = kWithNoiseDbfs;
    gain_applier.Process(signal_with_level);

    // Wait so that the adaptive gain applier has time to lower the gain.
    if (i > num_initial_frames) {
      const float maximal_ratio =
          *std::max_element(fake_audio.float_frame_view().channel(0).begin(),
                            fake_audio.float_frame_view().channel(0).end());

      EXPECT_NEAR(maximal_ratio, 1.f, 0.001f);
    }
  }
}

TEST(AutomaticGainController2GainApplier, CanHandlePositiveSpeechLevels) {
  ApmDataDumper apm_data_dumper(0);
  AdaptiveDigitalGainApplier gain_applier(&apm_data_dumper);

  // Make one call with positive audio level values and settings.
  VectorFloatFrame fake_audio(2, 480, 10000.f);
  auto signal_with_level = TestSignalWithLevel(fake_audio.float_frame_view());
  signal_with_level.input_level_dbfs = 5.0f;
  gain_applier.Process(signal_with_level);
}

TEST(AutomaticGainController2GainApplier, AudioLevelLimitsGain) {
  ApmDataDumper apm_data_dumper(0);
  AdaptiveDigitalGainApplier gain_applier(&apm_data_dumper);

  constexpr float initial_level_dbfs = -25.f;
  constexpr int num_samples = 480;
  constexpr int num_initial_frames =
      kInitialAdaptiveDigitalGainDb / kMaxGainChangePerFrameDb;
  constexpr int num_frames = 50;

  ASSERT_GT(kWithNoiseDbfs, kMaxNoiseLevelDbfs) << "kWithNoiseDbfs is too low";

  for (int i = 0; i < num_initial_frames + num_frames; ++i) {
    VectorFloatFrame fake_audio(1, num_samples, 1.f);
    auto signal_with_level = TestSignalWithLevel(fake_audio.float_frame_view());
    signal_with_level.input_level_dbfs = initial_level_dbfs;
    signal_with_level.limiter_audio_level_dbfs = 1.f;
    signal_with_level.estimate_is_confident = false;
    gain_applier.Process(signal_with_level);

    // Wait so that the adaptive gain applier has time to lower the gain.
    if (i > num_initial_frames) {
      const float maximal_ratio =
          *std::max_element(fake_audio.float_frame_view().channel(0).begin(),
                            fake_audio.float_frame_view().channel(0).end());

      EXPECT_NEAR(maximal_ratio, 1.f, 0.001f);
    }
  }
}
}  // namespace webrtc
