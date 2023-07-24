/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/time_controller/simulated_time_controller.h"

#include <atomic>
#include <memory>

#include "rtc_base/event.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/task_queue_for_test.h"
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
}  // namespace

TEST(SimulatedTimeControllerTest, TaskIsStoppedOnStop) {
  const TimeDelta kShortInterval = TimeDelta::Millis(5);
  const TimeDelta kLongInterval = TimeDelta::Millis(20);
  const int kShortIntervalCount = 4;
  const int kMargin = 1;
  GlobalSimulatedTimeController time_simulation(kStartTime);
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

TEST(SimulatedTimeControllerTest, TaskCanStopItself) {
  std::atomic_int counter(0);
  GlobalSimulatedTimeController time_simulation(kStartTime);
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

TEST(SimulatedTimeControllerTest, Example) {
  class ObjectOnTaskQueue {
   public:
    void DoPeriodicTask() {}
    TimeDelta TimeUntilNextRun() { return TimeDelta::Millis(100); }
    void StartPeriodicTask(RepeatingTaskHandle* handle,
                           rtc::TaskQueue* task_queue) {
      *handle = RepeatingTaskHandle::Start(task_queue->Get(), [this] {
        DoPeriodicTask();
        return TimeUntilNextRun();
      });
    }
  };
  GlobalSimulatedTimeController time_simulation(kStartTime);
  rtc::TaskQueue task_queue(
      time_simulation.GetTaskQueueFactory()->CreateTaskQueue(
          "TestQueue", TaskQueueFactory::Priority::NORMAL));
  auto object = std::make_unique<ObjectOnTaskQueue>();
  // Create and start the periodic task.
  RepeatingTaskHandle handle;
  object->StartPeriodicTask(&handle, &task_queue);
  // Restart the task
  task_queue.PostTask(
      [handle = std::move(handle)]() mutable { handle.Stop(); });
  object->StartPeriodicTask(&handle, &task_queue);
  task_queue.PostTask(
      [handle = std::move(handle)]() mutable { handle.Stop(); });

  task_queue.PostTask([object = std::move(object)] {});
}

TEST(SimulatedTimeControllerTest, DelayTaskRunOnTime) {
  GlobalSimulatedTimeController time_simulation(kStartTime);
  std::unique_ptr<TaskQueueBase, TaskQueueDeleter> task_queue =
      time_simulation.GetTaskQueueFactory()->CreateTaskQueue(
          "TestQueue", TaskQueueFactory::Priority::NORMAL);

  bool delay_task_executed = false;
  task_queue->PostDelayedTask([&] { delay_task_executed = true; },
                              TimeDelta::Millis(10));

  time_simulation.AdvanceTime(TimeDelta::Millis(10));
  EXPECT_TRUE(delay_task_executed);
}

TEST(SimulatedTimeControllerTest, ThreadYeildsOnSynchronousCall) {
  GlobalSimulatedTimeController sim(kStartTime);
  auto main_thread = sim.GetMainThread();
  auto t2 = sim.CreateThread("thread", nullptr);
  bool task_has_run = false;
  // Posting a task to the main thread, this should not run until AdvanceTime is
  // called.
  main_thread->PostTask([&] { task_has_run = true; });
  SendTask(t2.get(), [] {
    rtc::Event yield_event;
    // Wait() triggers YieldExecution() which will runs message processing on
    // all threads that are not in the yielded set.

    yield_event.Wait(TimeDelta::Zero());
  });
  // Since we are doing an invoke from the main thread, we don't expect the main
  // thread message loop to be processed.
  EXPECT_FALSE(task_has_run);
  sim.AdvanceTime(TimeDelta::Seconds(1));
  ASSERT_TRUE(task_has_run);
}

}  // namespace webrtc
