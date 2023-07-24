/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_processing/agc2/input_volume_stats_reporter.h"

#include "absl/strings/string_view.h"
#include "rtc_base/strings/string_builder.h"
#include "system_wrappers/include/metrics.h"
#include "test/gmock.h"

namespace webrtc {
namespace {

using InputVolumeType = InputVolumeStatsReporter::InputVolumeType;

constexpr int kFramesIn60Seconds = 6000;

constexpr absl::string_view kLabelPrefix = "WebRTC.Audio.Apm.";

class InputVolumeStatsReporterTest
    : public ::testing::TestWithParam<InputVolumeType> {
 public:
  InputVolumeStatsReporterTest() { metrics::Reset(); }

 protected:
  InputVolumeType InputVolumeType() const { return GetParam(); }
  std::string VolumeLabel() const {
    return (rtc::StringBuilder(kLabelPrefix) << VolumeTypeLabel() << "OnChange")
        .str();
  }
  std::string DecreaseRateLabel() const {
    return (rtc::StringBuilder(kLabelPrefix)
            << VolumeTypeLabel() << "DecreaseRate")
        .str();
  }
  std::string DecreaseAverageLabel() const {
    return (rtc::StringBuilder(kLabelPrefix)
            << VolumeTypeLabel() << "DecreaseAverage")
        .str();
  }
  std::string IncreaseRateLabel() const {
    return (rtc::StringBuilder(kLabelPrefix)
            << VolumeTypeLabel() << "IncreaseRate")
        .str();
  }
  std::string IncreaseAverageLabel() const {
    return (rtc::StringBuilder(kLabelPrefix)
            << VolumeTypeLabel() << "IncreaseAverage")
        .str();
  }
  std::string UpdateRateLabel() const {
    return (rtc::StringBuilder(kLabelPrefix)
            << VolumeTypeLabel() << "UpdateRate")
        .str();
  }
  std::string UpdateAverageLabel() const {
    return (rtc::StringBuilder(kLabelPrefix)
            << VolumeTypeLabel() << "UpdateAverage")
        .str();
  }

 private:
  absl::string_view VolumeTypeLabel() const {
    switch (InputVolumeType()) {
      case InputVolumeType::kApplied:
        return "AppliedInputVolume.";
      case InputVolumeType::kRecommended:
        return "RecommendedInputVolume.";
    }
  }
};

TEST_P(InputVolumeStatsReporterTest, CheckVolumeOnChangeIsEmpty) {
  InputVolumeStatsReporter stats_reporter(InputVolumeType());
  stats_reporter.UpdateStatistics(10);
  EXPECT_METRIC_THAT(metrics::Samples(VolumeLabel()), ::testing::ElementsAre());
}

TEST_P(InputVolumeStatsReporterTest, CheckRateAverageStatsEmpty) {
  InputVolumeStatsReporter stats_reporter(InputVolumeType());
  constexpr int kInputVolume = 10;
  stats_reporter.UpdateStatistics(kInputVolume);
  // Update almost until the periodic logging and reset.
  for (int i = 0; i < kFramesIn60Seconds - 2; i += 2) {
    stats_reporter.UpdateStatistics(kInputVolume + 2);
    stats_reporter.UpdateStatistics(kInputVolume);
  }
  EXPECT_METRIC_THAT(metrics::Samples(UpdateRateLabel()),
                     ::testing::ElementsAre());
  EXPECT_METRIC_THAT(metrics::Samples(DecreaseRateLabel()),
                     ::testing::ElementsAre());
  EXPECT_METRIC_THAT(metrics::Samples(IncreaseRateLabel()),
                     ::testing::ElementsAre());
  EXPECT_METRIC_THAT(metrics::Samples(UpdateAverageLabel()),
                     ::testing::ElementsAre());
  EXPECT_METRIC_THAT(metrics::Samples(DecreaseAverageLabel()),
                     ::testing::ElementsAre());
  EXPECT_METRIC_THAT(metrics::Samples(IncreaseAverageLabel()),
                     ::testing::ElementsAre());
}

TEST_P(InputVolumeStatsReporterTest, CheckSamples) {
  InputVolumeStatsReporter stats_reporter(InputVolumeType());

  constexpr int kInputVolume1 = 10;
  stats_reporter.UpdateStatistics(kInputVolume1);
  // Update until periodic logging.
  constexpr int kInputVolume2 = 12;
  for (int i = 0; i < kFramesIn60Seconds; i += 2) {
    stats_reporter.UpdateStatistics(kInputVolume2);
    stats_reporter.UpdateStatistics(kInputVolume1);
  }
  // Update until periodic logging.
  constexpr int kInputVolume3 = 13;
  for (int i = 0; i < kFramesIn60Seconds; i += 2) {
    stats_reporter.UpdateStatistics(kInputVolume3);
    stats_reporter.UpdateStatistics(kInputVolume1);
  }

  // Check volume changes stats.
  EXPECT_METRIC_THAT(
      metrics::Samples(VolumeLabel()),
      ::testing::ElementsAre(
          ::testing::Pair(kInputVolume1, kFramesIn60Seconds),
          ::testing::Pair(kInputVolume2, kFramesIn60Seconds / 2),
          ::testing::Pair(kInputVolume3, kFramesIn60Seconds / 2)));

  // Check volume change rate stats.
  EXPECT_METRIC_THAT(
      metrics::Samples(UpdateRateLabel()),
      ::testing::ElementsAre(::testing::Pair(kFramesIn60Seconds - 1, 1),
                             ::testing::Pair(kFramesIn60Seconds, 1)));
  EXPECT_METRIC_THAT(
      metrics::Samples(DecreaseRateLabel()),
      ::testing::ElementsAre(::testing::Pair(kFramesIn60Seconds / 2 - 1, 1),
                             ::testing::Pair(kFramesIn60Seconds / 2, 1)));
  EXPECT_METRIC_THAT(
      metrics::Samples(IncreaseRateLabel()),
      ::testing::ElementsAre(::testing::Pair(kFramesIn60Seconds / 2, 2)));

  // Check volume change average stats.
  EXPECT_METRIC_THAT(
      metrics::Samples(UpdateAverageLabel()),
      ::testing::ElementsAre(::testing::Pair(2, 1), ::testing::Pair(3, 1)));
  EXPECT_METRIC_THAT(
      metrics::Samples(DecreaseAverageLabel()),
      ::testing::ElementsAre(::testing::Pair(2, 1), ::testing::Pair(3, 1)));
  EXPECT_METRIC_THAT(
      metrics::Samples(IncreaseAverageLabel()),
      ::testing::ElementsAre(::testing::Pair(2, 1), ::testing::Pair(3, 1)));
}
}  // namespace

TEST_P(InputVolumeStatsReporterTest, CheckVolumeUpdateStatsForEmptyStats) {
  InputVolumeStatsReporter stats_reporter(InputVolumeType());
  const auto& update_stats = stats_reporter.volume_update_stats();
  EXPECT_EQ(update_stats.num_decreases, 0);
  EXPECT_EQ(update_stats.sum_decreases, 0);
  EXPECT_EQ(update_stats.num_increases, 0);
  EXPECT_EQ(update_stats.sum_increases, 0);
}

TEST_P(InputVolumeStatsReporterTest,
       CheckVolumeUpdateStatsAfterNoVolumeChange) {
  constexpr int kInputVolume = 10;
  InputVolumeStatsReporter stats_reporter(InputVolumeType());
  stats_reporter.UpdateStatistics(kInputVolume);
  stats_reporter.UpdateStatistics(kInputVolume);
  stats_reporter.UpdateStatistics(kInputVolume);
  const auto& update_stats = stats_reporter.volume_update_stats();
  EXPECT_EQ(update_stats.num_decreases, 0);
  EXPECT_EQ(update_stats.sum_decreases, 0);
  EXPECT_EQ(update_stats.num_increases, 0);
  EXPECT_EQ(update_stats.sum_increases, 0);
}

TEST_P(InputVolumeStatsReporterTest,
       CheckVolumeUpdateStatsAfterVolumeIncrease) {
  constexpr int kInputVolume = 10;
  InputVolumeStatsReporter stats_reporter(InputVolumeType());
  stats_reporter.UpdateStatistics(kInputVolume);
  stats_reporter.UpdateStatistics(kInputVolume + 4);
  stats_reporter.UpdateStatistics(kInputVolume + 5);
  const auto& update_stats = stats_reporter.volume_update_stats();
  EXPECT_EQ(update_stats.num_decreases, 0);
  EXPECT_EQ(update_stats.sum_decreases, 0);
  EXPECT_EQ(update_stats.num_increases, 2);
  EXPECT_EQ(update_stats.sum_increases, 5);
}

TEST_P(InputVolumeStatsReporterTest,
       CheckVolumeUpdateStatsAfterVolumeDecrease) {
  constexpr int kInputVolume = 10;
  InputVolumeStatsReporter stats_reporter(InputVolumeType());
  stats_reporter.UpdateStatistics(kInputVolume);
  stats_reporter.UpdateStatistics(kInputVolume - 4);
  stats_reporter.UpdateStatistics(kInputVolume - 5);
  const auto& stats_update = stats_reporter.volume_update_stats();
  EXPECT_EQ(stats_update.num_decreases, 2);
  EXPECT_EQ(stats_update.sum_decreases, 5);
  EXPECT_EQ(stats_update.num_increases, 0);
  EXPECT_EQ(stats_update.sum_increases, 0);
}

TEST_P(InputVolumeStatsReporterTest, CheckVolumeUpdateStatsAfterReset) {
  InputVolumeStatsReporter stats_reporter(InputVolumeType());
  constexpr int kInputVolume = 10;
  stats_reporter.UpdateStatistics(kInputVolume);
  // Update until the periodic reset.
  for (int i = 0; i < kFramesIn60Seconds - 2; i += 2) {
    stats_reporter.UpdateStatistics(kInputVolume + 2);
    stats_reporter.UpdateStatistics(kInputVolume);
  }
  const auto& stats_before_reset = stats_reporter.volume_update_stats();
  EXPECT_EQ(stats_before_reset.num_decreases, kFramesIn60Seconds / 2 - 1);
  EXPECT_EQ(stats_before_reset.sum_decreases, kFramesIn60Seconds - 2);
  EXPECT_EQ(stats_before_reset.num_increases, kFramesIn60Seconds / 2 - 1);
  EXPECT_EQ(stats_before_reset.sum_increases, kFramesIn60Seconds - 2);
  stats_reporter.UpdateStatistics(kInputVolume + 2);
  const auto& stats_during_reset = stats_reporter.volume_update_stats();
  EXPECT_EQ(stats_during_reset.num_decreases, 0);
  EXPECT_EQ(stats_during_reset.sum_decreases, 0);
  EXPECT_EQ(stats_during_reset.num_increases, 0);
  EXPECT_EQ(stats_during_reset.sum_increases, 0);
  stats_reporter.UpdateStatistics(kInputVolume);
  stats_reporter.UpdateStatistics(kInputVolume + 3);
  const auto& stats_after_reset = stats_reporter.volume_update_stats();
  EXPECT_EQ(stats_after_reset.num_decreases, 1);
  EXPECT_EQ(stats_after_reset.sum_decreases, 2);
  EXPECT_EQ(stats_after_reset.num_increases, 1);
  EXPECT_EQ(stats_after_reset.sum_increases, 3);
}

INSTANTIATE_TEST_SUITE_P(,
                         InputVolumeStatsReporterTest,
                         ::testing::Values(InputVolumeType::kApplied,
                                           InputVolumeType::kRecommended));

}  // namespace webrtc
