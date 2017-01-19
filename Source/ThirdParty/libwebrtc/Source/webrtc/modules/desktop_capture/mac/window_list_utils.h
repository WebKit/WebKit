/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_WINDOW_LIST_UTILS_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_WINDOW_LIST_UTILS_H_

#include <ApplicationServices/ApplicationServices.h>

#include "webrtc/modules/desktop_capture/desktop_capturer.h"
#include "webrtc/modules/desktop_capture/mac/desktop_configuration.h"

namespace webrtc {

// Another helper function to get the on-screen windows.
bool GetWindowList(DesktopCapturer::SourceList* windows, bool ignore_minimized);

// Returns true if the window is occupying a full screen.
bool IsWindowFullScreen(const MacDesktopConfiguration& desktop_config,
                        CFDictionaryRef window);

// Returns true if the window is minimized.
bool IsWindowMinimized(CGWindowID id);


}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_WINDOW_LIST_UTILS_H_

