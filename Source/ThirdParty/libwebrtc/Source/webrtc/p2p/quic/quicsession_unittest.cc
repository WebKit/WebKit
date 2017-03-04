/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/quic/quicsession.h"

#include <memory>
#include <string>
#include <vector>

#include "net/base/ip_endpoint.h"
#include "net/quic/crypto/crypto_server_config_protobuf.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/crypto/proof_source.h"
#include "net/quic/crypto/proof_verifier.h"
#include "net/quic/crypto/quic_crypto_client_config.h"
#include "net/quic/crypto/quic_crypto_server_config.h"
#include "net/quic/quic_crypto_client_stream.h"
#include "net/quic/quic_crypto_server_stream.h"
#include "webrtc/base/gunit.h"
#include "webrtc/p2p/base/faketransportcontroller.h"
#include "webrtc/p2p/quic/quicconnectionhelper.h"
#include "webrtc/p2p/quic/reliablequicstream.h"

using net::IPAddress;
using net::IPEndPoint;
using net::PerPacketOptions;
using net::Perspective;
using net::ProofVerifyContext;
using net::ProofVerifyDetails;
using net::QuicByteCount;
using net::QuicClock;
using net::QuicCompressedCertsCache;
using net::QuicConfig;
using net::QuicConnection;
using net::QuicCryptoClientConfig;
using net::QuicCryptoServerConfig;
using net::QuicCryptoClientStream;
using net::QuicCryptoServerStream;
using net::QuicCryptoStream;
using net::QuicErrorCode;
using net::QuicPacketWriter;
using net::QuicRandom;
using net::QuicServerConfigProtobuf;
using net::QuicServerId;
using net::QuicStreamId;
using net::WriteResult;
using net::WriteStatus;

using cricket::FakeTransportChannel;
using cricket::QuicConnectionHelper;
using cricket::QuicSession;
using cricket::ReliableQuicStream;
using cricket::TransportChannel;

using rtc::Thread;

// Timeout for running asynchronous operations within unit tests.
static const int kTimeoutMs = 1000;
// Testing SpdyPriority value for creating outgoing ReliableQuicStream.
static const uint8_t kDefaultPriority = 3;
// TExport keying material function
static const char kExporterLabel[] = "label";
static const char kExporterContext[] = "context";
static const size_t kExporterContextLen = sizeof(kExporterContext);
// Identifies QUIC server session
static const QuicServerId kServerId("www.google.com", 443);

// Used by QuicCryptoServerConfig to provide server credentials, returning a
// canned response equal to |success|.
class FakeProofSource : public net::ProofSource {
 public:
  explicit FakeProofSource(bool success) : success_(success) {}

  // ProofSource override.
  bool GetProof(const IPAddress& server_ip,
                const std::string& hostname,
                const std::string& server_config,
                net::QuicVersion quic_version,
                base::StringPiece chlo_hash,
                bool ecdsa_ok,
                scoped_refptr<net::ProofSource::Chain>* out_certs,
                std::string* out_signature,
                std::string* out_leaf_cert_sct) override {
    if (success_) {
      std::vector<std::string> certs;
      certs.push_back("Required to establish handshake");
      *out_certs = new ProofSource::Chain(certs);
      *out_signature = "Signature";
      *out_leaf_cert_sct = "Time";
    }
    return success_;
  }

 private:
  // Whether or not obtaining proof source succeeds.
  bool success_;
};

// Used by QuicCryptoClientConfig to verify server credentials, returning a
// canned response of QUIC_SUCCESS if |success| is true.
class FakeProofVerifier : public net::ProofVerifier {
 public:
  explicit FakeProofVerifier(bool success) : success_(success) {}

  // ProofVerifier override
  net::QuicAsyncStatus VerifyProof(
      const std::string& hostname,
      const uint16_t port,
      const std::string& server_config,
      net::QuicVersion quic_version,
      base::StringPiece chlo_hash,
      const std::vector<std::string>& certs,
      const std::string& cert_sct,
      const std::string& signature,
      const ProofVerifyContext* context,
      std::string* error_details,
      std::unique_ptr<net::ProofVerifyDetails>* verify_details,
      net::ProofVerifierCallback* callback) override {
    return success_ ? net::QUIC_SUCCESS : net::QUIC_FAILURE;
  }

 private:
  // Whether or not proof verification succeeds.
  bool success_;
};

// Writes QUIC packets to a fake transport channel that simulates a network.
class FakeQuicPacketWriter : public QuicPacketWriter {
 public:
  explicit FakeQuicPacketWriter(FakeTransportChannel* fake_channel)
      : fake_channel_(fake_channel) {}

  // Sends packets across the network.
  WriteResult WritePacket(const char* buffer,
                          size_t buf_len,
                          const IPAddress& self_address,
                          const IPEndPoint& peer_address,
                          PerPacketOptions* options) override {
    rtc::PacketOptions packet_options;
    int rv = fake_channel_->SendPacket(buffer, buf_len, packet_options, 0);
    net::WriteStatus status;
    if (rv > 0) {
      status = net::WRITE_STATUS_OK;
    } else if (fake_channel_->GetError() == EWOULDBLOCK) {
      status = net::WRITE_STATUS_BLOCKED;
    } else {
      status = net::WRITE_STATUS_ERROR;
    }
    return net::WriteResult(status, rv);
  }

  // Returns true if the writer buffers and subsequently rewrites data
  // when an attempt to write results in the underlying socket becoming
  // write blocked.
  bool IsWriteBlockedDataBuffered() const override { return true; }

  // Returns true if the network socket is not writable.
  bool IsWriteBlocked() const override { return !fake_channel_->writable(); }

  // Records that the socket has become writable, for example when an EPOLLOUT
  // is received or an asynchronous write completes.
  void SetWritable() override { fake_channel_->SetWritable(true); }

  // Returns the maximum size of the packet which can be written using this
  // writer for the supplied peer address.  This size may actually exceed the
  // size of a valid QUIC packet.
  QuicByteCount GetMaxPacketSize(
      const IPEndPoint& peer_address) const override {
    return net::kMaxPacketSize;
  }

 private:
  FakeTransportChannel* fake_channel_;
};

// Wrapper for QuicSession and transport channel that stores incoming data.
class QuicSessionForTest : public QuicSession {
 public:
  QuicSessionForTest(std::unique_ptr<net::QuicConnection> connection,
                     const net::QuicConfig& config,
                     std::unique_ptr<FakeTransportChannel> channel)
      : QuicSession(std::move(connection), config),
        channel_(std::move(channel)) {
    channel_->SignalReadPacket.connect(
        this, &QuicSessionForTest::OnChannelReadPacket);
  }

  // Called when channel has packets to read.
  void OnChannelReadPacket(TransportChannel* channel,
                           const char* data,
                           size_t size,
                           const rtc::PacketTime& packet_time,
                           int flags) {
    OnReadPacket(data, size);
  }

  // Called when peer receives incoming stream from another peer.
  void OnIncomingStream(ReliableQuicStream* stream) {
    stream->SignalDataReceived.connect(this,
                                       &QuicSessionForTest::OnDataReceived);
    last_incoming_stream_ = stream;
  }

  // Called when peer has data to read from incoming stream.
  void OnDataReceived(net::QuicStreamId id, const char* data, size_t length) {
    last_received_data_ = std::string(data, length);
  }

  std::string data() { return last_received_data_; }

  bool has_data() { return data().size() > 0; }

  FakeTransportChannel* channel() { return channel_.get(); }

  ReliableQuicStream* incoming_stream() { return last_incoming_stream_; }

 private:
  // Transports QUIC packets to/from peer.
  std::unique_ptr<FakeTransportChannel> channel_;
  // Stores data received by peer once it is sent from the other peer.
  std::string last_received_data_;
  // Handles incoming streams from sender.
  ReliableQuicStream* last_incoming_stream_ = nullptr;
};

// Simulates data transfer between two peers using QUIC.
class QuicSessionTest : public ::testing::Test,
                        public QuicCryptoClientStream::ProofHandler {
 public:
  QuicSessionTest()
      : quic_helper_(rtc::Thread::Current()),
        quic_compressed_certs_cache_(
            QuicCompressedCertsCache::kQuicCompressedCertsCacheSize) {}

  // Instantiates |client_peer_| and |server_peer_|.
  void CreateClientAndServerSessions();

  std::unique_ptr<QuicSessionForTest> CreateSession(
      std::unique_ptr<FakeTransportChannel> channel,
      Perspective perspective);

  QuicCryptoClientStream* CreateCryptoClientStream(QuicSessionForTest* session,
                                                   bool handshake_success);
  QuicCryptoServerStream* CreateCryptoServerStream(QuicSessionForTest* session,
                                                   bool handshake_success);

  std::unique_ptr<QuicConnection> CreateConnection(
      FakeTransportChannel* channel,
      Perspective perspective);

  void StartHandshake(bool client_handshake_success,
                      bool server_handshake_success);

  // Test handshake establishment and sending/receiving of data.
  void TestStreamConnection(QuicSessionForTest* from_session,
                            QuicSessionForTest* to_session);
  // Test that client and server are not connected after handshake failure.
  void TestDisconnectAfterFailedHandshake();

  // QuicCryptoClientStream::ProofHelper overrides.
  void OnProofValid(
      const QuicCryptoClientConfig::CachedState& cached) override {}
  void OnProofVerifyDetailsAvailable(
      const ProofVerifyDetails& verify_details) override {}

 protected:
  QuicConnectionHelper quic_helper_;
  QuicConfig config_;
  QuicClock clock_;
  QuicCompressedCertsCache quic_compressed_certs_cache_;

  std::unique_ptr<QuicSessionForTest> client_peer_;
  std::unique_ptr<QuicSessionForTest> server_peer_;
};

// Initializes "client peer" who begins crypto handshake and "server peer" who
// establishes encryption with client.
void QuicSessionTest::CreateClientAndServerSessions() {
  std::unique_ptr<FakeTransportChannel> channel1(
      new FakeTransportChannel("channel1", 0));
  std::unique_ptr<FakeTransportChannel> channel2(
      new FakeTransportChannel("channel2", 0));

  // Prevent channel1->OnReadPacket and channel2->OnReadPacket from calling
  // themselves in a loop, which causes to future packets to be recursively
  // consumed while the current thread blocks consumption of current ones.
  channel2->SetAsync(true);

  // Configure peers to send packets to each other.
  channel1->SetDestination(channel2.get());

  client_peer_ = CreateSession(std::move(channel1), Perspective::IS_CLIENT);
  server_peer_ = CreateSession(std::move(channel2), Perspective::IS_SERVER);
}

std::unique_ptr<QuicSessionForTest> QuicSessionTest::CreateSession(
    std::unique_ptr<FakeTransportChannel> channel,
    Perspective perspective) {
  std::unique_ptr<QuicConnection> quic_connection =
      CreateConnection(channel.get(), perspective);
  return std::unique_ptr<QuicSessionForTest>(new QuicSessionForTest(
      std::move(quic_connection), config_, std::move(channel)));
}

QuicCryptoClientStream* QuicSessionTest::CreateCryptoClientStream(
    QuicSessionForTest* session,
    bool handshake_success) {
  QuicCryptoClientConfig* client_config =
      new QuicCryptoClientConfig(new FakeProofVerifier(handshake_success));
  return new QuicCryptoClientStream(
      kServerId, session, new ProofVerifyContext(), client_config, this);
}

QuicCryptoServerStream* QuicSessionTest::CreateCryptoServerStream(
    QuicSessionForTest* session,
    bool handshake_success) {
  QuicCryptoServerConfig* server_config =
      new QuicCryptoServerConfig("TESTING", QuicRandom::GetInstance(),
                                 new FakeProofSource(handshake_success));
  // Provide server with serialized config string to prove ownership.
  QuicCryptoServerConfig::ConfigOptions options;
  QuicServerConfigProtobuf* primary_config = server_config->GenerateConfig(
      QuicRandom::GetInstance(), &clock_, options);
  server_config->AddConfig(primary_config, clock_.WallNow());
  bool use_stateless_rejects_if_peer_supported = false;
  return new QuicCryptoServerStream(
      server_config, &quic_compressed_certs_cache_,
      use_stateless_rejects_if_peer_supported, session);
}

std::unique_ptr<QuicConnection> QuicSessionTest::CreateConnection(
    FakeTransportChannel* channel,
    Perspective perspective) {
  FakeQuicPacketWriter* writer = new FakeQuicPacketWriter(channel);

  IPAddress ip(0, 0, 0, 0);
  bool owns_writer = true;

  return std::unique_ptr<QuicConnection>(new QuicConnection(
      0, net::IPEndPoint(ip, 0), &quic_helper_, writer, owns_writer,
      perspective, net::QuicSupportedVersions()));
}

void QuicSessionTest::StartHandshake(bool client_handshake_success,
                                     bool server_handshake_success) {
  server_peer_->StartServerHandshake(
      CreateCryptoServerStream(server_peer_.get(), server_handshake_success));
  client_peer_->StartClientHandshake(
      CreateCryptoClientStream(client_peer_.get(), client_handshake_success));
}

void QuicSessionTest::TestStreamConnection(QuicSessionForTest* from_session,
                                           QuicSessionForTest* to_session) {
  // Wait for crypto handshake to finish then check if encryption established.
  ASSERT_TRUE_WAIT(from_session->IsCryptoHandshakeConfirmed() &&
                       to_session->IsCryptoHandshakeConfirmed(),
                   kTimeoutMs);

  ASSERT_TRUE(from_session->IsEncryptionEstablished());
  ASSERT_TRUE(to_session->IsEncryptionEstablished());

  std::string from_key;
  std::string to_key;

  bool from_success = from_session->ExportKeyingMaterial(
      kExporterLabel, kExporterContext, kExporterContextLen, &from_key);
  ASSERT_TRUE(from_success);
  bool to_success = to_session->ExportKeyingMaterial(
      kExporterLabel, kExporterContext, kExporterContextLen, &to_key);
  ASSERT_TRUE(to_success);

  EXPECT_EQ(from_key.size(), kExporterContextLen);
  EXPECT_EQ(from_key, to_key);

  // Now we can establish encrypted outgoing stream.
  ReliableQuicStream* outgoing_stream =
      from_session->CreateOutgoingDynamicStream(kDefaultPriority);
  ASSERT_NE(nullptr, outgoing_stream);
  EXPECT_TRUE(from_session->HasOpenDynamicStreams());

  outgoing_stream->SignalDataReceived.connect(
      from_session, &QuicSessionForTest::OnDataReceived);
  to_session->SignalIncomingStream.connect(
      to_session, &QuicSessionForTest::OnIncomingStream);

  // Send a test message from peer 1 to peer 2.
  const char kTestMessage[] = "Hello, World!";
  outgoing_stream->Write(kTestMessage, strlen(kTestMessage));

  // Wait for peer 2 to receive messages.
  ASSERT_TRUE_WAIT(to_session->has_data(), kTimeoutMs);

  ReliableQuicStream* incoming = to_session->incoming_stream();
  ASSERT_TRUE(incoming);
  EXPECT_TRUE(to_session->HasOpenDynamicStreams());

  EXPECT_EQ(to_session->data(), kTestMessage);

  // Send a test message from peer 2 to peer 1.
  const char kTestResponse[] = "Response";
  incoming->Write(kTestResponse, strlen(kTestResponse));

  // Wait for peer 1 to receive messages.
  ASSERT_TRUE_WAIT(from_session->has_data(), kTimeoutMs);

  EXPECT_EQ(from_session->data(), kTestResponse);
}

// Client and server should disconnect when proof verification fails.
void QuicSessionTest::TestDisconnectAfterFailedHandshake() {
  EXPECT_TRUE_WAIT(!client_peer_->connection()->connected(), kTimeoutMs);
  EXPECT_TRUE_WAIT(!server_peer_->connection()->connected(), kTimeoutMs);

  EXPECT_FALSE(client_peer_->IsEncryptionEstablished());
  EXPECT_FALSE(client_peer_->IsCryptoHandshakeConfirmed());

  EXPECT_FALSE(server_peer_->IsEncryptionEstablished());
  EXPECT_FALSE(server_peer_->IsCryptoHandshakeConfirmed());
}

// Establish encryption then send message from client to server.
TEST_F(QuicSessionTest, ClientToServer) {
  CreateClientAndServerSessions();
  StartHandshake(true, true);
  TestStreamConnection(client_peer_.get(), server_peer_.get());
}

// Establish encryption then send message from server to client.
TEST_F(QuicSessionTest, ServerToClient) {
  CreateClientAndServerSessions();
  StartHandshake(true, true);
  TestStreamConnection(server_peer_.get(), client_peer_.get());
}

// Make client fail to verify proof from server.
TEST_F(QuicSessionTest, ClientRejection) {
  CreateClientAndServerSessions();
  StartHandshake(false, true);
  TestDisconnectAfterFailedHandshake();
}

// Make server fail to give proof to client.
TEST_F(QuicSessionTest, ServerRejection) {
  CreateClientAndServerSessions();
  StartHandshake(true, false);
  TestDisconnectAfterFailedHandshake();
}

// Test that data streams are not created before handshake.
TEST_F(QuicSessionTest, CannotCreateDataStreamBeforeHandshake) {
  CreateClientAndServerSessions();
  EXPECT_EQ(nullptr, server_peer_->CreateOutgoingDynamicStream(5));
  EXPECT_EQ(nullptr, client_peer_->CreateOutgoingDynamicStream(5));
}

// Test that closing a QUIC stream causes the QuicSession to remove it.
TEST_F(QuicSessionTest, CloseQuicStream) {
  CreateClientAndServerSessions();
  StartHandshake(true, true);
  ASSERT_TRUE_WAIT(client_peer_->IsCryptoHandshakeConfirmed() &&
                       server_peer_->IsCryptoHandshakeConfirmed(),
                   kTimeoutMs);
  ReliableQuicStream* stream = client_peer_->CreateOutgoingDynamicStream(5);
  ASSERT_NE(nullptr, stream);
  EXPECT_FALSE(client_peer_->IsClosedStream(stream->id()));
  stream->Close();
  EXPECT_TRUE(client_peer_->IsClosedStream(stream->id()));
}
