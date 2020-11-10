/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/call_stats.h"

#include <memory>

#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/event.h"
#include "rtc_base/location.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "system_wrappers/include/metrics.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::AnyNumber;
using ::testing::InvokeWithoutArgs;
using ::testing::Return;

namespace webrtc {

class MockStatsObserver : public CallStatsObserver {
 public:
  MockStatsObserver() {}
  virtual ~MockStatsObserver() {}

  MOCK_METHOD(void, OnRttUpdate, (int64_t, int64_t), (override));
};

class CallStatsTest : public ::testing::Test {
 public:
  CallStatsTest() {
    process_thread_->RegisterModule(&call_stats_, RTC_FROM_HERE);
    process_thread_->Start();
  }
  ~CallStatsTest() override {
    process_thread_->Stop();
    process_thread_->DeRegisterModule(&call_stats_);
  }

  // Queues an rtt update call on the process thread.
  void AsyncSimulateRttUpdate(int64_t rtt) {
    RtcpRttStats* rtcp_rtt_stats = &call_stats_;
    process_thread_->PostTask(ToQueuedTask(
        [rtcp_rtt_stats, rtt] { rtcp_rtt_stats->OnRttUpdate(rtt); }));
  }

 protected:
  std::unique_ptr<ProcessThread> process_thread_{
      ProcessThread::Create("CallStats")};
  SimulatedClock fake_clock_{12345};
  CallStats call_stats_{&fake_clock_, process_thread_.get()};
};

TEST_F(CallStatsTest, AddAndTriggerCallback) {
  rtc::Event event;

  static constexpr const int64_t kRtt = 25;

  MockStatsObserver stats_observer;
  EXPECT_CALL(stats_observer, OnRttUpdate(kRtt, kRtt))
      .Times(1)
      .WillOnce(InvokeWithoutArgs([&event] { event.Set(); }));

  RtcpRttStats* rtcp_rtt_stats = &call_stats_;
  call_stats_.RegisterStatsObserver(&stats_observer);
  EXPECT_EQ(-1, rtcp_rtt_stats->LastProcessedRtt());

  AsyncSimulateRttUpdate(kRtt);

  EXPECT_TRUE(event.Wait(1000));

  EXPECT_EQ(kRtt, rtcp_rtt_stats->LastProcessedRtt());

  call_stats_.DeregisterStatsObserver(&stats_observer);
}

TEST_F(CallStatsTest, ProcessTime) {
  rtc::Event event;

  static constexpr const int64_t kRtt = 100;
  static constexpr const int64_t kRtt2 = 80;

  RtcpRttStats* rtcp_rtt_stats = &call_stats_;

  MockStatsObserver stats_observer;

  EXPECT_CALL(stats_observer, OnRttUpdate(kRtt, kRtt))
      .Times(2)
      .WillOnce(InvokeWithoutArgs([this] {
        // Advance clock and verify we get an update.
        fake_clock_.AdvanceTimeMilliseconds(CallStats::kUpdateIntervalMs);
      }))
      .WillRepeatedly(InvokeWithoutArgs([this, rtcp_rtt_stats] {
        rtcp_rtt_stats->OnRttUpdate(kRtt2);
        // Advance clock just too little to get an update.
        fake_clock_.AdvanceTimeMilliseconds(CallStats::kUpdateIntervalMs - 1);
      }));

  // In case you're reading this and wondering how this number is arrived at,
  // please see comments in the ChangeRtt test that go into some detail.
  static constexpr const int64_t kLastAvg = 94;
  EXPECT_CALL(stats_observer, OnRttUpdate(kLastAvg, kRtt2))
      .Times(1)
      .WillOnce(InvokeWithoutArgs([&event] { event.Set(); }));

  call_stats_.RegisterStatsObserver(&stats_observer);

  AsyncSimulateRttUpdate(kRtt);
  EXPECT_TRUE(event.Wait(1000));

  call_stats_.DeregisterStatsObserver(&stats_observer);
}

// Verify all observers get correct estimates and observers can be added and
// removed.
TEST_F(CallStatsTest, MultipleObservers) {
  MockStatsObserver stats_observer_1;
  call_stats_.RegisterStatsObserver(&stats_observer_1);
  // Add the second observer twice, there should still be only one report to the
  // observer.
  MockStatsObserver stats_observer_2;
  call_stats_.RegisterStatsObserver(&stats_observer_2);
  call_stats_.RegisterStatsObserver(&stats_observer_2);

  static constexpr const int64_t kRtt = 100;

  // Verify both observers are updated.
  rtc::Event ev1;
  rtc::Event ev2;
  EXPECT_CALL(stats_observer_1, OnRttUpdate(kRtt, kRtt))
      .Times(AnyNumber())
      .WillOnce(InvokeWithoutArgs([&ev1] { ev1.Set(); }))
      .WillRepeatedly(Return());
  EXPECT_CALL(stats_observer_2, OnRttUpdate(kRtt, kRtt))
      .Times(AnyNumber())
      .WillOnce(InvokeWithoutArgs([&ev2] { ev2.Set(); }))
      .WillRepeatedly(Return());
  AsyncSimulateRttUpdate(kRtt);
  ASSERT_TRUE(ev1.Wait(100));
  ASSERT_TRUE(ev2.Wait(100));

  // Deregister the second observer and verify update is only sent to the first
  // observer.
  call_stats_.DeregisterStatsObserver(&stats_observer_2);

  EXPECT_CALL(stats_observer_1, OnRttUpdate(kRtt, kRtt))
      .Times(AnyNumber())
      .WillOnce(InvokeWithoutArgs([&ev1] { ev1.Set(); }))
      .WillRepeatedly(Return());
  EXPECT_CALL(stats_observer_2, OnRttUpdate(kRtt, kRtt)).Times(0);
  AsyncSimulateRttUpdate(kRtt);
  ASSERT_TRUE(ev1.Wait(100));

  // Deregister the first observer.
  call_stats_.DeregisterStatsObserver(&stats_observer_1);

  // Now make sure we don't get any callbacks.
  EXPECT_CALL(stats_observer_1, OnRttUpdate(kRtt, kRtt)).Times(0);
  EXPECT_CALL(stats_observer_2, OnRttUpdate(kRtt, kRtt)).Times(0);
  AsyncSimulateRttUpdate(kRtt);

  // Force a call to Process().
  process_thread_->WakeUp(&call_stats_);

  // Flush the queue on the process thread to make sure we return after
  // Process() has been called.
  rtc::Event event;
  process_thread_->PostTask(ToQueuedTask([&event] { event.Set(); }));
  event.Wait(rtc::Event::kForever);
}

// Verify increasing and decreasing rtt triggers callbacks with correct values.
TEST_F(CallStatsTest, ChangeRtt) {
  // TODO(tommi): This test assumes things about how old reports are removed
  // inside of call_stats.cc. The threshold ms value is 1500ms, but it's not
  // clear here that how the clock is advanced, affects that algorithm and
  // subsequently the average reported rtt.

  MockStatsObserver stats_observer;
  call_stats_.RegisterStatsObserver(&stats_observer);
  RtcpRttStats* rtcp_rtt_stats = &call_stats_;

  rtc::Event event;

  static constexpr const int64_t kFirstRtt = 100;
  static constexpr const int64_t kLowRtt = kFirstRtt - 20;
  static constexpr const int64_t kHighRtt = kFirstRtt + 20;

  EXPECT_CALL(stats_observer, OnRttUpdate(kFirstRtt, kFirstRtt))
      .Times(1)
      .WillOnce(InvokeWithoutArgs([&rtcp_rtt_stats, this] {
        fake_clock_.AdvanceTimeMilliseconds(1000);
        rtcp_rtt_stats->OnRttUpdate(kHighRtt);  // Reported at T1 (1000ms).
      }));

  // TODO(tommi): This relies on the internal algorithms of call_stats.cc.
  // There's a weight factor there (0.3), that weighs the previous average to
  // the new one by 70%, so the number 103 in this case is arrived at like so:
  // (100) / 1 * 0.7 + (100+120)/2 * 0.3 = 103
  static constexpr const int64_t kAvgRtt1 = 103;
  EXPECT_CALL(stats_observer, OnRttUpdate(kAvgRtt1, kHighRtt))
      .Times(1)
      .WillOnce(InvokeWithoutArgs([&rtcp_rtt_stats, this] {
        // This interacts with an internal implementation detail in call_stats
        // that decays the oldest rtt value. See more below.
        fake_clock_.AdvanceTimeMilliseconds(1000);
        rtcp_rtt_stats->OnRttUpdate(kLowRtt);  // Reported at T2 (2000ms).
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
      .WillOnce(InvokeWithoutArgs([&event] { event.Set(); }));

  // Trigger the first rtt value and set off the chain of callbacks.
  AsyncSimulateRttUpdate(kFirstRtt);  // Reported at T0 (0ms).
  EXPECT_TRUE(event.Wait(1000));

  call_stats_.DeregisterStatsObserver(&stats_observer);
}

TEST_F(CallStatsTest, LastProcessedRtt) {
  rtc::Event event;
  MockStatsObserver stats_observer;
  call_stats_.RegisterStatsObserver(&stats_observer);
  RtcpRttStats* rtcp_rtt_stats = &call_stats_;

  static constexpr const int64_t kRttLow = 10;
  static constexpr const int64_t kRttHigh = 30;
  // The following two average numbers dependend on average + weight
  // calculations in call_stats.cc.
  static constexpr const int64_t kAvgRtt1 = 13;
  static constexpr const int64_t kAvgRtt2 = 15;

  EXPECT_CALL(stats_observer, OnRttUpdate(kRttLow, kRttLow))
      .Times(1)
      .WillOnce(InvokeWithoutArgs([rtcp_rtt_stats] {
        EXPECT_EQ(kRttLow, rtcp_rtt_stats->LastProcessedRtt());
        // Don't advance the clock to make sure that low and high rtt values
        // are associated with the same time stamp.
        rtcp_rtt_stats->OnRttUpdate(kRttHigh);
      }));

  EXPECT_CALL(stats_observer, OnRttUpdate(kAvgRtt1, kRttHigh))
      .Times(1)
      .WillOnce(InvokeWithoutArgs([rtcp_rtt_stats, this] {
        EXPECT_EQ(kAvgRtt1, rtcp_rtt_stats->LastProcessedRtt());
        fake_clock_.AdvanceTimeMilliseconds(CallStats::kUpdateIntervalMs);
        rtcp_rtt_stats->OnRttUpdate(kRttLow);
        rtcp_rtt_stats->OnRttUpdate(kRttHigh);
      }));

  EXPECT_CALL(stats_observer, OnRttUpdate(kAvgRtt2, kRttHigh))
      .Times(1)
      .WillOnce(InvokeWithoutArgs([rtcp_rtt_stats, &event] {
        EXPECT_EQ(kAvgRtt2, rtcp_rtt_stats->LastProcessedRtt());
        event.Set();
      }));

  // Set a first values and verify that LastProcessedRtt initially returns the
  // average rtt.
  fake_clock_.AdvanceTimeMilliseconds(CallStats::kUpdateIntervalMs);
  AsyncSimulateRttUpdate(kRttLow);
  EXPECT_TRUE(event.Wait(1000));
  EXPECT_EQ(kAvgRtt2, rtcp_rtt_stats->LastProcessedRtt());

  call_stats_.DeregisterStatsObserver(&stats_observer);
}

TEST_F(CallStatsTest, ProducesHistogramMetrics) {
  metrics::Reset();
  rtc::Event event;
  static constexpr const int64_t kRtt = 123;
  MockStatsObserver stats_observer;
  call_stats_.RegisterStatsObserver(&stats_observer);
  EXPECT_CALL(stats_observer, OnRttUpdate(kRtt, kRtt))
      .Times(AnyNumber())
      .WillRepeatedly(InvokeWithoutArgs([&event] { event.Set(); }));

  AsyncSimulateRttUpdate(kRtt);
  EXPECT_TRUE(event.Wait(1000));
  fake_clock_.AdvanceTimeMilliseconds(metrics::kMinRunTimeInSeconds *
                                      CallStats::kUpdateIntervalMs);
  AsyncSimulateRttUpdate(kRtt);
  EXPECT_TRUE(event.Wait(1000));

  call_stats_.DeregisterStatsObserver(&stats_observer);

  process_thread_->Stop();
  call_stats_.UpdateHistogramsForTest();

  EXPECT_METRIC_EQ(1, metrics::NumSamples(
                          "WebRTC.Video.AverageRoundTripTimeInMilliseconds"));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.Video.AverageRoundTripTimeInMilliseconds",
                            kRtt));
}

}  // namespace webrtc
