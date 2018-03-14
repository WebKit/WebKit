/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef SYSTEM_WRAPPERS_INCLUDE_NTP_TIME_H_
#define SYSTEM_WRAPPERS_INCLUDE_NTP_TIME_H_

#include <stdint.h>

#include "rtc_base/numerics/safe_conversions.h"

namespace webrtc {

class NtpTime {
 public:
  static constexpr uint64_t kFractionsPerSecond = 0x100000000;
  NtpTime() : value_(0) {}
  explicit NtpTime(uint64_t value) : value_(value) {}
  NtpTime(uint32_t seconds, uint32_t fractions)
      : value_(seconds * kFractionsPerSecond + fractions) {}

  NtpTime(const NtpTime&) = default;
  NtpTime& operator=(const NtpTime&) = default;
  explicit operator uint64_t() const { return value_; }

  void Set(uint32_t seconds, uint32_t fractions) {
    value_ = seconds * kFractionsPerSecond + fractions;
  }
  void Reset() { value_ = 0; }

  int64_t ToMs() const {
    static constexpr double kNtpFracPerMs = 4.294967296E6;  // 2^32 / 1000.
    const double frac_ms = static_cast<double>(fractions()) / kNtpFracPerMs;
    return 1000 * static_cast<int64_t>(seconds()) +
           static_cast<int64_t>(frac_ms + 0.5);
  }
  // NTP standard (RFC1305, section 3.1) explicitly state value 0 is invalid.
  bool Valid() const { return value_ != 0; }

  uint32_t seconds() const {
    return rtc::dchecked_cast<uint32_t>(value_ / kFractionsPerSecond);
  }
  uint32_t fractions() const {
    return rtc::dchecked_cast<uint32_t>(value_ % kFractionsPerSecond);
  }

 private:
  uint64_t value_;
};

inline bool operator==(const NtpTime& n1, const NtpTime& n2) {
  return static_cast<uint64_t>(n1) == static_cast<uint64_t>(n2);
}
inline bool operator!=(const NtpTime& n1, const NtpTime& n2) {
  return !(n1 == n2);
}

}  // namespace webrtc
#endif  // SYSTEM_WRAPPERS_INCLUDE_NTP_TIME_H_
