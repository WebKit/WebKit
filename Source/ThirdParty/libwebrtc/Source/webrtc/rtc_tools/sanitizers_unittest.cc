/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stddef.h>
#include <stdio.h>

#include <memory>
#include <random>

#include "rtc_base/checks.h"
#include "rtc_base/null_socket_server.h"
#include "rtc_base/thread.h"
#include "test/gtest.h"

namespace rtc {

namespace {

#if defined(MEMORY_SANITIZER)
void UseOfUninitializedValue() {
  int* buf = new int[2];
  std::random_device engine;
  if (buf[engine() % 2]) {  // Non-deterministic conditional.
    printf("Externally visible action.");
  }
  delete[] buf;
}

TEST(SanitizersDeathTest, MemorySanitizer) {
  EXPECT_DEATH(UseOfUninitializedValue(), "use-of-uninitialized-value");
}
#endif

#if defined(ADDRESS_SANITIZER)
void HeapUseAfterFree() {
  char* buf = new char[2];
  delete[] buf;
  buf[0] = buf[1];
}

TEST(SanitizersDeathTest, AddressSanitizer) {
  EXPECT_DEATH(HeapUseAfterFree(), "heap-use-after-free");
}
#endif

#if defined(UNDEFINED_SANITIZER)
// For ubsan:
void SignedIntegerOverflow() {
  int32_t x = 1234567890;
  x *= 2;
  (void)x;
}

// For ubsan_vptr:
struct Base {
  virtual void f() {}
  virtual ~Base() {}
};
struct Derived : public Base {};

void InvalidVptr() {
  Base b;
  auto* d = static_cast<Derived*>(&b);  // Bad downcast.
  d->f();  // Virtual function call with object of wrong dynamic type.
}

TEST(SanitizersDeathTest, UndefinedSanitizer) {
  EXPECT_DEATH(
      {
        SignedIntegerOverflow();
        InvalidVptr();
      },
      "runtime error");
}
#endif

#if defined(THREAD_SANITIZER)
class IncrementThread : public Thread {
 public:
  explicit IncrementThread(int* value)
      : Thread(std::make_unique<NullSocketServer>()), value_(value) {}

  IncrementThread(const IncrementThread&) = delete;
  IncrementThread& operator=(const IncrementThread&) = delete;

  void Run() override {
    ++*value_;
    Thread::Current()->SleepMs(100);
  }

  // Un-protect Thread::Join for the test.
  void Join() { Thread::Join(); }

 private:
  int* value_;
};

void DataRace() {
  int value = 0;
  IncrementThread thread1(&value);
  IncrementThread thread2(&value);
  thread1.Start();
  thread2.Start();
  thread1.Join();
  thread2.Join();
  // TSan seems to mess with gtest's death detection.
  // Fail intentionally, and rely on detecting the error message.
  RTC_CHECK_NOTREACHED();
}

TEST(SanitizersDeathTest, ThreadSanitizer) {
  EXPECT_DEATH(DataRace(), "data race");
}
#endif

}  // namespace

}  // namespace rtc
