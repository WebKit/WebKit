/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_MAC_FULL_SCREEN_CHROME_WINDOW_DETECTOR_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_MAC_FULL_SCREEN_CHROME_WINDOW_DETECTOR_H_

#include <ApplicationServices/ApplicationServices.h>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/desktop_capture/desktop_capturer.h"
#include "webrtc/system_wrappers/include/atomic32.h"

namespace webrtc {

// This is a work around for the Chrome tab full-screen behavior: Chrome
// creates a new window in full-screen mode to show a tab full-screen and
// minimizes the old window. To continue capturing in this case, we try to
// find the new full-screen window using these criteria:
// 0. The original shared window is minimized.
// 1. The original shared window's owner application name is "Google Chrome".
// 2. The original window and the new window have the same title and owner
// pid.
// 3. The new window is full-screen.
// 4. The new window didn't exist at least 500 millisecond ago.

class FullScreenChromeWindowDetector {
 public:
  FullScreenChromeWindowDetector();

  void AddRef() { ++ref_count_; }
  void Release() {
    if (--ref_count_ == 0)
      delete this;
  }

  // Returns the full-screen window in place of the original window if all the
  // criteria are met, or kCGNullWindowID if no such window found.
  CGWindowID FindFullScreenWindow(CGWindowID original_window);

  // The caller should call this function periodically, no less than twice per
  // second.
  void UpdateWindowListIfNeeded(CGWindowID original_window);

 private:
  ~FullScreenChromeWindowDetector();

  Atomic32 ref_count_;

  // We cache the last two results of the window list, so
  // |previous_window_list_| is taken at least 500ms before the next Capture()
  // call. If we only save the last result, we may get false positive (i.e.
  // full-screen window exists in the list) if Capture() is called too soon.
  DesktopCapturer::SourceList current_window_list_;
  DesktopCapturer::SourceList previous_window_list_;
  int64_t last_update_time_ns_;

  RTC_DISALLOW_COPY_AND_ASSIGN(FullScreenChromeWindowDetector);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_MAC_FULL_SCREEN_CHROME_WINDOW_DETECTOR_H_
