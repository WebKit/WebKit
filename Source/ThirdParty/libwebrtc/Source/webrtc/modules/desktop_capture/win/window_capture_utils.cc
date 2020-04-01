/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/win/window_capture_utils.h"

// Just for the DWMWINDOWATTRIBUTE enums (DWMWA_CLOAKED).
#include <dwmapi.h>

#include "modules/desktop_capture/win/scoped_gdi_object.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/win32.h"

namespace webrtc {

// Prefix used to match the window class for Chrome windows.
const wchar_t kChromeWindowClassPrefix[] = L"Chrome_WidgetWin_";

// The hiddgen taskbar will leave a 2 pixel margin on the screen.
const int kHiddenTaskbarMarginOnScreen = 2;

bool GetWindowRect(HWND window, DesktopRect* result) {
  RECT rect;
  if (!::GetWindowRect(window, &rect)) {
    return false;
  }
  *result = DesktopRect::MakeLTRB(rect.left, rect.top, rect.right, rect.bottom);
  return true;
}

bool GetCroppedWindowRect(HWND window,
                          bool avoid_cropping_border,
                          DesktopRect* cropped_rect,
                          DesktopRect* original_rect) {
  DesktopRect window_rect;
  if (!GetWindowRect(window, &window_rect)) {
    return false;
  }

  if (original_rect) {
    *original_rect = window_rect;
  }
  *cropped_rect = window_rect;

  bool is_maximized = false;
  if (!IsWindowMaximized(window, &is_maximized)) {
    return false;
  }

  // As of Windows8, transparent resize borders are added by the OS at
  // left/bottom/right sides of a resizeable window. If the cropped window
  // doesn't remove these borders, the background will be exposed a bit.
  if (rtc::IsWindows8OrLater() || is_maximized) {
    // Only apply this cropping to windows with a resize border (otherwise,
    // it'd clip the edges of captured pop-up windows without this border).
    LONG style = GetWindowLong(window, GWL_STYLE);
    if (style & WS_THICKFRAME || style & DS_MODALFRAME) {
      int width = GetSystemMetrics(SM_CXSIZEFRAME);
      int bottom_height = GetSystemMetrics(SM_CYSIZEFRAME);
      const int visible_border_height = GetSystemMetrics(SM_CYBORDER);
      int top_height = visible_border_height;

      // If requested, avoid cropping the visible window border. This is used
      // for pop-up windows to include their border, but not for the outermost
      // window (where a partially-transparent border may expose the
      // background a bit).
      if (avoid_cropping_border) {
        width = std::max(0, width - GetSystemMetrics(SM_CXBORDER));
        bottom_height = std::max(0, bottom_height - visible_border_height);
        top_height = 0;
      }
      cropped_rect->Extend(-width, -top_height, -width, -bottom_height);
    }
  }

  return true;
}

bool GetWindowContentRect(HWND window, DesktopRect* result) {
  if (!GetWindowRect(window, result)) {
    return false;
  }

  RECT rect;
  if (!::GetClientRect(window, &rect)) {
    return false;
  }

  const int width = rect.right - rect.left;
  // The GetClientRect() is not expected to return a larger area than
  // GetWindowRect().
  if (width > 0 && width < result->width()) {
    // - GetClientRect() always set the left / top of RECT to 0. So we need to
    //   estimate the border width from GetClientRect() and GetWindowRect().
    // - Border width of a window varies according to the window type.
    // - GetClientRect() excludes the title bar, which should be considered as
    //   part of the content and included in the captured frame. So we always
    //   estimate the border width according to the window width.
    // - We assume a window has same border width in each side.
    // So we shrink half of the width difference from all four sides.
    const int shrink = ((width - result->width()) / 2);
    // When |shrink| is negative, DesktopRect::Extend() shrinks itself.
    result->Extend(shrink, 0, shrink, 0);
    // Usually this should not happen, just in case we have received a strange
    // window, which has only left and right borders.
    if (result->height() > shrink * 2) {
      result->Extend(0, shrink, 0, shrink);
    }
    RTC_DCHECK(!result->is_empty());
  }

  return true;
}

int GetWindowRegionTypeWithBoundary(HWND window, DesktopRect* result) {
  win::ScopedGDIObject<HRGN, win::DeleteObjectTraits<HRGN>> scoped_hrgn(
      CreateRectRgn(0, 0, 0, 0));
  const int region_type = GetWindowRgn(window, scoped_hrgn.Get());

  if (region_type == SIMPLEREGION) {
    RECT rect;
    GetRgnBox(scoped_hrgn.Get(), &rect);
    *result =
        DesktopRect::MakeLTRB(rect.left, rect.top, rect.right, rect.bottom);
  }
  return region_type;
}

bool GetDcSize(HDC hdc, DesktopSize* size) {
  win::ScopedGDIObject<HGDIOBJ, win::DeleteObjectTraits<HGDIOBJ>> scoped_hgdi(
      GetCurrentObject(hdc, OBJ_BITMAP));
  BITMAP bitmap;
  memset(&bitmap, 0, sizeof(BITMAP));
  if (GetObject(scoped_hgdi.Get(), sizeof(BITMAP), &bitmap) == 0) {
    return false;
  }
  size->set(bitmap.bmWidth, bitmap.bmHeight);
  return true;
}

bool IsWindowMaximized(HWND window, bool* result) {
  WINDOWPLACEMENT placement;
  memset(&placement, 0, sizeof(WINDOWPLACEMENT));
  placement.length = sizeof(WINDOWPLACEMENT);
  if (!::GetWindowPlacement(window, &placement)) {
    return false;
  }

  *result = (placement.showCmd == SW_SHOWMAXIMIZED);
  return true;
}

// WindowCaptureHelperWin implementation.
WindowCaptureHelperWin::WindowCaptureHelperWin() {
  // Try to load dwmapi.dll dynamically since it is not available on XP.
  dwmapi_library_ = LoadLibraryW(L"dwmapi.dll");
  if (dwmapi_library_) {
    func_ = reinterpret_cast<DwmIsCompositionEnabledFunc>(
        GetProcAddress(dwmapi_library_, "DwmIsCompositionEnabled"));
    dwm_get_window_attribute_func_ =
        reinterpret_cast<DwmGetWindowAttributeFunc>(
            GetProcAddress(dwmapi_library_, "DwmGetWindowAttribute"));
  }

  if (rtc::IsWindows10OrLater()) {
    if (FAILED(::CoCreateInstance(__uuidof(VirtualDesktopManager), nullptr,
                                  CLSCTX_ALL,
                                  IID_PPV_ARGS(&virtual_desktop_manager_)))) {
      RTC_LOG(LS_WARNING) << "Fail to create instance of VirtualDesktopManager";
    }
  }
}

WindowCaptureHelperWin::~WindowCaptureHelperWin() {
  if (dwmapi_library_) {
    FreeLibrary(dwmapi_library_);
  }
}

bool WindowCaptureHelperWin::IsAeroEnabled() {
  BOOL result = FALSE;
  if (func_) {
    func_(&result);
  }
  return result != FALSE;
}

// This is just a best guess of a notification window. Chrome uses the Windows
// native framework for showing notifications. So far what we know about such a
// window includes: no title, class name with prefix "Chrome_WidgetWin_" and
// with certain extended styles.
bool WindowCaptureHelperWin::IsWindowChromeNotification(HWND hwnd) {
  const size_t kTitleLength = 32;
  WCHAR window_title[kTitleLength];
  GetWindowTextW(hwnd, window_title, kTitleLength);
  if (wcsnlen_s(window_title, kTitleLength) != 0) {
    return false;
  }

  const size_t kClassLength = 256;
  WCHAR class_name[kClassLength];
  const int class_name_length = GetClassNameW(hwnd, class_name, kClassLength);
  if (class_name_length < 1 ||
      wcsncmp(class_name, kChromeWindowClassPrefix,
              wcsnlen_s(kChromeWindowClassPrefix, kClassLength)) != 0) {
    return false;
  }

  const LONG exstyle = GetWindowLong(hwnd, GWL_EXSTYLE);
  if ((exstyle & WS_EX_NOACTIVATE) && (exstyle & WS_EX_TOOLWINDOW) &&
      (exstyle & WS_EX_TOPMOST)) {
    return true;
  }

  return false;
}

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
bool WindowCaptureHelperWin::AreWindowsOverlapping(
    HWND hwnd,
    HWND selected_hwnd,
    const DesktopRect& selected_window_rect) {
  DesktopRect content_rect;
  if (!GetWindowContentRect(hwnd, &content_rect)) {
    // Bail out if failed to get the window area.
    return true;
  }
  content_rect.IntersectWith(selected_window_rect);

  if (content_rect.is_empty()) {
    return false;
  }

  // When the taskbar is automatically hidden, it will leave a 2 pixel margin on
  // the screen which will overlap the maximized selected window that will use
  // up the full screen area. Since there is no solid way to identify a hidden
  // taskbar window, we have to make an exemption here if the overlapping is
  // 2 x screen_width/height to a maximized window.
  bool is_maximized = false;
  IsWindowMaximized(selected_hwnd, &is_maximized);
  bool overlaps_hidden_horizontal_taskbar =
      selected_window_rect.width() == content_rect.width() &&
      content_rect.height() == kHiddenTaskbarMarginOnScreen;
  bool overlaps_hidden_vertical_taskbar =
      selected_window_rect.height() == content_rect.height() &&
      content_rect.width() == kHiddenTaskbarMarginOnScreen;
  if (is_maximized && (overlaps_hidden_horizontal_taskbar ||
                       overlaps_hidden_vertical_taskbar)) {
    return false;
  }

  return true;
}

bool WindowCaptureHelperWin::IsWindowOnCurrentDesktop(HWND hwnd) {
  // Make sure the window is on the current virtual desktop.
  if (virtual_desktop_manager_) {
    BOOL on_current_desktop;
    if (SUCCEEDED(virtual_desktop_manager_->IsWindowOnCurrentVirtualDesktop(
            hwnd, &on_current_desktop)) &&
        !on_current_desktop) {
      return false;
    }
  }
  return true;
}

bool WindowCaptureHelperWin::IsWindowVisibleOnCurrentDesktop(HWND hwnd) {
  return !::IsIconic(hwnd) && ::IsWindowVisible(hwnd) &&
         IsWindowOnCurrentDesktop(hwnd) && !IsWindowCloaked(hwnd);
}

// A cloaked window is composited but not visible to the user.
// Example: Cortana or the Action Center when collapsed.
bool WindowCaptureHelperWin::IsWindowCloaked(HWND hwnd) {
  if (!dwm_get_window_attribute_func_) {
    // Does not apply.
    return false;
  }

  int res = 0;
  if (dwm_get_window_attribute_func_(hwnd, DWMWA_CLOAKED, &res, sizeof(res)) !=
      S_OK) {
    // Cannot tell so assume not cloacked for backward compatibility.
    return false;
  }

  return res != 0;
}

}  // namespace webrtc
