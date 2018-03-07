/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SEQUENCED_TASK_CHECKER_IMPL_H_
#define RTC_BASE_SEQUENCED_TASK_CHECKER_IMPL_H_

#include "rtc_base/thread_checker.h"

namespace rtc {

class TaskQueue;
// Real implementation of SequencedTaskChecker, for use in debug mode, or
// for temporary use in release mode.
//
// Note: You should almost always use the SequencedTaskChecker class to get the
// right version for your build configuration.
class SequencedTaskCheckerImpl {
 public:
  SequencedTaskCheckerImpl();
  ~SequencedTaskCheckerImpl();

  bool CalledSequentially() const;

  // Changes the task queue or thread that is checked for in IsCurrent.  This
  // may be useful when an object may be created on one task queue / thread and
  // then used exclusively on another thread.
  void Detach();

 private:
  typedef const void* QueueId;
  CriticalSection lock_;
  ThreadChecker thread_checker_;
  mutable bool attached_;
  mutable QueueId valid_queue_;
};

}  // namespace rtc
#endif  // RTC_BASE_SEQUENCED_TASK_CHECKER_IMPL_H_
