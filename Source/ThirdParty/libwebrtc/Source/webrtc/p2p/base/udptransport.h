/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_UDPTRANSPORT_H_
#define WEBRTC_P2P_BASE_UDPTRANSPORT_H_

#include <memory>
#include <string>

#include "webrtc/api/ortc/udptransportinterface.h"
#include "webrtc/base/asyncpacketsocket.h"  // For PacketOptions.
#include "webrtc/base/optional.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/p2p/base/packettransportinternal.h"

namespace rtc {
class AsyncPacketSocket;
struct PacketTime;
struct SentPacket;
class SocketAddress;
}

namespace cricket {

// Implementation of UdpTransportInterface.
// Used by OrtcFactory.
class UdpTransport : public rtc::PacketTransportInternal,
                     public webrtc::UdpTransportInterface {
 public:
  // |transport_name| is only used for identification/logging.
  // |socket| must be non-null.
  UdpTransport(const std::string& transport_name,
               std::unique_ptr<rtc::AsyncPacketSocket> socket);
  ~UdpTransport();

  // Overrides of UdpTransportInterface, used by the API consumer.
  rtc::SocketAddress GetLocalAddress() const override;
  bool SetRemoteAddress(const rtc::SocketAddress& addr) override;
  rtc::SocketAddress GetRemoteAddress() const override;

  // Overrides of PacketTransportInternal, used by webrtc internally.
  std::string debug_name() const override { return transport_name_; }

  bool receiving() const override {
    // TODO(johan): Implement method and signal.
    return true;
  }

  bool writable() const override;

  int SendPacket(const char* data,
                 size_t len,
                 const rtc::PacketOptions& options,
                 int flags) override;

  int SetOption(rtc::Socket::Option opt, int value) override { return 0; }

  int GetError() override { return send_error_; }

 protected:
  PacketTransportInternal* GetInternal() override { return this; }

 private:
  void OnSocketReadPacket(rtc::AsyncPacketSocket* socket,
                          const char* data,
                          size_t len,
                          const rtc::SocketAddress& remote_addr,
                          const rtc::PacketTime& packet_time);
  void OnSocketSentPacket(rtc::AsyncPacketSocket* socket,
                          const rtc::SentPacket& packet);
  bool IsLocalConsistent();
  std::string transport_name_;
  int send_error_ = 0;
  std::unique_ptr<rtc::AsyncPacketSocket> socket_;
  // If not set, will be an "nil" address ("IsNil" returns true).
  rtc::SocketAddress remote_address_;
  rtc::ThreadChecker network_thread_checker_;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_UDPTRANSPORT_H_
