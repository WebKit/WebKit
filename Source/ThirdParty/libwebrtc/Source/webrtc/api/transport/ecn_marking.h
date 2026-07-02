/*
 *  Copyright 2024 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TRANSPORT_ECN_MARKING_H_
#define API_TRANSPORT_ECN_MARKING_H_

#include <cstdint>

#include "absl/strings/string_view.h"

namespace webrtc {

// TODO: bugs.webrtc.org/42225697 - L4S support is slowly being developed.
// Help is appreciated.

// L4S Explicit Congestion Notification (ECN) .
// https://www.rfc-editor.org/rfc/rfc9331.html ECT stands for ECN-Capable
// Transport and CE stands for Congestion Experienced.

// https://www.rfc-editor.org/rfc/rfc3168.html#section-5
// +-----+-----+
// | ECN FIELD |
// +-----+-----+
//   ECT   CE         [Obsolete] RFC 2481 names for the ECN bits.
//    0     0         Not-ECT
//    0     1         ECT(1)
//    1     0         ECT(0)
//    1     1         CE

enum class EcnMarking : uint8_t {
  kNotEct = 0b00,  // Not ECN-Capable Transport
  kEct1 = 0b01,    // ECN-Capable Transport
  kEct0 = 0b10,    // Not used by L4S (or webrtc.)
  kCe = 0b11,      // Congestion experienced
};

inline absl::string_view AsString(EcnMarking marking) {
  switch (marking) {
    case EcnMarking::kNotEct:
      return "none";
    case EcnMarking::kEct1:
      return "ect1";
    case EcnMarking::kEct0:
      return "ect0";
    case EcnMarking::kCe:
      return "ce";
    default:
      return "unknown";
  }
}

template <typename Sink>
void AbslStringify(Sink& sink, EcnMarking self) {
  sink.Append(AsString(self));
}

}  // namespace webrtc

#endif  // API_TRANSPORT_ECN_MARKING_H_
