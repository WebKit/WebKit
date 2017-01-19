/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/checks.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/platform_thread.h"
#include "webrtc/base/sequenced_task_checker.h"
#include "webrtc/base/task_queue.h"
#include "webrtc/test/gtest.h"

namespace rtc {

namespace {
// Calls SequencedTaskChecker::CalledSequentially on another thread.
class CallCalledSequentiallyOnThread {
 public:
  CallCalledSequentiallyOnThread(bool expect_true,
                                 SequencedTaskChecker* sequenced_task_checker)
      : expect_true_(expect_true),
        thread_has_run_event_(false, false),
        thread_(&Run, this, "call_do_stuff_on_thread"),
        sequenced_task_checker_(sequenced_task_checker) {
    thread_.Start();
  }
  ~CallCalledSequentiallyOnThread() {
    EXPECT_TRUE(thread_has_run_event_.Wait(1000));
    thread_.Stop();
  }

 private:
  static bool Run(void* obj) {
    CallCalledSequentiallyOnThread* call_stuff_on_thread =
        static_cast<CallCalledSequentiallyOnThread*>(obj);
    EXPECT_EQ(
        call_stuff_on_thread->expect_true_,
        call_stuff_on_thread->sequenced_task_checker_->CalledSequentially());
    call_stuff_on_thread->thread_has_run_event_.Set();
    return false;
  }

  const bool expect_true_;
  Event thread_has_run_event_;
  PlatformThread thread_;
  SequencedTaskChecker* const sequenced_task_checker_;

  RTC_DISALLOW_COPY_AND_ASSIGN(CallCalledSequentiallyOnThread);
};

// Deletes SequencedTaskChecker on a different thread.
class DeleteSequencedCheckerOnThread {
 public:
  explicit DeleteSequencedCheckerOnThread(
      std::unique_ptr<SequencedTaskChecker> sequenced_task_checker)
      : thread_(&Run, this, "delete_sequenced_task_checker_on_thread"),
        thread_has_run_event_(false, false),
        sequenced_task_checker_(std::move(sequenced_task_checker)) {
    thread_.Start();
  }

  ~DeleteSequencedCheckerOnThread() {
    EXPECT_TRUE(thread_has_run_event_.Wait(1000));
    thread_.Stop();
  }

 private:
  static bool Run(void* obj) {
    DeleteSequencedCheckerOnThread* instance =
        static_cast<DeleteSequencedCheckerOnThread*>(obj);
    instance->sequenced_task_checker_.reset();
    instance->thread_has_run_event_.Set();
    return false;
  }

 private:
  PlatformThread thread_;
  Event thread_has_run_event_;
  std::unique_ptr<SequencedTaskChecker> sequenced_task_checker_;

  RTC_DISALLOW_COPY_AND_ASSIGN(DeleteSequencedCheckerOnThread);
};

void RunMethodOnDifferentThread(bool expect_true) {
  std::unique_ptr<SequencedTaskChecker> sequenced_task_checker(
      new SequencedTaskChecker());

  CallCalledSequentiallyOnThread call_on_thread(expect_true,
                                                sequenced_task_checker.get());
}

void RunMethodOnDifferentTaskQueue(bool expect_true) {
  std::unique_ptr<SequencedTaskChecker> sequenced_task_checker(
      new SequencedTaskChecker());

  static const char kQueueName[] = "MethodNotAllowedOnDifferentTq";
  TaskQueue queue(kQueueName);
  Event done_event(false, false);
  queue.PostTask([&sequenced_task_checker, &done_event, expect_true] {
    if (expect_true)
      EXPECT_TRUE(sequenced_task_checker->CalledSequentially());
    else
      EXPECT_FALSE(sequenced_task_checker->CalledSequentially());
    done_event.Set();
  });
  EXPECT_TRUE(done_event.Wait(1000));
}

void DetachThenCallFromDifferentTaskQueue(bool expect_true) {
  std::unique_ptr<SequencedTaskChecker> sequenced_task_checker(
      new SequencedTaskChecker());

  sequenced_task_checker->Detach();

  Event done_event(false, false);
  TaskQueue queue1("DetachThenCallFromDifferentTaskQueueImpl1");
  queue1.PostTask([&sequenced_task_checker, &done_event] {
    EXPECT_TRUE(sequenced_task_checker->CalledSequentially());
    done_event.Set();
  });
  EXPECT_TRUE(done_event.Wait(1000));

  // CalledSequentially should return false in debug builds after moving to
  // another task queue.
  TaskQueue queue2("DetachThenCallFromDifferentTaskQueueImpl2");
  queue2.PostTask([&sequenced_task_checker, &done_event, expect_true] {
    if (expect_true)
      EXPECT_TRUE(sequenced_task_checker->CalledSequentially());
    else
      EXPECT_FALSE(sequenced_task_checker->CalledSequentially());
    done_event.Set();
  });
  EXPECT_TRUE(done_event.Wait(1000));
}
}  // namespace

TEST(SequencedTaskCheckerTest, CallsAllowedOnSameThread) {
  std::unique_ptr<SequencedTaskChecker> sequenced_task_checker(
      new SequencedTaskChecker());

  EXPECT_TRUE(sequenced_task_checker->CalledSequentially());

  // Verify that the destructor doesn't assert.
  sequenced_task_checker.reset();
}

TEST(SequencedTaskCheckerTest, DestructorAllowedOnDifferentThread) {
  std::unique_ptr<SequencedTaskChecker> sequenced_task_checker(
      new SequencedTaskChecker());

  // Verify that the destructor doesn't assert when called on a different
  // thread.
  DeleteSequencedCheckerOnThread delete_on_thread(
      std::move(sequenced_task_checker));
}

TEST(SequencedTaskCheckerTest, DetachFromThread) {
  std::unique_ptr<SequencedTaskChecker> sequenced_task_checker(
      new SequencedTaskChecker());

  sequenced_task_checker->Detach();
  CallCalledSequentiallyOnThread call_on_thread(true,
                                                sequenced_task_checker.get());
}

TEST(SequencedTaskCheckerTest, DetachFromThreadAndUseOnTaskQueue) {
  std::unique_ptr<SequencedTaskChecker> sequenced_task_checker(
      new SequencedTaskChecker());

  sequenced_task_checker->Detach();
  static const char kQueueName[] = "DetachFromThreadAndUseOnTaskQueue";
  TaskQueue queue(kQueueName);
  Event done_event(false, false);
  queue.PostTask([&sequenced_task_checker, &done_event] {
    EXPECT_TRUE(sequenced_task_checker->CalledSequentially());
    done_event.Set();
  });
  EXPECT_TRUE(done_event.Wait(1000));
}

TEST(SequencedTaskCheckerTest, DetachFromTaskQueueAndUseOnThread) {
  TaskQueue queue("DetachFromTaskQueueAndUseOnThread");
  Event done_event(false, false);
  queue.PostTask([&done_event] {
    std::unique_ptr<SequencedTaskChecker> sequenced_task_checker(
        new SequencedTaskChecker());

    sequenced_task_checker->Detach();
    CallCalledSequentiallyOnThread call_on_thread(true,
                                                  sequenced_task_checker.get());
    done_event.Set();
  });
  EXPECT_TRUE(done_event.Wait(1000));
}

#if RTC_DCHECK_IS_ON
TEST(SequencedTaskCheckerTest, MethodNotAllowedOnDifferentThreadInDebug) {
  RunMethodOnDifferentThread(false);
}
#else
TEST(SequencedTaskCheckerTest, MethodAllowedOnDifferentThreadInRelease) {
  RunMethodOnDifferentThread(true);
}
#endif

#if RTC_DCHECK_IS_ON
TEST(SequencedTaskCheckerTest, MethodNotAllowedOnDifferentTaskQueueInDebug) {
  RunMethodOnDifferentTaskQueue(false);
}
#else
TEST(SequencedTaskCheckerTest, MethodAllowedOnDifferentTaskQueueInRelease) {
  RunMethodOnDifferentTaskQueue(true);
}
#endif

#if RTC_DCHECK_IS_ON
TEST(SequencedTaskCheckerTest, DetachFromTaskQueueInDebug) {
  DetachThenCallFromDifferentTaskQueue(false);
}
#else
TEST(SequencedTaskCheckerTest, DetachFromTaskQueueInRelease) {
  DetachThenCallFromDifferentTaskQueue(true);
}
#endif

class TestAnnotations {
 public:
  TestAnnotations() : test_var_(false) {}

  void ModifyTestVar() {
    RTC_DCHECK_CALLED_SEQUENTIALLY(&checker_);
    test_var_ = true;
  }

 private:
  bool test_var_ GUARDED_BY(&checker_);
  SequencedTaskChecker checker_;
};

TEST(SequencedTaskCheckerTest, TestAnnotations) {
  TestAnnotations annotations;
  annotations.ModifyTestVar();
}

#if GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

void TestAnnotationsOnWrongQueue() {
  TestAnnotations annotations;
  static const char kQueueName[] = "TestAnnotationsOnWrongQueueDebug";
  TaskQueue queue(kQueueName);
  Event done_event(false, false);
  queue.PostTask([&annotations, &done_event] {
    annotations.ModifyTestVar();
    done_event.Set();
  });
  EXPECT_TRUE(done_event.Wait(1000));
}

#if RTC_DCHECK_IS_ON
TEST(SequencedTaskCheckerTest, TestAnnotationsOnWrongQueueDebug) {
  ASSERT_DEATH({ TestAnnotationsOnWrongQueue(); }, "");
}
#else
TEST(SequencedTaskCheckerTest, TestAnnotationsOnWrongQueueRelease) {
  TestAnnotationsOnWrongQueue();
}
#endif
#endif  // GTEST_HAS_DEATH_TEST
}  // namespace rtc
