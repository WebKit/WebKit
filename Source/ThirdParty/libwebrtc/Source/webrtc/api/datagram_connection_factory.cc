/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/datagram_connection_factory.h"

#include <memory>
#include <utility>

#include "absl/strings/string_view.h"
#include "api/datagram_connection.h"
#include "api/environment/environment.h"
#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "p2p/base/port_allocator.h"
#include "pc/datagram_connection_internal.h"
#include "rtc_base/rtc_certificate.h"

namespace webrtc {

// static
scoped_refptr<DatagramConnection> CreateDatagramConnection(
    const Environment& env,
    std::unique_ptr<PortAllocator> port_allocator,
    absl::string_view transport_name,
    bool ice_controlling,
    scoped_refptr<RTCCertificate> certificate,
    std::unique_ptr<DatagramConnection::Observer> observer,
    DatagramConnection::WireProtocol wire_protocol) {
  return make_ref_counted<DatagramConnectionInternal>(
      env, std::move(port_allocator), transport_name, ice_controlling,
      certificate, std::move(observer), wire_protocol);
}
}  // namespace webrtc
