/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_SEQUENCED_TASK_CHECKER_H_
#define WEBRTC_BASE_SEQUENCED_TASK_CHECKER_H_

// Apart from debug builds, we also enable the sequence checker in
// builds with RTC_DCHECK_IS_ON so that trybots and waterfall bots
// with this define will get the same level of checking as debug bots.
#define ENABLE_SEQUENCED_TASK_CHECKER RTC_DCHECK_IS_ON

#include "webrtc/base/checks.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/base/sequenced_task_checker_impl.h"

namespace rtc {

// Do nothing implementation, for use in release mode.
//
// Note: You should almost always use the SequencedTaskChecker class to get the
// right version for your build configuration.
class SequencedTaskCheckerDoNothing {
 public:
  bool CalledSequentially() const { return true; }

  void Detach() {}
};

// SequencedTaskChecker is a helper class used to help verify that some methods
// of a class are called on the same task queue or thread. A
// SequencedTaskChecker is bound to a a task queue if the object is
// created on a task queue, or a thread otherwise.
//
//
// Example:
// class MyClass {
//  public:
//   void Foo() {
//     RTC_DCHECK(sequence_checker_.CalledSequentially());
//     ... (do stuff) ...
//   }
//
//  private:
//   SequencedTaskChecker sequence_checker_;
// }
//
// In Release mode, CalledOnValidThread will always return true.
#if ENABLE_SEQUENCED_TASK_CHECKER
class LOCKABLE SequencedTaskChecker : public SequencedTaskCheckerImpl {};
#else
class LOCKABLE SequencedTaskChecker : public SequencedTaskCheckerDoNothing {};
#endif  // ENABLE_SEQUENCED_TASK_CHECKER_H_

namespace internal {
class SCOPED_LOCKABLE SequencedTaskCheckerScope {
 public:
  explicit SequencedTaskCheckerScope(const SequencedTaskChecker* checker)
      EXCLUSIVE_LOCK_FUNCTION(checker);
  ~SequencedTaskCheckerScope() UNLOCK_FUNCTION();
};

}  // namespace internal

#define RTC_DCHECK_CALLED_SEQUENTIALLY(x) \
  rtc::internal::SequencedTaskCheckerScope checker(x)

#undef ENABLE_SEQUENCED_TASK_CHECKER

}  // namespace rtc
#endif  // WEBRTC_BASE_SEQUENCED_TASK_CHECKER_H_
