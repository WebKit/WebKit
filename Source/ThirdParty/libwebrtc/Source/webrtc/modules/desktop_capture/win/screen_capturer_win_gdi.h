/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_SCREEN_CAPTURER_WIN_GDI_H_
#define WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_SCREEN_CAPTURER_WIN_GDI_H_

#include <memory>

#include <windows.h>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/desktop_capture/desktop_capturer.h"
#include "webrtc/modules/desktop_capture/screen_capture_frame_queue.h"
#include "webrtc/modules/desktop_capture/shared_desktop_frame.h"
#include "webrtc/modules/desktop_capture/win/scoped_thread_desktop.h"

namespace webrtc {

// ScreenCapturerWinGdi captures 32bit RGB using GDI.
//
// ScreenCapturerWinGdi is double-buffered as required by ScreenCapturer.
// This class does not detect DesktopFrame::updated_region(), the field is
// always set to the entire frame rectangle. ScreenCapturerDifferWrapper should
// be used if that functionality is necessary.
class ScreenCapturerWinGdi : public DesktopCapturer {
 public:
  explicit ScreenCapturerWinGdi(const DesktopCaptureOptions& options);
  ~ScreenCapturerWinGdi() override;

  // Overridden from ScreenCapturer:
  void Start(Callback* callback) override;
  void SetSharedMemoryFactory(
      std::unique_ptr<SharedMemoryFactory> shared_memory_factory) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;

 private:
  typedef HRESULT (WINAPI * DwmEnableCompositionFunc)(UINT);

  // Make sure that the device contexts match the screen configuration.
  void PrepareCaptureResources();

  // Captures the current screen contents into the current buffer. Returns true
  // if succeeded.
  bool CaptureImage();

  // Capture the current cursor shape.
  void CaptureCursor();

  Callback* callback_ = nullptr;
  std::unique_ptr<SharedMemoryFactory> shared_memory_factory_;
  SourceId current_screen_id_ = kFullDesktopScreenId;
  std::wstring current_device_key_;

  ScopedThreadDesktop desktop_;

  // GDI resources used for screen capture.
  HDC desktop_dc_ = NULL;
  HDC memory_dc_ = NULL;

  // Queue of the frames buffers.
  ScreenCaptureFrameQueue<SharedDesktopFrame> queue_;

  // Rectangle describing the bounds of the desktop device context, relative to
  // the primary display's top-left.
  DesktopRect desktop_dc_rect_;

  HMODULE dwmapi_library_ = NULL;
  DwmEnableCompositionFunc composition_func_ = nullptr;

  RTC_DISALLOW_COPY_AND_ASSIGN(ScreenCapturerWinGdi);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_DESKTOP_CAPTURE_WIN_SCREEN_CAPTURER_WIN_GDI_H_
