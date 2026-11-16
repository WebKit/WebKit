/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_TEST_STUN_SERVER_H_
#define P2P_TEST_STUN_SERVER_H_

#include <memory>

#include "absl/base/nullability.h"
#include "absl/strings/string_view.h"
#include "api/sequence_checker.h"
#include "api/transport/stun.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/async_udp_socket.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/socket_address.h"

namespace webrtc {

const int STUN_SERVER_PORT = 3478;

class StunServer {
 public:
  // Creates a STUN server, which will listen on the given socket.
  explicit StunServer(absl_nonnull std::unique_ptr<AsyncUDPSocket> socket);
  // Removes the STUN server from the socket and deletes the socket.
  virtual ~StunServer();

 protected:
  // Callback for packets from socket.
  void OnPacket(AsyncPacketSocket* socket, const ReceivedIpPacket& packet);

  // Handlers for the different types of STUN/TURN requests:
  virtual void OnBindingRequest(StunMessage* msg, const SocketAddress& addr);
  void OnAllocateRequest(StunMessage* msg, const SocketAddress& addr);
  void OnSharedSecretRequest(StunMessage* msg, const SocketAddress& addr);
  void OnSendRequest(StunMessage* msg, const SocketAddress& addr);

  // Sends an error response to the given message back to the user.
  void SendErrorResponse(const StunMessage& msg,
                         const SocketAddress& addr,
                         int error_code,
                         absl::string_view error_desc);

  // Sends the given message to the appropriate destination.
  void SendResponse(const StunMessage& msg, const SocketAddress& addr);

  // A helper method to compose a STUN binding response.
  void GetStunBindResponse(StunMessage* message,
                           const SocketAddress& remote_addr,
                           StunMessage* response) const;

 private:
  SequenceChecker sequence_checker_;
  std::unique_ptr<AsyncUDPSocket> socket_;
};

}  //  namespace webrtc


#endif  // P2P_TEST_STUN_SERVER_H_
