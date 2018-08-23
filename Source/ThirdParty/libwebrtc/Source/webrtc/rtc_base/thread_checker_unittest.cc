/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Borrowed from Chromium's src/base/threading/thread_checker_unittest.cc.

#include <memory>

#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/nullsocketserver.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_checker.h"
#include "test/gtest.h"

// Duplicated from base/threading/thread_checker.h so that we can be
// good citizens there and undef the macro.
#define ENABLE_THREAD_CHECKER RTC_DCHECK_IS_ON

namespace rtc {

namespace {

// Simple class to exercise the basics of ThreadChecker.
// Both the destructor and DoStuff should verify that they were
// called on the same thread as the constructor.
class ThreadCheckerClass : public ThreadChecker {
 public:
  ThreadCheckerClass() {}

  // Verifies that it was called on the same thread as the constructor.
  void DoStuff() { RTC_DCHECK(CalledOnValidThread()); }

  void DetachFromThread() { ThreadChecker::DetachFromThread(); }

  static void MethodOnDifferentThreadImpl();
  static void DetachThenCallFromDifferentThreadImpl();

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(ThreadCheckerClass);
};

// Calls ThreadCheckerClass::DoStuff on another thread.
class CallDoStuffOnThread : public Thread {
 public:
  explicit CallDoStuffOnThread(ThreadCheckerClass* thread_checker_class)
      : Thread(std::unique_ptr<SocketServer>(new rtc::NullSocketServer())),
        thread_checker_class_(thread_checker_class) {
    SetName("call_do_stuff_on_thread", nullptr);
  }

  void Run() override { thread_checker_class_->DoStuff(); }

  // New method. Needed since Thread::Join is protected, and it is called by
  // the TEST.
  void Join() { Thread::Join(); }

 private:
  ThreadCheckerClass* thread_checker_class_;

  RTC_DISALLOW_COPY_AND_ASSIGN(CallDoStuffOnThread);
};

// Deletes ThreadCheckerClass on a different thread.
class DeleteThreadCheckerClassOnThread : public Thread {
 public:
  explicit DeleteThreadCheckerClassOnThread(
      std::unique_ptr<ThreadCheckerClass> thread_checker_class)
      : Thread(std::unique_ptr<SocketServer>(new rtc::NullSocketServer())),
        thread_checker_class_(std::move(thread_checker_class)) {
    SetName("delete_thread_checker_class_on_thread", nullptr);
  }

  void Run() override { thread_checker_class_.reset(); }

  // New method. Needed since Thread::Join is protected, and it is called by
  // the TEST.
  void Join() { Thread::Join(); }

  bool has_been_deleted() const { return !thread_checker_class_; }

 private:
  std::unique_ptr<ThreadCheckerClass> thread_checker_class_;

  RTC_DISALLOW_COPY_AND_ASSIGN(DeleteThreadCheckerClassOnThread);
};

}  // namespace

TEST(ThreadCheckerTest, CallsAllowedOnSameThread) {
  std::unique_ptr<ThreadCheckerClass> thread_checker_class(
      new ThreadCheckerClass);

  // Verify that DoStuff doesn't assert.
  thread_checker_class->DoStuff();

  // Verify that the destructor doesn't assert.
  thread_checker_class.reset();
}

TEST(ThreadCheckerTest, DestructorAllowedOnDifferentThread) {
  std::unique_ptr<ThreadCheckerClass> thread_checker_class(
      new ThreadCheckerClass);

  // Verify that the destructor doesn't assert
  // when called on a different thread.
  DeleteThreadCheckerClassOnThread delete_on_thread(
      std::move(thread_checker_class));

  EXPECT_FALSE(delete_on_thread.has_been_deleted());

  delete_on_thread.Start();
  delete_on_thread.Join();

  EXPECT_TRUE(delete_on_thread.has_been_deleted());
}

TEST(ThreadCheckerTest, DetachFromThread) {
  std::unique_ptr<ThreadCheckerClass> thread_checker_class(
      new ThreadCheckerClass);

  // Verify that DoStuff doesn't assert when called on a different thread after
  // a call to DetachFromThread.
  thread_checker_class->DetachFromThread();
  CallDoStuffOnThread call_on_thread(thread_checker_class.get());

  call_on_thread.Start();
  call_on_thread.Join();
}

#if GTEST_HAS_DEATH_TEST || !ENABLE_THREAD_CHECKER

void ThreadCheckerClass::MethodOnDifferentThreadImpl() {
  std::unique_ptr<ThreadCheckerClass> thread_checker_class(
      new ThreadCheckerClass);

  // DoStuff should assert in debug builds only when called on a
  // different thread.
  CallDoStuffOnThread call_on_thread(thread_checker_class.get());

  call_on_thread.Start();
  call_on_thread.Join();
}

#if ENABLE_THREAD_CHECKER
TEST(ThreadCheckerDeathTest, MethodNotAllowedOnDifferentThreadInDebug) {
  ASSERT_DEATH({ ThreadCheckerClass::MethodOnDifferentThreadImpl(); }, "");
}
#else
TEST(ThreadCheckerTest, MethodAllowedOnDifferentThreadInRelease) {
  ThreadCheckerClass::MethodOnDifferentThreadImpl();
}
#endif  // ENABLE_THREAD_CHECKER

void ThreadCheckerClass::DetachThenCallFromDifferentThreadImpl() {
  std::unique_ptr<ThreadCheckerClass> thread_checker_class(
      new ThreadCheckerClass);

  // DoStuff doesn't assert when called on a different thread
  // after a call to DetachFromThread.
  thread_checker_class->DetachFromThread();
  CallDoStuffOnThread call_on_thread(thread_checker_class.get());

  call_on_thread.Start();
  call_on_thread.Join();

  // DoStuff should assert in debug builds only after moving to
  // another thread.
  thread_checker_class->DoStuff();
}

#if ENABLE_THREAD_CHECKER
TEST(ThreadCheckerDeathTest, DetachFromThreadInDebug) {
  ASSERT_DEATH({ ThreadCheckerClass::DetachThenCallFromDifferentThreadImpl(); },
               "");
}
#else
TEST(ThreadCheckerTest, DetachFromThreadInRelease) {
  ThreadCheckerClass::DetachThenCallFromDifferentThreadImpl();
}
#endif  // ENABLE_THREAD_CHECKER

#endif  // GTEST_HAS_DEATH_TEST || !ENABLE_THREAD_CHECKER

class ThreadAnnotateTest {
 public:
  // Next two function should create warnings when compile (e.g. if used with
  // specific T).
  // TODO(danilchap): Find a way to test they do not compile when thread
  // annotation checks enabled.
  template <typename T>
  void access_var_no_annotate() {
    var_thread_ = 42;
  }

  template <typename T>
  void access_fun_no_annotate() {
    function();
  }

  // Functions below should be able to compile.
  void access_var_annotate_thread() {
    RTC_DCHECK_RUN_ON(thread_);
    var_thread_ = 42;
  }

  void access_var_annotate_checker() {
    RTC_DCHECK_RUN_ON(&checker_);
    var_checker_ = 44;
  }

  void access_var_annotate_queue() {
    RTC_DCHECK_RUN_ON(queue_);
    var_queue_ = 46;
  }

  void access_fun_annotate() {
    RTC_DCHECK_RUN_ON(thread_);
    function();
  }

  void access_fun_and_var() {
    RTC_DCHECK_RUN_ON(thread_);
    fun_acccess_var();
  }

 private:
  void function() RTC_RUN_ON(thread_) {}
  void fun_acccess_var() RTC_RUN_ON(thread_) { var_thread_ = 13; }

  rtc::Thread* thread_;
  rtc::ThreadChecker checker_;
  rtc::TaskQueue* queue_;

  int var_thread_ RTC_GUARDED_BY(thread_);
  int var_checker_ RTC_GUARDED_BY(checker_);
  int var_queue_ RTC_GUARDED_BY(queue_);
};

// Just in case we ever get lumped together with other compilation units.
#undef ENABLE_THREAD_CHECKER

}  // namespace rtc
