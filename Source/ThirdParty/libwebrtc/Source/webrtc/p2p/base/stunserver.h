/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_STUNSERVER_H_
#define WEBRTC_P2P_BASE_STUNSERVER_H_

#include <memory>

#include "webrtc/p2p/base/stun.h"
#include "webrtc/base/asyncudpsocket.h"

namespace cricket {

const int STUN_SERVER_PORT = 3478;

class StunServer : public sigslot::has_slots<> {
 public:
  // Creates a STUN server, which will listen on the given socket.
  explicit StunServer(rtc::AsyncUDPSocket* socket);
  // Removes the STUN server from the socket and deletes the socket.
  ~StunServer() override;

 protected:
  // Slot for AsyncSocket.PacketRead:
  void OnPacket(
      rtc::AsyncPacketSocket* socket, const char* buf, size_t size,
      const rtc::SocketAddress& remote_addr,
      const rtc::PacketTime& packet_time);

  // Handlers for the different types of STUN/TURN requests:
  virtual void OnBindingRequest(StunMessage* msg,
      const rtc::SocketAddress& addr);
  void OnAllocateRequest(StunMessage* msg,
      const rtc::SocketAddress& addr);
  void OnSharedSecretRequest(StunMessage* msg,
      const rtc::SocketAddress& addr);
  void OnSendRequest(StunMessage* msg,
      const rtc::SocketAddress& addr);

  // Sends an error response to the given message back to the user.
  void SendErrorResponse(
      const StunMessage& msg, const rtc::SocketAddress& addr,
      int error_code, const char* error_desc);

  // Sends the given message to the appropriate destination.
  void SendResponse(const StunMessage& msg,
       const rtc::SocketAddress& addr);

  // A helper method to compose a STUN binding response.
  void GetStunBindReqponse(StunMessage* request,
                           const rtc::SocketAddress& remote_addr,
                           StunMessage* response) const;

 private:
  std::unique_ptr<rtc::AsyncUDPSocket> socket_;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_STUNSERVER_H_
