/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/screen_drawer.h"

#include <stdint.h>

#include "webrtc/base/logging.h"
#include "webrtc/base/random.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/system_wrappers/include/sleep.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

// These are a set of manual test cases, as we do not have an automatical way to
// detect whether a ScreenDrawer on a certain platform works well without
// ScreenCapturer(s). So you may execute these test cases with
// --gtest_also_run_disabled_tests --gtest_filter=ScreenDrawerTest.*.
TEST(ScreenDrawerTest, DISABLED_DrawRectangles) {
  std::unique_ptr<ScreenDrawer> drawer = ScreenDrawer::Create();
  if (!drawer) {
    LOG(LS_WARNING) << "No ScreenDrawer implementation for current platform.";
    return;
  }

  if (drawer->DrawableRegion().is_empty()) {
    LOG(LS_WARNING) << "ScreenDrawer of current platform does not provide a "
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

}  // namespace webrtc
