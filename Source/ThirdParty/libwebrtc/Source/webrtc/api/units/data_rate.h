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

#include <limits>
#include <string>
#include <type_traits>

#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "rtc_base/checks.h"
#include "rtc_base/units/unit_base.h"

namespace webrtc {
namespace data_rate_impl {
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
class DataRate final : public rtc_units_impl::RelativeUnit<DataRate> {
 public:
  DataRate() = delete;
  static constexpr DataRate Infinity() { return PlusInfinity(); }
  template <int64_t bps>
  static constexpr DataRate BitsPerSec() {
    return FromStaticValue<bps>();
  }
  template <int64_t kbps>
  static constexpr DataRate KilobitsPerSec() {
    return FromStaticFraction<kbps, 1000>();
  }
  template <typename T>
  static constexpr DataRate bps(T bits_per_second) {
    return FromValue(bits_per_second);
  }
  template <typename T>
  static constexpr DataRate kbps(T kilobits_per_sec) {
    return FromFraction<1000>(kilobits_per_sec);
  }
  template <typename T = int64_t>
  constexpr T bps() const {
    return ToValue<T>();
  }
  template <typename T = int64_t>
  T kbps() const {
    return ToFraction<1000, T>();
  }
  constexpr int64_t bps_or(int64_t fallback_value) const {
    return ToValueOr(fallback_value);
  }
  constexpr int64_t kbps_or(int64_t fallback_value) const {
    return ToFractionOr<1000>(fallback_value);
  }

 private:
  // Bits per second used internally to simplify debugging by making the value
  // more recognizable.
  friend class rtc_units_impl::UnitBase<DataRate>;
  using RelativeUnit::RelativeUnit;
  static constexpr bool one_sided = true;
};

inline DataRate operator/(const DataSize size, const TimeDelta duration) {
  return DataRate::bps(data_rate_impl::Microbits(size) / duration.us());
}
inline TimeDelta operator/(const DataSize size, const DataRate rate) {
  return TimeDelta::us(data_rate_impl::Microbits(size) / rate.bps());
}
inline DataSize operator*(const DataRate rate, const TimeDelta duration) {
  int64_t microbits = rate.bps() * duration.us();
  return DataSize::bytes((microbits + 4000000) / 8000000);
}
inline DataSize operator*(const TimeDelta duration, const DataRate rate) {
  return rate * duration;
}

std::string ToString(DataRate value);

#ifdef UNIT_TEST
inline std::ostream& operator<<(  // no-presubmit-check TODO(webrtc:8982)
    std::ostream& stream,         // no-presubmit-check TODO(webrtc:8982)
    DataRate value) {
  return stream << ToString(value);
}
#endif  // UNIT_TEST

}  // namespace webrtc

#endif  // API_UNITS_DATA_RATE_H_
