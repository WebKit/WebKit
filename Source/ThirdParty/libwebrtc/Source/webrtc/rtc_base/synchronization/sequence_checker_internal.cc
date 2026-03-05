/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_base/synchronization/sequence_checker_internal.h"

#include <string>

#include "api/task_queue/task_queue_base.h"
#include "rtc_base/checks.h"
#include "rtc_base/platform_thread_types.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/synchronization/mutex.h"

namespace webrtc {
namespace webrtc_sequence_checker_internal {

SequenceCheckerImpl::SequenceCheckerImpl(bool attach_to_current_thread)
    : attached_(attach_to_current_thread),
      valid_thread_(CurrentThreadRef()),
      valid_queue_(TaskQueueBase::Current()) {}

SequenceCheckerImpl::SequenceCheckerImpl(TaskQueueBase* attached_queue)
    : attached_(attached_queue != nullptr),
      valid_thread_(PlatformThreadRef()),
      valid_queue_(attached_queue) {}

SequenceCheckerImpl::SequenceCheckerImpl(SequenceCheckerImpl&& o)
    : SequenceCheckerImpl(/*attach_to_current_thread=*/false) {
  o.Detach();
}

bool SequenceCheckerImpl::IsCurrent() const {
  const TaskQueueBase* const current_queue = TaskQueueBase::Current();
  const PlatformThreadRef current_thread = CurrentThreadRef();
  MutexLock scoped_lock(&lock_);
  if (!attached_) {  // Previously detached.
    attached_ = true;
    valid_thread_ = current_thread;
    valid_queue_ = current_queue;
    return true;
  }
  if (valid_queue_) {
    return valid_queue_ == current_queue;
  }
  return IsThreadRefEqual(valid_thread_, current_thread);
}

void SequenceCheckerImpl::Detach() {
  MutexLock scoped_lock(&lock_);
  attached_ = false;
  // We don't need to touch the other members here, they will be
  // reset on the next call to IsCurrent().
}

#if RTC_DCHECK_IS_ON
std::string SequenceCheckerImpl::ExpectationToString() const {
  const TaskQueueBase* const current_queue = TaskQueueBase::Current();
  const PlatformThreadRef current_thread = CurrentThreadRef();
  MutexLock scoped_lock(&lock_);
  if (!attached_)
    return "Checker currently not attached.";

  // The format of the string is meant to compliment the one we have inside of
  // FatalLog() (checks.cc).  Example:
  //
  // # Expected: TQ: 0x0 SysQ: 0x7fff69541330 Thread: 0x11dcf6dc0
  // # Actual:   TQ: 0x7fa8f0604190 SysQ: 0x7fa8f0604a30 Thread: 0x700006f1a000
  // TaskQueue doesn't match

  StringBuilder message;
  message.AppendFormat(
      "# Expected: TQ: %p Thread: %p\n"
      "# Actual:   TQ: %p Thread: %p\n",
      valid_queue_, reinterpret_cast<const void*>(valid_thread_), current_queue,
      reinterpret_cast<const void*>(current_thread));

  if ((valid_queue_ || current_queue) && valid_queue_ != current_queue) {
    message << "TaskQueue doesn't match\n";
  } else if (!IsThreadRefEqual(valid_thread_, current_thread)) {
    message << "Threads don't match\n";
  }

  return message.Release();
}
#endif  // RTC_DCHECK_IS_ON

}  // namespace webrtc_sequence_checker_internal
}  // namespace webrtc
