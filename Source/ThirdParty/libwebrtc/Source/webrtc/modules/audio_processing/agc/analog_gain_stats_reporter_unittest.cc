/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc/analog_gain_stats_reporter.h"

#include "system_wrappers/include/metrics.h"
#include "test/gmock.h"

namespace webrtc {
namespace {

constexpr int kFramesIn60Seconds = 6000;

class AnalogGainStatsReporterTest : public ::testing::Test {
 public:
  AnalogGainStatsReporterTest() {}

 protected:
  void SetUp() override { metrics::Reset(); }
};

TEST_F(AnalogGainStatsReporterTest, CheckLogLevelUpdateStatsEmpty) {
  AnalogGainStatsReporter stats_reporter;
  constexpr int kMicLevel = 10;
  stats_reporter.UpdateStatistics(kMicLevel);
  // Update almost until the periodic logging and reset.
  for (int i = 0; i < kFramesIn60Seconds - 2; i += 2) {
    stats_reporter.UpdateStatistics(kMicLevel + 2);
    stats_reporter.UpdateStatistics(kMicLevel);
  }
  EXPECT_METRIC_THAT(metrics::Samples("WebRTC.Audio.ApmAnalogGainUpdateRate"),
                     ::testing::ElementsAre());
  EXPECT_METRIC_THAT(metrics::Samples("WebRTC.Audio.ApmAnalogGainDecreaseRate"),
                     ::testing::ElementsAre());
  EXPECT_METRIC_THAT(metrics::Samples("WebRTC.Audio.ApmAnalogGainIncreaseRate"),
                     ::testing::ElementsAre());
  EXPECT_METRIC_THAT(
      metrics::Samples("WebRTC.Audio.ApmAnalogGainUpdateAverage"),
      ::testing::ElementsAre());
  EXPECT_METRIC_THAT(
      metrics::Samples("WebRTC.Audio.ApmAnalogGainDecreaseAverage"),
      ::testing::ElementsAre());
  EXPECT_METRIC_THAT(
      metrics::Samples("WebRTC.Audio.ApmAnalogGainIncreaseAverage"),
      ::testing::ElementsAre());
}

TEST_F(AnalogGainStatsReporterTest, CheckLogLevelUpdateStatsNotEmpty) {
  AnalogGainStatsReporter stats_reporter;
  constexpr int kMicLevel = 10;
  stats_reporter.UpdateStatistics(kMicLevel);
  // Update until periodic logging.
  for (int i = 0; i < kFramesIn60Seconds; i += 2) {
    stats_reporter.UpdateStatistics(kMicLevel + 2);
    stats_reporter.UpdateStatistics(kMicLevel);
  }
  // Update until periodic logging.
  for (int i = 0; i < kFramesIn60Seconds; i += 2) {
    stats_reporter.UpdateStatistics(kMicLevel + 3);
    stats_reporter.UpdateStatistics(kMicLevel);
  }
  EXPECT_METRIC_THAT(
      metrics::Samples("WebRTC.Audio.ApmAnalogGainUpdateRate"),
      ::testing::ElementsAre(::testing::Pair(kFramesIn60Seconds - 1, 1),
                             ::testing::Pair(kFramesIn60Seconds, 1)));
  EXPECT_METRIC_THAT(
      metrics::Samples("WebRTC.Audio.ApmAnalogGainDecreaseRate"),
      ::testing::ElementsAre(::testing::Pair(kFramesIn60Seconds / 2 - 1, 1),
                             ::testing::Pair(kFramesIn60Seconds / 2, 1)));
  EXPECT_METRIC_THAT(
      metrics::Samples("WebRTC.Audio.ApmAnalogGainIncreaseRate"),
      ::testing::ElementsAre(::testing::Pair(kFramesIn60Seconds / 2, 2)));
  EXPECT_METRIC_THAT(
      metrics::Samples("WebRTC.Audio.ApmAnalogGainUpdateAverage"),
      ::testing::ElementsAre(::testing::Pair(2, 1), ::testing::Pair(3, 1)));
  EXPECT_METRIC_THAT(
      metrics::Samples("WebRTC.Audio.ApmAnalogGainDecreaseAverage"),
      ::testing::ElementsAre(::testing::Pair(2, 1), ::testing::Pair(3, 1)));
  EXPECT_METRIC_THAT(
      metrics::Samples("WebRTC.Audio.ApmAnalogGainIncreaseAverage"),
      ::testing::ElementsAre(::testing::Pair(2, 1), ::testing::Pair(3, 1)));
}
}  // namespace

TEST_F(AnalogGainStatsReporterTest, CheckLevelUpdateStatsForEmptyStats) {
  AnalogGainStatsReporter stats_reporter;
  const auto& update_stats = stats_reporter.level_update_stats();
  EXPECT_EQ(update_stats.num_decreases, 0);
  EXPECT_EQ(update_stats.sum_decreases, 0);
  EXPECT_EQ(update_stats.num_increases, 0);
  EXPECT_EQ(update_stats.sum_increases, 0);
}

TEST_F(AnalogGainStatsReporterTest, CheckLevelUpdateStatsAfterNoGainChange) {
  constexpr int kMicLevel = 10;
  AnalogGainStatsReporter stats_reporter;
  stats_reporter.UpdateStatistics(kMicLevel);
  stats_reporter.UpdateStatistics(kMicLevel);
  stats_reporter.UpdateStatistics(kMicLevel);
  const auto& update_stats = stats_reporter.level_update_stats();
  EXPECT_EQ(update_stats.num_decreases, 0);
  EXPECT_EQ(update_stats.sum_decreases, 0);
  EXPECT_EQ(update_stats.num_increases, 0);
  EXPECT_EQ(update_stats.sum_increases, 0);
}

TEST_F(AnalogGainStatsReporterTest, CheckLevelUpdateStatsAfterGainIncrease) {
  constexpr int kMicLevel = 10;
  AnalogGainStatsReporter stats_reporter;
  stats_reporter.UpdateStatistics(kMicLevel);
  stats_reporter.UpdateStatistics(kMicLevel + 4);
  stats_reporter.UpdateStatistics(kMicLevel + 5);
  const auto& update_stats = stats_reporter.level_update_stats();
  EXPECT_EQ(update_stats.num_decreases, 0);
  EXPECT_EQ(update_stats.sum_decreases, 0);
  EXPECT_EQ(update_stats.num_increases, 2);
  EXPECT_EQ(update_stats.sum_increases, 5);
}

TEST_F(AnalogGainStatsReporterTest, CheckLevelUpdateStatsAfterGainDecrease) {
  constexpr int kMicLevel = 10;
  AnalogGainStatsReporter stats_reporter;
  stats_reporter.UpdateStatistics(kMicLevel);
  stats_reporter.UpdateStatistics(kMicLevel - 4);
  stats_reporter.UpdateStatistics(kMicLevel - 5);
  const auto& stats_update = stats_reporter.level_update_stats();
  EXPECT_EQ(stats_update.num_decreases, 2);
  EXPECT_EQ(stats_update.sum_decreases, 5);
  EXPECT_EQ(stats_update.num_increases, 0);
  EXPECT_EQ(stats_update.sum_increases, 0);
}

TEST_F(AnalogGainStatsReporterTest, CheckLevelUpdateStatsAfterReset) {
  AnalogGainStatsReporter stats_reporter;
  constexpr int kMicLevel = 10;
  stats_reporter.UpdateStatistics(kMicLevel);
  // Update until the periodic reset.
  for (int i = 0; i < kFramesIn60Seconds - 2; i += 2) {
    stats_reporter.UpdateStatistics(kMicLevel + 2);
    stats_reporter.UpdateStatistics(kMicLevel);
  }
  const auto& stats_before_reset = stats_reporter.level_update_stats();
  EXPECT_EQ(stats_before_reset.num_decreases, kFramesIn60Seconds / 2 - 1);
  EXPECT_EQ(stats_before_reset.sum_decreases, kFramesIn60Seconds - 2);
  EXPECT_EQ(stats_before_reset.num_increases, kFramesIn60Seconds / 2 - 1);
  EXPECT_EQ(stats_before_reset.sum_increases, kFramesIn60Seconds - 2);
  stats_reporter.UpdateStatistics(kMicLevel + 2);
  const auto& stats_during_reset = stats_reporter.level_update_stats();
  EXPECT_EQ(stats_during_reset.num_decreases, 0);
  EXPECT_EQ(stats_during_reset.sum_decreases, 0);
  EXPECT_EQ(stats_during_reset.num_increases, 0);
  EXPECT_EQ(stats_during_reset.sum_increases, 0);
  stats_reporter.UpdateStatistics(kMicLevel);
  stats_reporter.UpdateStatistics(kMicLevel + 3);
  const auto& stats_after_reset = stats_reporter.level_update_stats();
  EXPECT_EQ(stats_after_reset.num_decreases, 1);
  EXPECT_EQ(stats_after_reset.sum_decreases, 2);
  EXPECT_EQ(stats_after_reset.num_increases, 1);
  EXPECT_EQ(stats_after_reset.sum_increases, 3);
}

}  // namespace webrtc
