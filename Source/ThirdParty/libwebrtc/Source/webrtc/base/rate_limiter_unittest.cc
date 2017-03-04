/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <memory>

#include "webrtc/base/event.h"
#include "webrtc/base/platform_thread.h"
#include "webrtc/base/rate_limiter.h"
#include "webrtc/base/task_queue.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

class RateLimitTest : public ::testing::Test {
 public:
  RateLimitTest()
      : clock_(0), rate_limiter(new RateLimiter(&clock_, kWindowSizeMs)) {}
  ~RateLimitTest() override {}

  void SetUp() override { rate_limiter->SetMaxRate(kMaxRateBps); }

 protected:
  static constexpr int64_t kWindowSizeMs = 1000;
  static constexpr uint32_t kMaxRateBps = 100000;
  // Bytes needed to completely saturate the rate limiter.
  static constexpr size_t kRateFillingBytes =
      (kMaxRateBps * kWindowSizeMs) / (8 * 1000);
  SimulatedClock clock_;
  std::unique_ptr<RateLimiter> rate_limiter;
};

TEST_F(RateLimitTest, IncreasingMaxRate) {
  // Fill rate, extend window to full size.
  EXPECT_TRUE(rate_limiter->TryUseRate(kRateFillingBytes / 2));
  clock_.AdvanceTimeMilliseconds(kWindowSizeMs - 1);
  EXPECT_TRUE(rate_limiter->TryUseRate(kRateFillingBytes / 2));

  // All rate consumed.
  EXPECT_FALSE(rate_limiter->TryUseRate(1));

  // Double the available rate and fill that too.
  rate_limiter->SetMaxRate(kMaxRateBps * 2);
  EXPECT_TRUE(rate_limiter->TryUseRate(kRateFillingBytes));

  // All rate consumed again.
  EXPECT_FALSE(rate_limiter->TryUseRate(1));
}

TEST_F(RateLimitTest, DecreasingMaxRate) {
  // Fill rate, extend window to full size.
  EXPECT_TRUE(rate_limiter->TryUseRate(kRateFillingBytes / 2));
  clock_.AdvanceTimeMilliseconds(kWindowSizeMs - 1);
  EXPECT_TRUE(rate_limiter->TryUseRate(kRateFillingBytes / 2));

  // All rate consumed.
  EXPECT_FALSE(rate_limiter->TryUseRate(1));

  // Halve the available rate and move window so half of the data falls out.
  rate_limiter->SetMaxRate(kMaxRateBps / 2);
  clock_.AdvanceTimeMilliseconds(1);

  // All rate still consumed.
  EXPECT_FALSE(rate_limiter->TryUseRate(1));
}

TEST_F(RateLimitTest, ChangingWindowSize) {
  // Fill rate, extend window to full size.
  EXPECT_TRUE(rate_limiter->TryUseRate(kRateFillingBytes / 2));
  clock_.AdvanceTimeMilliseconds(kWindowSizeMs - 1);
  EXPECT_TRUE(rate_limiter->TryUseRate(kRateFillingBytes / 2));

  // All rate consumed.
  EXPECT_FALSE(rate_limiter->TryUseRate(1));

  // Decrease window size so half of the data falls out.
  rate_limiter->SetWindowSize(kWindowSizeMs / 2);
  // Average rate should still be the same, so rate is still all consumed.
  EXPECT_FALSE(rate_limiter->TryUseRate(1));

  // Increase window size again. Now the rate is only half used (removed data
  // points don't come back to life).
  rate_limiter->SetWindowSize(kWindowSizeMs);
  EXPECT_TRUE(rate_limiter->TryUseRate(kRateFillingBytes / 2));

  // All rate consumed again.
  EXPECT_FALSE(rate_limiter->TryUseRate(1));
}

TEST_F(RateLimitTest, SingleUsageAlwaysOk) {
  // Using more bytes than can fit in a window is OK for a single packet.
  EXPECT_TRUE(rate_limiter->TryUseRate(kRateFillingBytes + 1));
}

TEST_F(RateLimitTest, WindowSizeLimits) {
  EXPECT_TRUE(rate_limiter->SetWindowSize(1));
  EXPECT_FALSE(rate_limiter->SetWindowSize(0));
  EXPECT_TRUE(rate_limiter->SetWindowSize(kWindowSizeMs));
  EXPECT_FALSE(rate_limiter->SetWindowSize(kWindowSizeMs + 1));
}

static const int64_t kMaxTimeoutMs = 30000;

class ThreadTask {
 public:
  explicit ThreadTask(RateLimiter* rate_limiter)
      : rate_limiter_(rate_limiter),
        start_signal_(false, false),
        end_signal_(false, false) {}
  virtual ~ThreadTask() {}

  void Run() {
    start_signal_.Wait(kMaxTimeoutMs);
    DoRun();
    end_signal_.Set();
  }

  virtual void DoRun() = 0;

  RateLimiter* const rate_limiter_;
  rtc::Event start_signal_;
  rtc::Event end_signal_;
};

void RunTask(void* thread_task) {
  reinterpret_cast<ThreadTask*>(thread_task)->Run();
}

TEST_F(RateLimitTest, MultiThreadedUsage) {
  // Simple sanity test, with different threads calling the various methods.
  // Runs a few simple tasks, each on its own thread, but coordinated with
  // events so that they run in a serialized order. Intended to catch data
  // races when run with tsan et al.

  // Half window size, double rate -> same amount of bytes needed to fill rate.

  class SetWindowSizeTask : public ThreadTask {
   public:
    explicit SetWindowSizeTask(RateLimiter* rate_limiter)
        : ThreadTask(rate_limiter) {}
    ~SetWindowSizeTask() override {}

    void DoRun() override {
      EXPECT_TRUE(rate_limiter_->SetWindowSize(kWindowSizeMs / 2));
    }
  } set_window_size_task(rate_limiter.get());
  rtc::PlatformThread thread1(RunTask, &set_window_size_task, "Thread1");
  thread1.Start();

  class SetMaxRateTask : public ThreadTask {
   public:
    explicit SetMaxRateTask(RateLimiter* rate_limiter)
        : ThreadTask(rate_limiter) {}
    ~SetMaxRateTask() override {}

    void DoRun() override { rate_limiter_->SetMaxRate(kMaxRateBps * 2); }
  } set_max_rate_task(rate_limiter.get());
  rtc::PlatformThread thread2(RunTask, &set_max_rate_task, "Thread2");
  thread2.Start();

  class UseRateTask : public ThreadTask {
   public:
    UseRateTask(RateLimiter* rate_limiter, SimulatedClock* clock)
        : ThreadTask(rate_limiter), clock_(clock) {}
    ~UseRateTask() override {}

    void DoRun() override {
      EXPECT_TRUE(rate_limiter_->TryUseRate(kRateFillingBytes / 2));
      clock_->AdvanceTimeMilliseconds((kWindowSizeMs / 2) - 1);
      EXPECT_TRUE(rate_limiter_->TryUseRate(kRateFillingBytes / 2));
    }

    SimulatedClock* const clock_;
  } use_rate_task(rate_limiter.get(), &clock_);
  rtc::PlatformThread thread3(RunTask, &use_rate_task, "Thread3");
  thread3.Start();

  set_window_size_task.start_signal_.Set();
  EXPECT_TRUE(set_window_size_task.end_signal_.Wait(kMaxTimeoutMs));

  set_max_rate_task.start_signal_.Set();
  EXPECT_TRUE(set_max_rate_task.end_signal_.Wait(kMaxTimeoutMs));

  use_rate_task.start_signal_.Set();
  EXPECT_TRUE(use_rate_task.end_signal_.Wait(kMaxTimeoutMs));

  // All rate consumed.
  EXPECT_FALSE(rate_limiter->TryUseRate(1));

  thread1.Stop();
  thread2.Stop();
  thread3.Stop();
}

}  // namespace webrtc
