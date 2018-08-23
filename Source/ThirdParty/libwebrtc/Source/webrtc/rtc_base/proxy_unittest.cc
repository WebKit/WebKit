/*
 *  Copyright 2009 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <string>
#include "rtc_base/gunit.h"
#include "rtc_base/httpserver.h"
#include "rtc_base/proxyserver.h"
#include "rtc_base/socketadapters.h"
#include "rtc_base/testclient.h"
#include "rtc_base/testechoserver.h"
#include "rtc_base/virtualsocketserver.h"

using rtc::Socket;
using rtc::Thread;
using rtc::SocketAddress;

static const SocketAddress kSocksProxyIntAddr("1.2.3.4", 1080);
static const SocketAddress kSocksProxyExtAddr("1.2.3.5", 0);
static const SocketAddress kHttpsProxyIntAddr("1.2.3.4", 443);
static const SocketAddress kHttpsProxyExtAddr("1.2.3.5", 0);
static const SocketAddress kBogusProxyIntAddr("1.2.3.4", 999);

// Sets up a virtual socket server and HTTPS/SOCKS5 proxy servers.
class ProxyTest : public testing::Test {
 public:
  ProxyTest() : ss_(new rtc::VirtualSocketServer()), thread_(ss_.get()) {
    socks_.reset(new rtc::SocksProxyServer(ss_.get(), kSocksProxyIntAddr,
                                           ss_.get(), kSocksProxyExtAddr));
    https_.reset(new rtc::HttpListenServer());
    https_->Listen(kHttpsProxyIntAddr);
  }

  rtc::SocketServer* ss() { return ss_.get(); }

 private:
  std::unique_ptr<rtc::SocketServer> ss_;
  rtc::AutoSocketServerThread thread_;
  std::unique_ptr<rtc::SocksProxyServer> socks_;
  // TODO: Make this a real HTTPS proxy server.
  std::unique_ptr<rtc::HttpListenServer> https_;
};

// Tests whether we can use a SOCKS5 proxy to connect to a server.
TEST_F(ProxyTest, TestSocks5Connect) {
  rtc::AsyncSocket* socket =
      ss()->CreateAsyncSocket(kSocksProxyIntAddr.family(), SOCK_STREAM);
  rtc::AsyncSocksProxySocket* proxy_socket = new rtc::AsyncSocksProxySocket(
      socket, kSocksProxyIntAddr, "", rtc::CryptString());
  // TODO: IPv6-ize these tests when proxy supports IPv6.

  rtc::TestEchoServer server(Thread::Current(), SocketAddress(INADDR_ANY, 0));

  std::unique_ptr<rtc::AsyncTCPSocket> packet_socket(
      rtc::AsyncTCPSocket::Create(proxy_socket, SocketAddress(INADDR_ANY, 0),
                                  server.address()));
  EXPECT_TRUE(packet_socket != nullptr);
  rtc::TestClient client(std::move(packet_socket));

  EXPECT_EQ(Socket::CS_CONNECTING, proxy_socket->GetState());
  EXPECT_TRUE(client.CheckConnected());
  EXPECT_EQ(Socket::CS_CONNECTED, proxy_socket->GetState());
  EXPECT_EQ(server.address(), client.remote_address());
  client.Send("foo", 3);
  EXPECT_TRUE(client.CheckNextPacket("foo", 3, nullptr));
  EXPECT_TRUE(client.CheckNoPacket());
}
