/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_UNITS_TIME_DELTA_H_
#define API_UNITS_TIME_DELTA_H_

#ifdef UNIT_TEST
#include <ostream>  // no-presubmit-check TODO(webrtc:8982)
#endif              // UNIT_TEST

#include <cstdlib>
#include <string>
#include <type_traits>

#include "rtc_base/units/unit_base.h"

namespace webrtc {
// TimeDelta represents the difference between two timestamps. Commonly this can
// be a duration. However since two Timestamps are not guaranteed to have the
// same epoch (they might come from different computers, making exact
// synchronisation infeasible), the duration covered by a TimeDelta can be
// undefined. To simplify usage, it can be constructed and converted to
// different units, specifically seconds (s), milliseconds (ms) and
// microseconds (us).
class TimeDelta final : public rtc_units_impl::RelativeUnit<TimeDelta> {
 public:
  TimeDelta() = delete;
  template <int64_t seconds>
  static constexpr TimeDelta Seconds() {
    return FromStaticFraction<seconds, 1000000>();
  }
  template <int64_t ms>
  static constexpr TimeDelta Millis() {
    return FromStaticFraction<ms, 1000>();
  }
  template <int64_t us>
  static constexpr TimeDelta Micros() {
    return FromStaticValue<us>();
  }
  template <typename T>
  static TimeDelta seconds(T seconds) {
    return FromFraction<1000000>(seconds);
  }
  template <typename T>
  static TimeDelta ms(T milliseconds) {
    return FromFraction<1000>(milliseconds);
  }
  template <typename T>
  static TimeDelta us(T microseconds) {
    return FromValue(microseconds);
  }
  template <typename T = int64_t>
  T seconds() const {
    return ToFraction<1000000, T>();
  }
  template <typename T = int64_t>
  T ms() const {
    return ToFraction<1000, T>();
  }
  template <typename T = int64_t>
  T us() const {
    return ToValue<T>();
  }
  template <typename T = int64_t>
  T ns() const {
    return ToMultiple<1000, T>();
  }

  constexpr int64_t seconds_or(int64_t fallback_value) const {
    return ToFractionOr<1000000>(fallback_value);
  }
  constexpr int64_t ms_or(int64_t fallback_value) const {
    return ToFractionOr<1000>(fallback_value);
  }
  constexpr int64_t us_or(int64_t fallback_value) const {
    return ToValueOr(fallback_value);
  }

  TimeDelta Abs() const { return TimeDelta::us(std::abs(us())); }

 private:
  friend class rtc_units_impl::UnitBase<TimeDelta>;
  using RelativeUnit::RelativeUnit;
  static constexpr bool one_sided = false;
};

std::string ToString(TimeDelta value);

#ifdef UNIT_TEST
inline std::ostream& operator<<(  // no-presubmit-check TODO(webrtc:8982)
    std::ostream& stream,         // no-presubmit-check TODO(webrtc:8982)
    TimeDelta value) {
  return stream << ToString(value);
}
#endif  // UNIT_TEST

}  // namespace webrtc

#endif  // API_UNITS_TIME_DELTA_H_
