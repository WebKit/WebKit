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

void NullRunFunction(void* obj) {}

// Function that sets a boolean.
void SetFlagRunFunction(void* obj) {
  bool* obj_as_bool = static_cast<bool*>(obj);
  *obj_as_bool = true;
}

}  // namespace

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

}  // namespace rtc
