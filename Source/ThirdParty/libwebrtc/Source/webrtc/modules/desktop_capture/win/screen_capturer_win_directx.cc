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

#include <utility>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
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

DesktopSize ScreenCapturerWinDirectx::SelectedDesktopSize() const {
  if (current_screen_id_ == kFullDesktopScreenId) {
    return DxgiDuplicatorController::Instance()->desktop_size();
  }
  return DxgiDuplicatorController::Instance()
      ->ScreenRect(current_screen_id_)
      .size();
}

void ScreenCapturerWinDirectx::CaptureFrame() {
  RTC_DCHECK(callback_);

  int64_t capture_start_time_nanos = rtc::TimeNanos();

  frames_.MoveToNextFrame();
  if (!frames_.current_frame()) {
    std::unique_ptr<DesktopFrame> new_frame;
    if (shared_memory_factory_) {
      new_frame = SharedMemoryDesktopFrame::Create(
          SelectedDesktopSize(), shared_memory_factory_.get());
    } else {
      new_frame.reset(new BasicDesktopFrame(SelectedDesktopSize()));
    }
    if (!new_frame) {
      LOG(LS_ERROR) << "Failed to allocate a new DesktopFrame.";
      // This usually means we do not have enough memory or SharedMemoryFactory
      // cannot work correctly.
      callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
      return;
    }
    frames_.ReplaceCurrentFrame(SharedDesktopFrame::Wrap(std::move(new_frame)));
  }

  if (current_screen_id_ == kFullDesktopScreenId) {
    if (!DxgiDuplicatorController::Instance()->Duplicate(
            &context_, frames_.current_frame())) {
      // Screen size may be changed, so we need to reset the frames.
      frames_.Reset();
      callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
      return;
    }
  } else {
    if (!DxgiDuplicatorController::Instance()->DuplicateMonitor(
            &context_, current_screen_id_, frames_.current_frame())) {
      // Screen size may be changed, so we need to reset the frames.
      frames_.Reset();
      if (current_screen_id_ >=
          DxgiDuplicatorController::Instance()->ScreenCount()) {
        // Current monitor has been removed from the system.
        callback_->OnCaptureResult(Result::ERROR_PERMANENT, nullptr);
      } else {
        callback_->OnCaptureResult(Result::ERROR_TEMPORARY, nullptr);
      }
      return;
    }
  }

  std::unique_ptr<DesktopFrame> result = frames_.current_frame()->Share();
  result->set_capture_time_ms(
      (rtc::TimeNanos() - capture_start_time_nanos) /
      rtc::kNumNanosecsPerMillisec);
  callback_->OnCaptureResult(Result::SUCCESS, std::move(result));
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

  // Changing target screen may or may not impact frame size. So resetting
  // frames only when a Duplicate() function call returns false.
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
