/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/aec3/matched_filter_lag_aggregator.h"

#include <sstream>
#include <string>
#include <vector>

#include "api/array_view.h"
#include "api/audio/echo_canceller3_config.h"
#include "modules/audio_processing/aec3/aec3_common.h"
#include "modules/audio_processing/logging/apm_data_dumper.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr size_t kNumLagsBeforeDetection = 26;

}  // namespace

// Verifies that varying lag estimates causes lag estimates to not be deemed
// reliable.
TEST(MatchedFilterLagAggregator,
     LagEstimateInvarianceRequiredForAggregatedLag) {
  ApmDataDumper data_dumper(0);
  EchoCanceller3Config config;
  MatchedFilterLagAggregator aggregator(&data_dumper, /*max_filter_lag=*/100,
                                        config.delay);

  std::optional<DelayEstimate> aggregated_lag;
  for (size_t k = 0; k < kNumLagsBeforeDetection; ++k) {
    aggregated_lag = aggregator.Aggregate(
        MatchedFilter::LagEstimate(/*lag=*/10, /*pre_echo_lag=*/10));
  }
  EXPECT_TRUE(aggregated_lag);

  for (size_t k = 0; k < kNumLagsBeforeDetection * 100; ++k) {
    aggregated_lag = aggregator.Aggregate(
        MatchedFilter::LagEstimate(/*lag=*/k % 100, /*pre_echo_lag=*/k % 100));
  }
  EXPECT_FALSE(aggregated_lag);

  for (size_t k = 0; k < kNumLagsBeforeDetection * 100; ++k) {
    aggregated_lag = aggregator.Aggregate(
        MatchedFilter::LagEstimate(/*lag=*/k % 100, /*pre_echo_lag=*/k % 100));
    EXPECT_FALSE(aggregated_lag);
  }
}

// Verifies that lag estimate updates are required to produce an updated lag
// aggregate.
TEST(MatchedFilterLagAggregator,
     DISABLED_LagEstimateUpdatesRequiredForAggregatedLag) {
  constexpr size_t kLag = 5;
  ApmDataDumper data_dumper(0);
  EchoCanceller3Config config;
  MatchedFilterLagAggregator aggregator(&data_dumper, /*max_filter_lag=*/kLag,
                                        config.delay);
  for (size_t k = 0; k < kNumLagsBeforeDetection * 10; ++k) {
    std::optional<DelayEstimate> aggregated_lag = aggregator.Aggregate(
        MatchedFilter::LagEstimate(/*lag=*/kLag, /*pre_echo_lag=*/kLag));
    EXPECT_FALSE(aggregated_lag);
    EXPECT_EQ(kLag, aggregated_lag->delay);
  }
}

// Verifies that an aggregated lag is persistent if the lag estimates do not
// change and that an aggregated lag is not produced without gaining lag
// estimate confidence.
TEST(MatchedFilterLagAggregator, DISABLED_PersistentAggregatedLag) {
  constexpr size_t kLag1 = 5;
  constexpr size_t kLag2 = 10;
  ApmDataDumper data_dumper(0);
  EchoCanceller3Config config;
  std::vector<MatchedFilter::LagEstimate> lag_estimates(1);
  MatchedFilterLagAggregator aggregator(&data_dumper, std::max(kLag1, kLag2),
                                        config.delay);
  std::optional<DelayEstimate> aggregated_lag;
  for (size_t k = 0; k < kNumLagsBeforeDetection; ++k) {
    aggregated_lag = aggregator.Aggregate(
        MatchedFilter::LagEstimate(/*lag=*/kLag1, /*pre_echo_lag=*/kLag1));
  }
  EXPECT_TRUE(aggregated_lag);
  EXPECT_EQ(kLag1, aggregated_lag->delay);

  for (size_t k = 0; k < kNumLagsBeforeDetection * 40; ++k) {
    aggregated_lag = aggregator.Aggregate(
        MatchedFilter::LagEstimate(/*lag=*/kLag2, /*pre_echo_lag=*/kLag2));
    EXPECT_TRUE(aggregated_lag);
    EXPECT_EQ(kLag1, aggregated_lag->delay);
  }
}

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies the check for non-null data dumper.
TEST(MatchedFilterLagAggregatorDeathTest, NullDataDumper) {
  EchoCanceller3Config config;
  EXPECT_DEATH(MatchedFilterLagAggregator(nullptr, 10, config.delay), "");
}

#endif

}  // namespace webrtc
