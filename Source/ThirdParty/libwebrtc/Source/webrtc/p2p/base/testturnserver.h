/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_TESTTURNSERVER_H_
#define WEBRTC_P2P_BASE_TESTTURNSERVER_H_

#include <string>
#include <vector>

#include "webrtc/p2p/base/basicpacketsocketfactory.h"
#include "webrtc/p2p/base/stun.h"
#include "webrtc/p2p/base/turnserver.h"
#include "webrtc/base/asyncudpsocket.h"
#include "webrtc/base/thread.h"

namespace cricket {

static const char kTestRealm[] = "example.org";
static const char kTestSoftware[] = "TestTurnServer";

class TestTurnRedirector : public TurnRedirectInterface {
 public:
  explicit TestTurnRedirector(const std::vector<rtc::SocketAddress>& addresses)
      : alternate_server_addresses_(addresses),
        iter_(alternate_server_addresses_.begin()) {
  }

  virtual bool ShouldRedirect(const rtc::SocketAddress&,
                              rtc::SocketAddress* out) {
    if (!out || iter_ == alternate_server_addresses_.end()) {
      return false;
    }
    *out = *iter_++;
    return true;
  }

 private:
  const std::vector<rtc::SocketAddress>& alternate_server_addresses_;
  std::vector<rtc::SocketAddress>::const_iterator iter_;
};

class TestTurnServer : public TurnAuthInterface {
 public:
  TestTurnServer(rtc::Thread* thread,
                 const rtc::SocketAddress& int_addr,
                 const rtc::SocketAddress& udp_ext_addr,
                 ProtocolType int_protocol = PROTO_UDP)
      : server_(thread), thread_(thread) {
    AddInternalSocket(int_addr, int_protocol);
    server_.SetExternalSocketFactory(new rtc::BasicPacketSocketFactory(thread),
                                     udp_ext_addr);
    server_.set_realm(kTestRealm);
    server_.set_software(kTestSoftware);
    server_.set_auth_hook(this);
  }

  void set_enable_otu_nonce(bool enable) {
    server_.set_enable_otu_nonce(enable);
  }

  TurnServer* server() { return &server_; }

  void set_redirect_hook(TurnRedirectInterface* redirect_hook) {
    server_.set_redirect_hook(redirect_hook);
  }

  void set_enable_permission_checks(bool enable) {
    server_.set_enable_permission_checks(enable);
  }

  void AddInternalSocket(const rtc::SocketAddress& int_addr,
                         ProtocolType proto) {
    if (proto == cricket::PROTO_UDP) {
      server_.AddInternalSocket(
          rtc::AsyncUDPSocket::Create(thread_->socketserver(), int_addr),
          proto);
    } else if (proto == cricket::PROTO_TCP) {
      // For TCP we need to create a server socket which can listen for incoming
      // new connections.
      rtc::AsyncSocket* socket =
          thread_->socketserver()->CreateAsyncSocket(SOCK_STREAM);
      socket->Bind(int_addr);
      socket->Listen(5);
      server_.AddInternalServerSocket(socket, proto);
    }
  }

  // Finds the first allocation in the server allocation map with a source
  // ip and port matching the socket address provided.
  TurnServerAllocation* FindAllocation(const rtc::SocketAddress& src) {
    const TurnServer::AllocationMap& map = server_.allocations();
    for (TurnServer::AllocationMap::const_iterator it = map.begin();
        it != map.end(); ++it) {
      if (src == it->first.src()) {
        return it->second.get();
      }
    }
    return NULL;
  }

 private:
  // For this test server, succeed if the password is the same as the username.
  // Obviously, do not use this in a production environment.
  virtual bool GetKey(const std::string& username, const std::string& realm,
                      std::string* key) {
    return ComputeStunCredentialHash(username, realm, username, key);
  }

  TurnServer server_;
  rtc::Thread* thread_;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_TESTTURNSERVER_H_
