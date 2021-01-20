/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/call_stats2.h"

#include <memory>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "rtc_base/thread.h"
#include "system_wrappers/include/metrics.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/run_loop.h"

using ::testing::AnyNumber;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;

namespace webrtc {
namespace internal {

class MockStatsObserver : public CallStatsObserver {
 public:
  MockStatsObserver() {}
  virtual ~MockStatsObserver() {}

  MOCK_METHOD(void, OnRttUpdate, (int64_t, int64_t), (override));
};

class CallStats2Test : public ::testing::Test {
 public:
  CallStats2Test() { process_thread_->Start(); }

  ~CallStats2Test() override { process_thread_->Stop(); }

  // Queues an rtt update call on the process thread.
  void AsyncSimulateRttUpdate(int64_t rtt) {
    RtcpRttStats* rtcp_rtt_stats = call_stats_.AsRtcpRttStats();
    process_thread_->PostTask(ToQueuedTask(
        [rtcp_rtt_stats, rtt] { rtcp_rtt_stats->OnRttUpdate(rtt); }));
  }

 protected:
  void FlushProcessAndWorker() {
    process_thread_->PostTask(
        ToQueuedTask([this] { loop_.PostTask([this]() { loop_.Quit(); }); }));
    loop_.Run();
  }

  test::RunLoop loop_;
  std::unique_ptr<ProcessThread> process_thread_{
      ProcessThread::Create("CallStats")};
  // Note: Since rtc::Thread doesn't support injecting a Clock, we're going
  // to be using a mix of the fake clock (used by CallStats) as well as the
  // system clock (used by rtc::Thread). This isn't ideal and will result in
  // the tests taking longer to execute in some cases than they need to.
  SimulatedClock fake_clock_{12345};
  CallStats call_stats_{&fake_clock_, loop_.task_queue()};
};

TEST_F(CallStats2Test, AddAndTriggerCallback) {
  static constexpr const int64_t kRtt = 25;

  MockStatsObserver stats_observer;
  EXPECT_CALL(stats_observer, OnRttUpdate(kRtt, kRtt))
      .Times(1)
      .WillOnce(InvokeWithoutArgs([this] { loop_.Quit(); }));

  call_stats_.RegisterStatsObserver(&stats_observer);
  EXPECT_EQ(-1, call_stats_.LastProcessedRtt());

  AsyncSimulateRttUpdate(kRtt);
  loop_.Run();

  EXPECT_EQ(kRtt, call_stats_.LastProcessedRtt());

  call_stats_.DeregisterStatsObserver(&stats_observer);
}

TEST_F(CallStats2Test, ProcessTime) {
  static constexpr const int64_t kRtt = 100;
  static constexpr const int64_t kRtt2 = 80;

  MockStatsObserver stats_observer;

  EXPECT_CALL(stats_observer, OnRttUpdate(kRtt, kRtt))
      .Times(2)
      .WillOnce(InvokeWithoutArgs([this] {
        // Advance clock and verify we get an update.
        fake_clock_.AdvanceTimeMilliseconds(CallStats::kUpdateInterval.ms());
      }))
      .WillRepeatedly(InvokeWithoutArgs([this] {
        AsyncSimulateRttUpdate(kRtt2);
        // Advance clock just too little to get an update.
        fake_clock_.AdvanceTimeMilliseconds(CallStats::kUpdateInterval.ms() -
                                            1);
      }));

  // In case you're reading this and wondering how this number is arrived at,
  // please see comments in the ChangeRtt test that go into some detail.
  static constexpr const int64_t kLastAvg = 94;
  EXPECT_CALL(stats_observer, OnRttUpdate(kLastAvg, kRtt2))
      .Times(1)
      .WillOnce(InvokeWithoutArgs([this] { loop_.Quit(); }));

  call_stats_.RegisterStatsObserver(&stats_observer);

  AsyncSimulateRttUpdate(kRtt);
  loop_.Run();

  call_stats_.DeregisterStatsObserver(&stats_observer);
}

// Verify all observers get correct estimates and observers can be added and
// removed.
TEST_F(CallStats2Test, MultipleObservers) {
  MockStatsObserver stats_observer_1;
  call_stats_.RegisterStatsObserver(&stats_observer_1);
  // Add the second observer twice, there should still be only one report to the
  // observer.
  MockStatsObserver stats_observer_2;
  call_stats_.RegisterStatsObserver(&stats_observer_2);
  call_stats_.RegisterStatsObserver(&stats_observer_2);

  static constexpr const int64_t kRtt = 100;

  // Verify both observers are updated.
  EXPECT_CALL(stats_observer_1, OnRttUpdate(kRtt, kRtt))
      .Times(AnyNumber())
      .WillRepeatedly(Return());
  EXPECT_CALL(stats_observer_2, OnRttUpdate(kRtt, kRtt))
      .Times(AnyNumber())
      .WillOnce(InvokeWithoutArgs([this] { loop_.Quit(); }))
      .WillRepeatedly(Return());
  AsyncSimulateRttUpdate(kRtt);
  loop_.Run();

  // Deregister the second observer and verify update is only sent to the first
  // observer.
  call_stats_.DeregisterStatsObserver(&stats_observer_2);

  EXPECT_CALL(stats_observer_1, OnRttUpdate(kRtt, kRtt))
      .Times(AnyNumber())
      .WillOnce(InvokeWithoutArgs([this] { loop_.Quit(); }))
      .WillRepeatedly(Return());
  EXPECT_CALL(stats_observer_2, OnRttUpdate(kRtt, kRtt)).Times(0);
  AsyncSimulateRttUpdate(kRtt);
  loop_.Run();

  // Deregister the first observer.
  call_stats_.DeregisterStatsObserver(&stats_observer_1);

  // Now make sure we don't get any callbacks.
  EXPECT_CALL(stats_observer_1, OnRttUpdate(kRtt, kRtt)).Times(0);
  EXPECT_CALL(stats_observer_2, OnRttUpdate(kRtt, kRtt)).Times(0);
  AsyncSimulateRttUpdate(kRtt);

  // Flush the queue on the process thread to make sure we return after
  // Process() has been called.
  FlushProcessAndWorker();
}

// Verify increasing and decreasing rtt triggers callbacks with correct values.
TEST_F(CallStats2Test, ChangeRtt) {
  // NOTE: This test assumes things about how old reports are removed
  // inside of call_stats.cc. The threshold ms value is 1500ms, but it's not
  // clear here that how the clock is advanced, affects that algorithm and
  // subsequently the average reported rtt.

  MockStatsObserver stats_observer;
  call_stats_.RegisterStatsObserver(&stats_observer);

  static constexpr const int64_t kFirstRtt = 100;
  static constexpr const int64_t kLowRtt = kFirstRtt - 20;
  static constexpr const int64_t kHighRtt = kFirstRtt + 20;

  EXPECT_CALL(stats_observer, OnRttUpdate(kFirstRtt, kFirstRtt))
      .Times(1)
      .WillOnce(InvokeWithoutArgs([this] {
        fake_clock_.AdvanceTimeMilliseconds(1000);
        AsyncSimulateRttUpdate(kHighRtt);  // Reported at T1 (1000ms).
      }));

  // NOTE: This relies on the internal algorithms of call_stats.cc.
  // There's a weight factor there (0.3), that weighs the previous average to
  // the new one by 70%, so the number 103 in this case is arrived at like so:
  // (100) / 1 * 0.7 + (100+120)/2 * 0.3 = 103
  static constexpr const int64_t kAvgRtt1 = 103;
  EXPECT_CALL(stats_observer, OnRttUpdate(kAvgRtt1, kHighRtt))
      .Times(1)
      .WillOnce(InvokeWithoutArgs([this] {
        // This interacts with an internal implementation detail in call_stats
        // that decays the oldest rtt value. See more below.
        fake_clock_.AdvanceTimeMilliseconds(1000);
        AsyncSimulateRttUpdate(kLowRtt);  // Reported at T2 (2000ms).
      }));

  // Increase time enough for a new update, but not too much to make the
  // rtt invalid. Report a lower rtt and verify the old/high value still is sent
  // in the callback.

  // Here, enough time must have passed in order to remove exactly the first
  // report and nothing else (>1500ms has passed since the first rtt).
  // So, this value is arrived by doing:
  // (kAvgRtt1)/1 * 0.7 + (kHighRtt+kLowRtt)/2 * 0.3 = 102.1
  static constexpr const int64_t kAvgRtt2 = 102;
  EXPECT_CALL(stats_observer, OnRttUpdate(kAvgRtt2, kHighRtt))
      .Times(1)
      .WillOnce(InvokeWithoutArgs([this] {
        // Advance time to make the high report invalid, the lower rtt should
        // now be in the callback.
        fake_clock_.AdvanceTimeMilliseconds(1000);
      }));

  static constexpr const int64_t kAvgRtt3 = 95;
  EXPECT_CALL(stats_observer, OnRttUpdate(kAvgRtt3, kLowRtt))
      .Times(1)
      .WillOnce(InvokeWithoutArgs([this] { loop_.Quit(); }));

  // Trigger the first rtt value and set off the chain of callbacks.
  AsyncSimulateRttUpdate(kFirstRtt);  // Reported at T0 (0ms).
  loop_.Run();

  call_stats_.DeregisterStatsObserver(&stats_observer);
}

TEST_F(CallStats2Test, LastProcessedRtt) {
  MockStatsObserver stats_observer;
  call_stats_.RegisterStatsObserver(&stats_observer);

  static constexpr const int64_t kRttLow = 10;
  static constexpr const int64_t kRttHigh = 30;
  // The following two average numbers dependend on average + weight
  // calculations in call_stats.cc.
  static constexpr const int64_t kAvgRtt1 = 13;
  static constexpr const int64_t kAvgRtt2 = 15;

  EXPECT_CALL(stats_observer, OnRttUpdate(kRttLow, kRttLow))
      .Times(1)
      .WillOnce(InvokeWithoutArgs([this] {
        EXPECT_EQ(kRttLow, call_stats_.LastProcessedRtt());
        // Don't advance the clock to make sure that low and high rtt values
        // are associated with the same time stamp.
        AsyncSimulateRttUpdate(kRttHigh);
      }));

  EXPECT_CALL(stats_observer, OnRttUpdate(kAvgRtt1, kRttHigh))
      .Times(AnyNumber())
      .WillOnce(InvokeWithoutArgs([this] {
        EXPECT_EQ(kAvgRtt1, call_stats_.LastProcessedRtt());
        fake_clock_.AdvanceTimeMilliseconds(CallStats::kUpdateInterval.ms());
        AsyncSimulateRttUpdate(kRttLow);
        AsyncSimulateRttUpdate(kRttHigh);
      }))
      .WillRepeatedly(Return());

  EXPECT_CALL(stats_observer, OnRttUpdate(kAvgRtt2, kRttHigh))
      .Times(AnyNumber())
      .WillOnce(InvokeWithoutArgs([this] {
        EXPECT_EQ(kAvgRtt2, call_stats_.LastProcessedRtt());
        loop_.Quit();
      }))
      .WillRepeatedly(Return());

  // Set a first values and verify that LastProcessedRtt initially returns the
  // average rtt.
  fake_clock_.AdvanceTimeMilliseconds(CallStats::kUpdateInterval.ms());
  AsyncSimulateRttUpdate(kRttLow);
  loop_.Run();
  EXPECT_EQ(kAvgRtt2, call_stats_.LastProcessedRtt());

  call_stats_.DeregisterStatsObserver(&stats_observer);
}

TEST_F(CallStats2Test, ProducesHistogramMetrics) {
  metrics::Reset();
  static constexpr const int64_t kRtt = 123;
  MockStatsObserver stats_observer;
  call_stats_.RegisterStatsObserver(&stats_observer);
  EXPECT_CALL(stats_observer, OnRttUpdate(kRtt, kRtt))
      .Times(AnyNumber())
      .WillRepeatedly(InvokeWithoutArgs([this] { loop_.Quit(); }));

  AsyncSimulateRttUpdate(kRtt);
  loop_.Run();
  fake_clock_.AdvanceTimeMilliseconds(metrics::kMinRunTimeInSeconds *
                                      CallStats::kUpdateInterval.ms());
  AsyncSimulateRttUpdate(kRtt);
  loop_.Run();

  call_stats_.DeregisterStatsObserver(&stats_observer);

  call_stats_.UpdateHistogramsForTest();

  EXPECT_METRIC_EQ(1, metrics::NumSamples(
                          "WebRTC.Video.AverageRoundTripTimeInMilliseconds"));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.Video.AverageRoundTripTimeInMilliseconds",
                            kRtt));
}

}  // namespace internal
}  // namespace webrtc
