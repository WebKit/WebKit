/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/cancelable_task_handle.h"

#include <utility>

#include "rtc_base/refcounter.h"
#include "rtc_base/sequenced_task_checker.h"
#include "rtc_base/thread_annotations.h"
#include "rtc_base/thread_checker.h"

namespace rtc {

class CancelableTaskHandle::CancellationToken {
 public:
  CancellationToken() : canceled_(false), ref_count_(0) { checker_.Detach(); }
  CancellationToken(const CancellationToken&) = delete;
  CancellationToken& operator=(const CancellationToken&) = delete;

  void Cancel() {
    RTC_DCHECK_RUN_ON(&checker_);
    canceled_ = true;
  }

  bool Canceled() {
    RTC_DCHECK_RUN_ON(&checker_);
    return canceled_;
  }

  void AddRef() { ref_count_.IncRef(); }

  void Release() {
    if (ref_count_.DecRef() == rtc::RefCountReleaseStatus::kDroppedLastRef)
      delete this;
  }

 private:
  ~CancellationToken() = default;

  rtc::SequencedTaskChecker checker_;
  bool canceled_ RTC_GUARDED_BY(checker_);
  webrtc::webrtc_impl::RefCounter ref_count_;
};

CancelableTaskHandle::CancelableTaskHandle() = default;
CancelableTaskHandle::CancelableTaskHandle(const CancelableTaskHandle&) =
    default;
CancelableTaskHandle::CancelableTaskHandle(CancelableTaskHandle&&) = default;
CancelableTaskHandle& CancelableTaskHandle::operator=(
    const CancelableTaskHandle&) = default;
CancelableTaskHandle& CancelableTaskHandle::operator=(CancelableTaskHandle&&) =
    default;
CancelableTaskHandle::~CancelableTaskHandle() = default;

void CancelableTaskHandle::Cancel() const {
  if (cancellation_token_.get() != nullptr)
    cancellation_token_->Cancel();
}

CancelableTaskHandle::CancelableTaskHandle(
    rtc::scoped_refptr<CancellationToken> cancellation_token)
    : cancellation_token_(std::move(cancellation_token)) {}

BaseCancelableTask::~BaseCancelableTask() = default;

CancelableTaskHandle BaseCancelableTask::GetCancellationHandle() const {
  return CancelableTaskHandle(cancellation_token_);
}

BaseCancelableTask::BaseCancelableTask()
    : cancellation_token_(new CancelableTaskHandle::CancellationToken) {}

bool BaseCancelableTask::Canceled() const {
  return cancellation_token_->Canceled();
}

}  // namespace rtc
