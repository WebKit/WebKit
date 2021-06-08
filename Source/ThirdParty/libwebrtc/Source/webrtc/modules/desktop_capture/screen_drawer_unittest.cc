/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/screen_drawer.h"

#include <stdint.h>

#include <atomic>
#include <memory>

#include "api/function_view.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/platform_thread.h"
#include "rtc_base/random.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/sleep.h"
#include "test/gtest.h"

#if defined(WEBRTC_POSIX)
#include "modules/desktop_capture/screen_drawer_lock_posix.h"
#endif

namespace webrtc {

namespace {

void TestScreenDrawerLock(
    rtc::FunctionView<std::unique_ptr<ScreenDrawerLock>()> ctor) {
  constexpr int kLockDurationMs = 100;

  std::atomic<bool> created(false);
  std::atomic<bool> ready(false);

  class Task {
   public:
    Task(std::atomic<bool>* created,
         const std::atomic<bool>& ready,
         rtc::FunctionView<std::unique_ptr<ScreenDrawerLock>()> ctor)
        : created_(created), ready_(ready), ctor_(ctor) {}

    ~Task() = default;

    void RunTask() {
      std::unique_ptr<ScreenDrawerLock> lock = ctor_();
      ASSERT_TRUE(!!lock);
      created_->store(true);
      // Wait for the main thread to get the signal of created_.
      while (!ready_.load()) {
        SleepMs(1);
      }
      // At this point, main thread should begin to create a second lock. Though
      // it's still possible the second lock won't be created before the
      // following sleep has been finished, the possibility will be
      // significantly reduced.
      const int64_t current_ms = rtc::TimeMillis();
      // SleepMs() may return early. See
      // https://cs.chromium.org/chromium/src/third_party/webrtc/system_wrappers/include/sleep.h?rcl=4a604c80cecce18aff6fc5e16296d04675312d83&l=20
      // But we need to ensure at least 100 ms has been passed before unlocking
      // |lock|.
      while (rtc::TimeMillis() - current_ms < kLockDurationMs) {
        SleepMs(kLockDurationMs - (rtc::TimeMillis() - current_ms));
      }
    }

   private:
    std::atomic<bool>* const created_;
    const std::atomic<bool>& ready_;
    const rtc::FunctionView<std::unique_ptr<ScreenDrawerLock>()> ctor_;
  } task(&created, ready, ctor);

  auto lock_thread = rtc::PlatformThread::SpawnJoinable(
      [&task] { task.RunTask(); }, "lock_thread");

  // Wait for the first lock in Task::RunTask() to be created.
  // TODO(zijiehe): Find a better solution to wait for the creation of the first
  // lock. See
  // https://chromium-review.googlesource.com/c/607688/13/webrtc/modules/desktop_capture/screen_drawer_unittest.cc
  while (!created.load()) {
    SleepMs(1);
  }

  const int64_t start_ms = rtc::TimeMillis();
  ready.store(true);
  // This is unlikely to fail, but just in case current thread is too laggy and
  // cause the SleepMs() in RunTask() to finish before we creating another lock.
  ASSERT_GT(kLockDurationMs, rtc::TimeMillis() - start_ms);
  ctor();
  ASSERT_LE(kLockDurationMs, rtc::TimeMillis() - start_ms);
}

}  // namespace

// These are a set of manual test cases, as we do not have an automatical way to
// detect whether a ScreenDrawer on a certain platform works well without
// ScreenCapturer(s). So you may execute these test cases with
// --gtest_also_run_disabled_tests --gtest_filter=ScreenDrawerTest.*.
TEST(ScreenDrawerTest, DISABLED_DrawRectangles) {
  std::unique_ptr<ScreenDrawer> drawer = ScreenDrawer::Create();
  if (!drawer) {
    RTC_LOG(LS_WARNING)
        << "No ScreenDrawer implementation for current platform.";
    return;
  }

  if (drawer->DrawableRegion().is_empty()) {
    RTC_LOG(LS_WARNING)
        << "ScreenDrawer of current platform does not provide a "
           "non-empty DrawableRegion().";
    return;
  }

  DesktopRect rect = drawer->DrawableRegion();
  Random random(rtc::TimeMicros());
  for (int i = 0; i < 100; i++) {
    // Make sure we at least draw one pixel.
    int left = random.Rand(rect.left(), rect.right() - 2);
    int top = random.Rand(rect.top(), rect.bottom() - 2);
    drawer->DrawRectangle(
        DesktopRect::MakeLTRB(left, top, random.Rand(left + 1, rect.right()),
                              random.Rand(top + 1, rect.bottom())),
        RgbaColor(random.Rand<uint8_t>(), random.Rand<uint8_t>(),
                  random.Rand<uint8_t>(), random.Rand<uint8_t>()));

    if (i == 50) {
      SleepMs(10000);
    }
  }

  SleepMs(10000);
}

#if defined(THREAD_SANITIZER)  // bugs.webrtc.org/10019
#define MAYBE_TwoScreenDrawerLocks DISABLED_TwoScreenDrawerLocks
#else
#define MAYBE_TwoScreenDrawerLocks TwoScreenDrawerLocks
#endif
TEST(ScreenDrawerTest, MAYBE_TwoScreenDrawerLocks) {
#if defined(WEBRTC_POSIX)
  // ScreenDrawerLockPosix won't be able to unlink the named semaphore. So use a
  // different semaphore name here to avoid deadlock.
  const char* semaphore_name = "GSDL8784541a812011e788ff67427b";
  ScreenDrawerLockPosix::Unlink(semaphore_name);

  TestScreenDrawerLock([semaphore_name]() {
    return std::make_unique<ScreenDrawerLockPosix>(semaphore_name);
  });
#elif defined(WEBRTC_WIN)
  TestScreenDrawerLock([]() { return ScreenDrawerLock::Create(); });
#endif
}

}  // namespace webrtc
