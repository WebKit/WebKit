/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <utility>

#include "webrtc/modules/desktop_capture/desktop_capturer.h"
#include "webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "webrtc/modules/desktop_capture/fallback_desktop_capturer_wrapper.h"
#include "webrtc/modules/desktop_capture/win/screen_capturer_win_directx.h"
#include "webrtc/modules/desktop_capture/win/screen_capturer_win_gdi.h"
#include "webrtc/modules/desktop_capture/win/screen_capturer_win_magnifier.h"

namespace webrtc {

// static
std::unique_ptr<DesktopCapturer> DesktopCapturer::CreateRawScreenCapturer(
    const DesktopCaptureOptions& options) {
  std::unique_ptr<DesktopCapturer> capturer;
  if (options.allow_directx_capturer() &&
      ScreenCapturerWinDirectx::IsSupported()) {
    capturer.reset(new ScreenCapturerWinDirectx(options));
  } else {
    capturer.reset(new ScreenCapturerWinGdi(options));
  }

  if (options.allow_use_magnification_api()) {
    // ScreenCapturerWinMagnifier cannot work on Windows XP or earlier, as well
    // as 64-bit only Windows, and it may randomly crash on multi-screen
    // systems. So we may need to fallback to use original capturer.
    capturer.reset(new FallbackDesktopCapturerWrapper(
        std::unique_ptr<DesktopCapturer>(new ScreenCapturerWinMagnifier()),
        std::move(capturer)));
  }

  return capturer;
}

}  // namespace webrtc
