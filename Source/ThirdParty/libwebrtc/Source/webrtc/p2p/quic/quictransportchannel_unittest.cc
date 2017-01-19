/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/quic/quictransportchannel.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "webrtc/base/common.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/sslidentity.h"
#include "webrtc/p2p/base/faketransportcontroller.h"

using cricket::ConnectionRole;
using cricket::IceRole;
using cricket::QuicTransportChannel;
using cricket::ReliableQuicStream;
using cricket::TransportChannel;
using cricket::TransportDescription;

// Timeout in milliseconds for asynchronous operations in unit tests.
static const int kTimeoutMs = 1000;

// Export keying material parameters.
static const char kExporterLabel[] = "label";
static const uint8_t kExporterContext[] = "context";
static const size_t kExporterContextLength = sizeof(kExporterContext);
static const size_t kOutputKeyLength = 20;

// Packet size for SRTP.
static const size_t kPacketSize = 100;

// Indicates ICE channel has no write error.
static const int kNoWriteError = 0;

// ICE parameters.
static const char kIceUfrag[] = "TESTICEUFRAG0001";
static const char kIcePwd[] = "TESTICEPWD00000000000001";

// QUIC packet parameters.
static const net::IPAddress kIpAddress(0, 0, 0, 0);
static const net::IPEndPoint kIpEndpoint(kIpAddress, 0);

// Detects incoming RTP packets.
static bool IsRtpLeadByte(uint8_t b) {
  return (b & 0xC0) == 0x80;
}

// Maps SSL role to ICE connection role. The peer with a client role is assumed
// to be the one who initiates the connection.
static ConnectionRole SslRoleToConnectionRole(rtc::SSLRole ssl_role) {
  return (ssl_role == rtc::SSL_CLIENT) ? cricket::CONNECTIONROLE_ACTIVE
                                       : cricket::CONNECTIONROLE_PASSIVE;
}

// Allows cricket::FakeTransportChannel to simulate write blocked
// and write error states.
// TODO(mikescarlett): Add this functionality to cricket::FakeTransportChannel.
class FailableTransportChannel : public cricket::FakeTransportChannel {
 public:
  FailableTransportChannel(const std::string& name, int component)
      : cricket::FakeTransportChannel(name, component), error_(kNoWriteError) {}
  int GetError() override { return error_; }
  void SetError(int error) { error_ = error; }
  int SendPacket(const char* data,
                 size_t len,
                 const rtc::PacketOptions& options,
                 int flags) override {
    if (error_ == kNoWriteError) {
      return cricket::FakeTransportChannel::SendPacket(data, len, options,
                                                       flags);
    }
    return -1;
  }

 private:
  int error_;
};

// Peer who establishes a handshake using a QuicTransportChannel, which wraps
// a FailableTransportChannel to simulate network connectivity and ICE
// negotiation.
class QuicTestPeer : public sigslot::has_slots<> {
 public:
  explicit QuicTestPeer(const std::string& name)
      : name_(name),
        bytes_sent_(0),
        ice_channel_(new FailableTransportChannel(name_, 0)),
        quic_channel_(ice_channel_),
        incoming_stream_count_(0) {
    quic_channel_.SignalReadPacket.connect(
        this, &QuicTestPeer::OnTransportChannelReadPacket);
    quic_channel_.SignalIncomingStream.connect(this,
                                               &QuicTestPeer::OnIncomingStream);
    quic_channel_.SignalClosed.connect(this, &QuicTestPeer::OnClosed);
    ice_channel_->SetAsync(true);
    rtc::scoped_refptr<rtc::RTCCertificate> local_cert =
        rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
            rtc::SSLIdentity::Generate(name_, rtc::KT_DEFAULT)));
    quic_channel_.SetLocalCertificate(local_cert);
    local_fingerprint_.reset(CreateFingerprint(local_cert.get()));
  }

  // Connects |ice_channel_| to that of the other peer.
  void Connect(QuicTestPeer* other_peer) {
    ice_channel_->SetDestination(other_peer->ice_channel_);
  }

  // Disconnects |ice_channel_|.
  void Disconnect() { ice_channel_->SetDestination(nullptr); }

  // Generates ICE credentials and passes them to |quic_channel_|.
  void SetIceParameters(IceRole local_ice_role,
                        ConnectionRole local_connection_role,
                        ConnectionRole remote_connection_role,
                        rtc::SSLFingerprint* remote_fingerprint) {
    quic_channel_.SetIceRole(local_ice_role);
    quic_channel_.SetIceTiebreaker(
        (local_ice_role == cricket::ICEROLE_CONTROLLING) ? 1 : 2);

    TransportDescription local_desc(
        std::vector<std::string>(), kIceUfrag, kIcePwd, cricket::ICEMODE_FULL,
        local_connection_role, local_fingerprint_.get());
    TransportDescription remote_desc(
        std::vector<std::string>(), kIceUfrag, kIcePwd, cricket::ICEMODE_FULL,
        remote_connection_role, remote_fingerprint);

    quic_channel_.SetIceParameters(local_desc.GetIceParameters());
    quic_channel_.SetRemoteIceParameters(remote_desc.GetIceParameters());
  }

  // Creates fingerprint from certificate.
  rtc::SSLFingerprint* CreateFingerprint(rtc::RTCCertificate* cert) {
    std::string digest_algorithm;
    bool get_digest_algorithm =
        cert->ssl_certificate().GetSignatureDigestAlgorithm(&digest_algorithm);
    if (!get_digest_algorithm || digest_algorithm.empty()) {
      return nullptr;
    }
    std::unique_ptr<rtc::SSLFingerprint> fingerprint(
        rtc::SSLFingerprint::Create(digest_algorithm, cert->identity()));
    if (digest_algorithm != rtc::DIGEST_SHA_256) {
      return nullptr;
    }
    return fingerprint.release();
  }

  // Sends SRTP packet to the other peer via |quic_channel_|.
  int SendSrtpPacket() {
    char packet[kPacketSize];
    packet[0] = 0x80;  // Make the packet header look like RTP.
    int rv = quic_channel_.SendPacket(
        &packet[0], kPacketSize, rtc::PacketOptions(), cricket::PF_SRTP_BYPASS);
    bytes_sent_ += rv;
    return rv;
  }

  // Sends a non-SRTP packet with the PF_SRTP_BYPASS flag via |quic_channel_|.
  int SendInvalidSrtpPacket() {
    char packet[kPacketSize];
    // Fill the packet with 0 to form an invalid SRTP packet.
    memset(packet, 0, kPacketSize);
    return quic_channel_.SendPacket(
        &packet[0], kPacketSize, rtc::PacketOptions(), cricket::PF_SRTP_BYPASS);
  }

  // Sends an RTP packet to the other peer via |quic_channel_|, without the SRTP
  // bypass flag.
  int SendRtpPacket() {
    char packet[kPacketSize];
    packet[0] = 0x80;  // Make the packet header look like RTP.
    return quic_channel_.SendPacket(&packet[0], kPacketSize,
                                    rtc::PacketOptions(), 0);
  }

  void ClearBytesSent() { bytes_sent_ = 0; }

  void ClearBytesReceived() { bytes_received_ = 0; }

  void SetWriteError(int error) { ice_channel_->SetError(error); }

  size_t bytes_received() const { return bytes_received_; }

  size_t bytes_sent() const { return bytes_sent_; }

  FailableTransportChannel* ice_channel() { return ice_channel_; }

  QuicTransportChannel* quic_channel() { return &quic_channel_; }

  std::unique_ptr<rtc::SSLFingerprint>& local_fingerprint() {
    return local_fingerprint_;
  }

  ReliableQuicStream* incoming_quic_stream() { return incoming_quic_stream_; }

  size_t incoming_stream_count() const { return incoming_stream_count_; }

  bool signal_closed_emitted() const { return signal_closed_emitted_; }

 private:
  // QuicTransportChannel callbacks.
  void OnTransportChannelReadPacket(TransportChannel* channel,
                                    const char* data,
                                    size_t size,
                                    const rtc::PacketTime& packet_time,
                                    int flags) {
    bytes_received_ += size;
    // Only SRTP packets should have the bypass flag set.
    int expected_flags = IsRtpLeadByte(data[0]) ? cricket::PF_SRTP_BYPASS : 0;
    ASSERT_EQ(expected_flags, flags);
  }
  void OnIncomingStream(ReliableQuicStream* stream) {
    incoming_quic_stream_ = stream;
    ++incoming_stream_count_;
  }
  void OnClosed() { signal_closed_emitted_ = true; }

  std::string name_;                      // Channel name.
  size_t bytes_sent_;                     // Bytes sent by QUIC channel.
  size_t bytes_received_;                 // Bytes received by QUIC channel.
  FailableTransportChannel* ice_channel_;  // Simulates an ICE channel.
  QuicTransportChannel quic_channel_;     // QUIC channel to test.
  std::unique_ptr<rtc::SSLFingerprint> local_fingerprint_;
  ReliableQuicStream* incoming_quic_stream_ = nullptr;
  size_t incoming_stream_count_;
  bool signal_closed_emitted_ = false;
};

class QuicTransportChannelTest : public testing::Test {
 public:
  QuicTransportChannelTest() : peer1_("P1"), peer2_("P2") {}

  // Performs negotiation before QUIC handshake, then connects the fake
  // transport channels of each peer. As a side effect, the QUIC channels
  // start sending handshake messages. |peer1_| has a client role and |peer2_|
  // has server role in the QUIC handshake.
  void Connect() {
    SetIceAndCryptoParameters(rtc::SSL_CLIENT, rtc::SSL_SERVER);
    peer1_.Connect(&peer2_);
  }

  // Disconnects the fake transport channels.
  void Disconnect() {
    peer1_.Disconnect();
    peer2_.Disconnect();
  }

  // Sets up ICE parameters and exchanges fingerprints before QUIC handshake.
  void SetIceAndCryptoParameters(rtc::SSLRole peer1_ssl_role,
                                 rtc::SSLRole peer2_ssl_role) {
    peer1_.quic_channel()->SetSslRole(peer1_ssl_role);
    peer2_.quic_channel()->SetSslRole(peer2_ssl_role);

    std::unique_ptr<rtc::SSLFingerprint>& peer1_fingerprint =
        peer1_.local_fingerprint();
    std::unique_ptr<rtc::SSLFingerprint>& peer2_fingerprint =
        peer2_.local_fingerprint();

    peer1_.quic_channel()->SetRemoteFingerprint(
        peer2_fingerprint->algorithm,
        reinterpret_cast<const uint8_t*>(peer2_fingerprint->digest.data()),
        peer2_fingerprint->digest.size());
    peer2_.quic_channel()->SetRemoteFingerprint(
        peer1_fingerprint->algorithm,
        reinterpret_cast<const uint8_t*>(peer1_fingerprint->digest.data()),
        peer1_fingerprint->digest.size());

    ConnectionRole peer1_connection_role =
        SslRoleToConnectionRole(peer1_ssl_role);
    ConnectionRole peer2_connection_role =
        SslRoleToConnectionRole(peer2_ssl_role);

    peer1_.SetIceParameters(cricket::ICEROLE_CONTROLLED, peer1_connection_role,
                            peer2_connection_role, peer2_fingerprint.get());
    peer2_.SetIceParameters(cricket::ICEROLE_CONTROLLING, peer2_connection_role,
                            peer1_connection_role, peer1_fingerprint.get());
  }

  // Checks if QUIC handshake is done.
  bool quic_connected() {
    return peer1_.quic_channel()->quic_state() ==
               cricket::QUIC_TRANSPORT_CONNECTED &&
           peer2_.quic_channel()->quic_state() ==
               cricket::QUIC_TRANSPORT_CONNECTED;
  }

  // Checks if QUIC channels are writable.
  bool quic_writable() {
    return peer1_.quic_channel()->writable() &&
           peer2_.quic_channel()->writable();
  }

 protected:
  // QUIC peer with a client role, who initiates the QUIC handshake.
  QuicTestPeer peer1_;
  // QUIC peer with a server role, who responds to the client peer.
  QuicTestPeer peer2_;
};

// Test that the QUIC channel passes ICE parameters to the underlying ICE
// channel.
TEST_F(QuicTransportChannelTest, ChannelSetupIce) {
  SetIceAndCryptoParameters(rtc::SSL_CLIENT, rtc::SSL_SERVER);
  FailableTransportChannel* channel1 = peer1_.ice_channel();
  FailableTransportChannel* channel2 = peer2_.ice_channel();
  EXPECT_EQ(cricket::ICEROLE_CONTROLLED, channel1->GetIceRole());
  EXPECT_EQ(2u, channel1->IceTiebreaker());
  EXPECT_EQ(kIceUfrag, channel1->ice_ufrag());
  EXPECT_EQ(kIcePwd, channel1->ice_pwd());
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, channel2->GetIceRole());
  EXPECT_EQ(1u, channel2->IceTiebreaker());
}

// Test that export keying material generates identical keys for both peers
// after the QUIC handshake.
TEST_F(QuicTransportChannelTest, ExportKeyingMaterial) {
  Connect();
  ASSERT_TRUE_WAIT(quic_connected(), kTimeoutMs);
  uint8_t key1[kOutputKeyLength];
  uint8_t key2[kOutputKeyLength];

  bool from_success = peer1_.quic_channel()->ExportKeyingMaterial(
      kExporterLabel, kExporterContext, kExporterContextLength, true, key1,
      kOutputKeyLength);
  ASSERT_TRUE(from_success);
  bool to_success = peer2_.quic_channel()->ExportKeyingMaterial(
      kExporterLabel, kExporterContext, kExporterContextLength, true, key2,
      kOutputKeyLength);
  ASSERT_TRUE(to_success);

  EXPECT_EQ(0, memcmp(key1, key2, sizeof(key1)));
}

// Test that the QUIC channel is not writable before the QUIC handshake.
TEST_F(QuicTransportChannelTest, NotWritableBeforeHandshake) {
  Connect();
  EXPECT_FALSE(quic_writable());
  Disconnect();
  EXPECT_FALSE(quic_writable());
  Connect();
  EXPECT_FALSE(quic_writable());
}

// Test that once handshake begins, QUIC is not writable until its completion.
TEST_F(QuicTransportChannelTest, QuicHandshake) {
  Connect();
  EXPECT_FALSE(quic_writable());
  ASSERT_TRUE_WAIT(quic_connected(), kTimeoutMs);
  EXPECT_TRUE(quic_writable());
}

// Test that Non-SRTP data is not sent using SendPacket(), regardless of QUIC
// channel state.
TEST_F(QuicTransportChannelTest, TransferNonSrtp) {
  // Send data before ICE channel is connected.
  peer1_.ClearBytesSent();
  peer2_.ClearBytesReceived();
  ASSERT_EQ(-1, peer1_.SendRtpPacket());
  EXPECT_EQ(0u, peer1_.bytes_sent());
  // Send data after ICE channel is connected, before QUIC handshake.
  Connect();
  peer1_.ClearBytesSent();
  peer2_.ClearBytesReceived();
  ASSERT_EQ(-1, peer1_.SendRtpPacket());
  EXPECT_EQ(0u, peer1_.bytes_sent());
  // Send data after QUIC handshake.
  ASSERT_TRUE_WAIT(quic_connected(), kTimeoutMs);
  peer1_.ClearBytesSent();
  peer2_.ClearBytesReceived();
  ASSERT_EQ(-1, peer1_.SendRtpPacket());
  EXPECT_EQ(0u, peer1_.bytes_sent());
}

// Test that SRTP data is always be sent, regardless of QUIC channel state, when
// the ICE channel is connected.
TEST_F(QuicTransportChannelTest, TransferSrtp) {
  // Send data after ICE channel is connected, before QUIC handshake.
  Connect();
  peer1_.ClearBytesSent();
  peer2_.ClearBytesReceived();
  ASSERT_EQ(kPacketSize, static_cast<size_t>(peer1_.SendSrtpPacket()));
  EXPECT_EQ_WAIT(kPacketSize, peer2_.bytes_received(), kTimeoutMs);
  EXPECT_EQ(kPacketSize, peer1_.bytes_sent());
  ASSERT_TRUE_WAIT(quic_connected(), kTimeoutMs);
  // Send data after QUIC handshake.
  peer1_.ClearBytesSent();
  peer2_.ClearBytesReceived();
  ASSERT_EQ(kPacketSize, static_cast<size_t>(peer1_.SendSrtpPacket()));
  EXPECT_EQ_WAIT(kPacketSize, peer2_.bytes_received(), kTimeoutMs);
  EXPECT_EQ(kPacketSize, peer1_.bytes_sent());
}

// Test that invalid SRTP (non-SRTP data with
// PF_SRTP_BYPASS flag) fails to send with return value -1.
TEST_F(QuicTransportChannelTest, TransferInvalidSrtp) {
  peer1_.ClearBytesSent();
  peer2_.ClearBytesReceived();
  EXPECT_EQ(-1, peer1_.SendInvalidSrtpPacket());
  EXPECT_EQ(0u, peer2_.bytes_received());
  Connect();
  peer1_.ClearBytesSent();
  peer2_.ClearBytesReceived();
  EXPECT_EQ(-1, peer1_.SendInvalidSrtpPacket());
  EXPECT_EQ(0u, peer2_.bytes_received());
}

// Test that QuicTransportChannel::WritePacket blocks when the ICE
// channel is not writable, and otherwise succeeds.
TEST_F(QuicTransportChannelTest, QuicWritePacket) {
  peer1_.ice_channel()->SetDestination(peer2_.ice_channel());
  std::string packet = "FAKEQUICPACKET";

  // QUIC should be write blocked when the ICE channel is not writable.
  peer1_.ice_channel()->SetWritable(false);
  EXPECT_TRUE(peer1_.quic_channel()->IsWriteBlocked());
  net::WriteResult write_blocked_result = peer1_.quic_channel()->WritePacket(
      packet.data(), packet.size(), kIpAddress, kIpEndpoint, nullptr);
  EXPECT_EQ(net::WRITE_STATUS_BLOCKED, write_blocked_result.status);
  EXPECT_EQ(EWOULDBLOCK, write_blocked_result.error_code);

  // QUIC should ignore errors when the ICE channel is writable.
  peer1_.ice_channel()->SetWritable(true);
  EXPECT_FALSE(peer1_.quic_channel()->IsWriteBlocked());
  peer1_.SetWriteError(EWOULDBLOCK);
  net::WriteResult ignore_error_result = peer1_.quic_channel()->WritePacket(
      packet.data(), packet.size(), kIpAddress, kIpEndpoint, nullptr);
  EXPECT_EQ(net::WRITE_STATUS_OK, ignore_error_result.status);
  EXPECT_EQ(0, ignore_error_result.bytes_written);

  peer1_.SetWriteError(kNoWriteError);
  net::WriteResult no_error_result = peer1_.quic_channel()->WritePacket(
      packet.data(), packet.size(), kIpAddress, kIpEndpoint, nullptr);
  EXPECT_EQ(net::WRITE_STATUS_OK, no_error_result.status);
  EXPECT_EQ(static_cast<int>(packet.size()), no_error_result.bytes_written);
}

// Test that SSL roles can be reversed before QUIC handshake.
TEST_F(QuicTransportChannelTest, QuicRoleReversalBeforeQuic) {
  EXPECT_TRUE(peer1_.quic_channel()->SetSslRole(rtc::SSL_SERVER));
  EXPECT_TRUE(peer1_.quic_channel()->SetSslRole(rtc::SSL_CLIENT));
  EXPECT_TRUE(peer1_.quic_channel()->SetSslRole(rtc::SSL_SERVER));
}

// Test that SSL roles cannot be reversed after the QUIC handshake. SetSslRole
// returns true if the current SSL role equals the proposed SSL role.
TEST_F(QuicTransportChannelTest, QuicRoleReversalAfterQuic) {
  Connect();
  ASSERT_TRUE_WAIT(quic_connected(), kTimeoutMs);
  EXPECT_FALSE(peer1_.quic_channel()->SetSslRole(rtc::SSL_SERVER));
  EXPECT_TRUE(peer1_.quic_channel()->SetSslRole(rtc::SSL_CLIENT));
  EXPECT_FALSE(peer2_.quic_channel()->SetSslRole(rtc::SSL_CLIENT));
  EXPECT_TRUE(peer2_.quic_channel()->SetSslRole(rtc::SSL_SERVER));
}

// Set the SSL role, then test that GetSslRole returns the same value.
TEST_F(QuicTransportChannelTest, SetGetSslRole) {
  ASSERT_TRUE(peer1_.quic_channel()->SetSslRole(rtc::SSL_SERVER));
  std::unique_ptr<rtc::SSLRole> role(new rtc::SSLRole());
  ASSERT_TRUE(peer1_.quic_channel()->GetSslRole(role.get()));
  EXPECT_EQ(rtc::SSL_SERVER, *role);
}

// Test that after the QUIC handshake is complete, the QUIC handshake remains
// confirmed even if the ICE channel reconnects.
TEST_F(QuicTransportChannelTest, HandshakeConfirmedAfterReconnect) {
  Connect();
  ASSERT_TRUE_WAIT(quic_connected(), kTimeoutMs);
  Disconnect();
  EXPECT_TRUE(quic_connected());
  Connect();
  EXPECT_TRUE(quic_connected());
}

// Test that if the ICE channel becomes receiving after the QUIC channel is
// connected, then the QUIC channel becomes receiving.
TEST_F(QuicTransportChannelTest, IceReceivingAfterConnected) {
  Connect();
  ASSERT_TRUE_WAIT(quic_connected(), kTimeoutMs);
  ASSERT_FALSE(peer1_.ice_channel()->receiving());
  EXPECT_FALSE(peer1_.quic_channel()->receiving());
  peer1_.ice_channel()->SetReceiving(true);
  EXPECT_TRUE(peer1_.quic_channel()->receiving());
}

// Test that if the ICE channel becomes receiving before the QUIC channel is
// connected, then the QUIC channel becomes receiving.
TEST_F(QuicTransportChannelTest, IceReceivingBeforeConnected) {
  Connect();
  peer1_.ice_channel()->SetReceiving(true);
  ASSERT_TRUE(peer1_.ice_channel()->receiving());
  ASSERT_TRUE_WAIT(quic_connected(), kTimeoutMs);
  EXPECT_TRUE(peer1_.quic_channel()->receiving());
}

// Test that when peer 1 creates an outgoing stream, peer 2 creates an incoming
// QUIC stream with the same ID and fires OnIncomingStream.
TEST_F(QuicTransportChannelTest, CreateOutgoingAndIncomingQuicStream) {
  Connect();
  EXPECT_EQ(nullptr, peer1_.quic_channel()->CreateQuicStream());
  ASSERT_TRUE_WAIT(quic_connected(), kTimeoutMs);
  ReliableQuicStream* stream = peer1_.quic_channel()->CreateQuicStream();
  ASSERT_NE(nullptr, stream);
  stream->Write("Hi", 2);
  EXPECT_TRUE_WAIT(peer2_.incoming_quic_stream() != nullptr, kTimeoutMs);
  EXPECT_EQ(stream->id(), peer2_.incoming_quic_stream()->id());
}

// Test that if the QuicTransportChannel is unwritable, then all outgoing QUIC
// streams can send data once the QuicTransprotChannel becomes writable again.
TEST_F(QuicTransportChannelTest, OutgoingQuicStreamSendsDataAfterReconnect) {
  Connect();
  ASSERT_TRUE_WAIT(quic_connected(), kTimeoutMs);
  ReliableQuicStream* stream1 = peer1_.quic_channel()->CreateQuicStream();
  ASSERT_NE(nullptr, stream1);
  ReliableQuicStream* stream2 = peer1_.quic_channel()->CreateQuicStream();
  ASSERT_NE(nullptr, stream2);

  peer1_.ice_channel()->SetWritable(false);
  stream1->Write("First", 5);
  EXPECT_EQ(5u, stream1->queued_data_bytes());
  stream2->Write("Second", 6);
  EXPECT_EQ(6u, stream2->queued_data_bytes());
  EXPECT_EQ(0u, peer2_.incoming_stream_count());

  peer1_.ice_channel()->SetWritable(true);
  EXPECT_EQ_WAIT(0u, stream1->queued_data_bytes(), kTimeoutMs);
  EXPECT_EQ_WAIT(0u, stream2->queued_data_bytes(), kTimeoutMs);
  EXPECT_EQ_WAIT(2u, peer2_.incoming_stream_count(), kTimeoutMs);
}

// Test that SignalClosed is emitted when the QuicConnection closes.
TEST_F(QuicTransportChannelTest, SignalClosedEmitted) {
  Connect();
  ASSERT_TRUE_WAIT(quic_connected(), kTimeoutMs);
  ASSERT_FALSE(peer1_.signal_closed_emitted());
  ReliableQuicStream* stream = peer1_.quic_channel()->CreateQuicStream();
  ASSERT_NE(nullptr, stream);
  stream->CloseConnectionWithDetails(net::QuicErrorCode::QUIC_NO_ERROR,
                                     "Closing QUIC for testing");
  EXPECT_TRUE(peer1_.signal_closed_emitted());
  EXPECT_TRUE_WAIT(peer2_.signal_closed_emitted(), kTimeoutMs);
}
