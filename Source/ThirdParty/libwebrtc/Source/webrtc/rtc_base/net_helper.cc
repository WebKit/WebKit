/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/net_helper.h"

#include <optional>

#include "absl/strings/match.h"
#include "absl/strings/string_view.h"

namespace webrtc {

const char UDP_PROTOCOL_NAME[] = "udp";
const char DTLS_PROTOCOL_NAME[] = "dtls";
const char TCP_PROTOCOL_NAME[] = "tcp";
const char SSLTCP_PROTOCOL_NAME[] = "ssltcp";
const char TLS_PROTOCOL_NAME[] = "tls";

int GetProtocolOverhead(absl::string_view protocol) {
  if (protocol == TCP_PROTOCOL_NAME || protocol == SSLTCP_PROTOCOL_NAME) {
    return kTcpHeaderSize;
  } else if (protocol == UDP_PROTOCOL_NAME) {
    return kUdpHeaderSize;
  } else {
    // TODO(srte): We should crash on unexpected input and handle DTLS and TLS
    // correctly.
    return 8;
  }
}

absl::string_view ProtoToString(ProtocolType proto) {
  switch (proto) {
    case PROTO_UDP:
      return UDP_PROTOCOL_NAME;
    case PROTO_DTLS:
      return DTLS_PROTOCOL_NAME;
    case PROTO_TCP:
      return TCP_PROTOCOL_NAME;
    case PROTO_SSLTCP:
      return SSLTCP_PROTOCOL_NAME;
    case PROTO_TLS:
      return TLS_PROTOCOL_NAME;
  }
}

std::optional<ProtocolType> StringToProto(absl::string_view proto_name) {
  struct {
    ProtocolType type;
    absl::string_view name;
  } const mappings[] = {
      {.type = PROTO_UDP, .name = UDP_PROTOCOL_NAME},
      {.type = PROTO_TCP, .name = TCP_PROTOCOL_NAME},
      {.type = PROTO_SSLTCP, .name = SSLTCP_PROTOCOL_NAME},
      {.type = PROTO_TLS, .name = TLS_PROTOCOL_NAME},
  };
  for (const auto& m : mappings) {
    if (absl::EqualsIgnoreCase(m.name, proto_name)) {
      return m.type;
    }
  }
  return std::nullopt;
}

}  // namespace webrtc
