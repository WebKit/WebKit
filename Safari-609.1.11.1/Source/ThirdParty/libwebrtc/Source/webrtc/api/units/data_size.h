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

#include <string>
#include <type_traits>

#include "rtc_base/units/unit_base.h"

namespace webrtc {
// DataSize is a class represeting a count of bytes.
class DataSize final : public rtc_units_impl::RelativeUnit<DataSize> {
 public:
  DataSize() = delete;
  static constexpr DataSize Infinity() { return PlusInfinity(); }
  template <int64_t bytes>
  static constexpr DataSize Bytes() {
    return FromStaticValue<bytes>();
  }

  template <typename T>
  static DataSize bytes(T bytes) {
    static_assert(std::is_arithmetic<T>::value, "");
    return FromValue(bytes);
  }
  template <typename T = int64_t>
  T bytes() const {
    return ToValue<T>();
  }

  constexpr int64_t bytes_or(int64_t fallback_value) const {
    return ToValueOr(fallback_value);
  }

 private:
  friend class rtc_units_impl::UnitBase<DataSize>;
  using RelativeUnit::RelativeUnit;
  static constexpr bool one_sided = true;
};

std::string ToString(DataSize value);
inline std::string ToLogString(DataSize value) {
  return ToString(value);
}

#ifdef UNIT_TEST
inline std::ostream& operator<<(  // no-presubmit-check TODO(webrtc:8982)
    std::ostream& stream,         // no-presubmit-check TODO(webrtc:8982)
    DataSize value) {
  return stream << ToString(value);
}
#endif  // UNIT_TEST

}  // namespace webrtc

#endif  // API_UNITS_DATA_SIZE_H_
