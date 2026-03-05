/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/openssl_adapter.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/test/rtc_error_matchers.h"      // IWYU pragma: keep
#include "api/units/time_delta.h"             // IWYU pragma: keep
#include "rtc_base/logging.h"                 // IWYU pragma: keep
#include "rtc_base/net_helpers.h"             // IWYU pragma: keep
#include "rtc_base/physical_socket_server.h"  // IWYU pragma: keep
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/ssl_stream_adapter.h"           // IWYU pragma: keep
#include "rtc_base/third_party/sigslot/sigslot.h"  // IWYU pragma: keep
#include "rtc_base/thread.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/wait_until.h"  // IWYU pragma: keep

namespace webrtc {
namespace {

class MockAsyncSocket : public Socket {
 public:
  ~MockAsyncSocket() override = default;
  MOCK_METHOD(Socket*, Accept, (SocketAddress*), (override));
  MOCK_METHOD(SocketAddress, GetLocalAddress, (), (const, override));
  MOCK_METHOD(SocketAddress, GetRemoteAddress, (), (const, override));
  MOCK_METHOD(int, Bind, (const SocketAddress&), (override));
  MOCK_METHOD(int, Connect, (const SocketAddress&), (override));
  MOCK_METHOD(int, Send, (const void*, size_t), (override));
  MOCK_METHOD(int,
              SendTo,
              (const void*, size_t, const SocketAddress&),
              (override));
  MOCK_METHOD(int, Recv, (void*, size_t, int64_t*), (override));
  MOCK_METHOD(int,
              RecvFrom,
              (void*, size_t, SocketAddress*, int64_t*),
              (override));
  MOCK_METHOD(int, Listen, (int), (override));
  MOCK_METHOD(int, Close, (), (override));
  MOCK_METHOD(int, GetError, (), (const, override));
  MOCK_METHOD(void, SetError, (int), (override));
  MOCK_METHOD(ConnState, GetState, (), (const, override));
  MOCK_METHOD(int, GetOption, (Option, int*), (override));
  MOCK_METHOD(int, SetOption, (Option, int), (override));
};

class MockCertVerifier : public SSLCertificateVerifier {
 public:
  ~MockCertVerifier() override = default;
  MOCK_METHOD(bool, Verify, (const SSLCertificate&), (override));
};

#if defined(WEBRTC_EXCLUDE_BUILT_IN_SSL_ROOT_CERTS)
// Helper class to handle SSL connection events and state for testing.
class SSLConnectionHandler : public sigslot::has_slots<> {
 public:
  explicit SSLConnectionHandler(absl::string_view hostname)
      : hostname_(hostname) {}
  void OnConnectEvent(Socket* socket) {
    RTC_LOG(LS_INFO) << "OnConnectEvent - Socket state: " << socket->GetState();
    OpenSSLAdapter* ssl_socket = static_cast<OpenSSLAdapter*>(socket);
    if (!ssl_started_ && ssl_socket->GetState() == Socket::CS_CONNECTED) {
      ssl_started_ = true;
      RTC_LOG(LS_INFO) << "TCP connected, starting SSL handshake...";
      int result = ssl_socket->StartSSL(hostname_);
      if (result != 0) {
        RTC_LOG(LS_ERROR) << "StartSSL failed with error: " << result;
        has_error_ = true;
      }
    } else if (ssl_started_ && ssl_socket->GetState() == Socket::CS_CONNECTED) {
      RTC_LOG(LS_INFO) << "SSL handshake completed!";
      ssl_connected_ = true;
    }
  }
  void OnReadEvent(Socket* socket) {
    RTC_LOG(LS_INFO) << "OnReadEvent - Socket state: " << socket->GetState();
    if (ssl_started_ && !ssl_connected_) {
      RTC_LOG(LS_INFO) << "SSL handshake completed via ReadEvent!";
      ssl_connected_ = true;
    }
  }
  void OnCloseEvent(Socket* socket, int err) {
    RTC_LOG(LS_INFO) << "OnCloseEvent - error: " << err
                     << ", state: " << socket->GetState();
    if (err != 0) {
      has_error_ = true;
    }
  }
  bool IsSSLConnected() const { return ssl_connected_; }
  bool HasError() const { return has_error_; }

 private:
  const absl::string_view hostname_;
  bool ssl_started_ = false;
  bool ssl_connected_ = false;
  bool has_error_ = false;
};

#endif  // WEBRTC_EXCLUDE_BUILT_IN_SSL_ROOT_CERTS

}  // namespace

using ::testing::_;
using ::testing::Return;

TEST(OpenSSLAdapterTest, TestTransformAlpnProtocols) {
  EXPECT_EQ("", TransformAlpnProtocols(std::vector<std::string>()));

  // Protocols larger than 255 characters (whose size can't be fit in a byte),
  // can't be converted, and an empty string will be returned.
  std::string large_protocol(256, 'a');
  EXPECT_EQ("",
            TransformAlpnProtocols(std::vector<std::string>{large_protocol}));

  // One protocol test.
  std::vector<std::string> alpn_protos{"h2"};
  std::stringstream expected_response;
  expected_response << static_cast<char>(2) << "h2";
  EXPECT_EQ(expected_response.str(), TransformAlpnProtocols(alpn_protos));

  // Standard protocols test (h2,http/1.1).
  alpn_protos.push_back("http/1.1");
  expected_response << static_cast<char>(8) << "http/1.1";
  EXPECT_EQ(expected_response.str(), TransformAlpnProtocols(alpn_protos));
}

// Verifies that SSLStart works when OpenSSLAdapter is started in standalone
// mode.
TEST(OpenSSLAdapterTest, TestBeginSSLBeforeConnection) {
  AutoThread main_thread;
  Socket* async_socket = new MockAsyncSocket();
  OpenSSLAdapter adapter(async_socket);
  EXPECT_EQ(adapter.StartSSL("webrtc.org"), 0);
}

#if defined(WEBRTC_EXCLUDE_BUILT_IN_SSL_ROOT_CERTS)
// This test is for validation of https://bugs.webrtc.org/451479054
// Since this path is not normally tested by bots, manually set
// rtc_builtin_ssl_root_certificates=false in GN to
// build and run this test.
TEST(OpenSSLAdaptorTest, TestRealSSLConnection) {
  PhysicalSocketServer socket_server;
  AutoSocketServerThread main_thread(&socket_server);

  constexpr absl::string_view kHostname = "webrtc.org";
  constexpr int kPort = 443;
  constexpr TimeDelta kTimeout = TimeDelta::Millis(10000);

  Socket* async_socket = socket_server.CreateSocket(AF_INET, SOCK_STREAM);
  ASSERT_NE(async_socket, nullptr);

  std::unique_ptr<MockCertVerifier> mock_verifier =
      std::make_unique<MockCertVerifier>();
  EXPECT_CALL(*mock_verifier, Verify(_))
      .WillRepeatedly([]([[maybe_unused]] const SSLCertificate& cert) {
        RTC_LOG(LS_INFO) << "MockCertVerifier: assuming certificate is valid";
        return true;
      });

  std::unique_ptr<OpenSSLAdapter> ssl_adapter =
      std::make_unique<OpenSSLAdapter>(
          async_socket, /* ssl_session_cache=*/nullptr,
          /* ssl_cert_verifier=*/mock_verifier.get());
  ssl_adapter->SetRole(SSL_CLIENT);

  SSLConnectionHandler handler(kHostname);
  ssl_adapter->SubscribeConnectEvent(
      &handler, [&handler](Socket* socket) { handler.OnConnectEvent(socket); });
  ssl_adapter->SignalReadEvent.connect(&handler,
                                       &SSLConnectionHandler::OnReadEvent);
  ssl_adapter->SubscribeCloseEvent(&handler,
                                   [&handler](Socket* socket, int error) {
                                     handler.OnCloseEvent(socket, error);
                                   });

  SocketAddress addr(kHostname, kPort);
  int connect_result = ssl_adapter->Connect(addr);
  EXPECT_TRUE(connect_result == 0 || ssl_adapter->IsBlocking());

  // Wait for SSL handshake to complete.
  EXPECT_THAT(WaitUntil([&] { return handler.IsSSLConnected(); },
                        ::testing::IsTrue(), {.timeout = kTimeout}),
              IsRtcOk())
      << "SSL handshake failed. Socket state: " << ssl_adapter->GetState()
      << ", Has error: " << handler.HasError();

  // Verify the connection is established.
  EXPECT_EQ(ssl_adapter->GetState(), Socket::CS_CONNECTED);
  EXPECT_FALSE(handler.HasError());

  RTC_LOG(LS_INFO) << "SSL handshake completed successfully!";
}

#endif  // WEBRTC_EXCLUDE_BUILT_IN_SSL_ROOT_CERTS

// Verifies that the adapter factory can create new adapters.
TEST(OpenSSLAdapterFactoryTest, CreateSingleOpenSSLAdapter) {
  AutoThread main_thread;
  OpenSSLAdapterFactory adapter_factory;
  Socket* async_socket = new MockAsyncSocket();
  auto simple_adapter = std::unique_ptr<OpenSSLAdapter>(
      adapter_factory.CreateAdapter(async_socket));
  EXPECT_NE(simple_adapter, nullptr);
}

// Verifies that setting a custom verifier still allows for adapters to be
// created.
TEST(OpenSSLAdapterFactoryTest, CreateWorksWithCustomVerifier) {
  AutoThread main_thread;
  MockCertVerifier* mock_verifier = new MockCertVerifier();
  EXPECT_CALL(*mock_verifier, Verify(_)).WillRepeatedly(Return(true));
  auto cert_verifier = std::unique_ptr<SSLCertificateVerifier>(mock_verifier);

  OpenSSLAdapterFactory adapter_factory;
  adapter_factory.SetCertVerifier(cert_verifier.get());
  Socket* async_socket = new MockAsyncSocket();
  auto simple_adapter = std::unique_ptr<OpenSSLAdapter>(
      adapter_factory.CreateAdapter(async_socket));
  EXPECT_NE(simple_adapter, nullptr);
}

}  // namespace webrtc
