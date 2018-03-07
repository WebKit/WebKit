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

#include "modules/desktop_capture/win/screen_capture_utils.h"
#include "modules/desktop_capture/win/window_capture_utils.h"
#include "rtc_base/logging.h"
#include "rtc_base/win32.h"

namespace webrtc {

namespace {

// Used to pass input/output data during the EnumWindow call for verifying if
// the selected window is on top.
struct TopWindowVerifierContext {
  TopWindowVerifierContext(HWND selected_window,
                           HWND excluded_window,
                           DesktopRect selected_window_rect)
      : selected_window(selected_window),
        excluded_window(excluded_window),
        selected_window_rect(selected_window_rect),
        is_top_window(false) {
    RTC_DCHECK_NE(selected_window, excluded_window);
  }

  const HWND selected_window;
  const HWND excluded_window;
  const DesktopRect selected_window_rect;
  bool is_top_window;
};

// The function is called during EnumWindow for every window enumerated and is
// responsible for verifying if the selected window is on top.
BOOL CALLBACK TopWindowVerifier(HWND hwnd, LPARAM param) {
  TopWindowVerifierContext* context =
      reinterpret_cast<TopWindowVerifierContext*>(param);

  if (hwnd == context->selected_window) {
    context->is_top_window = true;
    return FALSE;
  }

  // Ignore the excluded window.
  if (hwnd == context->excluded_window) {
    return TRUE;
  }

  // Ignore hidden or minimized window.
  if (IsIconic(hwnd) || !IsWindowVisible(hwnd)) {
    return TRUE;
  }

  // Ignore descendant windows since we want to capture them.
  // This check does not work for tooltips and context menus. Drop down menus
  // and popup windows are fine.
  //
  // GA_ROOT returns the root window instead of the owner. I.e. for a dialog
  // window, GA_ROOT returns the dialog window itself. GA_ROOTOWNER returns the
  // application main window which opens the dialog window. Since we are sharing
  // the application main window, GA_ROOT should be used here.
  if (GetAncestor(hwnd, GA_ROOT) == context->selected_window) {
    return TRUE;
  }

  // If |hwnd| has no title and belongs to the same process, assume it's a
  // tooltip or context menu from the selected window and ignore it.
  // TODO(zijiehe): This check cannot cover the case where tooltip or context
  // menu of the child-window is covering the main window. See
  // https://bugs.chromium.org/p/webrtc/issues/detail?id=8062 for details.
  const size_t kTitleLength = 32;
  WCHAR window_title[kTitleLength];
  GetWindowText(hwnd, window_title, kTitleLength);
  if (wcsnlen_s(window_title, kTitleLength) == 0) {
    DWORD enumerated_window_process_id;
    DWORD selected_window_process_id;
    GetWindowThreadProcessId(hwnd, &enumerated_window_process_id);
    GetWindowThreadProcessId(context->selected_window,
                             &selected_window_process_id);
    if (selected_window_process_id == enumerated_window_process_id) {
      return TRUE;
    }
  }

  // Checks whether current window |hwnd| intersects with
  // |context|->selected_window.
  // |content_rect| is preferred because,
  // 1. WindowCapturerWin is using GDI capturer, which cannot capture DX output.
  //    So ScreenCapturer should be used as much as possible to avoid
  //    uncapturable cases. Note: lots of new applications are using DX output
  //    (hardware acceleration) to improve the performance which cannot be
  //    captured by WindowCapturerWin. See bug http://crbug.com/741770.
  // 2. WindowCapturerWin is still useful because we do not want to expose the
  //    content on other windows if the target window is covered by them.
  // 3. Shadow and borders should not be considered as "content" on other
  //    windows because they do not expose any useful information.
  //
  // So we can bear the false-negative cases (target window is covered by the
  // borders or shadow of other windows, but we have not detected it) in favor
  // of using ScreenCapturer, rather than let the false-positive cases (target
  // windows is only covered by borders or shadow of other windows, but we treat
  // it as overlapping) impact the user experience.
  DesktopRect content_rect;
  if (!GetWindowContentRect(hwnd, &content_rect)) {
    // Bail out if failed to get the window area.
    context->is_top_window = false;
    return FALSE;
  }

  content_rect.IntersectWith(context->selected_window_rect);

  // If intersection is not empty, the selected window is not on top.
  if (!content_rect.is_empty()) {
    context->is_top_window = false;
    return FALSE;
  }
  // Otherwise, keep enumerating.
  return TRUE;
}

class CroppingWindowCapturerWin : public CroppingWindowCapturer {
 public:
  CroppingWindowCapturerWin(
      const DesktopCaptureOptions& options)
      : CroppingWindowCapturer(options) {}

 private:
  bool ShouldUseScreenCapturer() override;
  DesktopRect GetWindowRectInVirtualScreen() override;

  // The region from GetWindowRgn in the desktop coordinate if the region is
  // rectangular, or the rect from GetWindowRect if the region is not set.
  DesktopRect window_region_rect_;

  AeroChecker aero_checker_;
};

bool CroppingWindowCapturerWin::ShouldUseScreenCapturer() {
  if (!rtc::IsWindows8OrLater() && aero_checker_.IsAeroEnabled()) {
    return false;
  }

  const HWND selected = reinterpret_cast<HWND>(selected_window());
  // Check if the window is hidden or minimized.
  if (IsIconic(selected) || !IsWindowVisible(selected)) {
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
    region_rect.Translate(
        window_region_rect_.left(), window_region_rect_.top());
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
  TopWindowVerifierContext context(
      selected, reinterpret_cast<HWND>(excluded_window()), content_rect);
  const LPARAM enum_param = reinterpret_cast<LPARAM>(&context);
  EnumWindows(&TopWindowVerifier, enum_param);
  if (!context.is_top_window) {
    return false;
  }

  // If |selected| is not covered by other windows, check whether it is
  // covered by its own child windows. Note: EnumChildWindows() enumerates child
  // windows in all generations, but does not include any controls like buttons
  // or textboxes.
  EnumChildWindows(selected, &TopWindowVerifier, enum_param);
  return context.is_top_window;
}

DesktopRect CroppingWindowCapturerWin::GetWindowRectInVirtualScreen() {
  DesktopRect window_rect;
  HWND hwnd = reinterpret_cast<HWND>(selected_window());
  if (!GetCroppedWindowRect(hwnd, &window_rect, /* original_rect */ nullptr)) {
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
  return std::unique_ptr<DesktopCapturer>(
      new CroppingWindowCapturerWin(options));
}

}  // namespace webrtc
