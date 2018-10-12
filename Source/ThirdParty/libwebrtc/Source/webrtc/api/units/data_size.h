/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_UNITS_DATA_SIZE_H_
#define API_UNITS_DATA_SIZE_H_

#ifdef UNIT_TEST
#include <ostream>  // no-presubmit-check TODO(webrtc:8982)
#endif              // UNIT_TEST

#include <stdint.h>
#include <cmath>
#include <limits>
#include <string>
#include <type_traits>

#include "rtc_base/checks.h"
#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {
namespace data_size_impl {
constexpr int64_t kPlusInfinityVal = std::numeric_limits<int64_t>::max();
}  // namespace data_size_impl

// DataSize is a class represeting a count of bytes.
class DataSize {
 public:
  DataSize() = delete;
  static constexpr DataSize Zero() { return DataSize(0); }
  static constexpr DataSize Infinity() {
    return DataSize(data_size_impl::kPlusInfinityVal);
  }
  template <int64_t bytes>
  static constexpr DataSize Bytes() {
    static_assert(bytes >= 0, "");
    static_assert(bytes < data_size_impl::kPlusInfinityVal, "");
    return DataSize(bytes);
  }

  template <
      typename T,
      typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
  static DataSize bytes(T bytes) {
    RTC_DCHECK_GE(bytes, 0);
    RTC_DCHECK_LT(bytes, data_size_impl::kPlusInfinityVal);
    return DataSize(rtc::dchecked_cast<int64_t>(bytes));
  }

  template <typename T,
            typename std::enable_if<std::is_floating_point<T>::value>::type* =
                nullptr>
  static DataSize bytes(T bytes) {
    if (bytes == std::numeric_limits<T>::infinity()) {
      return Infinity();
    } else {
      RTC_DCHECK(!std::isnan(bytes));
      RTC_DCHECK_GE(bytes, 0);
      RTC_DCHECK_LT(bytes, data_size_impl::kPlusInfinityVal);
      return DataSize(rtc::dchecked_cast<int64_t>(bytes));
    }
  }

  template <typename T = int64_t>
  typename std::enable_if<std::is_integral<T>::value, T>::type bytes() const {
    RTC_DCHECK(IsFinite());
    return rtc::dchecked_cast<T>(bytes_);
  }

  template <typename T>
  constexpr typename std::enable_if<std::is_floating_point<T>::value, T>::type
  bytes() const {
    return IsInfinite() ? std::numeric_limits<T>::infinity() : bytes_;
  }

  constexpr int64_t bytes_or(int64_t fallback_value) const {
    return IsFinite() ? bytes_ : fallback_value;
  }

  constexpr bool IsZero() const { return bytes_ == 0; }
  constexpr bool IsInfinite() const {
    return bytes_ == data_size_impl::kPlusInfinityVal;
  }
  constexpr bool IsFinite() const { return !IsInfinite(); }
  DataSize operator-(const DataSize& other) const {
    return DataSize::bytes(bytes() - other.bytes());
  }
  DataSize operator+(const DataSize& other) const {
    return DataSize::bytes(bytes() + other.bytes());
  }
  DataSize& operator-=(const DataSize& other) {
    *this = *this - other;
    return *this;
  }
  DataSize& operator+=(const DataSize& other) {
    *this = *this + other;
    return *this;
  }
  constexpr double operator/(const DataSize& other) const {
    return bytes<double>() / other.bytes<double>();
  }
  constexpr bool operator==(const DataSize& other) const {
    return bytes_ == other.bytes_;
  }
  constexpr bool operator!=(const DataSize& other) const {
    return bytes_ != other.bytes_;
  }
  constexpr bool operator<=(const DataSize& other) const {
    return bytes_ <= other.bytes_;
  }
  constexpr bool operator>=(const DataSize& other) const {
    return bytes_ >= other.bytes_;
  }
  constexpr bool operator>(const DataSize& other) const {
    return bytes_ > other.bytes_;
  }
  constexpr bool operator<(const DataSize& other) const {
    return bytes_ < other.bytes_;
  }

 private:
  explicit constexpr DataSize(int64_t bytes) : bytes_(bytes) {}
  int64_t bytes_;
};

inline DataSize operator*(const DataSize& size, const double& scalar) {
  return DataSize::bytes(std::round(size.bytes() * scalar));
}
inline DataSize operator*(const double& scalar, const DataSize& size) {
  return size * scalar;
}
inline DataSize operator*(const DataSize& size, const int64_t& scalar) {
  return DataSize::bytes(size.bytes() * scalar);
}
inline DataSize operator*(const int64_t& scalar, const DataSize& size) {
  return size * scalar;
}
inline DataSize operator*(const DataSize& size, const int32_t& scalar) {
  return DataSize::bytes(size.bytes() * scalar);
}
inline DataSize operator*(const int32_t& scalar, const DataSize& size) {
  return size * scalar;
}
inline DataSize operator/(const DataSize& size, const int64_t& scalar) {
  return DataSize::bytes(size.bytes() / scalar);
}

std::string ToString(const DataSize& value);

#ifdef UNIT_TEST
inline std::ostream& operator<<(  // no-presubmit-check TODO(webrtc:8982)
    std::ostream& stream,         // no-presubmit-check TODO(webrtc:8982)
    DataSize value) {
  return stream << ToString(value);
}
#endif  // UNIT_TEST

}  // namespace webrtc

#endif  // API_UNITS_DATA_SIZE_H_
