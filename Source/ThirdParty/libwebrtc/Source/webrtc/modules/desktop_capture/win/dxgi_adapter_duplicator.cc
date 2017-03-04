/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/win/dxgi_adapter_duplicator.h"

#include <comdef.h>
#include <DXGI.h>

#include <algorithm>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"

namespace webrtc {

using Microsoft::WRL::ComPtr;

namespace {

bool IsValidRect(const RECT& rect) {
  return rect.left >= 0 && rect.top >= 0 && rect.right > rect.left &&
         rect.bottom > rect.top;
}

}  // namespace

DxgiAdapterDuplicator::Context::Context() = default;
DxgiAdapterDuplicator::Context::Context(const Context& other) = default;
DxgiAdapterDuplicator::Context::~Context() = default;

DxgiAdapterDuplicator::DxgiAdapterDuplicator(const D3dDevice& device)
    : device_(device) {}
DxgiAdapterDuplicator::DxgiAdapterDuplicator(DxgiAdapterDuplicator&&) = default;
DxgiAdapterDuplicator::~DxgiAdapterDuplicator() = default;

bool DxgiAdapterDuplicator::Initialize() {
  if (DoInitialize()) {
    return true;
  }
  duplicators_.clear();
  return false;
}

bool DxgiAdapterDuplicator::DoInitialize() {
  for (int i = 0;; i++) {
    ComPtr<IDXGIOutput> output;
    _com_error error =
        device_.dxgi_adapter()->EnumOutputs(i, output.GetAddressOf());
    if (error.Error() == DXGI_ERROR_NOT_FOUND) {
      break;
    }

    if (error.Error() != S_OK || !output) {
      LOG(LS_WARNING) << "IDXGIAdapter::EnumOutputs returns an unexpected "
                         "result "
                      << error.ErrorMessage() << " with error code"
                      << error.Error();
      return false;
    }

    DXGI_OUTPUT_DESC desc;
    error = output->GetDesc(&desc);
    if (error.Error() == S_OK) {
      if (desc.AttachedToDesktop && IsValidRect(desc.DesktopCoordinates)) {
        ComPtr<IDXGIOutput1> output1;
        error = output.As(&output1);
        if (error.Error() != S_OK || !output1) {
          LOG(LS_WARNING) << "Failed to convert IDXGIOutput to IDXGIOutput1, "
                             "this usually means the system does not support "
                             "DirectX 11";
          return false;
        }
        duplicators_.emplace_back(device_, output1, desc);
        if (!duplicators_.back().Initialize()) {
          return false;
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
    } else {
      LOG(LS_WARNING) << "Failed to get output description of device " << i
                      << ", ignore.";
    }
  }
  return true;
}

void DxgiAdapterDuplicator::Setup(Context* context) {
  RTC_DCHECK(context->contexts.empty());
  context->contexts.resize(duplicators_.size());
  for (size_t i = 0; i < duplicators_.size(); i++) {
    duplicators_[i].Setup(&context->contexts[i]);
  }
}

void DxgiAdapterDuplicator::Unregister(const Context* const context) {
  RTC_DCHECK_EQ(context->contexts.size(), duplicators_.size());
  for (size_t i = 0; i < duplicators_.size(); i++) {
    duplicators_[i].Unregister(&context->contexts[i]);
  }
}

bool DxgiAdapterDuplicator::Duplicate(Context* context,
                                      SharedDesktopFrame* target) {
  RTC_DCHECK_EQ(context->contexts.size(), duplicators_.size());
  for (size_t i = 0; i < duplicators_.size(); i++) {
    if (!duplicators_[i].Duplicate(&context->contexts[i],
                                   duplicators_[i].desktop_rect().top_left(),
                                   target)) {
      return false;
    }
  }
  return true;
}

bool DxgiAdapterDuplicator::DuplicateMonitor(Context* context,
                                             int monitor_id,
                                             SharedDesktopFrame* target) {
  RTC_DCHECK(monitor_id >= 0 &&
             monitor_id < static_cast<int>(duplicators_.size()) &&
             context->contexts.size() == duplicators_.size());
  return duplicators_[monitor_id].Duplicate(&context->contexts[monitor_id],
                                            DesktopVector(), target);
}

DesktopRect DxgiAdapterDuplicator::ScreenRect(int id) const {
  RTC_DCHECK(id >= 0 && id < static_cast<int>(duplicators_.size()));
  return duplicators_[id].desktop_rect();
}

int DxgiAdapterDuplicator::screen_count() const {
  return static_cast<int>(duplicators_.size());
}

int64_t DxgiAdapterDuplicator::GetNumFramesCaptured() const {
  int64_t min = INT64_MAX;
  for (const auto& duplicator : duplicators_) {
    min = std::min(min, duplicator.num_frames_captured());
  }

  return min;
}

}  // namespace webrtc
