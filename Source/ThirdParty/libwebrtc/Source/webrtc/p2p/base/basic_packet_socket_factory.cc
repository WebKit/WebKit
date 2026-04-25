/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/basic_packet_socket_factory.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "api/async_dns_resolver.h"
#include "api/environment/environment.h"
#include "api/packet_socket_factory.h"
#include "p2p/base/async_stun_tcp_socket.h"
#include "rtc_base/async_dns_resolver.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/async_tcp_socket.h"
#include "rtc_base/async_udp_socket.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_adapters.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/socket_factory.h"
#include "rtc_base/ssl_adapter.h"

namespace webrtc {

BasicPacketSocketFactory::BasicPacketSocketFactory(
    SocketFactory* socket_factory)
    : socket_factory_(socket_factory) {}

BasicPacketSocketFactory::~BasicPacketSocketFactory() {}

std::unique_ptr<AsyncPacketSocket> BasicPacketSocketFactory::CreateUdpSocket(
    const Environment& env,
    const SocketAddress& address,
    uint16_t min_port,
    uint16_t max_port) {
  // UDP sockets are simple.
  std::unique_ptr<Socket> socket =
      socket_factory_->Create(address.family(), SOCK_DGRAM);
  if (!socket) {
    return nullptr;
  }
  if (BindSocket(*socket, address, min_port, max_port) < 0) {
    RTC_LOG(LS_ERROR) << "UDP bind failed with error " << socket->GetError();
    return nullptr;
  }
  return std::make_unique<AsyncUDPSocket>(env, std::move(socket));
}

std::unique_ptr<AsyncPacketSocket>
BasicPacketSocketFactory::CreateClientUdpSocket(
    const Environment& env,
    const SocketAddress& local_address,
    const SocketAddress& remote_address,
    uint16_t min_port,
    uint16_t max_port,
    const PacketSocketTcpOptions& udp_options) {
  std::unique_ptr<Socket> socket =
      socket_factory_->Create(local_address.family(), SOCK_DGRAM);
  if (!socket) {
    return nullptr;
  }
  if (BindSocket(*socket, local_address, min_port, max_port) < 0) {
    RTC_LOG(LS_ERROR) << "UDP bind failed with error " << socket->GetError();
    return nullptr;
  }
  // Assert that at most one DTLS option is used.
  int udp_opts = udp_options.opts & (PacketSocketFactory::OPT_DTLS |
                                     PacketSocketFactory::OPT_DTLS_INSECURE);

  RTC_DCHECK(!((udp_opts & PacketSocketFactory::OPT_DTLS) &&
               (udp_opts & PacketSocketFactory::OPT_DTLS_INSECURE)));
  if ((udp_opts & PacketSocketFactory::OPT_DTLS) ||
      (udp_opts & PacketSocketFactory::OPT_DTLS_INSECURE)) {
    if (socket->Connect(remote_address) < 0) {
      RTC_LOG(LS_ERROR) << "UDP connect failed with error "
                        << socket->GetError();
      return nullptr;
    }
    // Using TLS, wrap the socket in an SSL adapter.
    SSLAdapter* ssl_adapter = SSLAdapter::Create(socket.release(), true);
    if (!ssl_adapter) {
      return nullptr;
    }

    if (udp_opts & PacketSocketFactory::OPT_DTLS_INSECURE) {
      ssl_adapter->SetIgnoreBadCert(true);
    }

    ssl_adapter->SetAlpnProtocols(udp_options.tls_alpn_protocols);
    ssl_adapter->SetEllipticCurves(udp_options.tls_elliptic_curves);
    ssl_adapter->SetCertVerifier(udp_options.tls_cert_verifier);
    socket = absl::WrapUnique(ssl_adapter);
    if (ssl_adapter->StartSSL(remote_address.hostname()) != 0) {
      return nullptr;
    }
  }
  return std::make_unique<AsyncUDPSocket>(env, std::move(socket));
}

std::unique_ptr<AsyncListenSocket>
BasicPacketSocketFactory::CreateServerTcpSocket(
    const Environment& env,
    const SocketAddress& local_address,
    uint16_t min_port,
    uint16_t max_port,
    int opts) {
  // Fail if TLS is required.
  if (opts & PacketSocketFactory::OPT_TLS) {
    RTC_LOG(LS_ERROR) << "TLS support currently is not available.";
    return nullptr;
  }

  if (opts & PacketSocketFactory::OPT_TLS_FAKE) {
    RTC_LOG(LS_ERROR) << "Fake TLS not supported.";
    return nullptr;
  }
  std::unique_ptr<Socket> socket =
      socket_factory_->Create(local_address.family(), SOCK_STREAM);
  if (!socket) {
    return nullptr;
  }

  if (BindSocket(*socket, local_address, min_port, max_port) < 0) {
    RTC_LOG(LS_ERROR) << "TCP bind failed with error " << socket->GetError();
    return nullptr;
  }

  RTC_CHECK(!(opts & PacketSocketFactory::OPT_STUN));

  return std::make_unique<AsyncTcpListenSocket>(env, std::move(socket));
}

std::unique_ptr<AsyncPacketSocket>
BasicPacketSocketFactory::CreateClientTcpSocket(
    const Environment& env,
    const SocketAddress& local_address,
    const SocketAddress& remote_address,
    const PacketSocketTcpOptions& tcp_options) {
  std::unique_ptr<Socket> socket =
      socket_factory_->Create(local_address.family(), SOCK_STREAM);
  if (!socket) {
    return nullptr;
  }

  if (BindSocket(*socket, local_address, 0, 0) < 0) {
    // Allow BindSocket to fail if we're binding to the ANY address, since this
    // is mostly redundant in the first place. The socket will be bound when we
    // call Connect() instead.
    if (local_address.IsAnyIP()) {
      RTC_LOG(LS_WARNING) << "TCP bind failed with error " << socket->GetError()
                          << "; ignoring since socket is using 'any' address.";
    } else {
      RTC_LOG(LS_ERROR) << "TCP bind failed with error " << socket->GetError();
      return nullptr;
    }
  }

  // Set TCP_NODELAY (via OPT_NODELAY) for improved performance; this causes
  // small media packets to be sent immediately rather than being buffered up,
  // reducing latency.
  //
  // Must be done before calling Connect, otherwise it may fail.
  if (socket->SetOption(Socket::OPT_NODELAY, 1) != 0) {
    RTC_LOG(LS_ERROR) << "Setting TCP_NODELAY option failed with error "
                      << socket->GetError();
  }

  // Assert that at most one TLS option is used.
  int tlsOpts = tcp_options.opts & (PacketSocketFactory::OPT_TLS |
                                    PacketSocketFactory::OPT_TLS_FAKE |
                                    PacketSocketFactory::OPT_TLS_INSECURE);
  RTC_DCHECK((tlsOpts & (tlsOpts - 1)) == 0);

  if ((tlsOpts & PacketSocketFactory::OPT_TLS) ||
      (tlsOpts & PacketSocketFactory::OPT_TLS_INSECURE)) {
    // Using TLS, wrap the socket in an SSL adapter.
    std::unique_ptr<SSLAdapter> ssl_adapter =
        absl::WrapUnique(SSLAdapter::Create(socket.release()));
    if (!ssl_adapter) {
      return nullptr;
    }

    if (tlsOpts & PacketSocketFactory::OPT_TLS_INSECURE) {
      ssl_adapter->SetIgnoreBadCert(true);
    }

    ssl_adapter->SetAlpnProtocols(tcp_options.tls_alpn_protocols);
    ssl_adapter->SetEllipticCurves(tcp_options.tls_elliptic_curves);
    ssl_adapter->SetCertVerifier(tcp_options.tls_cert_verifier);

    if (ssl_adapter->StartSSL(remote_address.hostname()) != 0) {
      return nullptr;
    }
    socket = std::move(ssl_adapter);
  } else if (tlsOpts & PacketSocketFactory::OPT_TLS_FAKE) {
    // Using fake TLS, wrap the TCP socket in a pseudo-SSL socket.
    socket = std::make_unique<AsyncSSLSocket>(socket.release());
  }

  if (socket->Connect(remote_address) < 0) {
    RTC_LOG(LS_ERROR) << "TCP connect failed with error " << socket->GetError();
    return nullptr;
  }

  // Finally, wrap that socket in a TCP or STUN TCP packet socket.
  if (tcp_options.opts & PacketSocketFactory::OPT_STUN) {
    return std::make_unique<AsyncStunTCPSocket>(env, std::move(socket));
  } else {
    return std::make_unique<AsyncTCPSocket>(env, std::move(socket));
  }
}

std::unique_ptr<AsyncDnsResolverInterface>
BasicPacketSocketFactory::CreateAsyncDnsResolver() {
  return std::make_unique<AsyncDnsResolver>();
}

int BasicPacketSocketFactory::BindSocket(Socket& socket,
                                         const SocketAddress& local_address,
                                         uint16_t min_port,
                                         uint16_t max_port) {
  int ret = -1;
  if (min_port == 0 && max_port == 0) {
    // If there's no port range, let the OS pick a port for us.
    ret = socket.Bind(local_address);
  } else {
    // Otherwise, try to find a port in the provided range.
    for (int port = min_port; ret < 0 && port <= max_port; ++port) {
      ret = socket.Bind(SocketAddress(local_address.ipaddr(), port));
    }
  }
  return ret;
}

}  // namespace webrtc
