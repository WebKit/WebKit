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

#include "modules/desktop_capture/win/scoped_gdi_object.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/win32.h"

namespace webrtc {

// Prefix used to match the window class for Chrome windows.
const wchar_t kChromeWindowClassPrefix[] = L"Chrome_WidgetWin_";

bool GetWindowRect(HWND window, DesktopRect* result) {
  RECT rect;
  if (!::GetWindowRect(window, &rect)) {
    return false;
  }
  *result = DesktopRect::MakeLTRB(rect.left, rect.top, rect.right, rect.bottom);
  return true;
}

bool GetCroppedWindowRect(HWND window,
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

  // After Windows8, transparent borders will be added by OS at
  // left/bottom/right sides of a window. If the cropped window
  // doesn't remove these borders, the background will be exposed a bit.
  if (rtc::IsWindows8OrLater() || is_maximized) {
    const int width = GetSystemMetrics(SM_CXSIZEFRAME);
    const int height = GetSystemMetrics(SM_CYSIZEFRAME);
    cropped_rect->Extend(-width, 0, -width, -height);
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
WindowCaptureHelperWin::WindowCaptureHelperWin()
    : dwmapi_library_(nullptr),
      func_(nullptr),
      virtual_desktop_manager_(nullptr) {
  // Try to load dwmapi.dll dynamically since it is not available on XP.
  dwmapi_library_ = LoadLibrary(L"dwmapi.dll");
  if (dwmapi_library_) {
    func_ = reinterpret_cast<DwmIsCompositionEnabledFunc>(
        GetProcAddress(dwmapi_library_, "DwmIsCompositionEnabled"));
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
  GetWindowText(hwnd, window_title, kTitleLength);
  if (wcsnlen_s(window_title, kTitleLength) != 0) {
    return false;
  }

  const size_t kClassLength = 256;
  WCHAR class_name[kClassLength];
  const int class_name_length = GetClassName(hwnd, class_name, kClassLength);
  RTC_DCHECK(class_name_length)
      << "Error retrieving the application's class name";
  if (wcsncmp(class_name, kChromeWindowClassPrefix,
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
         IsWindowOnCurrentDesktop(hwnd);
}

}  // namespace webrtc
