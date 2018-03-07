/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/stats_counter.h"

#include "system_wrappers/include/clock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
const int kDefaultProcessIntervalMs = 2000;
const uint32_t kStreamId = 123456;

class StatsCounterObserverImpl : public StatsCounterObserver {
 public:
  StatsCounterObserverImpl() : num_calls_(0), last_sample_(-1) {}
  void OnMetricUpdated(int sample) override {
    ++num_calls_;
    last_sample_ = sample;
  }
  int num_calls_;
  int last_sample_;
};
}  // namespace

class StatsCounterTest : public ::testing::Test {
 protected:
  StatsCounterTest()
      : clock_(1234) {}

  void AddSampleAndAdvance(int sample, int interval_ms, AvgCounter* counter) {
    counter->Add(sample);
    clock_.AdvanceTimeMilliseconds(interval_ms);
  }

  void SetSampleAndAdvance(int sample,
                           int interval_ms,
                           RateAccCounter* counter) {
    counter->Set(sample, kStreamId);
    clock_.AdvanceTimeMilliseconds(interval_ms);
  }

  void VerifyStatsIsNotSet(const AggregatedStats& stats) {
    EXPECT_EQ(0, stats.num_samples);
    EXPECT_EQ(-1, stats.min);
    EXPECT_EQ(-1, stats.max);
    EXPECT_EQ(-1, stats.average);
  }

  SimulatedClock clock_;
};

TEST_F(StatsCounterTest, NoSamples) {
  AvgCounter counter(&clock_, nullptr, false);
  VerifyStatsIsNotSet(counter.GetStats());
}

TEST_F(StatsCounterTest, TestRegisterObserver) {
  StatsCounterObserverImpl* observer = new StatsCounterObserverImpl();
  const int kSample = 22;
  AvgCounter counter(&clock_, observer, false);
  AddSampleAndAdvance(kSample, kDefaultProcessIntervalMs, &counter);
  // Trigger process (sample included in next interval).
  counter.Add(111);
  EXPECT_EQ(1, observer->num_calls_);
}

TEST_F(StatsCounterTest, HasSample) {
  AvgCounter counter(&clock_, nullptr, false);
  EXPECT_FALSE(counter.HasSample());
  counter.Add(1);
  EXPECT_TRUE(counter.HasSample());
}

TEST_F(StatsCounterTest, VerifyProcessInterval) {
  StatsCounterObserverImpl* observer = new StatsCounterObserverImpl();
  AvgCounter counter(&clock_, observer, false);
  counter.Add(4);
  clock_.AdvanceTimeMilliseconds(kDefaultProcessIntervalMs - 1);
  // Try trigger process (interval has not passed).
  counter.Add(8);
  EXPECT_EQ(0, observer->num_calls_);
  VerifyStatsIsNotSet(counter.GetStats());
  // Make process interval pass.
  clock_.AdvanceTimeMilliseconds(1);
  // Trigger process (sample included in next interval).
  counter.Add(111);
  EXPECT_EQ(1, observer->num_calls_);
  EXPECT_EQ(6, observer->last_sample_);
  // Aggregated stats.
  AggregatedStats stats = counter.GetStats();
  EXPECT_EQ(1, stats.num_samples);
}

TEST_F(StatsCounterTest, TestMetric_AvgCounter) {
  StatsCounterObserverImpl* observer = new StatsCounterObserverImpl();
  AvgCounter counter(&clock_, observer, false);
  counter.Add(4);
  counter.Add(8);
  counter.Add(9);
  clock_.AdvanceTimeMilliseconds(kDefaultProcessIntervalMs);
  // Trigger process (sample included in next interval).
  counter.Add(111);
  // Average per interval.
  EXPECT_EQ(1, observer->num_calls_);
  EXPECT_EQ(7, observer->last_sample_);
  // Aggregated stats.
  AggregatedStats stats = counter.GetStats();
  EXPECT_EQ(1, stats.num_samples);
  EXPECT_EQ(7, stats.min);
  EXPECT_EQ(7, stats.max);
  EXPECT_EQ(7, stats.average);
}

TEST_F(StatsCounterTest, TestMetric_MaxCounter) {
  const int64_t kProcessIntervalMs = 1000;
  StatsCounterObserverImpl* observer = new StatsCounterObserverImpl();
  MaxCounter counter(&clock_, observer, kProcessIntervalMs);
  counter.Add(4);
  counter.Add(9);
  counter.Add(8);
  clock_.AdvanceTimeMilliseconds(kProcessIntervalMs);
  // Trigger process (sample included in next interval).
  counter.Add(111);
  // Average per interval.
  EXPECT_EQ(1, observer->num_calls_);
  EXPECT_EQ(9, observer->last_sample_);
  // Aggregated stats.
  AggregatedStats stats = counter.GetStats();
  EXPECT_EQ(1, stats.num_samples);
  EXPECT_EQ(9, stats.min);
  EXPECT_EQ(9, stats.max);
  EXPECT_EQ(9, stats.average);
}

TEST_F(StatsCounterTest, TestMetric_PercentCounter) {
  StatsCounterObserverImpl* observer = new StatsCounterObserverImpl();
  PercentCounter counter(&clock_, observer);
  counter.Add(true);
  counter.Add(false);
  clock_.AdvanceTimeMilliseconds(kDefaultProcessIntervalMs);
  // Trigger process (sample included in next interval).
  counter.Add(false);
  // Percentage per interval.
  EXPECT_EQ(1, observer->num_calls_);
  EXPECT_EQ(50, observer->last_sample_);
  // Aggregated stats.
  AggregatedStats stats = counter.GetStats();
  EXPECT_EQ(1, stats.num_samples);
  EXPECT_EQ(50, stats.min);
  EXPECT_EQ(50, stats.max);
}

TEST_F(StatsCounterTest, TestMetric_PermilleCounter) {
  StatsCounterObserverImpl* observer = new StatsCounterObserverImpl();
  PermilleCounter counter(&clock_, observer);
  counter.Add(true);
  counter.Add(false);
  clock_.AdvanceTimeMilliseconds(kDefaultProcessIntervalMs);
  // Trigger process (sample included in next interval).
  counter.Add(false);
  // Permille per interval.
  EXPECT_EQ(1, observer->num_calls_);
  EXPECT_EQ(500, observer->last_sample_);
  // Aggregated stats.
  AggregatedStats stats = counter.GetStats();
  EXPECT_EQ(1, stats.num_samples);
  EXPECT_EQ(500, stats.min);
  EXPECT_EQ(500, stats.max);
}

TEST_F(StatsCounterTest, TestMetric_RateCounter) {
  StatsCounterObserverImpl* observer = new StatsCounterObserverImpl();
  RateCounter counter(&clock_, observer, true);
  counter.Add(186);
  counter.Add(350);
  counter.Add(22);
  clock_.AdvanceTimeMilliseconds(kDefaultProcessIntervalMs);
  // Trigger process (sample included in next interval).
  counter.Add(111);
  // Rate per interval, (186 + 350 + 22) / 2 sec = 279 samples/sec
  EXPECT_EQ(1, observer->num_calls_);
  EXPECT_EQ(279, observer->last_sample_);
  // Aggregated stats.
  AggregatedStats stats = counter.GetStats();
  EXPECT_EQ(1, stats.num_samples);
  EXPECT_EQ(279, stats.min);
  EXPECT_EQ(279, stats.max);
}

TEST_F(StatsCounterTest, TestMetric_RateAccCounter) {
  StatsCounterObserverImpl* observer = new StatsCounterObserverImpl();
  RateAccCounter counter(&clock_, observer, true);
  counter.Set(175, kStreamId);
  counter.Set(188, kStreamId);
  clock_.AdvanceTimeMilliseconds(kDefaultProcessIntervalMs);
  // Trigger process (sample included in next interval).
  counter.Set(192, kStreamId);
  // Rate per interval: (188 - 0) / 2 sec = 94 samples/sec
  EXPECT_EQ(1, observer->num_calls_);
  EXPECT_EQ(94, observer->last_sample_);
  // Aggregated stats.
  AggregatedStats stats = counter.GetStats();
  EXPECT_EQ(1, stats.num_samples);
  EXPECT_EQ(94, stats.min);
  EXPECT_EQ(94, stats.max);
}

TEST_F(StatsCounterTest, TestMetric_RateAccCounterWithSetLast) {
  StatsCounterObserverImpl* observer = new StatsCounterObserverImpl();
  RateAccCounter counter(&clock_, observer, true);
  counter.SetLast(98, kStreamId);
  counter.Set(175, kStreamId);
  counter.Set(188, kStreamId);
  clock_.AdvanceTimeMilliseconds(kDefaultProcessIntervalMs);
  // Trigger process (sample included in next interval).
  counter.Set(192, kStreamId);
  // Rate per interval: (188 - 98) / 2 sec = 45 samples/sec
  EXPECT_EQ(1, observer->num_calls_);
  EXPECT_EQ(45, observer->last_sample_);
}

TEST_F(StatsCounterTest, TestMetric_RateAccCounterWithMultipleStreamIds) {
  StatsCounterObserverImpl* observer = new StatsCounterObserverImpl();
  RateAccCounter counter(&clock_, observer, true);
  counter.Set(175, kStreamId);
  counter.Set(188, kStreamId);
  counter.Set(100, kStreamId + 1);
  clock_.AdvanceTimeMilliseconds(kDefaultProcessIntervalMs);
  // Trigger process (sample included in next interval).
  counter.Set(150, kStreamId + 1);
  // Rate per interval: ((188 - 0) + (100 - 0)) / 2 sec = 144 samples/sec
  EXPECT_EQ(1, observer->num_calls_);
  EXPECT_EQ(144, observer->last_sample_);
  clock_.AdvanceTimeMilliseconds(kDefaultProcessIntervalMs);
  // Trigger process (sample included in next interval).
  counter.Set(198, kStreamId);
  // Rate per interval: (0 + (150 - 100)) / 2 sec = 25 samples/sec
  EXPECT_EQ(2, observer->num_calls_);
  EXPECT_EQ(25, observer->last_sample_);
  clock_.AdvanceTimeMilliseconds(kDefaultProcessIntervalMs);
  // Trigger process (sample included in next interval).
  counter.Set(200, kStreamId);
  // Rate per interval: ((198 - 188) + (0)) / 2 sec = 5 samples/sec
  EXPECT_EQ(3, observer->num_calls_);
  EXPECT_EQ(5, observer->last_sample_);
  // Aggregated stats.
  AggregatedStats stats = counter.GetStats();
  EXPECT_EQ(3, stats.num_samples);
  EXPECT_EQ(5, stats.min);
  EXPECT_EQ(144, stats.max);
}

TEST_F(StatsCounterTest, TestGetStats_MultipleIntervals) {
  AvgCounter counter(&clock_, nullptr, false);
  const int kSample1 = 1;
  const int kSample2 = 5;
  const int kSample3 = 8;
  const int kSample4 = 11;
  const int kSample5 = 50;
  AddSampleAndAdvance(kSample1, kDefaultProcessIntervalMs, &counter);
  AddSampleAndAdvance(kSample2, kDefaultProcessIntervalMs, &counter);
  AddSampleAndAdvance(kSample3, kDefaultProcessIntervalMs, &counter);
  AddSampleAndAdvance(kSample4, kDefaultProcessIntervalMs, &counter);
  AddSampleAndAdvance(kSample5, kDefaultProcessIntervalMs, &counter);
  // Trigger process (sample included in next interval).
  counter.Add(111);
  AggregatedStats stats = counter.GetStats();
  EXPECT_EQ(5, stats.num_samples);
  EXPECT_EQ(kSample1, stats.min);
  EXPECT_EQ(kSample5, stats.max);
  EXPECT_EQ(15, stats.average);
}

TEST_F(StatsCounterTest, TestGetStatsTwice) {
  const int kSample1 = 4;
  const int kSample2 = 7;
  AvgCounter counter(&clock_, nullptr, false);
  AddSampleAndAdvance(kSample1, kDefaultProcessIntervalMs, &counter);
  // Trigger process (sample included in next interval).
  counter.Add(kSample2);
  AggregatedStats stats = counter.GetStats();
  EXPECT_EQ(1, stats.num_samples);
  EXPECT_EQ(kSample1, stats.min);
  EXPECT_EQ(kSample1, stats.max);
  // Trigger process (sample included in next interval).
  clock_.AdvanceTimeMilliseconds(kDefaultProcessIntervalMs);
  counter.Add(111);
  stats = counter.GetStats();
  EXPECT_EQ(2, stats.num_samples);
  EXPECT_EQ(kSample1, stats.min);
  EXPECT_EQ(kSample2, stats.max);
  EXPECT_EQ(6, stats.average);
}

TEST_F(StatsCounterTest, TestRateAccCounter_NegativeRateIgnored) {
  StatsCounterObserverImpl* observer = new StatsCounterObserverImpl();
  const int kSample1 = 200;  //  200 / 2 sec
  const int kSample2 = 100;  // -100 / 2 sec - negative ignored
  const int kSample3 = 700;  //  600 / 2 sec
  RateAccCounter counter(&clock_, observer, true);
  SetSampleAndAdvance(kSample1, kDefaultProcessIntervalMs, &counter);
  SetSampleAndAdvance(kSample2, kDefaultProcessIntervalMs, &counter);
  SetSampleAndAdvance(kSample3, kDefaultProcessIntervalMs, &counter);
  EXPECT_EQ(1, observer->num_calls_);
  EXPECT_EQ(100, observer->last_sample_);
  // Trigger process (sample included in next interval).
  counter.Set(2000, kStreamId);
  EXPECT_EQ(2, observer->num_calls_);
  EXPECT_EQ(300, observer->last_sample_);
  // Aggregated stats.
  AggregatedStats stats = counter.GetStats();
  EXPECT_EQ(2, stats.num_samples);
  EXPECT_EQ(100, stats.min);
  EXPECT_EQ(300, stats.max);
  EXPECT_EQ(200, stats.average);
}

TEST_F(StatsCounterTest, TestAvgCounter_IntervalsWithoutSamplesIncluded) {
  // Samples: | 6 | x | x | 8 |  // x: empty interval
  // Stats:   | 6 | 6 | 6 | 8 |  // x -> last value reported
  StatsCounterObserverImpl* observer = new StatsCounterObserverImpl();
  AvgCounter counter(&clock_, observer, true);
  AddSampleAndAdvance(6, kDefaultProcessIntervalMs * 4 - 1, &counter);
  // Trigger process (sample included in next interval).
  counter.Add(8);
  // [6:3], 3 intervals passed (2 without samples -> last value reported).
  AggregatedStats stats = counter.ProcessAndGetStats();
  EXPECT_EQ(3, stats.num_samples);
  EXPECT_EQ(6, stats.min);
  EXPECT_EQ(6, stats.max);
  // Make next interval pass and verify stats: [6:3],[8:1]
  clock_.AdvanceTimeMilliseconds(1);
  counter.ProcessAndGetStats();
  EXPECT_EQ(4, observer->num_calls_);
  EXPECT_EQ(8, observer->last_sample_);
}

TEST_F(StatsCounterTest, TestAvgCounter_WithPause) {
  // Samples: | 6 | x | x | x | - | 22 | x  |  // x: empty interval, -: paused
  // Stats:   | 6 | 6 | 6 | 6 | - | 22 | 22 |  // x -> last value reported
  StatsCounterObserverImpl* observer = new StatsCounterObserverImpl();
  AvgCounter counter(&clock_, observer, true);
  // Add sample and advance 3 intervals (2 w/o samples -> last value reported).
  AddSampleAndAdvance(6, kDefaultProcessIntervalMs * 4 - 1, &counter);
  // Trigger process and verify stats: [6:3]
  counter.ProcessAndGetStats();
  EXPECT_EQ(3, observer->num_calls_);
  EXPECT_EQ(6, observer->last_sample_);
  // Make next interval pass (1 without samples).
  // Process and pause. Verify stats: [6:4].
  clock_.AdvanceTimeMilliseconds(1);
  counter.ProcessAndPause();
  EXPECT_EQ(4, observer->num_calls_);  // Last value reported.
  EXPECT_EQ(6, observer->last_sample_);
  // Make next interval pass (1 without samples -> ignored while paused).
  clock_.AdvanceTimeMilliseconds(kDefaultProcessIntervalMs * 2 - 1);
  counter.Add(22);  // Stops pause.
  EXPECT_EQ(4, observer->num_calls_);
  EXPECT_EQ(6, observer->last_sample_);
  // Make next interval pass, [6:4][22:1]
  clock_.AdvanceTimeMilliseconds(1);
  counter.ProcessAndGetStats();
  EXPECT_EQ(5, observer->num_calls_);
  EXPECT_EQ(22, observer->last_sample_);
  // Make 1 interval pass (1 w/o samples -> pause stopped, last value reported).
  clock_.AdvanceTimeMilliseconds(kDefaultProcessIntervalMs);
  counter.ProcessAndGetStats();
  EXPECT_EQ(6, observer->num_calls_);
  EXPECT_EQ(22, observer->last_sample_);
}

TEST_F(StatsCounterTest, TestRateAccCounter_AddSampleStopsPause) {
  // Samples: | 12 | 24 |  // -: paused
  // Stats:   | 6  | 6  |
  StatsCounterObserverImpl* observer = new StatsCounterObserverImpl();
  RateAccCounter counter(&clock_, observer, true);
  // Add sample and advance 1 intervals.
  counter.Set(12, kStreamId);
  clock_.AdvanceTimeMilliseconds(kDefaultProcessIntervalMs);
  // Trigger process and verify stats: [6:1]
  counter.ProcessAndPause();
  EXPECT_EQ(1, observer->num_calls_);
  EXPECT_EQ(6, observer->last_sample_);
  // Add sample and advance 1 intervals.
  counter.Set(24, kStreamId);  // Pause stopped.
  clock_.AdvanceTimeMilliseconds(kDefaultProcessIntervalMs);
  counter.ProcessAndGetStats();
  EXPECT_EQ(2, observer->num_calls_);
  EXPECT_EQ(6, observer->last_sample_);
}

TEST_F(StatsCounterTest, TestRateAccCounter_AddSameSampleDoesNotStopPause) {
  // Samples: | 12 | 12 | 24 |  // -: paused
  // Stats:   | 6  | -  | 6  |
  StatsCounterObserverImpl* observer = new StatsCounterObserverImpl();
  RateAccCounter counter(&clock_, observer, true);
  // Add sample and advance 1 intervals.
  counter.Set(12, kStreamId);
  clock_.AdvanceTimeMilliseconds(kDefaultProcessIntervalMs);
  // Trigger process and verify stats: [6:1]
  counter.ProcessAndPause();
  EXPECT_EQ(1, observer->num_calls_);
  EXPECT_EQ(6, observer->last_sample_);
  // Add same sample and advance 1 intervals.
  counter.Set(12, kStreamId);  // Pause not stopped.
  clock_.AdvanceTimeMilliseconds(kDefaultProcessIntervalMs);
  counter.ProcessAndGetStats();
  EXPECT_EQ(1, observer->num_calls_);
  EXPECT_EQ(6, observer->last_sample_);
  // Add new sample and advance 1 intervals.
  counter.Set(24, kStreamId);  // Pause stopped.
  clock_.AdvanceTimeMilliseconds(kDefaultProcessIntervalMs);
  counter.ProcessAndGetStats();
  EXPECT_EQ(2, observer->num_calls_);
  EXPECT_EQ(6, observer->last_sample_);
}

TEST_F(StatsCounterTest, TestRateAccCounter_PauseAndStopPause) {
  // Samples: | 12 | 12 | 12 |  // -: paused
  // Stats:   | 6  | -  | 0  |
  StatsCounterObserverImpl* observer = new StatsCounterObserverImpl();
  RateAccCounter counter(&clock_, observer, true);
  // Add sample and advance 1 intervals.
  counter.Set(12, kStreamId);
  clock_.AdvanceTimeMilliseconds(kDefaultProcessIntervalMs);
  // Trigger process and verify stats: [6:1]
  counter.ProcessAndPause();
  EXPECT_EQ(1, observer->num_calls_);
  EXPECT_EQ(6, observer->last_sample_);
  // Add same sample and advance 1 intervals.
  counter.Set(12, kStreamId);  // Pause not stopped.
  clock_.AdvanceTimeMilliseconds(kDefaultProcessIntervalMs);
  counter.ProcessAndGetStats();
  EXPECT_EQ(1, observer->num_calls_);
  EXPECT_EQ(6, observer->last_sample_);
  // Stop pause, add sample and advance 1 intervals.
  counter.ProcessAndStopPause();
  counter.Set(12, kStreamId);
  clock_.AdvanceTimeMilliseconds(kDefaultProcessIntervalMs);
  counter.ProcessAndGetStats();
  EXPECT_EQ(2, observer->num_calls_);
  EXPECT_EQ(0, observer->last_sample_);
}

TEST_F(StatsCounterTest, TestAvgCounter_WithoutMinPauseTimePassed) {
  // Samples: | 6 | 2 | - |  // x: empty interval, -: paused
  // Stats:   | 6 | 2 | - |  // x -> last value reported
  StatsCounterObserverImpl* observer = new StatsCounterObserverImpl();
  AvgCounter counter(&clock_, observer, true);
  // Add sample and advance 1 intervals.
  AddSampleAndAdvance(6, kDefaultProcessIntervalMs, &counter);
  // Process and pause. Verify stats: [6:1].
  const int64_t kMinMs = 500;
  counter.ProcessAndPauseForDuration(kMinMs);
  EXPECT_EQ(1, observer->num_calls_);  // Last value reported.
  EXPECT_EQ(6, observer->last_sample_);
  // Min pause time has not pass.
  clock_.AdvanceTimeMilliseconds(kMinMs - 1);
  counter.Add(2);  // Pause not stopped.
  // Make two intervals pass (1 without samples -> ignored while paused).
  clock_.AdvanceTimeMilliseconds(kDefaultProcessIntervalMs * 2 - (kMinMs - 1));
  counter.ProcessAndGetStats();
  EXPECT_EQ(2, observer->num_calls_);
  EXPECT_EQ(2, observer->last_sample_);
}

TEST_F(StatsCounterTest, TestAvgCounter_WithMinPauseTimePassed) {
  // Samples: | 6 | 2 | x |  // x: empty interval, -: paused
  // Stats:   | 6 | 2 | 2 |  // x -> last value reported
  StatsCounterObserverImpl* observer = new StatsCounterObserverImpl();
  AvgCounter counter(&clock_, observer, true);
  // Add sample and advance 1 intervals.
  AddSampleAndAdvance(6, kDefaultProcessIntervalMs, &counter);
  // Process and pause. Verify stats: [6:1].
  const int64_t kMinMs = 500;
  counter.ProcessAndPauseForDuration(kMinMs);
  EXPECT_EQ(1, observer->num_calls_);  // Last value reported.
  EXPECT_EQ(6, observer->last_sample_);
  // Make min pause time pass.
  clock_.AdvanceTimeMilliseconds(kMinMs);
  counter.Add(2);  // Stop pause.
  // Make two intervals pass (1 without samples -> last value reported).
  clock_.AdvanceTimeMilliseconds(kDefaultProcessIntervalMs * 2 - kMinMs);
  counter.ProcessAndGetStats();
  EXPECT_EQ(3, observer->num_calls_);
  EXPECT_EQ(2, observer->last_sample_);
}

TEST_F(StatsCounterTest, TestRateCounter_IntervalsWithoutSamplesIgnored) {
  // Samples: | 50 | x | 20 |  // x: empty interval
  // Stats:   | 25 | x | 10 |  // x -> ignored
  const bool kIncludeEmptyIntervals = false;
  StatsCounterObserverImpl* observer = new StatsCounterObserverImpl();
  const int kSample1 = 50;  //  50 / 2 sec
  const int kSample2 = 20;  //  20 / 2 sec
  RateCounter counter(&clock_, observer, kIncludeEmptyIntervals);
  counter.Add(kSample1);
  clock_.AdvanceTimeMilliseconds(kDefaultProcessIntervalMs * 3 - 1);
  // Trigger process (sample included in next interval).
  counter.Add(kSample2);
  // [25:1], 2 intervals passed (1 without samples -> ignored).
  EXPECT_EQ(1, observer->num_calls_);
  EXPECT_EQ(25, observer->last_sample_);
  // Make next interval pass and verify stats: [10:1],[25:1]
  clock_.AdvanceTimeMilliseconds(1);
  counter.ProcessAndGetStats();
  EXPECT_EQ(2, observer->num_calls_);
  EXPECT_EQ(10, observer->last_sample_);
}

TEST_F(StatsCounterTest, TestRateCounter_IntervalsWithoutSamplesIncluded) {
  // Samples: | 50 | x | 20 |  // x: empty interval
  // Stats:   | 25 | 0 | 10 |  // x -> zero reported
  const bool kIncludeEmptyIntervals = true;
  StatsCounterObserverImpl* observer = new StatsCounterObserverImpl();
  const int kSample1 = 50;  //  50 / 2 sec
  const int kSample2 = 20;  //  20 / 2 sec
  RateCounter counter(&clock_, observer, kIncludeEmptyIntervals);
  counter.Add(kSample1);
  clock_.AdvanceTimeMilliseconds(kDefaultProcessIntervalMs * 3 - 1);
  // Trigger process (sample included in next interval).
  counter.Add(kSample2);
  // [0:1],[25:1], 2 intervals passed (1 without samples -> zero reported).
  EXPECT_EQ(2, observer->num_calls_);
  EXPECT_EQ(0, observer->last_sample_);
  // Make last interval pass and verify stats: [0:1],[10:1],[25:1]
  clock_.AdvanceTimeMilliseconds(1);
  AggregatedStats stats = counter.ProcessAndGetStats();
  EXPECT_EQ(25, stats.max);
  EXPECT_EQ(3, observer->num_calls_);
  EXPECT_EQ(10, observer->last_sample_);
}

TEST_F(StatsCounterTest, TestRateAccCounter_IntervalsWithoutSamplesIncluded) {
  // Samples: | 12 | x | x | x | 60 |  // x: empty interval
  // Stats:   | 6  | 0 | 0 | 0 | 24 |  // x -> zero reported
  StatsCounterObserverImpl* observer = new StatsCounterObserverImpl();
  RateAccCounter counter(&clock_, observer, true);
  VerifyStatsIsNotSet(counter.ProcessAndGetStats());
  // Advance one interval and verify stats.
  clock_.AdvanceTimeMilliseconds(kDefaultProcessIntervalMs);
  VerifyStatsIsNotSet(counter.ProcessAndGetStats());
  // Add sample and advance 3 intervals (2 w/o samples -> zero reported).
  counter.Set(12, kStreamId);
  clock_.AdvanceTimeMilliseconds(kDefaultProcessIntervalMs * 4 - 1);
  // Trigger process and verify stats: [0:2][6:1]
  counter.ProcessAndGetStats();
  EXPECT_EQ(3, observer->num_calls_);
  EXPECT_EQ(0, observer->last_sample_);
  // Make next interval pass (1 w/o samples -> zero reported), [0:3][6:1]
  clock_.AdvanceTimeMilliseconds(1);
  counter.ProcessAndGetStats();
  EXPECT_EQ(4, observer->num_calls_);
  EXPECT_EQ(0, observer->last_sample_);
  // Insert sample and advance non-complete interval, no change, [0:3][6:1]
  clock_.AdvanceTimeMilliseconds(kDefaultProcessIntervalMs - 1);
  counter.Set(60, kStreamId);
  EXPECT_EQ(4, observer->num_calls_);
  // Make next interval pass, [0:3][6:1][24:1]
  clock_.AdvanceTimeMilliseconds(1);
  AggregatedStats stats = counter.ProcessAndGetStats();
  EXPECT_EQ(5, observer->num_calls_);
  EXPECT_EQ(24, observer->last_sample_);
  EXPECT_EQ(6, stats.average);
}

TEST_F(StatsCounterTest, TestRateAccCounter_IntervalsWithoutSamplesIgnored) {
  // Samples: | 12 | x | x | x | 60 |  // x: empty interval
  // Stats:   | 6  | x | x | x | 24 |  // x -> ignored
  StatsCounterObserverImpl* observer = new StatsCounterObserverImpl();
  RateAccCounter counter(&clock_, observer, false);
  // Add sample and advance 3 intervals (2 w/o samples -> ignored).
  counter.Set(12, kStreamId);
  clock_.AdvanceTimeMilliseconds(kDefaultProcessIntervalMs * 4 - 1);
  // Trigger process and verify stats: [6:1]
  counter.ProcessAndGetStats();
  EXPECT_EQ(1, observer->num_calls_);
  EXPECT_EQ(6, observer->last_sample_);
  // Make next interval pass (1 w/o samples -> ignored), [6:1]
  clock_.AdvanceTimeMilliseconds(1);
  counter.ProcessAndGetStats();
  EXPECT_EQ(1, observer->num_calls_);
  // Insert sample and advance non-complete interval, no change, [6:1]
  clock_.AdvanceTimeMilliseconds(kDefaultProcessIntervalMs - 1);
  counter.Set(60, kStreamId);
  counter.ProcessAndGetStats();
  EXPECT_EQ(1, observer->num_calls_);
  // Make next interval pass, [6:1][24:1]
  clock_.AdvanceTimeMilliseconds(1);
  counter.ProcessAndGetStats();
  EXPECT_EQ(2, observer->num_calls_);
  EXPECT_EQ(24, observer->last_sample_);
}

}  // namespace webrtc
