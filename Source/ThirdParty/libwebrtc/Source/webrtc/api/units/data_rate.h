/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_UNITS_DATA_RATE_H_
#define API_UNITS_DATA_RATE_H_

#ifdef UNIT_TEST
#include <ostream>  // no-presubmit-check TODO(webrtc:8982)
#endif              // UNIT_TEST

#include <stdint.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"

#include "api/units/data_size.h"
#include "api/units/time_delta.h"

namespace webrtc {
namespace data_rate_impl {
constexpr int64_t kPlusInfinityVal = std::numeric_limits<int64_t>::max();

inline int64_t Microbits(const DataSize& size) {
  constexpr int64_t kMaxBeforeConversion =
      std::numeric_limits<int64_t>::max() / 8000000;
  RTC_DCHECK_LE(size.bytes(), kMaxBeforeConversion)
      << "size is too large to be expressed in microbytes";
  return size.bytes() * 8000000;
}
}  // namespace data_rate_impl

// DataRate is a class that represents a given data rate. This can be used to
// represent bandwidth, encoding bitrate, etc. The internal storage is bits per
// second (bps).
class DataRate {
 public:
  DataRate() = delete;
  static constexpr DataRate Zero() { return DataRate(0); }
  static constexpr DataRate Infinity() {
    return DataRate(data_rate_impl::kPlusInfinityVal);
  }
  template <int64_t bps>
  static constexpr DataRate BitsPerSec() {
    static_assert(bps >= 0, "");
    static_assert(bps < data_rate_impl::kPlusInfinityVal, "");
    return DataRate(bps);
  }
  template <int64_t kbps>
  static constexpr DataRate KilobitsPerSec() {
    static_assert(kbps >= 0, "");
    static_assert(kbps < data_rate_impl::kPlusInfinityVal / 1000, "");
    return DataRate(kbps * 1000);
  }

  template <
      typename T,
      typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
  static DataRate bps(T bits_per_second) {
    RTC_DCHECK_GE(bits_per_second, 0);
    RTC_DCHECK_LT(bits_per_second, data_rate_impl::kPlusInfinityVal);
    return DataRate(rtc::dchecked_cast<int64_t>(bits_per_second));
  }
  template <
      typename T,
      typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
  static DataRate kbps(T kilobits_per_sec) {
    RTC_DCHECK_GE(kilobits_per_sec, 0);
    RTC_DCHECK_LT(kilobits_per_sec, data_rate_impl::kPlusInfinityVal / 1000);
    return DataRate::bps(rtc::dchecked_cast<int64_t>(kilobits_per_sec) * 1000);
  }

  template <typename T,
            typename std::enable_if<std::is_floating_point<T>::value>::type* =
                nullptr>
  static DataRate bps(T bits_per_second) {
    if (bits_per_second == std::numeric_limits<T>::infinity()) {
      return Infinity();
    } else {
      RTC_DCHECK(!std::isnan(bits_per_second));
      RTC_DCHECK_GE(bits_per_second, 0);
      RTC_DCHECK_LT(bits_per_second, data_rate_impl::kPlusInfinityVal);
      return DataRate(rtc::dchecked_cast<int64_t>(bits_per_second));
    }
  }
  template <typename T,
            typename std::enable_if<std::is_floating_point<T>::value>::type* =
                nullptr>
  static DataRate kbps(T kilobits_per_sec) {
    return DataRate::bps(kilobits_per_sec * 1e3);
  }

  template <typename T = int64_t>
  typename std::enable_if<std::is_integral<T>::value, T>::type bps() const {
    RTC_DCHECK(IsFinite());
    return rtc::dchecked_cast<T>(bits_per_sec_);
  }
  template <typename T = int64_t>
  typename std::enable_if<std::is_integral<T>::value, T>::type kbps() const {
    RTC_DCHECK(IsFinite());
    return rtc::dchecked_cast<T>(UnsafeKilobitsPerSec());
  }

  template <typename T>
  typename std::enable_if<std::is_floating_point<T>::value,
                          T>::type constexpr bps() const {
    return IsInfinite() ? std::numeric_limits<T>::infinity() : bits_per_sec_;
  }
  template <typename T>
  typename std::enable_if<std::is_floating_point<T>::value,
                          T>::type constexpr kbps() const {
    return bps<T>() * 1e-3;
  }

  constexpr int64_t bps_or(int64_t fallback_value) const {
    return IsFinite() ? bits_per_sec_ : fallback_value;
  }
  constexpr int64_t kbps_or(int64_t fallback_value) const {
    return IsFinite() ? UnsafeKilobitsPerSec() : fallback_value;
  }

  constexpr bool IsZero() const { return bits_per_sec_ == 0; }
  constexpr bool IsInfinite() const {
    return bits_per_sec_ == data_rate_impl::kPlusInfinityVal;
  }
  constexpr bool IsFinite() const { return !IsInfinite(); }
  DataRate Clamped(DataRate min_rate, DataRate max_rate) const {
    return std::max(min_rate, std::min(*this, max_rate));
  }
  void Clamp(DataRate min_rate, DataRate max_rate) {
    *this = Clamped(min_rate, max_rate);
  }
  DataRate operator-(const DataRate& other) const {
    return DataRate::bps(bps() - other.bps());
  }
  DataRate operator+(const DataRate& other) const {
    return DataRate::bps(bps() + other.bps());
  }
  DataRate& operator-=(const DataRate& other) {
    *this = *this - other;
    return *this;
  }
  DataRate& operator+=(const DataRate& other) {
    *this = *this + other;
    return *this;
  }
  constexpr double operator/(const DataRate& other) const {
    return bps<double>() / other.bps<double>();
  }
  constexpr bool operator==(const DataRate& other) const {
    return bits_per_sec_ == other.bits_per_sec_;
  }
  constexpr bool operator!=(const DataRate& other) const {
    return bits_per_sec_ != other.bits_per_sec_;
  }
  constexpr bool operator<=(const DataRate& other) const {
    return bits_per_sec_ <= other.bits_per_sec_;
  }
  constexpr bool operator>=(const DataRate& other) const {
    return bits_per_sec_ >= other.bits_per_sec_;
  }
  constexpr bool operator>(const DataRate& other) const {
    return bits_per_sec_ > other.bits_per_sec_;
  }
  constexpr bool operator<(const DataRate& other) const {
    return bits_per_sec_ < other.bits_per_sec_;
  }

 private:
  // Bits per second used internally to simplify debugging by making the value
  // more recognizable.
  explicit constexpr DataRate(int64_t bits_per_second)
      : bits_per_sec_(bits_per_second) {}
  constexpr int64_t UnsafeKilobitsPerSec() const {
    return (bits_per_sec_ + 500) / 1000;
  }
  int64_t bits_per_sec_;
};

inline DataRate operator*(const DataRate& rate, const double& scalar) {
  return DataRate::bps(std::round(rate.bps() * scalar));
}
inline DataRate operator*(const double& scalar, const DataRate& rate) {
  return rate * scalar;
}
inline DataRate operator*(const DataRate& rate, const int64_t& scalar) {
  return DataRate::bps(rate.bps() * scalar);
}
inline DataRate operator*(const int64_t& scalar, const DataRate& rate) {
  return rate * scalar;
}
inline DataRate operator*(const DataRate& rate, const int32_t& scalar) {
  return DataRate::bps(rate.bps() * scalar);
}
inline DataRate operator*(const int32_t& scalar, const DataRate& rate) {
  return rate * scalar;
}

inline DataRate operator/(const DataSize& size, const TimeDelta& duration) {
  return DataRate::bps(data_rate_impl::Microbits(size) / duration.us());
}
inline TimeDelta operator/(const DataSize& size, const DataRate& rate) {
  return TimeDelta::us(data_rate_impl::Microbits(size) / rate.bps());
}
inline DataSize operator*(const DataRate& rate, const TimeDelta& duration) {
  int64_t microbits = rate.bps() * duration.us();
  return DataSize::bytes((microbits + 4000000) / 8000000);
}
inline DataSize operator*(const TimeDelta& duration, const DataRate& rate) {
  return rate * duration;
}

std::string ToString(const DataRate& value);

#ifdef UNIT_TEST
inline std::ostream& operator<<(  // no-presubmit-check TODO(webrtc:8982)
    std::ostream& stream,         // no-presubmit-check TODO(webrtc:8982)
    DataRate value) {
  return stream << ToString(value);
}
#endif  // UNIT_TEST

}  // namespace webrtc

#endif  // API_UNITS_DATA_RATE_H_
