/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/network/emulated_turn_server.h"

#include <string>
#include <utility>

#include "api/packet_socket_factory.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/task_queue_for_test.h"

namespace {

static const char kTestRealm[] = "example.org";
static const char kTestSoftware[] = "TestTurnServer";

// A wrapper class for cricket::TurnServer to allocate sockets.
class PacketSocketFactoryWrapper : public rtc::PacketSocketFactory {
 public:
  explicit PacketSocketFactoryWrapper(
      webrtc::test::EmulatedTURNServer* turn_server)
      : turn_server_(turn_server) {}
  ~PacketSocketFactoryWrapper() override {}

  // This method is called from TurnServer when making a TURN ALLOCATION.
  // It will create a socket on the `peer_` endpoint.
  rtc::AsyncPacketSocket* CreateUdpSocket(const rtc::SocketAddress& address,
                                          uint16_t min_port,
                                          uint16_t max_port) override {
    return turn_server_->CreatePeerSocket();
  }

  rtc::AsyncListenSocket* CreateServerTcpSocket(
      const rtc::SocketAddress& local_address,
      uint16_t min_port,
      uint16_t max_port,
      int opts) override {
    return nullptr;
  }
  rtc::AsyncPacketSocket* CreateClientTcpSocket(
      const rtc::SocketAddress& local_address,
      const rtc::SocketAddress& remote_address,
      const rtc::PacketSocketTcpOptions& tcp_options) override {
    return nullptr;
  }
  std::unique_ptr<webrtc::AsyncDnsResolverInterface> CreateAsyncDnsResolver()
      override {
    return nullptr;
  }

 private:
  webrtc::test::EmulatedTURNServer* turn_server_;
};

}  //  namespace

namespace webrtc {
namespace test {

// A wrapper class for copying data between an AsyncPacketSocket and a
// EmulatedEndpoint. This is used by the cricket::TurnServer when
// sending data back into the emulated network.
class EmulatedTURNServer::AsyncPacketSocketWrapper
    : public rtc::AsyncPacketSocket {
 public:
  AsyncPacketSocketWrapper(webrtc::test::EmulatedTURNServer* turn_server,
                           webrtc::EmulatedEndpoint* endpoint,
                           uint16_t port)
      : turn_server_(turn_server),
        endpoint_(endpoint),
        local_address_(
            rtc::SocketAddress(endpoint_->GetPeerLocalAddress(), port)) {}
  ~AsyncPacketSocketWrapper() { turn_server_->Unbind(local_address_); }

  rtc::SocketAddress GetLocalAddress() const override { return local_address_; }
  rtc::SocketAddress GetRemoteAddress() const override {
    return rtc::SocketAddress();
  }
  int Send(const void* pv,
           size_t cb,
           const rtc::PacketOptions& options) override {
    RTC_CHECK(false) << "TCP not implemented";
    return -1;
  }
  int SendTo(const void* pv,
             size_t cb,
             const rtc::SocketAddress& addr,
             const rtc::PacketOptions& options) override {
    // Copy from rtc::AsyncPacketSocket to EmulatedEndpoint.
    rtc::CopyOnWriteBuffer buf(reinterpret_cast<const char*>(pv), cb);
    endpoint_->SendPacket(local_address_, addr, buf);
    return cb;
  }
  int Close() override { return 0; }
  void NotifyPacketReceived(const rtc::ReceivedPacket& packet) {
    rtc::AsyncPacketSocket::NotifyPacketReceived(packet);
  }

  rtc::AsyncPacketSocket::State GetState() const override {
    return rtc::AsyncPacketSocket::STATE_BOUND;
  }
  int GetOption(rtc::Socket::Option opt, int* value) override { return 0; }
  int SetOption(rtc::Socket::Option opt, int value) override { return 0; }
  int GetError() const override { return 0; }
  void SetError(int error) override {}

 private:
  webrtc::test::EmulatedTURNServer* const turn_server_;
  webrtc::EmulatedEndpoint* const endpoint_;
  const rtc::SocketAddress local_address_;
};

EmulatedTURNServer::EmulatedTURNServer(std::unique_ptr<rtc::Thread> thread,
                                       EmulatedEndpoint* client,
                                       EmulatedEndpoint* peer)
    : thread_(std::move(thread)), client_(client), peer_(peer) {
  ice_config_.username = "keso";
  ice_config_.password = "keso";
  SendTask(thread_.get(), [this]() {
    RTC_DCHECK_RUN_ON(thread_.get());
    turn_server_ = std::make_unique<cricket::TurnServer>(thread_.get());
    turn_server_->set_realm(kTestRealm);
    turn_server_->set_realm(kTestSoftware);
    turn_server_->set_auth_hook(this);

    auto client_socket = Wrap(client_);
    turn_server_->AddInternalSocket(client_socket, cricket::PROTO_UDP);
    turn_server_->SetExternalSocketFactory(new PacketSocketFactoryWrapper(this),
                                           rtc::SocketAddress());
    client_address_ = client_socket->GetLocalAddress();
    char buf[256];
    rtc::SimpleStringBuilder str(buf);
    str.AppendFormat("turn:%s?transport=udp",
                     client_address_.ToString().c_str());
    ice_config_.url = str.str();
  });
}

void EmulatedTURNServer::Stop() {
  SendTask(thread_.get(), [this]() {
    RTC_DCHECK_RUN_ON(thread_.get());
    sockets_.clear();
  });
}

EmulatedTURNServer::~EmulatedTURNServer() {
  SendTask(thread_.get(), [this]() {
    RTC_DCHECK_RUN_ON(thread_.get());
    turn_server_.reset(nullptr);
  });
}

rtc::AsyncPacketSocket* EmulatedTURNServer::Wrap(EmulatedEndpoint* endpoint) {
  RTC_DCHECK_RUN_ON(thread_.get());
  auto port = endpoint->BindReceiver(0, this).value();
  auto socket = new AsyncPacketSocketWrapper(this, endpoint, port);
  sockets_[rtc::SocketAddress(endpoint->GetPeerLocalAddress(), port)] = socket;
  return socket;
}

void EmulatedTURNServer::OnPacketReceived(webrtc::EmulatedIpPacket packet) {
  // Copy from EmulatedEndpoint to rtc::AsyncPacketSocket.
  thread_->PostTask([this, packet(std::move(packet))]() {
    RTC_DCHECK_RUN_ON(thread_.get());
    auto it = sockets_.find(packet.to);
    if (it != sockets_.end()) {
      it->second->NotifyPacketReceived(
          rtc::ReceivedPacket(packet.data, packet.from, packet.arrival_time));
    }
  });
}

void EmulatedTURNServer::Unbind(rtc::SocketAddress address) {
  RTC_DCHECK_RUN_ON(thread_.get());
  if (GetClientEndpoint()->GetPeerLocalAddress() == address.ipaddr()) {
    GetClientEndpoint()->UnbindReceiver(address.port());
  } else {
    GetPeerEndpoint()->UnbindReceiver(address.port());
  }
  sockets_.erase(address);
}

}  // namespace test
}  // namespace webrtc
