/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/erl_estimator.h"

#include "rtc_base/strings/string_builder.h"
#include "test/gtest.h"

namespace webrtc {

namespace {
std::string ProduceDebugText(size_t num_render_channels,
                             size_t num_capture_channels) {
  rtc::StringBuilder ss;
  ss << "Render channels: " << num_render_channels;
  ss << ", Capture channels: " << num_capture_channels;
  return ss.Release();
}

void VerifyErl(const std::array<float, kFftLengthBy2Plus1>& erl,
               float erl_time_domain,
               float reference) {
  std::for_each(erl.begin(), erl.end(),
                [reference](float a) { EXPECT_NEAR(reference, a, 0.001); });
  EXPECT_NEAR(reference, erl_time_domain, 0.001);
}

}  // namespace

class ErlEstimatorMultiChannel
    : public ::testing::Test,
      public ::testing::WithParamInterface<std::tuple<size_t, size_t>> {};

INSTANTIATE_TEST_SUITE_P(MultiChannel,
                         ErlEstimatorMultiChannel,
                         ::testing::Combine(::testing::Values(1, 2, 8),
                                            ::testing::Values(1, 2, 8)));

// Verifies that the correct ERL estimates are achieved.
TEST_P(ErlEstimatorMultiChannel, Estimates) {
  const size_t num_render_channels = std::get<0>(GetParam());
  const size_t num_capture_channels = std::get<1>(GetParam());
  SCOPED_TRACE(ProduceDebugText(num_render_channels, num_capture_channels));
  std::vector<std::array<float, kFftLengthBy2Plus1>> X2(num_render_channels);
  for (auto& X2_ch : X2) {
    X2_ch.fill(0.f);
  }
  std::vector<std::array<float, kFftLengthBy2Plus1>> Y2(num_capture_channels);
  for (auto& Y2_ch : Y2) {
    Y2_ch.fill(0.f);
  }
  std::vector<bool> converged_filters(num_capture_channels, false);
  const size_t converged_idx = num_capture_channels - 1;
  converged_filters[converged_idx] = true;

  ErlEstimator estimator(0);

  // Verifies that the ERL estimate is properly reduced to lower values.
  for (auto& X2_ch : X2) {
    X2_ch.fill(500 * 1000.f * 1000.f);
  }
  Y2[converged_idx].fill(10 * X2[0][0]);
  for (size_t k = 0; k < 200; ++k) {
    estimator.Update(converged_filters, X2, Y2);
  }
  VerifyErl(estimator.Erl(), estimator.ErlTimeDomain(), 10.f);

  // Verifies that the ERL is not immediately increased when the ERL in the
  // data increases.
  Y2[converged_idx].fill(10000 * X2[0][0]);
  for (size_t k = 0; k < 998; ++k) {
    estimator.Update(converged_filters, X2, Y2);
  }
  VerifyErl(estimator.Erl(), estimator.ErlTimeDomain(), 10.f);

  // Verifies that the rate of increase is 3 dB.
  estimator.Update(converged_filters, X2, Y2);
  VerifyErl(estimator.Erl(), estimator.ErlTimeDomain(), 20.f);

  // Verifies that the maximum ERL is achieved when there are no low RLE
  // estimates.
  for (size_t k = 0; k < 1000; ++k) {
    estimator.Update(converged_filters, X2, Y2);
  }
  VerifyErl(estimator.Erl(), estimator.ErlTimeDomain(), 1000.f);

  // Verifies that the ERL estimate is is not updated for low-level signals
  for (auto& X2_ch : X2) {
    X2_ch.fill(1000.f * 1000.f);
  }
  Y2[converged_idx].fill(10 * X2[0][0]);
  for (size_t k = 0; k < 200; ++k) {
    estimator.Update(converged_filters, X2, Y2);
  }
  VerifyErl(estimator.Erl(), estimator.ErlTimeDomain(), 1000.f);
}
}  // namespace webrtc
