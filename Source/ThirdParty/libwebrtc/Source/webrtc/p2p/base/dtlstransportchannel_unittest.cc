/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <set>

#include "webrtc/p2p/base/dtlstransportchannel.h"
#include "webrtc/p2p/base/fakeicetransport.h"
#include "webrtc/p2p/base/packettransportinternal.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/dscp.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/sslidentity.h"
#include "webrtc/base/sslstreamadapter.h"
#include "webrtc/base/stringutils.h"

#define MAYBE_SKIP_TEST(feature)                              \
  if (!(rtc::SSLStreamAdapter::feature())) {                  \
    LOG(LS_INFO) << #feature " feature disabled... skipping"; \
    return;                                                   \
  }

static const char kIceUfrag1[] = "TESTICEUFRAG0001";
static const char kIcePwd1[] = "TESTICEPWD00000000000001";
static const size_t kPacketNumOffset = 8;
static const size_t kPacketHeaderLen = 12;
static const int kFakePacketId = 0x1234;
static const int kTimeout = 10000;

static bool IsRtpLeadByte(uint8_t b) {
  return ((b & 0xC0) == 0x80);
}

cricket::TransportDescription MakeTransportDescription(
    const rtc::scoped_refptr<rtc::RTCCertificate>& cert,
    cricket::ConnectionRole role) {
  std::unique_ptr<rtc::SSLFingerprint> fingerprint;
  if (cert) {
    std::string digest_algorithm;
    EXPECT_TRUE(
        cert->ssl_certificate().GetSignatureDigestAlgorithm(&digest_algorithm));
    EXPECT_FALSE(digest_algorithm.empty());
    fingerprint.reset(
        rtc::SSLFingerprint::Create(digest_algorithm, cert->identity()));
    EXPECT_TRUE(fingerprint.get() != NULL);
    EXPECT_EQ(rtc::DIGEST_SHA_256, digest_algorithm);
  }
  return cricket::TransportDescription(std::vector<std::string>(), kIceUfrag1,
                                       kIcePwd1, cricket::ICEMODE_FULL, role,
                                       fingerprint.get());
}

using cricket::ConnectionRole;

enum Flags { NF_REOFFER = 0x1, NF_EXPECT_FAILURE = 0x2 };

// TODO(deadbeef): Remove the dependency on JsepTransport. This test should be
// testing DtlsTransportChannel by itself, calling methods to set the
// configuration directly instead of negotiating TransportDescriptions.
class DtlsTestClient : public sigslot::has_slots<> {
 public:
  DtlsTestClient(const std::string& name) : name_(name) {}
  void CreateCertificate(rtc::KeyType key_type) {
    certificate_ =
        rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
            rtc::SSLIdentity::Generate(name_, key_type)));
  }
  const rtc::scoped_refptr<rtc::RTCCertificate>& certificate() {
    return certificate_;
  }
  void SetupSrtp() {
    EXPECT_TRUE(certificate_ != nullptr);
    use_dtls_srtp_ = true;
  }
  void SetupMaxProtocolVersion(rtc::SSLProtocolVersion version) {
    ssl_max_version_ = version;
  }
  void SetupChannels(int count, cricket::IceRole role, int async_delay_ms = 0) {
    transport_.reset(
        new cricket::JsepTransport("dtls content name", certificate_));
    for (int i = 0; i < count; ++i) {
      cricket::FakeIceTransport* fake_ice_channel =
          new cricket::FakeIceTransport(transport_->mid(), i);
      fake_ice_channel->SetAsync(true);
      fake_ice_channel->SetAsyncDelay(async_delay_ms);
      // Hook the raw packets so that we can verify they are encrypted.
      fake_ice_channel->SignalReadPacket.connect(
          this, &DtlsTestClient::OnFakeTransportChannelReadPacket);

      cricket::DtlsTransport* dtls =
          new cricket::DtlsTransport(fake_ice_channel);
      dtls->SetLocalCertificate(certificate_);
      dtls->ice_transport()->SetIceRole(role);
      dtls->ice_transport()->SetIceTiebreaker(
          (role == cricket::ICEROLE_CONTROLLING) ? 1 : 2);
      dtls->SetSslMaxProtocolVersion(ssl_max_version_);
      dtls->SignalWritableState.connect(
          this, &DtlsTestClient::OnTransportChannelWritableState);
      dtls->SignalReadPacket.connect(
          this, &DtlsTestClient::OnTransportChannelReadPacket);
      dtls->SignalSentPacket.connect(
          this, &DtlsTestClient::OnTransportChannelSentPacket);
      fake_dtls_transports_.push_back(
          std::unique_ptr<cricket::DtlsTransport>(dtls));
      fake_ice_transports_.push_back(
          std::unique_ptr<cricket::FakeIceTransport>(fake_ice_channel));
      transport_->AddChannel(dtls, i);
    }
  }

  cricket::JsepTransport* transport() { return transport_.get(); }

  cricket::FakeIceTransport* GetFakeIceTransort(int component) {
    for (const auto& ch : fake_ice_transports_) {
      if (ch->component() == component) {
        return ch.get();
      }
    }
    return nullptr;
  }

  cricket::DtlsTransport* GetDtlsTransport(int component) {
    for (const auto& dtls : fake_dtls_transports_) {
      if (dtls->component() == component) {
        return dtls.get();
      }
    }
    return nullptr;
  }

  // Offer DTLS if we have an identity; pass in a remote fingerprint only if
  // both sides support DTLS.
  void Negotiate(DtlsTestClient* peer, cricket::ContentAction action,
                 ConnectionRole local_role, ConnectionRole remote_role,
                 int flags) {
    Negotiate(certificate_, certificate_ ? peer->certificate_ : nullptr, action,
              local_role, remote_role, flags);
  }

  void MaybeSetSrtpCryptoSuites() {
    if (!use_dtls_srtp_) {
      return;
    }
    std::vector<int> ciphers;
    ciphers.push_back(rtc::SRTP_AES128_CM_SHA1_80);
    // SRTP ciphers will be set only in the beginning.
    for (const auto& dtls : fake_dtls_transports_) {
      EXPECT_TRUE(dtls->SetSrtpCryptoSuites(ciphers));
    }
  }

  void SetLocalTransportDescription(
      const rtc::scoped_refptr<rtc::RTCCertificate>& cert,
      cricket::ContentAction action,
      ConnectionRole role,
      int flags) {
    // If |NF_EXPECT_FAILURE| is set, expect SRTD or SLTD to fail when
    // content action is CA_ANSWER.
    bool expect_success =
        !((action == cricket::CA_ANSWER) && (flags & NF_EXPECT_FAILURE));
    EXPECT_EQ(expect_success,
              transport_->SetLocalTransportDescription(
                  MakeTransportDescription(cert, role), action, nullptr));
  }

  void SetRemoteTransportDescription(
      const rtc::scoped_refptr<rtc::RTCCertificate>& cert,
      cricket::ContentAction action,
      ConnectionRole role,
      int flags) {
    // If |NF_EXPECT_FAILURE| is set, expect SRTD or SLTD to fail when
    // content action is CA_ANSWER.
    bool expect_success =
        !((action == cricket::CA_ANSWER) && (flags & NF_EXPECT_FAILURE));
    EXPECT_EQ(expect_success,
              transport_->SetRemoteTransportDescription(
                  MakeTransportDescription(cert, role), action, nullptr));
  }

  // Allow any DTLS configuration to be specified (including invalid ones).
  void Negotiate(const rtc::scoped_refptr<rtc::RTCCertificate>& local_cert,
                 const rtc::scoped_refptr<rtc::RTCCertificate>& remote_cert,
                 cricket::ContentAction action,
                 ConnectionRole local_role,
                 ConnectionRole remote_role,
                 int flags) {
    if (!(flags & NF_REOFFER)) {
      // SRTP ciphers will be set only in the beginning.
      MaybeSetSrtpCryptoSuites();
    }
    if (action == cricket::CA_OFFER) {
      SetLocalTransportDescription(local_cert, cricket::CA_OFFER, local_role,
                                   flags);
      SetRemoteTransportDescription(remote_cert, cricket::CA_ANSWER,
                                    remote_role, flags);
    } else {
      SetRemoteTransportDescription(remote_cert, cricket::CA_OFFER, remote_role,
                                    flags);
      // If remote if the offerer and has no DTLS support, answer will be
      // without any fingerprint.
      SetLocalTransportDescription(remote_cert ? local_cert : nullptr,
                                   cricket::CA_ANSWER, local_role, flags);
    }
  }

  bool Connect(DtlsTestClient* peer, bool asymmetric) {
    for (auto& ice : fake_ice_transports_) {
      ice->SetDestination(peer->GetFakeIceTransort(ice->component()),
                          asymmetric);
    }
    return true;
  }

  bool all_dtls_transports_writable() const {
    if (fake_dtls_transports_.empty()) {
      return false;
    }
    for (const auto& dtls : fake_dtls_transports_) {
      if (!dtls->writable()) {
        return false;
      }
    }
    return true;
  }

  bool all_ice_transports_writable() const {
    if (fake_dtls_transports_.empty()) {
      return false;
    }
    for (const auto& dtls : fake_dtls_transports_) {
      if (!dtls->ice_transport()->writable()) {
        return false;
      }
    }
    return true;
  }

  int received_dtls_client_hellos() const {
    return received_dtls_client_hellos_;
  }

  int received_dtls_server_hellos() const {
    return received_dtls_server_hellos_;
  }

  bool negotiated_dtls() const {
    return transport_->local_description() &&
           transport_->local_description()->identity_fingerprint &&
           transport_->remote_description() &&
           transport_->remote_description()->identity_fingerprint;
  }

  void CheckRole(rtc::SSLRole role) {
    if (role == rtc::SSL_CLIENT) {
      ASSERT_EQ(0, received_dtls_client_hellos_);
      ASSERT_GT(received_dtls_server_hellos_, 0);
    } else {
      ASSERT_GT(received_dtls_client_hellos_, 0);
      ASSERT_EQ(0, received_dtls_server_hellos_);
    }
  }

  void CheckSrtp(int expected_crypto_suite) {
    for (const auto& dtls : fake_dtls_transports_) {
      int crypto_suite;

      bool rv = dtls->GetSrtpCryptoSuite(&crypto_suite);
      if (negotiated_dtls() && expected_crypto_suite) {
        ASSERT_TRUE(rv);

        ASSERT_EQ(crypto_suite, expected_crypto_suite);
      } else {
        ASSERT_FALSE(rv);
      }
    }
  }

  void CheckSsl() {
    for (const auto& dtls : fake_dtls_transports_) {
      int cipher;

      bool rv = dtls->GetSslCipherSuite(&cipher);
      if (negotiated_dtls()) {
        ASSERT_TRUE(rv);

        EXPECT_TRUE(
            rtc::SSLStreamAdapter::IsAcceptableCipher(cipher, rtc::KT_DEFAULT));
      } else {
        ASSERT_FALSE(rv);
      }
    }
  }

  void SendPackets(size_t transport, size_t size, size_t count, bool srtp) {
    RTC_CHECK(transport < fake_dtls_transports_.size());
    std::unique_ptr<char[]> packet(new char[size]);
    size_t sent = 0;
    do {
      // Fill the packet with a known value and a sequence number to check
      // against, and make sure that it doesn't look like DTLS.
      memset(packet.get(), sent & 0xff, size);
      packet[0] = (srtp) ? 0x80 : 0x00;
      rtc::SetBE32(packet.get() + kPacketNumOffset,
                   static_cast<uint32_t>(sent));

      // Only set the bypass flag if we've activated DTLS.
      int flags = (certificate_ && srtp) ? cricket::PF_SRTP_BYPASS : 0;
      rtc::PacketOptions packet_options;
      packet_options.packet_id = kFakePacketId;
      int rv = fake_dtls_transports_[transport]->SendPacket(
          packet.get(), size, packet_options, flags);
      ASSERT_GT(rv, 0);
      ASSERT_EQ(size, static_cast<size_t>(rv));
      ++sent;
    } while (sent < count);
  }

  int SendInvalidSrtpPacket(size_t transport, size_t size) {
    RTC_CHECK(transport < fake_dtls_transports_.size());
    std::unique_ptr<char[]> packet(new char[size]);
    // Fill the packet with 0 to form an invalid SRTP packet.
    memset(packet.get(), 0, size);

    rtc::PacketOptions packet_options;
    return fake_dtls_transports_[transport]->SendPacket(
        packet.get(), size, packet_options, cricket::PF_SRTP_BYPASS);
  }

  void ExpectPackets(size_t transport, size_t size) {
    packet_size_ = size;
    received_.clear();
  }

  size_t NumPacketsReceived() {
    return received_.size();
  }

  bool VerifyPacket(const char* data, size_t size, uint32_t* out_num) {
    if (size != packet_size_ ||
        (data[0] != 0 && static_cast<uint8_t>(data[0]) != 0x80)) {
      return false;
    }
    uint32_t packet_num = rtc::GetBE32(data + kPacketNumOffset);
    for (size_t i = kPacketHeaderLen; i < size; ++i) {
      if (static_cast<uint8_t>(data[i]) != (packet_num & 0xff)) {
        return false;
      }
    }
    if (out_num) {
      *out_num = packet_num;
    }
    return true;
  }
  bool VerifyEncryptedPacket(const char* data, size_t size) {
    // This is an encrypted data packet; let's make sure it's mostly random;
    // less than 10% of the bytes should be equal to the cleartext packet.
    if (size <= packet_size_) {
      return false;
    }
    uint32_t packet_num = rtc::GetBE32(data + kPacketNumOffset);
    int num_matches = 0;
    for (size_t i = kPacketNumOffset; i < size; ++i) {
      if (static_cast<uint8_t>(data[i]) == (packet_num & 0xff)) {
        ++num_matches;
      }
    }
    return (num_matches < ((static_cast<int>(size) - 5) / 10));
  }

  // Transport channel callbacks
  void OnTransportChannelWritableState(
      rtc::PacketTransportInternal* transport) {
    LOG(LS_INFO) << name_ << ": Channel '" << transport->debug_name()
                 << "' is writable";
  }

  void OnTransportChannelReadPacket(rtc::PacketTransportInternal* transport,
                                    const char* data,
                                    size_t size,
                                    const rtc::PacketTime& packet_time,
                                    int flags) {
    uint32_t packet_num = 0;
    ASSERT_TRUE(VerifyPacket(data, size, &packet_num));
    received_.insert(packet_num);
    // Only DTLS-SRTP packets should have the bypass flag set.
    int expected_flags =
        (certificate_ && IsRtpLeadByte(data[0])) ? cricket::PF_SRTP_BYPASS : 0;
    ASSERT_EQ(expected_flags, flags);
  }

  void OnTransportChannelSentPacket(rtc::PacketTransportInternal* transport,
                                    const rtc::SentPacket& sent_packet) {
    sent_packet_ = sent_packet;
  }

  rtc::SentPacket sent_packet() const { return sent_packet_; }

  // Hook into the raw packet stream to make sure DTLS packets are encrypted.
  void OnFakeTransportChannelReadPacket(rtc::PacketTransportInternal* transport,
                                        const char* data,
                                        size_t size,
                                        const rtc::PacketTime& time,
                                        int flags) {
    // Flags shouldn't be set on the underlying TransportChannel packets.
    ASSERT_EQ(0, flags);

    // Look at the handshake packets to see what role we played.
    // Check that non-handshake packets are DTLS data or SRTP bypass.
    if (data[0] == 22 && size > 17) {
      if (data[13] == 1) {
        ++received_dtls_client_hellos_;
      } else if (data[13] == 2) {
        ++received_dtls_server_hellos_;
      }
    } else if (negotiated_dtls() && !(data[0] >= 20 && data[0] <= 22)) {
      ASSERT_TRUE(data[0] == 23 || IsRtpLeadByte(data[0]));
      if (data[0] == 23) {
        ASSERT_TRUE(VerifyEncryptedPacket(data, size));
      } else if (IsRtpLeadByte(data[0])) {
        ASSERT_TRUE(VerifyPacket(data, size, NULL));
      }
    }
  }

 private:
  std::string name_;
  rtc::scoped_refptr<rtc::RTCCertificate> certificate_;
  std::vector<std::unique_ptr<cricket::FakeIceTransport>> fake_ice_transports_;
  std::vector<std::unique_ptr<cricket::DtlsTransport>> fake_dtls_transports_;
  std::unique_ptr<cricket::JsepTransport> transport_;
  size_t packet_size_ = 0u;
  std::set<int> received_;
  bool use_dtls_srtp_ = false;
  rtc::SSLProtocolVersion ssl_max_version_ = rtc::SSL_PROTOCOL_DTLS_12;
  int received_dtls_client_hellos_ = 0;
  int received_dtls_server_hellos_ = 0;
  rtc::SentPacket sent_packet_;
};

// Base class for DtlsTransportChannelTest and DtlsEventOrderingTest, which
// inherit from different variants of testing::Test.
//
// Note that this test always uses a FakeClock, due to the |fake_clock_| member
// variable.
class DtlsTransportChannelTestBase {
 public:
  DtlsTransportChannelTestBase()
      : client1_("P1"),
        client2_("P2"),
        channel_ct_(1),
        use_dtls_(false),
        use_dtls_srtp_(false),
        ssl_expected_version_(rtc::SSL_PROTOCOL_DTLS_12) {}

  void SetChannelCount(size_t channel_ct) {
    channel_ct_ = static_cast<int>(channel_ct);
  }
  void SetMaxProtocolVersions(rtc::SSLProtocolVersion c1,
                              rtc::SSLProtocolVersion c2) {
    client1_.SetupMaxProtocolVersion(c1);
    client2_.SetupMaxProtocolVersion(c2);
    ssl_expected_version_ = std::min(c1, c2);
  }
  void PrepareDtls(bool c1, bool c2, rtc::KeyType key_type) {
    if (c1) {
      client1_.CreateCertificate(key_type);
    }
    if (c2) {
      client2_.CreateCertificate(key_type);
    }
    if (c1 && c2)
      use_dtls_ = true;
  }
  void PrepareDtlsSrtp(bool c1, bool c2) {
    if (!use_dtls_)
      return;

    if (c1)
      client1_.SetupSrtp();
    if (c2)
      client2_.SetupSrtp();

    if (c1 && c2)
      use_dtls_srtp_ = true;
  }

  // Negotiate local/remote fingerprint before or after the underlying
  // tranpsort is connected?
  enum NegotiateOrdering { NEGOTIATE_BEFORE_CONNECT, CONNECT_BEFORE_NEGOTIATE };
  bool Connect(ConnectionRole client1_role,
               ConnectionRole client2_role,
               NegotiateOrdering ordering = NEGOTIATE_BEFORE_CONNECT) {
    bool rv;
    if (ordering == NEGOTIATE_BEFORE_CONNECT) {
      Negotiate(client1_role, client2_role);
      rv = client1_.Connect(&client2_, false);
    } else {
      client1_.SetupChannels(channel_ct_, cricket::ICEROLE_CONTROLLING);
      client2_.SetupChannels(channel_ct_, cricket::ICEROLE_CONTROLLED);
      client1_.MaybeSetSrtpCryptoSuites();
      client2_.MaybeSetSrtpCryptoSuites();
      // This is equivalent to an offer being processed on both sides, but an
      // answer not yet being received on the initiating side. So the
      // connection will be made before negotiation has finished on both sides.
      client1_.SetLocalTransportDescription(client1_.certificate(),
                                            cricket::CA_OFFER, client1_role, 0);
      client2_.SetRemoteTransportDescription(
          client1_.certificate(), cricket::CA_OFFER, client1_role, 0);
      client2_.SetLocalTransportDescription(
          client2_.certificate(), cricket::CA_ANSWER, client2_role, 0);
      rv = client1_.Connect(&client2_, false);
      client1_.SetRemoteTransportDescription(
          client2_.certificate(), cricket::CA_ANSWER, client2_role, 0);
    }

    EXPECT_TRUE(rv);
    if (!rv)
      return false;

    EXPECT_TRUE_SIMULATED_WAIT(client1_.all_dtls_transports_writable() &&
                                   client2_.all_dtls_transports_writable(),
                               kTimeout, fake_clock_);
    if (!client1_.all_dtls_transports_writable() ||
        !client2_.all_dtls_transports_writable())
      return false;

    // Check that we used the right roles.
    if (use_dtls_) {
      rtc::SSLRole client1_ssl_role =
          (client1_role == cricket::CONNECTIONROLE_ACTIVE ||
           (client2_role == cricket::CONNECTIONROLE_PASSIVE &&
            client1_role == cricket::CONNECTIONROLE_ACTPASS)) ?
              rtc::SSL_CLIENT : rtc::SSL_SERVER;

      rtc::SSLRole client2_ssl_role =
          (client2_role == cricket::CONNECTIONROLE_ACTIVE ||
           (client1_role == cricket::CONNECTIONROLE_PASSIVE &&
            client2_role == cricket::CONNECTIONROLE_ACTPASS)) ?
              rtc::SSL_CLIENT : rtc::SSL_SERVER;

      client1_.CheckRole(client1_ssl_role);
      client2_.CheckRole(client2_ssl_role);
    }

    // Check that we negotiated the right ciphers.
    if (use_dtls_srtp_) {
      client1_.CheckSrtp(rtc::SRTP_AES128_CM_SHA1_80);
      client2_.CheckSrtp(rtc::SRTP_AES128_CM_SHA1_80);
    } else {
      client1_.CheckSrtp(rtc::SRTP_INVALID_CRYPTO_SUITE);
      client2_.CheckSrtp(rtc::SRTP_INVALID_CRYPTO_SUITE);
    }

    client1_.CheckSsl();
    client2_.CheckSsl();

    return true;
  }

  bool Connect() {
    // By default, Client1 will be Server and Client2 will be Client.
    return Connect(cricket::CONNECTIONROLE_ACTPASS,
                   cricket::CONNECTIONROLE_ACTIVE);
  }

  void Negotiate() {
    Negotiate(cricket::CONNECTIONROLE_ACTPASS, cricket::CONNECTIONROLE_ACTIVE);
  }

  void Negotiate(ConnectionRole client1_role, ConnectionRole client2_role) {
    client1_.SetupChannels(channel_ct_, cricket::ICEROLE_CONTROLLING);
    client2_.SetupChannels(channel_ct_, cricket::ICEROLE_CONTROLLED);
    // Expect success from SLTD and SRTD.
    client1_.Negotiate(&client2_, cricket::CA_OFFER,
                       client1_role, client2_role, 0);
    client2_.Negotiate(&client1_, cricket::CA_ANSWER,
                       client2_role, client1_role, 0);
  }

  // Negotiate with legacy client |client2|. Legacy client doesn't use setup
  // attributes, except NONE.
  void NegotiateWithLegacy() {
    client1_.SetupChannels(channel_ct_, cricket::ICEROLE_CONTROLLING);
    client2_.SetupChannels(channel_ct_, cricket::ICEROLE_CONTROLLED);
    // Expect success from SLTD and SRTD.
    client1_.Negotiate(&client2_, cricket::CA_OFFER,
                       cricket::CONNECTIONROLE_ACTPASS,
                       cricket::CONNECTIONROLE_NONE, 0);
    client2_.Negotiate(&client1_, cricket::CA_ANSWER,
                       cricket::CONNECTIONROLE_ACTIVE,
                       cricket::CONNECTIONROLE_NONE, 0);
  }

  void Renegotiate(DtlsTestClient* reoffer_initiator,
                   ConnectionRole client1_role, ConnectionRole client2_role,
                   int flags) {
    if (reoffer_initiator == &client1_) {
      client1_.Negotiate(&client2_, cricket::CA_OFFER,
                         client1_role, client2_role, flags);
      client2_.Negotiate(&client1_, cricket::CA_ANSWER,
                         client2_role, client1_role, flags);
    } else {
      client2_.Negotiate(&client1_, cricket::CA_OFFER,
                         client2_role, client1_role, flags);
      client1_.Negotiate(&client2_, cricket::CA_ANSWER,
                         client1_role, client2_role, flags);
    }
  }

  void TestTransfer(size_t transport, size_t size, size_t count, bool srtp) {
    LOG(LS_INFO) << "Expect packets, size=" << size;
    client2_.ExpectPackets(transport, size);
    client1_.SendPackets(transport, size, count, srtp);
    EXPECT_EQ_SIMULATED_WAIT(count, client2_.NumPacketsReceived(), kTimeout,
                             fake_clock_);
  }

 protected:
  rtc::ScopedFakeClock fake_clock_;
  DtlsTestClient client1_;
  DtlsTestClient client2_;
  int channel_ct_;
  bool use_dtls_;
  bool use_dtls_srtp_;
  rtc::SSLProtocolVersion ssl_expected_version_;
};

class DtlsTransportChannelTest : public DtlsTransportChannelTestBase,
                                 public ::testing::Test {};

// Test that transport negotiation of ICE, no DTLS works properly.
TEST_F(DtlsTransportChannelTest, TestChannelSetupIce) {
  Negotiate();
  cricket::FakeIceTransport* channel1 = client1_.GetFakeIceTransort(0);
  cricket::FakeIceTransport* channel2 = client2_.GetFakeIceTransort(0);
  ASSERT_TRUE(channel1 != NULL);
  ASSERT_TRUE(channel2 != NULL);
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, channel1->GetIceRole());
  EXPECT_EQ(1U, channel1->IceTiebreaker());
  EXPECT_EQ(kIceUfrag1, channel1->ice_ufrag());
  EXPECT_EQ(kIcePwd1, channel1->ice_pwd());
  EXPECT_EQ(cricket::ICEROLE_CONTROLLED, channel2->GetIceRole());
  EXPECT_EQ(2U, channel2->IceTiebreaker());
}

// Connect without DTLS, and transfer some data.
TEST_F(DtlsTransportChannelTest, TestTransfer) {
  ASSERT_TRUE(Connect());
  TestTransfer(0, 1000, 100, false);
}

// Connect without DTLS, and transfer some data.
TEST_F(DtlsTransportChannelTest, TestOnSentPacket) {
  ASSERT_TRUE(Connect());
  EXPECT_EQ(client1_.sent_packet().send_time_ms, -1);
  TestTransfer(0, 1000, 100, false);
  EXPECT_EQ(kFakePacketId, client1_.sent_packet().packet_id);
  EXPECT_GE(client1_.sent_packet().send_time_ms, 0);
}

// Create two channels without DTLS, and transfer some data.
TEST_F(DtlsTransportChannelTest, TestTransferTwoChannels) {
  SetChannelCount(2);
  ASSERT_TRUE(Connect());
  TestTransfer(0, 1000, 100, false);
  TestTransfer(1, 1000, 100, false);
}

// Connect without DTLS, and transfer SRTP data.
TEST_F(DtlsTransportChannelTest, TestTransferSrtp) {
  ASSERT_TRUE(Connect());
  TestTransfer(0, 1000, 100, true);
}

// Create two channels without DTLS, and transfer SRTP data.
TEST_F(DtlsTransportChannelTest, TestTransferSrtpTwoChannels) {
  SetChannelCount(2);
  ASSERT_TRUE(Connect());
  TestTransfer(0, 1000, 100, true);
  TestTransfer(1, 1000, 100, true);
}

// Connect with DTLS, and transfer some data.
TEST_F(DtlsTransportChannelTest, TestTransferDtls) {
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  ASSERT_TRUE(Connect());
  TestTransfer(0, 1000, 100, false);
}

// Create two channels with DTLS, and transfer some data.
TEST_F(DtlsTransportChannelTest, TestTransferDtlsTwoChannels) {
  SetChannelCount(2);
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  ASSERT_TRUE(Connect());
  TestTransfer(0, 1000, 100, false);
  TestTransfer(1, 1000, 100, false);
}

// Connect with A doing DTLS and B not, and transfer some data.
TEST_F(DtlsTransportChannelTest, TestTransferDtlsRejected) {
  PrepareDtls(true, false, rtc::KT_DEFAULT);
  ASSERT_TRUE(Connect());
  TestTransfer(0, 1000, 100, false);
}

// Connect with B doing DTLS and A not, and transfer some data.
TEST_F(DtlsTransportChannelTest, TestTransferDtlsNotOffered) {
  PrepareDtls(false, true, rtc::KT_DEFAULT);
  ASSERT_TRUE(Connect());
  TestTransfer(0, 1000, 100, false);
}

// Create two channels with DTLS 1.0 and check ciphers.
TEST_F(DtlsTransportChannelTest, TestDtls12None) {
  SetChannelCount(2);
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  SetMaxProtocolVersions(rtc::SSL_PROTOCOL_DTLS_10, rtc::SSL_PROTOCOL_DTLS_10);
  ASSERT_TRUE(Connect());
}

// Create two channels with DTLS 1.2 and check ciphers.
TEST_F(DtlsTransportChannelTest, TestDtls12Both) {
  SetChannelCount(2);
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  SetMaxProtocolVersions(rtc::SSL_PROTOCOL_DTLS_12, rtc::SSL_PROTOCOL_DTLS_12);
  ASSERT_TRUE(Connect());
}

// Create two channels with DTLS 1.0 / DTLS 1.2 and check ciphers.
TEST_F(DtlsTransportChannelTest, TestDtls12Client1) {
  SetChannelCount(2);
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  SetMaxProtocolVersions(rtc::SSL_PROTOCOL_DTLS_12, rtc::SSL_PROTOCOL_DTLS_10);
  ASSERT_TRUE(Connect());
}

// Create two channels with DTLS 1.2 / DTLS 1.0 and check ciphers.
TEST_F(DtlsTransportChannelTest, TestDtls12Client2) {
  SetChannelCount(2);
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  SetMaxProtocolVersions(rtc::SSL_PROTOCOL_DTLS_10, rtc::SSL_PROTOCOL_DTLS_12);
  ASSERT_TRUE(Connect());
}

// Connect with DTLS, negotiate DTLS-SRTP, and transfer SRTP using bypass.
TEST_F(DtlsTransportChannelTest, TestTransferDtlsSrtp) {
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  PrepareDtlsSrtp(true, true);
  ASSERT_TRUE(Connect());
  TestTransfer(0, 1000, 100, true);
}

// Connect with DTLS-SRTP, transfer an invalid SRTP packet, and expects -1
// returned.
TEST_F(DtlsTransportChannelTest, TestTransferDtlsInvalidSrtpPacket) {
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  PrepareDtlsSrtp(true, true);
  ASSERT_TRUE(Connect());
  int result = client1_.SendInvalidSrtpPacket(0, 100);
  ASSERT_EQ(-1, result);
}

// Connect with DTLS. A does DTLS-SRTP but B does not.
TEST_F(DtlsTransportChannelTest, TestTransferDtlsSrtpRejected) {
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  PrepareDtlsSrtp(true, false);
  ASSERT_TRUE(Connect());
}

// Connect with DTLS. B does DTLS-SRTP but A does not.
TEST_F(DtlsTransportChannelTest, TestTransferDtlsSrtpNotOffered) {
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  PrepareDtlsSrtp(false, true);
  ASSERT_TRUE(Connect());
}

// Create two channels with DTLS, negotiate DTLS-SRTP, and transfer bypass SRTP.
TEST_F(DtlsTransportChannelTest, TestTransferDtlsSrtpTwoChannels) {
  SetChannelCount(2);
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  PrepareDtlsSrtp(true, true);
  ASSERT_TRUE(Connect());
  TestTransfer(0, 1000, 100, true);
  TestTransfer(1, 1000, 100, true);
}

// Create a single channel with DTLS, and send normal data and SRTP data on it.
TEST_F(DtlsTransportChannelTest, TestTransferDtlsSrtpDemux) {
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  PrepareDtlsSrtp(true, true);
  ASSERT_TRUE(Connect());
  TestTransfer(0, 1000, 100, false);
  TestTransfer(0, 1000, 100, true);
}

// Testing when the remote is passive.
TEST_F(DtlsTransportChannelTest, TestTransferDtlsAnswererIsPassive) {
  SetChannelCount(2);
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  PrepareDtlsSrtp(true, true);
  ASSERT_TRUE(Connect(cricket::CONNECTIONROLE_ACTPASS,
                      cricket::CONNECTIONROLE_PASSIVE));
  TestTransfer(0, 1000, 100, true);
  TestTransfer(1, 1000, 100, true);
}

// Testing with the legacy DTLS client which doesn't use setup attribute.
// In this case legacy is the answerer.
TEST_F(DtlsTransportChannelTest, TestDtlsSetupWithLegacyAsAnswerer) {
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  NegotiateWithLegacy();
  rtc::SSLRole channel1_role;
  rtc::SSLRole channel2_role;
  client1_.transport()->GetSslRole(&channel1_role);
  client2_.transport()->GetSslRole(&channel2_role);
  EXPECT_EQ(rtc::SSL_SERVER, channel1_role);
  EXPECT_EQ(rtc::SSL_CLIENT, channel2_role);
}

// Testing re offer/answer after the session is estbalished. Roles will be
// kept same as of the previous negotiation.
TEST_F(DtlsTransportChannelTest, TestDtlsReOfferFromOfferer) {
  SetChannelCount(2);
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  PrepareDtlsSrtp(true, true);
  // Initial role for client1 is ACTPASS and client2 is ACTIVE.
  ASSERT_TRUE(Connect(cricket::CONNECTIONROLE_ACTPASS,
                      cricket::CONNECTIONROLE_ACTIVE));
  TestTransfer(0, 1000, 100, true);
  TestTransfer(1, 1000, 100, true);
  // Using input roles for the re-offer.
  Renegotiate(&client1_, cricket::CONNECTIONROLE_ACTPASS,
              cricket::CONNECTIONROLE_ACTIVE, NF_REOFFER);
  TestTransfer(0, 1000, 100, true);
  TestTransfer(1, 1000, 100, true);
}

TEST_F(DtlsTransportChannelTest, TestDtlsReOfferFromAnswerer) {
  SetChannelCount(2);
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  PrepareDtlsSrtp(true, true);
  // Initial role for client1 is ACTPASS and client2 is ACTIVE.
  ASSERT_TRUE(Connect(cricket::CONNECTIONROLE_ACTPASS,
                      cricket::CONNECTIONROLE_ACTIVE));
  TestTransfer(0, 1000, 100, true);
  TestTransfer(1, 1000, 100, true);
  // Using input roles for the re-offer.
  Renegotiate(&client2_, cricket::CONNECTIONROLE_PASSIVE,
              cricket::CONNECTIONROLE_ACTPASS, NF_REOFFER);
  TestTransfer(0, 1000, 100, true);
  TestTransfer(1, 1000, 100, true);
}

// Test that any change in role after the intial setup will result in failure.
TEST_F(DtlsTransportChannelTest, TestDtlsRoleReversal) {
  SetChannelCount(2);
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  PrepareDtlsSrtp(true, true);
  ASSERT_TRUE(Connect(cricket::CONNECTIONROLE_ACTPASS,
                      cricket::CONNECTIONROLE_PASSIVE));

  // Renegotiate from client2 with actpass and client1 as active.
  Renegotiate(&client2_, cricket::CONNECTIONROLE_ACTPASS,
              cricket::CONNECTIONROLE_ACTIVE,
              NF_REOFFER | NF_EXPECT_FAILURE);
}

// Test that using different setup attributes which results in similar ssl
// role as the initial negotiation will result in success.
TEST_F(DtlsTransportChannelTest, TestDtlsReOfferWithDifferentSetupAttr) {
  SetChannelCount(2);
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  PrepareDtlsSrtp(true, true);
  ASSERT_TRUE(Connect(cricket::CONNECTIONROLE_ACTPASS,
                      cricket::CONNECTIONROLE_PASSIVE));
  // Renegotiate from client2 with actpass and client1 as active.
  Renegotiate(&client2_, cricket::CONNECTIONROLE_ACTIVE,
              cricket::CONNECTIONROLE_ACTPASS, NF_REOFFER);
  TestTransfer(0, 1000, 100, true);
  TestTransfer(1, 1000, 100, true);
}

// Test that re-negotiation can be started before the clients become connected
// in the first negotiation.
TEST_F(DtlsTransportChannelTest, TestRenegotiateBeforeConnect) {
  SetChannelCount(2);
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  PrepareDtlsSrtp(true, true);
  Negotiate();

  Renegotiate(&client1_, cricket::CONNECTIONROLE_ACTPASS,
              cricket::CONNECTIONROLE_ACTIVE, NF_REOFFER);
  bool rv = client1_.Connect(&client2_, false);
  EXPECT_TRUE(rv);
  EXPECT_TRUE_SIMULATED_WAIT(client1_.all_dtls_transports_writable() &&
                                 client2_.all_dtls_transports_writable(),
                             kTimeout, fake_clock_);

  TestTransfer(0, 1000, 100, true);
  TestTransfer(1, 1000, 100, true);
}

// Test Certificates state after negotiation but before connection.
TEST_F(DtlsTransportChannelTest, TestCertificatesBeforeConnect) {
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  Negotiate();

  rtc::scoped_refptr<rtc::RTCCertificate> certificate1;
  rtc::scoped_refptr<rtc::RTCCertificate> certificate2;
  std::unique_ptr<rtc::SSLCertificate> remote_cert1;
  std::unique_ptr<rtc::SSLCertificate> remote_cert2;

  // After negotiation, each side has a distinct local certificate, but still no
  // remote certificate, because connection has not yet occurred.
  ASSERT_TRUE(client1_.transport()->GetLocalCertificate(&certificate1));
  ASSERT_TRUE(client2_.transport()->GetLocalCertificate(&certificate2));
  ASSERT_NE(certificate1->ssl_certificate().ToPEMString(),
            certificate2->ssl_certificate().ToPEMString());
  ASSERT_FALSE(client1_.GetDtlsTransport(0)->GetRemoteSSLCertificate());
  ASSERT_FALSE(client2_.GetDtlsTransport(0)->GetRemoteSSLCertificate());
}

// Test Certificates state after connection.
TEST_F(DtlsTransportChannelTest, TestCertificatesAfterConnect) {
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  ASSERT_TRUE(Connect());

  rtc::scoped_refptr<rtc::RTCCertificate> certificate1;
  rtc::scoped_refptr<rtc::RTCCertificate> certificate2;

  // After connection, each side has a distinct local certificate.
  ASSERT_TRUE(client1_.transport()->GetLocalCertificate(&certificate1));
  ASSERT_TRUE(client2_.transport()->GetLocalCertificate(&certificate2));
  ASSERT_NE(certificate1->ssl_certificate().ToPEMString(),
            certificate2->ssl_certificate().ToPEMString());

  // Each side's remote certificate is the other side's local certificate.
  std::unique_ptr<rtc::SSLCertificate> remote_cert1 =
      client1_.GetDtlsTransport(0)->GetRemoteSSLCertificate();
  ASSERT_TRUE(remote_cert1);
  ASSERT_EQ(remote_cert1->ToPEMString(),
            certificate2->ssl_certificate().ToPEMString());
  std::unique_ptr<rtc::SSLCertificate> remote_cert2 =
      client2_.GetDtlsTransport(0)->GetRemoteSSLCertificate();
  ASSERT_TRUE(remote_cert2);
  ASSERT_EQ(remote_cert2->ToPEMString(),
            certificate1->ssl_certificate().ToPEMString());
}

// Test that packets are retransmitted according to the expected schedule.
// Each time a timeout occurs, the retransmission timer should be doubled up to
// 60 seconds. The timer defaults to 1 second, but for WebRTC we should be
// initializing it to 50ms.
TEST_F(DtlsTransportChannelTest, TestRetransmissionSchedule) {
  // We can only change the retransmission schedule with a recently-added
  // BoringSSL API. Skip the test if not built with BoringSSL.
  MAYBE_SKIP_TEST(IsBoringSsl);

  PrepareDtls(true, true, rtc::KT_DEFAULT);
  // Exchange transport descriptions.
  Negotiate(cricket::CONNECTIONROLE_ACTPASS, cricket::CONNECTIONROLE_ACTIVE);

  // Make client2_ writable, but not client1_.
  // This means client1_ will send DTLS client hellos but get no response.
  EXPECT_TRUE(client2_.Connect(&client1_, true));
  EXPECT_TRUE_SIMULATED_WAIT(client2_.all_ice_transports_writable(), kTimeout,
                             fake_clock_);

  // Wait for the first client hello to be sent.
  EXPECT_EQ_WAIT(1, client1_.received_dtls_client_hellos(), kTimeout);
  EXPECT_FALSE(client1_.all_ice_transports_writable());

  static int timeout_schedule_ms[] = {50,   100,  200,   400,   800,   1600,
                                      3200, 6400, 12800, 25600, 51200, 60000};

  int expected_hellos = 1;
  for (size_t i = 0;
       i < (sizeof(timeout_schedule_ms) / sizeof(timeout_schedule_ms[0]));
       ++i) {
    // For each expected retransmission time, advance the fake clock a
    // millisecond before the expected time and verify that no unexpected
    // retransmissions were sent. Then advance it the final millisecond and
    // verify that the expected retransmission was sent.
    fake_clock_.AdvanceTime(
        rtc::TimeDelta::FromMilliseconds(timeout_schedule_ms[i] - 1));
    EXPECT_EQ(expected_hellos, client1_.received_dtls_client_hellos());
    fake_clock_.AdvanceTime(rtc::TimeDelta::FromMilliseconds(1));
    EXPECT_EQ(++expected_hellos, client1_.received_dtls_client_hellos());
  }
}

// Test that a DTLS connection can be made even if the underlying transport
// is connected before DTLS fingerprints/roles have been negotiated.
TEST_F(DtlsTransportChannelTest, TestConnectBeforeNegotiate) {
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  ASSERT_TRUE(Connect(cricket::CONNECTIONROLE_ACTPASS,
                      cricket::CONNECTIONROLE_ACTIVE,
                      CONNECT_BEFORE_NEGOTIATE));
  TestTransfer(0, 1000, 100, false);
}

// The following events can occur in many different orders:
// 1. Caller receives remote fingerprint.
// 2. Caller is writable.
// 3. Caller receives ClientHello.
// 4. DTLS handshake finishes.
//
// The tests below cover all causally consistent permutations of these events;
// the caller must be writable and receive a ClientHello before the handshake
// finishes, but otherwise any ordering is possible.
//
// For each permutation, the test verifies that a connection is established and
// fingerprint verified without any DTLS packet needing to be retransmitted.
//
// Each permutation is also tested with valid and invalid fingerprints,
// ensuring that the handshake fails with an invalid fingerprint.
enum DtlsTransportEvent {
  CALLER_RECEIVES_FINGERPRINT,
  CALLER_WRITABLE,
  CALLER_RECEIVES_CLIENTHELLO,
  HANDSHAKE_FINISHES
};

class DtlsEventOrderingTest
    : public DtlsTransportChannelTestBase,
      public ::testing::TestWithParam<
          ::testing::tuple<std::vector<DtlsTransportEvent>, bool>> {
 protected:
  // If |valid_fingerprint| is false, the caller will receive a fingerprint
  // that doesn't match the callee's certificate, so the handshake should fail.
  void TestEventOrdering(const std::vector<DtlsTransportEvent>& events,
                         bool valid_fingerprint) {
    // Pre-setup: Set local certificate on both caller and callee, and
    // remote fingerprint on callee, but neither is writable and the caller
    // doesn't have the callee's fingerprint.
    PrepareDtls(true, true, rtc::KT_DEFAULT);
    // Simulate packets being sent and arriving asynchronously.
    // Otherwise the entire DTLS handshake would occur in one clock tick, and
    // we couldn't inject method calls in the middle of it.
    int simulated_delay_ms = 10;
    client1_.SetupChannels(channel_ct_, cricket::ICEROLE_CONTROLLING,
                           simulated_delay_ms);
    client2_.SetupChannels(channel_ct_, cricket::ICEROLE_CONTROLLED,
                           simulated_delay_ms);
    client1_.SetLocalTransportDescription(client1_.certificate(),
                                          cricket::CA_OFFER,
                                          cricket::CONNECTIONROLE_ACTPASS, 0);
    client2_.Negotiate(&client1_, cricket::CA_ANSWER,
                       cricket::CONNECTIONROLE_ACTIVE,
                       cricket::CONNECTIONROLE_ACTPASS, 0);

    for (DtlsTransportEvent e : events) {
      switch (e) {
        case CALLER_RECEIVES_FINGERPRINT:
          if (valid_fingerprint) {
            client1_.SetRemoteTransportDescription(
                client2_.certificate(), cricket::CA_ANSWER,
                cricket::CONNECTIONROLE_ACTIVE, 0);
          } else {
            // Create a fingerprint with a correct algorithm but an invalid
            // digest.
            cricket::TransportDescription remote_desc =
                MakeTransportDescription(client2_.certificate(),
                                         cricket::CONNECTIONROLE_ACTIVE);
            ++(remote_desc.identity_fingerprint->digest[0]);
            // Even if certificate verification fails inside this method,
            // it should return true as long as the fingerprint was formatted
            // correctly.
            EXPECT_TRUE(client1_.transport()->SetRemoteTransportDescription(
                remote_desc, cricket::CA_ANSWER, nullptr));
          }
          break;
        case CALLER_WRITABLE:
          EXPECT_TRUE(client1_.Connect(&client2_, true));
          EXPECT_TRUE_SIMULATED_WAIT(client1_.all_ice_transports_writable(),
                                     kTimeout, fake_clock_);
          break;
        case CALLER_RECEIVES_CLIENTHELLO:
          // Sanity check that a ClientHello hasn't already been received.
          EXPECT_EQ(0, client1_.received_dtls_client_hellos());
          // Making client2_ writable will cause it to send the ClientHello.
          EXPECT_TRUE(client2_.Connect(&client1_, true));
          EXPECT_TRUE_SIMULATED_WAIT(client2_.all_ice_transports_writable(),
                                     kTimeout, fake_clock_);
          EXPECT_EQ_SIMULATED_WAIT(1, client1_.received_dtls_client_hellos(),
                                   kTimeout, fake_clock_);
          break;
        case HANDSHAKE_FINISHES:
          // Sanity check that the handshake hasn't already finished.
          EXPECT_FALSE(client1_.GetDtlsTransport(0)->IsDtlsConnected() ||
                       client1_.GetDtlsTransport(0)->dtls_state() ==
                           cricket::DTLS_TRANSPORT_FAILED);
          EXPECT_TRUE_SIMULATED_WAIT(
              client1_.GetDtlsTransport(0)->IsDtlsConnected() ||
                  client1_.GetDtlsTransport(0)->dtls_state() ==
                      cricket::DTLS_TRANSPORT_FAILED,
              kTimeout, fake_clock_);
          break;
      }
    }

    cricket::DtlsTransportState expected_final_state =
        valid_fingerprint ? cricket::DTLS_TRANSPORT_CONNECTED
                          : cricket::DTLS_TRANSPORT_FAILED;
    EXPECT_EQ_SIMULATED_WAIT(expected_final_state,
                             client1_.GetDtlsTransport(0)->dtls_state(),
                             kTimeout, fake_clock_);
    EXPECT_EQ_SIMULATED_WAIT(expected_final_state,
                             client2_.GetDtlsTransport(0)->dtls_state(),
                             kTimeout, fake_clock_);

    // Channel should be writable iff there was a valid fingerprint.
    EXPECT_EQ(valid_fingerprint, client1_.GetDtlsTransport(0)->writable());
    EXPECT_EQ(valid_fingerprint, client2_.GetDtlsTransport(0)->writable());

    // Check that no hello needed to be retransmitted.
    EXPECT_EQ(1, client1_.received_dtls_client_hellos());
    EXPECT_EQ(1, client2_.received_dtls_server_hellos());

    if (valid_fingerprint) {
      TestTransfer(0, 1000, 100, false);
    }
  }
};

TEST_P(DtlsEventOrderingTest, TestEventOrdering) {
  TestEventOrdering(::testing::get<0>(GetParam()),
                    ::testing::get<1>(GetParam()));
}

INSTANTIATE_TEST_CASE_P(
    TestEventOrdering,
    DtlsEventOrderingTest,
    ::testing::Combine(
        ::testing::Values(
            std::vector<DtlsTransportEvent>{
                CALLER_RECEIVES_FINGERPRINT, CALLER_WRITABLE,
                CALLER_RECEIVES_CLIENTHELLO, HANDSHAKE_FINISHES},
            std::vector<DtlsTransportEvent>{
                CALLER_WRITABLE, CALLER_RECEIVES_FINGERPRINT,
                CALLER_RECEIVES_CLIENTHELLO, HANDSHAKE_FINISHES},
            std::vector<DtlsTransportEvent>{
                CALLER_WRITABLE, CALLER_RECEIVES_CLIENTHELLO,
                CALLER_RECEIVES_FINGERPRINT, HANDSHAKE_FINISHES},
            std::vector<DtlsTransportEvent>{
                CALLER_WRITABLE, CALLER_RECEIVES_CLIENTHELLO,
                HANDSHAKE_FINISHES, CALLER_RECEIVES_FINGERPRINT},
            std::vector<DtlsTransportEvent>{
                CALLER_RECEIVES_FINGERPRINT, CALLER_RECEIVES_CLIENTHELLO,
                CALLER_WRITABLE, HANDSHAKE_FINISHES},
            std::vector<DtlsTransportEvent>{
                CALLER_RECEIVES_CLIENTHELLO, CALLER_RECEIVES_FINGERPRINT,
                CALLER_WRITABLE, HANDSHAKE_FINISHES},
            std::vector<DtlsTransportEvent>{
                CALLER_RECEIVES_CLIENTHELLO, CALLER_WRITABLE,
                CALLER_RECEIVES_FINGERPRINT, HANDSHAKE_FINISHES},
            std::vector<DtlsTransportEvent>{CALLER_RECEIVES_CLIENTHELLO,
                                            CALLER_WRITABLE, HANDSHAKE_FINISHES,
                                            CALLER_RECEIVES_FINGERPRINT}),
        ::testing::Bool()));
