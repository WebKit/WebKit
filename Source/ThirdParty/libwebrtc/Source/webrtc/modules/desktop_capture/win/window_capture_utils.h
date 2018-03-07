/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <windows.h>

#include "modules/desktop_capture/desktop_geometry.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

// Outputs the window rect. The returned DesktopRect is in system coordinates,
// i.e. the primary monitor on the system always starts from (0, 0). This
// function returns false if native APIs fail.
bool GetWindowRect(HWND window, DesktopRect* result);

// Outputs the window rect, with the left/right/bottom frame border cropped if
// the window is maximized. |cropped_rect| is the cropped rect relative to the
// desktop. |original_rect| is the original rect returned from GetWindowRect.
// Returns true if all API calls succeeded. The returned DesktopRect is in
// system coordinates, i.e. the primary monitor on the system always starts from
// (0, 0). |original_rect| can be nullptr.
//
// TODO(zijiehe): Move this function to CroppingWindowCapturerWin after it has
// been removed from MouseCursorMonitorWin.
// This function should only be used by CroppingWindowCapturerWin. Instead a
// DesktopRect CropWindowRect(const DesktopRect& rect)
// should be added as a utility function to help CroppingWindowCapturerWin and
// WindowCapturerWin to crop out the borders or shadow according to their
// scenarios. But this function is too generic and easy to be misused.
bool GetCroppedWindowRect(HWND window,
                          DesktopRect* cropped_rect,
                          DesktopRect* original_rect);

// Retrieves the rectangle of the content area of |window|. Usually it contains
// title bar and window client area, but borders or shadow are excluded. The
// returned DesktopRect is in system coordinates, i.e. the primary monitor on
// the system always starts from (0, 0). This function returns false if native
// APIs fail.
bool GetWindowContentRect(HWND window, DesktopRect* result);

// Returns the region type of the |window| and fill |rect| with the region of
// |window| if region type is SIMPLEREGION.
int GetWindowRegionTypeWithBoundary(HWND window, DesktopRect* result);

// Retrieves the size of the |hdc|. This function returns false if native APIs
// fail.
bool GetDcSize(HDC hdc, DesktopSize* size);

// Retrieves whether the |window| is maximized and stores in |result|. This
// function returns false if native APIs fail.
bool IsWindowMaximized(HWND window, bool* result);

typedef HRESULT (WINAPI *DwmIsCompositionEnabledFunc)(BOOL* enabled);
class AeroChecker {
 public:
  AeroChecker();
  ~AeroChecker();

  bool IsAeroEnabled();

 private:
  HMODULE dwmapi_library_;
  DwmIsCompositionEnabledFunc func_;

  RTC_DISALLOW_COPY_AND_ASSIGN(AeroChecker);
};

}  // namespace webrtc
