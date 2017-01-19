/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/base/udptransportchannel.h"

#include <string>

#include "webrtc/base/asyncudpsocket.h"
#include "webrtc/base/asyncpacketsocket.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/physicalsocketserver.h"
#include "webrtc/base/socketaddress.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/p2p/base/basicpacketsocketfactory.h"
#include "webrtc/p2p/base/packettransportinterface.h"

namespace cricket {

UdpTransportChannel::UdpTransportChannel(const std::string& transport_name)
    : UdpTransportChannel(transport_name,
                          rtc::Thread::Current()->socketserver()) {}

UdpTransportChannel::UdpTransportChannel(const std::string& transport_name,
                                         rtc::SocketServer* socket_server)
    : transport_name_(transport_name), socket_server_(socket_server) {}

UdpTransportChannel::~UdpTransportChannel() {
  RTC_DCHECK_RUN_ON(&network_thread_checker_);
}

void UdpTransportChannel::OnSocketReadPacket(
    rtc::AsyncPacketSocket* socket,
    const char* data,
    size_t len,
    const rtc::SocketAddress& remote_addr,
    const rtc::PacketTime& packet_time) {
  // No thread_checker in high frequency network function.
  SignalReadPacket(this, data, len, packet_time, 0);
}

void UdpTransportChannel::OnSocketSentPacket(rtc::AsyncPacketSocket* socket,
                                             const rtc::SentPacket& packet) {
  RTC_DCHECK_EQ(socket_.get(), socket);
  SignalSentPacket(this, packet);
}

bool UdpTransportChannel::writable() const {
  return state_ == State::CONNECTED;
}

int UdpTransportChannel::SendPacket(const char* data,
                                    size_t len,
                                    const rtc::PacketOptions& options,
                                    int flags) {
  // No thread_checker in high frequency network function.
  if (!remote_parameters_) {
    LOG(LS_WARNING) << "Remote parameters not set.";
    send_error_ = ENOTCONN;
    return -1;
  }
  const rtc::SocketAddress& remote_addr_ = *remote_parameters_;
  int result = socket_->SendTo((const void*)data, len, remote_addr_, options);
  if (result <= 0) {
    LOG(LS_VERBOSE) << "SendPacket() " << result;
  }
  return result;
}

void UdpTransportChannel::Start() {
  RTC_DCHECK_RUN_ON(&network_thread_checker_);
  if (socket_) {
    LOG(LS_WARNING) << "Local socket already allocated.";
    return;
  }
  static constexpr uint16_t kMaxTries = 100;
  static constexpr uint16_t kMinPortNumber = 2000;
  // TODO(johan) provide configuration option for kMinPortNumber.
  rtc::SocketAddress socket_addr("0.0.0.0", 0);
  // TODO(johan): Replace BasicPacketSocketFactory by something that honors RFC
  // 3550 Section 11 port number requirements like
  // {port_{RTP} is even, port_{RTCP} := port{RTP} + 1}.
  rtc::BasicPacketSocketFactory socket_factory(socket_server_);
  socket_.reset(socket_factory.CreateUdpSocket(socket_addr, kMinPortNumber,
                                               kMinPortNumber + kMaxTries));
  if (socket_) {
    local_parameters_ =
        rtc::Optional<rtc::SocketAddress>(socket_->GetLocalAddress());
    LOG(INFO) << "Created UDP socket with addr " << local_parameters_->ipaddr()
              << " port " << local_parameters_->port() << ".";
    socket_->SignalReadPacket.connect(this,
                                      &UdpTransportChannel::OnSocketReadPacket);
    socket_->SignalSentPacket.connect(this,
                                      &UdpTransportChannel::OnSocketSentPacket);
  } else {
    LOG(INFO) << "Local socket allocation failure";
  }
  UpdateState();
  return;
}

void UdpTransportChannel::UpdateState() {
  RTC_DCHECK_RUN_ON(&network_thread_checker_);
  RTC_DCHECK(!(local_parameters_ && !socket_));
  RTC_DCHECK(!(!local_parameters_ && socket_));
  if (!local_parameters_) {
    SetState(State::INIT);
  } else if (!remote_parameters_) {
    SetState(State::CONNECTING);
  } else {
    SetState(State::CONNECTED);
  }
}

void UdpTransportChannel::SetRemoteParameters(const rtc::SocketAddress& addr) {
  RTC_DCHECK_RUN_ON(&network_thread_checker_);
  if (!addr.IsComplete()) {
    LOG(INFO) << "remote address not complete";
    return;
  }
  // TODO(johan) check for ipv4, other settings.
  remote_parameters_ = rtc::Optional<rtc::SocketAddress>(addr);
  UpdateState();
}

void UdpTransportChannel::SetState(State state) {
  RTC_DCHECK_RUN_ON(&network_thread_checker_);
  if (state_ == state) {
    return;
  }
  state_ = state;
  if (state == State::CONNECTED) {
    SignalWritableState(this);
    SignalReadyToSend(this);
  }
}
}  // namespace cricket
