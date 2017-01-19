/* Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_MOCK_DESKTOP_CAPTURER_CALLBACK_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_MOCK_DESKTOP_CAPTURER_CALLBACK_H_

#include "webrtc/modules/desktop_capture/desktop_capturer.h"

#include <memory>

#include "webrtc/test/gmock.h"

namespace webrtc {

class MockDesktopCapturerCallback : public DesktopCapturer::Callback {
 public:
  MockDesktopCapturerCallback();
  ~MockDesktopCapturerCallback() override;

  MOCK_METHOD2(OnCaptureResultPtr,
               void(DesktopCapturer::Result result,
                    std::unique_ptr<DesktopFrame>* frame));
  void OnCaptureResult(DesktopCapturer::Result result,
                       std::unique_ptr<DesktopFrame> frame) final;

 private:
  RTC_DISALLOW_COPY_AND_ASSIGN(MockDesktopCapturerCallback);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_MOCK_DESKTOP_CAPTURER_CALLBACK_H_
