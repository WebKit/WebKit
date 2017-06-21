/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if defined(WEBRTC_WIN)
// clang-format off
#include <windows.h>  // Must come first.
#include <mmsystem.h>
// clang-format on
#endif

#include <memory>
#include <vector>

#include "webrtc/base/bind.h"
#include "webrtc/base/event.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/task_queue.h"
#include "webrtc/base/timeutils.h"

namespace rtc {
namespace {
// Noop on all platforms except Windows, where it turns on high precision
// multimedia timers which increases the precision of TimeMillis() while in
// scope.
class EnableHighResTimers {
 public:
#if !defined(WEBRTC_WIN)
  EnableHighResTimers() {}
#else
  EnableHighResTimers() : enabled_(timeBeginPeriod(1) == TIMERR_NOERROR) {}
  ~EnableHighResTimers() {
    if (enabled_)
      timeEndPeriod(1);
  }

 private:
  const bool enabled_;
#endif
};
}

namespace {
void CheckCurrent(const char* expected_queue, Event* signal, TaskQueue* queue) {
  EXPECT_TRUE(TaskQueue::IsCurrent(expected_queue));
  EXPECT_TRUE(queue->IsCurrent());
  if (signal)
    signal->Set();
}

}  // namespace

TEST(TaskQueueTest, Construct) {
  static const char kQueueName[] = "Construct";
  TaskQueue queue(kQueueName);
  EXPECT_FALSE(queue.IsCurrent());
}

TEST(TaskQueueTest, PostAndCheckCurrent) {
  static const char kQueueName[] = "PostAndCheckCurrent";
  Event event(false, false);
  TaskQueue queue(kQueueName);

  // We're not running a task, so there shouldn't be a current queue.
  EXPECT_FALSE(queue.IsCurrent());
  EXPECT_FALSE(TaskQueue::Current());

  queue.PostTask(Bind(&CheckCurrent, kQueueName, &event, &queue));
  EXPECT_TRUE(event.Wait(1000));
}

TEST(TaskQueueTest, PostCustomTask) {
  static const char kQueueName[] = "PostCustomImplementation";
  Event event(false, false);
  TaskQueue queue(kQueueName);

  class CustomTask : public QueuedTask {
   public:
    explicit CustomTask(Event* event) : event_(event) {}

   private:
    bool Run() override {
      event_->Set();
      return false;  // Never allows the task to be deleted by the queue.
    }

    Event* const event_;
  } my_task(&event);

  // Please don't do this in production code! :)
  queue.PostTask(std::unique_ptr<QueuedTask>(&my_task));
  EXPECT_TRUE(event.Wait(1000));
}

TEST(TaskQueueTest, PostLambda) {
  static const char kQueueName[] = "PostLambda";
  Event event(false, false);
  TaskQueue queue(kQueueName);

  queue.PostTask([&event]() { event.Set(); });
  EXPECT_TRUE(event.Wait(1000));
}

TEST(TaskQueueTest, PostDelayedZero) {
  static const char kQueueName[] = "PostDelayedZero";
  Event event(false, false);
  TaskQueue queue(kQueueName);

  queue.PostDelayedTask([&event]() { event.Set(); }, 0);
  EXPECT_TRUE(event.Wait(1000));
}

TEST(TaskQueueTest, PostFromQueue) {
  static const char kQueueName[] = "PostFromQueue";
  Event event(false, false);
  TaskQueue queue(kQueueName);

  queue.PostTask(
      [&event, &queue]() { queue.PostTask([&event]() { event.Set(); }); });
  EXPECT_TRUE(event.Wait(1000));
}

TEST(TaskQueueTest, PostDelayed) {
  static const char kQueueName[] = "PostDelayed";
  Event event(false, false);
  TaskQueue queue(kQueueName, TaskQueue::Priority::HIGH);

  uint32_t start = Time();
  queue.PostDelayedTask(Bind(&CheckCurrent, kQueueName, &event, &queue), 100);
  EXPECT_TRUE(event.Wait(1000));
  uint32_t end = Time();
  // These tests are a little relaxed due to how "powerful" our test bots can
  // be.  Most recently we've seen windows bots fire the callback after 94-99ms,
  // which is why we have a little bit of leeway backwards as well.
  EXPECT_GE(end - start, 90u);
  EXPECT_NEAR(end - start, 190u, 100u);  // Accept 90-290.
}

// This task needs to be run manually due to the slowness of some of our bots.
// TODO(tommi): Can we run this on the perf bots?
TEST(TaskQueueTest, DISABLED_PostDelayedHighRes) {
  EnableHighResTimers high_res_scope;

  static const char kQueueName[] = "PostDelayedHighRes";
  Event event(false, false);
  TaskQueue queue(kQueueName, TaskQueue::Priority::HIGH);

  uint32_t start = Time();
  queue.PostDelayedTask(Bind(&CheckCurrent, kQueueName, &event, &queue), 3);
  EXPECT_TRUE(event.Wait(1000));
  uint32_t end = TimeMillis();
  // These tests are a little relaxed due to how "powerful" our test bots can
  // be.  Most recently we've seen windows bots fire the callback after 94-99ms,
  // which is why we have a little bit of leeway backwards as well.
  EXPECT_GE(end - start, 3u);
  EXPECT_NEAR(end - start, 3, 3u);
}

TEST(TaskQueueTest, PostMultipleDelayed) {
  static const char kQueueName[] = "PostMultipleDelayed";
  TaskQueue queue(kQueueName);

  std::vector<std::unique_ptr<Event>> events;
  for (int i = 0; i < 100; ++i) {
    events.push_back(std::unique_ptr<Event>(new Event(false, false)));
    queue.PostDelayedTask(
        Bind(&CheckCurrent, kQueueName, events.back().get(), &queue), i);
  }

  for (const auto& e : events)
    EXPECT_TRUE(e->Wait(1000));
}

TEST(TaskQueueTest, PostDelayedAfterDestruct) {
  static const char kQueueName[] = "PostDelayedAfterDestruct";
  Event event(false, false);
  {
    TaskQueue queue(kQueueName);
    queue.PostDelayedTask(Bind(&CheckCurrent, kQueueName, &event, &queue), 100);
  }
  EXPECT_FALSE(event.Wait(200));  // Task should not run.
}

TEST(TaskQueueTest, PostAndReply) {
  static const char kPostQueue[] = "PostQueue";
  static const char kReplyQueue[] = "ReplyQueue";
  Event event(false, false);
  TaskQueue post_queue(kPostQueue);
  TaskQueue reply_queue(kReplyQueue);

  post_queue.PostTaskAndReply(
      Bind(&CheckCurrent, kPostQueue, nullptr, &post_queue),
      Bind(&CheckCurrent, kReplyQueue, &event, &reply_queue), &reply_queue);
  EXPECT_TRUE(event.Wait(1000));
}

TEST(TaskQueueTest, PostAndReuse) {
  static const char kPostQueue[] = "PostQueue";
  static const char kReplyQueue[] = "ReplyQueue";
  Event event(false, false);
  TaskQueue post_queue(kPostQueue);
  TaskQueue reply_queue(kReplyQueue);

  int call_count = 0;

  class ReusedTask : public QueuedTask {
   public:
    ReusedTask(int* counter, TaskQueue* reply_queue, Event* event)
        : counter_(counter), reply_queue_(reply_queue), event_(event) {
      EXPECT_EQ(0, *counter_);
    }

   private:
    bool Run() override {
      if (++(*counter_) == 1) {
        std::unique_ptr<QueuedTask> myself(this);
        reply_queue_->PostTask(std::move(myself));
        // At this point, the object is owned by reply_queue_ and it's
        // theoratically possible that the object has been deleted (e.g. if
        // posting wasn't possible).  So, don't touch any member variables here.

        // Indicate to the current queue that ownership has been transferred.
        return false;
      } else {
        EXPECT_EQ(2, *counter_);
        EXPECT_TRUE(reply_queue_->IsCurrent());
        event_->Set();
        return true;  // Indicate that the object should be deleted.
      }
    }

    int* const counter_;
    TaskQueue* const reply_queue_;
    Event* const event_;
  };

  std::unique_ptr<QueuedTask> task(
      new ReusedTask(&call_count, &reply_queue, &event));

  post_queue.PostTask(std::move(task));
  EXPECT_TRUE(event.Wait(1000));
}

TEST(TaskQueueTest, PostAndReplyLambda) {
  static const char kPostQueue[] = "PostQueue";
  static const char kReplyQueue[] = "ReplyQueue";
  Event event(false, false);
  TaskQueue post_queue(kPostQueue);
  TaskQueue reply_queue(kReplyQueue);

  bool my_flag = false;
  post_queue.PostTaskAndReply([&my_flag]() { my_flag = true; },
                              [&event]() { event.Set(); }, &reply_queue);
  EXPECT_TRUE(event.Wait(1000));
  EXPECT_TRUE(my_flag);
}

// This test covers a particular bug that we had in the libevent implementation
// where we could hit a deadlock while trying to post a reply task to a queue
// that was being deleted.  The test isn't guaranteed to hit that case but it's
// written in a way that makes it likely and by running with --gtest_repeat=1000
// the bug would occur. Alas, now it should be fixed.
TEST(TaskQueueTest, PostAndReplyDeadlock) {
  Event event(false, false);
  TaskQueue post_queue("PostQueue");
  TaskQueue reply_queue("ReplyQueue");

  post_queue.PostTaskAndReply([&event]() { event.Set(); }, []() {},
                              &reply_queue);
  EXPECT_TRUE(event.Wait(1000));
}

void TestPostTaskAndReply(TaskQueue* work_queue,
                          const char* work_queue_name,
                          Event* event) {
  ASSERT_FALSE(work_queue->IsCurrent());
  work_queue->PostTaskAndReply(
      Bind(&CheckCurrent, work_queue_name, nullptr, work_queue),
      NewClosure([event]() { event->Set(); }));
}

// Does a PostTaskAndReply from within a task to post and reply to the current
// queue.  All in all there will be 3 tasks posted and run.
TEST(TaskQueueTest, PostAndReply2) {
  static const char kQueueName[] = "PostAndReply2";
  static const char kWorkQueueName[] = "PostAndReply2_Worker";
  Event event(false, false);
  TaskQueue queue(kQueueName);
  TaskQueue work_queue(kWorkQueueName);

  queue.PostTask(
      Bind(&TestPostTaskAndReply, &work_queue, kWorkQueueName, &event));
  EXPECT_TRUE(event.Wait(1000));
}

// Tests posting more messages than a queue can queue up.
// In situations like that, tasks will get dropped.
TEST(TaskQueueTest, PostALot) {
  // To destruct the event after the queue has gone out of scope.
  Event event(false, false);

  int tasks_executed = 0;
  int tasks_cleaned_up = 0;
  static const int kTaskCount = 0xffff;

  {
    static const char kQueueName[] = "PostALot";
    TaskQueue queue(kQueueName);

    // On linux, the limit of pending bytes in the pipe buffer is 0xffff.
    // So here we post a total of 0xffff+1 messages, which triggers a failure
    // case inside of the libevent queue implementation.

    queue.PostTask([&event]() { event.Wait(Event::kForever); });
    for (int i = 0; i < kTaskCount; ++i)
      queue.PostTask(NewClosure([&tasks_executed]() { ++tasks_executed; },
                                [&tasks_cleaned_up]() { ++tasks_cleaned_up; }));
    event.Set();  // Unblock the first task.
  }

  EXPECT_GE(tasks_cleaned_up, tasks_executed);
  EXPECT_EQ(kTaskCount, tasks_cleaned_up);
}

}  // namespace rtc
