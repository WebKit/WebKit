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
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/gunit.h"

namespace webrtc {
namespace {

// Number of speech frames that the level estimator must observe in order to
// become confident about the estimated level.
constexpr int kNumFramesToConfidence =
    kLevelEstimatorTimeToConfidenceMs / kFrameDurationMs;
static_assert(kNumFramesToConfidence > 0, "");

// Fake levels and speech probabilities used in the tests.
static_assert(kInitialSpeechLevelEstimateDbfs < 0.0f, "");
constexpr float kVadLevelRms = kInitialSpeechLevelEstimateDbfs / 2.0f;
constexpr float kVadLevelPeak = kInitialSpeechLevelEstimateDbfs / 3.0f;
static_assert(kVadLevelRms < kVadLevelPeak, "");
static_assert(kVadLevelRms > kInitialSpeechLevelEstimateDbfs, "");
static_assert(kVadLevelRms - kInitialSpeechLevelEstimateDbfs > 5.0f,
              "Adjust `kVadLevelRms` so that the difference from the initial "
              "level is wide enough for the tests.");

constexpr VadLevelAnalyzer::Result kVadDataSpeech{/*speech_probability=*/1.0f,
                                                  kVadLevelRms, kVadLevelPeak};
constexpr VadLevelAnalyzer::Result kVadDataNonSpeech{
    /*speech_probability=*/kVadConfidenceThreshold / 2.0f, kVadLevelRms,
    kVadLevelPeak};

constexpr float kMinSpeechProbability = 0.0f;
constexpr float kMaxSpeechProbability = 1.0f;

constexpr float kConvergenceSpeedTestsLevelTolerance = 0.5f;

// Provides the `vad_level` value `num_iterations` times to `level_estimator`.
void RunOnConstantLevel(int num_iterations,
                        const VadLevelAnalyzer::Result& vad_level,
                        AdaptiveModeLevelEstimator& level_estimator) {
  for (int i = 0; i < num_iterations; ++i) {
    level_estimator.Update(vad_level);
  }
}

// Level estimator with data dumper.
struct TestLevelEstimator {
  TestLevelEstimator()
      : data_dumper(0),
        estimator(std::make_unique<AdaptiveModeLevelEstimator>(
            &data_dumper,
            /*adjacent_speech_frames_threshold=*/1)) {}
  ApmDataDumper data_dumper;
  std::unique_ptr<AdaptiveModeLevelEstimator> estimator;
};

// Checks the initially estimated level.
TEST(GainController2AdaptiveModeLevelEstimator, CheckInitialEstimate) {
  TestLevelEstimator level_estimator;
  EXPECT_FLOAT_EQ(level_estimator.estimator->level_dbfs(),
                  kInitialSpeechLevelEstimateDbfs);
}

// Checks that the level estimator converges to a constant input speech level.
TEST(GainController2AdaptiveModeLevelEstimator, LevelStabilizes) {
  TestLevelEstimator level_estimator;
  RunOnConstantLevel(/*num_iterations=*/kNumFramesToConfidence, kVadDataSpeech,
                     *level_estimator.estimator);
  const float estimated_level_dbfs = level_estimator.estimator->level_dbfs();
  RunOnConstantLevel(/*num_iterations=*/1, kVadDataSpeech,
                     *level_estimator.estimator);
  EXPECT_NEAR(level_estimator.estimator->level_dbfs(), estimated_level_dbfs,
              0.1f);
}

// Checks that the level controller does not become confident when too few
// speech frames are observed.
TEST(GainController2AdaptiveModeLevelEstimator, IsNotConfident) {
  TestLevelEstimator level_estimator;
  RunOnConstantLevel(/*num_iterations=*/kNumFramesToConfidence / 2,
                     kVadDataSpeech, *level_estimator.estimator);
  EXPECT_FALSE(level_estimator.estimator->IsConfident());
}

// Checks that the level controller becomes confident when enough speech frames
// are observed.
TEST(GainController2AdaptiveModeLevelEstimator, IsConfident) {
  TestLevelEstimator level_estimator;
  RunOnConstantLevel(/*num_iterations=*/kNumFramesToConfidence, kVadDataSpeech,
                     *level_estimator.estimator);
  EXPECT_TRUE(level_estimator.estimator->IsConfident());
}

// Checks that the estimated level is not affected by the level of non-speech
// frames.
TEST(GainController2AdaptiveModeLevelEstimator,
     EstimatorIgnoresNonSpeechFrames) {
  TestLevelEstimator level_estimator;
  // Simulate speech.
  RunOnConstantLevel(/*num_iterations=*/kNumFramesToConfidence, kVadDataSpeech,
                     *level_estimator.estimator);
  const float estimated_level_dbfs = level_estimator.estimator->level_dbfs();
  // Simulate full-scale non-speech.
  RunOnConstantLevel(/*num_iterations=*/kNumFramesToConfidence,
                     VadLevelAnalyzer::Result{kMinSpeechProbability,
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
  TestLevelEstimator level_estimator;
  RunOnConstantLevel(/*num_iterations=*/kNumFramesToConfidence, kVadDataSpeech,
                     *level_estimator.estimator);
  EXPECT_NEAR(level_estimator.estimator->level_dbfs(), kVadDataSpeech.rms_dbfs,
              kConvergenceSpeedTestsLevelTolerance);
}

// Checks the convergence speed of the estimator after it becomes confident.
TEST(GainController2AdaptiveModeLevelEstimator,
     ConvergenceSpeedAfterConfidence) {
  TestLevelEstimator level_estimator;
  // Reach confidence using the initial level estimate.
  RunOnConstantLevel(
      /*num_iterations=*/kNumFramesToConfidence,
      VadLevelAnalyzer::Result{
          kMaxSpeechProbability,
          /*rms_dbfs=*/kInitialSpeechLevelEstimateDbfs,
          /*peak_dbfs=*/kInitialSpeechLevelEstimateDbfs + 6.0f},
      *level_estimator.estimator);
  // No estimate change should occur, but confidence is achieved.
  ASSERT_FLOAT_EQ(level_estimator.estimator->level_dbfs(),
                  kInitialSpeechLevelEstimateDbfs);
  ASSERT_TRUE(level_estimator.estimator->IsConfident());
  // After confidence.
  constexpr float kConvergenceTimeAfterConfidenceNumFrames = 600;  // 6 seconds.
  static_assert(
      kConvergenceTimeAfterConfidenceNumFrames > kNumFramesToConfidence, "");
  RunOnConstantLevel(
      /*num_iterations=*/kConvergenceTimeAfterConfidenceNumFrames,
      kVadDataSpeech, *level_estimator.estimator);
  EXPECT_NEAR(level_estimator.estimator->level_dbfs(), kVadDataSpeech.rms_dbfs,
              kConvergenceSpeedTestsLevelTolerance);
}

class AdaptiveModeLevelEstimatorParametrization
    : public ::testing::TestWithParam<int> {
 protected:
  int adjacent_speech_frames_threshold() const { return GetParam(); }
};

TEST_P(AdaptiveModeLevelEstimatorParametrization,
       DoNotAdaptToShortSpeechSegments) {
  ApmDataDumper apm_data_dumper(0);
  AdaptiveModeLevelEstimator level_estimator(
      &apm_data_dumper, adjacent_speech_frames_threshold());
  const float initial_level = level_estimator.level_dbfs();
  ASSERT_LT(initial_level, kVadDataSpeech.peak_dbfs);
  for (int i = 0; i < adjacent_speech_frames_threshold() - 1; ++i) {
    SCOPED_TRACE(i);
    level_estimator.Update(kVadDataSpeech);
    EXPECT_EQ(initial_level, level_estimator.level_dbfs());
  }
  level_estimator.Update(kVadDataNonSpeech);
  EXPECT_EQ(initial_level, level_estimator.level_dbfs());
}

TEST_P(AdaptiveModeLevelEstimatorParametrization, AdaptToEnoughSpeechSegments) {
  ApmDataDumper apm_data_dumper(0);
  AdaptiveModeLevelEstimator level_estimator(
      &apm_data_dumper, adjacent_speech_frames_threshold());
  const float initial_level = level_estimator.level_dbfs();
  ASSERT_LT(initial_level, kVadDataSpeech.peak_dbfs);
  for (int i = 0; i < adjacent_speech_frames_threshold(); ++i) {
    level_estimator.Update(kVadDataSpeech);
  }
  EXPECT_LT(initial_level, level_estimator.level_dbfs());
}

INSTANTIATE_TEST_SUITE_P(GainController2,
                         AdaptiveModeLevelEstimatorParametrization,
                         ::testing::Values(1, 9, 17));

}  // namespace
}  // namespace webrtc
