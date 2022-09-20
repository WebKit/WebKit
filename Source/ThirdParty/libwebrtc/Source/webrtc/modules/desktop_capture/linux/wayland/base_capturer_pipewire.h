/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_BASE_CAPTURER_PIPEWIRE_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_BASE_CAPTURER_PIPEWIRE_H_

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/linux/wayland/portal_request_response.h"
#include "modules/desktop_capture/linux/wayland/screen_capture_portal_interface.h"
#include "modules/desktop_capture/linux/wayland/screencast_portal.h"
#include "modules/desktop_capture/linux/wayland/shared_screencast_stream.h"
#include "modules/desktop_capture/linux/wayland/xdg_desktop_portal_utils.h"
#include "modules/desktop_capture/linux/wayland/xdg_session_details.h"

namespace webrtc {

class BaseCapturerPipeWire : public DesktopCapturer,
                             public ScreenCastPortal::PortalNotifier {
 public:
  explicit BaseCapturerPipeWire(const DesktopCaptureOptions& options);
  BaseCapturerPipeWire(
      const DesktopCaptureOptions& options,
      std::unique_ptr<xdg_portal::ScreenCapturePortalInterface> portal);
  ~BaseCapturerPipeWire() override;

  BaseCapturerPipeWire(const BaseCapturerPipeWire&) = delete;
  BaseCapturerPipeWire& operator=(const BaseCapturerPipeWire&) = delete;

  // DesktopCapturer interface.
  void Start(Callback* delegate) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;

  // ScreenCastPortal::PortalNotifier interface.
  void OnScreenCastRequestResult(xdg_portal::RequestResponse result,
                                 uint32_t stream_node_id,
                                 int fd) override;
  void OnScreenCastSessionClosed() override;
  void UpdateResolution(uint32_t width, uint32_t height) override;

  xdg_portal::SessionDetails GetSessionDetails();

 private:
  ScreenCastPortal* GetScreenCastPortal();

  DesktopCaptureOptions options_ = {};
  Callback* callback_ = nullptr;
  bool capturer_failed_ = false;
  bool is_screencast_portal_ = false;

  // SourceId that is selected using SelectSource() and that we previously
  // returned in GetSourceList(). This should be a SourceId that has a restore
  // token associated with it and can be restored if we have required version
  // of xdg-desktop-portal.
  SourceId selected_source_id_ = 0;
  // SourceID we randomly generate and that is returned in GetSourceList() as
  // available source that will later get assigned to a restore token in order
  // to be restored later using SelectSource().
  SourceId source_id_ = 0;
  std::unique_ptr<xdg_portal::ScreenCapturePortalInterface> portal_;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_WAYLAND_BASE_CAPTURER_PIPEWIRE_H_
