/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdint>

#include "api/task_queue/task_queue_factory.h"
#include "api/units/time_delta.h"
#include "rtc_base/event.h"
#include "rtc_base/task_queue_for_test.h"
#include "rtc_base/time_utils.h"
#include "test/gtest.h"

#if defined(WEBRTC_WIN)
// clang-format off
#include <windows.h>  // Must come first.
#include <mmsystem.h>
// clang-format on
#endif

namespace webrtc {
namespace {

// Noop on all platforms except Windows, where it turns on high precision
// multimedia timers which increases the precision of TimeMillis() while in
// scope.
class EnableHighResTimers {
 public:
#if !defined(WEBRTC_WIN)
  EnableHighResTimers() {}
#else
  EnableHighResTimers() : enabled_(timeBeginPeriod(1) == TIMERR_NOERROR) {}
  ~EnableHighResTimers() {
    if (enabled_)
      timeEndPeriod(1);
  }

 private:
  const bool enabled_;
#endif
};

}  // namespace

// This task needs to be run manually due to the slowness of some of our bots.
// TODO(tommi): Can we run this on the perf bots?
TEST(TaskQueueTest, DISABLED_PostDelayedHighRes) {
  EnableHighResTimers high_res_scope;

  static const char kQueueName[] = "PostDelayedHighRes";
  Event event;
  TaskQueueForTest queue(kQueueName, TaskQueueFactory::Priority::kHigh);

  uint32_t start = TimeMillis();
  queue.PostDelayedTask(
      [&event, &queue] {
        EXPECT_TRUE(queue.IsCurrent());
        event.Set();
      },
      TimeDelta::Millis(3));
  EXPECT_TRUE(event.Wait(TimeDelta::Seconds(1)));
  uint32_t end = TimeMillis();
  // These tests are a little relaxed due to how "powerful" our test bots can
  // be.  Most recently we've seen windows bots fire the callback after 94-99ms,
  // which is why we have a little bit of leeway backwards as well.
  EXPECT_GE(end - start, 3u);
  EXPECT_NEAR(end - start, 3, 3u);
}

}  // namespace webrtc
