/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef NET_DCSCTP_SOCKET_CAPABILITIES_H_
#define NET_DCSCTP_SOCKET_CAPABILITIES_H_

#include <algorithm>
#include <cstdint>

#include "net/dcsctp/public/dcsctp_options.h"
#include "net/dcsctp/public/types.h"

namespace dcsctp {
// Indicates what the association supports, meaning that both parties
// support it and that feature can be used.
struct Capabilities {
  // RFC3758 Partial Reliability Extension
  bool partial_reliability = false;
  // RFC8260 Stream Schedulers and User Message Interleaving
  bool message_interleaving = false;
  // RFC6525 Stream Reconfiguration
  bool reconfig = false;
  // https://datatracker.ietf.org/doc/draft-ietf-tsvwg-sctp-zero-checksum/
  ZeroChecksumAlternateErrorDetectionMethod zero_checksum_method =
      ZeroChecksumAlternateErrorDetectionMethod::None();
  // Negotiated maximum incoming and outgoing stream count.
  uint16_t negotiated_maximum_incoming_streams = 0;
  uint16_t negotiated_maximum_outgoing_streams = 0;

  bool zero_checksum_enabled() const {
    return zero_checksum_method !=
           ZeroChecksumAlternateErrorDetectionMethod::None();
  }

  Capabilities Negotiate(const DcSctpOptions& options) const {
    Capabilities negotiated;
    negotiated.partial_reliability =
        partial_reliability && options.enable_partial_reliability;
    negotiated.message_interleaving =
        message_interleaving && options.enable_message_interleaving;
    negotiated.reconfig = reconfig;
    if (zero_checksum_method ==
        options.zero_checksum_alternate_error_detection_method) {
      negotiated.zero_checksum_method = zero_checksum_method;
    } else {
      negotiated.zero_checksum_method =
          ZeroChecksumAlternateErrorDetectionMethod::None();
    }
    negotiated.negotiated_maximum_incoming_streams =
        std::min(options.announced_maximum_incoming_streams,
                 negotiated_maximum_incoming_streams);
    negotiated.negotiated_maximum_outgoing_streams =
        std::min(options.announced_maximum_outgoing_streams,
                 negotiated_maximum_outgoing_streams);
    return negotiated;
  }
};
}  // namespace dcsctp

#endif  // NET_DCSCTP_SOCKET_CAPABILITIES_H_
