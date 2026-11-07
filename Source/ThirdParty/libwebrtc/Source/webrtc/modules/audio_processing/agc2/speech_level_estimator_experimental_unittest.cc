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
#include "api/field_trials.h"
#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/agc2/speech_level_estimator.h"
#include "modules/audio_processing/agc2/speech_level_estimator_experimental_impl.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/checks.h"
#include "test/create_test_field_trials.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using AdaptiveDigitalConfig =
    AudioProcessing::Config::GainController2::AdaptiveDigital;

constexpr float kConvergenceSpeedTestsLevelTolerance = 0.5f;
constexpr float kLevelToleranceDbfs = 1.0f;
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
      : data_dumper(/*instance_index=*/0),
        estimator(std::make_unique<SpeechLevelEstimatorExperimentalImpl>(
            &data_dumper,
            AdaptiveDigitalConfig{},
            adjacent_speech_frames_threshold,
            SpeechLevelEstimatorExperimentalImpl::
                kDefaultBackgroundSpeakerOffsetDbfs,
            SpeechLevelEstimatorExperimentalImpl::kDefaultMaxTimeToUpdateMs)),
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
  EXPECT_NEAR(level_estimator.estimator->GetLevelDbfs(), estimated_level_dbfs,
              kLevelToleranceDbfs);
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
  ASSERT_NEAR(level_estimator.estimator->GetLevelDbfs(),
              level_estimator.initial_speech_level_dbfs, kLevelToleranceDbfs);
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

// Checks that the estimator detects background speakers and does not adapt the
// level estimate down.
TEST(GainController2SpeechLevelEstimatorExperimental,
     DetectsBackgroundSpeaker) {
  TestLevelEstimator level_estimator(/*adjacent_speech_frames_threshold=*/1);
  // Reach confidence with the initial speaker level.
  RunOnConstantLevel(kFramesPerUpdate, level_estimator.level_rms_dbfs,
                     kMaxSpeechProbability, *level_estimator.estimator);
  ASSERT_TRUE(level_estimator.estimator->IsConfident());
  EXPECT_TRUE(level_estimator.estimator->IsTargetSpeakerActive());
  const float confident_level_dbfs = level_estimator.estimator->GetLevelDbfs();

  // Present a quieter background speaker below the offset threshold.
  constexpr float kBackgroundSpeakerLevelDropDbfs =
      SpeechLevelEstimatorExperimentalImpl::
          kDefaultBackgroundSpeakerOffsetDbfs +
      5.0f;
  const float background_speaker_level_dbfs =
      confident_level_dbfs - kBackgroundSpeakerLevelDropDbfs;
  RunOnConstantLevel(kFramesPerUpdate, background_speaker_level_dbfs,
                     kMaxSpeechProbability, *level_estimator.estimator);

  // The background speaker should be detected and estimated level retained.
  EXPECT_FALSE(level_estimator.estimator->IsTargetSpeakerActive());
  EXPECT_NEAR(level_estimator.estimator->GetLevelDbfs(), confident_level_dbfs,
              kLevelToleranceDbfs);

  // When the primary speaker speaks again, target speaker is active again.
  RunOnConstantLevel(kFramesPerUpdate, level_estimator.level_rms_dbfs,
                     kMaxSpeechProbability, *level_estimator.estimator);
  EXPECT_TRUE(level_estimator.estimator->IsTargetSpeakerActive());
  EXPECT_NEAR(level_estimator.estimator->GetLevelDbfs(), confident_level_dbfs,
              kLevelToleranceDbfs);
}

TEST(GainController2SpeechLevelEstimatorExperimental,
     DoesNotResetStateBeforeConfidenceWhenExceedingMaxTime) {
  constexpr int kMaxFramesToUpdate =
      SpeechLevelEstimatorExperimentalImpl::kDefaultMaxTimeToUpdateMs /
      kFrameDurationMs;
  TestLevelEstimator level_estimator(/*adjacent_speech_frames_threshold=*/12);

  // Before `is_confident_` is true, accumulate half of the required frames,
  // wait past the timeout limit, and then accumulate the remaining frames.
  // State should not be reset before initial confidence is reached.
  RunOnConstantLevel(kFramesPerUpdate / 2, level_estimator.level_rms_dbfs,
                     kMaxSpeechProbability, *level_estimator.estimator);
  ASSERT_FALSE(level_estimator.estimator->IsConfident());

  RunOnConstantLevel(kMaxFramesToUpdate, level_estimator.level_rms_dbfs,
                     kNoSpeechProbability, *level_estimator.estimator);

  RunOnConstantLevel(kFramesPerUpdate / 2, level_estimator.level_rms_dbfs,
                     kMaxSpeechProbability, *level_estimator.estimator);
  EXPECT_TRUE(level_estimator.estimator->IsConfident());
  EXPECT_NEAR(level_estimator.estimator->GetLevelDbfs(),
              level_estimator.level_rms_dbfs,
              kConvergenceSpeedTestsLevelTolerance);
}

TEST(GainController2SpeechLevelEstimatorExperimental,
     ResetsStateWhenUpdateTakesMoreThanMaxTimeAfterConfidence) {
  constexpr int kMaxFramesToUpdate =
      SpeechLevelEstimatorExperimentalImpl::kDefaultMaxTimeToUpdateMs /
      kFrameDurationMs;
  TestLevelEstimator level_estimator(/*adjacent_speech_frames_threshold=*/12);

  // Reach initial confidence first.
  RunOnConstantLevel(kFramesPerUpdate, level_estimator.level_rms_dbfs,
                     kMaxSpeechProbability, *level_estimator.estimator);
  ASSERT_TRUE(level_estimator.estimator->IsConfident());
  const float confident_level_dbfs = level_estimator.estimator->GetLevelDbfs();
  const float new_speaker_level_dbfs = confident_level_dbfs + 10.0f;

  // Accumulate half of the required frames at the new level.
  RunOnConstantLevel(kFramesPerUpdate / 2, new_speaker_level_dbfs,
                     kMaxSpeechProbability, *level_estimator.estimator);

  // Wait long enough for the timeout since reliable speech accumulation began
  // to trigger a state reset and flag low activity.
  RunOnConstantLevel(kMaxFramesToUpdate, new_speaker_level_dbfs,
                     kNoSpeechProbability, *level_estimator.estimator);
  EXPECT_FALSE(level_estimator.estimator->IsTargetSpeakerActive());

  // Accumulate another half of the required frames. Because the earlier frames
  // expired, the estimator should not yet reach `kFramesPerUpdate` and must
  // retain `confident_level_dbfs` and the low activity flag.
  RunOnConstantLevel(kFramesPerUpdate / 2, new_speaker_level_dbfs,
                     kMaxSpeechProbability, *level_estimator.estimator);
  EXPECT_FALSE(level_estimator.estimator->IsTargetSpeakerActive());
  EXPECT_NEAR(level_estimator.estimator->GetLevelDbfs(), confident_level_dbfs,
              kLevelToleranceDbfs);

  // Providing the remaining half of the required frames within the new window
  // reaches `kFramesPerUpdate`, clears the low activity flag, and updates
  // the level.
  RunOnConstantLevel(kFramesPerUpdate / 2, new_speaker_level_dbfs,
                     kMaxSpeechProbability, *level_estimator.estimator);
  EXPECT_TRUE(level_estimator.estimator->IsTargetSpeakerActive());
  EXPECT_NEAR(level_estimator.estimator->GetLevelDbfs(), new_speaker_level_dbfs,
              kLevelToleranceDbfs);
}

TEST(GainController2SpeechLevelEstimatorExperimental,
     SporadicSpeechResetsBeforeUpdatingLevel) {
  constexpr int kMaxFramesToUpdate =
      SpeechLevelEstimatorExperimentalImpl::kDefaultMaxTimeToUpdateMs /
      kFrameDurationMs;
  constexpr int kAdjacentSpeechFramesThreshold = 12;
  TestLevelEstimator level_estimator(kAdjacentSpeechFramesThreshold);
  // Reach confidence with the primary speaker.
  RunOnConstantLevel(kFramesPerUpdate, level_estimator.level_rms_dbfs,
                     kMaxSpeechProbability, *level_estimator.estimator);
  ASSERT_TRUE(level_estimator.estimator->IsConfident());
  const float confident_level_dbfs = level_estimator.estimator->GetLevelDbfs();

  // Simulate sporadic speech bursts whose total speech frames exceed
  // `kFramesPerUpdate`, but spaced far enough apart that `kMaxFramesToUpdate`
  // elapses before `kFramesPerUpdate` is reached.
  constexpr int kSpeechFramesPerBurst = kAdjacentSpeechFramesThreshold + 3;
  constexpr int kNumBursts = kFramesPerUpdate / kSpeechFramesPerBurst + 1;
  constexpr int kSilenceFramesBetweenBursts =
      kMaxFramesToUpdate / (kNumBursts - 1);
  const float louder_speaker_level_dbfs = confident_level_dbfs + 10.0f;
  for (int i = 0; i < kNumBursts; ++i) {
    RunOnConstantLevel(kSpeechFramesPerBurst, louder_speaker_level_dbfs,
                       kMaxSpeechProbability, *level_estimator.estimator);
    RunOnConstantLevel(kSilenceFramesBetweenBursts, louder_speaker_level_dbfs,
                       kNoSpeechProbability, *level_estimator.estimator);
  }

  EXPECT_FALSE(level_estimator.estimator->IsTargetSpeakerActive());
  EXPECT_NEAR(level_estimator.estimator->GetLevelDbfs(), confident_level_dbfs,
              kLevelToleranceDbfs);
}

TEST(GainController2SpeechLevelEstimatorExperimental,
     FactoryDefaultThresholdWhenEnabledWithoutParameters) {
  FieldTrials field_trials = CreateTestFieldTrials(
      "WebRTC-Agc2SpeechLevelEstimatorExperimental/Enabled/");
  ApmDataDumper data_dumper(/*instance_index=*/0);
  std::unique_ptr<SpeechLevelEstimator> estimator =
      SpeechLevelEstimator::Create(field_trials, &data_dumper,
                                   AdaptiveDigitalConfig{},
                                   /*adjacent_speech_frames_threshold=*/1);
  ASSERT_TRUE(estimator);
  SpeechLevelEstimatorExperimentalImpl* experimental_estimator =
      static_cast<SpeechLevelEstimatorExperimentalImpl*>(estimator.get());
  EXPECT_EQ(experimental_estimator->GetBackgroundSpeakerOffsetDbfs(),
            SpeechLevelEstimatorExperimentalImpl::
                kDefaultBackgroundSpeakerOffsetDbfs);
  EXPECT_EQ(experimental_estimator->GetMaxTimeToUpdateMs(),
            SpeechLevelEstimatorExperimentalImpl::kDefaultMaxTimeToUpdateMs);
}

TEST(GainController2SpeechLevelEstimatorExperimental,
     FactoryConfiguresOffsetViaFieldTrial) {
  FieldTrials field_trials = CreateTestFieldTrials(
      "WebRTC-Agc2SpeechLevelEstimatorExperimental/Enabled,offset:15.0/");
  ApmDataDumper data_dumper(/*instance_index=*/0);
  std::unique_ptr<SpeechLevelEstimator> estimator =
      SpeechLevelEstimator::Create(field_trials, &data_dumper,
                                   AdaptiveDigitalConfig{},
                                   /*adjacent_speech_frames_threshold=*/1);
  ASSERT_TRUE(estimator);
  SpeechLevelEstimatorExperimentalImpl* experimental_estimator =
      static_cast<SpeechLevelEstimatorExperimentalImpl*>(estimator.get());
  EXPECT_EQ(experimental_estimator->GetBackgroundSpeakerOffsetDbfs(), 15.0f);
}

TEST(GainController2SpeechLevelEstimatorExperimental,
     FactoryFallbackOnInvalidFieldTrialValue) {
  FieldTrials field_trials = CreateTestFieldTrials(
      "WebRTC-Agc2SpeechLevelEstimatorExperimental/Enabled,offset:-5.0/");
  ApmDataDumper data_dumper(/*instance_index=*/0);
  std::unique_ptr<SpeechLevelEstimator> estimator =
      SpeechLevelEstimator::Create(field_trials, &data_dumper,
                                   AdaptiveDigitalConfig{},
                                   /*adjacent_speech_frames_threshold=*/1);
  ASSERT_TRUE(estimator);
  SpeechLevelEstimatorExperimentalImpl* experimental_estimator =
      static_cast<SpeechLevelEstimatorExperimentalImpl*>(estimator.get());
  EXPECT_EQ(experimental_estimator->GetBackgroundSpeakerOffsetDbfs(),
            SpeechLevelEstimatorExperimentalImpl::
                kDefaultBackgroundSpeakerOffsetDbfs);
}

TEST(GainController2SpeechLevelEstimatorExperimental,
     FactoryConfiguresMaxTimeViaFieldTrial) {
  FieldTrials field_trials = CreateTestFieldTrials(
      "WebRTC-Agc2SpeechLevelEstimatorExperimental/Enabled,max_time_ms:15000/");
  ApmDataDumper data_dumper(/*instance_index=*/0);
  std::unique_ptr<SpeechLevelEstimator> estimator =
      SpeechLevelEstimator::Create(field_trials, &data_dumper,
                                   AdaptiveDigitalConfig{},
                                   /*adjacent_speech_frames_threshold=*/1);
  ASSERT_TRUE(estimator);
  SpeechLevelEstimatorExperimentalImpl* experimental_estimator =
      static_cast<SpeechLevelEstimatorExperimentalImpl*>(estimator.get());
  EXPECT_EQ(experimental_estimator->GetMaxTimeToUpdateMs(), 15000);
}

TEST(GainController2SpeechLevelEstimatorExperimental,
     FactoryFallbackOnInvalidMaxTimeFieldTrialValue) {
  FieldTrials field_trials = CreateTestFieldTrials(
      "WebRTC-Agc2SpeechLevelEstimatorExperimental/Enabled,max_time_ms:-1000/");
  ApmDataDumper data_dumper(/*instance_index=*/0);
  std::unique_ptr<SpeechLevelEstimator> estimator =
      SpeechLevelEstimator::Create(field_trials, &data_dumper,
                                   AdaptiveDigitalConfig{},
                                   /*adjacent_speech_frames_threshold=*/1);
  ASSERT_TRUE(estimator);
  SpeechLevelEstimatorExperimentalImpl* experimental_estimator =
      static_cast<SpeechLevelEstimatorExperimentalImpl*>(estimator.get());
  EXPECT_EQ(experimental_estimator->GetMaxTimeToUpdateMs(),
            SpeechLevelEstimatorExperimentalImpl::kDefaultMaxTimeToUpdateMs);
}

}  // namespace
}  // namespace webrtc
