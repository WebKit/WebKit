/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_TEST_DRIFTING_CLOCK_H_
#define WEBRTC_TEST_DRIFTING_CLOCK_H_

#include "webrtc/system_wrappers/include/clock.h"

namespace webrtc {
namespace test {
class DriftingClock : public Clock {
 public:
  // TODO(danilchap): Make this constants constexpr when it would be supported.
  static const float kDoubleSpeed;  // 2.0f;
  static const float kNoDrift;      // 1.0f;
  static const float kHalfSpeed;    // 0.5f;

  DriftingClock(Clock* clock, float speed);

  // TODO(danilchap): Make this functions constexpr when it would be supported.
  static float PercentsFaster(float percent) { return 1.0f + percent / 100.0f; }
  static float PercentsSlower(float percent) { return 1.0f - percent / 100.0f; }

  int64_t TimeInMilliseconds() const override;
  int64_t TimeInMicroseconds() const override;
  void CurrentNtp(uint32_t& seconds, uint32_t& fractions) const override;
  int64_t CurrentNtpInMilliseconds() const override;

 private:
  float Drift() const;

  Clock* const clock_;
  const float drift_;
  const int64_t start_time_;
};
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_DRIFTING_CLOCK_H_
