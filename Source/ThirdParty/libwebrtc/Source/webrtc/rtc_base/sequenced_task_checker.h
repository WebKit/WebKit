/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_SEQUENCED_TASK_CHECKER_H_
#define RTC_BASE_SEQUENCED_TASK_CHECKER_H_

// Apart from debug builds, we also enable the sequence checker in
// builds with RTC_DCHECK_IS_ON so that trybots and waterfall bots
// with this define will get the same level of checking as debug bots.
#define ENABLE_SEQUENCED_TASK_CHECKER RTC_DCHECK_IS_ON

#include "rtc_base/checks.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/sequenced_task_checker_impl.h"
#include "rtc_base/thread_annotations.h"

namespace rtc {
namespace internal {
// Forward declaration of the internal implementation of RTC_GUARDED_BY().
// SequencedTaskChecker grants this class access to call its IsCurrent() method.
// See thread_checker.h for more details.
class AnnounceOnThread;
}  // namespace internal

// Do nothing implementation, for use in release mode.
//
// Note: You should almost always use the SequencedTaskChecker class to get the
// right version for your build configuration.
class SequencedTaskCheckerDoNothing {
 public:
  bool CalledSequentially() const { return true; }
  void Detach() {}

 private:
  friend class internal::AnnounceOnThread;
  bool IsCurrent() const { return CalledSequentially(); }
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
class RTC_LOCKABLE SequencedTaskChecker : public SequencedTaskCheckerImpl {};
#else
class RTC_LOCKABLE SequencedTaskChecker : public SequencedTaskCheckerDoNothing {
};
#endif  // ENABLE_SEQUENCED_TASK_CHECKER_H_

namespace internal {
class RTC_SCOPED_LOCKABLE SequencedTaskCheckerScope {
 public:
  explicit SequencedTaskCheckerScope(const SequencedTaskChecker* checker)
      RTC_EXCLUSIVE_LOCK_FUNCTION(checker);
  ~SequencedTaskCheckerScope() RTC_UNLOCK_FUNCTION();
};

}  // namespace internal

#define RTC_DCHECK_CALLED_SEQUENTIALLY(x) \
  rtc::internal::SequencedTaskCheckerScope checker(x)

#undef ENABLE_SEQUENCED_TASK_CHECKER

}  // namespace rtc
#endif  // RTC_BASE_SEQUENCED_TASK_CHECKER_H_
