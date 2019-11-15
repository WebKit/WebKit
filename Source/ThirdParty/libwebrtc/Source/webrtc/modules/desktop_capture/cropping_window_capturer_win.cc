/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/cropping_window_capturer.h"
#include "modules/desktop_capture/desktop_capturer_differ_wrapper.h"
#include "modules/desktop_capture/win/screen_capture_utils.h"
#include "modules/desktop_capture/win/selected_window_context.h"
#include "modules/desktop_capture/win/window_capture_utils.h"
#include "rtc_base/logging.h"
#include "rtc_base/trace_event.h"
#include "rtc_base/win32.h"

namespace webrtc {

namespace {

// Used to pass input/output data during the EnumWindows call for verifying if
// the selected window is on top.
struct TopWindowVerifierContext : public SelectedWindowContext {
  TopWindowVerifierContext(HWND selected_window,
                           HWND excluded_window,
                           DesktopRect selected_window_rect,
                           WindowCaptureHelperWin* window_capture_helper)
      : SelectedWindowContext(selected_window,
                              selected_window_rect,
                              window_capture_helper),
        excluded_window(excluded_window),
        is_top_window(false) {
    RTC_DCHECK_NE(selected_window, excluded_window);
  }

  const HWND excluded_window;
  bool is_top_window;
};

// The function is called during EnumWindow for every window enumerated and is
// responsible for verifying if the selected window is on top.
// Return TRUE to continue enumerating if the current window belongs to the
// selected window or is to be ignored.
// Return FALSE to stop enumerating if the selected window is found or decided
// if it's on top most.
BOOL CALLBACK TopWindowVerifier(HWND hwnd, LPARAM param) {
  TopWindowVerifierContext* context =
      reinterpret_cast<TopWindowVerifierContext*>(param);

  if (context->IsWindowSelected(hwnd)) {
    // Windows are enumerated in top-down z-order, so we can stop enumerating
    // upon reaching the selected window & report it's on top.
    context->is_top_window = true;
    return FALSE;
  }

  // Ignore the excluded window.
  if (hwnd == context->excluded_window) {
    return TRUE;
  }

  // Ignore invisible window on current desktop.
  if (!context->window_capture_helper()->IsWindowVisibleOnCurrentDesktop(
          hwnd)) {
    return TRUE;
  }

  // Ignore Chrome notification windows, especially the notification for the
  // ongoing window sharing.
  // Notes:
  // - This only works with notifications from Chrome, not other Apps.
  // - All notifications from Chrome will be ignored.
  // - This may cause part or whole of notification window being cropped into
  // the capturing of the target window if there is overlapping.
  if (context->window_capture_helper()->IsWindowChromeNotification(hwnd)) {
    return TRUE;
  }

  // Ignore descendant/owned windows since we want to capture them.
  if (context->IsWindowOwned(hwnd)) {
    return TRUE;
  }

  // Checks whether current window |hwnd| intersects with
  // |context|->selected_window.
  if (context->IsWindowOverlapping(hwnd)) {
    // If intersection is not empty, the selected window is not on top.
    context->is_top_window = false;
    return FALSE;
  }

  // Otherwise, keep enumerating.
  return TRUE;
}

class CroppingWindowCapturerWin : public CroppingWindowCapturer {
 public:
  CroppingWindowCapturerWin(const DesktopCaptureOptions& options)
      : CroppingWindowCapturer(options) {}

 private:
  bool ShouldUseScreenCapturer() override;
  DesktopRect GetWindowRectInVirtualScreen() override;

  // The region from GetWindowRgn in the desktop coordinate if the region is
  // rectangular, or the rect from GetWindowRect if the region is not set.
  DesktopRect window_region_rect_;

  WindowCaptureHelperWin window_capture_helper_;
};

bool CroppingWindowCapturerWin::ShouldUseScreenCapturer() {
  if (!rtc::IsWindows8OrLater() && window_capture_helper_.IsAeroEnabled()) {
    return false;
  }

  const HWND selected = reinterpret_cast<HWND>(selected_window());
  // Check if the window is visible on current desktop.
  if (!window_capture_helper_.IsWindowVisibleOnCurrentDesktop(selected)) {
    return false;
  }

  // Check if the window is a translucent layered window.
  const LONG window_ex_style = GetWindowLong(selected, GWL_EXSTYLE);
  if (window_ex_style & WS_EX_LAYERED) {
    COLORREF color_ref_key = 0;
    BYTE alpha = 0;
    DWORD flags = 0;

    // GetLayeredWindowAttributes fails if the window was setup with
    // UpdateLayeredWindow. We have no way to know the opacity of the window in
    // that case. This happens for Stiky Note (crbug/412726).
    if (!GetLayeredWindowAttributes(selected, &color_ref_key, &alpha, &flags))
      return false;

    // UpdateLayeredWindow is the only way to set per-pixel alpha and will cause
    // the previous GetLayeredWindowAttributes to fail. So we only need to check
    // the window wide color key or alpha.
    if ((flags & LWA_COLORKEY) || ((flags & LWA_ALPHA) && (alpha < 255))) {
      return false;
    }
  }

  if (!GetWindowRect(selected, &window_region_rect_)) {
    return false;
  }

  DesktopRect content_rect;
  if (!GetWindowContentRect(selected, &content_rect)) {
    return false;
  }

  DesktopRect region_rect;
  // Get the window region and check if it is rectangular.
  const int region_type =
      GetWindowRegionTypeWithBoundary(selected, &region_rect);

  // Do not use the screen capturer if the region is empty or not rectangular.
  if (region_type == COMPLEXREGION || region_type == NULLREGION) {
    return false;
  }

  if (region_type == SIMPLEREGION) {
    // The |region_rect| returned from GetRgnBox() is always in window
    // coordinate.
    region_rect.Translate(window_region_rect_.left(),
                          window_region_rect_.top());
    // MSDN: The window region determines the area *within* the window where the
    // system permits drawing.
    // https://msdn.microsoft.com/en-us/library/windows/desktop/dd144950(v=vs.85).aspx.
    //
    // |region_rect| should always be inside of |window_region_rect_|. So after
    // the intersection, |window_region_rect_| == |region_rect|. If so, what's
    // the point of the intersecting operations? Why cannot we directly retrieve
    // |window_region_rect_| from GetWindowRegionTypeWithBoundary() function?
    // TODO(zijiehe): Figure out the purpose of these intersections.
    window_region_rect_.IntersectWith(region_rect);
    content_rect.IntersectWith(region_rect);
  }

  // Check if the client area is out of the screen area. When the window is
  // maximized, only its client area is visible in the screen, the border will
  // be hidden. So we are using |content_rect| here.
  if (!GetFullscreenRect().ContainsRect(content_rect)) {
    return false;
  }

  // Check if the window is occluded by any other window, excluding the child
  // windows, context menus, and |excluded_window_|.
  // |content_rect| is preferred, see the comments in TopWindowVerifier()
  // function.
  TopWindowVerifierContext context(selected,
                                   reinterpret_cast<HWND>(excluded_window()),
                                   content_rect, &window_capture_helper_);
  if (!context.IsSelectedWindowValid()) {
    return false;
  }

  EnumWindows(&TopWindowVerifier, reinterpret_cast<LPARAM>(&context));
  return context.is_top_window;
}

DesktopRect CroppingWindowCapturerWin::GetWindowRectInVirtualScreen() {
  TRACE_EVENT0("webrtc",
               "CroppingWindowCapturerWin::GetWindowRectInVirtualScreen");
  DesktopRect window_rect;
  HWND hwnd = reinterpret_cast<HWND>(selected_window());
  if (!GetCroppedWindowRect(hwnd, /*avoid_cropping_border*/ false, &window_rect,
                            /*original_rect*/ nullptr)) {
    RTC_LOG(LS_WARNING) << "Failed to get window info: " << GetLastError();
    return window_rect;
  }
  window_rect.IntersectWith(window_region_rect_);

  // Convert |window_rect| to be relative to the top-left of the virtual screen.
  DesktopRect screen_rect(GetFullscreenRect());
  window_rect.IntersectWith(screen_rect);
  window_rect.Translate(-screen_rect.left(), -screen_rect.top());
  return window_rect;
}

}  // namespace

// static
std::unique_ptr<DesktopCapturer> CroppingWindowCapturer::CreateCapturer(
    const DesktopCaptureOptions& options) {
  std::unique_ptr<DesktopCapturer> capturer(
      new CroppingWindowCapturerWin(options));
  if (capturer && options.detect_updated_region()) {
    capturer.reset(new DesktopCapturerDifferWrapper(std::move(capturer)));
  }

  return capturer;
}

}  // namespace webrtc
