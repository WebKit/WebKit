/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_SCREEN_CAPTURER_WIN_DIRECTX_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_SCREEN_CAPTURER_WIN_DIRECTX_H_

#include <D3DCommon.h>

#include <memory>
#include <vector>

#include "webrtc/modules/desktop_capture/desktop_capturer.h"
#include "webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "webrtc/modules/desktop_capture/desktop_region.h"
#include "webrtc/modules/desktop_capture/resolution_change_detector.h"
#include "webrtc/modules/desktop_capture/screen_capture_frame_queue.h"
#include "webrtc/modules/desktop_capture/shared_desktop_frame.h"
#include "webrtc/modules/desktop_capture/win/dxgi_duplicator_controller.h"

namespace webrtc {

// ScreenCapturerWinDirectx captures 32bit RGBA using DirectX.
class ScreenCapturerWinDirectx : public DesktopCapturer {
 public:
  using D3dInfo = DxgiDuplicatorController::D3dInfo;

  // Whether the system supports DirectX based capturing.
  static bool IsSupported();

  // Returns a most recent D3dInfo composed by
  // DxgiDuplicatorController::Initialize() function. This function implicitly
  // calls DxgiDuplicatorController::Initialize() if it has not been
  // initialized. This function returns false and output parameter is kept
  // unchanged if DxgiDuplicatorController::Initialize() failed.
  // The D3dInfo may change based on hardware configuration even without
  // restarting the hardware and software. Refer to https://goo.gl/OOCppq. So
  // consumers should not cache the result returned by this function.
  static bool RetrieveD3dInfo(D3dInfo* info);

  explicit ScreenCapturerWinDirectx(const DesktopCaptureOptions& options);

  ~ScreenCapturerWinDirectx() override;

  void Start(Callback* callback) override;
  void SetSharedMemoryFactory(
      std::unique_ptr<SharedMemoryFactory> shared_memory_factory) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;

 private:
  // Returns desktop size of selected screen.
  DesktopSize SelectedDesktopSize() const;

  ScreenCaptureFrameQueue<SharedDesktopFrame> frames_;
  std::unique_ptr<SharedMemoryFactory> shared_memory_factory_;
  Callback* callback_ = nullptr;
  DxgiDuplicatorController::Context context_;
  SourceId current_screen_id_ = kFullDesktopScreenId;
  ResolutionChangeDetector resolution_change_detector_;

  RTC_DISALLOW_COPY_AND_ASSIGN(ScreenCapturerWinDirectx);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_SCREEN_CAPTURER_WIN_DIRECTX_H_
