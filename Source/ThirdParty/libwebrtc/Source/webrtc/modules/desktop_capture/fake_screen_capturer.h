/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_FAKE_SCREEN_CAPTURER_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_FAKE_SCREEN_CAPTURER_H_

#include "webrtc/modules/desktop_capture/fake_desktop_capturer.h"
#include "webrtc/modules/desktop_capture/screen_capturer.h"

namespace webrtc {

class FakeScreenCapturer : public FakeDesktopCapturer<ScreenCapturer> {
 public:
  FakeScreenCapturer();
  ~FakeScreenCapturer() override;

  // ScreenCapturer interface.
  bool GetScreenList(ScreenList* list) override;
  bool SelectScreen(ScreenId id) override;

 private:
  // A random ScreenId.
  const ScreenId id_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_FAKE_SCREEN_CAPTURER_H_
