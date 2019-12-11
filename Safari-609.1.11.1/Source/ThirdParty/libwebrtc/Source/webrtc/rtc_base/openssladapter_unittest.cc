/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <sstream>
#include <string>
#include <vector>

#include "absl/memory/memory.h"
#include "rtc_base/asyncsocket.h"
#include "rtc_base/gunit.h"
#include "rtc_base/openssladapter.h"
#include "test/gmock.h"

namespace rtc {
namespace {

class MockAsyncSocket : public AsyncSocket {
 public:
  virtual ~MockAsyncSocket() = default;
  MOCK_METHOD1(Accept, AsyncSocket*(SocketAddress*));
  MOCK_CONST_METHOD0(GetLocalAddress, SocketAddress());
  MOCK_CONST_METHOD0(GetRemoteAddress, SocketAddress());
  MOCK_METHOD1(Bind, int(const SocketAddress&));
  MOCK_METHOD1(Connect, int(const SocketAddress&));
  MOCK_METHOD2(Send, int(const void*, size_t));
  MOCK_METHOD3(SendTo, int(const void*, size_t, const SocketAddress&));
  MOCK_METHOD3(Recv, int(void*, size_t, int64_t*));
  MOCK_METHOD4(RecvFrom, int(void*, size_t, SocketAddress*, int64_t*));
  MOCK_METHOD1(Listen, int(int));
  MOCK_METHOD0(Close, int());
  MOCK_CONST_METHOD0(GetError, int());
  MOCK_METHOD1(SetError, void(int));
  MOCK_CONST_METHOD0(GetState, ConnState());
  MOCK_METHOD2(GetOption, int(Option, int*));
  MOCK_METHOD2(SetOption, int(Option, int));
};

class MockCertVerifier : public SSLCertificateVerifier {
 public:
  virtual ~MockCertVerifier() = default;
  MOCK_METHOD1(Verify, bool(const SSLCertificate&));
};

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
  AsyncSocket* async_socket = new MockAsyncSocket();
  OpenSSLAdapter adapter(async_socket);
  EXPECT_EQ(adapter.StartSSL("webrtc.org", false), 0);
}

// Verifies that the adapter factory can create new adapters.
TEST(OpenSSLAdapterFactoryTest, CreateSingleOpenSSLAdapter) {
  OpenSSLAdapterFactory adapter_factory;
  AsyncSocket* async_socket = new MockAsyncSocket();
  auto simple_adapter = std::unique_ptr<OpenSSLAdapter>(
      adapter_factory.CreateAdapter(async_socket));
  EXPECT_NE(simple_adapter, nullptr);
}

// Verifies that setting a custom verifier still allows for adapters to be
// created.
TEST(OpenSSLAdapterFactoryTest, CreateWorksWithCustomVerifier) {
  MockCertVerifier* mock_verifier = new MockCertVerifier();
  EXPECT_CALL(*mock_verifier, Verify(_)).WillRepeatedly(Return(true));
  auto cert_verifier = std::unique_ptr<SSLCertificateVerifier>(mock_verifier);

  OpenSSLAdapterFactory adapter_factory;
  adapter_factory.SetCertVerifier(cert_verifier.get());
  AsyncSocket* async_socket = new MockAsyncSocket();
  auto simple_adapter = std::unique_ptr<OpenSSLAdapter>(
      adapter_factory.CreateAdapter(async_socket));
  EXPECT_NE(simple_adapter, nullptr);
}

}  // namespace rtc
