/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "api/audio/audio_processing.h"
#include "modules/audio_processing/agc2/speech_level_estimator_experimental_impl.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/checks.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using AdaptiveDigitalConfig =
    AudioProcessing::Config::GainController2::AdaptiveDigital;

constexpr float kConvergenceSpeedTestsLevelTolerance = 0.5f;
constexpr float kNoSpeechProbability = 0.0f;
constexpr float kMaxSpeechProbability = 1.0f;
constexpr int kFramesPerUpdate = 100;

// Provides the `vad_level` value `num_iterations` times to `level_estimator`.
void RunOnConstantLevel(int num_iterations,
                        float rms_dbfs,
                        float speech_probability,
                        SpeechLevelEstimatorExperimentalImpl& level_estimator) {
  for (int i = 0; i < num_iterations; ++i) {
    level_estimator.Update(rms_dbfs, speech_probability);
  }
}

// Level estimator with data dumper.
struct TestLevelEstimator {
  explicit TestLevelEstimator(int adjacent_speech_frames_threshold)
      : data_dumper(0),
        estimator(std::make_unique<SpeechLevelEstimatorExperimentalImpl>(
            &data_dumper,
            AdaptiveDigitalConfig{},
            adjacent_speech_frames_threshold)),
        initial_speech_level_dbfs(estimator->GetLevelDbfs()),
        level_rms_dbfs(initial_speech_level_dbfs / 2.0f),
        level_peak_dbfs(initial_speech_level_dbfs / 3.0f) {
    RTC_DCHECK_LT(level_rms_dbfs, level_peak_dbfs);
    RTC_DCHECK_LT(initial_speech_level_dbfs, level_rms_dbfs);
    RTC_DCHECK_GT(level_rms_dbfs - initial_speech_level_dbfs, 5.0f)
        << "Adjust `level_rms_dbfs` so that the difference from the initial "
           "level is wide enough for the tests";
  }
  ApmDataDumper data_dumper;
  std::unique_ptr<SpeechLevelEstimatorExperimentalImpl> estimator;
  const float initial_speech_level_dbfs;
  const float level_rms_dbfs;
  const float level_peak_dbfs;
};

// Checks that the level estimator converges to a constant input speech level.
TEST(GainController2SpeechLevelEstimatorExperimental, LevelStabilizes) {
  TestLevelEstimator level_estimator(/*adjacent_speech_frames_threshold=*/1);
  RunOnConstantLevel(kFramesPerUpdate, level_estimator.level_rms_dbfs,
                     kMaxSpeechProbability, *level_estimator.estimator);
  const float estimated_level_dbfs = level_estimator.estimator->GetLevelDbfs();
  RunOnConstantLevel(/*num_iterations=*/1, level_estimator.level_rms_dbfs,
                     kMaxSpeechProbability, *level_estimator.estimator);
  EXPECT_NEAR(level_estimator.estimator->GetLevelDbfs(), estimated_level_dbfs,
              0.1f);
}

// Checks that the level controller does not become confident when too few
// speech frames are observed.
TEST(GainController2SpeechLevelEstimatorExperimental, IsNotConfident) {
  TestLevelEstimator level_estimator(/*adjacent_speech_frames_threshold=*/1);
  RunOnConstantLevel(kFramesPerUpdate / 2, level_estimator.level_rms_dbfs,
                     kMaxSpeechProbability, *level_estimator.estimator);
  EXPECT_FALSE(level_estimator.estimator->IsConfident());
}

// Checks that the level controller becomes confident when enough speech frames
// are observed.
TEST(GainController2SpeechLevelEstimatorExperimental, IsConfident) {
  TestLevelEstimator level_estimator(/*adjacent_speech_frames_threshold=*/1);
  RunOnConstantLevel(kFramesPerUpdate, level_estimator.level_rms_dbfs,
                     kMaxSpeechProbability, *level_estimator.estimator);
  EXPECT_TRUE(level_estimator.estimator->IsConfident());
}

// Checks that the estimated level is not affected by the level of non-speech
// frames.
TEST(GainController2SpeechLevelEstimatorExperimental,
     EstimatorIgnoresNonSpeechFrames) {
  TestLevelEstimator level_estimator(/*adjacent_speech_frames_threshold=*/1);
  // Simulate speech.
  RunOnConstantLevel(kFramesPerUpdate, level_estimator.level_rms_dbfs,
                     kMaxSpeechProbability, *level_estimator.estimator);
  const float estimated_level_dbfs = level_estimator.estimator->GetLevelDbfs();
  // Simulate full-scale non-speech.
  RunOnConstantLevel(kFramesPerUpdate, /*rms_dbfs=*/0.0f, kNoSpeechProbability,
                     *level_estimator.estimator);
  // No estimated level change is expected.
  EXPECT_FLOAT_EQ(level_estimator.estimator->GetLevelDbfs(),
                  estimated_level_dbfs);
}

// Checks the convergence speed of the estimator before it becomes confident.
TEST(GainController2SpeechLevelEstimatorExperimental,
     ConvergenceSpeedBeforeConfidence) {
  TestLevelEstimator level_estimator(/*adjacent_speech_frames_threshold=*/1);
  RunOnConstantLevel(kFramesPerUpdate, level_estimator.level_rms_dbfs,
                     kMaxSpeechProbability, *level_estimator.estimator);
  EXPECT_NEAR(level_estimator.estimator->GetLevelDbfs(),
              level_estimator.level_rms_dbfs,
              kConvergenceSpeedTestsLevelTolerance);
}

// Checks the convergence speed of the estimator after it becomes confident.
TEST(GainController2SpeechLevelEstimatorExperimental,
     ConvergenceSpeedAfterConfidence) {
  TestLevelEstimator level_estimator(/*adjacent_speech_frames_threshold=*/1);
  // Reach confidence using the initial level estimate.
  RunOnConstantLevel(kFramesPerUpdate,
                     /*rms_dbfs=*/level_estimator.initial_speech_level_dbfs,
                     kMaxSpeechProbability, *level_estimator.estimator);
  // No estimate change should occur, but confidence is achieved.
  ASSERT_FLOAT_EQ(level_estimator.estimator->GetLevelDbfs(),
                  level_estimator.initial_speech_level_dbfs);
  ASSERT_TRUE(level_estimator.estimator->IsConfident());
  // After confidence.
  constexpr float kConvergenceTimeAfterConfidenceNumFrames = 700;  // 7 seconds.
  static_assert(kConvergenceTimeAfterConfidenceNumFrames > kFramesPerUpdate,
                "");
  RunOnConstantLevel(
      /*num_iterations=*/kConvergenceTimeAfterConfidenceNumFrames,
      level_estimator.level_rms_dbfs, kMaxSpeechProbability,
      *level_estimator.estimator);
  EXPECT_NEAR(level_estimator.estimator->GetLevelDbfs(),
              level_estimator.level_rms_dbfs,
              kConvergenceSpeedTestsLevelTolerance);
}

}  // namespace
}  // namespace webrtc
