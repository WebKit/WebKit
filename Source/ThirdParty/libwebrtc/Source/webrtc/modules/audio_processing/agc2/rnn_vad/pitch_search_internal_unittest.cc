/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/rnn_vad/pitch_search_internal.h"

#include <array>
#include <string>
#include <tuple>

#include "modules/audio_processing/agc2/rnn_vad/test_utils.h"
#include "rtc_base/strings/string_builder.h"
// TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
// #include "test/fpe_observer.h"
#include "test/gtest.h"

namespace webrtc {
namespace rnn_vad {
namespace {

constexpr int kTestPitchPeriodsLow = 3 * kMinPitch48kHz / 2;
constexpr int kTestPitchPeriodsHigh = (3 * kMinPitch48kHz + kMaxPitch48kHz) / 2;

constexpr float kTestPitchStrengthLow = 0.35f;
constexpr float kTestPitchStrengthHigh = 0.75f;

template <class T>
std::string PrintTestIndexAndCpuFeatures(
    const ::testing::TestParamInfo<T>& info) {
  rtc::StringBuilder builder;
  builder << info.index << "_" << info.param.cpu_features.ToString();
  return builder.str();
}

// Finds the relevant CPU features combinations to test.
std::vector<AvailableCpuFeatures> GetCpuFeaturesToTest() {
  std::vector<AvailableCpuFeatures> v;
  v.push_back(NoAvailableCpuFeatures());
  AvailableCpuFeatures available = GetAvailableCpuFeatures();
  if (available.avx2) {
    v.push_back({/*sse2=*/false, /*avx2=*/true, /*neon=*/false});
  }
  if (available.sse2) {
    v.push_back({/*sse2=*/true, /*avx2=*/false, /*neon=*/false});
  }
  return v;
}

// Checks that the frame-wise sliding square energy function produces output
// within tolerance given test input data.
TEST(RnnVadTest, ComputeSlidingFrameSquareEnergies24kHzWithinTolerance) {
  const AvailableCpuFeatures cpu_features = GetAvailableCpuFeatures();

  PitchTestData test_data;
  std::array<float, kRefineNumLags24kHz> computed_output;
  // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
  // FloatingPointExceptionObserver fpe_observer;
  ComputeSlidingFrameSquareEnergies24kHz(test_data.PitchBuffer24kHzView(),
                                         computed_output, cpu_features);
  auto square_energies_view = test_data.SquareEnergies24kHzView();
  ExpectNearAbsolute({square_energies_view.data(), square_energies_view.size()},
                     computed_output, 1e-3f);
}

// Checks that the estimated pitch period is bit-exact given test input data.
TEST(RnnVadTest, ComputePitchPeriod12kHzBitExactness) {
  const AvailableCpuFeatures cpu_features = GetAvailableCpuFeatures();

  PitchTestData test_data;
  std::array<float, kBufSize12kHz> pitch_buf_decimated;
  Decimate2x(test_data.PitchBuffer24kHzView(), pitch_buf_decimated);
  CandidatePitchPeriods pitch_candidates;
  // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
  // FloatingPointExceptionObserver fpe_observer;
  pitch_candidates = ComputePitchPeriod12kHz(
      pitch_buf_decimated, test_data.AutoCorrelation12kHzView(), cpu_features);
  EXPECT_EQ(pitch_candidates.best, 140);
  EXPECT_EQ(pitch_candidates.second_best, 142);
}

// Checks that the refined pitch period is bit-exact given test input data.
TEST(RnnVadTest, ComputePitchPeriod48kHzBitExactness) {
  const AvailableCpuFeatures cpu_features = GetAvailableCpuFeatures();

  PitchTestData test_data;
  std::vector<float> y_energy(kRefineNumLags24kHz);
  rtc::ArrayView<float, kRefineNumLags24kHz> y_energy_view(y_energy.data(),
                                                           kRefineNumLags24kHz);
  ComputeSlidingFrameSquareEnergies24kHz(test_data.PitchBuffer24kHzView(),
                                         y_energy_view, cpu_features);
  // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
  // FloatingPointExceptionObserver fpe_observer;
  EXPECT_EQ(
      ComputePitchPeriod48kHz(test_data.PitchBuffer24kHzView(), y_energy_view,
                              /*pitch_candidates=*/{280, 284}, cpu_features),
      560);
  EXPECT_EQ(
      ComputePitchPeriod48kHz(test_data.PitchBuffer24kHzView(), y_energy_view,
                              /*pitch_candidates=*/{260, 284}, cpu_features),
      568);
}

struct PitchCandidatesParameters {
  CandidatePitchPeriods pitch_candidates;
  AvailableCpuFeatures cpu_features;
};

class PitchCandidatesParametrization
    : public ::testing::TestWithParam<PitchCandidatesParameters> {};

// Checks that the result of `ComputePitchPeriod48kHz()` does not depend on the
// order of the input pitch candidates.
TEST_P(PitchCandidatesParametrization,
       ComputePitchPeriod48kHzOrderDoesNotMatter) {
  const PitchCandidatesParameters params = GetParam();
  const CandidatePitchPeriods swapped_pitch_candidates{
      params.pitch_candidates.second_best, params.pitch_candidates.best};

  PitchTestData test_data;
  std::vector<float> y_energy(kRefineNumLags24kHz);
  rtc::ArrayView<float, kRefineNumLags24kHz> y_energy_view(y_energy.data(),
                                                           kRefineNumLags24kHz);
  ComputeSlidingFrameSquareEnergies24kHz(test_data.PitchBuffer24kHzView(),
                                         y_energy_view, params.cpu_features);
  EXPECT_EQ(
      ComputePitchPeriod48kHz(test_data.PitchBuffer24kHzView(), y_energy_view,
                              params.pitch_candidates, params.cpu_features),
      ComputePitchPeriod48kHz(test_data.PitchBuffer24kHzView(), y_energy_view,
                              swapped_pitch_candidates, params.cpu_features));
}

std::vector<PitchCandidatesParameters> CreatePitchCandidatesParameters() {
  std::vector<PitchCandidatesParameters> v;
  for (AvailableCpuFeatures cpu_features : GetCpuFeaturesToTest()) {
    v.push_back({{0, 2}, cpu_features});
    v.push_back({{260, 284}, cpu_features});
    v.push_back({{280, 284}, cpu_features});
    v.push_back(
        {{kInitialNumLags24kHz - 2, kInitialNumLags24kHz - 1}, cpu_features});
  }
  return v;
}

INSTANTIATE_TEST_SUITE_P(
    RnnVadTest,
    PitchCandidatesParametrization,
    ::testing::ValuesIn(CreatePitchCandidatesParameters()),
    PrintTestIndexAndCpuFeatures<PitchCandidatesParameters>);

struct ExtendedPitchPeriodSearchParameters {
  int initial_pitch_period;
  PitchInfo last_pitch;
  PitchInfo expected_pitch;
  AvailableCpuFeatures cpu_features;
};

class ExtendedPitchPeriodSearchParametrizaion
    : public ::testing::TestWithParam<ExtendedPitchPeriodSearchParameters> {};

// Checks that the computed pitch period is bit-exact and that the computed
// pitch strength is within tolerance given test input data.
TEST_P(ExtendedPitchPeriodSearchParametrizaion,
       PeriodBitExactnessGainWithinTolerance) {
  const ExtendedPitchPeriodSearchParameters params = GetParam();

  PitchTestData test_data;
  std::vector<float> y_energy(kRefineNumLags24kHz);
  rtc::ArrayView<float, kRefineNumLags24kHz> y_energy_view(y_energy.data(),
                                                           kRefineNumLags24kHz);
  ComputeSlidingFrameSquareEnergies24kHz(test_data.PitchBuffer24kHzView(),
                                         y_energy_view, params.cpu_features);
  // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
  // FloatingPointExceptionObserver fpe_observer;
  const auto computed_output = ComputeExtendedPitchPeriod48kHz(
      test_data.PitchBuffer24kHzView(), y_energy_view,
      params.initial_pitch_period, params.last_pitch, params.cpu_features);
  EXPECT_EQ(params.expected_pitch.period, computed_output.period);
  EXPECT_NEAR(params.expected_pitch.strength, computed_output.strength, 1e-6f);
}

std::vector<ExtendedPitchPeriodSearchParameters>
CreateExtendedPitchPeriodSearchParameters() {
  std::vector<ExtendedPitchPeriodSearchParameters> v;
  for (AvailableCpuFeatures cpu_features : GetCpuFeaturesToTest()) {
    for (int last_pitch_period :
         {kTestPitchPeriodsLow, kTestPitchPeriodsHigh}) {
      for (float last_pitch_strength :
           {kTestPitchStrengthLow, kTestPitchStrengthHigh}) {
        v.push_back({kTestPitchPeriodsLow,
                     {last_pitch_period, last_pitch_strength},
                     {91, -0.0188608f},
                     cpu_features});
        v.push_back({kTestPitchPeriodsHigh,
                     {last_pitch_period, last_pitch_strength},
                     {475, -0.0904344f},
                     cpu_features});
      }
    }
  }
  return v;
}

INSTANTIATE_TEST_SUITE_P(
    RnnVadTest,
    ExtendedPitchPeriodSearchParametrizaion,
    ::testing::ValuesIn(CreateExtendedPitchPeriodSearchParameters()),
    PrintTestIndexAndCpuFeatures<ExtendedPitchPeriodSearchParameters>);

}  // namespace
}  // namespace rnn_vad
}  // namespace webrtc
