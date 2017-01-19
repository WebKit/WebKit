/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_RACE_CHECKER_H_
#define WEBRTC_BASE_RACE_CHECKER_H_

#include "webrtc/base/checks.h"
#include "webrtc/base/platform_thread.h"
#include "webrtc/base/thread_annotations.h"

namespace rtc {

namespace internal {
class RaceCheckerScope;
}  // namespace internal

// Best-effort race-checking implementation. This primitive uses no
// synchronization at all to be as-fast-as-possible in the non-racy case.
class LOCKABLE RaceChecker {
 public:
  friend class internal::RaceCheckerScope;
  RaceChecker();

 private:
  bool Acquire() const EXCLUSIVE_LOCK_FUNCTION();
  void Release() const UNLOCK_FUNCTION();

  // Volatile to prevent code being optimized away in Acquire()/Release().
  mutable volatile int access_count_ = 0;
  mutable volatile PlatformThreadRef accessing_thread_;
};

namespace internal {
class SCOPED_LOCKABLE RaceCheckerScope {
 public:
  explicit RaceCheckerScope(const RaceChecker* race_checker)
      EXCLUSIVE_LOCK_FUNCTION(race_checker);

  bool RaceDetected() const;
  ~RaceCheckerScope() UNLOCK_FUNCTION();

 private:
  const RaceChecker* const race_checker_;
  const bool race_check_ok_;
};

class SCOPED_LOCKABLE RaceCheckerScopeDoNothing {
 public:
  explicit RaceCheckerScopeDoNothing(const RaceChecker* race_checker)
      EXCLUSIVE_LOCK_FUNCTION(race_checker) {}

  ~RaceCheckerScopeDoNothing() UNLOCK_FUNCTION() {}
};

}  // namespace internal
}  // namespace rtc

#define RTC_CHECK_RUNS_SERIALIZED(x)               \
  rtc::internal::RaceCheckerScope race_checker(x); \
  RTC_CHECK(!race_checker.RaceDetected())

#if RTC_DCHECK_IS_ON
#define RTC_DCHECK_RUNS_SERIALIZED(x)              \
  rtc::internal::RaceCheckerScope race_checker(x); \
  RTC_DCHECK(!race_checker.RaceDetected())
#else
#define RTC_DCHECK_RUNS_SERIALIZED(x) \
  rtc::internal::RaceCheckerScopeDoNothing race_checker(x)
#endif

#endif  // WEBRTC_BASE_RACE_CHECKER_H_
