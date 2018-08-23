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
#include "common_audio/real_fourier.h"

#include <array>
#include <tuple>

#include "modules/audio_processing/agc2/rnn_vad/test_utils.h"
// TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
// #include "test/fpe_observer.h"
#include "test/gtest.h"

namespace webrtc {
namespace rnn_vad {
namespace test {
namespace {

constexpr std::array<size_t, 2> kTestPitchPeriods = {
    3 * kMinPitch48kHz / 2, (3 * kMinPitch48kHz + kMaxPitch48kHz) / 2,
};
constexpr std::array<float, 2> kTestPitchGains = {0.35f, 0.75f};

constexpr size_t kNumPitchBufSquareEnergies = 385;
constexpr size_t kNumPitchBufAutoCorrCoeffs = 147;
constexpr size_t kTestDataSize =
    kBufSize24kHz + kNumPitchBufSquareEnergies + kNumPitchBufAutoCorrCoeffs;

class TestData {
 public:
  TestData() {
    auto test_data_reader = CreatePitchSearchTestDataReader();
    test_data_reader->ReadChunk(test_data_);
  }
  rtc::ArrayView<const float, kBufSize24kHz> GetPitchBufView() {
    return {test_data_.data(), kBufSize24kHz};
  }
  rtc::ArrayView<const float, kNumPitchBufSquareEnergies>
  GetPitchBufSquareEnergiesView() {
    return {test_data_.data() + kBufSize24kHz, kNumPitchBufSquareEnergies};
  }
  rtc::ArrayView<const float, kNumPitchBufAutoCorrCoeffs>
  GetPitchBufAutoCorrCoeffsView() {
    return {test_data_.data() + kBufSize24kHz + kNumPitchBufSquareEnergies,
            kNumPitchBufAutoCorrCoeffs};
  }

 private:
  std::array<float, kTestDataSize> test_data_;
};

}  // namespace

class ComputePitchGainThresholdTest
    : public testing::Test,
      public ::testing::WithParamInterface<
          std::tuple<size_t, size_t, size_t, float, size_t, float, float>> {};

TEST_P(ComputePitchGainThresholdTest, BitExactness) {
  const auto params = GetParam();
  const size_t candidate_pitch_period = std::get<0>(params);
  const size_t pitch_period_ratio = std::get<1>(params);
  const size_t initial_pitch_period = std::get<2>(params);
  const float initial_pitch_gain = std::get<3>(params);
  const size_t prev_pitch_period = std::get<4>(params);
  const size_t prev_pitch_gain = std::get<5>(params);
  const float threshold = std::get<6>(params);

  {
    // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
    // FloatingPointExceptionObserver fpe_observer;

    EXPECT_NEAR(
        threshold,
        ComputePitchGainThreshold(candidate_pitch_period, pitch_period_ratio,
                                  initial_pitch_period, initial_pitch_gain,
                                  prev_pitch_period, prev_pitch_gain),
        3e-6f);
  }
}

INSTANTIATE_TEST_CASE_P(
    RnnVadTest,
    ComputePitchGainThresholdTest,
    ::testing::Values(
        std::make_tuple(31, 7, 219, 0.45649201f, 199, 0.604747f, 0.40000001f),
        std::make_tuple(113,
                        2,
                        226,
                        0.20967799f,
                        219,
                        0.40392199f,
                        0.30000001f),
        std::make_tuple(63, 2, 126, 0.210788f, 364, 0.098519f, 0.40000001f),
        std::make_tuple(30, 5, 152, 0.82356697f, 149, 0.55535901f, 0.700032f),
        std::make_tuple(76, 2, 151, 0.79522997f, 151, 0.82356697f, 0.675946f),
        std::make_tuple(31, 5, 153, 0.85069299f, 150, 0.79073799f, 0.72308898f),
        std::make_tuple(78, 2, 156, 0.72750503f, 153, 0.85069299f, 0.618379f)));

TEST(RnnVadTest, ComputeSlidingFrameSquareEnergiesBitExactness) {
  TestData test_data;
  std::array<float, kNumPitchBufSquareEnergies> computed_output;
  {
    // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
    // FloatingPointExceptionObserver fpe_observer;
    ComputeSlidingFrameSquareEnergies(test_data.GetPitchBufView(),
                                      computed_output);
  }
  auto square_energies_view = test_data.GetPitchBufSquareEnergiesView();
  ExpectNearAbsolute({square_energies_view.data(), square_energies_view.size()},
                     computed_output, 3e-2f);
}

TEST(RnnVadTest, ComputePitchAutoCorrelationBitExactness) {
  TestData test_data;
  std::array<float, kBufSize12kHz> pitch_buf_decimated;
  Decimate2x(test_data.GetPitchBufView(), pitch_buf_decimated);
  std::array<float, kNumPitchBufAutoCorrCoeffs> computed_output;
  {
    // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
    // FloatingPointExceptionObserver fpe_observer;
    std::unique_ptr<RealFourier> fft =
        RealFourier::Create(kAutoCorrelationFftOrder);
    ComputePitchAutoCorrelation(pitch_buf_decimated, kMaxPitch12kHz,
                                computed_output, fft.get());
  }
  auto auto_corr_view = test_data.GetPitchBufAutoCorrCoeffsView();
  ExpectNearAbsolute({auto_corr_view.data(), auto_corr_view.size()},
                     computed_output, 3e-3f);
}

// Check that the auto correlation function computes the right thing for a
// simple use case.
TEST(RnnVadTest, ComputePitchAutoCorrelationConstantBuffer) {
  // Create constant signal with no pitch.
  std::array<float, kBufSize12kHz> pitch_buf_decimated;
  std::fill(pitch_buf_decimated.begin(), pitch_buf_decimated.end(), 1.f);

  std::array<float, kNumPitchBufAutoCorrCoeffs> computed_output;
  {
    // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
    // FloatingPointExceptionObserver fpe_observer;
    std::unique_ptr<RealFourier> fft =
        RealFourier::Create(kAutoCorrelationFftOrder);
    ComputePitchAutoCorrelation(pitch_buf_decimated, kMaxPitch12kHz,
                                computed_output, fft.get());
  }

  // The expected output is constantly the length of the fixed 'x'
  // array in ComputePitchAutoCorrelation.
  std::array<float, kNumPitchBufAutoCorrCoeffs> expected_output;
  std::fill(expected_output.begin(), expected_output.end(),
            kBufSize12kHz - kMaxPitch12kHz);
  ExpectNearAbsolute(expected_output, computed_output, 4e-5f);
}

TEST(RnnVadTest, FindBestPitchPeriodsBitExactness) {
  TestData test_data;
  std::array<float, kBufSize12kHz> pitch_buf_decimated;
  Decimate2x(test_data.GetPitchBufView(), pitch_buf_decimated);
  std::array<size_t, 2> pitch_candidates_inv_lags;
  {
    // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
    // FloatingPointExceptionObserver fpe_observer;
    auto auto_corr_view = test_data.GetPitchBufAutoCorrCoeffsView();
    pitch_candidates_inv_lags =
        FindBestPitchPeriods({auto_corr_view.data(), auto_corr_view.size()},
                             pitch_buf_decimated, kMaxPitch12kHz);
  }
  const std::array<size_t, 2> expected_output = {140, 142};
  EXPECT_EQ(expected_output, pitch_candidates_inv_lags);
}

TEST(RnnVadTest, RefinePitchPeriod48kHzBitExactness) {
  TestData test_data;
  std::array<float, kBufSize12kHz> pitch_buf_decimated;
  Decimate2x(test_data.GetPitchBufView(), pitch_buf_decimated);
  size_t pitch_inv_lag;
  {
    // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
    // FloatingPointExceptionObserver fpe_observer;
    const std::array<size_t, 2> pitch_candidates_inv_lags = {280, 284};
    pitch_inv_lag = RefinePitchPeriod48kHz(test_data.GetPitchBufView(),
                                           pitch_candidates_inv_lags);
  }
  EXPECT_EQ(560u, pitch_inv_lag);
}

class CheckLowerPitchPeriodsAndComputePitchGainTest
    : public testing::Test,
      public ::testing::WithParamInterface<
          std::tuple<size_t, size_t, float, size_t, float>> {};

TEST_P(CheckLowerPitchPeriodsAndComputePitchGainTest, BitExactness) {
  const auto params = GetParam();
  const size_t initial_pitch_period = std::get<0>(params);
  const size_t prev_pitch_period = std::get<1>(params);
  const float prev_pitch_gain = std::get<2>(params);
  const size_t expected_pitch_period = std::get<3>(params);
  const float expected_pitch_gain = std::get<4>(params);
  TestData test_data;
  {
    // TODO(bugs.webrtc.org/8948): Add when the issue is fixed.
    // FloatingPointExceptionObserver fpe_observer;
    const auto computed_output = CheckLowerPitchPeriodsAndComputePitchGain(
        test_data.GetPitchBufView(), initial_pitch_period,
        {prev_pitch_period, prev_pitch_gain});
    EXPECT_EQ(expected_pitch_period, computed_output.period);
    EXPECT_NEAR(expected_pitch_gain, computed_output.gain, 1e-6f);
  }
}

INSTANTIATE_TEST_CASE_P(RnnVadTest,
                        CheckLowerPitchPeriodsAndComputePitchGainTest,
                        ::testing::Values(std::make_tuple(kTestPitchPeriods[0],
                                                          kTestPitchPeriods[0],
                                                          kTestPitchGains[0],
                                                          91,
                                                          -0.0188608f),
                                          std::make_tuple(kTestPitchPeriods[0],
                                                          kTestPitchPeriods[0],
                                                          kTestPitchGains[1],
                                                          91,
                                                          -0.0188608f),
                                          std::make_tuple(kTestPitchPeriods[0],
                                                          kTestPitchPeriods[1],
                                                          kTestPitchGains[0],
                                                          91,
                                                          -0.0188608f),
                                          std::make_tuple(kTestPitchPeriods[0],
                                                          kTestPitchPeriods[1],
                                                          kTestPitchGains[1],
                                                          91,
                                                          -0.0188608f),
                                          std::make_tuple(kTestPitchPeriods[1],
                                                          kTestPitchPeriods[0],
                                                          kTestPitchGains[0],
                                                          475,
                                                          -0.0904344f),
                                          std::make_tuple(kTestPitchPeriods[1],
                                                          kTestPitchPeriods[0],
                                                          kTestPitchGains[1],
                                                          475,
                                                          -0.0904344f),
                                          std::make_tuple(kTestPitchPeriods[1],
                                                          kTestPitchPeriods[1],
                                                          kTestPitchGains[0],
                                                          475,
                                                          -0.0904344f),
                                          std::make_tuple(kTestPitchPeriods[1],
                                                          kTestPitchPeriods[1],
                                                          kTestPitchGains[1],
                                                          475,
                                                          -0.0904344f)));

}  // namespace test
}  // namespace rnn_vad
}  // namespace webrtc
