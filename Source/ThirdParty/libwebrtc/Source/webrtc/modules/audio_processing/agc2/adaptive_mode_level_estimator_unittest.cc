/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/adaptive_mode_level_estimator.h"

#include <memory>

#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/gunit.h"

namespace webrtc {
namespace {

using AdaptiveDigitalConfig =
    AudioProcessing::Config::GainController2::AdaptiveDigital;

// Number of speech frames that the level estimator must observe in order to
// become confident about the estimated level.
constexpr int kNumFramesToConfidence =
    kLevelEstimatorTimeToConfidenceMs / kFrameDurationMs;
static_assert(kNumFramesToConfidence > 0, "");

constexpr float kConvergenceSpeedTestsLevelTolerance = 0.5f;

// Provides the `vad_level` value `num_iterations` times to `level_estimator`.
void RunOnConstantLevel(int num_iterations,
                        const VadLevelAnalyzer::Result& vad_level,
                        AdaptiveModeLevelEstimator& level_estimator) {
  for (int i = 0; i < num_iterations; ++i) {
    level_estimator.Update(vad_level);
  }
}

constexpr AdaptiveDigitalConfig GetAdaptiveDigitalConfig(
    int adjacent_speech_frames_threshold) {
  AdaptiveDigitalConfig config;
  config.adjacent_speech_frames_threshold = adjacent_speech_frames_threshold;
  return config;
}

// Level estimator with data dumper.
struct TestLevelEstimator {
  explicit TestLevelEstimator(int adjacent_speech_frames_threshold)
      : data_dumper(0),
        estimator(std::make_unique<AdaptiveModeLevelEstimator>(
            &data_dumper,
            GetAdaptiveDigitalConfig(adjacent_speech_frames_threshold))),
        initial_speech_level_dbfs(estimator->level_dbfs()),
        vad_level_rms(initial_speech_level_dbfs / 2.0f),
        vad_level_peak(initial_speech_level_dbfs / 3.0f),
        vad_data_speech(
            {/*speech_probability=*/1.0f, vad_level_rms, vad_level_peak}),
        vad_data_non_speech(
            {/*speech_probability=*/kVadConfidenceThreshold / 2.0f,
             vad_level_rms, vad_level_peak}) {
    RTC_DCHECK_LT(vad_level_rms, vad_level_peak);
    RTC_DCHECK_LT(initial_speech_level_dbfs, vad_level_rms);
    RTC_DCHECK_GT(vad_level_rms - initial_speech_level_dbfs, 5.0f)
        << "Adjust `vad_level_rms` so that the difference from the initial "
           "level is wide enough for the tests";
  }
  ApmDataDumper data_dumper;
  std::unique_ptr<AdaptiveModeLevelEstimator> estimator;
  const float initial_speech_level_dbfs;
  const float vad_level_rms;
  const float vad_level_peak;
  const VadLevelAnalyzer::Result vad_data_speech;
  const VadLevelAnalyzer::Result vad_data_non_speech;
};

// Checks that the level estimator converges to a constant input speech level.
TEST(GainController2AdaptiveModeLevelEstimator, LevelStabilizes) {
  TestLevelEstimator level_estimator(/*adjacent_speech_frames_threshold=*/1);
  RunOnConstantLevel(/*num_iterations=*/kNumFramesToConfidence,
                     level_estimator.vad_data_speech,
                     *level_estimator.estimator);
  const float estimated_level_dbfs = level_estimator.estimator->level_dbfs();
  RunOnConstantLevel(/*num_iterations=*/1, level_estimator.vad_data_speech,
                     *level_estimator.estimator);
  EXPECT_NEAR(level_estimator.estimator->level_dbfs(), estimated_level_dbfs,
              0.1f);
}

// Checks that the level controller does not become confident when too few
// speech frames are observed.
TEST(GainController2AdaptiveModeLevelEstimator, IsNotConfident) {
  TestLevelEstimator level_estimator(/*adjacent_speech_frames_threshold=*/1);
  RunOnConstantLevel(/*num_iterations=*/kNumFramesToConfidence / 2,
                     level_estimator.vad_data_speech,
                     *level_estimator.estimator);
  EXPECT_FALSE(level_estimator.estimator->IsConfident());
}

// Checks that the level controller becomes confident when enough speech frames
// are observed.
TEST(GainController2AdaptiveModeLevelEstimator, IsConfident) {
  TestLevelEstimator level_estimator(/*adjacent_speech_frames_threshold=*/1);
  RunOnConstantLevel(/*num_iterations=*/kNumFramesToConfidence,
                     level_estimator.vad_data_speech,
                     *level_estimator.estimator);
  EXPECT_TRUE(level_estimator.estimator->IsConfident());
}

// Checks that the estimated level is not affected by the level of non-speech
// frames.
TEST(GainController2AdaptiveModeLevelEstimator,
     EstimatorIgnoresNonSpeechFrames) {
  TestLevelEstimator level_estimator(/*adjacent_speech_frames_threshold=*/1);
  // Simulate speech.
  RunOnConstantLevel(/*num_iterations=*/kNumFramesToConfidence,
                     level_estimator.vad_data_speech,
                     *level_estimator.estimator);
  const float estimated_level_dbfs = level_estimator.estimator->level_dbfs();
  // Simulate full-scale non-speech.
  RunOnConstantLevel(/*num_iterations=*/kNumFramesToConfidence,
                     VadLevelAnalyzer::Result{/*speech_probability=*/0.0f,
                                              /*rms_dbfs=*/0.0f,
                                              /*peak_dbfs=*/0.0f},
                     *level_estimator.estimator);
  // No estimated level change is expected.
  EXPECT_FLOAT_EQ(level_estimator.estimator->level_dbfs(),
                  estimated_level_dbfs);
}

// Checks the convergence speed of the estimator before it becomes confident.
TEST(GainController2AdaptiveModeLevelEstimator,
     ConvergenceSpeedBeforeConfidence) {
  TestLevelEstimator level_estimator(/*adjacent_speech_frames_threshold=*/1);
  RunOnConstantLevel(/*num_iterations=*/kNumFramesToConfidence,
                     level_estimator.vad_data_speech,
                     *level_estimator.estimator);
  EXPECT_NEAR(level_estimator.estimator->level_dbfs(),
              level_estimator.vad_data_speech.rms_dbfs,
              kConvergenceSpeedTestsLevelTolerance);
}

// Checks the convergence speed of the estimator after it becomes confident.
TEST(GainController2AdaptiveModeLevelEstimator,
     ConvergenceSpeedAfterConfidence) {
  TestLevelEstimator level_estimator(/*adjacent_speech_frames_threshold=*/1);
  // Reach confidence using the initial level estimate.
  RunOnConstantLevel(
      /*num_iterations=*/kNumFramesToConfidence,
      VadLevelAnalyzer::Result{
          /*speech_probability=*/1.0f,
          /*rms_dbfs=*/level_estimator.initial_speech_level_dbfs,
          /*peak_dbfs=*/level_estimator.initial_speech_level_dbfs + 6.0f},
      *level_estimator.estimator);
  // No estimate change should occur, but confidence is achieved.
  ASSERT_FLOAT_EQ(level_estimator.estimator->level_dbfs(),
                  level_estimator.initial_speech_level_dbfs);
  ASSERT_TRUE(level_estimator.estimator->IsConfident());
  // After confidence.
  constexpr float kConvergenceTimeAfterConfidenceNumFrames = 600;  // 6 seconds.
  static_assert(
      kConvergenceTimeAfterConfidenceNumFrames > kNumFramesToConfidence, "");
  RunOnConstantLevel(
      /*num_iterations=*/kConvergenceTimeAfterConfidenceNumFrames,
      level_estimator.vad_data_speech, *level_estimator.estimator);
  EXPECT_NEAR(level_estimator.estimator->level_dbfs(),
              level_estimator.vad_data_speech.rms_dbfs,
              kConvergenceSpeedTestsLevelTolerance);
}

class AdaptiveModeLevelEstimatorParametrization
    : public ::testing::TestWithParam<int> {
 protected:
  int adjacent_speech_frames_threshold() const { return GetParam(); }
};

TEST_P(AdaptiveModeLevelEstimatorParametrization,
       DoNotAdaptToShortSpeechSegments) {
  TestLevelEstimator level_estimator(adjacent_speech_frames_threshold());
  const float initial_level = level_estimator.estimator->level_dbfs();
  ASSERT_LT(initial_level, level_estimator.vad_data_speech.peak_dbfs);
  for (int i = 0; i < adjacent_speech_frames_threshold() - 1; ++i) {
    SCOPED_TRACE(i);
    level_estimator.estimator->Update(level_estimator.vad_data_speech);
    EXPECT_EQ(initial_level, level_estimator.estimator->level_dbfs());
  }
  level_estimator.estimator->Update(level_estimator.vad_data_non_speech);
  EXPECT_EQ(initial_level, level_estimator.estimator->level_dbfs());
}

TEST_P(AdaptiveModeLevelEstimatorParametrization, AdaptToEnoughSpeechSegments) {
  TestLevelEstimator level_estimator(adjacent_speech_frames_threshold());
  const float initial_level = level_estimator.estimator->level_dbfs();
  ASSERT_LT(initial_level, level_estimator.vad_data_speech.peak_dbfs);
  for (int i = 0; i < adjacent_speech_frames_threshold(); ++i) {
    level_estimator.estimator->Update(level_estimator.vad_data_speech);
  }
  EXPECT_LT(initial_level, level_estimator.estimator->level_dbfs());
}

INSTANTIATE_TEST_SUITE_P(GainController2,
                         AdaptiveModeLevelEstimatorParametrization,
                         ::testing::Values(1, 9, 17));

}  // namespace
}  // namespace webrtc
