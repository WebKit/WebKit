/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/synchronization/sequence_checker.h"

#if defined(WEBRTC_MAC)
#include <dispatch/dispatch.h>
#endif

namespace webrtc {
namespace {
// On Mac, returns the label of the current dispatch queue; elsewhere, return
// null.
const void* GetSystemQueueRef() {
#if defined(WEBRTC_MAC)
  return dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL);
#else
  return nullptr;
#endif
}
}  // namespace

SequenceCheckerImpl::SequenceCheckerImpl()
    : attached_(true),
      valid_thread_(rtc::CurrentThreadRef()),
      valid_queue_(TaskQueueBase::Current()),
      valid_system_queue_(GetSystemQueueRef()) {}

SequenceCheckerImpl::~SequenceCheckerImpl() = default;

bool SequenceCheckerImpl::IsCurrent() const {
  const TaskQueueBase* const current_queue = TaskQueueBase::Current();
  const rtc::PlatformThreadRef current_thread = rtc::CurrentThreadRef();
  const void* const current_system_queue = GetSystemQueueRef();
  rtc::CritScope scoped_lock(&lock_);
  if (!attached_) {  // Previously detached.
    attached_ = true;
    valid_thread_ = current_thread;
    valid_queue_ = current_queue;
    valid_system_queue_ = current_system_queue;
    return true;
  }
  if (valid_queue_ || current_queue) {
    return valid_queue_ == current_queue;
  }
  if (valid_system_queue_ && valid_system_queue_ == current_system_queue) {
    return true;
  }
  return rtc::IsThreadRefEqual(valid_thread_, current_thread);
}

void SequenceCheckerImpl::Detach() {
  rtc::CritScope scoped_lock(&lock_);
  attached_ = false;
  // We don't need to touch the other members here, they will be
  // reset on the next call to IsCurrent().
}

}  // namespace webrtc
