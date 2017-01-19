/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/desktop_capture_options.h"

namespace webrtc {

DesktopCaptureOptions::DesktopCaptureOptions() {}
DesktopCaptureOptions::DesktopCaptureOptions(
    const DesktopCaptureOptions& options) = default;
DesktopCaptureOptions::DesktopCaptureOptions(DesktopCaptureOptions&& options) =
    default;
DesktopCaptureOptions::~DesktopCaptureOptions() {}

DesktopCaptureOptions& DesktopCaptureOptions::operator=(
    const DesktopCaptureOptions& options) = default;
DesktopCaptureOptions& DesktopCaptureOptions::operator=(
    DesktopCaptureOptions&& options) = default;

// static
DesktopCaptureOptions DesktopCaptureOptions::CreateDefault() {
  DesktopCaptureOptions result;
#if defined(USE_X11)
  result.set_x_display(SharedXDisplay::CreateDefault());
#endif
#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
  result.set_configuration_monitor(new DesktopConfigurationMonitor());
  result.set_full_screen_chrome_window_detector(
      new FullScreenChromeWindowDetector());
#endif
  return result;
}

}  // namespace webrtc
