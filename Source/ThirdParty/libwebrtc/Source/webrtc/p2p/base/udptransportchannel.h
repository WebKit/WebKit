/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_UDPTRANSPORTCHANNEL_H_
#define WEBRTC_P2P_BASE_UDPTRANSPORTCHANNEL_H_

#include <memory>
#include <string>

#include "webrtc/base/optional.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/p2p/base/packettransportinterface.h"

namespace rtc {
class AsyncPacketSocket;
class PhysicalSocketServer;
class SocketAddress;
class SocketServer;
class Thread;
}

namespace cricket {

class UdpTransportChannel : public rtc::PacketTransportInterface {
 public:
  enum class State { INIT, CONNECTING, CONNECTED, FAILED };
  explicit UdpTransportChannel(const std::string& transport_name);
  UdpTransportChannel(const std::string& transport_name, rtc::SocketServer* ss);
  ~UdpTransportChannel();

  const std::string debug_name() const override { return transport_name_; }

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

  State state() const {
    RTC_DCHECK_RUN_ON(&network_thread_checker_);
    return state_;
  }

  // Start() makes UdpTransportChannel transition from state INIT to CONNECTING.
  // It creates the local UDP socket and binds it to a port.
  // Consider checking state() after calling Start().
  void Start();

  void SetRemoteParameters(const rtc::SocketAddress& remote);

  // Returned optional does not have a value if in the INIT or FAILED state.
  // Consider checking state() before calling local_parameters().
  rtc::Optional<rtc::SocketAddress> local_parameters() const {
    return local_parameters_;
  }

 private:
  void OnSocketReadPacket(rtc::AsyncPacketSocket* socket,
                          const char* data,
                          size_t len,
                          const rtc::SocketAddress& remote_addr,
                          const rtc::PacketTime& packet_time);
  void OnSocketSentPacket(rtc::AsyncPacketSocket* socket,
                          const rtc::SentPacket& packet);
  void SetState(State state);  // Set State and Signal.
  bool IsLocalConsistent();
  void UpdateState();
  std::string transport_name_;
  rtc::SocketServer* socket_server_;
  State state_ = State::INIT;
  int send_error_ = 0;
  std::unique_ptr<rtc::AsyncPacketSocket> socket_;
  rtc::Optional<rtc::SocketAddress> local_parameters_;
  rtc::Optional<rtc::SocketAddress> remote_parameters_;
  rtc::ThreadChecker network_thread_checker_;
};
}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_UDPTRANSPORTCHANNEL_H_
