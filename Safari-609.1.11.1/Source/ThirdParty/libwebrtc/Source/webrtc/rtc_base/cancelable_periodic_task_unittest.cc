/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/cancelable_periodic_task.h"

#include "rtc_base/event.h"
#include "test/gmock.h"

namespace {

using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::MockFunction;
using ::testing::Return;

constexpr int kTimeoutMs = 1000;

class MockClosure {
 public:
  MOCK_METHOD0(Call, int());
  MOCK_METHOD0(Delete, void());
};

class MoveOnlyClosure {
 public:
  explicit MoveOnlyClosure(MockClosure* mock) : mock_(mock) {}
  MoveOnlyClosure(const MoveOnlyClosure&) = delete;
  MoveOnlyClosure(MoveOnlyClosure&& other) : mock_(other.mock_) {
    other.mock_ = nullptr;
  }
  ~MoveOnlyClosure() {
    if (mock_)
      mock_->Delete();
  }
  int operator()() { return mock_->Call(); }

 private:
  MockClosure* mock_;
};

class CopyableClosure {
 public:
  explicit CopyableClosure(MockClosure* mock) : mock_(mock) {}
  CopyableClosure(const CopyableClosure& other) : mock_(other.mock_) {}
  ~CopyableClosure() {
    if (mock_) {
      mock_->Delete();
      mock_ = nullptr;
    }
  }
  int operator()() { return mock_->Call(); }

 private:
  MockClosure* mock_;
};

TEST(CancelablePeriodicTaskTest, CanCallCancelOnEmptyHandle) {
  rtc::CancelableTaskHandle handle;
  handle.Cancel();
}

TEST(CancelablePeriodicTaskTest, CancelTaskBeforeItRuns) {
  rtc::Event done;
  MockClosure mock;
  EXPECT_CALL(mock, Call).Times(0);
  EXPECT_CALL(mock, Delete).WillOnce(Invoke([&done] { done.Set(); }));

  rtc::TaskQueue task_queue("queue");

  auto task = rtc::CreateCancelablePeriodicTask(MoveOnlyClosure(&mock));
  rtc::CancelableTaskHandle handle = task->GetCancellationHandle();
  task_queue.PostTask([handle] { handle.Cancel(); });
  task_queue.PostTask(std::move(task));

  EXPECT_TRUE(done.Wait(kTimeoutMs));
}

TEST(CancelablePeriodicTaskTest, CancelDelayedTaskBeforeItRuns) {
  rtc::Event done;
  MockClosure mock;
  EXPECT_CALL(mock, Call).Times(0);
  EXPECT_CALL(mock, Delete).WillOnce(Invoke([&done] { done.Set(); }));

  rtc::TaskQueue task_queue("queue");

  auto task = rtc::CreateCancelablePeriodicTask(MoveOnlyClosure(&mock));
  rtc::CancelableTaskHandle handle = task->GetCancellationHandle();
  task_queue.PostDelayedTask(std::move(task), 100);
  task_queue.PostTask([handle] { handle.Cancel(); });

  EXPECT_TRUE(done.Wait(kTimeoutMs));
}

TEST(CancelablePeriodicTaskTest, CancelTaskAfterItRuns) {
  rtc::Event done;
  MockClosure mock;
  EXPECT_CALL(mock, Call).WillOnce(Return(100));
  EXPECT_CALL(mock, Delete).WillOnce(Invoke([&done] { done.Set(); }));

  rtc::TaskQueue task_queue("queue");

  auto task = rtc::CreateCancelablePeriodicTask(MoveOnlyClosure(&mock));
  rtc::CancelableTaskHandle handle = task->GetCancellationHandle();
  task_queue.PostTask(std::move(task));
  task_queue.PostTask([handle] { handle.Cancel(); });

  EXPECT_TRUE(done.Wait(kTimeoutMs));
}

TEST(CancelablePeriodicTaskTest, ZeroReturnValueRepostsTheTask) {
  NiceMock<MockClosure> closure;
  rtc::Event done;
  EXPECT_CALL(closure, Call()).WillOnce(Return(0)).WillOnce(Invoke([&done] {
    done.Set();
    return kTimeoutMs;
  }));
  rtc::TaskQueue task_queue("queue");
  task_queue.PostTask(
      rtc::CreateCancelablePeriodicTask(MoveOnlyClosure(&closure)));
  EXPECT_TRUE(done.Wait(kTimeoutMs));
}

TEST(CancelablePeriodicTaskTest, StartPeriodicTask) {
  MockFunction<int()> closure;
  rtc::Event done;
  EXPECT_CALL(closure, Call())
      .WillOnce(Return(20))
      .WillOnce(Return(20))
      .WillOnce(Invoke([&done] {
        done.Set();
        return kTimeoutMs;
      }));
  rtc::TaskQueue task_queue("queue");
  task_queue.PostTask(
      rtc::CreateCancelablePeriodicTask(closure.AsStdFunction()));
  EXPECT_TRUE(done.Wait(kTimeoutMs));
}

// Validates perfect forwarding doesn't keep reference to deleted copy.
TEST(CancelablePeriodicTaskTest, CreateWithCopyOfAClosure) {
  rtc::Event done;
  MockClosure mock;
  EXPECT_CALL(mock, Call).WillOnce(Invoke([&done] {
    done.Set();
    return kTimeoutMs;
  }));
  EXPECT_CALL(mock, Delete).Times(AtLeast(2));
  CopyableClosure closure(&mock);
  std::unique_ptr<rtc::BaseCancelableTask> task;
  {
    CopyableClosure copy = closure;
    task = rtc::CreateCancelablePeriodicTask(copy);
  }

  rtc::TaskQueue task_queue("queue");
  task_queue.PostTask(std::move(task));
  EXPECT_TRUE(done.Wait(kTimeoutMs));
}

TEST(CancelablePeriodicTaskTest, DeletingHandleDoesntStopTheTask) {
  rtc::Event run;
  rtc::TaskQueue task_queue("queue");
  auto task = rtc::CreateCancelablePeriodicTask(([&] {
    run.Set();
    return kTimeoutMs;
  }));
  rtc::CancelableTaskHandle handle = task->GetCancellationHandle();
  handle = {};  // delete the handle.
  task_queue.PostTask(std::move(task));
  EXPECT_TRUE(run.Wait(kTimeoutMs));
}

// Example to test there are no thread races and use after free for suggested
// typical usage of the CancelablePeriodicTask
TEST(CancelablePeriodicTaskTest, Example) {
  class ObjectOnTaskQueue {
   public:
    void DoPeriodicTask() {}
    int TimeUntilNextRunMs() { return 100; }

    rtc::CancelableTaskHandle StartPeriodicTask(rtc::TaskQueue* task_queue) {
      auto periodic_task = rtc::CreateCancelablePeriodicTask([this] {
        DoPeriodicTask();
        return TimeUntilNextRunMs();
      });
      rtc::CancelableTaskHandle handle = periodic_task->GetCancellationHandle();
      task_queue->PostTask(std::move(periodic_task));
      return handle;
    }
  };

  rtc::TaskQueue task_queue("queue");

  auto object = absl::make_unique<ObjectOnTaskQueue>();
  // Create and start the periodic task.
  rtc::CancelableTaskHandle handle = object->StartPeriodicTask(&task_queue);

  // Restart the task
  task_queue.PostTask([handle] { handle.Cancel(); });
  handle = object->StartPeriodicTask(&task_queue);

  // Stop the task and destroy the object.
  struct Destructor {
    void operator()() {
      // Cancel must be run on the task_queue, but if task failed to start
      // because of task queue destruction, there is no need to run Cancel.
      handle.Cancel();
    }
    // Destruction will happen either on the task queue or because task
    // queue is destroyed.

    std::unique_ptr<ObjectOnTaskQueue> object;
    rtc::CancelableTaskHandle handle;
  };
  task_queue.PostTask(Destructor{std::move(object), std::move(handle)});
  // Do not wait for the Destructor closure in order to create a race between
  // task queue destruction and running the Desctructor closure.
}

}  // namespace
