/*
 *  Copyright 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_RTP_TRANSPORT_FACTORY_H_
#define API_RTP_TRANSPORT_FACTORY_H_

#include <memory>

#include "absl/strings/string_view.h"

namespace webrtc {

class RtpTransport;
class DtlsTransportInternal;

class RtpTransportFactory {
 public:
  virtual ~RtpTransportFactory() = default;
  virtual std::unique_ptr<RtpTransport> CreateRtpTransport(
      absl::string_view transport_name,
      std::unique_ptr<DtlsTransportInternal> rtp_dtls_transport,
      std::unique_ptr<DtlsTransportInternal> rtcp_dtls_transport) = 0;
};

}  // namespace webrtc

#endif  // API_RTP_TRANSPORT_FACTORY_H_
