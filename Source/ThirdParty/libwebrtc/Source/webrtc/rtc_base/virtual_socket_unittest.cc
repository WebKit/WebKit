/*
 *  Copyright 2006 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if defined(WEBRTC_POSIX)
#include <netinet/in.h>
#endif

#include <algorithm>
#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "rtc_base/arraysize.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/async_udp_socket.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/gunit.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
#include "rtc_base/message_handler.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/test_client.h"
#include "rtc_base/test_utils.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/thread.h"
#include "rtc_base/time_utils.h"
#include "rtc_base/virtual_socket_server.h"
#include "test/gtest.h"

namespace rtc {
namespace {

using webrtc::testing::SSE_CLOSE;
using webrtc::testing::SSE_ERROR;
using webrtc::testing::SSE_OPEN;
using webrtc::testing::SSE_READ;
using webrtc::testing::SSE_WRITE;
using webrtc::testing::StreamSink;

// Sends at a constant rate but with random packet sizes.
struct Sender : public MessageHandlerAutoCleanup {
  Sender(Thread* th, Socket* s, uint32_t rt)
      : thread(th),
        socket(std::make_unique<AsyncUDPSocket>(s)),
        done(false),
        rate(rt),
        count(0) {
    last_send = rtc::TimeMillis();
    thread->PostDelayed(RTC_FROM_HERE, NextDelay(), this, 1);
  }

  uint32_t NextDelay() {
    uint32_t size = (rand() % 4096) + 1;
    return 1000 * size / rate;
  }

  void OnMessage(Message* pmsg) override {
    ASSERT_EQ(1u, pmsg->message_id);

    if (done)
      return;

    int64_t cur_time = rtc::TimeMillis();
    int64_t delay = cur_time - last_send;
    uint32_t size = static_cast<uint32_t>(rate * delay / 1000);
    size = std::min<uint32_t>(size, 4096);
    size = std::max<uint32_t>(size, sizeof(uint32_t));

    count += size;
    memcpy(dummy, &cur_time, sizeof(cur_time));
    socket->Send(dummy, size, options);

    last_send = cur_time;
    thread->PostDelayed(RTC_FROM_HERE, NextDelay(), this, 1);
  }

  Thread* thread;
  std::unique_ptr<AsyncUDPSocket> socket;
  rtc::PacketOptions options;
  bool done;
  uint32_t rate;  // bytes per second
  uint32_t count;
  int64_t last_send;
  char dummy[4096];
};

struct Receiver : public MessageHandlerAutoCleanup,
                  public sigslot::has_slots<> {
  Receiver(Thread* th, Socket* s, uint32_t bw)
      : thread(th),
        socket(std::make_unique<AsyncUDPSocket>(s)),
        bandwidth(bw),
        done(false),
        count(0),
        sec_count(0),
        sum(0),
        sum_sq(0),
        samples(0) {
    socket->SignalReadPacket.connect(this, &Receiver::OnReadPacket);
    thread->PostDelayed(RTC_FROM_HERE, 1000, this, 1);
  }

  ~Receiver() override { thread->Clear(this); }

  void OnReadPacket(AsyncPacketSocket* s,
                    const char* data,
                    size_t size,
                    const SocketAddress& remote_addr,
                    const int64_t& /* packet_time_us */) {
    ASSERT_EQ(socket.get(), s);
    ASSERT_GE(size, 4U);

    count += size;
    sec_count += size;

    uint32_t send_time = *reinterpret_cast<const uint32_t*>(data);
    uint32_t recv_time = rtc::TimeMillis();
    uint32_t delay = recv_time - send_time;
    sum += delay;
    sum_sq += delay * delay;
    samples += 1;
  }

  void OnMessage(Message* pmsg) override {
    ASSERT_EQ(1u, pmsg->message_id);

    if (done)
      return;

    // It is always possible for us to receive more than expected because
    // packets can be further delayed in delivery.
    if (bandwidth > 0)
      ASSERT_TRUE(sec_count <= 5 * bandwidth / 4);
    sec_count = 0;
    thread->PostDelayed(RTC_FROM_HERE, 1000, this, 1);
  }

  Thread* thread;
  std::unique_ptr<AsyncUDPSocket> socket;
  uint32_t bandwidth;
  bool done;
  size_t count;
  size_t sec_count;
  double sum;
  double sum_sq;
  uint32_t samples;
};

// Note: This test uses a fake clock in addition to a virtual network.
class VirtualSocketServerTest : public ::testing::Test {
 public:
  VirtualSocketServerTest()
      : ss_(&fake_clock_),
        thread_(&ss_),
        kIPv4AnyAddress(IPAddress(INADDR_ANY), 0),
        kIPv6AnyAddress(IPAddress(in6addr_any), 0) {}

  void CheckPortIncrementalization(const SocketAddress& post,
                                   const SocketAddress& pre) {
    EXPECT_EQ(post.port(), pre.port() + 1);
    IPAddress post_ip = post.ipaddr();
    IPAddress pre_ip = pre.ipaddr();
    EXPECT_EQ(pre_ip.family(), post_ip.family());
    if (post_ip.family() == AF_INET) {
      in_addr pre_ipv4 = pre_ip.ipv4_address();
      in_addr post_ipv4 = post_ip.ipv4_address();
      EXPECT_EQ(post_ipv4.s_addr, pre_ipv4.s_addr);
    } else if (post_ip.family() == AF_INET6) {
      in6_addr post_ip6 = post_ip.ipv6_address();
      in6_addr pre_ip6 = pre_ip.ipv6_address();
      uint32_t* post_as_ints = reinterpret_cast<uint32_t*>(&post_ip6.s6_addr);
      uint32_t* pre_as_ints = reinterpret_cast<uint32_t*>(&pre_ip6.s6_addr);
      EXPECT_EQ(post_as_ints[3], pre_as_ints[3]);
    }
  }

  // Test a client can bind to the any address, and all sent packets will have
  // the default source address. Also, it can receive packets sent to the
  // default address.
  void TestDefaultSourceAddress(const IPAddress& default_address) {
    ss_.SetDefaultSourceAddress(default_address);

    // Create client1 bound to the any address.
    Socket* socket = ss_.CreateSocket(default_address.family(), SOCK_DGRAM);
    socket->Bind(EmptySocketAddressWithFamily(default_address.family()));
    SocketAddress client1_any_addr = socket->GetLocalAddress();
    EXPECT_TRUE(client1_any_addr.IsAnyIP());
    auto client1 = std::make_unique<TestClient>(
        std::make_unique<AsyncUDPSocket>(socket), &fake_clock_);

    // Create client2 bound to the address route.
    Socket* socket2 = ss_.CreateSocket(default_address.family(), SOCK_DGRAM);
    socket2->Bind(SocketAddress(default_address, 0));
    SocketAddress client2_addr = socket2->GetLocalAddress();
    EXPECT_FALSE(client2_addr.IsAnyIP());
    auto client2 = std::make_unique<TestClient>(
        std::make_unique<AsyncUDPSocket>(socket2), &fake_clock_);

    // Client1 sends to client2, client2 should see the default address as
    // client1's address.
    SocketAddress client1_addr;
    EXPECT_EQ(6, client1->SendTo("bizbaz", 6, client2_addr));
    EXPECT_TRUE(client2->CheckNextPacket("bizbaz", 6, &client1_addr));
    EXPECT_EQ(client1_addr,
              SocketAddress(default_address, client1_any_addr.port()));

    // Client2 can send back to client1's default address.
    EXPECT_EQ(3, client2->SendTo("foo", 3, client1_addr));
    EXPECT_TRUE(client1->CheckNextPacket("foo", 3, &client2_addr));
  }

  void BasicTest(const SocketAddress& initial_addr) {
    Socket* socket = ss_.CreateSocket(initial_addr.family(), SOCK_DGRAM);
    socket->Bind(initial_addr);
    SocketAddress server_addr = socket->GetLocalAddress();
    // Make sure VSS didn't switch families on us.
    EXPECT_EQ(server_addr.family(), initial_addr.family());

    auto client1 = std::make_unique<TestClient>(
        std::make_unique<AsyncUDPSocket>(socket), &fake_clock_);
    Socket* socket2 = ss_.CreateSocket(initial_addr.family(), SOCK_DGRAM);
    auto client2 = std::make_unique<TestClient>(
        std::make_unique<AsyncUDPSocket>(socket2), &fake_clock_);

    SocketAddress client2_addr;
    EXPECT_EQ(3, client2->SendTo("foo", 3, server_addr));
    EXPECT_TRUE(client1->CheckNextPacket("foo", 3, &client2_addr));

    SocketAddress client1_addr;
    EXPECT_EQ(6, client1->SendTo("bizbaz", 6, client2_addr));
    EXPECT_TRUE(client2->CheckNextPacket("bizbaz", 6, &client1_addr));
    EXPECT_EQ(client1_addr, server_addr);

    SocketAddress empty = EmptySocketAddressWithFamily(initial_addr.family());
    for (int i = 0; i < 10; i++) {
      client2 = std::make_unique<TestClient>(
          absl::WrapUnique(AsyncUDPSocket::Create(&ss_, empty)), &fake_clock_);

      SocketAddress next_client2_addr;
      EXPECT_EQ(3, client2->SendTo("foo", 3, server_addr));
      EXPECT_TRUE(client1->CheckNextPacket("foo", 3, &next_client2_addr));
      CheckPortIncrementalization(next_client2_addr, client2_addr);
      // EXPECT_EQ(next_client2_addr.port(), client2_addr.port() + 1);

      SocketAddress server_addr2;
      EXPECT_EQ(6, client1->SendTo("bizbaz", 6, next_client2_addr));
      EXPECT_TRUE(client2->CheckNextPacket("bizbaz", 6, &server_addr2));
      EXPECT_EQ(server_addr2, server_addr);

      client2_addr = next_client2_addr;
    }
  }

  // initial_addr should be made from either INADDR_ANY or in6addr_any.
  void ConnectTest(const SocketAddress& initial_addr) {
    StreamSink sink;
    SocketAddress accept_addr;
    const SocketAddress kEmptyAddr =
        EmptySocketAddressWithFamily(initial_addr.family());

    // Create client
    std::unique_ptr<Socket> client =
        absl::WrapUnique(ss_.CreateSocket(initial_addr.family(), SOCK_STREAM));
    sink.Monitor(client.get());
    EXPECT_EQ(client->GetState(), Socket::CS_CLOSED);
    EXPECT_TRUE(client->GetLocalAddress().IsNil());

    // Create server
    std::unique_ptr<Socket> server =
        absl::WrapUnique(ss_.CreateSocket(initial_addr.family(), SOCK_STREAM));
    sink.Monitor(server.get());
    EXPECT_NE(0, server->Listen(5));  // Bind required
    EXPECT_EQ(0, server->Bind(initial_addr));
    EXPECT_EQ(server->GetLocalAddress().family(), initial_addr.family());
    EXPECT_EQ(0, server->Listen(5));
    EXPECT_EQ(server->GetState(), Socket::CS_CONNECTING);

    // No pending server connections
    EXPECT_FALSE(sink.Check(server.get(), SSE_READ));
    EXPECT_TRUE(nullptr == server->Accept(&accept_addr));
    EXPECT_EQ(AF_UNSPEC, accept_addr.family());

    // Attempt connect to listening socket
    EXPECT_EQ(0, client->Connect(server->GetLocalAddress()));
    EXPECT_NE(client->GetLocalAddress(), kEmptyAddr);          // Implicit Bind
    EXPECT_NE(AF_UNSPEC, client->GetLocalAddress().family());  // Implicit Bind
    EXPECT_NE(client->GetLocalAddress(), server->GetLocalAddress());

    // Client is connecting
    EXPECT_EQ(client->GetState(), Socket::CS_CONNECTING);
    EXPECT_FALSE(sink.Check(client.get(), SSE_OPEN));
    EXPECT_FALSE(sink.Check(client.get(), SSE_CLOSE));

    ss_.ProcessMessagesUntilIdle();

    // Client still connecting
    EXPECT_EQ(client->GetState(), Socket::CS_CONNECTING);
    EXPECT_FALSE(sink.Check(client.get(), SSE_OPEN));
    EXPECT_FALSE(sink.Check(client.get(), SSE_CLOSE));

    // Server has pending connection
    EXPECT_TRUE(sink.Check(server.get(), SSE_READ));
    std::unique_ptr<Socket> accepted =
        absl::WrapUnique(server->Accept(&accept_addr));
    EXPECT_TRUE(nullptr != accepted);
    EXPECT_NE(accept_addr, kEmptyAddr);
    EXPECT_EQ(accepted->GetRemoteAddress(), accept_addr);

    EXPECT_EQ(accepted->GetState(), Socket::CS_CONNECTED);
    EXPECT_EQ(accepted->GetLocalAddress(), server->GetLocalAddress());
    EXPECT_EQ(accepted->GetRemoteAddress(), client->GetLocalAddress());

    ss_.ProcessMessagesUntilIdle();

    // Client has connected
    EXPECT_EQ(client->GetState(), Socket::CS_CONNECTED);
    EXPECT_TRUE(sink.Check(client.get(), SSE_OPEN));
    EXPECT_FALSE(sink.Check(client.get(), SSE_CLOSE));
    EXPECT_EQ(client->GetRemoteAddress(), server->GetLocalAddress());
    EXPECT_EQ(client->GetRemoteAddress(), accepted->GetLocalAddress());
  }

  void ConnectToNonListenerTest(const SocketAddress& initial_addr) {
    StreamSink sink;
    SocketAddress accept_addr;
    const SocketAddress nil_addr;
    const SocketAddress empty_addr =
        EmptySocketAddressWithFamily(initial_addr.family());

    // Create client
    std::unique_ptr<Socket> client =
        absl::WrapUnique(ss_.CreateSocket(initial_addr.family(), SOCK_STREAM));
    sink.Monitor(client.get());

    // Create server
    std::unique_ptr<Socket> server =
        absl::WrapUnique(ss_.CreateSocket(initial_addr.family(), SOCK_STREAM));
    sink.Monitor(server.get());
    EXPECT_EQ(0, server->Bind(initial_addr));
    EXPECT_EQ(server->GetLocalAddress().family(), initial_addr.family());
    // Attempt connect to non-listening socket
    EXPECT_EQ(0, client->Connect(server->GetLocalAddress()));

    ss_.ProcessMessagesUntilIdle();

    // No pending server connections
    EXPECT_FALSE(sink.Check(server.get(), SSE_READ));
    EXPECT_TRUE(nullptr == server->Accept(&accept_addr));
    EXPECT_EQ(accept_addr, nil_addr);

    // Connection failed
    EXPECT_EQ(client->GetState(), Socket::CS_CLOSED);
    EXPECT_FALSE(sink.Check(client.get(), SSE_OPEN));
    EXPECT_TRUE(sink.Check(client.get(), SSE_ERROR));
    EXPECT_EQ(client->GetRemoteAddress(), nil_addr);
  }

  void CloseDuringConnectTest(const SocketAddress& initial_addr) {
    StreamSink sink;
    SocketAddress accept_addr;
    const SocketAddress empty_addr =
        EmptySocketAddressWithFamily(initial_addr.family());

    // Create client and server
    std::unique_ptr<Socket> client(
        ss_.CreateSocket(initial_addr.family(), SOCK_STREAM));
    sink.Monitor(client.get());
    std::unique_ptr<Socket> server(
        ss_.CreateSocket(initial_addr.family(), SOCK_STREAM));
    sink.Monitor(server.get());

    // Initiate connect
    EXPECT_EQ(0, server->Bind(initial_addr));
    EXPECT_EQ(server->GetLocalAddress().family(), initial_addr.family());

    EXPECT_EQ(0, server->Listen(5));
    EXPECT_EQ(0, client->Connect(server->GetLocalAddress()));

    // Server close before socket enters accept queue
    EXPECT_FALSE(sink.Check(server.get(), SSE_READ));
    server->Close();

    ss_.ProcessMessagesUntilIdle();

    // Result: connection failed
    EXPECT_EQ(client->GetState(), Socket::CS_CLOSED);
    EXPECT_TRUE(sink.Check(client.get(), SSE_ERROR));

    server.reset(ss_.CreateSocket(initial_addr.family(), SOCK_STREAM));
    sink.Monitor(server.get());

    // Initiate connect
    EXPECT_EQ(0, server->Bind(initial_addr));
    EXPECT_EQ(server->GetLocalAddress().family(), initial_addr.family());

    EXPECT_EQ(0, server->Listen(5));
    EXPECT_EQ(0, client->Connect(server->GetLocalAddress()));

    ss_.ProcessMessagesUntilIdle();

    // Server close while socket is in accept queue
    EXPECT_TRUE(sink.Check(server.get(), SSE_READ));
    server->Close();

    ss_.ProcessMessagesUntilIdle();

    // Result: connection failed
    EXPECT_EQ(client->GetState(), Socket::CS_CLOSED);
    EXPECT_TRUE(sink.Check(client.get(), SSE_ERROR));

    // New server
    server.reset(ss_.CreateSocket(initial_addr.family(), SOCK_STREAM));
    sink.Monitor(server.get());

    // Initiate connect
    EXPECT_EQ(0, server->Bind(initial_addr));
    EXPECT_EQ(server->GetLocalAddress().family(), initial_addr.family());

    EXPECT_EQ(0, server->Listen(5));
    EXPECT_EQ(0, client->Connect(server->GetLocalAddress()));

    ss_.ProcessMessagesUntilIdle();

    // Server accepts connection
    EXPECT_TRUE(sink.Check(server.get(), SSE_READ));
    std::unique_ptr<Socket> accepted(server->Accept(&accept_addr));
    ASSERT_TRUE(nullptr != accepted.get());
    sink.Monitor(accepted.get());

    // Client closes before connection complets
    EXPECT_EQ(accepted->GetState(), Socket::CS_CONNECTED);

    // Connected message has not been processed yet.
    EXPECT_EQ(client->GetState(), Socket::CS_CONNECTING);
    client->Close();

    ss_.ProcessMessagesUntilIdle();

    // Result: accepted socket closes
    EXPECT_EQ(accepted->GetState(), Socket::CS_CLOSED);
    EXPECT_TRUE(sink.Check(accepted.get(), SSE_CLOSE));
    EXPECT_FALSE(sink.Check(client.get(), SSE_CLOSE));
  }

  void CloseTest(const SocketAddress& initial_addr) {
    StreamSink sink;
    const SocketAddress kEmptyAddr;

    // Create clients
    std::unique_ptr<Socket> a =
        absl::WrapUnique(ss_.CreateSocket(initial_addr.family(), SOCK_STREAM));
    sink.Monitor(a.get());
    a->Bind(initial_addr);
    EXPECT_EQ(a->GetLocalAddress().family(), initial_addr.family());

    std::unique_ptr<Socket> b =
        absl::WrapUnique(ss_.CreateSocket(initial_addr.family(), SOCK_STREAM));
    sink.Monitor(b.get());
    b->Bind(initial_addr);
    EXPECT_EQ(b->GetLocalAddress().family(), initial_addr.family());

    EXPECT_EQ(0, a->Connect(b->GetLocalAddress()));
    EXPECT_EQ(0, b->Connect(a->GetLocalAddress()));

    ss_.ProcessMessagesUntilIdle();

    EXPECT_TRUE(sink.Check(a.get(), SSE_OPEN));
    EXPECT_EQ(a->GetState(), Socket::CS_CONNECTED);
    EXPECT_EQ(a->GetRemoteAddress(), b->GetLocalAddress());

    EXPECT_TRUE(sink.Check(b.get(), SSE_OPEN));
    EXPECT_EQ(b->GetState(), Socket::CS_CONNECTED);
    EXPECT_EQ(b->GetRemoteAddress(), a->GetLocalAddress());

    EXPECT_EQ(1, a->Send("a", 1));
    b->Close();
    EXPECT_EQ(1, a->Send("b", 1));

    ss_.ProcessMessagesUntilIdle();

    char buffer[10];
    EXPECT_FALSE(sink.Check(b.get(), SSE_READ));
    EXPECT_EQ(-1, b->Recv(buffer, 10, nullptr));

    EXPECT_TRUE(sink.Check(a.get(), SSE_CLOSE));
    EXPECT_EQ(a->GetState(), Socket::CS_CLOSED);
    EXPECT_EQ(a->GetRemoteAddress(), kEmptyAddr);

    // No signal for Closer
    EXPECT_FALSE(sink.Check(b.get(), SSE_CLOSE));
    EXPECT_EQ(b->GetState(), Socket::CS_CLOSED);
    EXPECT_EQ(b->GetRemoteAddress(), kEmptyAddr);
  }

  void TcpSendTest(const SocketAddress& initial_addr) {
    StreamSink sink;
    const SocketAddress kEmptyAddr;

    // Connect two sockets
    std::unique_ptr<Socket> a =
        absl::WrapUnique(ss_.CreateSocket(initial_addr.family(), SOCK_STREAM));
    sink.Monitor(a.get());
    a->Bind(initial_addr);
    EXPECT_EQ(a->GetLocalAddress().family(), initial_addr.family());

    std::unique_ptr<Socket> b =
        absl::WrapUnique(ss_.CreateSocket(initial_addr.family(), SOCK_STREAM));
    sink.Monitor(b.get());
    b->Bind(initial_addr);
    EXPECT_EQ(b->GetLocalAddress().family(), initial_addr.family());

    EXPECT_EQ(0, a->Connect(b->GetLocalAddress()));
    EXPECT_EQ(0, b->Connect(a->GetLocalAddress()));

    ss_.ProcessMessagesUntilIdle();

    const size_t kBufferSize = 2000;
    ss_.set_send_buffer_capacity(kBufferSize);
    ss_.set_recv_buffer_capacity(kBufferSize);

    const size_t kDataSize = 5000;
    char send_buffer[kDataSize], recv_buffer[kDataSize];
    for (size_t i = 0; i < kDataSize; ++i)
      send_buffer[i] = static_cast<char>(i % 256);
    memset(recv_buffer, 0, sizeof(recv_buffer));
    size_t send_pos = 0, recv_pos = 0;

    // Can't send more than send buffer in one write
    int result = a->Send(send_buffer + send_pos, kDataSize - send_pos);
    EXPECT_EQ(static_cast<int>(kBufferSize), result);
    send_pos += result;

    ss_.ProcessMessagesUntilIdle();
    EXPECT_FALSE(sink.Check(a.get(), SSE_WRITE));
    EXPECT_TRUE(sink.Check(b.get(), SSE_READ));

    // Receive buffer is already filled, fill send buffer again
    result = a->Send(send_buffer + send_pos, kDataSize - send_pos);
    EXPECT_EQ(static_cast<int>(kBufferSize), result);
    send_pos += result;

    ss_.ProcessMessagesUntilIdle();
    EXPECT_FALSE(sink.Check(a.get(), SSE_WRITE));
    EXPECT_FALSE(sink.Check(b.get(), SSE_READ));

    // No more room in send or receive buffer
    result = a->Send(send_buffer + send_pos, kDataSize - send_pos);
    EXPECT_EQ(-1, result);
    EXPECT_TRUE(a->IsBlocking());

    // Read a subset of the data
    result = b->Recv(recv_buffer + recv_pos, 500, nullptr);
    EXPECT_EQ(500, result);
    recv_pos += result;

    ss_.ProcessMessagesUntilIdle();
    EXPECT_TRUE(sink.Check(a.get(), SSE_WRITE));
    EXPECT_TRUE(sink.Check(b.get(), SSE_READ));

    // Room for more on the sending side
    result = a->Send(send_buffer + send_pos, kDataSize - send_pos);
    EXPECT_EQ(500, result);
    send_pos += result;

    // Empty the recv buffer
    while (true) {
      result = b->Recv(recv_buffer + recv_pos, kDataSize - recv_pos, nullptr);
      if (result < 0) {
        EXPECT_EQ(-1, result);
        EXPECT_TRUE(b->IsBlocking());
        break;
      }
      recv_pos += result;
    }

    ss_.ProcessMessagesUntilIdle();
    EXPECT_TRUE(sink.Check(b.get(), SSE_READ));

    // Continue to empty the recv buffer
    while (true) {
      result = b->Recv(recv_buffer + recv_pos, kDataSize - recv_pos, nullptr);
      if (result < 0) {
        EXPECT_EQ(-1, result);
        EXPECT_TRUE(b->IsBlocking());
        break;
      }
      recv_pos += result;
    }

    // Send last of the data
    result = a->Send(send_buffer + send_pos, kDataSize - send_pos);
    EXPECT_EQ(500, result);
    send_pos += result;

    ss_.ProcessMessagesUntilIdle();
    EXPECT_TRUE(sink.Check(b.get(), SSE_READ));

    // Receive the last of the data
    while (true) {
      result = b->Recv(recv_buffer + recv_pos, kDataSize - recv_pos, nullptr);
      if (result < 0) {
        EXPECT_EQ(-1, result);
        EXPECT_TRUE(b->IsBlocking());
        break;
      }
      recv_pos += result;
    }

    ss_.ProcessMessagesUntilIdle();
    EXPECT_FALSE(sink.Check(b.get(), SSE_READ));

    // The received data matches the sent data
    EXPECT_EQ(kDataSize, send_pos);
    EXPECT_EQ(kDataSize, recv_pos);
    EXPECT_EQ(0, memcmp(recv_buffer, send_buffer, kDataSize));
  }

  void TcpSendsPacketsInOrderTest(const SocketAddress& initial_addr) {
    const SocketAddress kEmptyAddr;

    // Connect two sockets
    std::unique_ptr<Socket> a =
        absl::WrapUnique(ss_.CreateSocket(initial_addr.family(), SOCK_STREAM));
    std::unique_ptr<Socket> b =
        absl::WrapUnique(ss_.CreateSocket(initial_addr.family(), SOCK_STREAM));
    a->Bind(initial_addr);
    EXPECT_EQ(a->GetLocalAddress().family(), initial_addr.family());

    b->Bind(initial_addr);
    EXPECT_EQ(b->GetLocalAddress().family(), initial_addr.family());

    EXPECT_EQ(0, a->Connect(b->GetLocalAddress()));
    EXPECT_EQ(0, b->Connect(a->GetLocalAddress()));
    ss_.ProcessMessagesUntilIdle();

    // First, deliver all packets in 0 ms.
    char buffer[2] = {0, 0};
    const char cNumPackets = 10;
    for (char i = 0; i < cNumPackets; ++i) {
      buffer[0] = '0' + i;
      EXPECT_EQ(1, a->Send(buffer, 1));
    }

    ss_.ProcessMessagesUntilIdle();

    for (char i = 0; i < cNumPackets; ++i) {
      EXPECT_EQ(1, b->Recv(buffer, sizeof(buffer), nullptr));
      EXPECT_EQ(static_cast<char>('0' + i), buffer[0]);
    }

    // Next, deliver packets at random intervals
    const uint32_t mean = 50;
    const uint32_t stddev = 50;

    ss_.set_delay_mean(mean);
    ss_.set_delay_stddev(stddev);
    ss_.UpdateDelayDistribution();

    for (char i = 0; i < cNumPackets; ++i) {
      buffer[0] = 'A' + i;
      EXPECT_EQ(1, a->Send(buffer, 1));
    }

    ss_.ProcessMessagesUntilIdle();

    for (char i = 0; i < cNumPackets; ++i) {
      EXPECT_EQ(1, b->Recv(buffer, sizeof(buffer), nullptr));
      EXPECT_EQ(static_cast<char>('A' + i), buffer[0]);
    }
  }

  // It is important that initial_addr's port has to be 0 such that the
  // incremental port behavior could ensure the 2 Binds result in different
  // address.
  void BandwidthTest(const SocketAddress& initial_addr) {
    Socket* send_socket = ss_.CreateSocket(initial_addr.family(), SOCK_DGRAM);
    Socket* recv_socket = ss_.CreateSocket(initial_addr.family(), SOCK_DGRAM);
    ASSERT_EQ(0, send_socket->Bind(initial_addr));
    ASSERT_EQ(0, recv_socket->Bind(initial_addr));
    EXPECT_EQ(send_socket->GetLocalAddress().family(), initial_addr.family());
    EXPECT_EQ(recv_socket->GetLocalAddress().family(), initial_addr.family());
    ASSERT_EQ(0, send_socket->Connect(recv_socket->GetLocalAddress()));

    uint32_t bandwidth = 64 * 1024;
    ss_.set_bandwidth(bandwidth);

    Thread* pthMain = Thread::Current();
    Sender sender(pthMain, send_socket, 80 * 1024);
    Receiver receiver(pthMain, recv_socket, bandwidth);

    // Allow the sender to run for 5 (simulated) seconds, then be stopped for 5
    // seconds.
    SIMULATED_WAIT(false, 5000, fake_clock_);
    sender.done = true;
    SIMULATED_WAIT(false, 5000, fake_clock_);

    // Ensure the observed bandwidth fell within a reasonable margin of error.
    EXPECT_TRUE(receiver.count >= 5 * 3 * bandwidth / 4);
    EXPECT_TRUE(receiver.count <= 6 * bandwidth);  // queue could drain for 1s

    ss_.set_bandwidth(0);
  }

  // It is important that initial_addr's port has to be 0 such that the
  // incremental port behavior could ensure the 2 Binds result in different
  // address.
  void DelayTest(const SocketAddress& initial_addr) {
    time_t seed = ::time(nullptr);
    RTC_LOG(LS_VERBOSE) << "seed = " << seed;
    srand(static_cast<unsigned int>(seed));

    const uint32_t mean = 2000;
    const uint32_t stddev = 500;

    ss_.set_delay_mean(mean);
    ss_.set_delay_stddev(stddev);
    ss_.UpdateDelayDistribution();

    Socket* send_socket = ss_.CreateSocket(initial_addr.family(), SOCK_DGRAM);
    Socket* recv_socket = ss_.CreateSocket(initial_addr.family(), SOCK_DGRAM);
    ASSERT_EQ(0, send_socket->Bind(initial_addr));
    ASSERT_EQ(0, recv_socket->Bind(initial_addr));
    EXPECT_EQ(send_socket->GetLocalAddress().family(), initial_addr.family());
    EXPECT_EQ(recv_socket->GetLocalAddress().family(), initial_addr.family());
    ASSERT_EQ(0, send_socket->Connect(recv_socket->GetLocalAddress()));

    Thread* pthMain = Thread::Current();
    // Avg packet size is 2K, so at 200KB/s for 10s, we should see about
    // 1000 packets, which is necessary to get a good distribution.
    Sender sender(pthMain, send_socket, 100 * 2 * 1024);
    Receiver receiver(pthMain, recv_socket, 0);

    // Simulate 10 seconds of packets being sent, then check the observed delay
    // distribution.
    SIMULATED_WAIT(false, 10000, fake_clock_);
    sender.done = receiver.done = true;
    ss_.ProcessMessagesUntilIdle();

    const double sample_mean = receiver.sum / receiver.samples;
    double num =
        receiver.samples * receiver.sum_sq - receiver.sum * receiver.sum;
    double den = receiver.samples * (receiver.samples - 1);
    const double sample_stddev = sqrt(num / den);
    RTC_LOG(LS_VERBOSE) << "mean=" << sample_mean
                        << " stddev=" << sample_stddev;

    EXPECT_LE(500u, receiver.samples);
    // We initially used a 0.1 fudge factor, but on the build machine, we
    // have seen the value differ by as much as 0.13.
    EXPECT_NEAR(mean, sample_mean, 0.15 * mean);
    EXPECT_NEAR(stddev, sample_stddev, 0.15 * stddev);

    ss_.set_delay_mean(0);
    ss_.set_delay_stddev(0);
    ss_.UpdateDelayDistribution();
  }

  // Test cross-family communication between a client bound to client_addr and a
  // server bound to server_addr. shouldSucceed indicates if communication is
  // expected to work or not.
  void CrossFamilyConnectionTest(const SocketAddress& client_addr,
                                 const SocketAddress& server_addr,
                                 bool shouldSucceed) {
    StreamSink sink;
    SocketAddress accept_address;
    const SocketAddress kEmptyAddr;

    // Client gets a IPv4 address
    std::unique_ptr<Socket> client =
        absl::WrapUnique(ss_.CreateSocket(client_addr.family(), SOCK_STREAM));
    sink.Monitor(client.get());
    EXPECT_EQ(client->GetState(), Socket::CS_CLOSED);
    EXPECT_EQ(client->GetLocalAddress(), kEmptyAddr);
    client->Bind(client_addr);

    // Server gets a non-mapped non-any IPv6 address.
    // IPv4 sockets should not be able to connect to this.
    std::unique_ptr<Socket> server =
        absl::WrapUnique(ss_.CreateSocket(server_addr.family(), SOCK_STREAM));
    sink.Monitor(server.get());
    server->Bind(server_addr);
    server->Listen(5);

    if (shouldSucceed) {
      EXPECT_EQ(0, client->Connect(server->GetLocalAddress()));
      ss_.ProcessMessagesUntilIdle();
      EXPECT_TRUE(sink.Check(server.get(), SSE_READ));
      std::unique_ptr<Socket> accepted =
          absl::WrapUnique(server->Accept(&accept_address));
      EXPECT_TRUE(nullptr != accepted);
      EXPECT_NE(kEmptyAddr, accept_address);
      ss_.ProcessMessagesUntilIdle();
      EXPECT_TRUE(sink.Check(client.get(), SSE_OPEN));
      EXPECT_EQ(client->GetRemoteAddress(), server->GetLocalAddress());
    } else {
      // Check that the connection failed.
      EXPECT_EQ(-1, client->Connect(server->GetLocalAddress()));
      ss_.ProcessMessagesUntilIdle();

      EXPECT_FALSE(sink.Check(server.get(), SSE_READ));
      EXPECT_TRUE(nullptr == server->Accept(&accept_address));
      EXPECT_EQ(accept_address, kEmptyAddr);
      EXPECT_EQ(client->GetState(), Socket::CS_CLOSED);
      EXPECT_FALSE(sink.Check(client.get(), SSE_OPEN));
      EXPECT_EQ(client->GetRemoteAddress(), kEmptyAddr);
    }
  }

  // Test cross-family datagram sending between a client bound to client_addr
  // and a server bound to server_addr. shouldSucceed indicates if sending is
  // expected to succeed or not.
  void CrossFamilyDatagramTest(const SocketAddress& client_addr,
                               const SocketAddress& server_addr,
                               bool shouldSucceed) {
    Socket* socket = ss_.CreateSocket(AF_INET, SOCK_DGRAM);
    socket->Bind(server_addr);
    SocketAddress bound_server_addr = socket->GetLocalAddress();
    auto client1 = std::make_unique<TestClient>(
        std::make_unique<AsyncUDPSocket>(socket), &fake_clock_);

    Socket* socket2 = ss_.CreateSocket(AF_INET, SOCK_DGRAM);
    socket2->Bind(client_addr);
    auto client2 = std::make_unique<TestClient>(
        std::make_unique<AsyncUDPSocket>(socket2), &fake_clock_);
    SocketAddress client2_addr;

    if (shouldSucceed) {
      EXPECT_EQ(3, client2->SendTo("foo", 3, bound_server_addr));
      EXPECT_TRUE(client1->CheckNextPacket("foo", 3, &client2_addr));
      SocketAddress client1_addr;
      EXPECT_EQ(6, client1->SendTo("bizbaz", 6, client2_addr));
      EXPECT_TRUE(client2->CheckNextPacket("bizbaz", 6, &client1_addr));
      EXPECT_EQ(client1_addr, bound_server_addr);
    } else {
      EXPECT_EQ(-1, client2->SendTo("foo", 3, bound_server_addr));
      EXPECT_TRUE(client1->CheckNoPacket());
    }
  }

 protected:
  rtc::ScopedFakeClock fake_clock_;
  VirtualSocketServer ss_;
  AutoSocketServerThread thread_;
  const SocketAddress kIPv4AnyAddress;
  const SocketAddress kIPv6AnyAddress;
};

TEST_F(VirtualSocketServerTest, basic_v4) {
  SocketAddress ipv4_test_addr(IPAddress(INADDR_ANY), 5000);
  BasicTest(ipv4_test_addr);
}

TEST_F(VirtualSocketServerTest, basic_v6) {
  SocketAddress ipv6_test_addr(IPAddress(in6addr_any), 5000);
  BasicTest(ipv6_test_addr);
}

TEST_F(VirtualSocketServerTest, TestDefaultRoute_v4) {
  IPAddress ipv4_default_addr(0x01020304);
  TestDefaultSourceAddress(ipv4_default_addr);
}

TEST_F(VirtualSocketServerTest, TestDefaultRoute_v6) {
  IPAddress ipv6_default_addr;
  EXPECT_TRUE(
      IPFromString("2401:fa00:4:1000:be30:5bff:fee5:c3", &ipv6_default_addr));
  TestDefaultSourceAddress(ipv6_default_addr);
}

TEST_F(VirtualSocketServerTest, connect_v4) {
  ConnectTest(kIPv4AnyAddress);
}

TEST_F(VirtualSocketServerTest, connect_v6) {
  ConnectTest(kIPv6AnyAddress);
}

TEST_F(VirtualSocketServerTest, connect_to_non_listener_v4) {
  ConnectToNonListenerTest(kIPv4AnyAddress);
}

TEST_F(VirtualSocketServerTest, connect_to_non_listener_v6) {
  ConnectToNonListenerTest(kIPv6AnyAddress);
}

TEST_F(VirtualSocketServerTest, close_during_connect_v4) {
  CloseDuringConnectTest(kIPv4AnyAddress);
}

TEST_F(VirtualSocketServerTest, close_during_connect_v6) {
  CloseDuringConnectTest(kIPv6AnyAddress);
}

TEST_F(VirtualSocketServerTest, close_v4) {
  CloseTest(kIPv4AnyAddress);
}

TEST_F(VirtualSocketServerTest, close_v6) {
  CloseTest(kIPv6AnyAddress);
}

TEST_F(VirtualSocketServerTest, tcp_send_v4) {
  TcpSendTest(kIPv4AnyAddress);
}

TEST_F(VirtualSocketServerTest, tcp_send_v6) {
  TcpSendTest(kIPv6AnyAddress);
}

TEST_F(VirtualSocketServerTest, TcpSendsPacketsInOrder_v4) {
  TcpSendsPacketsInOrderTest(kIPv4AnyAddress);
}

TEST_F(VirtualSocketServerTest, TcpSendsPacketsInOrder_v6) {
  TcpSendsPacketsInOrderTest(kIPv6AnyAddress);
}

TEST_F(VirtualSocketServerTest, bandwidth_v4) {
  BandwidthTest(kIPv4AnyAddress);
}

TEST_F(VirtualSocketServerTest, bandwidth_v6) {
  BandwidthTest(kIPv6AnyAddress);
}

TEST_F(VirtualSocketServerTest, delay_v4) {
  DelayTest(kIPv4AnyAddress);
}

TEST_F(VirtualSocketServerTest, delay_v6) {
  DelayTest(kIPv6AnyAddress);
}

// Works, receiving socket sees 127.0.0.2.
TEST_F(VirtualSocketServerTest, CanConnectFromMappedIPv6ToIPv4Any) {
  CrossFamilyConnectionTest(SocketAddress("::ffff:127.0.0.2", 0),
                            SocketAddress("0.0.0.0", 5000), true);
}

// Fails.
TEST_F(VirtualSocketServerTest, CantConnectFromUnMappedIPv6ToIPv4Any) {
  CrossFamilyConnectionTest(SocketAddress("::2", 0),
                            SocketAddress("0.0.0.0", 5000), false);
}

// Fails.
TEST_F(VirtualSocketServerTest, CantConnectFromUnMappedIPv6ToMappedIPv6) {
  CrossFamilyConnectionTest(SocketAddress("::2", 0),
                            SocketAddress("::ffff:127.0.0.1", 5000), false);
}

// Works. receiving socket sees ::ffff:127.0.0.2.
TEST_F(VirtualSocketServerTest, CanConnectFromIPv4ToIPv6Any) {
  CrossFamilyConnectionTest(SocketAddress("127.0.0.2", 0),
                            SocketAddress("::", 5000), true);
}

// Fails.
TEST_F(VirtualSocketServerTest, CantConnectFromIPv4ToUnMappedIPv6) {
  CrossFamilyConnectionTest(SocketAddress("127.0.0.2", 0),
                            SocketAddress("::1", 5000), false);
}

// Works. Receiving socket sees ::ffff:127.0.0.1.
TEST_F(VirtualSocketServerTest, CanConnectFromIPv4ToMappedIPv6) {
  CrossFamilyConnectionTest(SocketAddress("127.0.0.1", 0),
                            SocketAddress("::ffff:127.0.0.2", 5000), true);
}

// Works, receiving socket sees a result from GetNextIP.
TEST_F(VirtualSocketServerTest, CanConnectFromUnboundIPv6ToIPv4Any) {
  CrossFamilyConnectionTest(SocketAddress("::", 0),
                            SocketAddress("0.0.0.0", 5000), true);
}

// Works, receiving socket sees whatever GetNextIP gave the client.
TEST_F(VirtualSocketServerTest, CanConnectFromUnboundIPv4ToIPv6Any) {
  CrossFamilyConnectionTest(SocketAddress("0.0.0.0", 0),
                            SocketAddress("::", 5000), true);
}

TEST_F(VirtualSocketServerTest, CanSendDatagramFromUnboundIPv4ToIPv6Any) {
  CrossFamilyDatagramTest(SocketAddress("0.0.0.0", 0),
                          SocketAddress("::", 5000), true);
}

TEST_F(VirtualSocketServerTest, CanSendDatagramFromMappedIPv6ToIPv4Any) {
  CrossFamilyDatagramTest(SocketAddress("::ffff:127.0.0.1", 0),
                          SocketAddress("0.0.0.0", 5000), true);
}

TEST_F(VirtualSocketServerTest, CantSendDatagramFromUnMappedIPv6ToIPv4Any) {
  CrossFamilyDatagramTest(SocketAddress("::2", 0),
                          SocketAddress("0.0.0.0", 5000), false);
}

TEST_F(VirtualSocketServerTest, CantSendDatagramFromUnMappedIPv6ToMappedIPv6) {
  CrossFamilyDatagramTest(SocketAddress("::2", 0),
                          SocketAddress("::ffff:127.0.0.1", 5000), false);
}

TEST_F(VirtualSocketServerTest, CanSendDatagramFromIPv4ToIPv6Any) {
  CrossFamilyDatagramTest(SocketAddress("127.0.0.2", 0),
                          SocketAddress("::", 5000), true);
}

TEST_F(VirtualSocketServerTest, CantSendDatagramFromIPv4ToUnMappedIPv6) {
  CrossFamilyDatagramTest(SocketAddress("127.0.0.2", 0),
                          SocketAddress("::1", 5000), false);
}

TEST_F(VirtualSocketServerTest, CanSendDatagramFromIPv4ToMappedIPv6) {
  CrossFamilyDatagramTest(SocketAddress("127.0.0.1", 0),
                          SocketAddress("::ffff:127.0.0.2", 5000), true);
}

TEST_F(VirtualSocketServerTest, CanSendDatagramFromUnboundIPv6ToIPv4Any) {
  CrossFamilyDatagramTest(SocketAddress("::", 0),
                          SocketAddress("0.0.0.0", 5000), true);
}

TEST_F(VirtualSocketServerTest, SetSendingBlockedWithUdpSocket) {
  Socket* socket1 = ss_.CreateSocket(kIPv4AnyAddress.family(), SOCK_DGRAM);
  std::unique_ptr<Socket> socket2 =
      absl::WrapUnique(ss_.CreateSocket(kIPv4AnyAddress.family(), SOCK_DGRAM));
  socket1->Bind(kIPv4AnyAddress);
  socket2->Bind(kIPv4AnyAddress);
  auto client1 = std::make_unique<TestClient>(
      std::make_unique<AsyncUDPSocket>(socket1), &fake_clock_);

  ss_.SetSendingBlocked(true);
  EXPECT_EQ(-1, client1->SendTo("foo", 3, socket2->GetLocalAddress()));
  EXPECT_TRUE(socket1->IsBlocking());
  EXPECT_EQ(0, client1->ready_to_send_count());

  ss_.SetSendingBlocked(false);
  EXPECT_EQ(1, client1->ready_to_send_count());
  EXPECT_EQ(3, client1->SendTo("foo", 3, socket2->GetLocalAddress()));
}

TEST_F(VirtualSocketServerTest, SetSendingBlockedWithTcpSocket) {
  constexpr size_t kBufferSize = 1024;
  ss_.set_send_buffer_capacity(kBufferSize);
  ss_.set_recv_buffer_capacity(kBufferSize);

  StreamSink sink;
  std::unique_ptr<Socket> socket1 =
      absl::WrapUnique(ss_.CreateSocket(kIPv4AnyAddress.family(), SOCK_STREAM));
  std::unique_ptr<Socket> socket2 =
      absl::WrapUnique(ss_.CreateSocket(kIPv4AnyAddress.family(), SOCK_STREAM));
  sink.Monitor(socket1.get());
  sink.Monitor(socket2.get());
  socket1->Bind(kIPv4AnyAddress);
  socket2->Bind(kIPv4AnyAddress);

  // Connect sockets.
  EXPECT_EQ(0, socket1->Connect(socket2->GetLocalAddress()));
  EXPECT_EQ(0, socket2->Connect(socket1->GetLocalAddress()));
  ss_.ProcessMessagesUntilIdle();

  char data[kBufferSize] = {};

  // First Send call will fill the send buffer but not send anything.
  ss_.SetSendingBlocked(true);
  EXPECT_EQ(static_cast<int>(kBufferSize), socket1->Send(data, kBufferSize));
  ss_.ProcessMessagesUntilIdle();
  EXPECT_FALSE(sink.Check(socket1.get(), SSE_WRITE));
  EXPECT_FALSE(sink.Check(socket2.get(), SSE_READ));
  EXPECT_FALSE(socket1->IsBlocking());

  // Since the send buffer is full, next Send will result in EWOULDBLOCK.
  EXPECT_EQ(-1, socket1->Send(data, kBufferSize));
  EXPECT_FALSE(sink.Check(socket1.get(), SSE_WRITE));
  EXPECT_FALSE(sink.Check(socket2.get(), SSE_READ));
  EXPECT_TRUE(socket1->IsBlocking());

  // When sending is unblocked, the buffered data should be sent and
  // SignalWriteEvent should fire.
  ss_.SetSendingBlocked(false);
  ss_.ProcessMessagesUntilIdle();
  EXPECT_TRUE(sink.Check(socket1.get(), SSE_WRITE));
  EXPECT_TRUE(sink.Check(socket2.get(), SSE_READ));
}

TEST_F(VirtualSocketServerTest, CreatesStandardDistribution) {
  const uint32_t kTestMean[] = {10, 100, 333, 1000};
  const double kTestDev[] = {0.25, 0.1, 0.01};
  // TODO(deadbeef): The current code only works for 1000 data points or more.
  const uint32_t kTestSamples[] = {/*10, 100,*/ 1000};
  for (size_t midx = 0; midx < arraysize(kTestMean); ++midx) {
    for (size_t didx = 0; didx < arraysize(kTestDev); ++didx) {
      for (size_t sidx = 0; sidx < arraysize(kTestSamples); ++sidx) {
        ASSERT_LT(0u, kTestSamples[sidx]);
        const uint32_t kStdDev =
            static_cast<uint32_t>(kTestDev[didx] * kTestMean[midx]);
        std::unique_ptr<VirtualSocketServer::Function> f =
            VirtualSocketServer::CreateDistribution(kTestMean[midx], kStdDev,
                                                    kTestSamples[sidx]);
        ASSERT_TRUE(nullptr != f.get());
        ASSERT_EQ(kTestSamples[sidx], f->size());
        double sum = 0;
        for (uint32_t i = 0; i < f->size(); ++i) {
          sum += (*f)[i].second;
        }
        const double mean = sum / f->size();
        double sum_sq_dev = 0;
        for (uint32_t i = 0; i < f->size(); ++i) {
          double dev = (*f)[i].second - mean;
          sum_sq_dev += dev * dev;
        }
        const double stddev = sqrt(sum_sq_dev / f->size());
        EXPECT_NEAR(kTestMean[midx], mean, 0.1 * kTestMean[midx])
            << "M=" << kTestMean[midx] << " SD=" << kStdDev
            << " N=" << kTestSamples[sidx];
        EXPECT_NEAR(kStdDev, stddev, 0.1 * kStdDev)
            << "M=" << kTestMean[midx] << " SD=" << kStdDev
            << " N=" << kTestSamples[sidx];
      }
    }
  }
}

}  // namespace
}  // namespace rtc
