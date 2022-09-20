/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/wayland/base_capturer_pipewire.h"

#include "modules/desktop_capture/desktop_capture_options.h"
#include "modules/desktop_capture/desktop_capturer.h"
#include "modules/desktop_capture/linux/wayland/restore_token_manager.h"
#include "modules/desktop_capture/linux/wayland/xdg_desktop_portal_utils.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/random.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

namespace {

using xdg_portal::RequestResponse;
using xdg_portal::ScreenCapturePortalInterface;
using xdg_portal::SessionDetails;

}  // namespace

BaseCapturerPipeWire::BaseCapturerPipeWire(const DesktopCaptureOptions& options)
    : BaseCapturerPipeWire(
          options,
          std::make_unique<ScreenCastPortal>(
              ScreenCastPortal::CaptureSourceType::kAnyScreenContent,
              this)) {
  is_screencast_portal_ = true;
}

BaseCapturerPipeWire::BaseCapturerPipeWire(
    const DesktopCaptureOptions& options,
    std::unique_ptr<ScreenCapturePortalInterface> portal)
    : options_(options),
      is_screencast_portal_(false),
      portal_(std::move(portal)) {
  Random random(rtc::TimeMicros());
  source_id_ = static_cast<SourceId>(random.Rand(1, INT_MAX));
}

BaseCapturerPipeWire::~BaseCapturerPipeWire() {}

void BaseCapturerPipeWire::OnScreenCastRequestResult(RequestResponse result,
                                                     uint32_t stream_node_id,
                                                     int fd) {
  if (result != RequestResponse::kSuccess ||
      !options_.screencast_stream()->StartScreenCastStream(
          stream_node_id, fd, options_.get_width(), options_.get_height())) {
    capturer_failed_ = true;
    RTC_LOG(LS_ERROR) << "ScreenCastPortal failed: "
                      << static_cast<uint>(result);
  } else if (ScreenCastPortal* screencast_portal = GetScreenCastPortal()) {
    if (!screencast_portal->RestoreToken().empty()) {
      RestoreTokenManager::GetInstance().AddToken(
          source_id_, screencast_portal->RestoreToken());
    }
  }
}

void BaseCapturerPipeWire::OnScreenCastSessionClosed() {
  if (!capturer_failed_) {
    options_.screencast_stream()->StopScreenCastStream();
  }
}

void BaseCapturerPipeWire::UpdateResolution(uint32_t width, uint32_t height) {
  if (!capturer_failed_) {
    options_.screencast_stream()->UpdateScreenCastStreamResolution(width,
                                                                   height);
  }
}

void BaseCapturerPipeWire::Start(Callback* callback) {
  RTC_DCHECK(!callback_);
  RTC_DCHECK(callback);

  callback_ = callback;

  if (ScreenCastPortal* screencast_portal = GetScreenCastPortal()) {
    screencast_portal->SetPersistMode(
        ScreenCastPortal::PersistMode::kTransient);
    if (selected_source_id_) {
      screencast_portal->SetRestoreToken(
          RestoreTokenManager::GetInstance().TakeToken(selected_source_id_));
    }
  }

  portal_->Start();
}

void BaseCapturerPipeWire::CaptureFrame() {
  if (capturer_failed_) {
    callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
    return;
  }

  std::unique_ptr<DesktopFrame> frame =
      options_.screencast_stream()->CaptureFrame();

  if (!frame || !frame->data()) {
    callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
    return;
  }

  // TODO(julien.isorce): http://crbug.com/945468. Set the icc profile on
  // the frame, see ScreenCapturerX11::CaptureFrame.

  callback_->OnCaptureResult(Result::SUCCESS, std::move(frame));
}

bool BaseCapturerPipeWire::GetSourceList(SourceList* sources) {
  RTC_DCHECK(sources->size() == 0);
  // List of available screens is already presented by the xdg-desktop-portal,
  // so we just need a (valid) source id for any callers to pass around, even
  // though it doesn't mean anything to us. Until the user selects a source in
  // xdg-desktop-portal we'll just end up returning empty frames. Note that "0"
  // is often treated as a null/placeholder id, so we shouldn't use that.
  // TODO(https://crbug.com/1297671): Reconsider type of ID when plumbing
  // token that will enable stream re-use.
  sources->push_back({source_id_});
  return true;
}

bool BaseCapturerPipeWire::SelectSource(SourceId id) {
  // Screen selection is handled by the xdg-desktop-portal.
  selected_source_id_ = id;
  return true;
}

SessionDetails BaseCapturerPipeWire::GetSessionDetails() {
  return portal_->GetSessionDetails();
}

ScreenCastPortal* BaseCapturerPipeWire::GetScreenCastPortal() {
  return is_screencast_portal_ ? static_cast<ScreenCastPortal*>(portal_.get())
                               : nullptr;
}

}  // namespace webrtc
