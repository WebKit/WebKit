/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/window_finder.h"

#include <stdint.h>

#include <memory>

#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/screen_drawer.h"
#include "rtc_base/logging.h"
#include "test/gtest.h"

#if defined(USE_X11)
#include "modules/desktop_capture/x11/shared_x_display.h"
#include "modules/desktop_capture/x11/x_atom_cache.h"
#include "rtc_base/ptr_util.h"
#endif

#if defined(WEBRTC_WIN)
#include <windows.h>

#include "modules/desktop_capture/window_finder_win.h"
#include "modules/desktop_capture/win/window_capture_utils.h"
#endif

namespace webrtc {

namespace {

#if defined(WEBRTC_WIN)
// ScreenDrawerWin does not have a message loop, so it's unresponsive to user
// inputs. WindowFinderWin cannot detect this kind of unresponsive windows.
// Instead, console window is used to test WindowFinderWin.
TEST(WindowFinderTest, FindConsoleWindow) {
  // Creates a ScreenDrawer to avoid this test from conflicting with
  // ScreenCapturerIntegrationTest: both tests require its window to be in
  // foreground.
  //
  // In ScreenCapturer related tests, this is controlled by
  // ScreenDrawer, which has a global lock to ensure only one ScreenDrawer
  // window is active. So even we do not use ScreenDrawer for Windows test,
  // creating an instance can block ScreenCapturer related tests until this test
  // finishes.
  //
  // Usually the test framework should take care of this "isolated test"
  // requirement, but unfortunately WebRTC trybots do not support this.
  std::unique_ptr<ScreenDrawer> drawer = ScreenDrawer::Create();
  const int kMaxSize = 10000;
  // Enlarges current console window.
  system("mode 1000,1000");
  const HWND console_window = GetConsoleWindow();
  // Ensures that current console window is visible.
  ShowWindow(console_window, SW_MAXIMIZE);
  // Moves the window to the top-left of the display.
  MoveWindow(console_window, 0, 0, kMaxSize, kMaxSize, true);

  // Brings console window to top.
  SetWindowPos(
      console_window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
  BringWindowToTop(console_window);

  WindowFinderWin finder;
  for (int i = 0; i < kMaxSize; i++) {
    const DesktopVector spot(i, i);
    const HWND id = reinterpret_cast<HWND>(finder.GetWindowUnderPoint(spot));
    if (id == console_window) {
      return;
    }
  }

  FAIL();
}

#else
TEST(WindowFinderTest, FindDrawerWindow) {
  WindowFinder::Options options;
#if defined(USE_X11)
  std::unique_ptr<XAtomCache> cache;
  const auto shared_x_display = SharedXDisplay::CreateDefault();
  if (shared_x_display) {
    cache = rtc::MakeUnique<XAtomCache>(shared_x_display->display());
    options.cache = cache.get();
  }
#endif
  std::unique_ptr<WindowFinder> finder = WindowFinder::Create(options);
  if (!finder) {
    RTC_LOG(LS_WARNING)
        << "No WindowFinder implementation for current platform.";
    return;
  }

  std::unique_ptr<ScreenDrawer> drawer = ScreenDrawer::Create();
  if (!drawer) {
    RTC_LOG(LS_WARNING)
        << "No ScreenDrawer implementation for current platform.";
    return;
  }

  if (drawer->window_id() == kNullWindowId) {
    // TODO(zijiehe): WindowFinderTest can use a dedicated window without
    // relying on ScreenDrawer.
    RTC_LOG(LS_WARNING)
        << "ScreenDrawer implementation for current platform does "
           "create a window.";
    return;
  }

  // ScreenDrawer may not be able to bring the window to the top. So we test
  // several spots, at least one of them should succeed.
  const DesktopRect region = drawer->DrawableRegion();
  if (region.is_empty()) {
    RTC_LOG(LS_WARNING)
        << "ScreenDrawer::DrawableRegion() is too small for the "
           "WindowFinderTest.";
    return;
  }

  for (int i = 0; i < region.width(); i++) {
    const DesktopVector spot(
        region.left() + i, region.top() + i * region.height() / region.width());
    const WindowId id = finder->GetWindowUnderPoint(spot);
    if (id == drawer->window_id()) {
      return;
    }
  }

  FAIL();
}
#endif

TEST(WindowFinderTest, ShouldReturnNullWindowIfSpotIsOutOfScreen) {
  WindowFinder::Options options;
#if defined(USE_X11)
  std::unique_ptr<XAtomCache> cache;
  const auto shared_x_display = SharedXDisplay::CreateDefault();
  if (shared_x_display) {
    cache = rtc::MakeUnique<XAtomCache>(shared_x_display->display());
    options.cache = cache.get();
  }
#endif
  std::unique_ptr<WindowFinder> finder = WindowFinder::Create(options);
  if (!finder) {
    RTC_LOG(LS_WARNING)
        << "No WindowFinder implementation for current platform.";
    return;
  }

  ASSERT_EQ(kNullWindowId, finder->GetWindowUnderPoint(
      DesktopVector(INT16_MAX, INT16_MAX)));
  ASSERT_EQ(kNullWindowId, finder->GetWindowUnderPoint(
      DesktopVector(INT16_MAX, INT16_MIN)));
  ASSERT_EQ(kNullWindowId, finder->GetWindowUnderPoint(
      DesktopVector(INT16_MIN, INT16_MAX)));
  ASSERT_EQ(kNullWindowId, finder->GetWindowUnderPoint(
      DesktopVector(INT16_MIN, INT16_MIN)));
}

}  // namespace

}  // namespace webrtc
