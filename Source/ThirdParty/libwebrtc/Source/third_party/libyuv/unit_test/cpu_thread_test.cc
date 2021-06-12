/*
 *  Copyright 2017 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <gtest/gtest.h>

#include "libyuv/cpu_id.h"

#if defined(__clang__) && !defined(__wasm__)
#if __has_include(<pthread.h>)
#define LIBYUV_HAVE_PTHREAD 1
#endif
#elif defined(__linux__)
#define LIBYUV_HAVE_PTHREAD 1
#endif

#ifdef LIBYUV_HAVE_PTHREAD
#include <pthread.h>
#endif

namespace libyuv {

#ifdef LIBYUV_HAVE_PTHREAD
void* ThreadMain(void* arg) {
  int* flags = static_cast<int*>(arg);

  *flags = TestCpuFlag(kCpuInitialized);
  return nullptr;
}
#endif  // LIBYUV_HAVE_PTHREAD

// Call TestCpuFlag() from two threads. ThreadSanitizer should not report any
// data race.
TEST(LibYUVCpuThreadTest, TestCpuFlagMultipleThreads) {
#ifdef LIBYUV_HAVE_PTHREAD
  int cpu_flags1;
  int cpu_flags2;
  int ret;
  pthread_t thread1;
  pthread_t thread2;

  MaskCpuFlags(0);  // Reset to 0 to allow auto detect.
  ret = pthread_create(&thread1, nullptr, ThreadMain, &cpu_flags1);
  ASSERT_EQ(ret, 0);
  ret = pthread_create(&thread2, nullptr, ThreadMain, &cpu_flags2);
  ASSERT_EQ(ret, 0);
  ret = pthread_join(thread1, nullptr);
  EXPECT_EQ(ret, 0);
  ret = pthread_join(thread2, nullptr);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(cpu_flags1, cpu_flags2);
#else
  printf("pthread unavailable; Test skipped.");
#endif  // LIBYUV_HAVE_PTHREAD
}

}  // namespace libyuv
