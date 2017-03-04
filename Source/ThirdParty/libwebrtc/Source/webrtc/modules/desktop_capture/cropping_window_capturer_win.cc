/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/cropping_window_capturer.h"

#include "webrtc/base/win32.h"
#include "webrtc/modules/desktop_capture/win/scoped_gdi_object.h"
#include "webrtc/modules/desktop_capture/win/screen_capture_utils.h"
#include "webrtc/modules/desktop_capture/win/window_capture_utils.h"
#include "webrtc/system_wrappers/include/logging.h"

namespace webrtc {

namespace {

// Used to pass input/output data during the EnumWindow call for verifying if
// the selected window is on top.
struct TopWindowVerifierContext {
  TopWindowVerifierContext(HWND selected_window, HWND excluded_window)
      : selected_window(selected_window),
        excluded_window(excluded_window),
        is_top_window(false),
        selected_window_process_id(0) {}

  HWND selected_window;
  HWND excluded_window;
  bool is_top_window;
  DWORD selected_window_process_id;
  DesktopRect selected_window_rect;
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

  // Ignore descendant/owned windows since we want to capture them.
  // This check does not work for tooltips and context menus. Drop down menus
  // and popup windows are fine.
  if (GetAncestor(hwnd, GA_ROOTOWNER) == context->selected_window) {
    return TRUE;
  }

  // If |hwnd| has no title and belongs to the same process, assume it's a
  // tooltip or context menu from the selected window and ignore it.
  const size_t kTitleLength = 32;
  WCHAR window_title[kTitleLength];
  GetWindowText(hwnd, window_title, kTitleLength);
  if (wcsnlen_s(window_title, kTitleLength) == 0) {
    DWORD enumerated_process;
    GetWindowThreadProcessId(hwnd, &enumerated_process);
    if (!context->selected_window_process_id) {
      GetWindowThreadProcessId(context->selected_window,
                               &context->selected_window_process_id);
    }
    if (context->selected_window_process_id == enumerated_process) {
      return TRUE;
    }
  }

  // Check if the enumerated window intersects with the selected window.
  RECT enumerated_rect;
  if (!GetWindowRect(hwnd, &enumerated_rect)) {
    // Bail out if failed to get the window area.
    context->is_top_window = false;
    return FALSE;
  }

  DesktopRect intersect_rect = context->selected_window_rect;
  DesktopRect enumerated_desktop_rect =
      DesktopRect::MakeLTRB(enumerated_rect.left,
                            enumerated_rect.top,
                            enumerated_rect.right,
                            enumerated_rect.bottom);
  intersect_rect.IntersectWith(enumerated_desktop_rect);

  // If intersection is not empty, the selected window is not on top.
  if (!intersect_rect.is_empty()) {
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
  if (!rtc::IsWindows8OrLater() && aero_checker_.IsAeroEnabled())
    return false;

  // Check if the window is a translucent layered window.
  HWND selected = reinterpret_cast<HWND>(selected_window());
  LONG window_ex_style = GetWindowLong(selected, GWL_EXSTYLE);
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
    if ((flags & LWA_COLORKEY) || ((flags & LWA_ALPHA) && (alpha < 255)))
      return false;
  }

  TopWindowVerifierContext context(
      selected, reinterpret_cast<HWND>(excluded_window()));

  RECT selected_window_rect;
  if (!GetWindowRect(selected, &selected_window_rect)) {
    return false;
  }
  context.selected_window_rect = DesktopRect::MakeLTRB(
      selected_window_rect.left,
      selected_window_rect.top,
      selected_window_rect.right,
      selected_window_rect.bottom);

  // Get the window region and check if it is rectangular.
  win::ScopedGDIObject<HRGN, win::DeleteObjectTraits<HRGN> >
      scoped_hrgn(CreateRectRgn(0, 0, 0, 0));
  int region_type = GetWindowRgn(selected, scoped_hrgn.Get());

  // Do not use the screen capturer if the region is empty or not rectangular.
  if (region_type == COMPLEXREGION || region_type == NULLREGION)
    return false;

  if (region_type == SIMPLEREGION) {
    RECT region_rect;
    GetRgnBox(scoped_hrgn.Get(), &region_rect);
    DesktopRect rgn_rect =
        DesktopRect::MakeLTRB(region_rect.left,
                              region_rect.top,
                              region_rect.right,
                              region_rect.bottom);
    rgn_rect.Translate(context.selected_window_rect.left(),
                       context.selected_window_rect.top());
    context.selected_window_rect.IntersectWith(rgn_rect);
  }
  window_region_rect_ = context.selected_window_rect;

  // Check if the window is occluded by any other window, excluding the child
  // windows, context menus, and |excluded_window_|.
  EnumWindows(&TopWindowVerifier, reinterpret_cast<LPARAM>(&context));
  return context.is_top_window;
}

DesktopRect CroppingWindowCapturerWin::GetWindowRectInVirtualScreen() {
  DesktopRect original_rect;
  DesktopRect window_rect;
  HWND hwnd = reinterpret_cast<HWND>(selected_window());
  if (!GetCroppedWindowRect(hwnd, &window_rect, &original_rect)) {
    LOG(LS_WARNING) << "Failed to get window info: " << GetLastError();
    return window_rect;
  }
  window_rect.IntersectWith(window_region_rect_);

  // Convert |window_rect| to be relative to the top-left of the virtual screen.
  DesktopRect screen_rect(GetScreenRect(kFullDesktopScreenId, L""));
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
