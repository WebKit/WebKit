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

#include "absl/types/optional.h"
#include "rtc_base/event.h"
#include "system_wrappers/include/sleep.h"
#include "test/gmock.h"

namespace rtc {

TEST(PlatformThreadTest, DefaultConstructedIsEmpty) {
  PlatformThread thread;
  EXPECT_EQ(thread.GetHandle(), absl::nullopt);
  EXPECT_TRUE(thread.empty());
}

TEST(PlatformThreadTest, StartFinalize) {
  PlatformThread thread = PlatformThread::SpawnJoinable([] {}, "1");
  EXPECT_NE(thread.GetHandle(), absl::nullopt);
  EXPECT_FALSE(thread.empty());
  thread.Finalize();
  EXPECT_TRUE(thread.empty());
  rtc::Event done;
  thread = PlatformThread::SpawnDetached([&] { done.Set(); }, "2");
  EXPECT_FALSE(thread.empty());
  thread.Finalize();
  EXPECT_TRUE(thread.empty());
  done.Wait(30000);
}

TEST(PlatformThreadTest, MovesEmpty) {
  PlatformThread thread1;
  PlatformThread thread2 = std::move(thread1);
  EXPECT_TRUE(thread1.empty());
  EXPECT_TRUE(thread2.empty());
}

TEST(PlatformThreadTest, MovesHandles) {
  PlatformThread thread1 = PlatformThread::SpawnJoinable([] {}, "1");
  PlatformThread thread2 = std::move(thread1);
  EXPECT_TRUE(thread1.empty());
  EXPECT_FALSE(thread2.empty());
  rtc::Event done;
  thread1 = PlatformThread::SpawnDetached([&] { done.Set(); }, "2");
  thread2 = std::move(thread1);
  EXPECT_TRUE(thread1.empty());
  EXPECT_FALSE(thread2.empty());
  done.Wait(30000);
}

TEST(PlatformThreadTest,
     TwoThreadHandlesAreDifferentWhenStartedAndEqualWhenJoined) {
  PlatformThread thread1 = PlatformThread();
  PlatformThread thread2 = PlatformThread();
  EXPECT_EQ(thread1.GetHandle(), thread2.GetHandle());
  thread1 = PlatformThread::SpawnJoinable([] {}, "1");
  thread2 = PlatformThread::SpawnJoinable([] {}, "2");
  EXPECT_NE(thread1.GetHandle(), thread2.GetHandle());
  thread1.Finalize();
  EXPECT_NE(thread1.GetHandle(), thread2.GetHandle());
  thread2.Finalize();
  EXPECT_EQ(thread1.GetHandle(), thread2.GetHandle());
}

TEST(PlatformThreadTest, RunFunctionIsCalled) {
  bool flag = false;
  PlatformThread::SpawnJoinable([&] { flag = true; }, "T");
  EXPECT_TRUE(flag);
}

TEST(PlatformThreadTest, JoinsThread) {
  // This test flakes if there are problems with the join implementation.
  rtc::Event event;
  PlatformThread::SpawnJoinable([&] { event.Set(); }, "T");
  EXPECT_TRUE(event.Wait(/*give_up_after_ms=*/0));
}

TEST(PlatformThreadTest, StopsBeforeDetachedThreadExits) {
  // This test flakes if there are problems with the detached thread
  // implementation.
  bool flag = false;
  rtc::Event thread_started;
  rtc::Event thread_continue;
  rtc::Event thread_exiting;
  PlatformThread::SpawnDetached(
      [&] {
        thread_started.Set();
        thread_continue.Wait(Event::kForever);
        flag = true;
        thread_exiting.Set();
      },
      "T");
  thread_started.Wait(Event::kForever);
  EXPECT_FALSE(flag);
  thread_continue.Set();
  thread_exiting.Wait(Event::kForever);
  EXPECT_TRUE(flag);
}

}  // namespace rtc
