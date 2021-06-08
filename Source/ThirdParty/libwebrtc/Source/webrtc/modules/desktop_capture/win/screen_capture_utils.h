/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_WIN_SCREEN_CAPTURE_UTILS_H_
#define MODULES_DESKTOP_CAPTURE_WIN_SCREEN_CAPTURE_UTILS_H_

#include <string>
#include <vector>

#include "modules/desktop_capture/desktop_capturer.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

// Output the list of active screens into |screens|. Returns true if succeeded,
// or false if it fails to enumerate the display devices. If the |device_names|
// is provided, it will be filled with the DISPLAY_DEVICE.DeviceName in UTF-8
// encoding. If this function returns true, consumers can always assume that
// |screens|[i] and |device_names|[i] indicate the same monitor on the system.
bool GetScreenList(DesktopCapturer::SourceList* screens,
                   std::vector<std::string>* device_names = nullptr);

// Returns true if |screen| is a valid screen. The screen device key is
// returned through |device_key| if the screen is valid. The device key can be
// used in GetScreenRect to verify the screen matches the previously obtained
// id.
bool IsScreenValid(DesktopCapturer::SourceId screen, std::wstring* device_key);

// Get the rect of the entire system in system coordinate system. I.e. the
// primary monitor always starts from (0, 0).
DesktopRect GetFullscreenRect();

// Get the rect of the screen identified by |screen|, relative to the primary
// display's top-left. If the screen device key does not match |device_key|, or
// the screen does not exist, or any error happens, an empty rect is returned.
RTC_EXPORT DesktopRect GetScreenRect(DesktopCapturer::SourceId screen,
                                     const std::wstring& device_key);

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_WIN_SCREEN_CAPTURE_UTILS_H_
