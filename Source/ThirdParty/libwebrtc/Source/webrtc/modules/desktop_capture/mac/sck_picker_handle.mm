/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sck_picker_handle.h"

#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include "absl/base/attributes.h"
#include "api/sequence_checker.h"

#include <memory>
#include <optional>

namespace webrtc {

class SckPickerProxy;

class API_AVAILABLE(macos(14.0)) SckPickerProxy {
 public:
  static SckPickerProxy* Get() {
    static SckPickerProxy* g_picker = new SckPickerProxy();
    return g_picker;
  }

  bool AtCapacityLocked() const RTC_EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    return handle_count_ == kMaximumStreamCount;
  }

  SCContentSharingPicker* GetPicker() const {
    return SCContentSharingPicker.sharedPicker;
  }

  ABSL_MUST_USE_RESULT std::optional<DesktopCapturer::SourceId>
      AcquireSourceId() {
    MutexLock lock(&mutex_);
    if (AtCapacityLocked()) {
      return std::nullopt;
    }
    if (handle_count_ == 0) {
      auto* picker = GetPicker();
      picker.maximumStreamCount =
          [NSNumber numberWithUnsignedInt:kMaximumStreamCount];
      picker.active = YES;
    }
    handle_count_ += 1;
    unique_source_id_ += 1;
    return unique_source_id_;
  }

  void RelinquishSourceId(DesktopCapturer::SourceId source) {
    MutexLock lock(&mutex_);
    handle_count_ -= 1;
    if (handle_count_ > 0) {
      return;
    }
    GetPicker().active = NO;
  }

 private:
  // SckPickerProxy is a process-wide singleton. ScreenCapturerSck and
  // SckPickerHandle are largely single-threaded but may be used on different
  // threads. For instance some clients use a capturer on one thread for
  // enumeration and on another for frame capture. Since all those capturers
  // share the same SckPickerProxy instance, it must be thread-safe.
  Mutex mutex_;
  // 100 is an arbitrary number that seems high enough to never get reached,
  // while still providing a reasonably low upper bound.
  static constexpr size_t kMaximumStreamCount = 100;
  size_t handle_count_ RTC_GUARDED_BY(mutex_) = 0;
  DesktopCapturer::SourceId unique_source_id_ RTC_GUARDED_BY(mutex_) = 0;
};

class API_AVAILABLE(macos(14.0)) SckPickerHandle
    : public SckPickerHandleInterface {
 public:
  static std::unique_ptr<SckPickerHandle> Create(SckPickerProxy* proxy) {
    std::optional<DesktopCapturer::SourceId> id = proxy->AcquireSourceId();
    if (!id) {
      return nullptr;
    }
    return std::unique_ptr<SckPickerHandle>(new SckPickerHandle(proxy, *id));
  }

  ~SckPickerHandle() {
    RTC_DCHECK_RUN_ON(&thread_checker_);
    proxy_->RelinquishSourceId(source_);
  }

  SCContentSharingPicker* GetPicker() const override {
    return proxy_->GetPicker();
  }

  DesktopCapturer::SourceId Source() const override { return source_; }

 private:
  SckPickerHandle(SckPickerProxy* proxy, DesktopCapturer::SourceId source)
      : proxy_(proxy), source_(source) {
    RTC_DCHECK_RUN_ON(&thread_checker_);
  }

  webrtc::SequenceChecker thread_checker_;
  SckPickerProxy* const proxy_;
  const DesktopCapturer::SourceId source_;
};

std::unique_ptr<SckPickerHandleInterface> CreateSckPickerHandle() {
  return SckPickerHandle::Create(SckPickerProxy::Get());
}

}  // namespace webrtc
