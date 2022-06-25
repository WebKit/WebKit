/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/android/native_api/stacktrace/stacktrace.h"

#include <dlfcn.h>

#include <atomic>
#include <memory>
#include <vector>

#include "rtc_base/event.h"
#include "rtc_base/logging.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/string_utils.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/synchronization/mutex.h"
#include "rtc_base/system/inline.h"
#include "system_wrappers/include/sleep.h"
#include "test/gtest.h"

namespace webrtc {
namespace test {

namespace {

// A simple atomic spin event. Implemented with std::atomic_flag, since the C++
// standard guarantees that that type is implemented with actual atomic
// instructions (as opposed to e.g. with a mutex). Uses sequentially consistent
// memory order since this is a test, where simplicity trumps performance.
class SimpleSpinEvent {
 public:
  // Initialize the event to its blocked state.
  SimpleSpinEvent() {
    static_cast<void>(blocked_.test_and_set(std::memory_order_seq_cst));
  }

  // Busy-wait for the event to become unblocked, and block it behind us as we
  // leave.
  void Wait() {
    bool was_blocked;
    do {
      // Check if the event was blocked, and set it to blocked.
      was_blocked = blocked_.test_and_set(std::memory_order_seq_cst);
    } while (was_blocked);
  }

  // Unblock the event.
  void Set() { blocked_.clear(std::memory_order_seq_cst); }

 private:
  std::atomic_flag blocked_;
};

// Returns the execution address relative to the .so base address. This matches
// the addresses we get from GetStacktrace().
RTC_NO_INLINE uint32_t GetCurrentRelativeExecutionAddress() {
  void* pc = __builtin_return_address(0);
  Dl_info dl_info = {};
  const bool success = dladdr(pc, &dl_info);
  EXPECT_TRUE(success);
  return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(pc) -
                               reinterpret_cast<uintptr_t>(dl_info.dli_fbase));
}

// Returns true if any of the stack trace element is inside the specified
// region.
bool StackTraceContainsRange(const std::vector<StackTraceElement>& stack_trace,
                             uintptr_t pc_low,
                             uintptr_t pc_high) {
  for (const StackTraceElement& stack_trace_element : stack_trace) {
    if (pc_low <= stack_trace_element.relative_address &&
        pc_high >= stack_trace_element.relative_address) {
      return true;
    }
  }
  return false;
}

class DeadlockInterface {
 public:
  virtual ~DeadlockInterface() {}

  // This function should deadlock until Release() is called.
  virtual void Deadlock() = 0;

  // This function should release the thread stuck in Deadlock().
  virtual void Release() = 0;
};

struct ThreadParams {
  volatile int tid;
  // Signaled when the deadlock region is entered.
  SimpleSpinEvent deadlock_start_event;
  DeadlockInterface* volatile deadlock_impl;
  // Defines an address range within the deadlock will occur.
  volatile uint32_t deadlock_region_start_address;
  volatile uint32_t deadlock_region_end_address;
  // Signaled when the deadlock is done.
  rtc::Event deadlock_done_event;
};

class RtcEventDeadlock : public DeadlockInterface {
 private:
  void Deadlock() override { event.Wait(rtc::Event::kForever); }
  void Release() override { event.Set(); }

  rtc::Event event;
};

class RtcCriticalSectionDeadlock : public DeadlockInterface {
 public:
  RtcCriticalSectionDeadlock()
      : mutex_lock_(std::make_unique<MutexLock>(&mutex_)) {}

 private:
  void Deadlock() override { MutexLock lock(&mutex_); }

  void Release() override { mutex_lock_.reset(); }

  Mutex mutex_;
  std::unique_ptr<MutexLock> mutex_lock_;
};

class SpinDeadlock : public DeadlockInterface {
 public:
  SpinDeadlock() : is_deadlocked_(true) {}

 private:
  void Deadlock() override {
    while (is_deadlocked_) {
    }
  }

  void Release() override { is_deadlocked_ = false; }

  std::atomic<bool> is_deadlocked_;
};

class SleepDeadlock : public DeadlockInterface {
 private:
  void Deadlock() override { sleep(1000000); }

  void Release() override {
    // The interrupt itself will break free from the sleep.
  }
};

void TestStacktrace(std::unique_ptr<DeadlockInterface> deadlock_impl) {
  // Set params that will be sent to other thread.
  ThreadParams params;
  params.deadlock_impl = deadlock_impl.get();

  // Spawn thread.
  auto thread = rtc::PlatformThread::SpawnJoinable(
      [&params] {
        params.tid = gettid();
        params.deadlock_region_start_address =
            GetCurrentRelativeExecutionAddress();
        params.deadlock_start_event.Set();
        params.deadlock_impl->Deadlock();
        params.deadlock_region_end_address =
            GetCurrentRelativeExecutionAddress();
        params.deadlock_done_event.Set();
      },
      "StacktraceTest");

  // Wait until the thread has entered the deadlock region, and take a very
  // brief nap to give it time to reach the actual deadlock.
  params.deadlock_start_event.Wait();
  SleepMs(1);

  // Acquire the stack trace of the thread which should now be deadlocking.
  std::vector<StackTraceElement> stack_trace = GetStackTrace(params.tid);

  // Release the deadlock so that the thread can continue.
  deadlock_impl->Release();

  // Wait until the thread has left the deadlock.
  params.deadlock_done_event.Wait(rtc::Event::kForever);

  // Assert that the stack trace contains the deadlock region.
  EXPECT_TRUE(StackTraceContainsRange(stack_trace,
                                      params.deadlock_region_start_address,
                                      params.deadlock_region_end_address))
      << "Deadlock region: ["
      << rtc::ToHex(params.deadlock_region_start_address) << ", "
      << rtc::ToHex(params.deadlock_region_end_address)
      << "] not contained in: " << StackTraceToString(stack_trace);
}

class LookoutLogSink final : public rtc::LogSink {
 public:
  explicit LookoutLogSink(std::string look_for)
      : look_for_(std::move(look_for)) {}
  void OnLogMessage(const std::string& message) override {
    if (message.find(look_for_) != std::string::npos) {
      when_found_.Set();
    }
  }
  rtc::Event& WhenFound() { return when_found_; }

 private:
  const std::string look_for_;
  rtc::Event when_found_;
};

}  // namespace

TEST(Stacktrace, TestCurrentThread) {
  const uint32_t start_addr = GetCurrentRelativeExecutionAddress();
  const std::vector<StackTraceElement> stack_trace = GetStackTrace();
  const uint32_t end_addr = GetCurrentRelativeExecutionAddress();
  EXPECT_TRUE(StackTraceContainsRange(stack_trace, start_addr, end_addr))
      << "Caller region: [" << rtc::ToHex(start_addr) << ", "
      << rtc::ToHex(end_addr)
      << "] not contained in: " << StackTraceToString(stack_trace);
}

TEST(Stacktrace, TestSpinLock) {
  TestStacktrace(std::make_unique<SpinDeadlock>());
}

TEST(Stacktrace, TestSleep) {
  TestStacktrace(std::make_unique<SleepDeadlock>());
}

// Stack traces originating from kernel space does not include user space stack
// traces for ARM 32.
#ifdef WEBRTC_ARCH_ARM64

TEST(Stacktrace, TestRtcEvent) {
  TestStacktrace(std::make_unique<RtcEventDeadlock>());
}

TEST(Stacktrace, TestRtcCriticalSection) {
  TestStacktrace(std::make_unique<RtcCriticalSectionDeadlock>());
}

#endif

TEST(Stacktrace, TestRtcEventDeadlockDetection) {
  // Start looking for the expected log output.
  LookoutLogSink sink(/*look_for=*/"Probable deadlock");
  rtc::LogMessage::AddLogToStream(&sink, rtc::LS_WARNING);

  // Start a thread that waits for an event.
  rtc::Event ev;
  auto thread = rtc::PlatformThread::SpawnJoinable(
      [&ev] { ev.Wait(rtc::Event::kForever); },
      "TestRtcEventDeadlockDetection");

  // The message should appear after 3 sec. We'll wait up to 10 sec in an
  // attempt to not be flaky.
  EXPECT_TRUE(sink.WhenFound().Wait(10000));

  // Unblock the thread and shut it down.
  ev.Set();
  thread.Finalize();
  rtc::LogMessage::RemoveLogToStream(&sink);
}

}  // namespace test
}  // namespace webrtc
