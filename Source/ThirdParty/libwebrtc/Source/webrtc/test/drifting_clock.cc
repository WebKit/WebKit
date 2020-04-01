/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/drifting_clock.h"

#include "rtc_base/checks.h"

namespace webrtc {
namespace test {
constexpr float DriftingClock::kNoDrift;

DriftingClock::DriftingClock(Clock* clock, float speed)
    : clock_(clock), drift_(speed - 1.0f), start_time_(clock_->CurrentTime()) {
  RTC_CHECK(clock);
  RTC_CHECK_GT(speed, 0.0f);
}

TimeDelta DriftingClock::Drift() const {
  auto now = clock_->CurrentTime();
  RTC_DCHECK_GE(now, start_time_);
  return (now - start_time_) * drift_;
}

Timestamp DriftingClock::CurrentTime() {
  return clock_->CurrentTime() + Drift() / 1000.;
}

NtpTime DriftingClock::CurrentNtpTime() {
  // NTP precision is 1/2^32 seconds, i.e. 2^32 ntp fractions = 1 second.
  const double kNtpFracPerMicroSecond = 4294.967296;  // = 2^32 / 10^6

  NtpTime ntp = clock_->CurrentNtpTime();
  uint64_t total_fractions = static_cast<uint64_t>(ntp);
  total_fractions += Drift().us() * kNtpFracPerMicroSecond;
  return NtpTime(total_fractions);
}

int64_t DriftingClock::CurrentNtpInMilliseconds() {
  return clock_->CurrentNtpInMilliseconds() + Drift().ms();
}
}  // namespace test
}  // namespace webrtc
