/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_WIN_WGC_CAPTURER_WIN_H_
#define MODULES_DESKTOP_CAPTURE_WIN_WGC_CAPTURER_WIN_H_

#include <d3d11.h>
#include <wrl/client.h>

#include <map>
#include <memory>

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/win/screen_capture_utils.h"
#include "modules/desktop_capture/win/wgc_capture_session.h"
#include "modules/desktop_capture/win/wgc_capture_source.h"
#include "modules/desktop_capture/win/window_capture_utils.h"

namespace webrtc {

// WgcCapturerWin is initialized with an implementation of this base class,
// which it uses to find capturable sources of a particular type. This way,
// WgcCapturerWin can remain source-agnostic.
class SourceEnumerator {
 public:
  virtual ~SourceEnumerator() = default;

  virtual bool FindAllSources(DesktopCapturer::SourceList* sources) = 0;
};

class WindowEnumerator final : public SourceEnumerator {
 public:
  explicit WindowEnumerator(bool enumerate_current_process_windows)
      : enumerate_current_process_windows_(enumerate_current_process_windows) {}

  WindowEnumerator(const WindowEnumerator&) = delete;
  WindowEnumerator& operator=(const WindowEnumerator&) = delete;

  ~WindowEnumerator() override = default;

  bool FindAllSources(DesktopCapturer::SourceList* sources) override {
    // WGC fails to capture windows with the WS_EX_TOOLWINDOW style, so we
    // provide it as a filter to ensure windows with the style are not returned.
    return window_capture_helper_.EnumerateCapturableWindows(
        sources, enumerate_current_process_windows_, WS_EX_TOOLWINDOW);
  }

 private:
  WindowCaptureHelperWin window_capture_helper_;
  bool enumerate_current_process_windows_;
};

class ScreenEnumerator final : public SourceEnumerator {
 public:
  ScreenEnumerator() = default;

  ScreenEnumerator(const ScreenEnumerator&) = delete;
  ScreenEnumerator& operator=(const ScreenEnumerator&) = delete;

  ~ScreenEnumerator() override = default;

  bool FindAllSources(DesktopCapturer::SourceList* sources) override {
    return webrtc::GetScreenList(sources);
  }
};

// A capturer that uses the Window.Graphics.Capture APIs. It is suitable for
// both window and screen capture (but only one type per instance). Consumers
// should not instantiate this class directly, instead they should use
// `CreateRawWindowCapturer()` or `CreateRawScreenCapturer()` to receive a
// capturer appropriate for the type of source they want to capture.
class WgcCapturerWin : public DesktopCapturer {
 public:
  WgcCapturerWin(std::unique_ptr<WgcCaptureSourceFactory> source_factory,
                 std::unique_ptr<SourceEnumerator> source_enumerator);

  WgcCapturerWin(const WgcCapturerWin&) = delete;
  WgcCapturerWin& operator=(const WgcCapturerWin&) = delete;

  ~WgcCapturerWin() override;

  static std::unique_ptr<DesktopCapturer> CreateRawWindowCapturer(
      const DesktopCaptureOptions& options);

  static std::unique_ptr<DesktopCapturer> CreateRawScreenCapturer(
      const DesktopCaptureOptions& options);

  // DesktopCapturer interface.
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;
  bool FocusOnSelectedSource() override;
  void Start(Callback* callback) override;
  void CaptureFrame() override;

  // Used in WgcCapturerTests.
  bool IsSourceBeingCaptured(SourceId id);

 private:
  // Factory to create a WgcCaptureSource for us whenever SelectSource is
  // called. Initialized at construction with a source-specific implementation.
  std::unique_ptr<WgcCaptureSourceFactory> source_factory_;

  // The source enumerator helps us find capturable sources of the appropriate
  // type. Initialized at construction with a source-specific implementation.
  std::unique_ptr<SourceEnumerator> source_enumerator_;

  // The WgcCaptureSource represents the source we are capturing. It tells us
  // if the source is capturable and it creates the GraphicsCaptureItem for us.
  std::unique_ptr<WgcCaptureSource> capture_source_;

  // A map of all the sources we are capturing and the associated
  // WgcCaptureSession. Frames for the current source (indicated via
  // SelectSource) will be retrieved from the appropriate session when
  // requested via CaptureFrame.
  // This helps us efficiently capture multiple sources (e.g. when consumers
  // are trying to display a list of available capture targets with thumbnails).
  std::map<SourceId, WgcCaptureSession> ongoing_captures_;

  // The callback that we deliver frames to, synchronously, before CaptureFrame
  // returns.
  Callback* callback_ = nullptr;

  // A Direct3D11 device that is shared amongst the WgcCaptureSessions, who
  // require one to perform the capture.
  Microsoft::WRL::ComPtr<::ID3D11Device> d3d11_device_;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_WIN_WGC_CAPTURER_WIN_H_
