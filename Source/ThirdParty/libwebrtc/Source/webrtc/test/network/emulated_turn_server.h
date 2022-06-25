/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_NETWORK_EMULATED_TURN_SERVER_H_
#define TEST_NETWORK_EMULATED_TURN_SERVER_H_

#include <map>
#include <memory>
#include <string>

#include "api/test/network_emulation_manager.h"
#include "api/transport/stun.h"
#include "p2p/base/turn_server.h"
#include "rtc_base/async_packet_socket.h"

namespace webrtc {
namespace test {

// EmulatedTURNServer wraps cricket::TurnServer to be used inside
// a emulated network.
//
// Packets from EmulatedEndpoint (client or peer) are received in
// EmulatedTURNServer::OnPacketReceived which performs a map lookup
// and delivers them into cricket::TurnServer using
// AsyncPacketSocket::SignalReadPacket
//
// Packets from cricket::TurnServer to EmulatedEndpoint are sent into
// using a wrapper around AsyncPacketSocket (no lookup required as the
// wrapper around AsyncPacketSocket keep a pointer to the EmulatedEndpoint).
class EmulatedTURNServer : public EmulatedTURNServerInterface,
                           public cricket::TurnAuthInterface,
                           public webrtc::EmulatedNetworkReceiverInterface {
 public:
  // Create an EmulatedTURNServer.
  // |thread| is a thread that will be used to run cricket::TurnServer
  // that expects all calls to be made from a single thread.
  EmulatedTURNServer(std::unique_ptr<rtc::Thread> thread,
                     EmulatedEndpoint* client,
                     EmulatedEndpoint* peer);
  ~EmulatedTURNServer() override;

  IceServerConfig GetIceServerConfig() const override { return ice_config_; }

  EmulatedEndpoint* GetClientEndpoint() const override { return client_; }

  rtc::SocketAddress GetClientEndpointAddress() const override {
    return client_address_;
  }

  EmulatedEndpoint* GetPeerEndpoint() const override { return peer_; }

  // cricket::TurnAuthInterface
  bool GetKey(const std::string& username,
              const std::string& realm,
              std::string* key) override {
    return cricket::ComputeStunCredentialHash(username, realm, username, key);
  }

  rtc::AsyncPacketSocket* CreatePeerSocket() { return Wrap(peer_); }

  // This method is called by network emulation when a packet
  // comes from an emulated link.
  void OnPacketReceived(webrtc::EmulatedIpPacket packet) override;

  // This is called when the TURN server deletes a socket.
  void Unbind(rtc::SocketAddress address);

  // Unbind all sockets.
  void Stop();

 private:
  std::unique_ptr<rtc::Thread> thread_;
  rtc::SocketAddress client_address_;
  IceServerConfig ice_config_;
  EmulatedEndpoint* const client_;
  EmulatedEndpoint* const peer_;
  std::unique_ptr<cricket::TurnServer> turn_server_ RTC_GUARDED_BY(&thread_);
  std::map<rtc::SocketAddress, rtc::AsyncPacketSocket*> sockets_
      RTC_GUARDED_BY(&thread_);

  // Wraps a EmulatedEndpoint in a AsyncPacketSocket to bridge interaction
  // with TurnServer. cricket::TurnServer gets ownership of the socket.
  rtc::AsyncPacketSocket* Wrap(EmulatedEndpoint* endpoint);
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_NETWORK_EMULATED_TURN_SERVER_H_
