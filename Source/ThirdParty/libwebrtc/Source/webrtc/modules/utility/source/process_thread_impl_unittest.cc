/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <utility>

#include "modules/include/module.h"
#include "modules/utility/source/process_thread_impl.h"
#include "rtc_base/location.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/timeutils.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {

using ::testing::_;
using ::testing::DoAll;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SetArgPointee;

// The length of time, in milliseconds, to wait for an event to become signaled.
// Set to a fairly large value as there is quite a bit of variation on some
// Windows bots.
static const int kEventWaitTimeout = 500;

class MockModule : public Module {
 public:
  MOCK_METHOD0(TimeUntilNextProcess, int64_t());
  MOCK_METHOD0(Process, void());
  MOCK_METHOD1(ProcessThreadAttached, void(ProcessThread*));
};

class RaiseEventTask : public rtc::QueuedTask {
 public:
  RaiseEventTask(EventWrapper* event) : event_(event) {}
  bool Run() override {
    event_->Set();
    return true;
  }

 private:
  EventWrapper* event_;
};

ACTION_P(SetEvent, event) {
  event->Set();
}

ACTION_P(Increment, counter) {
  ++(*counter);
}

ACTION_P(SetTimestamp, ptr) {
  *ptr = rtc::TimeMillis();
}

TEST(ProcessThreadImpl, StartStop) {
  ProcessThreadImpl thread("ProcessThread");
  thread.Start();
  thread.Stop();
}

TEST(ProcessThreadImpl, MultipleStartStop) {
  ProcessThreadImpl thread("ProcessThread");
  for (int i = 0; i < 5; ++i) {
    thread.Start();
    thread.Stop();
  }
}

// Verifies that we get at least call back to Process() on the worker thread.
TEST(ProcessThreadImpl, ProcessCall) {
  ProcessThreadImpl thread("ProcessThread");
  thread.Start();

  std::unique_ptr<EventWrapper> event(EventWrapper::Create());

  MockModule module;
  EXPECT_CALL(module, TimeUntilNextProcess())
      .WillOnce(Return(0))
      .WillRepeatedly(Return(1));
  EXPECT_CALL(module, Process())
      .WillOnce(DoAll(SetEvent(event.get()), Return()))
      .WillRepeatedly(Return());
  EXPECT_CALL(module, ProcessThreadAttached(&thread)).Times(1);

  thread.RegisterModule(&module, RTC_FROM_HERE);
  EXPECT_EQ(kEventSignaled, event->Wait(kEventWaitTimeout));

  EXPECT_CALL(module, ProcessThreadAttached(nullptr)).Times(1);
  thread.Stop();
}

// Same as ProcessCall except the module is registered before the
// call to Start().
TEST(ProcessThreadImpl, ProcessCall2) {
  ProcessThreadImpl thread("ProcessThread");
  std::unique_ptr<EventWrapper> event(EventWrapper::Create());

  MockModule module;
  EXPECT_CALL(module, TimeUntilNextProcess())
      .WillOnce(Return(0))
      .WillRepeatedly(Return(1));
  EXPECT_CALL(module, Process())
      .WillOnce(DoAll(SetEvent(event.get()), Return()))
      .WillRepeatedly(Return());

  thread.RegisterModule(&module, RTC_FROM_HERE);

  EXPECT_CALL(module, ProcessThreadAttached(&thread)).Times(1);
  thread.Start();
  EXPECT_EQ(kEventSignaled, event->Wait(kEventWaitTimeout));

  EXPECT_CALL(module, ProcessThreadAttached(nullptr)).Times(1);
  thread.Stop();
}

// Tests setting up a module for callbacks and then unregister that module.
// After unregistration, we should not receive any further callbacks.
TEST(ProcessThreadImpl, Deregister) {
  ProcessThreadImpl thread("ProcessThread");
  std::unique_ptr<EventWrapper> event(EventWrapper::Create());

  int process_count = 0;
  MockModule module;
  EXPECT_CALL(module, TimeUntilNextProcess())
      .WillOnce(Return(0))
      .WillRepeatedly(Return(1));
  EXPECT_CALL(module, Process())
      .WillOnce(
          DoAll(SetEvent(event.get()), Increment(&process_count), Return()))
      .WillRepeatedly(DoAll(Increment(&process_count), Return()));

  thread.RegisterModule(&module, RTC_FROM_HERE);

  EXPECT_CALL(module, ProcessThreadAttached(&thread)).Times(1);
  thread.Start();

  EXPECT_EQ(kEventSignaled, event->Wait(kEventWaitTimeout));

  EXPECT_CALL(module, ProcessThreadAttached(nullptr)).Times(1);
  thread.DeRegisterModule(&module);

  EXPECT_GE(process_count, 1);
  int count_after_deregister = process_count;

  // We shouldn't get any more callbacks.
  EXPECT_EQ(kEventTimeout, event->Wait(20));
  EXPECT_EQ(count_after_deregister, process_count);
  thread.Stop();
}

// Helper function for testing receiving a callback after a certain amount of
// time.  There's some variance of timing built into it to reduce chance of
// flakiness on bots.
void ProcessCallAfterAFewMs(int64_t milliseconds) {
  ProcessThreadImpl thread("ProcessThread");
  thread.Start();

  std::unique_ptr<EventWrapper> event(EventWrapper::Create());

  MockModule module;
  int64_t start_time = 0;
  int64_t called_time = 0;
  EXPECT_CALL(module, TimeUntilNextProcess())
      .WillOnce(DoAll(SetTimestamp(&start_time), Return(milliseconds)))
      .WillRepeatedly(Return(milliseconds));
  EXPECT_CALL(module, Process())
      .WillOnce(
          DoAll(SetTimestamp(&called_time), SetEvent(event.get()), Return()))
      .WillRepeatedly(Return());

  EXPECT_CALL(module, ProcessThreadAttached(&thread)).Times(1);
  thread.RegisterModule(&module, RTC_FROM_HERE);

  // Add a buffer of 50ms due to slowness of some trybots
  // (e.g. win_drmemory_light)
  EXPECT_EQ(kEventSignaled, event->Wait(milliseconds + 50));

  EXPECT_CALL(module, ProcessThreadAttached(nullptr)).Times(1);
  thread.Stop();

  ASSERT_GT(start_time, 0);
  ASSERT_GT(called_time, 0);
  // Use >= instead of > since due to rounding and timer accuracy (or lack
  // thereof), can make the test run in "0"ms time.
  EXPECT_GE(called_time, start_time);
  // Check for an acceptable range.
  uint32_t diff = called_time - start_time;
  EXPECT_GE(diff, milliseconds - 15);
  EXPECT_LT(diff, milliseconds + 15);
}

// DISABLED for now since the virtual build bots are too slow :(
// TODO(tommi): Fix.
TEST(ProcessThreadImpl, DISABLED_ProcessCallAfter5ms) {
  ProcessCallAfterAFewMs(5);
}

// DISABLED for now since the virtual build bots are too slow :(
// TODO(tommi): Fix.
TEST(ProcessThreadImpl, DISABLED_ProcessCallAfter50ms) {
  ProcessCallAfterAFewMs(50);
}

// DISABLED for now since the virtual build bots are too slow :(
// TODO(tommi): Fix.
TEST(ProcessThreadImpl, DISABLED_ProcessCallAfter200ms) {
  ProcessCallAfterAFewMs(200);
}

// Runs callbacks with the goal of getting up to 50 callbacks within a second
// (on average 1 callback every 20ms).  On real hardware, we're usually pretty
// close to that, but the test bots that run on virtual machines, will
// typically be in the range 30-40 callbacks.
// DISABLED for now since this can take up to 2 seconds to run on the slowest
// build bots.
// TODO(tommi): Fix.
TEST(ProcessThreadImpl, DISABLED_Process50Times) {
  ProcessThreadImpl thread("ProcessThread");
  thread.Start();

  std::unique_ptr<EventWrapper> event(EventWrapper::Create());

  MockModule module;
  int callback_count = 0;
  // Ask for a callback after 20ms.
  EXPECT_CALL(module, TimeUntilNextProcess()).WillRepeatedly(Return(20));
  EXPECT_CALL(module, Process())
      .WillRepeatedly(DoAll(Increment(&callback_count), Return()));

  EXPECT_CALL(module, ProcessThreadAttached(&thread)).Times(1);
  thread.RegisterModule(&module, RTC_FROM_HERE);

  EXPECT_EQ(kEventTimeout, event->Wait(1000));

  EXPECT_CALL(module, ProcessThreadAttached(nullptr)).Times(1);
  thread.Stop();

  printf("Callback count: %i\n", callback_count);
  // Check that we got called back up to 50 times.
  // Some of the try bots run on slow virtual machines, so the lower bound
  // is much more relaxed to avoid flakiness.
  EXPECT_GE(callback_count, 25);
  EXPECT_LE(callback_count, 50);
}

// Tests that we can wake up the worker thread to give us a callback right
// away when we know the thread is sleeping.
TEST(ProcessThreadImpl, WakeUp) {
  ProcessThreadImpl thread("ProcessThread");
  thread.Start();

  std::unique_ptr<EventWrapper> started(EventWrapper::Create());
  std::unique_ptr<EventWrapper> called(EventWrapper::Create());

  MockModule module;
  int64_t start_time;
  int64_t called_time;

  // Ask for a callback after 1000ms.
  // TimeUntilNextProcess will be called twice.
  // The first time we use it to get the thread into a waiting state.
  // Then we  wake the thread and there should not be another call made to
  // TimeUntilNextProcess before Process() is called.
  // The second time TimeUntilNextProcess is then called, is after Process
  // has been called and we don't expect any more calls.
  EXPECT_CALL(module, TimeUntilNextProcess())
      .WillOnce(DoAll(SetTimestamp(&start_time), SetEvent(started.get()),
                      Return(1000)))
      .WillOnce(Return(1000));
  EXPECT_CALL(module, Process())
      .WillOnce(
          DoAll(SetTimestamp(&called_time), SetEvent(called.get()), Return()))
      .WillRepeatedly(Return());

  EXPECT_CALL(module, ProcessThreadAttached(&thread)).Times(1);
  thread.RegisterModule(&module, RTC_FROM_HERE);

  EXPECT_EQ(kEventSignaled, started->Wait(kEventWaitTimeout));
  thread.WakeUp(&module);
  EXPECT_EQ(kEventSignaled, called->Wait(kEventWaitTimeout));

  EXPECT_CALL(module, ProcessThreadAttached(nullptr)).Times(1);
  thread.Stop();

  EXPECT_GE(called_time, start_time);
  uint32_t diff = called_time - start_time;
  // We should have been called back much quicker than 1sec.
  EXPECT_LE(diff, 100u);
}

// Tests that we can post a task that gets run straight away on the worker
// thread.
TEST(ProcessThreadImpl, PostTask) {
  ProcessThreadImpl thread("ProcessThread");
  std::unique_ptr<EventWrapper> task_ran(EventWrapper::Create());
  std::unique_ptr<RaiseEventTask> task(new RaiseEventTask(task_ran.get()));
  thread.Start();
  thread.PostTask(std::move(task));
  EXPECT_EQ(kEventSignaled, task_ran->Wait(kEventWaitTimeout));
  thread.Stop();
}

}  // namespace webrtc
