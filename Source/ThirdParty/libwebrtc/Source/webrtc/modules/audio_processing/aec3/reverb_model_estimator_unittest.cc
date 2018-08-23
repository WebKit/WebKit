/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/reverb_model_estimator.h"

#include <algorithm>
#include <cmath>
#include <numeric>

#include "absl/types/optional.h"
#include "api/array_view.h"
#include "api/audio/echo_canceller3_config.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/aec3/aec3_fft.h"
#include "modules/audio_processing/aec3/fft_data.h"
#include "rtc_base/checks.h"

#include "test/gtest.h"

namespace webrtc {

class ReverbModelEstimatorTest {
 public:
  explicit ReverbModelEstimatorTest(float default_decay)
      : default_decay_(default_decay), estimated_decay_(default_decay) {
    aec3_config_.ep_strength.default_len = default_decay_;
    aec3_config_.filter.main.length_blocks = 40;
    h_.resize(aec3_config_.filter.main.length_blocks * kBlockSize);
    H2_.resize(aec3_config_.filter.main.length_blocks);
    CreateImpulseResponseWithDecay();
  }
  void RunEstimator();
  float GetDecay() { return estimated_decay_; }
  float GetTrueDecay() { return kTruePowerDecay; }
  float GetPowerTailDb() { return 10.f * log10(estimated_power_tail_); }
  float GetTruePowerTailDb() { return 10.f * log10(true_power_tail_); }

 private:
  void CreateImpulseResponseWithDecay();

  absl::optional<float> quality_linear_ = 1.0f;
  static constexpr int kFilterDelayBlocks = 2;
  static constexpr bool kUsableLinearEstimate = true;
  static constexpr bool kStationaryBlock = false;
  static constexpr float kTruePowerDecay = 0.5f;
  EchoCanceller3Config aec3_config_;
  float default_decay_;
  float estimated_decay_;
  float estimated_power_tail_ = 0.f;
  float true_power_tail_ = 0.f;
  std::vector<float> h_;
  std::vector<std::array<float, kFftLengthBy2Plus1>> H2_;
};

void ReverbModelEstimatorTest::CreateImpulseResponseWithDecay() {
  const Aec3Fft fft;
  RTC_DCHECK_EQ(h_.size(), aec3_config_.filter.main.length_blocks * kBlockSize);
  RTC_DCHECK_EQ(H2_.size(), aec3_config_.filter.main.length_blocks);
  RTC_DCHECK_EQ(kFilterDelayBlocks, 2);

  float decay_sample = std::sqrt(powf(kTruePowerDecay, 1.f / kBlockSize));
  const size_t filter_delay_coefficients = kFilterDelayBlocks * kBlockSize;
  std::fill(h_.begin(), h_.end(), 0.f);
  h_[filter_delay_coefficients] = 1.f;
  for (size_t k = filter_delay_coefficients + 1; k < h_.size(); ++k) {
    h_[k] = h_[k - 1] * decay_sample;
  }

  std::array<float, kFftLength> fft_data;
  FftData H_j;
  for (size_t j = 0, k = 0; j < H2_.size(); ++j, k += kBlockSize) {
    fft_data.fill(0.f);
    std::copy(h_.begin() + k, h_.begin() + k + kBlockSize, fft_data.begin());
    fft.Fft(&fft_data, &H_j);
    H_j.Spectrum(Aec3Optimization::kNone, H2_[j]);
  }
  rtc::ArrayView<float> H2_tail(H2_[H2_.size() - 1]);
  true_power_tail_ = std::accumulate(H2_tail.begin(), H2_tail.end(), 0.f);
}
void ReverbModelEstimatorTest::RunEstimator() {
  ReverbModelEstimator estimator(aec3_config_);
  for (size_t k = 0; k < 3000; ++k) {
    estimator.Update(h_, H2_, quality_linear_, kFilterDelayBlocks,
                     kUsableLinearEstimate, kStationaryBlock);
  }
  estimated_decay_ = estimator.ReverbDecay();
  auto freq_resp_tail = estimator.GetReverbFrequencyResponse();
  estimated_power_tail_ =
      std::accumulate(freq_resp_tail.begin(), freq_resp_tail.end(), 0.f);
}

TEST(ReverbModelEstimatorTests, NotChangingDecay) {
  constexpr float default_decay = 0.9f;
  ReverbModelEstimatorTest test(default_decay);
  test.RunEstimator();
  EXPECT_EQ(test.GetDecay(), default_decay);
  EXPECT_NEAR(test.GetPowerTailDb(), test.GetTruePowerTailDb(), 5.f);
}

TEST(ReverbModelEstimatorTests, ChangingDecay) {
  constexpr float default_decay = -0.9f;
  ReverbModelEstimatorTest test(default_decay);
  test.RunEstimator();
  EXPECT_NEAR(test.GetDecay(), test.GetTrueDecay(), 0.1);
  EXPECT_NEAR(test.GetPowerTailDb(), test.GetTruePowerTailDb(), 5.f);
}

}  // namespace webrtc
