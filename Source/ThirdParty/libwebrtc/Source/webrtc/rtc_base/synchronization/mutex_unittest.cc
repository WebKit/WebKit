/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/synchronization/mutex.h"

#include <stddef.h>
#include <stdint.h>

#include <atomic>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "benchmark/benchmark.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"
#include "rtc_base/location.h"
#include "rtc_base/message_handler.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/synchronization/yield.h"
#include "rtc_base/thread.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::rtc::Event;
using ::rtc::Message;
using ::rtc::MessageHandler;
using ::rtc::Thread;

constexpr int kNumThreads = 16;

template <class MutexType>
class RTC_LOCKABLE RawMutexLocker {
 public:
  explicit RawMutexLocker(MutexType& mutex) : mutex_(mutex) {}
  void Lock() RTC_EXCLUSIVE_LOCK_FUNCTION() { mutex_.Lock(); }
  void Unlock() RTC_UNLOCK_FUNCTION() { mutex_.Unlock(); }

 private:
  MutexType& mutex_;
};

class RTC_LOCKABLE RawMutexTryLocker {
 public:
  explicit RawMutexTryLocker(Mutex& mutex) : mutex_(mutex) {}
  void Lock() RTC_EXCLUSIVE_LOCK_FUNCTION() {
    while (!mutex_.TryLock()) {
      YieldCurrentThread();
    }
  }
  void Unlock() RTC_UNLOCK_FUNCTION() { mutex_.Unlock(); }

 private:
  Mutex& mutex_;
};

template <class MutexType, class MutexLockType>
class MutexLockLocker {
 public:
  explicit MutexLockLocker(MutexType& mutex) : mutex_(mutex) {}
  void Lock() { lock_ = std::make_unique<MutexLockType>(&mutex_); }
  void Unlock() { lock_ = nullptr; }

 private:
  MutexType& mutex_;
  std::unique_ptr<MutexLockType> lock_;
};

template <class MutexType, class MutexLocker>
class LockRunner : public rtc::MessageHandlerAutoCleanup {
 public:
  template <typename... Args>
  explicit LockRunner(Args... args)
      : threads_active_(0),
        start_event_(true, false),
        done_event_(true, false),
        shared_value_(0),
        mutex_(args...),
        locker_(mutex_) {}

  bool Run() {
    // Signal all threads to start.
    start_event_.Set();

    // Wait for all threads to finish.
    return done_event_.Wait(kLongTime);
  }

  void SetExpectedThreadCount(int count) { threads_active_ = count; }

  int shared_value() {
    int shared_value;
    locker_.Lock();
    shared_value = shared_value_;
    locker_.Unlock();
    return shared_value_;
  }

  void OnMessage(Message* msg) override {
    ASSERT_TRUE(start_event_.Wait(kLongTime));
    locker_.Lock();

    EXPECT_EQ(0, shared_value_);
    int old = shared_value_;

    // Use a loop to increase the chance of race. If the |locker_|
    // implementation is faulty, it would be improbable that the error slips
    // through.
    for (int i = 0; i < kOperationsToRun; ++i) {
      benchmark::DoNotOptimize(++shared_value_);
    }
    EXPECT_EQ(old + kOperationsToRun, shared_value_);
    shared_value_ = 0;

    locker_.Unlock();
    if (threads_active_.fetch_sub(1) == 1) {
      done_event_.Set();
    }
  }

 private:
  static constexpr int kLongTime = 10000;  // 10 seconds
  static constexpr int kOperationsToRun = 1000;

  std::atomic<int> threads_active_;
  Event start_event_;
  Event done_event_;
  int shared_value_;
  MutexType mutex_;
  MutexLocker locker_;
};

void StartThreads(std::vector<std::unique_ptr<Thread>>& threads,
                  MessageHandler* handler) {
  for (int i = 0; i < kNumThreads; ++i) {
    std::unique_ptr<Thread> thread(Thread::Create());
    thread->Start();
    thread->Post(RTC_FROM_HERE, handler);
    threads.push_back(std::move(thread));
  }
}

TEST(MutexTest, ProtectsSharedResourceWithMutexAndRawMutexLocker) {
  std::vector<std::unique_ptr<Thread>> threads;
  LockRunner<Mutex, RawMutexLocker<Mutex>> runner;
  StartThreads(threads, &runner);
  runner.SetExpectedThreadCount(kNumThreads);
  EXPECT_TRUE(runner.Run());
  EXPECT_EQ(0, runner.shared_value());
}

TEST(MutexTest, ProtectsSharedResourceWithMutexAndRawMutexTryLocker) {
  std::vector<std::unique_ptr<Thread>> threads;
  LockRunner<Mutex, RawMutexTryLocker> runner;
  StartThreads(threads, &runner);
  runner.SetExpectedThreadCount(kNumThreads);
  EXPECT_TRUE(runner.Run());
  EXPECT_EQ(0, runner.shared_value());
}

TEST(MutexTest, ProtectsSharedResourceWithMutexAndMutexLocker) {
  std::vector<std::unique_ptr<Thread>> threads;
  LockRunner<Mutex, MutexLockLocker<Mutex, MutexLock>> runner;
  StartThreads(threads, &runner);
  runner.SetExpectedThreadCount(kNumThreads);
  EXPECT_TRUE(runner.Run());
  EXPECT_EQ(0, runner.shared_value());
}

TEST(MutexTest, ProtectsSharedResourceWithGlobalMutexAndRawMutexLocker) {
  std::vector<std::unique_ptr<Thread>> threads;
  LockRunner<GlobalMutex, RawMutexLocker<GlobalMutex>> runner(absl::kConstInit);
  StartThreads(threads, &runner);
  runner.SetExpectedThreadCount(kNumThreads);
  EXPECT_TRUE(runner.Run());
  EXPECT_EQ(0, runner.shared_value());
}

TEST(MutexTest, ProtectsSharedResourceWithGlobalMutexAndMutexLocker) {
  std::vector<std::unique_ptr<Thread>> threads;
  LockRunner<GlobalMutex, MutexLockLocker<GlobalMutex, GlobalMutexLock>> runner(
      absl::kConstInit);
  StartThreads(threads, &runner);
  runner.SetExpectedThreadCount(kNumThreads);
  EXPECT_TRUE(runner.Run());
  EXPECT_EQ(0, runner.shared_value());
}

TEST(MutexTest, GlobalMutexCanHaveStaticStorageDuration) {
  ABSL_CONST_INIT static GlobalMutex global_lock(absl::kConstInit);
  global_lock.Lock();
  global_lock.Unlock();
}

}  // namespace
}  // namespace webrtc
