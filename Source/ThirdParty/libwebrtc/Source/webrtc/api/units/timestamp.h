/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_UNITS_TIMESTAMP_H_
#define API_UNITS_TIMESTAMP_H_

#ifdef UNIT_TEST
#include <ostream>  // no-presubmit-check TODO(webrtc:8982)
#endif              // UNIT_TEST

#include <stdint.h>
#include <limits>
#include <string>

#include "api/units/time_delta.h"
#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {
namespace timestamp_impl {
constexpr int64_t kPlusInfinityVal = std::numeric_limits<int64_t>::max();
constexpr int64_t kMinusInfinityVal = std::numeric_limits<int64_t>::min();
}  // namespace timestamp_impl

// Timestamp represents the time that has passed since some unspecified epoch.
// The epoch is assumed to be before any represented timestamps, this means that
// negative values are not valid. The most notable feature is that the
// difference of two Timestamps results in a TimeDelta.
class Timestamp {
 public:
  Timestamp() = delete;
  static constexpr Timestamp PlusInfinity() {
    return Timestamp(timestamp_impl::kPlusInfinityVal);
  }
  static constexpr Timestamp MinusInfinity() {
    return Timestamp(timestamp_impl::kMinusInfinityVal);
  }
  template <int64_t seconds>
  static constexpr Timestamp Seconds() {
    static_assert(seconds >= 0, "");
    static_assert(seconds < timestamp_impl::kPlusInfinityVal / 1000000, "");
    return Timestamp(seconds * 1000000);
  }
  template <int64_t ms>
  static constexpr Timestamp Millis() {
    static_assert(ms >= 0, "");
    static_assert(ms < timestamp_impl::kPlusInfinityVal / 1000, "");
    return Timestamp(ms * 1000);
  }
  template <int64_t us>
  static constexpr Timestamp Micros() {
    static_assert(us >= 0, "");
    static_assert(us < timestamp_impl::kPlusInfinityVal, "");
    return Timestamp(us);
  }

  template <
      typename T,
      typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
  static Timestamp seconds(T seconds) {
    RTC_DCHECK_GE(seconds, 0);
    RTC_DCHECK_LT(seconds, timestamp_impl::kPlusInfinityVal / 1000000);
    return Timestamp(rtc::dchecked_cast<int64_t>(seconds) * 1000000);
  }

  template <
      typename T,
      typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
  static Timestamp ms(T milliseconds) {
    RTC_DCHECK_GE(milliseconds, 0);
    RTC_DCHECK_LT(milliseconds, timestamp_impl::kPlusInfinityVal / 1000);
    return Timestamp(rtc::dchecked_cast<int64_t>(milliseconds) * 1000);
  }

  template <
      typename T,
      typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
  static Timestamp us(T microseconds) {
    RTC_DCHECK_GE(microseconds, 0);
    RTC_DCHECK_LT(microseconds, timestamp_impl::kPlusInfinityVal);
    return Timestamp(rtc::dchecked_cast<int64_t>(microseconds));
  }

  template <typename T,
            typename std::enable_if<std::is_floating_point<T>::value>::type* =
                nullptr>
  static Timestamp seconds(T seconds) {
    return Timestamp::us(seconds * 1e6);
  }

  template <typename T,
            typename std::enable_if<std::is_floating_point<T>::value>::type* =
                nullptr>
  static Timestamp ms(T milliseconds) {
    return Timestamp::us(milliseconds * 1e3);
  }
  template <typename T,
            typename std::enable_if<std::is_floating_point<T>::value>::type* =
                nullptr>
  static Timestamp us(T microseconds) {
    if (microseconds == std::numeric_limits<double>::infinity()) {
      return PlusInfinity();
    } else if (microseconds == -std::numeric_limits<double>::infinity()) {
      return MinusInfinity();
    } else {
      RTC_DCHECK(!std::isnan(microseconds));
      RTC_DCHECK_GE(microseconds, 0);
      RTC_DCHECK_LT(microseconds, timestamp_impl::kPlusInfinityVal);
      return Timestamp(rtc::dchecked_cast<int64_t>(microseconds));
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

  constexpr int64_t seconds_or(int64_t fallback_value) const {
    return IsFinite() ? UnsafeSeconds() : fallback_value;
  }
  constexpr int64_t ms_or(int64_t fallback_value) const {
    return IsFinite() ? UnsafeMillis() : fallback_value;
  }
  constexpr int64_t us_or(int64_t fallback_value) const {
    return IsFinite() ? microseconds_ : fallback_value;
  }

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
  Timestamp operator+(const TimeDelta& other) const {
    if (IsPlusInfinity() || other.IsPlusInfinity()) {
      RTC_DCHECK(!IsMinusInfinity());
      RTC_DCHECK(!other.IsMinusInfinity());
      return PlusInfinity();
    } else if (IsMinusInfinity() || other.IsMinusInfinity()) {
      RTC_DCHECK(!IsPlusInfinity());
      RTC_DCHECK(!other.IsPlusInfinity());
      return MinusInfinity();
    }
    return Timestamp::us(us() + other.us());
  }
  Timestamp operator-(const TimeDelta& other) const {
    if (IsPlusInfinity() || other.IsMinusInfinity()) {
      RTC_DCHECK(!IsMinusInfinity());
      RTC_DCHECK(!other.IsPlusInfinity());
      return PlusInfinity();
    } else if (IsMinusInfinity() || other.IsPlusInfinity()) {
      RTC_DCHECK(!IsPlusInfinity());
      RTC_DCHECK(!other.IsMinusInfinity());
      return MinusInfinity();
    }
    return Timestamp::us(us() - other.us());
  }
  TimeDelta operator-(const Timestamp& other) const {
    if (IsPlusInfinity() || other.IsMinusInfinity()) {
      RTC_DCHECK(!IsMinusInfinity());
      RTC_DCHECK(!other.IsPlusInfinity());
      return TimeDelta::PlusInfinity();
    } else if (IsMinusInfinity() || other.IsPlusInfinity()) {
      RTC_DCHECK(!IsPlusInfinity());
      RTC_DCHECK(!other.IsMinusInfinity());
      return TimeDelta::MinusInfinity();
    }
    return TimeDelta::us(us() - other.us());
  }
  Timestamp& operator-=(const TimeDelta& other) {
    *this = *this - other;
    return *this;
  }
  Timestamp& operator+=(const TimeDelta& other) {
    *this = *this + other;
    return *this;
  }
  constexpr bool operator==(const Timestamp& other) const {
    return microseconds_ == other.microseconds_;
  }
  constexpr bool operator!=(const Timestamp& other) const {
    return microseconds_ != other.microseconds_;
  }
  constexpr bool operator<=(const Timestamp& other) const {
    return microseconds_ <= other.microseconds_;
  }
  constexpr bool operator>=(const Timestamp& other) const {
    return microseconds_ >= other.microseconds_;
  }
  constexpr bool operator>(const Timestamp& other) const {
    return microseconds_ > other.microseconds_;
  }
  constexpr bool operator<(const Timestamp& other) const {
    return microseconds_ < other.microseconds_;
  }

 private:
  explicit constexpr Timestamp(int64_t us) : microseconds_(us) {}
  constexpr int64_t UnsafeSeconds() const {
    return (microseconds_ + 500000) / 1000000;
  }
  constexpr int64_t UnsafeMillis() const {
    return (microseconds_ + 500) / 1000;
  }
  int64_t microseconds_;
};

std::string ToString(const Timestamp& value);

#ifdef UNIT_TEST
inline std::ostream& operator<<(  // no-presubmit-check TODO(webrtc:8982)
    std::ostream& stream,         // no-presubmit-check TODO(webrtc:8982)
    Timestamp value) {
  return stream << ToString(value);
}
#endif  // UNIT_TEST

}  // namespace webrtc

#endif  // API_UNITS_TIMESTAMP_H_
