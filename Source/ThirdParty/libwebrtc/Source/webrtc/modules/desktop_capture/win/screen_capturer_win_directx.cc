/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/desktop_capture/win/screen_capturer_win_directx.h"

#include <string>
#include <utility>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/ptr_util.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/modules/desktop_capture/desktop_frame.h"

namespace webrtc {

using Microsoft::WRL::ComPtr;

// static
bool ScreenCapturerWinDirectx::IsSupported() {
  // Forwards IsSupported() function call to DxgiDuplicatorController.
  return DxgiDuplicatorController::Instance()->IsSupported();
}

// static
bool ScreenCapturerWinDirectx::RetrieveD3dInfo(D3dInfo* info) {
  // Forwards SupportedFeatureLevels() function call to
  // DxgiDuplicatorController.
  return DxgiDuplicatorController::Instance()->RetrieveD3dInfo(info);
}

ScreenCapturerWinDirectx::ScreenCapturerWinDirectx(
    const DesktopCaptureOptions& options)
    : callback_(nullptr) {}

ScreenCapturerWinDirectx::~ScreenCapturerWinDirectx() {}

void ScreenCapturerWinDirectx::Start(Callback* callback) {
  RTC_DCHECK(!callback_);
  RTC_DCHECK(callback);

  callback_ = callback;
}

void ScreenCapturerWinDirectx::SetSharedMemoryFactory(
    std::unique_ptr<SharedMemoryFactory> shared_memory_factory) {
  shared_memory_factory_ = std::move(shared_memory_factory);
}

void ScreenCapturerWinDirectx::CaptureFrame() {
  RTC_DCHECK(callback_);

  int64_t capture_start_time_nanos = rtc::TimeNanos();

  frames_.MoveToNextFrame();
  if (!frames_.current_frame()) {
    frames_.ReplaceCurrentFrame(
        rtc::MakeUnique<DxgiFrame>(shared_memory_factory_.get()));
  }

  DxgiDuplicatorController::Result result;
  if (current_screen_id_ == kFullDesktopScreenId) {
    result = DxgiDuplicatorController::Instance()->Duplicate(
        frames_.current_frame());
  } else {
    result = DxgiDuplicatorController::Instance()->DuplicateMonitor(
        frames_.current_frame(), current_screen_id_);
  }

  using DuplicateResult = DxgiDuplicatorController::Result;
  switch (result) {
    case DuplicateResult::FRAME_PREPARE_FAILED: {
      LOG(LS_ERROR) << "Failed to allocate a new DesktopFrame.";
      // This usually means we do not have enough memory or SharedMemoryFactory
      // cannot work correctly.
      callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
      break;
    }
    case DuplicateResult::INVALID_MONITOR_ID: {
      callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
      break;
    }
    case DuplicateResult::INITIALIZATION_FAILED:
    case DuplicateResult::DUPLICATION_FAILED: {
      callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
      break;
    }
    case DuplicateResult::SUCCEEDED: {
      std::unique_ptr<DesktopFrame> frame =
          frames_.current_frame()->frame()->Share();
      frame->set_capture_time_ms(
          (rtc::TimeNanos() - capture_start_time_nanos) /
          rtc::kNumNanosecsPerMillisec);
      frame->set_capturer_id(DesktopCapturerId::kScreenCapturerWinDirectx);
      callback_->OnCaptureResult(Result::SUCCESS, std::move(frame));
      break;
    }
  }
}

bool ScreenCapturerWinDirectx::GetSourceList(SourceList* sources) {
  int screen_count = DxgiDuplicatorController::Instance()->ScreenCount();
  for (int i = 0; i < screen_count; i++) {
    sources->push_back({i});
  }
  return true;
}

bool ScreenCapturerWinDirectx::SelectSource(SourceId id) {
  if (id == current_screen_id_) {
    return true;
  }

  if (id == kFullDesktopScreenId) {
    current_screen_id_ = id;
    return true;
  }

  int screen_count = DxgiDuplicatorController::Instance()->ScreenCount();
  if (id >= 0 && id < screen_count) {
    current_screen_id_ = id;
    return true;
  }
  return false;
}

}  // namespace webrtc
