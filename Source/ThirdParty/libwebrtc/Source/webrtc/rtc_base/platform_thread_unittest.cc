/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/platform_thread.h"

#include "system_wrappers/include/sleep.h"
#include "test/gtest.h"

namespace rtc {
namespace {
// Function that does nothing, and reports success.
bool NullRunFunctionDeprecated(void* obj) {
  webrtc::SleepMs(2);  // Hand over timeslice, prevents busy looping.
  return true;
}

bool TooBusyRunFunction(void* obj) {
  // Indentionally busy looping.
  return true;
}

void NullRunFunction(void* obj) {}

// Function that sets a boolean.
bool SetFlagRunFunctionDeprecated(void* obj) {
  bool* obj_as_bool = static_cast<bool*>(obj);
  *obj_as_bool = true;
  webrtc::SleepMs(0);  // Hand over timeslice, prevents busy looping.
  return true;
}

void SetFlagRunFunction(void* obj) {
  bool* obj_as_bool = static_cast<bool*>(obj);
  *obj_as_bool = true;
}

}  // namespace

TEST(PlatformThreadTest, StartStopDeprecated) {
  PlatformThread thread(&NullRunFunctionDeprecated, nullptr,
                        "PlatformThreadTest");
  EXPECT_TRUE(thread.name() == "PlatformThreadTest");
  EXPECT_TRUE(thread.GetThreadRef() == 0);
  thread.Start();
  EXPECT_TRUE(thread.GetThreadRef() != 0);
  thread.Stop();
  EXPECT_TRUE(thread.GetThreadRef() == 0);
}

TEST(PlatformThreadTest, StartStop2Deprecated) {
  PlatformThread thread1(&NullRunFunctionDeprecated, nullptr,
                         "PlatformThreadTest1");
  PlatformThread thread2(&NullRunFunctionDeprecated, nullptr,
                         "PlatformThreadTest2");
  EXPECT_TRUE(thread1.GetThreadRef() == thread2.GetThreadRef());
  thread1.Start();
  thread2.Start();
  EXPECT_TRUE(thread1.GetThreadRef() != thread2.GetThreadRef());
  thread2.Stop();
  thread1.Stop();
}

TEST(PlatformThreadTest, RunFunctionIsCalledDeprecated) {
  bool flag = false;
  PlatformThread thread(&SetFlagRunFunctionDeprecated, &flag,
                        "RunFunctionIsCalled");
  thread.Start();

  // At this point, the flag may be either true or false.
  thread.Stop();

  // We expect the thread to have run at least once.
  EXPECT_TRUE(flag);
}

TEST(PlatformThreadTest, StartStop) {
  PlatformThread thread(&NullRunFunction, nullptr, "PlatformThreadTest");
  EXPECT_TRUE(thread.name() == "PlatformThreadTest");
  EXPECT_TRUE(thread.GetThreadRef() == 0);
  thread.Start();
  EXPECT_TRUE(thread.GetThreadRef() != 0);
  thread.Stop();
  EXPECT_TRUE(thread.GetThreadRef() == 0);
}

TEST(PlatformThreadTest, StartStop2) {
  PlatformThread thread1(&NullRunFunction, nullptr, "PlatformThreadTest1");
  PlatformThread thread2(&NullRunFunction, nullptr, "PlatformThreadTest2");
  EXPECT_TRUE(thread1.GetThreadRef() == thread2.GetThreadRef());
  thread1.Start();
  thread2.Start();
  EXPECT_TRUE(thread1.GetThreadRef() != thread2.GetThreadRef());
  thread2.Stop();
  thread1.Stop();
}

TEST(PlatformThreadTest, RunFunctionIsCalled) {
  bool flag = false;
  PlatformThread thread(&SetFlagRunFunction, &flag, "RunFunctionIsCalled");
  thread.Start();

  // At this point, the flag may be either true or false.
  thread.Stop();

  // We expect the thread to have run at least once.
  EXPECT_TRUE(flag);
}

// This test is disabled since it will cause a crash.
// There might be a way to implement this as a death test, but it looks like
// a death test requires an expression to be checked but does not allow a
// flag to be raised that says "some thread will crash after this point".
// TODO(tommi): Look into ways to enable the test by default.
TEST(PlatformThreadTest, DISABLED_TooBusyDeprecated) {
  PlatformThread thread(&TooBusyRunFunction, nullptr, "BusyThread");
  thread.Start();
  webrtc::SleepMs(1000);
  thread.Stop();
}

}  // namespace rtc
