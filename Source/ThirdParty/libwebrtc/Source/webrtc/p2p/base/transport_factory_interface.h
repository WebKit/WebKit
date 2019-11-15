/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_TRANSPORT_FACTORY_INTERFACE_H_
#define P2P_BASE_TRANSPORT_FACTORY_INTERFACE_H_

#include <memory>
#include <string>

#include "p2p/base/dtls_transport_internal.h"
#include "p2p/base/ice_transport_internal.h"

namespace cricket {

// This interface is used to create DTLS/ICE transports. The external transports
// can be injected into the JsepTransportController through it. For example, the
// FakeIceTransport/FakeDtlsTransport can be injected by setting a
// FakeTransportFactory which implements this interface to the
// JsepTransportController.
class TransportFactoryInterface {
 public:
  virtual ~TransportFactoryInterface() {}

  virtual std::unique_ptr<IceTransportInternal> CreateIceTransport(
      const std::string& transport_name,
      int component) = 0;

  virtual std::unique_ptr<DtlsTransportInternal> CreateDtlsTransport(
      IceTransportInternal* ice,
      const webrtc::CryptoOptions& crypto_options) = 0;
};

}  // namespace cricket

#endif  // P2P_BASE_TRANSPORT_FACTORY_INTERFACE_H_
