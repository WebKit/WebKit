/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "webrtc/p2p/quic/quicconnectionhelper.h"

#include "net/quic/quic_time.h"
#include "webrtc/base/gunit.h"

using cricket::QuicAlarm;
using cricket::QuicConnectionHelper;

using net::QuicClock;
using net::QuicTime;
using net::QuicWallTime;

// Clock that can be set to arbitrary times.
class MockClock : public QuicClock {
 public:
  MockClock() : now_(QuicTime::Zero()) {}

  void AdvanceTime(QuicTime::Delta delta) { now_ = now_.Add(delta); }

  QuicTime Now() const override { return now_; }

  QuicTime ApproximateNow() const override { return now_; }

  QuicWallTime WallNow() const override {
    return QuicWallTime::FromUNIXSeconds(
        now_.Subtract(QuicTime::Zero()).ToSeconds());
  }

  base::TimeTicks NowInTicks() const {
    base::TimeTicks ticks;
    return ticks + base::TimeDelta::FromMicroseconds(
                       now_.Subtract(QuicTime::Zero()).ToMicroseconds());
  }

 private:
  QuicTime now_;
};

// Implements OnAlarm() event which alarm triggers.
class MockAlarmDelegate : public QuicAlarm::Delegate {
 public:
  MockAlarmDelegate() : fired_(false) {}

  void OnAlarm() override { fired_ = true; }

  bool fired() const { return fired_; }
  void Clear() { fired_ = false; }

 private:
  bool fired_;
};

class QuicAlarmTest : public ::testing::Test {
 public:
  QuicAlarmTest()
      : delegate_(new MockAlarmDelegate()),
        alarm_(new QuicAlarm(
            &clock_,
            rtc::Thread::Current(),
            net::QuicArenaScopedPtr<net::QuicAlarm::Delegate>(delegate_))) {}

  // Make the alarm fire after the given microseconds (us). Negative values
  // imply the alarm should fire immediately.
  void SetTime(int us) {
    QuicTime::Delta delta = QuicTime::Delta::FromMicroseconds(us);
    alarm_->Set(clock_.Now().Add(delta));
  }

  // Make rtc::Thread::Current() process the next message.
  void ProcessNextMessage() { rtc::Thread::Current()->ProcessMessages(0); }

 protected:
  // Handles event that alarm fires.
  MockAlarmDelegate* delegate_;
  // Used for setting clock time relative to alarm.
  MockClock clock_;

  std::unique_ptr<QuicAlarm> alarm_;
};

// Test that the alarm is fired.
TEST_F(QuicAlarmTest, FireAlarm) {
  SetTime(-1);
  ProcessNextMessage();
  ASSERT_TRUE(delegate_->fired());
  ASSERT_EQ(QuicTime::Zero(), alarm_->deadline());
}

// Test cancellation of alarm when it is set to fire.
TEST_F(QuicAlarmTest, CancelAlarmAfterSet) {
  // TODO(mikescarlett): Test will fail in the future if
  // rtc::Thread::PostDelayed calls the delegate synchronously for times <= 0.
  // Rewrite this when rtc::Thread is able to use a mock clock.
  SetTime(-1);
  alarm_->Cancel();
  ProcessNextMessage();
  ASSERT_FALSE(delegate_->fired());
}

// Test cancellation of alarm when it is not set to fire.
TEST_F(QuicAlarmTest, CancelAlarmBeforeSet) {
  alarm_->Cancel();
  ProcessNextMessage();
  ASSERT_FALSE(delegate_->fired());
}

// Test that timing for posting task is accurate.
TEST_F(QuicAlarmTest, AlarmGetDelay) {
  SetTime(1000000);
  EXPECT_EQ(1000, alarm_->GetDelay());
  clock_.AdvanceTime(QuicTime::Delta::FromMicroseconds(300000));
  EXPECT_EQ(700, alarm_->GetDelay());
}
