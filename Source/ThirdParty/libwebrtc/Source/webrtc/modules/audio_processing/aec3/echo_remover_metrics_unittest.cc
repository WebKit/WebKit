/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/echo_remover_metrics.h"

#include <math.h>

#include "modules/audio_processing/aec3/aec3_fft.h"
#include "modules/audio_processing/aec3/aec_state.h"
#include "test/gtest.h"

namespace webrtc {

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies the check for non-null input.
TEST(UpdateDbMetric, NullValue) {
  std::array<float, kFftLengthBy2Plus1> value;
  value.fill(0.f);
  EXPECT_DEATH(aec3::UpdateDbMetric(value, nullptr), "");
}

#endif

// Verifies the updating functionality of UpdateDbMetric.
TEST(UpdateDbMetric, Updating) {
  std::array<float, kFftLengthBy2Plus1> value;
  std::array<EchoRemoverMetrics::DbMetric, 2> statistic;
  statistic.fill(EchoRemoverMetrics::DbMetric(0.f, 100.f, -100.f));
  constexpr float kValue0 = 10.f;
  constexpr float kValue1 = 20.f;
  std::fill(value.begin(), value.begin() + 32, kValue0);
  std::fill(value.begin() + 32, value.begin() + 64, kValue1);

  aec3::UpdateDbMetric(value, &statistic);
  EXPECT_FLOAT_EQ(kValue0, statistic[0].sum_value);
  EXPECT_FLOAT_EQ(kValue0, statistic[0].ceil_value);
  EXPECT_FLOAT_EQ(kValue0, statistic[0].floor_value);
  EXPECT_FLOAT_EQ(kValue1, statistic[1].sum_value);
  EXPECT_FLOAT_EQ(kValue1, statistic[1].ceil_value);
  EXPECT_FLOAT_EQ(kValue1, statistic[1].floor_value);

  aec3::UpdateDbMetric(value, &statistic);
  EXPECT_FLOAT_EQ(2.f * kValue0, statistic[0].sum_value);
  EXPECT_FLOAT_EQ(kValue0, statistic[0].ceil_value);
  EXPECT_FLOAT_EQ(kValue0, statistic[0].floor_value);
  EXPECT_FLOAT_EQ(2.f * kValue1, statistic[1].sum_value);
  EXPECT_FLOAT_EQ(kValue1, statistic[1].ceil_value);
  EXPECT_FLOAT_EQ(kValue1, statistic[1].floor_value);
}

// Verifies that the TransformDbMetricForReporting method produces the desired
// output for values for dBFS.
TEST(TransformDbMetricForReporting, DbFsScaling) {
  std::array<float, kBlockSize> x;
  FftData X;
  std::array<float, kFftLengthBy2Plus1> X2;
  Aec3Fft fft;
  x.fill(1000.f);
  fft.ZeroPaddedFft(x, Aec3Fft::Window::kRectangular, &X);
  X.Spectrum(Aec3Optimization::kNone, X2);

  float offset = -10.f * log10(32768.f * 32768.f);
  EXPECT_NEAR(offset, -90.3f, 0.1f);
  EXPECT_EQ(
      static_cast<int>(30.3f),
      aec3::TransformDbMetricForReporting(
          true, 0.f, 90.f, offset, 1.f / (kBlockSize * kBlockSize), X2[0]));
}

// Verifies that the TransformDbMetricForReporting method is able to properly
// limit the output.
TEST(TransformDbMetricForReporting, Limits) {
  EXPECT_EQ(0, aec3::TransformDbMetricForReporting(false, 0.f, 10.f, 0.f, 1.f,
                                                   0.001f));
  EXPECT_EQ(10, aec3::TransformDbMetricForReporting(false, 0.f, 10.f, 0.f, 1.f,
                                                    100.f));
}

// Verifies that the TransformDbMetricForReporting method is able to properly
// negate output.
TEST(TransformDbMetricForReporting, Negate) {
  EXPECT_EQ(10, aec3::TransformDbMetricForReporting(true, -20.f, 20.f, 0.f, 1.f,
                                                    0.1f));
  EXPECT_EQ(-10, aec3::TransformDbMetricForReporting(true, -20.f, 20.f, 0.f,
                                                     1.f, 10.f));
}

// Verify the Update functionality of DbMetric.
TEST(DbMetric, Update) {
  EchoRemoverMetrics::DbMetric metric(0.f, 20.f, -20.f);
  constexpr int kNumValues = 100;
  constexpr float kValue = 10.f;
  for (int k = 0; k < kNumValues; ++k) {
    metric.Update(kValue);
  }
  EXPECT_FLOAT_EQ(kValue * kNumValues, metric.sum_value);
  EXPECT_FLOAT_EQ(kValue, metric.ceil_value);
  EXPECT_FLOAT_EQ(kValue, metric.floor_value);
}

// Verify the Update functionality of DbMetric.
TEST(DbMetric, UpdateInstant) {
  EchoRemoverMetrics::DbMetric metric(0.f, 20.f, -20.f);
  constexpr float kMinValue = -77.f;
  constexpr float kMaxValue = 33.f;
  constexpr float kLastValue = (kMinValue + kMaxValue) / 2.0f;
  for (float value = kMinValue; value <= kMaxValue; value++)
    metric.UpdateInstant(value);
  metric.UpdateInstant(kLastValue);
  EXPECT_FLOAT_EQ(kLastValue, metric.sum_value);
  EXPECT_FLOAT_EQ(kMaxValue, metric.ceil_value);
  EXPECT_FLOAT_EQ(kMinValue, metric.floor_value);
}

// Verify the constructor functionality of DbMetric.
TEST(DbMetric, Constructor) {
  EchoRemoverMetrics::DbMetric metric;
  EXPECT_FLOAT_EQ(0.f, metric.sum_value);
  EXPECT_FLOAT_EQ(0.f, metric.ceil_value);
  EXPECT_FLOAT_EQ(0.f, metric.floor_value);

  metric = EchoRemoverMetrics::DbMetric(1.f, 2.f, 3.f);
  EXPECT_FLOAT_EQ(1.f, metric.sum_value);
  EXPECT_FLOAT_EQ(2.f, metric.floor_value);
  EXPECT_FLOAT_EQ(3.f, metric.ceil_value);
}

// Verify the general functionality of EchoRemoverMetrics.
TEST(EchoRemoverMetrics, NormalUsage) {
  EchoRemoverMetrics metrics;
  AecState aec_state(EchoCanceller3Config{});
  std::array<float, kFftLengthBy2Plus1> comfort_noise_spectrum;
  std::array<float, kFftLengthBy2Plus1> suppressor_gain;
  comfort_noise_spectrum.fill(10.f);
  suppressor_gain.fill(1.f);
  for (int j = 0; j < 3; ++j) {
    for (int k = 0; k < kMetricsReportingIntervalBlocks - 1; ++k) {
      metrics.Update(aec_state, comfort_noise_spectrum, suppressor_gain);
      EXPECT_FALSE(metrics.MetricsReported());
    }
    metrics.Update(aec_state, comfort_noise_spectrum, suppressor_gain);
    EXPECT_TRUE(metrics.MetricsReported());
  }
}

}  // namespace webrtc
