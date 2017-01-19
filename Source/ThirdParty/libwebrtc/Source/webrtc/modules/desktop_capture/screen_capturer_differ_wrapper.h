/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_SCREEN_CAPTURER_DIFFER_WRAPPER_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_SCREEN_CAPTURER_DIFFER_WRAPPER_H_

#include <memory>

#include "webrtc/modules/desktop_capture/screen_capturer.h"
#include "webrtc/modules/desktop_capture/shared_desktop_frame.h"

namespace webrtc {

// ScreenCapturer wrapper that calculates updated_region() by comparing frames
// content. This class always expects the underlying ScreenCapturer
// implementation returns a superset of updated regions in DestkopFrame. If a
// ScreenCapturer implementation does not know the updated region, it should
// set updated_region() to full frame.
//
// This class marks entire frame as updated if the frame size or frame stride
// has been changed.
class ScreenCapturerDifferWrapper : public ScreenCapturer,
                                    public DesktopCapturer::Callback {
 public:
  // Creates a ScreenCapturerDifferWrapper with a ScreenCapturer implementation,
  // and takes its ownership.
  explicit ScreenCapturerDifferWrapper(
      std::unique_ptr<ScreenCapturer> base_capturer);
  ~ScreenCapturerDifferWrapper() override;

  // ScreenCapturer interface.
  void Start(DesktopCapturer::Callback* callback) override;
  void SetSharedMemoryFactory(
      std::unique_ptr<SharedMemoryFactory> shared_memory_factory) override;
  void CaptureFrame() override;
  bool GetScreenList(ScreenList* screens) override;
  bool SelectScreen(ScreenId id) override;

 private:
  // DesktopCapturer::Callback interface.
  void OnCaptureResult(Result result,
                       std::unique_ptr<DesktopFrame> frame) override;

  const std::unique_ptr<ScreenCapturer> base_capturer_;
  DesktopCapturer::Callback* callback_;
  std::unique_ptr<SharedDesktopFrame> last_frame_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_SCREEN_CAPTURER_DIFFER_WRAPPER_H_
