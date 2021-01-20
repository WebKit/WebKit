/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/task_utils/to_queued_task.h"

#include <memory>

#include "absl/memory/memory.h"
#include "api/task_queue/queued_task.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::InSequence;
using ::testing::MockFunction;

void RunTask(std::unique_ptr<QueuedTask> task) {
  // Simulate how task queue suppose to run tasks.
  QueuedTask* raw = task.release();
  if (raw->Run())
    delete raw;
}

TEST(ToQueuedTaskTest, AcceptsLambda) {
  bool run = false;
  std::unique_ptr<QueuedTask> task = ToQueuedTask([&run] { run = true; });
  EXPECT_FALSE(run);
  RunTask(std::move(task));
  EXPECT_TRUE(run);
}

TEST(ToQueuedTaskTest, AcceptsCopyableClosure) {
  struct CopyableClosure {
    CopyableClosure(int* num_copies, int* num_moves, int* num_runs)
        : num_copies(num_copies), num_moves(num_moves), num_runs(num_runs) {}
    CopyableClosure(const CopyableClosure& other)
        : num_copies(other.num_copies),
          num_moves(other.num_moves),
          num_runs(other.num_runs) {
      ++*num_copies;
    }
    CopyableClosure(CopyableClosure&& other)
        : num_copies(other.num_copies),
          num_moves(other.num_moves),
          num_runs(other.num_runs) {
      ++*num_moves;
    }
    void operator()() { ++*num_runs; }

    int* num_copies;
    int* num_moves;
    int* num_runs;
  };

  int num_copies = 0;
  int num_moves = 0;
  int num_runs = 0;

  std::unique_ptr<QueuedTask> task;
  {
    CopyableClosure closure(&num_copies, &num_moves, &num_runs);
    task = ToQueuedTask(closure);
    // Destroy closure to check with msan task has own copy.
  }
  EXPECT_EQ(num_copies, 1);
  EXPECT_EQ(num_runs, 0);
  RunTask(std::move(task));
  EXPECT_EQ(num_copies, 1);
  EXPECT_EQ(num_moves, 0);
  EXPECT_EQ(num_runs, 1);
}

TEST(ToQueuedTaskTest, AcceptsMoveOnlyClosure) {
  struct MoveOnlyClosure {
    MoveOnlyClosure(int* num_moves, std::function<void()> trigger)
        : num_moves(num_moves), trigger(std::move(trigger)) {}
    MoveOnlyClosure(const MoveOnlyClosure&) = delete;
    MoveOnlyClosure(MoveOnlyClosure&& other)
        : num_moves(other.num_moves), trigger(std::move(other.trigger)) {
      ++*num_moves;
    }
    void operator()() { trigger(); }

    int* num_moves;
    std::function<void()> trigger;
  };

  int num_moves = 0;
  MockFunction<void()> run;

  auto task = ToQueuedTask(MoveOnlyClosure(&num_moves, run.AsStdFunction()));
  EXPECT_EQ(num_moves, 1);
  EXPECT_CALL(run, Call);
  RunTask(std::move(task));
  EXPECT_EQ(num_moves, 1);
}

TEST(ToQueuedTaskTest, AcceptsMoveOnlyCleanup) {
  struct MoveOnlyClosure {
    MoveOnlyClosure(const MoveOnlyClosure&) = delete;
    MoveOnlyClosure(MoveOnlyClosure&&) = default;
    void operator()() { trigger(); }

    std::function<void()> trigger;
  };

  MockFunction<void()> run;
  MockFunction<void()> cleanup;

  auto task = ToQueuedTask(MoveOnlyClosure{run.AsStdFunction()},
                           MoveOnlyClosure{cleanup.AsStdFunction()});

  // Expect run closure to complete before cleanup closure.
  InSequence in_sequence;
  EXPECT_CALL(run, Call);
  EXPECT_CALL(cleanup, Call);
  RunTask(std::move(task));
}

TEST(ToQueuedTaskTest, PendingTaskSafetyFlag) {
  rtc::scoped_refptr<PendingTaskSafetyFlag> flag =
      PendingTaskSafetyFlag::Create();

  int count = 0;
  // Create two identical tasks that increment the |count|.
  auto task1 = ToQueuedTask(flag, [&count]() { ++count; });
  auto task2 = ToQueuedTask(flag, [&count]() { ++count; });

  EXPECT_EQ(0, count);
  RunTask(std::move(task1));
  EXPECT_EQ(1, count);
  flag->SetNotAlive();
  // Now task2 should actually not run.
  RunTask(std::move(task2));
  EXPECT_EQ(1, count);
}

}  // namespace
}  // namespace webrtc
