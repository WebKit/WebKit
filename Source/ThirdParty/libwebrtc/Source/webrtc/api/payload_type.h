/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_PAYLOAD_TYPE_H_
#define API_PAYLOAD_TYPE_H_

#include <optional>

#include "absl/strings/str_format.h"
#include "rtc_base/strong_alias.h"

namespace webrtc {

class PayloadType : public StrongAlias<class PayloadTypeTag, int> {
 public:
  // The default constructor makes a NotSet.
  PayloadType() : StrongAlias(-1) {}
  // Non-explicit conversions from and to ints are to be deprecated and
  // removed once calling code is upgraded.
  constexpr PayloadType(int pt) : StrongAlias(pt) {  // NOLINT: explicit
    // The number of tests that use invalid values is high enough that
    // this DCHECK can't be deployed yet.
    // Also, allow -1 as argument as a temporary measure. Those calls should
    // eventually be replaced with PayloadType::NotSet() values.
    // Intended check:
    // RTC_DCHECK(pt >= -1 && pt <= 127) << "Payload type " << pt << " is
    // invalid";
  }

  constexpr operator int() const& { return value(); }  // NOLINT: explicit

  // Factory function to create a value if you need to check for
  // values in the valid range.
  static std::optional<PayloadType> Create(int pt) {
    if (pt < 0 || pt > kUpperDynamicRangeMax.value()) {
      return std::nullopt;
    }
    return PayloadType(pt);
  }
  // Factory function for the NotSet value. This should be the only way
  // to create a value outside the valid range.
  static constexpr PayloadType NotSet() { return PayloadType(Internal{}, -1); }

  static const PayloadType kLowerDynamicRangeMin;
  static const PayloadType kLowerDynamicRangeMax;
  static const PayloadType kUpperDynamicRangeMin;
  static const PayloadType kUpperDynamicRangeMax;

  bool Valid(bool rtcp_mux = false) const {
    // A payload type is a 7-bit value in the RTP header, so max = 127.
    // If RTCP multiplexing is used, the numbers from 64 to 95 are reserved
    // for RTCP packets.
    if (rtcp_mux &&
        (*this > kLowerDynamicRangeMax && *this < kUpperDynamicRangeMin)) {
      return false;
    }
    return *this >= 0 && *this <= kUpperDynamicRangeMax;
  }
  // Older interface to validity check.
  static bool IsValid(PayloadType id, bool rtcp_mux) {
    return id.Valid(rtcp_mux);
  }
  bool IsSet() const { return value() >= 0; }

  bool IsDynamic() const {
    return (*this >= kLowerDynamicRangeMin && *this <= kLowerDynamicRangeMax) ||
           (*this >= kUpperDynamicRangeMin && *this <= kUpperDynamicRangeMax);
  }

 private:
  class Internal {};
  // Allow -1 for "NotSet"
  explicit constexpr PayloadType(Internal tag, int pt) : StrongAlias(pt) {}
  template <typename Sink>
  friend void AbslStringify(Sink& sink, const PayloadType pt) {
    absl::Format(&sink, "%d", pt.value());
  }
};

inline constexpr PayloadType PayloadType::kLowerDynamicRangeMin =
    PayloadType(35);
inline constexpr PayloadType PayloadType::kLowerDynamicRangeMax =
    PayloadType(63);
inline constexpr PayloadType PayloadType::kUpperDynamicRangeMin =
    PayloadType(96);
inline constexpr PayloadType PayloadType::kUpperDynamicRangeMax =
    PayloadType(127);

}  // namespace webrtc

#endif  // API_PAYLOAD_TYPE_H_
