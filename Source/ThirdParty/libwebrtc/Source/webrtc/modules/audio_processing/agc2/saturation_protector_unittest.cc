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

#include "modules/audio_processing/agc2/agc2_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "rtc_base/gunit.h"

namespace webrtc {
namespace {

constexpr float kInitialHeadroomDb = 20.0f;
constexpr float kNoExtraHeadroomDb = 0.0f;
constexpr int kNoAdjacentSpeechFramesRequired = 1;
constexpr float kMaxSpeechProbability = 1.0f;

// Calls `Analyze(speech_probability, peak_dbfs, speech_level_dbfs)`
// `num_iterations` times on `saturation_protector` and return the largest
// headroom difference between two consecutive calls.
float RunOnConstantLevel(int num_iterations,
                         float speech_probability,
                         float peak_dbfs,
                         float speech_level_dbfs,
                         SaturationProtector& saturation_protector) {
  float last_headroom = saturation_protector.HeadroomDb();
  float max_difference = 0.0f;
  for (int i = 0; i < num_iterations; ++i) {
    saturation_protector.Analyze(speech_probability, peak_dbfs,
                                 speech_level_dbfs);
    const float new_headroom = saturation_protector.HeadroomDb();
    max_difference =
        std::max(max_difference, std::fabs(new_headroom - last_headroom));
    last_headroom = new_headroom;
  }
  return max_difference;
}

// Checks that the returned headroom value is correctly reset.
TEST(GainController2SaturationProtector, Reset) {
  ApmDataDumper apm_data_dumper(0);
  auto saturation_protector = CreateSaturationProtector(
      kInitialHeadroomDb, kNoExtraHeadroomDb, kNoAdjacentSpeechFramesRequired,
      &apm_data_dumper);
  const float initial_headroom_db = saturation_protector->HeadroomDb();
  RunOnConstantLevel(/*num_iterations=*/10, kMaxSpeechProbability,
                     /*peak_dbfs=*/0.0f,
                     /*speech_level_dbfs=*/-10.0f, *saturation_protector);
  // Make sure that there are side-effects.
  ASSERT_NE(initial_headroom_db, saturation_protector->HeadroomDb());
  saturation_protector->Reset();
  EXPECT_EQ(initial_headroom_db, saturation_protector->HeadroomDb());
}

// Checks that the estimate converges to the ratio between peaks and level
// estimator values after a while.
TEST(GainController2SaturationProtector, EstimatesCrestRatio) {
  constexpr int kNumIterations = 2000;
  constexpr float kPeakLevelDbfs = -20.0f;
  constexpr float kCrestFactorDb = kInitialHeadroomDb + 1.0f;
  constexpr float kSpeechLevelDbfs = kPeakLevelDbfs - kCrestFactorDb;
  const float kMaxDifferenceDb =
      0.5f * std::fabs(kInitialHeadroomDb - kCrestFactorDb);

  ApmDataDumper apm_data_dumper(0);
  auto saturation_protector = CreateSaturationProtector(
      kInitialHeadroomDb, kNoExtraHeadroomDb, kNoAdjacentSpeechFramesRequired,
      &apm_data_dumper);
  RunOnConstantLevel(kNumIterations, kMaxSpeechProbability, kPeakLevelDbfs,
                     kSpeechLevelDbfs, *saturation_protector);
  EXPECT_NEAR(saturation_protector->HeadroomDb(), kCrestFactorDb,
              kMaxDifferenceDb);
}

// Checks that the extra headroom is applied.
TEST(GainController2SaturationProtector, ExtraHeadroomApplied) {
  constexpr float kExtraHeadroomDb = 5.1234f;
  constexpr int kNumIterations = 10;
  constexpr float kPeakLevelDbfs = -20.0f;
  constexpr float kSpeechLevelDbfs = kPeakLevelDbfs - 15.0f;

  ApmDataDumper apm_data_dumper(0);

  auto saturation_protector_no_extra = CreateSaturationProtector(
      kInitialHeadroomDb, kNoExtraHeadroomDb, kNoAdjacentSpeechFramesRequired,
      &apm_data_dumper);
  for (int i = 0; i < kNumIterations; ++i) {
    saturation_protector_no_extra->Analyze(kMaxSpeechProbability,
                                           kPeakLevelDbfs, kSpeechLevelDbfs);
  }

  auto saturation_protector_extra = CreateSaturationProtector(
      kInitialHeadroomDb, kExtraHeadroomDb, kNoAdjacentSpeechFramesRequired,
      &apm_data_dumper);
  for (int i = 0; i < kNumIterations; ++i) {
    saturation_protector_extra->Analyze(kMaxSpeechProbability, kPeakLevelDbfs,
                                        kSpeechLevelDbfs);
  }

  EXPECT_EQ(saturation_protector_no_extra->HeadroomDb() + kExtraHeadroomDb,
            saturation_protector_extra->HeadroomDb());
}

// Checks that the headroom does not change too quickly.
TEST(GainController2SaturationProtector, ChangeSlowly) {
  constexpr int kNumIterations = 1000;
  constexpr float kPeakLevelDbfs = -20.f;
  constexpr float kCrestFactorDb = kInitialHeadroomDb - 5.f;
  constexpr float kOtherCrestFactorDb = kInitialHeadroomDb;
  constexpr float kSpeechLevelDbfs = kPeakLevelDbfs - kCrestFactorDb;
  constexpr float kOtherSpeechLevelDbfs = kPeakLevelDbfs - kOtherCrestFactorDb;

  ApmDataDumper apm_data_dumper(0);
  auto saturation_protector = CreateSaturationProtector(
      kInitialHeadroomDb, kNoExtraHeadroomDb, kNoAdjacentSpeechFramesRequired,
      &apm_data_dumper);
  float max_difference_db =
      RunOnConstantLevel(kNumIterations, kMaxSpeechProbability, kPeakLevelDbfs,
                         kSpeechLevelDbfs, *saturation_protector);
  max_difference_db = std::max(
      RunOnConstantLevel(kNumIterations, kMaxSpeechProbability, kPeakLevelDbfs,
                         kOtherSpeechLevelDbfs, *saturation_protector),
      max_difference_db);
  constexpr float kMaxChangeSpeedDbPerSecond = 0.5f;  // 1 db / 2 seconds.
  EXPECT_LE(max_difference_db,
            kMaxChangeSpeedDbPerSecond / 1000 * kFrameDurationMs);
}

class SaturationProtectorParametrization
    : public ::testing::TestWithParam<int> {
 protected:
  int adjacent_speech_frames_threshold() const { return GetParam(); }
};

TEST_P(SaturationProtectorParametrization, DoNotAdaptToShortSpeechSegments) {
  ApmDataDumper apm_data_dumper(0);
  auto saturation_protector = CreateSaturationProtector(
      kInitialHeadroomDb, kNoExtraHeadroomDb,
      adjacent_speech_frames_threshold(), &apm_data_dumper);
  const float initial_headroom_db = saturation_protector->HeadroomDb();
  RunOnConstantLevel(/*num_iterations=*/adjacent_speech_frames_threshold() - 1,
                     kMaxSpeechProbability,
                     /*peak_dbfs=*/0.0f,
                     /*speech_level_dbfs=*/-10.0f, *saturation_protector);
  // No adaptation expected.
  EXPECT_EQ(initial_headroom_db, saturation_protector->HeadroomDb());
}

TEST_P(SaturationProtectorParametrization, AdaptToEnoughSpeechSegments) {
  ApmDataDumper apm_data_dumper(0);
  auto saturation_protector = CreateSaturationProtector(
      kInitialHeadroomDb, kNoExtraHeadroomDb,
      adjacent_speech_frames_threshold(), &apm_data_dumper);
  const float initial_headroom_db = saturation_protector->HeadroomDb();
  RunOnConstantLevel(/*num_iterations=*/adjacent_speech_frames_threshold() + 1,
                     kMaxSpeechProbability,
                     /*peak_dbfs=*/0.0f,
                     /*speech_level_dbfs=*/-10.0f, *saturation_protector);
  // Adaptation expected.
  EXPECT_NE(initial_headroom_db, saturation_protector->HeadroomDb());
}

INSTANTIATE_TEST_SUITE_P(GainController2,
                         SaturationProtectorParametrization,
                         ::testing::Values(2, 9, 17));

}  // namespace
}  // namespace webrtc
