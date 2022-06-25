/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/win/display_configuration_monitor.h"

#include "modules/desktop_capture/win/screen_capture_utils.h"

namespace webrtc {

bool DisplayConfigurationMonitor::IsChanged() {
  DesktopRect rect = GetFullscreenRect();
  if (!initialized_) {
    initialized_ = true;
    rect_ = rect;
    return false;
  }

  if (rect.equals(rect_)) {
    return false;
  }

  rect_ = rect;
  return true;
}

void DisplayConfigurationMonitor::Reset() {
  initialized_ = false;
}

}  // namespace webrtc
