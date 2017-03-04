/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/win/dxgi_duplicator_controller.h"

#include <windows.h>

#include <algorithm>
#include <string>

#include "webrtc/base/checks.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "webrtc/modules/desktop_capture/win/screen_capture_utils.h"
#include "webrtc/system_wrappers/include/sleep.h"

namespace webrtc {

DxgiDuplicatorController::Context::Context() = default;

DxgiDuplicatorController::Context::~Context() {
  DxgiDuplicatorController::Instance()->Unregister(this);
}

void DxgiDuplicatorController::Context::Reset() {
  identity_ = 0;
}

// static
DxgiDuplicatorController* DxgiDuplicatorController::Instance() {
  // The static instance won't be deleted to ensure it can be used by other
  // threads even during program exiting.
  static DxgiDuplicatorController* instance = new DxgiDuplicatorController();
  return instance;
}

DxgiDuplicatorController::DxgiDuplicatorController() = default;

DxgiDuplicatorController::~DxgiDuplicatorController() {
  rtc::CritScope lock(&lock_);
  Deinitialize();
}

bool DxgiDuplicatorController::IsSupported() {
  rtc::CritScope lock(&lock_);
  return Initialize();
}

void DxgiDuplicatorController::Reset() {
  rtc::CritScope lock(&lock_);
  Deinitialize();
}

bool DxgiDuplicatorController::RetrieveD3dInfo(D3dInfo* info) {
  rtc::CritScope lock(&lock_);
  if (!Initialize()) {
    return false;
  }
  *info = d3d_info_;
  return true;
}

DesktopVector DxgiDuplicatorController::dpi() {
  rtc::CritScope lock(&lock_);
  if (Initialize()) {
    return dpi_;
  }
  return DesktopVector();
}

DesktopRect DxgiDuplicatorController::desktop_rect() {
  rtc::CritScope lock(&lock_);
  if (Initialize()) {
    return desktop_rect_;
  }
  return DesktopRect();
}

DesktopSize DxgiDuplicatorController::desktop_size() {
  DesktopRect rect = desktop_rect();
  return DesktopSize(rect.right(), rect.bottom());
}

DesktopRect DxgiDuplicatorController::ScreenRect(int id) {
  RTC_DCHECK(id >= 0);
  rtc::CritScope lock(&lock_);
  if (!Initialize()) {
    return DesktopRect();
  }
  for (size_t i = 0; i < duplicators_.size(); i++) {
    if (id >= duplicators_[i].screen_count()) {
      id -= duplicators_[i].screen_count();
    } else {
      return duplicators_[i].ScreenRect(id);
    }
  }
  return DesktopRect();
}

int DxgiDuplicatorController::ScreenCount() {
  rtc::CritScope lock(&lock_);
  return ScreenCountUnlocked();
}

void DxgiDuplicatorController::Unregister(const Context* const context) {
  rtc::CritScope lock(&lock_);
  if (ContextExpired(context)) {
    // The Context has not been setup after a recent initialization, so it
    // should not been registered in duplicators.
    return;
  }
  for (size_t i = 0; i < duplicators_.size(); i++) {
    duplicators_[i].Unregister(&context->contexts_[i]);
  }
}

bool DxgiDuplicatorController::Initialize() {
  if (!duplicators_.empty()) {
    return true;
  }

  if (DoInitialize()) {
    return true;
  }
  Deinitialize();
  return false;
}

bool DxgiDuplicatorController::DoInitialize() {
  RTC_DCHECK(desktop_rect_.is_empty());
  RTC_DCHECK(duplicators_.empty());

  d3d_info_.min_feature_level = static_cast<D3D_FEATURE_LEVEL>(0);
  d3d_info_.max_feature_level = static_cast<D3D_FEATURE_LEVEL>(0);

  std::vector<D3dDevice> devices = D3dDevice::EnumDevices();
  if (devices.empty()) {
    return false;
  }

  for (size_t i = 0; i < devices.size(); i++) {
    duplicators_.emplace_back(devices[i]);
    if (!duplicators_.back().Initialize()) {
      return false;
    }

    D3D_FEATURE_LEVEL feature_level =
        devices[i].d3d_device()->GetFeatureLevel();
    if (d3d_info_.max_feature_level == 0 ||
        feature_level > d3d_info_.max_feature_level) {
      d3d_info_.max_feature_level = feature_level;
    }
    if (d3d_info_.min_feature_level == 0 ||
        feature_level < d3d_info_.min_feature_level) {
      d3d_info_.min_feature_level = feature_level;
    }

    if (desktop_rect_.is_empty()) {
      desktop_rect_ = duplicators_.back().desktop_rect();
    } else {
      const DesktopRect& left = desktop_rect_;
      const DesktopRect& right = duplicators_.back().desktop_rect();
      desktop_rect_ =
          DesktopRect::MakeLTRB(std::min(left.left(), right.left()),
                                std::min(left.top(), right.top()),
                                std::max(left.right(), right.right()),
                                std::max(left.bottom(), right.bottom()));
    }
  }

  HDC hdc = GetDC(nullptr);
  // Use old DPI value if failed.
  if (hdc) {
    dpi_.set(GetDeviceCaps(hdc, LOGPIXELSX), GetDeviceCaps(hdc, LOGPIXELSY));
    ReleaseDC(nullptr, hdc);
  }

  identity_++;
  return true;
}

void DxgiDuplicatorController::Deinitialize() {
  desktop_rect_ = DesktopRect();
  duplicators_.clear();
  resolution_change_detector_.Reset();
}

bool DxgiDuplicatorController::ContextExpired(
    const Context* const context) const {
  return context->identity_ != identity_ ||
         context->contexts_.size() != duplicators_.size();
}

void DxgiDuplicatorController::Setup(Context* context) {
  if (ContextExpired(context)) {
    context->contexts_.clear();
    context->contexts_.resize(duplicators_.size());
    for (size_t i = 0; i < duplicators_.size(); i++) {
      duplicators_[i].Setup(&context->contexts_[i]);
    }
    context->identity_ = identity_;
  }
}

bool DxgiDuplicatorController::Duplicate(Context* context,
                                         SharedDesktopFrame* target) {
  return DoDuplicate(context, -1, target);
}

bool DxgiDuplicatorController::DuplicateMonitor(Context* context,
                                                int monitor_id,
                                                SharedDesktopFrame* target) {
  RTC_DCHECK_GE(monitor_id, 0);
  return DoDuplicate(context, monitor_id, target);
}

bool DxgiDuplicatorController::DoDuplicate(Context* context,
                                           int monitor_id,
                                           SharedDesktopFrame* target) {
  RTC_DCHECK(target);
  target->mutable_updated_region()->Clear();
  rtc::CritScope lock(&lock_);
  if (DoDuplicateUnlocked(context, monitor_id, target)) {
    return true;
  }
  if (monitor_id < ScreenCountUnlocked()) {
    // It's a user error to provide a |monitor_id| larger than screen count. We
    // do not need to deinitialize.
    Deinitialize();
  }
  return false;
}

bool DxgiDuplicatorController::DoDuplicateUnlocked(Context* context,
                                                   int monitor_id,
                                                   SharedDesktopFrame* target) {
  if (!Initialize()) {
    // Cannot initialize COM components now, display mode may be changing.
    return false;
  }

  if (resolution_change_detector_.IsChanged(
          GetScreenRect(kFullDesktopScreenId, std::wstring()).size())) {
    // Resolution of entire screen has been changed, which usually means a new
    // monitor has been attached or one has been removed. The simplest way is to
    // Deinitialize() and returns false to indicate downstream components.
    return false;
  }

  Setup(context);

  if (!EnsureFrameCaptured(context, target)) {
    return false;
  }

  bool result = false;
  if (monitor_id < 0) {
    // Capture entire screen.
    result = DoDuplicateAll(context, target);
  } else {
    result = DoDuplicateOne(context, monitor_id, target);
  }

  if (result) {
    target->set_dpi(dpi());
    return true;
  }

  return false;
}

bool DxgiDuplicatorController::DoDuplicateAll(Context* context,
                                              SharedDesktopFrame* target) {
  for (size_t i = 0; i < duplicators_.size(); i++) {
    if (!duplicators_[i].Duplicate(&context->contexts_[i], target)) {
      return false;
    }
  }
  return true;
}

bool DxgiDuplicatorController::DoDuplicateOne(Context* context,
                                              int monitor_id,
                                              SharedDesktopFrame* target) {
  RTC_DCHECK(monitor_id >= 0);
  for (size_t i = 0; i < duplicators_.size() && i < context->contexts_.size();
       i++) {
    if (monitor_id >= duplicators_[i].screen_count()) {
      monitor_id -= duplicators_[i].screen_count();
    } else {
      if (duplicators_[i].DuplicateMonitor(&context->contexts_[i], monitor_id,
                                           target)) {
        return true;
      }
      return false;
    }
  }
  return false;
}

int64_t DxgiDuplicatorController::GetNumFramesCaptured() const {
  int64_t min = INT64_MAX;
  for (const auto& duplicator : duplicators_) {
    min = std::min(min, duplicator.GetNumFramesCaptured());
  }

  return min;
}

int DxgiDuplicatorController::ScreenCountUnlocked() {
  if (!Initialize()) {
    return 0;
  }
  int result = 0;
  for (auto& duplicator : duplicators_) {
    result += duplicator.screen_count();
  }
  return result;
}

bool DxgiDuplicatorController::EnsureFrameCaptured(Context* context,
                                                   SharedDesktopFrame* target) {
  // On a modern system, the FPS / monitor refresh rate is usually larger than
  // or equal to 60. So 17 milliseconds is enough to capture at least one frame.
  const int64_t ms_per_frame = 17;
  // Skips the first frame to ensure a full frame refresh has happened before
  // this function returns.
  const int64_t frames_to_skip = 1;
  // The total time out milliseconds for this function. If we cannot get enough
  // frames during this time interval, this function returns false, and cause
  // the DXGI components to be reinitialized. This usually should not happen
  // unless the system is switching display mode when this function is being
  // called. 500 milliseconds should be enough for ~30 frames.
  const int64_t timeout_ms = 500;
  if (GetNumFramesCaptured() >= frames_to_skip) {
    return true;
  }

  std::unique_ptr<SharedDesktopFrame> fallback_frame;
  SharedDesktopFrame* shared_frame = nullptr;
  if (target->size().width() >= desktop_size().width() &&
      target->size().height() >= desktop_size().height()) {
    // |target| is large enough to cover entire screen, we do not need to use
    // |fallback_frame|.
    shared_frame = target;
  } else {
    fallback_frame = SharedDesktopFrame::Wrap(std::unique_ptr<DesktopFrame>(
        new BasicDesktopFrame(desktop_size())));
    shared_frame = fallback_frame.get();
  }

  const int64_t start_ms = rtc::TimeMillis();
  int64_t last_frame_start_ms = 0;
  while (GetNumFramesCaptured() < frames_to_skip) {
    if (GetNumFramesCaptured() > 0) {
      // Sleep |ms_per_frame| before capturing next frame to ensure the screen
      // has been updated by the video adapter.
      webrtc::SleepMs(
          ms_per_frame - (rtc::TimeMillis() - last_frame_start_ms));
    }
    last_frame_start_ms = rtc::TimeMillis();
    if (!DoDuplicateAll(context, shared_frame)) {
      return false;
    }
    if (rtc::TimeMillis() - start_ms > timeout_ms) {
      return false;
    }
  }
  return true;
}

}  // namespace webrtc
