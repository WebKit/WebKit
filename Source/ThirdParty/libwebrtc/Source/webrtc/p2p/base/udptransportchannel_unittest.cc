/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <list>
#include <memory>
#include <utility>
#include <vector>

#include "webrtc/base/gunit.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/asyncpacketsocket.h"
#include "webrtc/base/ipaddress.h"
#include "webrtc/base/physicalsocketserver.h"
#include "webrtc/base/socketaddress.h"
#include "webrtc/base/socketserver.h"
#include "webrtc/base/virtualsocketserver.h"
#include "webrtc/p2p/base/packettransportinterface.h"
#include "webrtc/p2p/base/udptransportchannel.h"

namespace cricket {

constexpr int kTimeoutMs = 10000;
static const rtc::IPAddress kIPv4LocalHostAddress =
    rtc::IPAddress(0x7F000001);  // 127.0.0.1

class UdpTransportChannelTest : public testing::Test,
                                public sigslot::has_slots<> {
 public:
  UdpTransportChannelTest()
      : network_thread_(rtc::Thread::Current()),
        physical_socket_server_(new rtc::PhysicalSocketServer),
        virtual_socket_server_(
            new rtc::VirtualSocketServer(physical_socket_server_.get())),
        ss_scope_(virtual_socket_server_.get()),
        ep1_("Name1"),
        ep2_("name2") {
    // Setup IP Address for outgoing packets from sockets bound to
    // IPV4 INADDR_ANY ("0.0.0.0."). Virtual socket server sends these packets
    // only if the default address is explicit set. With a physical socket, the
    // actual network stack / operating system would set the IP address for
    // outgoing packets.
    virtual_socket_server_->SetDefaultRoute(kIPv4LocalHostAddress);
  }

  struct Endpoint : public sigslot::has_slots<> {
    explicit Endpoint(std::string tch_name) {
      ch_.reset(new UdpTransportChannel(std::move(tch_name)));
      ch_->SignalReadPacket.connect(this, &Endpoint::OnReadPacket);
      ch_->SignalSentPacket.connect(this, &Endpoint::OnSentPacket);
      ch_->SignalReadyToSend.connect(this, &Endpoint::OnReadyToSend);
      ch_->SignalWritableState.connect(this, &Endpoint::OnWritableState);
    }

    bool CheckData(const char* data, int len) {
      bool ret = false;
      if (!ch_packets_.empty()) {
        std::string packet = ch_packets_.front();
        ret = (packet == std::string(data, len));
        ch_packets_.pop_front();
      }
      return ret;
    }

    void OnWritableState(rtc::PacketTransportInterface* transport) {
      num_sig_writable_++;
    }

    void OnReadyToSend(rtc::PacketTransportInterface* transport) {
      num_sig_ready_to_send_++;
    }

    void OnReadPacket(rtc::PacketTransportInterface* transport,
                      const char* data,
                      size_t len,
                      const rtc::PacketTime& packet_time,
                      int flags) {
      num_received_packets_++;
      LOG(LS_VERBOSE) << "OnReadPacket (unittest)";
      ch_packets_.push_front(std::string(data, len));
    }

    void OnSentPacket(rtc::PacketTransportInterface* transport,
                      const rtc::SentPacket&) {
      num_sig_sent_packets_++;
    }

    int SendData(const char* data, size_t len) {
      rtc::PacketOptions options;
      return ch_->SendPacket(data, len, options, 0);
    }

    void GetLocalPort(uint16_t* local_port) {
      rtc::Optional<rtc::SocketAddress> addr = ch_->local_parameters();
      if (!addr) {
        *local_port = 0;
        return;
      }
      *local_port = addr->port();
    }

    std::list<std::string> ch_packets_;
    std::unique_ptr<UdpTransportChannel> ch_;
    uint32_t num_received_packets_ = 0;   // Increases on SignalReadPacket.
    uint32_t num_sig_sent_packets_ = 0;   // Increases on SignalSentPacket.
    uint32_t num_sig_writable_ = 0;       // Increases on SignalWritable.
    uint32_t num_sig_ready_to_send_ = 0;  // Increases on SignalReadyToSend.
  };

  rtc::Thread* network_thread_ = nullptr;
  std::unique_ptr<rtc::PhysicalSocketServer> physical_socket_server_;
  std::unique_ptr<rtc::VirtualSocketServer> virtual_socket_server_;
  rtc::SocketServerScope ss_scope_;

  Endpoint ep1_;
  Endpoint ep2_;

  void TestSendRecv() {
    for (uint32_t i = 0; i < 5; ++i) {
      static const char* data = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
      int len = static_cast<int>(strlen(data));
      // local_channel <==> remote_channel
      EXPECT_EQ_WAIT(len, ep1_.SendData(data, len), kTimeoutMs);
      EXPECT_TRUE_WAIT(ep2_.CheckData(data, len), kTimeoutMs);
      EXPECT_EQ_WAIT(i + 1u, ep2_.num_received_packets_, kTimeoutMs);
      EXPECT_EQ_WAIT(len, ep2_.SendData(data, len), kTimeoutMs);
      EXPECT_TRUE_WAIT(ep1_.CheckData(data, len), kTimeoutMs);
      EXPECT_EQ_WAIT(i + 1u, ep1_.num_received_packets_, kTimeoutMs);
    }
  }
};

TEST_F(UdpTransportChannelTest, SendRecvBasic) {
  ep1_.ch_->Start();
  ep2_.ch_->Start();
  uint16_t port;
  ep2_.GetLocalPort(&port);
  rtc::SocketAddress addr2 = rtc::SocketAddress("127.0.0.1", port);
  ep1_.ch_->SetRemoteParameters(addr2);
  ep1_.GetLocalPort(&port);
  rtc::SocketAddress addr1 = rtc::SocketAddress("127.0.0.1", port);
  ep2_.ch_->SetRemoteParameters(addr1);
  TestSendRecv();
}

TEST_F(UdpTransportChannelTest, DefaultLocalParameters) {
  EXPECT_FALSE(ep1_.ch_->local_parameters());
}

TEST_F(UdpTransportChannelTest, StartTwice) {
  ep1_.ch_->Start();
  EXPECT_EQ(UdpTransportChannel::State::CONNECTING, ep1_.ch_->state());
  ep1_.ch_->Start();
  EXPECT_EQ(UdpTransportChannel::State::CONNECTING, ep1_.ch_->state());
}

TEST_F(UdpTransportChannelTest, StatusAndSignals) {
  EXPECT_EQ(UdpTransportChannel::State::INIT, ep1_.ch_->state());
  ep1_.ch_->Start();
  EXPECT_EQ(UdpTransportChannel::State::CONNECTING, ep1_.ch_->state());
  EXPECT_EQ(0u, ep1_.num_sig_writable_);
  EXPECT_EQ(0u, ep1_.num_sig_ready_to_send_);
  // Loopback
  EXPECT_TRUE(!ep1_.ch_->writable());
  rtc::Optional<rtc::SocketAddress> addr = ep1_.ch_->local_parameters();
  ASSERT_TRUE(addr);
  // Keep port, but explicitly set IP.
  addr->SetIP("127.0.0.1");
  ep1_.ch_->SetRemoteParameters(*addr);
  EXPECT_TRUE(ep1_.ch_->writable());
  EXPECT_EQ(1u, ep1_.num_sig_writable_);
  EXPECT_EQ(1u, ep1_.num_sig_ready_to_send_);
  const char data[] = "abc";
  ep1_.SendData(data, sizeof(data));
  EXPECT_EQ_WAIT(1u, ep1_.ch_packets_.size(), kTimeoutMs);
  EXPECT_EQ_WAIT(1u, ep1_.num_sig_sent_packets_, kTimeoutMs);
}
}  // namespace cricket
