/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/single_threaded_task_queue.h"

#include <atomic>
#include <memory>
#include <vector>

#include "rtc_base/event.h"
#include "rtc_base/ptr_util.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

namespace {

using TaskId = SingleThreadedTaskQueueForTesting::TaskId;

// Test should not rely on the object under test not being faulty. If the task
// queue ever blocks forever, we want the tests to fail, rather than hang.
constexpr int kMaxWaitTimeMs = 10000;

TEST(SingleThreadedTaskQueueForTestingTest, SanityConstructionDestruction) {
  SingleThreadedTaskQueueForTesting task_queue("task_queue");
}

TEST(SingleThreadedTaskQueueForTestingTest, ExecutesPostedTasks) {
  SingleThreadedTaskQueueForTesting task_queue("task_queue");

  std::atomic<bool> executed(false);
  rtc::Event done(true, false);

  task_queue.PostTask([&executed, &done]() {
    executed.store(true);
    done.Set();
  });
  ASSERT_TRUE(done.Wait(kMaxWaitTimeMs));

  EXPECT_TRUE(executed.load());
}

TEST(SingleThreadedTaskQueueForTestingTest,
     PostMultipleTasksFromSameExternalThread) {
  SingleThreadedTaskQueueForTesting task_queue("task_queue");

  constexpr size_t kCount = 3;
  std::atomic<bool> executed[kCount];
  for (std::atomic<bool>& exec : executed) {
    exec.store(false);
  }

  std::vector<std::unique_ptr<rtc::Event>> done_events;
  for (size_t i = 0; i < kCount; i++) {
    done_events.emplace_back(rtc::MakeUnique<rtc::Event>(false, false));
  }

  // To avoid the tasks which comprise the actual test from running before they
  // have all be posted, which could result in only one task ever being in the
  // queue at any given time, post one waiting task that would block the
  // task-queue, and unblock only after all tasks have been posted.
  rtc::Event rendezvous(true, false);
  task_queue.PostTask([&rendezvous]() {
    ASSERT_TRUE(rendezvous.Wait(kMaxWaitTimeMs));
  });

  // Post the tasks which comprise the test.
  for (size_t i = 0; i < kCount; i++) {
    task_queue.PostTask([&executed, &done_events, i]() {  // |i| by value.
      executed[i].store(true);
      done_events[i]->Set();
    });
  }

  rendezvous.Set();  // Release the task-queue.

  // Wait until the task queue has executed all the tasks.
  for (size_t i = 0; i < kCount; i++) {
    ASSERT_TRUE(done_events[i]->Wait(kMaxWaitTimeMs));
  }

  for (size_t i = 0; i < kCount; i++) {
    EXPECT_TRUE(executed[i].load());
  }
}

TEST(SingleThreadedTaskQueueForTestingTest, PostToTaskQueueFromOwnThread) {
  SingleThreadedTaskQueueForTesting task_queue("task_queue");

  std::atomic<bool> executed(false);
  rtc::Event done(true, false);

  auto internally_posted_task = [&executed, &done]() {
    executed.store(true);
    done.Set();
  };

  auto externally_posted_task = [&task_queue, &internally_posted_task]() {
    task_queue.PostTask(internally_posted_task);
  };

  task_queue.PostTask(externally_posted_task);

  ASSERT_TRUE(done.Wait(kMaxWaitTimeMs));
  EXPECT_TRUE(executed.load());
}

TEST(SingleThreadedTaskQueueForTestingTest, TasksExecutedInSequence) {
  SingleThreadedTaskQueueForTesting task_queue("task_queue");

  // The first task would perform:
  // accumulator = 10 * accumulator + i
  // Where |i| is 1, 2 and 3 for the 1st, 2nd and 3rd tasks, respectively.
  // The result would be 123 if and only iff the tasks were executed in order.
  size_t accumulator = 0;
  size_t expected_value = 0;  // Updates to the correct value.

  // Prevent the chain from being set in motion before we've had time to
  // schedule it all, lest the queue only contain one task at a time.
  rtc::Event rendezvous(true, false);
  task_queue.PostTask([&rendezvous]() {
    ASSERT_TRUE(rendezvous.Wait(kMaxWaitTimeMs));
  });

  for (size_t i = 0; i < 3; i++) {
    task_queue.PostTask([&accumulator, i]() {  // |i| passed by value.
      accumulator = 10 * accumulator + i;
    });
    expected_value = 10 * expected_value + i;
  }

  // The test will wait for the task-queue to finish.
  rtc::Event done(true, false);
  task_queue.PostTask([&done]() {
    done.Set();
  });

  rendezvous.Set();  // Set the chain in motion.

  ASSERT_TRUE(done.Wait(kMaxWaitTimeMs));

  EXPECT_EQ(accumulator, expected_value);
}

TEST(SingleThreadedTaskQueueForTestingTest, ExecutesPostedDelayedTask) {
  SingleThreadedTaskQueueForTesting task_queue("task_queue");

  std::atomic<bool> executed(false);
  rtc::Event done(true, false);

  constexpr int64_t delay_ms = 20;
  static_assert(delay_ms < kMaxWaitTimeMs / 2, "Delay too long for tests.");

  task_queue.PostDelayedTask([&executed, &done]() {
    executed.store(true);
    done.Set();
  }, delay_ms);
  ASSERT_TRUE(done.Wait(kMaxWaitTimeMs));

  EXPECT_TRUE(executed.load());
}

TEST(SingleThreadedTaskQueueForTestingTest, DoesNotExecuteDelayedTaskTooSoon) {
  SingleThreadedTaskQueueForTesting task_queue("task_queue");

  std::atomic<bool> executed(false);

  constexpr int64_t delay_ms = 2000;
  static_assert(delay_ms < kMaxWaitTimeMs / 2, "Delay too long for tests.");

  task_queue.PostDelayedTask([&executed]() {
    executed.store(true);
  }, delay_ms);

  // Wait less than is enough, make sure the task was not yet executed.
  rtc::Event not_done(true, false);
  ASSERT_FALSE(not_done.Wait(delay_ms / 2));
  EXPECT_FALSE(executed.load());
}

TEST(SingleThreadedTaskQueueForTestingTest,
     TaskWithLesserDelayPostedAfterFirstDelayedTaskExectuedBeforeFirst) {
  SingleThreadedTaskQueueForTesting task_queue("task_queue");

  std::atomic<bool> earlier_executed(false);
  constexpr int64_t earlier_delay_ms = 500;

  std::atomic<bool> later_executed(false);
  constexpr int64_t later_delay_ms = 1000;

  static_assert(earlier_delay_ms + later_delay_ms < kMaxWaitTimeMs / 2,
                "Delay too long for tests.");

  rtc::Event done(true, false);

  auto earlier_task = [&earlier_executed, &later_executed]() {
    EXPECT_FALSE(later_executed.load());
    earlier_executed.store(true);
  };

  auto later_task = [&earlier_executed, &later_executed, &done]() {
    EXPECT_TRUE(earlier_executed.load());
    later_executed.store(true);
    done.Set();
  };

  task_queue.PostDelayedTask(later_task, later_delay_ms);
  task_queue.PostDelayedTask(earlier_task, earlier_delay_ms);

  ASSERT_TRUE(done.Wait(kMaxWaitTimeMs));
  ASSERT_TRUE(earlier_executed);
  ASSERT_TRUE(later_executed);
}

TEST(SingleThreadedTaskQueueForTestingTest,
     TaskWithGreaterDelayPostedAfterFirstDelayedTaskExectuedAfterFirst) {
  SingleThreadedTaskQueueForTesting task_queue("task_queue");

  std::atomic<bool> earlier_executed(false);
  constexpr int64_t earlier_delay_ms = 500;

  std::atomic<bool> later_executed(false);
  constexpr int64_t later_delay_ms = 1000;

  static_assert(earlier_delay_ms + later_delay_ms < kMaxWaitTimeMs / 2,
                "Delay too long for tests.");

  rtc::Event done(true, false);

  auto earlier_task = [&earlier_executed, &later_executed]() {
    EXPECT_FALSE(later_executed.load());
    earlier_executed.store(true);
  };

  auto later_task = [&earlier_executed, &later_executed, &done]() {
    EXPECT_TRUE(earlier_executed.load());
    later_executed.store(true);
    done.Set();
  };

  task_queue.PostDelayedTask(earlier_task, earlier_delay_ms);
  task_queue.PostDelayedTask(later_task, later_delay_ms);

  ASSERT_TRUE(done.Wait(kMaxWaitTimeMs));
  ASSERT_TRUE(earlier_executed);
  ASSERT_TRUE(later_executed);
}

TEST(SingleThreadedTaskQueueForTestingTest, ExternalThreadCancelsTask) {
  SingleThreadedTaskQueueForTesting task_queue("task_queue");

  rtc::Event done(true, false);

  // Prevent the to-be-cancelled task from being executed before we've had
  // time to cancel it.
  rtc::Event rendezvous(true, false);
  task_queue.PostTask([&rendezvous]() {
    ASSERT_TRUE(rendezvous.Wait(kMaxWaitTimeMs));
  });

  TaskId cancelled_task_id = task_queue.PostTask([]() {
    EXPECT_TRUE(false);
  });
  task_queue.PostTask([&done]() {
    done.Set();
  });

  task_queue.CancelTask(cancelled_task_id);

  // Set the tasks in motion; the cancelled task does not run (otherwise the
  // test would fail). The last task ends the test, showing that the queue
  // progressed beyond the cancelled task.
  rendezvous.Set();
  ASSERT_TRUE(done.Wait(kMaxWaitTimeMs));
}

// In this test, we'll set off a chain where the first task cancels the second
// task, then a third task runs (showing that we really cancelled the task,
// rather than just halted the task-queue).
TEST(SingleThreadedTaskQueueForTestingTest, InternalThreadCancelsTask) {
  SingleThreadedTaskQueueForTesting task_queue("task_queue");

  rtc::Event done(true, false);

  // Prevent the chain from being set-off before we've set everything up.
  rtc::Event rendezvous(true, false);
  task_queue.PostTask([&rendezvous]() {
    ASSERT_TRUE(rendezvous.Wait(kMaxWaitTimeMs));
  });

  // This is the canceller-task. It takes cancelled_task_id by reference,
  // because the ID will only become known after the cancelled task is
  // scheduled.
  TaskId cancelled_task_id;
  auto canceller_task = [&task_queue, &cancelled_task_id]() {
    task_queue.CancelTask(cancelled_task_id);
  };
  task_queue.PostTask(canceller_task);

  // This task will be cancelled by the task before it.
  auto cancelled_task = []() {
    EXPECT_TRUE(false);
  };
  cancelled_task_id = task_queue.PostTask(cancelled_task);

  // When this task runs, it will allow the test to be finished.
  auto completion_marker_task = [&done]() {
    done.Set();
  };
  task_queue.PostTask(completion_marker_task);

  rendezvous.Set();  // Set the chain in motion.

  ASSERT_TRUE(done.Wait(kMaxWaitTimeMs));
}

TEST(SingleThreadedTaskQueueForTestingTest, SendTask) {
  SingleThreadedTaskQueueForTesting task_queue("task_queue");

  std::atomic<bool> executed(false);

  task_queue.SendTask([&executed]() {
    // Intentionally delay, so that if SendTask didn't block, the sender thread
    // would have time to read |executed|.
    rtc::Event delay(true, false);
    ASSERT_FALSE(delay.Wait(1000));
    executed.store(true);
  });

  EXPECT_TRUE(executed);
}

TEST(SingleThreadedTaskQueueForTestingTest,
     DestructTaskQueueWhileTasksPending) {
  auto task_queue =
      rtc::MakeUnique<SingleThreadedTaskQueueForTesting>("task_queue");

  std::atomic<size_t> counter(0);

  constexpr size_t tasks = 10;
  for (size_t i = 0; i < tasks; i++) {
    task_queue->PostTask([&counter]() {
      std::atomic_fetch_add(&counter, static_cast<size_t>(1));
      rtc::Event delay(true, false);
      ASSERT_FALSE(delay.Wait(500));
    });
  }

  task_queue.reset();

  EXPECT_LT(counter, tasks);
}

}  // namespace
}  // namespace test
}  // namespace webrtc
