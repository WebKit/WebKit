/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_BASE_CLOCK_ALIGNER_H_
#define RTC_BASE_CLOCK_ALIGNER_H_

#include <optional>

#include "api/environment/environment.h"
#include "api/sequence_checker.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/system/no_unique_address.h"
#include "rtc_base/thread_annotations.h"

namespace webrtc {

// Aligns timestamps from a non-monotonic or uncoordinated clock (such as socket
// receive timestamps) to the monotonic clock provided by `env.clock()`.
//
// The aligner assumes:
// - Callers invoke `Align()` in chronological order of events occurring in the
//   near past relative to the monotonic clock.
// - Input timestamps represent increasing time points, but values may
//   occasionally jump backward (e.g. due to system wall-clock adjustments or
//   NTP sync) or drift relative to the monotonic clock.
class ClockAligner {
 public:
  explicit ClockAligner(const Environment& env);

  // Calculates the monotonic time corresponding to `time`, aligned with
  // `env_.clock().CurrentTime()`.
  //
  // `time` does not need to share the same epoch as `env_.clock()` and may
  // originate from an uncoordinated clock (such as CLOCK_REALTIME).
  //
  // Handling non-monotonic clocks (such as backward clock steps or clock drift)
  // is enabled unless the field trial "WebRTC-ClockAligner" is disabled.
  Timestamp Align(Timestamp time);

 private:
  Timestamp AlignAssumingMonotonicClock(Timestamp time, Timestamp current_time)
      RTC_RUN_ON(&sequence_checker_);
  Timestamp AlignNonMonotonicClock(Timestamp time, Timestamp current_time)
      RTC_RUN_ON(&sequence_checker_);

  const Environment env_;
  const bool disable_non_monotonic_clock_fix_;

  RTC_NO_UNIQUE_ADDRESS SequenceChecker sequence_checker_{
      SequenceChecker::kDetached};
  std::optional<TimeDelta> time_offset_ RTC_GUARDED_BY(sequence_checker_);
  std::optional<Timestamp> last_current_time_ RTC_GUARDED_BY(sequence_checker_);
  std::optional<Timestamp> last_time_ RTC_GUARDED_BY(sequence_checker_);
};

}  // namespace webrtc

#endif  // RTC_BASE_CLOCK_ALIGNER_H_
