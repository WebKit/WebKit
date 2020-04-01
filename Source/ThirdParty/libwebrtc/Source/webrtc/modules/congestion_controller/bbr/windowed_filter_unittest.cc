/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/bbr/windowed_filter.h"

#include <stdint.h>

#include <string>
#include <type_traits>

#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "rtc_base/logging.h"
#include "test/gtest.h"

namespace webrtc {
namespace bbr {
namespace test {
class WindowedFilterTest : public ::testing::Test {
 public:
  // Set the window to 99ms, so 25ms is more than a quarter rtt.
  WindowedFilterTest()
      : windowed_min_rtt_(99, TimeDelta::Zero(), 0),
        windowed_max_bw_(99, DataRate::Zero(), 0) {}

  // Sets up windowed_min_rtt_ to have the following values:
  // Best = 20ms, recorded at 25ms
  // Second best = 40ms, recorded at 75ms
  // Third best = 50ms, recorded at 100ms
  void InitializeMinFilter() {
    int64_t now_ms = 0;
    TimeDelta rtt_sample = TimeDelta::Millis(10);
    for (int i = 0; i < 5; ++i) {
      windowed_min_rtt_.Update(rtt_sample, now_ms);
      RTC_LOG(LS_VERBOSE) << "i: " << i << " sample: " << ToString(rtt_sample)
                          << " mins: "
                             " "
                          << ToString(windowed_min_rtt_.GetBest()) << " "
                          << ToString(windowed_min_rtt_.GetSecondBest()) << " "
                          << ToString(windowed_min_rtt_.GetThirdBest());
      now_ms += 25;
      rtt_sample = rtt_sample + TimeDelta::Millis(10);
    }
    EXPECT_EQ(TimeDelta::Millis(20), windowed_min_rtt_.GetBest());
    EXPECT_EQ(TimeDelta::Millis(40), windowed_min_rtt_.GetSecondBest());
    EXPECT_EQ(TimeDelta::Millis(50), windowed_min_rtt_.GetThirdBest());
  }

  // Sets up windowed_max_bw_ to have the following values:
  // Best = 900 bps, recorded at 25ms
  // Second best = 700 bps, recorded at 75ms
  // Third best = 600 bps, recorded at 100ms
  void InitializeMaxFilter() {
    int64_t now_ms = 0;
    DataRate bw_sample = DataRate::BitsPerSec(1000);
    for (int i = 0; i < 5; ++i) {
      windowed_max_bw_.Update(bw_sample, now_ms);
      RTC_LOG(LS_VERBOSE) << "i: " << i << " sample: " << ToString(bw_sample)
                          << " maxs: "
                             " "
                          << ToString(windowed_max_bw_.GetBest()) << " "
                          << ToString(windowed_max_bw_.GetSecondBest()) << " "
                          << ToString(windowed_max_bw_.GetThirdBest());
      now_ms += 25;
      bw_sample = DataRate::BitsPerSec(bw_sample.bps() - 100);
    }
    EXPECT_EQ(DataRate::BitsPerSec(900), windowed_max_bw_.GetBest());
    EXPECT_EQ(DataRate::BitsPerSec(700), windowed_max_bw_.GetSecondBest());
    EXPECT_EQ(DataRate::BitsPerSec(600), windowed_max_bw_.GetThirdBest());
  }

 protected:
  WindowedFilter<TimeDelta, MinFilter<TimeDelta>, int64_t, int64_t>
      windowed_min_rtt_;
  WindowedFilter<DataRate, MaxFilter<DataRate>, int64_t, int64_t>
      windowed_max_bw_;
};

namespace {
// Test helper function: updates the filter with a lot of small values in order
// to ensure that it is not susceptible to noise.
void UpdateWithIrrelevantSamples(
    WindowedFilter<uint64_t, MaxFilter<uint64_t>, uint64_t, uint64_t>* filter,
    uint64_t max_value,
    uint64_t time) {
  for (uint64_t i = 0; i < 1000; i++) {
    filter->Update(i % max_value, time);
  }
}
}  // namespace

TEST_F(WindowedFilterTest, UninitializedEstimates) {
  EXPECT_EQ(TimeDelta::Zero(), windowed_min_rtt_.GetBest());
  EXPECT_EQ(TimeDelta::Zero(), windowed_min_rtt_.GetSecondBest());
  EXPECT_EQ(TimeDelta::Zero(), windowed_min_rtt_.GetThirdBest());
  EXPECT_EQ(DataRate::Zero(), windowed_max_bw_.GetBest());
  EXPECT_EQ(DataRate::Zero(), windowed_max_bw_.GetSecondBest());
  EXPECT_EQ(DataRate::Zero(), windowed_max_bw_.GetThirdBest());
}

TEST_F(WindowedFilterTest, MonotonicallyIncreasingMin) {
  int64_t now_ms = 0;
  TimeDelta rtt_sample = TimeDelta::Millis(10);
  windowed_min_rtt_.Update(rtt_sample, now_ms);
  EXPECT_EQ(TimeDelta::Millis(10), windowed_min_rtt_.GetBest());

  // Gradually increase the rtt samples and ensure the windowed min rtt starts
  // rising.
  for (int i = 0; i < 6; ++i) {
    now_ms += 25;
    rtt_sample = rtt_sample + TimeDelta::Millis(10);
    windowed_min_rtt_.Update(rtt_sample, now_ms);
    RTC_LOG(LS_VERBOSE) << "i: " << i << " sample: " << rtt_sample.ms()
                        << " mins: "
                           " "
                        << windowed_min_rtt_.GetBest().ms() << " "
                        << windowed_min_rtt_.GetSecondBest().ms() << " "
                        << windowed_min_rtt_.GetThirdBest().ms();
    if (i < 3) {
      EXPECT_EQ(TimeDelta::Millis(10), windowed_min_rtt_.GetBest());
    } else if (i == 3) {
      EXPECT_EQ(TimeDelta::Millis(20), windowed_min_rtt_.GetBest());
    } else if (i < 6) {
      EXPECT_EQ(TimeDelta::Millis(40), windowed_min_rtt_.GetBest());
    }
  }
}

TEST_F(WindowedFilterTest, MonotonicallyDecreasingMax) {
  int64_t now_ms = 0;
  DataRate bw_sample = DataRate::BitsPerSec(1000);
  windowed_max_bw_.Update(bw_sample, now_ms);
  EXPECT_EQ(DataRate::BitsPerSec(1000), windowed_max_bw_.GetBest());

  // Gradually decrease the bw samples and ensure the windowed max bw starts
  // decreasing.
  for (int i = 0; i < 6; ++i) {
    now_ms += 25;
    bw_sample = DataRate::BitsPerSec(bw_sample.bps() - 100);
    windowed_max_bw_.Update(bw_sample, now_ms);
    RTC_LOG(LS_VERBOSE) << "i: " << i << " sample: " << bw_sample.bps()
                        << " maxs: "
                           " "
                        << windowed_max_bw_.GetBest().bps() << " "
                        << windowed_max_bw_.GetSecondBest().bps() << " "
                        << windowed_max_bw_.GetThirdBest().bps();
    if (i < 3) {
      EXPECT_EQ(DataRate::BitsPerSec(1000), windowed_max_bw_.GetBest());
    } else if (i == 3) {
      EXPECT_EQ(DataRate::BitsPerSec(900), windowed_max_bw_.GetBest());
    } else if (i < 6) {
      EXPECT_EQ(DataRate::BitsPerSec(700), windowed_max_bw_.GetBest());
    }
  }
}

TEST_F(WindowedFilterTest, SampleChangesThirdBestMin) {
  InitializeMinFilter();
  // RTT sample lower than the third-choice min-rtt sets that, but nothing else.
  TimeDelta rtt_sample =
      windowed_min_rtt_.GetThirdBest() - TimeDelta::Millis(5);
  // This assert is necessary to avoid triggering -Wstrict-overflow
  // See crbug/616957
  ASSERT_GT(windowed_min_rtt_.GetThirdBest(), TimeDelta::Millis(5));
  // Latest sample was recorded at 100ms.
  int64_t now_ms = 101;
  windowed_min_rtt_.Update(rtt_sample, now_ms);
  EXPECT_EQ(rtt_sample, windowed_min_rtt_.GetThirdBest());
  EXPECT_EQ(TimeDelta::Millis(40), windowed_min_rtt_.GetSecondBest());
  EXPECT_EQ(TimeDelta::Millis(20), windowed_min_rtt_.GetBest());
}

TEST_F(WindowedFilterTest, SampleChangesThirdBestMax) {
  InitializeMaxFilter();
  // BW sample higher than the third-choice max sets that, but nothing else.
  DataRate bw_sample =
      DataRate::BitsPerSec(windowed_max_bw_.GetThirdBest().bps() + 50);
  // Latest sample was recorded at 100ms.
  int64_t now_ms = 101;
  windowed_max_bw_.Update(bw_sample, now_ms);
  EXPECT_EQ(bw_sample, windowed_max_bw_.GetThirdBest());
  EXPECT_EQ(DataRate::BitsPerSec(700), windowed_max_bw_.GetSecondBest());
  EXPECT_EQ(DataRate::BitsPerSec(900), windowed_max_bw_.GetBest());
}

TEST_F(WindowedFilterTest, SampleChangesSecondBestMin) {
  InitializeMinFilter();
  // RTT sample lower than the second-choice min sets that and also
  // the third-choice min.
  TimeDelta rtt_sample =
      windowed_min_rtt_.GetSecondBest() - TimeDelta::Millis(5);
  // This assert is necessary to avoid triggering -Wstrict-overflow
  // See crbug/616957
  ASSERT_GT(windowed_min_rtt_.GetSecondBest(), TimeDelta::Millis(5));
  // Latest sample was recorded at 100ms.
  int64_t now_ms = 101;
  windowed_min_rtt_.Update(rtt_sample, now_ms);
  EXPECT_EQ(rtt_sample, windowed_min_rtt_.GetThirdBest());
  EXPECT_EQ(rtt_sample, windowed_min_rtt_.GetSecondBest());
  EXPECT_EQ(TimeDelta::Millis(20), windowed_min_rtt_.GetBest());
}

TEST_F(WindowedFilterTest, SampleChangesSecondBestMax) {
  InitializeMaxFilter();
  // BW sample higher than the second-choice max sets that and also
  // the third-choice max.
  DataRate bw_sample =
      DataRate::BitsPerSec(windowed_max_bw_.GetSecondBest().bps() + 50);

  // Latest sample was recorded at 100ms.
  int64_t now_ms = 101;
  windowed_max_bw_.Update(bw_sample, now_ms);
  EXPECT_EQ(bw_sample, windowed_max_bw_.GetThirdBest());
  EXPECT_EQ(bw_sample, windowed_max_bw_.GetSecondBest());
  EXPECT_EQ(DataRate::BitsPerSec(900), windowed_max_bw_.GetBest());
}

TEST_F(WindowedFilterTest, SampleChangesAllMins) {
  InitializeMinFilter();
  // RTT sample lower than the first-choice min-rtt sets that and also
  // the second and third-choice mins.
  TimeDelta rtt_sample = windowed_min_rtt_.GetBest() - TimeDelta::Millis(5);
  // This assert is necessary to avoid triggering -Wstrict-overflow
  // See crbug/616957
  ASSERT_GT(windowed_min_rtt_.GetBest(), TimeDelta::Millis(5));
  // Latest sample was recorded at 100ms.
  int64_t now_ms = 101;
  windowed_min_rtt_.Update(rtt_sample, now_ms);
  EXPECT_EQ(rtt_sample, windowed_min_rtt_.GetThirdBest());
  EXPECT_EQ(rtt_sample, windowed_min_rtt_.GetSecondBest());
  EXPECT_EQ(rtt_sample, windowed_min_rtt_.GetBest());
}

TEST_F(WindowedFilterTest, SampleChangesAllMaxs) {
  InitializeMaxFilter();
  // BW sample higher than the first-choice max sets that and also
  // the second and third-choice maxs.
  DataRate bw_sample =
      DataRate::BitsPerSec(windowed_max_bw_.GetBest().bps() + 50);
  // Latest sample was recorded at 100ms.
  int64_t now_ms = 101;
  windowed_max_bw_.Update(bw_sample, now_ms);
  EXPECT_EQ(bw_sample, windowed_max_bw_.GetThirdBest());
  EXPECT_EQ(bw_sample, windowed_max_bw_.GetSecondBest());
  EXPECT_EQ(bw_sample, windowed_max_bw_.GetBest());
}

TEST_F(WindowedFilterTest, ExpireBestMin) {
  InitializeMinFilter();
  TimeDelta old_third_best = windowed_min_rtt_.GetThirdBest();
  TimeDelta old_second_best = windowed_min_rtt_.GetSecondBest();
  TimeDelta rtt_sample = old_third_best + TimeDelta::Millis(5);
  // Best min sample was recorded at 25ms, so expiry time is 124ms.
  int64_t now_ms = 125;
  windowed_min_rtt_.Update(rtt_sample, now_ms);
  EXPECT_EQ(rtt_sample, windowed_min_rtt_.GetThirdBest());
  EXPECT_EQ(old_third_best, windowed_min_rtt_.GetSecondBest());
  EXPECT_EQ(old_second_best, windowed_min_rtt_.GetBest());
}

TEST_F(WindowedFilterTest, ExpireBestMax) {
  InitializeMaxFilter();
  DataRate old_third_best = windowed_max_bw_.GetThirdBest();
  DataRate old_second_best = windowed_max_bw_.GetSecondBest();
  DataRate bw_sample = DataRate::BitsPerSec(old_third_best.bps() - 50);
  // Best max sample was recorded at 25ms, so expiry time is 124ms.
  int64_t now_ms = 125;
  windowed_max_bw_.Update(bw_sample, now_ms);
  EXPECT_EQ(bw_sample, windowed_max_bw_.GetThirdBest());
  EXPECT_EQ(old_third_best, windowed_max_bw_.GetSecondBest());
  EXPECT_EQ(old_second_best, windowed_max_bw_.GetBest());
}

TEST_F(WindowedFilterTest, ExpireSecondBestMin) {
  InitializeMinFilter();
  TimeDelta old_third_best = windowed_min_rtt_.GetThirdBest();
  TimeDelta rtt_sample = old_third_best + TimeDelta::Millis(5);
  // Second best min sample was recorded at 75ms, so expiry time is 174ms.
  int64_t now_ms = 175;
  windowed_min_rtt_.Update(rtt_sample, now_ms);
  EXPECT_EQ(rtt_sample, windowed_min_rtt_.GetThirdBest());
  EXPECT_EQ(rtt_sample, windowed_min_rtt_.GetSecondBest());
  EXPECT_EQ(old_third_best, windowed_min_rtt_.GetBest());
}

TEST_F(WindowedFilterTest, ExpireSecondBestMax) {
  InitializeMaxFilter();
  DataRate old_third_best = windowed_max_bw_.GetThirdBest();
  DataRate bw_sample = DataRate::BitsPerSec(old_third_best.bps() - 50);
  // Second best max sample was recorded at 75ms, so expiry time is 174ms.
  int64_t now_ms = 175;
  windowed_max_bw_.Update(bw_sample, now_ms);
  EXPECT_EQ(bw_sample, windowed_max_bw_.GetThirdBest());
  EXPECT_EQ(bw_sample, windowed_max_bw_.GetSecondBest());
  EXPECT_EQ(old_third_best, windowed_max_bw_.GetBest());
}

TEST_F(WindowedFilterTest, ExpireAllMins) {
  InitializeMinFilter();
  TimeDelta rtt_sample =
      windowed_min_rtt_.GetThirdBest() + TimeDelta::Millis(5);
  // This assert is necessary to avoid triggering -Wstrict-overflow
  // See crbug/616957
  ASSERT_LT(windowed_min_rtt_.GetThirdBest(), TimeDelta::PlusInfinity());
  // Third best min sample was recorded at 100ms, so expiry time is 199ms.
  int64_t now_ms = 200;
  windowed_min_rtt_.Update(rtt_sample, now_ms);
  EXPECT_EQ(rtt_sample, windowed_min_rtt_.GetThirdBest());
  EXPECT_EQ(rtt_sample, windowed_min_rtt_.GetSecondBest());
  EXPECT_EQ(rtt_sample, windowed_min_rtt_.GetBest());
}

TEST_F(WindowedFilterTest, ExpireAllMaxs) {
  InitializeMaxFilter();
  DataRate bw_sample =
      DataRate::BitsPerSec(windowed_max_bw_.GetThirdBest().bps() - 50);
  // Third best max sample was recorded at 100ms, so expiry time is 199ms.
  int64_t now_ms = 200;
  windowed_max_bw_.Update(bw_sample, now_ms);
  EXPECT_EQ(bw_sample, windowed_max_bw_.GetThirdBest());
  EXPECT_EQ(bw_sample, windowed_max_bw_.GetSecondBest());
  EXPECT_EQ(bw_sample, windowed_max_bw_.GetBest());
}

// Test the windowed filter where the time used is an exact counter instead of a
// timestamp.  This is useful if, for example, the time is measured in round
// trips.
TEST_F(WindowedFilterTest, ExpireCounterBasedMax) {
  // Create a window which starts at t = 0 and expires after two cycles.
  WindowedFilter<uint64_t, MaxFilter<uint64_t>, uint64_t, uint64_t> max_filter(
      2, 0, 0);

  const uint64_t kBest = 50000;
  // Insert 50000 at t = 1.
  max_filter.Update(50000, 1);
  EXPECT_EQ(kBest, max_filter.GetBest());
  UpdateWithIrrelevantSamples(&max_filter, 20, 1);
  EXPECT_EQ(kBest, max_filter.GetBest());

  // Insert 40000 at t = 2.  Nothing is expected to expire.
  max_filter.Update(40000, 2);
  EXPECT_EQ(kBest, max_filter.GetBest());
  UpdateWithIrrelevantSamples(&max_filter, 20, 2);
  EXPECT_EQ(kBest, max_filter.GetBest());

  // Insert 30000 at t = 3.  Nothing is expected to expire yet.
  max_filter.Update(30000, 3);
  EXPECT_EQ(kBest, max_filter.GetBest());
  UpdateWithIrrelevantSamples(&max_filter, 20, 3);
  EXPECT_EQ(kBest, max_filter.GetBest());
  RTC_LOG(LS_VERBOSE) << max_filter.GetSecondBest();
  RTC_LOG(LS_VERBOSE) << max_filter.GetThirdBest();

  // Insert 20000 at t = 4.  50000 at t = 1 expires, so 40000 becomes the new
  // maximum.
  const uint64_t kNewBest = 40000;
  max_filter.Update(20000, 4);
  EXPECT_EQ(kNewBest, max_filter.GetBest());
  UpdateWithIrrelevantSamples(&max_filter, 20, 4);
  EXPECT_EQ(kNewBest, max_filter.GetBest());
}

}  // namespace test
}  // namespace bbr
}  // namespace webrtc
