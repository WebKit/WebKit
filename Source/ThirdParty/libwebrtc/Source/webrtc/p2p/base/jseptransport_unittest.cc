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

#include "p2p/base/fakedtlstransport.h"
#include "p2p/base/fakeicetransport.h"
#include "rtc_base/fakesslidentity.h"
#include "rtc_base/gunit.h"
#include "rtc_base/network.h"

using cricket::JsepTransport;
using cricket::FakeDtlsTransport;
using cricket::FakeIceTransport;
using cricket::IceRole;
using cricket::TransportDescription;
using rtc::SocketAddress;

static const char kIceUfrag1[] = "TESTICEUFRAG0001";
static const char kIcePwd1[] = "TESTICEPWD00000000000001";

static const char kIceUfrag2[] = "TESTICEUFRAG0002";
static const char kIcePwd2[] = "TESTICEPWD00000000000002";

class JsepTransportTest : public testing::Test, public sigslot::has_slots<> {
 public:
  JsepTransportTest() { RecreateTransport(); }

  bool SetupChannel() {
    fake_ice_transport_.reset(new FakeIceTransport(transport_->mid(), 1));
    fake_dtls_transport_.reset(
        new FakeDtlsTransport(fake_ice_transport_.get()));
    return transport_->AddChannel(fake_dtls_transport_.get(), 1);
  }

  void DestroyChannel() { transport_->RemoveChannel(1); }

  void RecreateTransport() {
    transport_.reset(new JsepTransport("test content name", nullptr));
  }

 protected:
  std::unique_ptr<FakeDtlsTransport> fake_dtls_transport_;
  std::unique_ptr<FakeIceTransport> fake_ice_transport_;
  std::unique_ptr<JsepTransport> transport_;
};

// This test verifies channels are created with proper ICE
// ufrag/password after a transport description is applied.
TEST_F(JsepTransportTest, TestChannelIceParameters) {
  cricket::TransportDescription local_desc(kIceUfrag1, kIcePwd1);
  ASSERT_TRUE(transport_->SetLocalTransportDescription(
      local_desc, cricket::CA_OFFER, NULL));
  EXPECT_TRUE(SetupChannel());
  EXPECT_EQ(cricket::ICEMODE_FULL, fake_ice_transport_->remote_ice_mode());
  EXPECT_EQ(kIceUfrag1, fake_ice_transport_->ice_ufrag());
  EXPECT_EQ(kIcePwd1, fake_ice_transport_->ice_pwd());

  cricket::TransportDescription remote_desc(kIceUfrag1, kIcePwd1);
  ASSERT_TRUE(transport_->SetRemoteTransportDescription(
      remote_desc, cricket::CA_ANSWER, NULL));
  EXPECT_EQ(cricket::ICEMODE_FULL, fake_ice_transport_->remote_ice_mode());
  EXPECT_EQ(kIceUfrag1, fake_ice_transport_->remote_ice_ufrag());
  EXPECT_EQ(kIcePwd1, fake_ice_transport_->remote_ice_pwd());
}

// Verifies that IceCredentialsChanged returns true when either ufrag or pwd
// changed, and false in other cases.
TEST_F(JsepTransportTest, TestIceCredentialsChanged) {
  EXPECT_TRUE(cricket::IceCredentialsChanged("u1", "p1", "u2", "p2"));
  EXPECT_TRUE(cricket::IceCredentialsChanged("u1", "p1", "u2", "p1"));
  EXPECT_TRUE(cricket::IceCredentialsChanged("u1", "p1", "u1", "p2"));
  EXPECT_FALSE(cricket::IceCredentialsChanged("u1", "p1", "u1", "p1"));
}

// Tests SetNeedsIceRestartFlag and NeedsIceRestart, ensuring NeedsIceRestart
// only starts returning "false" once an ICE restart has been initiated.
TEST_F(JsepTransportTest, NeedsIceRestart) {
  // Do initial offer/answer so there's something to restart.
  cricket::TransportDescription local_desc(kIceUfrag1, kIcePwd1);
  cricket::TransportDescription remote_desc(kIceUfrag1, kIcePwd1);
  ASSERT_TRUE(transport_->SetLocalTransportDescription(
      local_desc, cricket::CA_OFFER, nullptr));
  ASSERT_TRUE(transport_->SetRemoteTransportDescription(
      remote_desc, cricket::CA_ANSWER, nullptr));

  // Flag initially should be false.
  EXPECT_FALSE(transport_->NeedsIceRestart());

  // After setting flag, it should be true.
  transport_->SetNeedsIceRestartFlag();
  EXPECT_TRUE(transport_->NeedsIceRestart());

  // Doing an identical offer/answer shouldn't clear the flag.
  ASSERT_TRUE(transport_->SetLocalTransportDescription(
      local_desc, cricket::CA_OFFER, nullptr));
  ASSERT_TRUE(transport_->SetRemoteTransportDescription(
      remote_desc, cricket::CA_ANSWER, nullptr));
  EXPECT_TRUE(transport_->NeedsIceRestart());

  // Doing an offer/answer that restarts ICE should clear the flag.
  cricket::TransportDescription ice_restart_local_desc(kIceUfrag2, kIcePwd2);
  cricket::TransportDescription ice_restart_remote_desc(kIceUfrag2, kIcePwd2);
  ASSERT_TRUE(transport_->SetLocalTransportDescription(
      ice_restart_local_desc, cricket::CA_OFFER, nullptr));
  ASSERT_TRUE(transport_->SetRemoteTransportDescription(
      ice_restart_remote_desc, cricket::CA_ANSWER, nullptr));
  EXPECT_FALSE(transport_->NeedsIceRestart());
}

TEST_F(JsepTransportTest, TestGetStats) {
  EXPECT_TRUE(SetupChannel());
  cricket::TransportStats stats;
  EXPECT_TRUE(transport_->GetStats(&stats));
  // Note that this tests the behavior of a FakeIceTransport.
  ASSERT_EQ(1U, stats.channel_stats.size());
  EXPECT_EQ(1, stats.channel_stats[0].component);
  // Set local transport description for FakeTransport before connecting.
  TransportDescription faketransport_desc(
      std::vector<std::string>(),
      rtc::CreateRandomString(cricket::ICE_UFRAG_LENGTH),
      rtc::CreateRandomString(cricket::ICE_PWD_LENGTH), cricket::ICEMODE_FULL,
      cricket::CONNECTIONROLE_NONE, nullptr);
  transport_->SetLocalTransportDescription(faketransport_desc,
                                           cricket::CA_OFFER, nullptr);
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
  rtc::scoped_refptr<rtc::RTCCertificate> certificate =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("testing", rtc::KT_ECDSA)));
  std::unique_ptr<rtc::SSLFingerprint> fingerprint(
      rtc::SSLFingerprint::CreateFromCertificate(certificate));

  TransportDescription local_desc(kIceUfrag1, kIcePwd1);
  TransportDescription remote_desc(kIceUfrag2, kIcePwd2);
  // Just use the same fingerprint in both descriptions; the remote fingerprint
  // doesn't matter in a non end-to-end test.
  local_desc.identity_fingerprint.reset(
      TransportDescription::CopyFingerprint(fingerprint.get()));
  remote_desc.identity_fingerprint.reset(
      TransportDescription::CopyFingerprint(fingerprint.get()));

  struct NegotiateRoleParams {
    cricket::ConnectionRole local_role;
    cricket::ConnectionRole remote_role;
    cricket::ContentAction local_action;
    cricket::ContentAction remote_action;
  };

  std::string error_desc;

  // Parameters which set the SSL role to SSL_CLIENT.
  NegotiateRoleParams valid_client_params[] = {
      {cricket::CONNECTIONROLE_ACTIVE, cricket::CONNECTIONROLE_ACTPASS,
       cricket::CA_ANSWER, cricket::CA_OFFER},
      {cricket::CONNECTIONROLE_ACTIVE, cricket::CONNECTIONROLE_ACTPASS,
       cricket::CA_PRANSWER, cricket::CA_OFFER},
      {cricket::CONNECTIONROLE_ACTPASS, cricket::CONNECTIONROLE_PASSIVE,
       cricket::CA_OFFER, cricket::CA_ANSWER},
      {cricket::CONNECTIONROLE_ACTPASS, cricket::CONNECTIONROLE_PASSIVE,
       cricket::CA_OFFER, cricket::CA_PRANSWER}};

  for (auto& param : valid_client_params) {
    RecreateTransport();
    transport_->SetLocalCertificate(certificate);

    local_desc.connection_role = param.local_role;
    remote_desc.connection_role = param.remote_role;

    // Set the offer first.
    if (param.local_action == cricket::CA_OFFER) {
      EXPECT_TRUE(transport_->SetLocalTransportDescription(
          local_desc, param.local_action, nullptr));
      EXPECT_TRUE(transport_->SetRemoteTransportDescription(
          remote_desc, param.remote_action, nullptr));
    } else {
      EXPECT_TRUE(transport_->SetRemoteTransportDescription(
          remote_desc, param.remote_action, nullptr));
      EXPECT_TRUE(transport_->SetLocalTransportDescription(
          local_desc, param.local_action, nullptr));
    }
    EXPECT_EQ(rtc::SSL_CLIENT, *transport_->GetSslRole());
  }

  // Parameters which set the SSL role to SSL_SERVER.
  NegotiateRoleParams valid_server_params[] = {
      {cricket::CONNECTIONROLE_PASSIVE, cricket::CONNECTIONROLE_ACTPASS,
       cricket::CA_ANSWER, cricket::CA_OFFER},
      {cricket::CONNECTIONROLE_PASSIVE, cricket::CONNECTIONROLE_ACTPASS,
       cricket::CA_PRANSWER, cricket::CA_OFFER},
      {cricket::CONNECTIONROLE_ACTPASS, cricket::CONNECTIONROLE_ACTIVE,
       cricket::CA_OFFER, cricket::CA_ANSWER},
      {cricket::CONNECTIONROLE_ACTPASS, cricket::CONNECTIONROLE_ACTIVE,
       cricket::CA_OFFER, cricket::CA_PRANSWER}};

  for (auto& param : valid_server_params) {
    RecreateTransport();
    transport_->SetLocalCertificate(certificate);

    local_desc.connection_role = param.local_role;
    remote_desc.connection_role = param.remote_role;

    // Set the offer first.
    if (param.local_action == cricket::CA_OFFER) {
      EXPECT_TRUE(transport_->SetLocalTransportDescription(
          local_desc, param.local_action, nullptr));
      EXPECT_TRUE(transport_->SetRemoteTransportDescription(
          remote_desc, param.remote_action, nullptr));
    } else {
      EXPECT_TRUE(transport_->SetRemoteTransportDescription(
          remote_desc, param.remote_action, nullptr));
      EXPECT_TRUE(transport_->SetLocalTransportDescription(
          local_desc, param.local_action, nullptr));
    }
    EXPECT_EQ(rtc::SSL_SERVER, *transport_->GetSslRole());
  }

  // Invalid parameters due to both peers having a duplicate role.
  NegotiateRoleParams duplicate_params[] = {
      {cricket::CONNECTIONROLE_ACTIVE, cricket::CONNECTIONROLE_ACTIVE,
       cricket::CA_ANSWER, cricket::CA_OFFER},
      {cricket::CONNECTIONROLE_ACTPASS, cricket::CONNECTIONROLE_ACTPASS,
       cricket::CA_ANSWER, cricket::CA_OFFER},
      {cricket::CONNECTIONROLE_PASSIVE, cricket::CONNECTIONROLE_PASSIVE,
       cricket::CA_ANSWER, cricket::CA_OFFER},
      {cricket::CONNECTIONROLE_ACTIVE, cricket::CONNECTIONROLE_ACTIVE,
       cricket::CA_PRANSWER, cricket::CA_OFFER},
      {cricket::CONNECTIONROLE_ACTPASS, cricket::CONNECTIONROLE_ACTPASS,
       cricket::CA_PRANSWER, cricket::CA_OFFER},
      {cricket::CONNECTIONROLE_PASSIVE, cricket::CONNECTIONROLE_PASSIVE,
       cricket::CA_PRANSWER, cricket::CA_OFFER},
      {cricket::CONNECTIONROLE_ACTIVE, cricket::CONNECTIONROLE_ACTIVE,
       cricket::CA_OFFER, cricket::CA_ANSWER},
      {cricket::CONNECTIONROLE_ACTPASS, cricket::CONNECTIONROLE_ACTPASS,
       cricket::CA_OFFER, cricket::CA_ANSWER},
      {cricket::CONNECTIONROLE_PASSIVE, cricket::CONNECTIONROLE_PASSIVE,
       cricket::CA_OFFER, cricket::CA_ANSWER},
      {cricket::CONNECTIONROLE_ACTIVE, cricket::CONNECTIONROLE_ACTIVE,
       cricket::CA_OFFER, cricket::CA_PRANSWER},
      {cricket::CONNECTIONROLE_ACTPASS, cricket::CONNECTIONROLE_ACTPASS,
       cricket::CA_OFFER, cricket::CA_PRANSWER},
      {cricket::CONNECTIONROLE_PASSIVE, cricket::CONNECTIONROLE_PASSIVE,
       cricket::CA_OFFER, cricket::CA_PRANSWER}};

  for (auto& param : duplicate_params) {
    RecreateTransport();
    transport_->SetLocalCertificate(certificate);

    local_desc.connection_role = param.local_role;
    remote_desc.connection_role = param.remote_role;

    // Set the offer first.
    if (param.local_action == cricket::CA_OFFER) {
      EXPECT_TRUE(transport_->SetLocalTransportDescription(
          local_desc, param.local_action, nullptr));
      EXPECT_FALSE(transport_->SetRemoteTransportDescription(
          remote_desc, param.remote_action, nullptr));
    } else {
      EXPECT_TRUE(transport_->SetRemoteTransportDescription(
          remote_desc, param.remote_action, nullptr));
      EXPECT_FALSE(transport_->SetLocalTransportDescription(
          local_desc, param.local_action, nullptr));
    }
  }

  // Invalid parameters due to the offerer not using ACTPASS.
  NegotiateRoleParams offerer_without_actpass_params[] = {
      {cricket::CONNECTIONROLE_ACTIVE, cricket::CONNECTIONROLE_PASSIVE,
       cricket::CA_ANSWER, cricket::CA_OFFER},
      {cricket::CONNECTIONROLE_PASSIVE, cricket::CONNECTIONROLE_ACTIVE,
       cricket::CA_ANSWER, cricket::CA_OFFER},
      {cricket::CONNECTIONROLE_ACTPASS, cricket::CONNECTIONROLE_PASSIVE,
       cricket::CA_ANSWER, cricket::CA_OFFER},
      {cricket::CONNECTIONROLE_ACTIVE, cricket::CONNECTIONROLE_PASSIVE,
       cricket::CA_PRANSWER, cricket::CA_OFFER},
      {cricket::CONNECTIONROLE_PASSIVE, cricket::CONNECTIONROLE_ACTIVE,
       cricket::CA_PRANSWER, cricket::CA_OFFER},
      {cricket::CONNECTIONROLE_ACTPASS, cricket::CONNECTIONROLE_PASSIVE,
       cricket::CA_PRANSWER, cricket::CA_OFFER},
      {cricket::CONNECTIONROLE_ACTIVE, cricket::CONNECTIONROLE_PASSIVE,
       cricket::CA_OFFER, cricket::CA_ANSWER},
      {cricket::CONNECTIONROLE_PASSIVE, cricket::CONNECTIONROLE_ACTIVE,
       cricket::CA_OFFER, cricket::CA_ANSWER},
      {cricket::CONNECTIONROLE_PASSIVE, cricket::CONNECTIONROLE_ACTPASS,
       cricket::CA_OFFER, cricket::CA_ANSWER},
      {cricket::CONNECTIONROLE_ACTIVE, cricket::CONNECTIONROLE_PASSIVE,
       cricket::CA_OFFER, cricket::CA_PRANSWER},
      {cricket::CONNECTIONROLE_PASSIVE, cricket::CONNECTIONROLE_ACTIVE,
       cricket::CA_OFFER, cricket::CA_PRANSWER},
      {cricket::CONNECTIONROLE_PASSIVE, cricket::CONNECTIONROLE_ACTPASS,
       cricket::CA_OFFER, cricket::CA_PRANSWER}};

  for (auto& param : offerer_without_actpass_params) {
    RecreateTransport();
    transport_->SetLocalCertificate(certificate);

    local_desc.connection_role = param.local_role;
    remote_desc.connection_role = param.remote_role;

    // Set the offer first.
    // TODO(deadbeef): Really this should fail as soon as the offer is
    // attempted to be applied, and not when the answer is applied.
    if (param.local_action == cricket::CA_OFFER) {
      EXPECT_TRUE(transport_->SetLocalTransportDescription(
          local_desc, param.local_action, nullptr));
      EXPECT_FALSE(transport_->SetRemoteTransportDescription(
          remote_desc, param.remote_action, nullptr));
    } else {
      EXPECT_TRUE(transport_->SetRemoteTransportDescription(
          remote_desc, param.remote_action, nullptr));
      EXPECT_FALSE(transport_->SetLocalTransportDescription(
          local_desc, param.local_action, nullptr));
    }
  }
}

// Test that a remote offer with the current negotiated role can be accepted.
// This is allowed by dtls-sdp, though we'll never generate such an offer,
// since JSEP requires generating "actpass".
TEST_F(JsepTransportTest, RemoteOfferWithCurrentNegotiatedDtlsRole) {
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

  remote_desc.connection_role = cricket::CONNECTIONROLE_ACTPASS;
  local_desc.connection_role = cricket::CONNECTIONROLE_ACTIVE;

  // Normal initial offer/answer with "actpass" in the offer and "active" in
  // the answer.
  ASSERT_TRUE(transport_->SetRemoteTransportDescription(
      remote_desc, cricket::CA_OFFER, nullptr));
  ASSERT_TRUE(transport_->SetLocalTransportDescription(
      local_desc, cricket::CA_ANSWER, nullptr));

  // Sanity check that role was actually negotiated.
  rtc::Optional<rtc::SSLRole> role = transport_->GetSslRole();
  ASSERT_TRUE(role);
  EXPECT_EQ(rtc::SSL_CLIENT, *role);

  // Subsequent offer with current negotiated role of "passive".
  remote_desc.connection_role = cricket::CONNECTIONROLE_PASSIVE;
  EXPECT_TRUE(transport_->SetRemoteTransportDescription(
      remote_desc, cricket::CA_OFFER, nullptr));
  EXPECT_TRUE(transport_->SetLocalTransportDescription(
      local_desc, cricket::CA_ANSWER, nullptr));
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

  remote_desc.connection_role = cricket::CONNECTIONROLE_ACTPASS;
  local_desc.connection_role = cricket::CONNECTIONROLE_ACTIVE;

  // Normal initial offer/answer with "actpass" in the offer and "active" in
  // the answer.
  ASSERT_TRUE(transport_->SetRemoteTransportDescription(
      remote_desc, cricket::CA_OFFER, nullptr));
  ASSERT_TRUE(transport_->SetLocalTransportDescription(
      local_desc, cricket::CA_ANSWER, nullptr));

  // Sanity check that role was actually negotiated.
  rtc::Optional<rtc::SSLRole> role = transport_->GetSslRole();
  ASSERT_TRUE(role);
  EXPECT_EQ(rtc::SSL_CLIENT, *role);

  // Subsequent offer with "active", which is the opposite of the remote
  // endpoint's negotiated role.
  // TODO(deadbeef): Really this should fail as soon as the offer is
  // attempted to be applied, and not when the answer is applied.
  remote_desc.connection_role = cricket::CONNECTIONROLE_ACTIVE;
  EXPECT_TRUE(transport_->SetRemoteTransportDescription(
      remote_desc, cricket::CA_OFFER, nullptr));
  EXPECT_FALSE(transport_->SetLocalTransportDescription(
      local_desc, cricket::CA_ANSWER, nullptr));
}
