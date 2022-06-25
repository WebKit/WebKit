/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/cpu_time.h"

#include "rtc_base/platform_thread.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/sleep.h"
#include "test/gtest.h"

// Only run these tests on non-instrumented builds, because timing on
// instrumented builds is unreliable, causing the test to be flaky.
#if defined(THREAD_SANITIZER) || defined(MEMORY_SANITIZER) || \
    defined(ADDRESS_SANITIZER)
#define MAYBE_TEST(test_name) DISABLED_##test_name
#else
#define MAYBE_TEST(test_name) test_name
#endif

namespace {
const int kAllowedErrorMillisecs = 30;
const int kProcessingTimeMillisecs = 500;
const int kWorkingThreads = 2;

// Consumes approximately kProcessingTimeMillisecs of CPU time in single thread.
void WorkingFunction(int64_t* counter) {
  *counter = 0;
  int64_t stop_cpu_time =
      rtc::GetThreadCpuTimeNanos() +
      kProcessingTimeMillisecs * rtc::kNumNanosecsPerMillisec;
  while (rtc::GetThreadCpuTimeNanos() < stop_cpu_time) {
    (*counter)++;
  }
}
}  // namespace

namespace rtc {

// A minimal test which can be run on instrumented builds, so that they're at
// least exercising the code to check for memory leaks/etc.
TEST(CpuTimeTest, BasicTest) {
  int64_t process_start_time_nanos = GetProcessCpuTimeNanos();
  int64_t thread_start_time_nanos = GetThreadCpuTimeNanos();
  int64_t process_duration_nanos =
      GetProcessCpuTimeNanos() - process_start_time_nanos;
  int64_t thread_duration_nanos =
      GetThreadCpuTimeNanos() - thread_start_time_nanos;
  EXPECT_GE(process_duration_nanos, 0);
  EXPECT_GE(thread_duration_nanos, 0);
}

TEST(CpuTimeTest, MAYBE_TEST(TwoThreads)) {
  int64_t process_start_time_nanos = GetProcessCpuTimeNanos();
  int64_t thread_start_time_nanos = GetThreadCpuTimeNanos();
  int64_t counter1;
  int64_t counter2;
  auto thread1 = PlatformThread::SpawnJoinable(
      [&counter1] { WorkingFunction(&counter1); }, "Thread1");
  auto thread2 = PlatformThread::SpawnJoinable(
      [&counter2] { WorkingFunction(&counter2); }, "Thread2");
  thread1.Finalize();
  thread2.Finalize();

  EXPECT_GE(counter1, 0);
  EXPECT_GE(counter2, 0);
  int64_t process_duration_nanos =
      GetProcessCpuTimeNanos() - process_start_time_nanos;
  int64_t thread_duration_nanos =
      GetThreadCpuTimeNanos() - thread_start_time_nanos;
  // This thread did almost nothing. Definetly less work than kProcessingTime.
  // Therefore GetThreadCpuTime is not a wall clock.
  EXPECT_LE(thread_duration_nanos,
            (kProcessingTimeMillisecs - kAllowedErrorMillisecs) *
                kNumNanosecsPerMillisec);
  // Total process time is at least twice working threads' CPU time.
  // Therefore process and thread times are correctly related.
  EXPECT_GE(process_duration_nanos,
            kWorkingThreads *
                (kProcessingTimeMillisecs - kAllowedErrorMillisecs) *
                kNumNanosecsPerMillisec);
}

TEST(CpuTimeTest, MAYBE_TEST(Sleeping)) {
  int64_t process_start_time_nanos = GetProcessCpuTimeNanos();
  webrtc::SleepMs(kProcessingTimeMillisecs);
  int64_t process_duration_nanos =
      GetProcessCpuTimeNanos() - process_start_time_nanos;
  // Sleeping should not introduce any additional CPU time.
  // Therefore GetProcessCpuTime is not a wall clock.
  EXPECT_LE(process_duration_nanos,
            (kProcessingTimeMillisecs - kAllowedErrorMillisecs) *
                kNumNanosecsPerMillisec);
}

}  // namespace rtc
