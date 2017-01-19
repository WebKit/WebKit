/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/win/screen_capture_utils.h"

#include <windows.h>

#include "webrtc/base/checks.h"

namespace webrtc {

bool GetScreenList(DesktopCapturer::SourceList* screens) {
  RTC_DCHECK(screens->size() == 0);

  BOOL enum_result = TRUE;
  for (int device_index = 0;; ++device_index) {
    DISPLAY_DEVICE device;
    device.cb = sizeof(device);
    enum_result = EnumDisplayDevices(NULL, device_index, &device, 0);

    // |enum_result| is 0 if we have enumerated all devices.
    if (!enum_result)
      break;

    // We only care about active displays.
    if (!(device.StateFlags & DISPLAY_DEVICE_ACTIVE))
      continue;

    screens->push_back({device_index, std::string()});
  }
  return true;
}

bool IsScreenValid(DesktopCapturer::SourceId screen, std::wstring* device_key) {
  if (screen == kFullDesktopScreenId) {
    *device_key = L"";
    return true;
  }

  DISPLAY_DEVICE device;
  device.cb = sizeof(device);
  BOOL enum_result = EnumDisplayDevices(NULL, screen, &device, 0);
  if (enum_result)
    *device_key = device.DeviceKey;

  return !!enum_result;
}

DesktopRect GetScreenRect(DesktopCapturer::SourceId screen,
                          const std::wstring& device_key) {
  if (screen == kFullDesktopScreenId) {
    return DesktopRect::MakeXYWH(GetSystemMetrics(SM_XVIRTUALSCREEN),
                                 GetSystemMetrics(SM_YVIRTUALSCREEN),
                                 GetSystemMetrics(SM_CXVIRTUALSCREEN),
                                 GetSystemMetrics(SM_CYVIRTUALSCREEN));
  }

  DISPLAY_DEVICE device;
  device.cb = sizeof(device);
  BOOL result = EnumDisplayDevices(NULL, screen, &device, 0);
  if (!result)
    return DesktopRect();

  // Verifies the device index still maps to the same display device, to make
  // sure we are capturing the same device when devices are added or removed.
  // DeviceKey is documented as reserved, but it actually contains the registry
  // key for the device and is unique for each monitor, while DeviceID is not.
  if (device_key != device.DeviceKey)
    return DesktopRect();

  DEVMODE device_mode;
  device_mode.dmSize = sizeof(device_mode);
  device_mode.dmDriverExtra = 0;
  result = EnumDisplaySettingsEx(
      device.DeviceName, ENUM_CURRENT_SETTINGS, &device_mode, 0);
  if (!result)
    return DesktopRect();

  return DesktopRect::MakeXYWH(device_mode.dmPosition.x,
                               device_mode.dmPosition.y,
                               device_mode.dmPelsWidth,
                               device_mode.dmPelsHeight);
}

}  // namespace webrtc
