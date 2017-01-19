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

#include "webrtc/base/checks.h"

namespace webrtc {

DxgiDuplicatorController::Context::Context() {}

DxgiDuplicatorController::Context::~Context() {
  DxgiDuplicatorController::Instance()->Unregister(this);
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
  if (!Initialize()) {
    return 0;
  }
  int result = 0;
  for (auto& duplicator : duplicators_) {
    result += duplicator.screen_count();
  }
  return result;
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
  if (!Initialize()) {
    // Cannot initialize COM components now, display mode may be changing.
    return false;
  }

  Setup(context);
  if (monitor_id < 0) {
    // Capture entire screen.
    for (size_t i = 0; i < duplicators_.size(); i++) {
      if (!duplicators_[i].Duplicate(&context->contexts_[i], target)) {
        Deinitialize();
        return false;
      }
    }
    target->set_dpi(dpi());
    return true;
  }

  // Capture one monitor.
  for (size_t i = 0; i < duplicators_.size() && i < context->contexts_.size();
       i++) {
    if (monitor_id >= duplicators_[i].screen_count()) {
      monitor_id -= duplicators_[i].screen_count();
    } else {
      if (duplicators_[i].DuplicateMonitor(&context->contexts_[i], monitor_id,
                                           target)) {
        target->set_dpi(dpi());
        return true;
      }
      Deinitialize();
      return false;
    }
  }
  // id >= ScreenCount(). This is a user error, so we do not need to
  // deinitialize.
  return false;
}

}  // namespace webrtc
