/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "modules/desktop_capture/mac/screen_capturer_mac.h"
#include "modules/desktop_capture/mac/screen_capturer_sck.h"
#include "rtc_base/logging.h"

namespace webrtc {

// static
std::unique_ptr<DesktopCapturer> DesktopCapturer::CreateRawScreenCapturer(
    const DesktopCaptureOptions& options) {
  if (!options.configuration_monitor()) {
    return nullptr;
  }

  if (options.allow_sck_capturer()) {
    // This will return nullptr on systems that don't support ScreenCaptureKit.
    std::unique_ptr<DesktopCapturer> sck_capturer =
        CreateScreenCapturerSck(options);
    if (sck_capturer) {
      RTC_LOG(LS_INFO)
          << "video capture: DesktopCapturer::CreateRawScreenCapturer creates "
             "DesktopCapturer of type ScreenCapturerSck";
      return sck_capturer;
    }
  }

  RTC_LOG(LS_INFO)
      << "video capture: DesktopCapturer::CreateRawScreenCapturer creates "
         "DesktopCapturer of type ScreenCapturerMac";
  auto capturer =
      std::make_unique<ScreenCapturerMac>(options.configuration_monitor(),
                                          options.detect_updated_region(),
                                          options.allow_iosurface());
  if (!capturer->Init()) {
    return nullptr;
  }

  return capturer;
}

}  // namespace webrtc
