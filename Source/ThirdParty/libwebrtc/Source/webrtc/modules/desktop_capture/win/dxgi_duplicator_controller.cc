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
#include "webrtc/modules/desktop_capture/win/dxgi_frame.h"
#include "webrtc/modules/desktop_capture/win/screen_capture_utils.h"
#include "webrtc/system_wrappers/include/sleep.h"

namespace webrtc {

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

bool DxgiDuplicatorController::RetrieveD3dInfo(D3dInfo* info) {
  rtc::CritScope lock(&lock_);
  if (!Initialize()) {
    return false;
  }
  *info = d3d_info_;
  return true;
}

DxgiDuplicatorController::Result
DxgiDuplicatorController::Duplicate(DxgiFrame* frame) {
  return DoDuplicate(frame, -1);
}

DxgiDuplicatorController::Result
DxgiDuplicatorController::DuplicateMonitor(DxgiFrame* frame, int monitor_id) {
  RTC_DCHECK_GE(monitor_id, 0);
  return DoDuplicate(frame, monitor_id);
}

DesktopVector DxgiDuplicatorController::dpi() {
  rtc::CritScope lock(&lock_);
  if (Initialize()) {
    return dpi_;
  }
  return DesktopVector();
}

int DxgiDuplicatorController::ScreenCount() {
  rtc::CritScope lock(&lock_);
  if (Initialize()) {
    return ScreenCountUnlocked();
  }
  return 0;
}

DxgiDuplicatorController::Result
DxgiDuplicatorController::DoDuplicate(DxgiFrame* frame, int monitor_id) {
  RTC_DCHECK(frame);
  rtc::CritScope lock(&lock_);

  // The dxgi components and APIs do not update the screen resolution without
  // a reinitialization. So we use the GetDC() function to retrieve the screen
  // resolution to decide whether dxgi components need to be reinitialized.
  // If the screen resolution changed, it's very likely the next Duplicate()
  // function call will fail because of a missing monitor or the frame size is
  // not enough to store the output. So we reinitialize dxgi components in-place
  // to avoid a capture failure.
  // But there is no guarantee GetDC() function returns the same resolution as
  // dxgi APIs, we still rely on dxgi components to return the output frame
  // size.
  // TODO(zijiehe): Confirm whether IDXGIOutput::GetDesc() and
  // IDXGIOutputDuplication::GetDesc() can detect the resolution change without
  // reinitialization.
  if (resolution_change_detector_.IsChanged(
          GetScreenRect(kFullDesktopScreenId, std::wstring()).size())) {
    Deinitialize();
  }

  if (!Initialize()) {
    // Cannot initialize COM components now, display mode may be changing.
    return Result::INITIALIZATION_FAILED;
  }

  if (!frame->Prepare(SelectedDesktopSize(monitor_id), monitor_id)) {
    return Result::FRAME_PREPARE_FAILED;
  }

  frame->frame()->mutable_updated_region()->Clear();

  if (DoDuplicateUnlocked(frame->context(), monitor_id, frame->frame())) {
    return Result::SUCCEEDED;
  }
  if (monitor_id >= ScreenCountUnlocked()) {
    // It's a user error to provide a |monitor_id| larger than screen count. We
    // do not need to deinitialize.
    return Result::INVALID_MONITOR_ID;
  }

  // If the |monitor_id| is valid, but DoDuplicateUnlocked() failed, something
  // must be wrong from capturer APIs. We should Deinitialize().
  Deinitialize();
  return Result::DUPLICATION_FAILED;
}

void DxgiDuplicatorController::Unregister(const Context* const context) {
  rtc::CritScope lock(&lock_);
  if (ContextExpired(context)) {
    // The Context has not been setup after a recent initialization, so it
    // should not been registered in duplicators.
    return;
  }
  for (size_t i = 0; i < duplicators_.size(); i++) {
    duplicators_[i].Unregister(&context->contexts[i]);
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

    desktop_rect_.UnionWith(duplicators_.back().desktop_rect());
  }
  TranslateRect();

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
  RTC_DCHECK(context);
  return context->controller_id != identity_ ||
         context->contexts.size() != duplicators_.size();
}

void DxgiDuplicatorController::Setup(Context* context) {
  if (ContextExpired(context)) {
    RTC_DCHECK(context);
    context->contexts.clear();
    context->contexts.resize(duplicators_.size());
    for (size_t i = 0; i < duplicators_.size(); i++) {
      duplicators_[i].Setup(&context->contexts[i]);
    }
    context->controller_id = identity_;
  }
}

bool DxgiDuplicatorController::DoDuplicateUnlocked(Context* context,
                                                   int monitor_id,
                                                   SharedDesktopFrame* target) {
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
    if (!duplicators_[i].Duplicate(&context->contexts[i], target)) {
      return false;
    }
  }
  return true;
}

bool DxgiDuplicatorController::DoDuplicateOne(Context* context,
                                              int monitor_id,
                                              SharedDesktopFrame* target) {
  RTC_DCHECK(monitor_id >= 0);
  for (size_t i = 0; i < duplicators_.size() && i < context->contexts.size();
       i++) {
    if (monitor_id >= duplicators_[i].screen_count()) {
      monitor_id -= duplicators_[i].screen_count();
    } else {
      if (duplicators_[i].DuplicateMonitor(&context->contexts[i], monitor_id,
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

DesktopSize DxgiDuplicatorController::desktop_size() const {
  return desktop_rect_.size();
}

DesktopRect DxgiDuplicatorController::ScreenRect(int id) const {
  RTC_DCHECK(id >= 0);
  for (size_t i = 0; i < duplicators_.size(); i++) {
    if (id >= duplicators_[i].screen_count()) {
      id -= duplicators_[i].screen_count();
    } else {
      return duplicators_[i].ScreenRect(id);
    }
  }
  return DesktopRect();
}

int DxgiDuplicatorController::ScreenCountUnlocked() const {
  int result = 0;
  for (auto& duplicator : duplicators_) {
    result += duplicator.screen_count();
  }
  return result;
}

DesktopSize DxgiDuplicatorController::SelectedDesktopSize(
    int monitor_id) const {
  if (monitor_id < 0) {
    return desktop_size();
  }

  return ScreenRect(monitor_id).size();
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

void DxgiDuplicatorController::TranslateRect() {
  const DesktopVector position =
      DesktopVector().subtract(desktop_rect_.top_left());
  desktop_rect_.Translate(position);
  for (auto& duplicator : duplicators_) {
    duplicator.TranslateRect(position);
  }
}

}  // namespace webrtc
