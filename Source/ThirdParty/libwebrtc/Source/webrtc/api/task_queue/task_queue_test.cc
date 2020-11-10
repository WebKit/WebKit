/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/task_queue/task_queue_test.h"

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "rtc_base/event.h"
#include "rtc_base/ref_counter.h"
#include "rtc_base/task_utils/to_queued_task.h"
#include "rtc_base/time_utils.h"

namespace webrtc {
namespace {

std::unique_ptr<TaskQueueBase, TaskQueueDeleter> CreateTaskQueue(
    const std::unique_ptr<webrtc::TaskQueueFactory>& factory,
    absl::string_view task_queue_name,
    TaskQueueFactory::Priority priority = TaskQueueFactory::Priority::NORMAL) {
  return factory->CreateTaskQueue(task_queue_name, priority);
}

TEST_P(TaskQueueTest, Construct) {
  std::unique_ptr<webrtc::TaskQueueFactory> factory = GetParam()();
  auto queue = CreateTaskQueue(factory, "Construct");
  EXPECT_FALSE(queue->IsCurrent());
}

TEST_P(TaskQueueTest, PostAndCheckCurrent) {
  std::unique_ptr<webrtc::TaskQueueFactory> factory = GetParam()();
  rtc::Event event;
  auto queue = CreateTaskQueue(factory, "PostAndCheckCurrent");

  // We're not running a task, so |queue| shouldn't be current.
  // Note that because rtc::Thread also supports the TQ interface and
  // TestMainImpl::Init wraps the main test thread (bugs.webrtc.org/9714), that
  // means that TaskQueueBase::Current() will still return a valid value.
  EXPECT_FALSE(queue->IsCurrent());

  queue->PostTask(ToQueuedTask([&event, &queue] {
    EXPECT_TRUE(queue->IsCurrent());
    event.Set();
  }));
  EXPECT_TRUE(event.Wait(1000));
}

TEST_P(TaskQueueTest, PostCustomTask) {
  std::unique_ptr<webrtc::TaskQueueFactory> factory = GetParam()();
  rtc::Event ran;
  auto queue = CreateTaskQueue(factory, "PostCustomImplementation");

  class CustomTask : public QueuedTask {
   public:
    explicit CustomTask(rtc::Event* ran) : ran_(ran) {}

   private:
    bool Run() override {
      ran_->Set();
      return false;  // Do not allow the task to be deleted by the queue.
    }

    rtc::Event* const ran_;
  } my_task(&ran);

  queue->PostTask(absl::WrapUnique(&my_task));
  EXPECT_TRUE(ran.Wait(1000));
}

TEST_P(TaskQueueTest, PostDelayedZero) {
  std::unique_ptr<webrtc::TaskQueueFactory> factory = GetParam()();
  rtc::Event event;
  auto queue = CreateTaskQueue(factory, "PostDelayedZero");

  queue->PostDelayedTask(ToQueuedTask([&event] { event.Set(); }), 0);
  EXPECT_TRUE(event.Wait(1000));
}

TEST_P(TaskQueueTest, PostFromQueue) {
  std::unique_ptr<webrtc::TaskQueueFactory> factory = GetParam()();
  rtc::Event event;
  auto queue = CreateTaskQueue(factory, "PostFromQueue");

  queue->PostTask(ToQueuedTask([&event, &queue] {
    queue->PostTask(ToQueuedTask([&event] { event.Set(); }));
  }));
  EXPECT_TRUE(event.Wait(1000));
}

TEST_P(TaskQueueTest, PostDelayed) {
  std::unique_ptr<webrtc::TaskQueueFactory> factory = GetParam()();
  rtc::Event event;
  auto queue =
      CreateTaskQueue(factory, "PostDelayed", TaskQueueFactory::Priority::HIGH);

  int64_t start = rtc::TimeMillis();
  queue->PostDelayedTask(ToQueuedTask([&event, &queue] {
                           EXPECT_TRUE(queue->IsCurrent());
                           event.Set();
                         }),
                         100);
  EXPECT_TRUE(event.Wait(1000));
  int64_t end = rtc::TimeMillis();
  // These tests are a little relaxed due to how "powerful" our test bots can
  // be.  Most recently we've seen windows bots fire the callback after 94-99ms,
  // which is why we have a little bit of leeway backwards as well.
  EXPECT_GE(end - start, 90u);
  EXPECT_NEAR(end - start, 190u, 100u);  // Accept 90-290.
}

TEST_P(TaskQueueTest, PostMultipleDelayed) {
  std::unique_ptr<webrtc::TaskQueueFactory> factory = GetParam()();
  auto queue = CreateTaskQueue(factory, "PostMultipleDelayed");

  std::vector<rtc::Event> events(100);
  for (int i = 0; i < 100; ++i) {
    rtc::Event* event = &events[i];
    queue->PostDelayedTask(ToQueuedTask([event, &queue] {
                             EXPECT_TRUE(queue->IsCurrent());
                             event->Set();
                           }),
                           i);
  }

  for (rtc::Event& e : events)
    EXPECT_TRUE(e.Wait(1000));
}

TEST_P(TaskQueueTest, PostDelayedAfterDestruct) {
  std::unique_ptr<webrtc::TaskQueueFactory> factory = GetParam()();
  rtc::Event run;
  rtc::Event deleted;
  auto queue = CreateTaskQueue(factory, "PostDelayedAfterDestruct");
  queue->PostDelayedTask(
      ToQueuedTask([&run] { run.Set(); }, [&deleted] { deleted.Set(); }), 100);
  // Destroy the queue.
  queue = nullptr;
  // Task might outlive the TaskQueue, but still should be deleted.
  EXPECT_TRUE(deleted.Wait(1000));
  EXPECT_FALSE(run.Wait(0));  // and should not run.
}

TEST_P(TaskQueueTest, PostAndReuse) {
  std::unique_ptr<webrtc::TaskQueueFactory> factory = GetParam()();
  rtc::Event event;
  auto post_queue = CreateTaskQueue(factory, "PostQueue");
  auto reply_queue = CreateTaskQueue(factory, "ReplyQueue");

  int call_count = 0;

  class ReusedTask : public QueuedTask {
   public:
    ReusedTask(int* counter, TaskQueueBase* reply_queue, rtc::Event* event)
        : counter_(*counter), reply_queue_(reply_queue), event_(*event) {
      EXPECT_EQ(counter_, 0);
    }

   private:
    bool Run() override {
      if (++counter_ == 1) {
        reply_queue_->PostTask(absl::WrapUnique(this));
        // At this point, the object is owned by reply_queue_ and it's
        // theoratically possible that the object has been deleted (e.g. if
        // posting wasn't possible).  So, don't touch any member variables here.

        // Indicate to the current queue that ownership has been transferred.
        return false;
      } else {
        EXPECT_EQ(counter_, 2);
        EXPECT_TRUE(reply_queue_->IsCurrent());
        event_.Set();
        return true;  // Indicate that the object should be deleted.
      }
    }

    int& counter_;
    TaskQueueBase* const reply_queue_;
    rtc::Event& event_;
  };

  auto task =
      std::make_unique<ReusedTask>(&call_count, reply_queue.get(), &event);
  post_queue->PostTask(std::move(task));
  EXPECT_TRUE(event.Wait(1000));
}

TEST_P(TaskQueueTest, PostALot) {
  // Waits until DecrementCount called |count| times. Thread safe.
  class BlockingCounter {
   public:
    explicit BlockingCounter(int initial_count) : count_(initial_count) {}

    void DecrementCount() {
      if (count_.DecRef() == rtc::RefCountReleaseStatus::kDroppedLastRef) {
        event_.Set();
      }
    }
    bool Wait(int give_up_after_ms) { return event_.Wait(give_up_after_ms); }

   private:
    webrtc_impl::RefCounter count_;
    rtc::Event event_;
  };

  std::unique_ptr<webrtc::TaskQueueFactory> factory = GetParam()();
  static constexpr int kTaskCount = 0xffff;
  rtc::Event posting_done;
  BlockingCounter all_destroyed(kTaskCount);

  int tasks_executed = 0;
  auto task_queue = CreateTaskQueue(factory, "PostALot");

  task_queue->PostTask(ToQueuedTask([&] {
    // Post tasks from the queue to guarantee that the 1st task won't be
    // executed before the last one is posted.
    for (int i = 0; i < kTaskCount; ++i) {
      task_queue->PostTask(ToQueuedTask(
          [&] { ++tasks_executed; }, [&] { all_destroyed.DecrementCount(); }));
    }

    posting_done.Set();
  }));

  // Before destroying the task queue wait until all child tasks are posted.
  posting_done.Wait(rtc::Event::kForever);
  // Destroy the task queue.
  task_queue = nullptr;

  // Expect all tasks are destroyed eventually. In some task queue
  // implementations that might happen on a different thread after task queue is
  // destroyed.
  EXPECT_TRUE(all_destroyed.Wait(60000));
  EXPECT_LE(tasks_executed, kTaskCount);
}

// Test posting two tasks that have shared state not protected by a
// lock. The TaskQueue should guarantee memory read-write order and
// FIFO task execution order, so the second task should always see the
// changes that were made by the first task.
//
// If the TaskQueue doesn't properly synchronize the execution of
// tasks, there will be a data race, which is undefined behavior. The
// EXPECT calls may randomly catch this, but to make the most of this
// unit test, run it under TSan or some other tool that is able to
// directly detect data races.
TEST_P(TaskQueueTest, PostTwoWithSharedUnprotectedState) {
  std::unique_ptr<webrtc::TaskQueueFactory> factory = GetParam()();
  struct SharedState {
    // First task will set this value to 1 and second will assert it.
    int state = 0;
  } state;

  auto queue = CreateTaskQueue(factory, "PostTwoWithSharedUnprotectedState");
  rtc::Event done;
  queue->PostTask(ToQueuedTask([&state, &queue, &done] {
    // Post tasks from queue to guarantee, that 1st task won't be
    // executed before the second one will be posted.
    queue->PostTask(ToQueuedTask([&state] { state.state = 1; }));
    queue->PostTask(ToQueuedTask([&state, &done] {
      EXPECT_EQ(state.state, 1);
      done.Set();
    }));
    // Check, that state changing tasks didn't start yet.
    EXPECT_EQ(state.state, 0);
  }));
  EXPECT_TRUE(done.Wait(1000));
}

// TaskQueueTest is a set of tests for any implementation of the TaskQueueBase.
// Tests are instantiated next to the concrete implementation(s).
// https://github.com/google/googletest/blob/master/googletest/docs/advanced.md#creating-value-parameterized-abstract-tests
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(TaskQueueTest);

}  // namespace
}  // namespace webrtc
