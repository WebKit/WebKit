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

#include <stdint.h>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>

#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {
namespace timedelta_impl {
constexpr int64_t kPlusInfinityVal = std::numeric_limits<int64_t>::max();
constexpr int64_t kMinusInfinityVal = std::numeric_limits<int64_t>::min();
}  // namespace timedelta_impl

// TimeDelta represents the difference between two timestamps. Commonly this can
// be a duration. However since two Timestamps are not guaranteed to have the
// same epoch (they might come from different computers, making exact
// synchronisation infeasible), the duration covered by a TimeDelta can be
// undefined. To simplify usage, it can be constructed and converted to
// different units, specifically seconds (s), milliseconds (ms) and
// microseconds (us).
class TimeDelta {
 public:
  TimeDelta() = delete;
  static constexpr TimeDelta Zero() { return TimeDelta(0); }
  static constexpr TimeDelta PlusInfinity() {
    return TimeDelta(timedelta_impl::kPlusInfinityVal);
  }
  static constexpr TimeDelta MinusInfinity() {
    return TimeDelta(timedelta_impl::kMinusInfinityVal);
  }
  template <int64_t seconds>
  static constexpr TimeDelta Seconds() {
    static_assert(seconds > timedelta_impl::kMinusInfinityVal / 1000000, "");
    static_assert(seconds < timedelta_impl::kPlusInfinityVal / 1000000, "");
    return TimeDelta(seconds * 1000000);
  }
  template <int64_t ms>
  static constexpr TimeDelta Millis() {
    static_assert(ms > timedelta_impl::kMinusInfinityVal / 1000, "");
    static_assert(ms < timedelta_impl::kPlusInfinityVal / 1000, "");
    return TimeDelta(ms * 1000);
  }
  template <int64_t us>
  static constexpr TimeDelta Micros() {
    static_assert(us > timedelta_impl::kMinusInfinityVal, "");
    static_assert(us < timedelta_impl::kPlusInfinityVal, "");
    return TimeDelta(us);
  }

  template <
      typename T,
      typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
  static TimeDelta seconds(T seconds) {
    RTC_DCHECK_GT(seconds, timedelta_impl::kMinusInfinityVal / 1000000);
    RTC_DCHECK_LT(seconds, timedelta_impl::kPlusInfinityVal / 1000000);
    return TimeDelta(rtc::dchecked_cast<int64_t>(seconds) * 1000000);
  }
  template <
      typename T,
      typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
  static TimeDelta ms(T milliseconds) {
    RTC_DCHECK_GT(milliseconds, timedelta_impl::kMinusInfinityVal / 1000);
    RTC_DCHECK_LT(milliseconds, timedelta_impl::kPlusInfinityVal / 1000);
    return TimeDelta(rtc::dchecked_cast<int64_t>(milliseconds) * 1000);
  }
  template <
      typename T,
      typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
  static TimeDelta us(T microseconds) {
    RTC_DCHECK_GT(microseconds, timedelta_impl::kMinusInfinityVal);
    RTC_DCHECK_LT(microseconds, timedelta_impl::kPlusInfinityVal);
    return TimeDelta(rtc::dchecked_cast<int64_t>(microseconds));
  }

  template <typename T,
            typename std::enable_if<std::is_floating_point<T>::value>::type* =
                nullptr>
  static TimeDelta seconds(T seconds) {
    return TimeDelta::us(seconds * 1e6);
  }
  template <typename T,
            typename std::enable_if<std::is_floating_point<T>::value>::type* =
                nullptr>
  static TimeDelta ms(T milliseconds) {
    return TimeDelta::us(milliseconds * 1e3);
  }
  template <typename T,
            typename std::enable_if<std::is_floating_point<T>::value>::type* =
                nullptr>
  static TimeDelta us(T microseconds) {
    if (microseconds == std::numeric_limits<T>::infinity()) {
      return PlusInfinity();
    } else if (microseconds == -std::numeric_limits<T>::infinity()) {
      return MinusInfinity();
    } else {
      RTC_DCHECK(!std::isnan(microseconds));
      RTC_DCHECK_GT(microseconds, timedelta_impl::kMinusInfinityVal);
      RTC_DCHECK_LT(microseconds, timedelta_impl::kPlusInfinityVal);
      return TimeDelta(rtc::dchecked_cast<int64_t>(microseconds));
    }
  }

  template <typename T = int64_t>
  typename std::enable_if<std::is_integral<T>::value, T>::type seconds() const {
    RTC_DCHECK(IsFinite());
    return rtc::dchecked_cast<T>(UnsafeSeconds());
  }
  template <typename T = int64_t>
  typename std::enable_if<std::is_integral<T>::value, T>::type ms() const {
    RTC_DCHECK(IsFinite());
    return rtc::dchecked_cast<T>(UnsafeMillis());
  }
  template <typename T = int64_t>
  typename std::enable_if<std::is_integral<T>::value, T>::type us() const {
    RTC_DCHECK(IsFinite());
    return rtc::dchecked_cast<T>(microseconds_);
  }
  template <typename T = int64_t>
  typename std::enable_if<std::is_integral<T>::value, T>::type ns() const {
    RTC_DCHECK_GE(us(), std::numeric_limits<T>::min() / 1000);
    RTC_DCHECK_LE(us(), std::numeric_limits<T>::max() / 1000);
    return rtc::dchecked_cast<T>(us() * 1000);
  }

  template <typename T>
  constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
  seconds() const {
    return us<T>() * 1e-6;
  }
  template <typename T>
  constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
  ms() const {
    return us<T>() * 1e-3;
  }
  template <typename T>
  constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
  us() const {
    return IsPlusInfinity()
               ? std::numeric_limits<T>::infinity()
               : IsMinusInfinity() ? -std::numeric_limits<T>::infinity()
                                   : microseconds_;
  }
  template <typename T>
  constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
  ns() const {
    return us<T>() * 1e3;
  }

  constexpr int64_t seconds_or(int64_t fallback_value) const {
    return IsFinite() ? UnsafeSeconds() : fallback_value;
  }
  constexpr int64_t ms_or(int64_t fallback_value) const {
    return IsFinite() ? UnsafeMillis() : fallback_value;
  }
  constexpr int64_t us_or(int64_t fallback_value) const {
    return IsFinite() ? microseconds_ : fallback_value;
  }

  TimeDelta Abs() const { return TimeDelta::us(std::abs(us())); }
  constexpr bool IsZero() const { return microseconds_ == 0; }
  constexpr bool IsFinite() const { return !IsInfinite(); }
  constexpr bool IsInfinite() const {
    return microseconds_ == timedelta_impl::kPlusInfinityVal ||
           microseconds_ == timedelta_impl::kMinusInfinityVal;
  }
  constexpr bool IsPlusInfinity() const {
    return microseconds_ == timedelta_impl::kPlusInfinityVal;
  }
  constexpr bool IsMinusInfinity() const {
    return microseconds_ == timedelta_impl::kMinusInfinityVal;
  }
  TimeDelta operator+(const TimeDelta& other) const {
    if (IsPlusInfinity() || other.IsPlusInfinity()) {
      RTC_DCHECK(!IsMinusInfinity());
      RTC_DCHECK(!other.IsMinusInfinity());
      return PlusInfinity();
    } else if (IsMinusInfinity() || other.IsMinusInfinity()) {
      RTC_DCHECK(!IsPlusInfinity());
      RTC_DCHECK(!other.IsPlusInfinity());
      return MinusInfinity();
    }
    return TimeDelta::us(us() + other.us());
  }
  TimeDelta operator-(const TimeDelta& other) const {
    if (IsPlusInfinity() || other.IsMinusInfinity()) {
      RTC_DCHECK(!IsMinusInfinity());
      RTC_DCHECK(!other.IsPlusInfinity());
      return PlusInfinity();
    } else if (IsMinusInfinity() || other.IsPlusInfinity()) {
      RTC_DCHECK(!IsPlusInfinity());
      RTC_DCHECK(!other.IsMinusInfinity());
      return MinusInfinity();
    }
    return TimeDelta::us(us() - other.us());
  }
  TimeDelta& operator-=(const TimeDelta& other) {
    *this = *this - other;
    return *this;
  }
  TimeDelta& operator+=(const TimeDelta& other) {
    *this = *this + other;
    return *this;
  }
  constexpr double operator/(const TimeDelta& other) const {
    return us<double>() / other.us<double>();
  }
  constexpr bool operator==(const TimeDelta& other) const {
    return microseconds_ == other.microseconds_;
  }
  constexpr bool operator!=(const TimeDelta& other) const {
    return microseconds_ != other.microseconds_;
  }
  constexpr bool operator<=(const TimeDelta& other) const {
    return microseconds_ <= other.microseconds_;
  }
  constexpr bool operator>=(const TimeDelta& other) const {
    return microseconds_ >= other.microseconds_;
  }
  constexpr bool operator>(const TimeDelta& other) const {
    return microseconds_ > other.microseconds_;
  }
  constexpr bool operator<(const TimeDelta& other) const {
    return microseconds_ < other.microseconds_;
  }

 private:
  explicit constexpr TimeDelta(int64_t us) : microseconds_(us) {}
  constexpr int64_t UnsafeSeconds() const {
    return (microseconds_ + (microseconds_ >= 0 ? 500000 : -500000)) / 1000000;
  }
  constexpr int64_t UnsafeMillis() const {
    return (microseconds_ + (microseconds_ >= 0 ? 500 : -500)) / 1000;
  }
  int64_t microseconds_;
};

inline TimeDelta operator*(const TimeDelta& delta, const double& scalar) {
  return TimeDelta::us(std::round(delta.us() * scalar));
}
inline TimeDelta operator*(const double& scalar, const TimeDelta& delta) {
  return delta * scalar;
}
inline TimeDelta operator*(const TimeDelta& delta, const int64_t& scalar) {
  return TimeDelta::us(delta.us() * scalar);
}
inline TimeDelta operator*(const int64_t& scalar, const TimeDelta& delta) {
  return delta * scalar;
}
inline TimeDelta operator*(const TimeDelta& delta, const int32_t& scalar) {
  return TimeDelta::us(delta.us() * scalar);
}
inline TimeDelta operator*(const int32_t& scalar, const TimeDelta& delta) {
  return delta * scalar;
}

inline TimeDelta operator/(const TimeDelta& delta, const int64_t& scalar) {
  return TimeDelta::us(delta.us() / scalar);
}
std::string ToString(const TimeDelta& value);

#ifdef UNIT_TEST
inline std::ostream& operator<<(  // no-presubmit-check TODO(webrtc:8982)
    std::ostream& stream,         // no-presubmit-check TODO(webrtc:8982)
    TimeDelta value) {
  return stream << ToString(value);
}
#endif  // UNIT_TEST

}  // namespace webrtc

#endif  // API_UNITS_TIME_DELTA_H_
