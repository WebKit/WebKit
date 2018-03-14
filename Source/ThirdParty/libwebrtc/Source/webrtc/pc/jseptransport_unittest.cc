/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "pc/jseptransport.h"

#include <memory>

#include "p2p/base/fakedtlstransport.h"
#include "p2p/base/fakeicetransport.h"
#include "rtc_base/fakesslidentity.h"
#include "rtc_base/gunit.h"
#include "rtc_base/network.h"

namespace cricket {

using rtc::SocketAddress;
using webrtc::SdpType;

static const char kIceUfrag1[] = "TESTICEUFRAG0001";
static const char kIcePwd1[] = "TESTICEPWD00000000000001";

static const char kIceUfrag2[] = "TESTICEUFRAG0002";
static const char kIcePwd2[] = "TESTICEPWD00000000000002";

TransportDescription MakeTransportDescription(
    const char* ufrag,
    const char* pwd,
    const rtc::scoped_refptr<rtc::RTCCertificate>& cert,
    ConnectionRole role = CONNECTIONROLE_NONE) {
  std::unique_ptr<rtc::SSLFingerprint> fingerprint;
  if (cert) {
    fingerprint.reset(rtc::SSLFingerprint::CreateFromCertificate(cert));
  }
  return TransportDescription(std::vector<std::string>(), ufrag, pwd,
                              ICEMODE_FULL, role, fingerprint.get());
}

class JsepTransportTest : public testing::Test, public sigslot::has_slots<> {
 public:
  JsepTransportTest() { RecreateTransport(); }

  bool SetupFakeTransports(int component) {
    fake_ice_transports_.emplace_back(
        new FakeIceTransport(transport_->mid(), component));
    fake_dtls_transports_.emplace_back(
        new FakeDtlsTransport(fake_ice_transports_.back().get()));
    return transport_->AddChannel(fake_dtls_transports_.back().get(),
                                  component);
  }

  void DestroyChannel(int component) { transport_->RemoveChannel(component); }

  void RecreateTransport() {
    transport_.reset(new JsepTransport("test content name", nullptr));
  }

  bool IceCredentialsChanged(const std::string& old_ufrag,
                             const std::string& old_pwd,
                             const std::string& new_ufrag,
                             const std::string& new_pwd) {
    return (old_ufrag != new_ufrag) || (old_pwd != new_pwd);
  }

 protected:
  std::vector<std::unique_ptr<FakeDtlsTransport>> fake_dtls_transports_;
  std::vector<std::unique_ptr<FakeIceTransport>> fake_ice_transports_;
  std::unique_ptr<JsepTransport> transport_;
};

// This test verifies transports are created with proper ICE ufrag/password
// after a transport description is applied.
TEST_F(JsepTransportTest, TestIceTransportParameters) {
  EXPECT_TRUE(SetupFakeTransports(1));
  TransportDescription local_desc(kIceUfrag1, kIcePwd1);
  ASSERT_TRUE(transport_->SetLocalTransportDescription(local_desc,
                                                       SdpType::kOffer, NULL));
  EXPECT_EQ(ICEMODE_FULL, fake_ice_transports_[0]->remote_ice_mode());
  EXPECT_EQ(kIceUfrag1, fake_ice_transports_[0]->ice_ufrag());
  EXPECT_EQ(kIcePwd1, fake_ice_transports_[0]->ice_pwd());

  TransportDescription remote_desc(kIceUfrag2, kIcePwd2);
  ASSERT_TRUE(transport_->SetRemoteTransportDescription(
      remote_desc, SdpType::kAnswer, NULL));
  EXPECT_EQ(ICEMODE_FULL, fake_ice_transports_[0]->remote_ice_mode());
  EXPECT_EQ(kIceUfrag2, fake_ice_transports_[0]->remote_ice_ufrag());
  EXPECT_EQ(kIcePwd2, fake_ice_transports_[0]->remote_ice_pwd());
}

// Similarly, test that DTLS parameters are applied after a transport
// description is applied.
TEST_F(JsepTransportTest, TestDtlsTransportParameters) {
  EXPECT_TRUE(SetupFakeTransports(1));

  // Create certificates.
  rtc::scoped_refptr<rtc::RTCCertificate> local_cert =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("local", rtc::KT_DEFAULT)));
  rtc::scoped_refptr<rtc::RTCCertificate> remote_cert =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("remote", rtc::KT_DEFAULT)));
  transport_->SetLocalCertificate(local_cert);

  // Apply offer/answer.
  TransportDescription local_desc = MakeTransportDescription(
      kIceUfrag1, kIcePwd1, local_cert, CONNECTIONROLE_ACTPASS);
  ASSERT_TRUE(transport_->SetLocalTransportDescription(
      local_desc, SdpType::kOffer, nullptr));
  TransportDescription remote_desc = MakeTransportDescription(
      kIceUfrag2, kIcePwd2, remote_cert, CONNECTIONROLE_ACTIVE);
  ASSERT_TRUE(transport_->SetRemoteTransportDescription(
      remote_desc, SdpType::kAnswer, nullptr));

  // Verify that SSL role and remote fingerprint were set correctly based on
  // transport descriptions.
  rtc::SSLRole role;
  EXPECT_TRUE(fake_dtls_transports_[0]->GetSslRole(&role));
  EXPECT_EQ(rtc::SSL_SERVER, role);  // Because remote description was "active".
  EXPECT_EQ(remote_desc.identity_fingerprint->ToString(),
            fake_dtls_transports_[0]->dtls_fingerprint().ToString());
}

// Same as above test, but with remote transport description using
// CONNECTIONROLE_PASSIVE, expecting SSL_CLIENT role.
TEST_F(JsepTransportTest, TestDtlsTransportParametersWithPassiveAnswer) {
  EXPECT_TRUE(SetupFakeTransports(1));

  // Create certificates.
  rtc::scoped_refptr<rtc::RTCCertificate> local_cert =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("local", rtc::KT_DEFAULT)));
  rtc::scoped_refptr<rtc::RTCCertificate> remote_cert =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("remote", rtc::KT_DEFAULT)));
  transport_->SetLocalCertificate(local_cert);

  // Apply offer/answer.
  TransportDescription local_desc = MakeTransportDescription(
      kIceUfrag1, kIcePwd1, local_cert, CONNECTIONROLE_ACTPASS);
  ASSERT_TRUE(transport_->SetLocalTransportDescription(
      local_desc, SdpType::kOffer, nullptr));
  TransportDescription remote_desc = MakeTransportDescription(
      kIceUfrag2, kIcePwd2, remote_cert, CONNECTIONROLE_PASSIVE);
  ASSERT_TRUE(transport_->SetRemoteTransportDescription(
      remote_desc, SdpType::kAnswer, nullptr));

  // Verify that SSL role and remote fingerprint were set correctly based on
  // transport descriptions.
  rtc::SSLRole role;
  EXPECT_TRUE(fake_dtls_transports_[0]->GetSslRole(&role));
  EXPECT_EQ(rtc::SSL_CLIENT,
            role);  // Because remote description was "passive".
  EXPECT_EQ(remote_desc.identity_fingerprint->ToString(),
            fake_dtls_transports_[0]->dtls_fingerprint().ToString());
}

// Add two DtlsTransports/IceTransports and make sure parameters are applied to
// both of them. Applicable when RTP/RTCP are not multiplexed, so they share
// the same parameters but different connections.
TEST_F(JsepTransportTest, TestTransportParametersAppliedToTwoComponents) {
  EXPECT_TRUE(SetupFakeTransports(1));
  EXPECT_TRUE(SetupFakeTransports(2));

  // Create certificates.
  rtc::scoped_refptr<rtc::RTCCertificate> local_cert =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("local", rtc::KT_DEFAULT)));
  rtc::scoped_refptr<rtc::RTCCertificate> remote_cert =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("remote", rtc::KT_DEFAULT)));
  transport_->SetLocalCertificate(local_cert);

  // Apply offer/answer.
  TransportDescription local_desc = MakeTransportDescription(
      kIceUfrag1, kIcePwd1, local_cert, CONNECTIONROLE_ACTPASS);
  ASSERT_TRUE(transport_->SetLocalTransportDescription(
      local_desc, SdpType::kOffer, nullptr));
  TransportDescription remote_desc = MakeTransportDescription(
      kIceUfrag2, kIcePwd2, remote_cert, CONNECTIONROLE_ACTIVE);
  ASSERT_TRUE(transport_->SetRemoteTransportDescription(
      remote_desc, SdpType::kAnswer, nullptr));

  for (int i = 0; i < 1; ++i) {
    // Verify parameters of ICE transports.
    EXPECT_EQ(ICEMODE_FULL, fake_ice_transports_[i]->remote_ice_mode());
    EXPECT_EQ(kIceUfrag1, fake_ice_transports_[i]->ice_ufrag());
    EXPECT_EQ(kIcePwd1, fake_ice_transports_[i]->ice_pwd());
    EXPECT_EQ(kIceUfrag2, fake_ice_transports_[i]->remote_ice_ufrag());
    EXPECT_EQ(kIcePwd2, fake_ice_transports_[i]->remote_ice_pwd());
    // Verify parameters of DTLS transports.
    rtc::SSLRole role;
    EXPECT_TRUE(fake_dtls_transports_[i]->GetSslRole(&role));
    EXPECT_EQ(rtc::SSL_SERVER,
              role);  // Because remote description was "active".
    EXPECT_EQ(remote_desc.identity_fingerprint->ToString(),
              fake_dtls_transports_[i]->dtls_fingerprint().ToString());
  }
}

// Verifies that IceCredentialsChanged returns true when either ufrag or pwd
// changed, and false in other cases.
TEST_F(JsepTransportTest, TestIceCredentialsChanged) {
  EXPECT_TRUE(IceCredentialsChanged("u1", "p1", "u2", "p2"));
  EXPECT_TRUE(IceCredentialsChanged("u1", "p1", "u2", "p1"));
  EXPECT_TRUE(IceCredentialsChanged("u1", "p1", "u1", "p2"));
  EXPECT_FALSE(IceCredentialsChanged("u1", "p1", "u1", "p1"));
}

// Tests SetNeedsIceRestartFlag and NeedsIceRestart, ensuring NeedsIceRestart
// only starts returning "false" once an ICE restart has been initiated.
TEST_F(JsepTransportTest, NeedsIceRestart) {
  // Do initial offer/answer so there's something to restart.
  TransportDescription local_desc(kIceUfrag1, kIcePwd1);
  TransportDescription remote_desc(kIceUfrag1, kIcePwd1);
  ASSERT_TRUE(transport_->SetLocalTransportDescription(
      local_desc, SdpType::kOffer, nullptr));
  ASSERT_TRUE(transport_->SetRemoteTransportDescription(
      remote_desc, SdpType::kAnswer, nullptr));

  // Flag initially should be false.
  EXPECT_FALSE(transport_->NeedsIceRestart());

  // After setting flag, it should be true.
  transport_->SetNeedsIceRestartFlag();
  EXPECT_TRUE(transport_->NeedsIceRestart());

  // Doing an identical offer/answer shouldn't clear the flag.
  ASSERT_TRUE(transport_->SetLocalTransportDescription(
      local_desc, SdpType::kOffer, nullptr));
  ASSERT_TRUE(transport_->SetRemoteTransportDescription(
      remote_desc, SdpType::kAnswer, nullptr));
  EXPECT_TRUE(transport_->NeedsIceRestart());

  // Doing an offer/answer that restarts ICE should clear the flag.
  TransportDescription ice_restart_local_desc(kIceUfrag2, kIcePwd2);
  TransportDescription ice_restart_remote_desc(kIceUfrag2, kIcePwd2);
  ASSERT_TRUE(transport_->SetLocalTransportDescription(
      ice_restart_local_desc, SdpType::kOffer, nullptr));
  ASSERT_TRUE(transport_->SetRemoteTransportDescription(
      ice_restart_remote_desc, SdpType::kAnswer, nullptr));
  EXPECT_FALSE(transport_->NeedsIceRestart());
}

TEST_F(JsepTransportTest, TestGetStats) {
  EXPECT_TRUE(SetupFakeTransports(1));
  TransportStats stats;
  EXPECT_TRUE(transport_->GetStats(&stats));
  // Note that this tests the behavior of a FakeIceTransport.
  ASSERT_EQ(1U, stats.channel_stats.size());
  EXPECT_EQ(1, stats.channel_stats[0].component);
  // Set local transport description for FakeTransport before connecting.
  TransportDescription faketransport_desc(
      std::vector<std::string>(), rtc::CreateRandomString(ICE_UFRAG_LENGTH),
      rtc::CreateRandomString(ICE_PWD_LENGTH), ICEMODE_FULL,
      CONNECTIONROLE_NONE, nullptr);
  transport_->SetLocalTransportDescription(faketransport_desc, SdpType::kOffer,
                                           nullptr);
  EXPECT_TRUE(transport_->GetStats(&stats));
  ASSERT_EQ(1U, stats.channel_stats.size());
  EXPECT_EQ(1, stats.channel_stats[0].component);
}

// Tests that VerifyCertificateFingerprint only returns true when the
// certificate matches the fingerprint.
TEST_F(JsepTransportTest, TestVerifyCertificateFingerprint) {
  std::string error_desc;
  EXPECT_FALSE(
      transport_->VerifyCertificateFingerprint(nullptr, nullptr, &error_desc));
  rtc::KeyType key_types[] = {rtc::KT_RSA, rtc::KT_ECDSA};

  for (auto& key_type : key_types) {
    rtc::scoped_refptr<rtc::RTCCertificate> certificate =
        rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
            rtc::SSLIdentity::Generate("testing", key_type)));
    ASSERT_NE(nullptr, certificate);

    std::string digest_algorithm;
    ASSERT_TRUE(certificate->ssl_certificate().GetSignatureDigestAlgorithm(
        &digest_algorithm));
    ASSERT_FALSE(digest_algorithm.empty());
    std::unique_ptr<rtc::SSLFingerprint> good_fingerprint(
        rtc::SSLFingerprint::Create(digest_algorithm, certificate->identity()));
    ASSERT_NE(nullptr, good_fingerprint);

    EXPECT_TRUE(transport_->VerifyCertificateFingerprint(
        certificate.get(), good_fingerprint.get(), &error_desc));
    EXPECT_FALSE(transport_->VerifyCertificateFingerprint(
        certificate.get(), nullptr, &error_desc));
    EXPECT_FALSE(transport_->VerifyCertificateFingerprint(
        nullptr, good_fingerprint.get(), &error_desc));

    rtc::SSLFingerprint bad_fingerprint = *good_fingerprint;
    bad_fingerprint.digest.AppendData("0", 1);
    EXPECT_FALSE(transport_->VerifyCertificateFingerprint(
        certificate.get(), &bad_fingerprint, &error_desc));
  }
}

// Tests the logic of DTLS role negotiation for an initial offer/answer.
TEST_F(JsepTransportTest, DtlsRoleNegotiation) {
  // Just use the same certificate for both sides; doesn't really matter in a
  // non end-to-end test.
  rtc::scoped_refptr<rtc::RTCCertificate> certificate =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("testing", rtc::KT_ECDSA)));

  TransportDescription local_desc =
      MakeTransportDescription(kIceUfrag1, kIcePwd1, certificate);
  TransportDescription remote_desc =
      MakeTransportDescription(kIceUfrag2, kIcePwd2, certificate);

  struct NegotiateRoleParams {
    ConnectionRole local_role;
    ConnectionRole remote_role;
    SdpType local_type;
    SdpType remote_type;
  };

  std::string error_desc;

  // Parameters which set the SSL role to SSL_CLIENT.
  NegotiateRoleParams valid_client_params[] = {
      {CONNECTIONROLE_ACTIVE, CONNECTIONROLE_ACTPASS, SdpType::kAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_ACTIVE, CONNECTIONROLE_ACTPASS, SdpType::kPrAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_ACTPASS, CONNECTIONROLE_PASSIVE, SdpType::kOffer,
       SdpType::kAnswer},
      {CONNECTIONROLE_ACTPASS, CONNECTIONROLE_PASSIVE, SdpType::kOffer,
       SdpType::kPrAnswer}};

  for (auto& param : valid_client_params) {
    RecreateTransport();
    transport_->SetLocalCertificate(certificate);

    local_desc.connection_role = param.local_role;
    remote_desc.connection_role = param.remote_role;

    // Set the offer first.
    if (param.local_type == SdpType::kOffer) {
      EXPECT_TRUE(transport_->SetLocalTransportDescription(
          local_desc, param.local_type, nullptr));
      EXPECT_TRUE(transport_->SetRemoteTransportDescription(
          remote_desc, param.remote_type, nullptr));
    } else {
      EXPECT_TRUE(transport_->SetRemoteTransportDescription(
          remote_desc, param.remote_type, nullptr));
      EXPECT_TRUE(transport_->SetLocalTransportDescription(
          local_desc, param.local_type, nullptr));
    }
    EXPECT_EQ(rtc::SSL_CLIENT, *transport_->GetSslRole());
  }

  // Parameters which set the SSL role to SSL_SERVER.
  NegotiateRoleParams valid_server_params[] = {
      {CONNECTIONROLE_PASSIVE, CONNECTIONROLE_ACTPASS, SdpType::kAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_PASSIVE, CONNECTIONROLE_ACTPASS, SdpType::kPrAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_ACTPASS, CONNECTIONROLE_ACTIVE, SdpType::kOffer,
       SdpType::kAnswer},
      {CONNECTIONROLE_ACTPASS, CONNECTIONROLE_ACTIVE, SdpType::kOffer,
       SdpType::kPrAnswer}};

  for (auto& param : valid_server_params) {
    RecreateTransport();
    transport_->SetLocalCertificate(certificate);

    local_desc.connection_role = param.local_role;
    remote_desc.connection_role = param.remote_role;

    // Set the offer first.
    if (param.local_type == SdpType::kOffer) {
      EXPECT_TRUE(transport_->SetLocalTransportDescription(
          local_desc, param.local_type, nullptr));
      EXPECT_TRUE(transport_->SetRemoteTransportDescription(
          remote_desc, param.remote_type, nullptr));
    } else {
      EXPECT_TRUE(transport_->SetRemoteTransportDescription(
          remote_desc, param.remote_type, nullptr));
      EXPECT_TRUE(transport_->SetLocalTransportDescription(
          local_desc, param.local_type, nullptr));
    }
    EXPECT_EQ(rtc::SSL_SERVER, *transport_->GetSslRole());
  }

  // Invalid parameters due to both peers having a duplicate role.
  NegotiateRoleParams duplicate_params[] = {
      {CONNECTIONROLE_ACTIVE, CONNECTIONROLE_ACTIVE, SdpType::kAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_ACTPASS, CONNECTIONROLE_ACTPASS, SdpType::kAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_PASSIVE, CONNECTIONROLE_PASSIVE, SdpType::kAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_ACTIVE, CONNECTIONROLE_ACTIVE, SdpType::kPrAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_ACTPASS, CONNECTIONROLE_ACTPASS, SdpType::kPrAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_PASSIVE, CONNECTIONROLE_PASSIVE, SdpType::kPrAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_ACTIVE, CONNECTIONROLE_ACTIVE, SdpType::kOffer,
       SdpType::kAnswer},
      {CONNECTIONROLE_ACTPASS, CONNECTIONROLE_ACTPASS, SdpType::kOffer,
       SdpType::kAnswer},
      {CONNECTIONROLE_PASSIVE, CONNECTIONROLE_PASSIVE, SdpType::kOffer,
       SdpType::kAnswer},
      {CONNECTIONROLE_ACTIVE, CONNECTIONROLE_ACTIVE, SdpType::kOffer,
       SdpType::kPrAnswer},
      {CONNECTIONROLE_ACTPASS, CONNECTIONROLE_ACTPASS, SdpType::kOffer,
       SdpType::kPrAnswer},
      {CONNECTIONROLE_PASSIVE, CONNECTIONROLE_PASSIVE, SdpType::kOffer,
       SdpType::kPrAnswer}};

  for (auto& param : duplicate_params) {
    RecreateTransport();
    transport_->SetLocalCertificate(certificate);

    local_desc.connection_role = param.local_role;
    remote_desc.connection_role = param.remote_role;

    // Set the offer first.
    if (param.local_type == SdpType::kOffer) {
      EXPECT_TRUE(transport_->SetLocalTransportDescription(
          local_desc, param.local_type, nullptr));
      EXPECT_FALSE(transport_->SetRemoteTransportDescription(
          remote_desc, param.remote_type, nullptr));
    } else {
      EXPECT_TRUE(transport_->SetRemoteTransportDescription(
          remote_desc, param.remote_type, nullptr));
      EXPECT_FALSE(transport_->SetLocalTransportDescription(
          local_desc, param.local_type, nullptr));
    }
  }

  // Invalid parameters due to the offerer not using ACTPASS.
  NegotiateRoleParams offerer_without_actpass_params[] = {
      {CONNECTIONROLE_ACTIVE, CONNECTIONROLE_PASSIVE, SdpType::kAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_PASSIVE, CONNECTIONROLE_ACTIVE, SdpType::kAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_ACTPASS, CONNECTIONROLE_PASSIVE, SdpType::kAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_ACTIVE, CONNECTIONROLE_PASSIVE, SdpType::kPrAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_PASSIVE, CONNECTIONROLE_ACTIVE, SdpType::kPrAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_ACTPASS, CONNECTIONROLE_PASSIVE, SdpType::kPrAnswer,
       SdpType::kOffer},
      {CONNECTIONROLE_ACTIVE, CONNECTIONROLE_PASSIVE, SdpType::kOffer,
       SdpType::kAnswer},
      {CONNECTIONROLE_PASSIVE, CONNECTIONROLE_ACTIVE, SdpType::kOffer,
       SdpType::kAnswer},
      {CONNECTIONROLE_PASSIVE, CONNECTIONROLE_ACTPASS, SdpType::kOffer,
       SdpType::kAnswer},
      {CONNECTIONROLE_ACTIVE, CONNECTIONROLE_PASSIVE, SdpType::kOffer,
       SdpType::kPrAnswer},
      {CONNECTIONROLE_PASSIVE, CONNECTIONROLE_ACTIVE, SdpType::kOffer,
       SdpType::kPrAnswer},
      {CONNECTIONROLE_PASSIVE, CONNECTIONROLE_ACTPASS, SdpType::kOffer,
       SdpType::kPrAnswer}};

  for (auto& param : offerer_without_actpass_params) {
    RecreateTransport();
    transport_->SetLocalCertificate(certificate);

    local_desc.connection_role = param.local_role;
    remote_desc.connection_role = param.remote_role;

    // Set the offer first.
    // TODO(deadbeef): Really this should fail as soon as the offer is
    // attempted to be applied, and not when the answer is applied.
    if (param.local_type == SdpType::kOffer) {
      EXPECT_TRUE(transport_->SetLocalTransportDescription(
          local_desc, param.local_type, nullptr));
      EXPECT_FALSE(transport_->SetRemoteTransportDescription(
          remote_desc, param.remote_type, nullptr));
    } else {
      EXPECT_TRUE(transport_->SetRemoteTransportDescription(
          remote_desc, param.remote_type, nullptr));
      EXPECT_FALSE(transport_->SetLocalTransportDescription(
          local_desc, param.local_type, nullptr));
    }
  }
}

// Test that a reoffer in the opposite direction is successful as long as the
// role isn't changing. Doesn't test every possible combination like the test
// above.
TEST_F(JsepTransportTest, ValidDtlsReofferFromAnswerer) {
  // Just use the same certificate for both sides; doesn't really matter in a
  // non end-to-end test.
  rtc::scoped_refptr<rtc::RTCCertificate> certificate =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("testing", rtc::KT_ECDSA)));
  transport_->SetLocalCertificate(certificate);

  TransportDescription local_offer = MakeTransportDescription(
      kIceUfrag1, kIcePwd1, certificate, CONNECTIONROLE_ACTPASS);
  TransportDescription remote_answer = MakeTransportDescription(
      kIceUfrag2, kIcePwd2, certificate, CONNECTIONROLE_ACTIVE);

  EXPECT_TRUE(transport_->SetLocalTransportDescription(
      local_offer, SdpType::kOffer, nullptr));
  EXPECT_TRUE(transport_->SetRemoteTransportDescription(
      remote_answer, SdpType::kAnswer, nullptr));

  // We were actpass->active previously, now in the other direction it's
  // actpass->passive.
  TransportDescription remote_offer = MakeTransportDescription(
      kIceUfrag2, kIcePwd2, certificate, CONNECTIONROLE_ACTPASS);
  TransportDescription local_answer = MakeTransportDescription(
      kIceUfrag1, kIcePwd1, certificate, CONNECTIONROLE_PASSIVE);

  EXPECT_TRUE(transport_->SetRemoteTransportDescription(
      remote_offer, SdpType::kOffer, nullptr));
  EXPECT_TRUE(transport_->SetLocalTransportDescription(
      local_answer, SdpType::kAnswer, nullptr));
}

// Test that a reoffer in the opposite direction fails if the role changes.
// Inverse of test above.
TEST_F(JsepTransportTest, InvalidDtlsReofferFromAnswerer) {
  // Just use the same certificate for both sides; doesn't really matter in a
  // non end-to-end test.
  rtc::scoped_refptr<rtc::RTCCertificate> certificate =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("testing", rtc::KT_ECDSA)));
  transport_->SetLocalCertificate(certificate);

  TransportDescription local_offer = MakeTransportDescription(
      kIceUfrag1, kIcePwd1, certificate, CONNECTIONROLE_ACTPASS);
  TransportDescription remote_answer = MakeTransportDescription(
      kIceUfrag2, kIcePwd2, certificate, CONNECTIONROLE_ACTIVE);

  EXPECT_TRUE(transport_->SetLocalTransportDescription(
      local_offer, SdpType::kOffer, nullptr));
  EXPECT_TRUE(transport_->SetRemoteTransportDescription(
      remote_answer, SdpType::kAnswer, nullptr));

  // Changing role to passive here isn't allowed. Though for some reason this
  // only fails in SetLocalTransportDescription.
  TransportDescription remote_offer = MakeTransportDescription(
      kIceUfrag2, kIcePwd2, certificate, CONNECTIONROLE_PASSIVE);
  TransportDescription local_answer = MakeTransportDescription(
      kIceUfrag1, kIcePwd1, certificate, CONNECTIONROLE_ACTIVE);

  EXPECT_TRUE(transport_->SetRemoteTransportDescription(
      remote_offer, SdpType::kOffer, nullptr));
  EXPECT_FALSE(transport_->SetLocalTransportDescription(
      local_answer, SdpType::kAnswer, nullptr));
}

// Test that a remote offer with the current negotiated role can be accepted.
// This is allowed by dtls-sdp, though we'll never generate such an offer,
// since JSEP requires generating "actpass".
TEST_F(JsepTransportTest, RemoteOfferWithCurrentNegotiatedDtlsRole) {
  // Just use the same certificate in both descriptions; the remote fingerprint
  // doesn't matter in a non end-to-end test.
  rtc::scoped_refptr<rtc::RTCCertificate> certificate =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("testing", rtc::KT_ECDSA)));
  transport_->SetLocalCertificate(certificate);

  TransportDescription remote_desc = MakeTransportDescription(
      kIceUfrag1, kIcePwd1, certificate, CONNECTIONROLE_ACTPASS);
  TransportDescription local_desc = MakeTransportDescription(
      kIceUfrag2, kIcePwd2, certificate, CONNECTIONROLE_ACTIVE);

  // Normal initial offer/answer with "actpass" in the offer and "active" in
  // the answer.
  ASSERT_TRUE(transport_->SetRemoteTransportDescription(
      remote_desc, SdpType::kOffer, nullptr));
  ASSERT_TRUE(transport_->SetLocalTransportDescription(
      local_desc, SdpType::kAnswer, nullptr));

  // Sanity check that role was actually negotiated.
  rtc::Optional<rtc::SSLRole> role = transport_->GetSslRole();
  ASSERT_TRUE(role);
  EXPECT_EQ(rtc::SSL_CLIENT, *role);

  // Subsequent offer with current negotiated role of "passive".
  remote_desc.connection_role = CONNECTIONROLE_PASSIVE;
  EXPECT_TRUE(transport_->SetRemoteTransportDescription(
      remote_desc, SdpType::kOffer, nullptr));
  EXPECT_TRUE(transport_->SetLocalTransportDescription(
      local_desc, SdpType::kAnswer, nullptr));
}

// Test that a remote offer with the inverse of the current negotiated DTLS
// role is rejected.
TEST_F(JsepTransportTest, RemoteOfferThatChangesNegotiatedDtlsRole) {
  rtc::scoped_refptr<rtc::RTCCertificate> certificate =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("testing", rtc::KT_ECDSA)));
  std::unique_ptr<rtc::SSLFingerprint> fingerprint(
      rtc::SSLFingerprint::CreateFromCertificate(certificate));
  transport_->SetLocalCertificate(certificate);

  TransportDescription local_desc(kIceUfrag1, kIcePwd1);
  TransportDescription remote_desc(kIceUfrag2, kIcePwd2);
  // Just use the same fingerprint in both descriptions; the remote fingerprint
  // doesn't matter in a non end-to-end test.
  local_desc.identity_fingerprint.reset(
      TransportDescription::CopyFingerprint(fingerprint.get()));
  remote_desc.identity_fingerprint.reset(
      TransportDescription::CopyFingerprint(fingerprint.get()));

  remote_desc.connection_role = CONNECTIONROLE_ACTPASS;
  local_desc.connection_role = CONNECTIONROLE_ACTIVE;

  // Normal initial offer/answer with "actpass" in the offer and "active" in
  // the answer.
  ASSERT_TRUE(transport_->SetRemoteTransportDescription(
      remote_desc, SdpType::kOffer, nullptr));
  ASSERT_TRUE(transport_->SetLocalTransportDescription(
      local_desc, SdpType::kAnswer, nullptr));

  // Sanity check that role was actually negotiated.
  rtc::Optional<rtc::SSLRole> role = transport_->GetSslRole();
  ASSERT_TRUE(role);
  EXPECT_EQ(rtc::SSL_CLIENT, *role);

  // Subsequent offer with "active", which is the opposite of the remote
  // endpoint's negotiated role.
  // TODO(deadbeef): Really this should fail as soon as the offer is
  // attempted to be applied, and not when the answer is applied.
  remote_desc.connection_role = CONNECTIONROLE_ACTIVE;
  EXPECT_TRUE(transport_->SetRemoteTransportDescription(
      remote_desc, SdpType::kOffer, nullptr));
  EXPECT_FALSE(transport_->SetLocalTransportDescription(
      local_desc, SdpType::kAnswer, nullptr));
}

// Testing that a legacy client that doesn't use the setup attribute will be
// interpreted as having an active role.
TEST_F(JsepTransportTest, TestDtlsSetupWithLegacyAsAnswerer) {
  rtc::scoped_refptr<rtc::RTCCertificate> certificate =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("testing", rtc::KT_ECDSA)));
  std::unique_ptr<rtc::SSLFingerprint> fingerprint(
      rtc::SSLFingerprint::CreateFromCertificate(certificate));
  transport_->SetLocalCertificate(certificate);

  TransportDescription local_desc(kIceUfrag1, kIcePwd1);
  TransportDescription remote_desc(kIceUfrag2, kIcePwd2);
  // Just use the same fingerprint in both descriptions; the remote fingerprint
  // doesn't matter in a non end-to-end test.
  local_desc.identity_fingerprint.reset(
      TransportDescription::CopyFingerprint(fingerprint.get()));
  remote_desc.identity_fingerprint.reset(
      TransportDescription::CopyFingerprint(fingerprint.get()));

  local_desc.connection_role = CONNECTIONROLE_ACTPASS;
  ASSERT_TRUE(transport_->SetLocalTransportDescription(
      local_desc, SdpType::kOffer, nullptr));
  // Use CONNECTIONROLE_NONE to simulate legacy endpoint.
  remote_desc.connection_role = CONNECTIONROLE_NONE;
  ASSERT_TRUE(transport_->SetRemoteTransportDescription(
      remote_desc, SdpType::kAnswer, nullptr));

  rtc::Optional<rtc::SSLRole> role = transport_->GetSslRole();
  ASSERT_TRUE(role);
  // Since legacy answer ommitted setup atribute, and we offered actpass, we
  // should act as passive (server).
  EXPECT_EQ(rtc::SSL_SERVER, *role);
}

}  // namespace cricket
