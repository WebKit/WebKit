/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_RTP_HEADER_EXTENSION_ID_H_
#define API_RTP_HEADER_EXTENSION_ID_H_

#include <cstdint>
#include <optional>
#include <utility>

#include "absl/strings/str_format.h"
#include "rtc_base/checks.h"

namespace webrtc {

// This class represents the ID of an RTP header extension, as
// defined in RFC 8285 section 4.
// It is a number between 1 and 255, and needs to have a consistent
// association with an URI for all RTP packets in an RTP session,
// such as that defined by a BUNDLE.
// We allow the value 0 to mean "not set".
class RtpHeaderExtensionId final {
 public:
  static const RtpHeaderExtensionId kMinId;
  static const RtpHeaderExtensionId kMaxId;
  static const RtpHeaderExtensionId kOneByteHeaderExtensionMaxId;

  // Factory function for the NotSet value.
  static constexpr RtpHeaderExtensionId NotSet() {
    return RtpHeaderExtensionId();
  }

  // Returns `RtpHeaderExtensionId` when id is valid, std::nullopt otherwise.
  // In particular, returns std::nullopt when id is 0.
  static constexpr std::optional<RtpHeaderExtensionId> Create(int id);

  // The default constructor makes a NotSet.
  constexpr RtpHeaderExtensionId() = default;

  constexpr RtpHeaderExtensionId(const RtpHeaderExtensionId&) = default;
  constexpr RtpHeaderExtensionId& operator=(const RtpHeaderExtensionId&) =
      default;

  explicit constexpr RtpHeaderExtensionId(int id)
      : value_(static_cast<uint8_t>(id)) {
    // For convenience allow all valid ids + special value 0 that represents
    // 'NotSet'.
    RTC_DCHECK_GE(id, 0);
    RTC_DCHECK_LE(id, 255);
  }

  constexpr int value() const { return value_; }
  constexpr explicit operator int() const { return value_; }

  constexpr friend bool operator==(const RtpHeaderExtensionId&,
                                   const RtpHeaderExtensionId&) = default;
  constexpr friend auto operator<=>(const RtpHeaderExtensionId&,
                                    const RtpHeaderExtensionId&) = default;

  // Returns true for an extension id that is set and is in the legal range.
  constexpr bool Valid() const {
    return value() >= kMinId.value() && value() <= kMaxId.value();
  }

  constexpr bool IsSet() const { return value() != 0; }

  template <typename Sink>
  friend void AbslStringify(Sink& sink, RtpHeaderExtensionId id) {
    absl::Format(&sink, "%d", id.value());
  }

  template <typename H>
  friend H AbslHashValue(H h, RtpHeaderExtensionId id) {
    return H::combine(std::move(h), id.value());
  }

 private:
  uint8_t value_ = 0;
};

inline constexpr RtpHeaderExtensionId RtpHeaderExtensionId::kMinId =
    RtpHeaderExtensionId(1);
inline constexpr RtpHeaderExtensionId RtpHeaderExtensionId::kMaxId =
    RtpHeaderExtensionId(255);
inline constexpr RtpHeaderExtensionId
    RtpHeaderExtensionId::kOneByteHeaderExtensionMaxId =
        RtpHeaderExtensionId(14);

inline constexpr std::optional<RtpHeaderExtensionId>
RtpHeaderExtensionId::Create(int id) {
  if (id >= kMinId.value() && id <= kMaxId.value()) {
    return RtpHeaderExtensionId(id);
  }
  return std::nullopt;
}

}  // namespace webrtc

#endif  // API_RTP_HEADER_EXTENSION_ID_H_
