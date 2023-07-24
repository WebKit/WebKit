/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/time_controller/external_time_controller.h"

#include <atomic>
#include <memory>
#include <utility>

#include "rtc_base/event.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/task_utils/repeating_task.h"
#include "test/gmock.h"
#include "test/gtest.h"

// NOTE: Since these tests rely on real time behavior, they will be flaky
// if run on heavily loaded systems.
namespace webrtc {
namespace {
using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::Return;
constexpr Timestamp kStartTime = Timestamp::Seconds(1000);

class FakeAlarm : public ControlledAlarmClock {
 public:
  explicit FakeAlarm(Timestamp start_time);

  Clock* GetClock() override;
  bool ScheduleAlarmAt(Timestamp deadline) override;
  void SetCallback(std::function<void()> callback) override;
  void Sleep(TimeDelta duration) override;

 private:
  SimulatedClock clock_;
  Timestamp deadline_;
  std::function<void()> callback_;
};

FakeAlarm::FakeAlarm(Timestamp start_time)
    : clock_(start_time),
      deadline_(Timestamp::PlusInfinity()),
      callback_([] {}) {}

Clock* FakeAlarm::GetClock() {
  return &clock_;
}

bool FakeAlarm::ScheduleAlarmAt(Timestamp deadline) {
  if (deadline < deadline_) {
    deadline_ = deadline;
    return true;
  }
  return false;
}

void FakeAlarm::SetCallback(std::function<void()> callback) {
  callback_ = callback;
}

void FakeAlarm::Sleep(TimeDelta duration) {
  Timestamp end_time = clock_.CurrentTime() + duration;

  while (deadline_ <= end_time) {
    clock_.AdvanceTime(deadline_ - clock_.CurrentTime());
    deadline_ = Timestamp::PlusInfinity();
    callback_();
  }

  clock_.AdvanceTime(end_time - clock_.CurrentTime());
}

}  // namespace

TEST(ExternalTimeControllerTest, TaskIsStoppedOnStop) {
  const TimeDelta kShortInterval = TimeDelta::Millis(5);
  const TimeDelta kLongInterval = TimeDelta::Millis(20);
  const int kShortIntervalCount = 4;
  const int kMargin = 1;
  FakeAlarm alarm(kStartTime);
  ExternalTimeController time_simulation(&alarm);
  rtc::TaskQueue task_queue(
      time_simulation.GetTaskQueueFactory()->CreateTaskQueue(
          "TestQueue", TaskQueueFactory::Priority::NORMAL));
  std::atomic_int counter(0);
  auto handle = RepeatingTaskHandle::Start(task_queue.Get(), [&] {
    if (++counter >= kShortIntervalCount)
      return kLongInterval;
    return kShortInterval;
  });
  // Sleep long enough to go through the initial phase.
  time_simulation.AdvanceTime(kShortInterval * (kShortIntervalCount + kMargin));
  EXPECT_EQ(counter.load(), kShortIntervalCount);

  task_queue.PostTask(
      [handle = std::move(handle)]() mutable { handle.Stop(); });

  // Sleep long enough that the task would run at least once more if not
  // stopped.
  time_simulation.AdvanceTime(kLongInterval * 2);
  EXPECT_EQ(counter.load(), kShortIntervalCount);
}

TEST(ExternalTimeControllerTest, TaskCanStopItself) {
  std::atomic_int counter(0);
  FakeAlarm alarm(kStartTime);
  ExternalTimeController time_simulation(&alarm);
  rtc::TaskQueue task_queue(
      time_simulation.GetTaskQueueFactory()->CreateTaskQueue(
          "TestQueue", TaskQueueFactory::Priority::NORMAL));

  RepeatingTaskHandle handle;
  task_queue.PostTask([&] {
    handle = RepeatingTaskHandle::Start(task_queue.Get(), [&] {
      ++counter;
      handle.Stop();
      return TimeDelta::Millis(2);
    });
  });
  time_simulation.AdvanceTime(TimeDelta::Millis(10));
  EXPECT_EQ(counter.load(), 1);
}

TEST(ExternalTimeControllerTest, YieldForTask) {
  FakeAlarm alarm(kStartTime);
  ExternalTimeController time_simulation(&alarm);

  rtc::TaskQueue task_queue(
      time_simulation.GetTaskQueueFactory()->CreateTaskQueue(
          "TestQueue", TaskQueueFactory::Priority::NORMAL));

  rtc::Event event;
  task_queue.PostTask([&] { event.Set(); });
  EXPECT_TRUE(event.Wait(TimeDelta::Millis(200)));
}

TEST(ExternalTimeControllerTest, TasksYieldToEachOther) {
  FakeAlarm alarm(kStartTime);
  ExternalTimeController time_simulation(&alarm);

  rtc::TaskQueue task_queue(
      time_simulation.GetTaskQueueFactory()->CreateTaskQueue(
          "TestQueue", TaskQueueFactory::Priority::NORMAL));
  rtc::TaskQueue other_queue(
      time_simulation.GetTaskQueueFactory()->CreateTaskQueue(
          "OtherQueue", TaskQueueFactory::Priority::NORMAL));

  task_queue.PostTask([&] {
    rtc::Event event;
    other_queue.PostTask([&] { event.Set(); });
    EXPECT_TRUE(event.Wait(TimeDelta::Millis(200)));
  });

  time_simulation.AdvanceTime(TimeDelta::Millis(300));
}

TEST(ExternalTimeControllerTest, CurrentTaskQueue) {
  FakeAlarm alarm(kStartTime);
  ExternalTimeController time_simulation(&alarm);

  rtc::TaskQueue task_queue(
      time_simulation.GetTaskQueueFactory()->CreateTaskQueue(
          "TestQueue", TaskQueueFactory::Priority::NORMAL));

  task_queue.PostTask([&] { EXPECT_TRUE(task_queue.IsCurrent()); });

  time_simulation.AdvanceTime(TimeDelta::Millis(10));
}

}  // namespace webrtc
