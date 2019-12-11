/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/win/screen_capture_utils.h"

#include <windows.h>

#include <string>
#include <vector>

#include "modules/desktop_capture/desktop_capturer.h"
#include "rtc_base/checks.h"
#include "rtc_base/string_utils.h"
#include "rtc_base/win32.h"

namespace webrtc {

bool GetScreenList(DesktopCapturer::SourceList* screens,
                   std::vector<std::string>* device_names /* = nullptr */) {
  RTC_DCHECK_EQ(screens->size(), 0U);
  if (device_names) {
    RTC_DCHECK_EQ(device_names->size(), 0U);
  }

  BOOL enum_result = TRUE;
  for (int device_index = 0;; ++device_index) {
    DISPLAY_DEVICEW device;
    device.cb = sizeof(device);
    enum_result = EnumDisplayDevicesW(NULL, device_index, &device, 0);

    // |enum_result| is 0 if we have enumerated all devices.
    if (!enum_result)
      break;

    // We only care about active displays.
    if (!(device.StateFlags & DISPLAY_DEVICE_ACTIVE))
      continue;

    screens->push_back({device_index, std::string()});
    if (device_names) {
      device_names->push_back(rtc::ToUtf8(device.DeviceName));
    }
  }
  return true;
}

bool IsScreenValid(DesktopCapturer::SourceId screen, std::wstring* device_key) {
  if (screen == kFullDesktopScreenId) {
    *device_key = L"";
    return true;
  }

  DISPLAY_DEVICEW device;
  device.cb = sizeof(device);
  BOOL enum_result = EnumDisplayDevicesW(NULL, screen, &device, 0);
  if (enum_result)
    *device_key = device.DeviceKey;

  return !!enum_result;
}

DesktopRect GetFullscreenRect() {
  return DesktopRect::MakeXYWH(GetSystemMetrics(SM_XVIRTUALSCREEN),
                               GetSystemMetrics(SM_YVIRTUALSCREEN),
                               GetSystemMetrics(SM_CXVIRTUALSCREEN),
                               GetSystemMetrics(SM_CYVIRTUALSCREEN));
}

DesktopRect GetScreenRect(DesktopCapturer::SourceId screen,
                          const std::wstring& device_key) {
  if (screen == kFullDesktopScreenId) {
    return GetFullscreenRect();
  }

  DISPLAY_DEVICEW device;
  device.cb = sizeof(device);
  BOOL result = EnumDisplayDevicesW(NULL, screen, &device, 0);
  if (!result)
    return DesktopRect();

  // Verifies the device index still maps to the same display device, to make
  // sure we are capturing the same device when devices are added or removed.
  // DeviceKey is documented as reserved, but it actually contains the registry
  // key for the device and is unique for each monitor, while DeviceID is not.
  if (device_key != device.DeviceKey)
    return DesktopRect();

  DEVMODEW device_mode;
  device_mode.dmSize = sizeof(device_mode);
  device_mode.dmDriverExtra = 0;
  result = EnumDisplaySettingsExW(device.DeviceName, ENUM_CURRENT_SETTINGS,
                                  &device_mode, 0);
  if (!result)
    return DesktopRect();

  return DesktopRect::MakeXYWH(
      device_mode.dmPosition.x, device_mode.dmPosition.y,
      device_mode.dmPelsWidth, device_mode.dmPelsHeight);
}

}  // namespace webrtc
