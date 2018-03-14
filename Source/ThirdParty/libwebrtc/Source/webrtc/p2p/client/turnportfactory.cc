/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/client/turnportfactory.h"

#include <memory>

#include "p2p/base/turnport.h"

namespace cricket {

TurnPortFactory::~TurnPortFactory() {
}

std::unique_ptr<Port> TurnPortFactory::Create(
    const CreateRelayPortArgs& args,
    rtc::AsyncPacketSocket* udp_socket) {

  TurnPort* port = TurnPort::Create(
      args.network_thread,
      args.socket_factory,
      args.network,
      udp_socket,
      args.username,
      args.password,
      *args.server_address,
      args.config->credentials,
      args.config->priority,
      args.origin,
      args.turn_customizer);
  port->SetTlsCertPolicy(args.config->tls_cert_policy);
  return std::unique_ptr<Port>(port);
}

std::unique_ptr<Port> TurnPortFactory::Create(
    const CreateRelayPortArgs& args,
    int min_port,
    int max_port) {

  TurnPort* port = TurnPort::Create(
      args.network_thread,
      args.socket_factory,
      args.network,
      min_port,
      max_port,
      args.username,
      args.password,
      *args.server_address,
      args.config->credentials,
      args.config->priority,
      args.origin,
      args.config->tls_alpn_protocols,
      args.config->tls_elliptic_curves,
      args.turn_customizer);
  port->SetTlsCertPolicy(args.config->tls_cert_policy);
  return std::unique_ptr<Port>(port);
}

}  // namespace cricket
