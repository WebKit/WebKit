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

#include "webrtc/base/fakesslidentity.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/network.h"
#include "webrtc/p2p/base/faketransportcontroller.h"
#include "webrtc/p2p/base/p2ptransport.h"

using cricket::Transport;
using cricket::FakeTransport;
using cricket::TransportChannel;
using cricket::FakeTransportChannel;
using cricket::IceRole;
using cricket::TransportDescription;
using rtc::SocketAddress;

static const char kIceUfrag1[] = "TESTICEUFRAG0001";
static const char kIcePwd1[] = "TESTICEPWD00000000000001";

static const char kIceUfrag2[] = "TESTICEUFRAG0002";
static const char kIcePwd2[] = "TESTICEPWD00000000000002";

class TransportTest : public testing::Test,
                      public sigslot::has_slots<> {
 public:
  TransportTest()
      : transport_(new FakeTransport("test content name")), channel_(NULL) {}
  ~TransportTest() {
    transport_->DestroyAllChannels();
  }
  bool SetupChannel() {
    channel_ = CreateChannel(1);
    return (channel_ != NULL);
  }
  FakeTransportChannel* CreateChannel(int component) {
    return static_cast<FakeTransportChannel*>(
        transport_->CreateChannel(component));
  }
  void DestroyChannel() {
    transport_->DestroyChannel(1);
    channel_ = NULL;
  }

 protected:
  std::unique_ptr<FakeTransport> transport_;
  FakeTransportChannel* channel_;
};

// This test verifies channels are created with proper ICE
// role, tiebreaker and remote ice mode and credentials after offer and
// answer negotiations.
TEST_F(TransportTest, TestChannelIceParameters) {
  transport_->SetIceRole(cricket::ICEROLE_CONTROLLING);
  transport_->SetIceTiebreaker(99U);
  cricket::TransportDescription local_desc(kIceUfrag1, kIcePwd1);
  ASSERT_TRUE(transport_->SetLocalTransportDescription(local_desc,
                                                       cricket::CA_OFFER,
                                                       NULL));
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, transport_->ice_role());
  EXPECT_TRUE(SetupChannel());
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, channel_->GetIceRole());
  EXPECT_EQ(cricket::ICEMODE_FULL, channel_->remote_ice_mode());
  EXPECT_EQ(kIceUfrag1, channel_->ice_ufrag());
  EXPECT_EQ(kIcePwd1, channel_->ice_pwd());

  cricket::TransportDescription remote_desc(kIceUfrag1, kIcePwd1);
  ASSERT_TRUE(transport_->SetRemoteTransportDescription(remote_desc,
                                                        cricket::CA_ANSWER,
                                                        NULL));
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, channel_->GetIceRole());
  EXPECT_EQ(99U, channel_->IceTiebreaker());
  EXPECT_EQ(cricket::ICEMODE_FULL, channel_->remote_ice_mode());
  // Changing the transport role from CONTROLLING to CONTROLLED.
  transport_->SetIceRole(cricket::ICEROLE_CONTROLLED);
  EXPECT_EQ(cricket::ICEROLE_CONTROLLED, channel_->GetIceRole());
  EXPECT_EQ(cricket::ICEMODE_FULL, channel_->remote_ice_mode());
  EXPECT_EQ(kIceUfrag1, channel_->remote_ice_ufrag());
  EXPECT_EQ(kIcePwd1, channel_->remote_ice_pwd());
}

// Verifies that IceCredentialsChanged returns true when either ufrag or pwd
// changed, and false in other cases.
TEST_F(TransportTest, TestIceCredentialsChanged) {
  EXPECT_TRUE(cricket::IceCredentialsChanged("u1", "p1", "u2", "p2"));
  EXPECT_TRUE(cricket::IceCredentialsChanged("u1", "p1", "u2", "p1"));
  EXPECT_TRUE(cricket::IceCredentialsChanged("u1", "p1", "u1", "p2"));
  EXPECT_FALSE(cricket::IceCredentialsChanged("u1", "p1", "u1", "p1"));
}

// This test verifies that the callee's ICE role remains the same when the
// callee triggers an ICE restart.
//
// RFC5245 currently says that the role *should* change on an ICE restart,
// but this rule was intended for an ICE restart that occurs when an endpoint
// is changing to ICE lite (which we already handle). See discussion here:
// https://mailarchive.ietf.org/arch/msg/ice/C0_QRCTNcwtvUF12y28jQicPR10
TEST_F(TransportTest, TestIceControlledToControllingOnIceRestart) {
  EXPECT_TRUE(SetupChannel());
  transport_->SetIceRole(cricket::ICEROLE_CONTROLLED);

  cricket::TransportDescription desc(kIceUfrag1, kIcePwd1);
  ASSERT_TRUE(transport_->SetRemoteTransportDescription(desc,
                                                        cricket::CA_OFFER,
                                                        NULL));
  ASSERT_TRUE(transport_->SetLocalTransportDescription(desc,
                                                       cricket::CA_ANSWER,
                                                       NULL));
  EXPECT_EQ(cricket::ICEROLE_CONTROLLED, transport_->ice_role());

  cricket::TransportDescription new_local_desc(kIceUfrag2, kIcePwd2);
  ASSERT_TRUE(transport_->SetLocalTransportDescription(new_local_desc,
                                                       cricket::CA_OFFER,
                                                       NULL));
  EXPECT_EQ(cricket::ICEROLE_CONTROLLED, transport_->ice_role());
  EXPECT_EQ(cricket::ICEROLE_CONTROLLED, channel_->GetIceRole());
}

// This test verifies that the caller's ICE role remains the same when the
// callee triggers an ICE restart.
//
// RFC5245 currently says that the role *should* change on an ICE restart,
// but this rule was intended for an ICE restart that occurs when an endpoint
// is changing to ICE lite (which we already handle). See discussion here:
// https://mailarchive.ietf.org/arch/msg/ice/C0_QRCTNcwtvUF12y28jQicPR10
TEST_F(TransportTest, TestIceControllingToControlledOnIceRestart) {
  EXPECT_TRUE(SetupChannel());
  transport_->SetIceRole(cricket::ICEROLE_CONTROLLING);

  cricket::TransportDescription desc(kIceUfrag1, kIcePwd1);
  ASSERT_TRUE(transport_->SetLocalTransportDescription(desc,
                                                       cricket::CA_OFFER,
                                                       NULL));
  ASSERT_TRUE(transport_->SetRemoteTransportDescription(desc,
                                                        cricket::CA_ANSWER,
                                                        NULL));
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, transport_->ice_role());

  cricket::TransportDescription new_local_desc(kIceUfrag2, kIcePwd2);
  ASSERT_TRUE(transport_->SetLocalTransportDescription(new_local_desc,
                                                       cricket::CA_ANSWER,
                                                       NULL));
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, transport_->ice_role());
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, channel_->GetIceRole());
}

// This test verifies that the caller's ICE role is still controlling after the
// callee triggers ICE restart if the callee's ICE mode is LITE.
TEST_F(TransportTest, TestIceControllingOnIceRestartIfRemoteIsIceLite) {
  EXPECT_TRUE(SetupChannel());
  transport_->SetIceRole(cricket::ICEROLE_CONTROLLING);

  cricket::TransportDescription desc(kIceUfrag1, kIcePwd1);
  ASSERT_TRUE(transport_->SetLocalTransportDescription(desc,
                                                       cricket::CA_OFFER,
                                                       NULL));

  cricket::TransportDescription remote_desc(
      std::vector<std::string>(), kIceUfrag1, kIcePwd1, cricket::ICEMODE_LITE,
      cricket::CONNECTIONROLE_NONE, NULL);
  ASSERT_TRUE(transport_->SetRemoteTransportDescription(remote_desc,
                                                        cricket::CA_ANSWER,
                                                        NULL));

  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, transport_->ice_role());

  cricket::TransportDescription new_local_desc(kIceUfrag2, kIcePwd2);
  ASSERT_TRUE(transport_->SetLocalTransportDescription(new_local_desc,
                                                       cricket::CA_ANSWER,
                                                       NULL));
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, transport_->ice_role());
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, channel_->GetIceRole());
}

// Tests channel role is reversed after receiving ice-lite from remote.
TEST_F(TransportTest, TestSetRemoteIceLiteInOffer) {
  transport_->SetIceRole(cricket::ICEROLE_CONTROLLED);
  cricket::TransportDescription remote_desc(
      std::vector<std::string>(), kIceUfrag1, kIcePwd1, cricket::ICEMODE_LITE,
      cricket::CONNECTIONROLE_ACTPASS, NULL);
  ASSERT_TRUE(transport_->SetRemoteTransportDescription(remote_desc,
                                                        cricket::CA_OFFER,
                                                        NULL));
  cricket::TransportDescription local_desc(kIceUfrag1, kIcePwd1);
  ASSERT_TRUE(transport_->SetLocalTransportDescription(local_desc,
                                                       cricket::CA_ANSWER,
                                                       NULL));
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, transport_->ice_role());
  EXPECT_TRUE(SetupChannel());
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, channel_->GetIceRole());
  EXPECT_EQ(cricket::ICEMODE_LITE, channel_->remote_ice_mode());
}

// Tests ice-lite in remote answer.
TEST_F(TransportTest, TestSetRemoteIceLiteInAnswer) {
  transport_->SetIceRole(cricket::ICEROLE_CONTROLLING);
  cricket::TransportDescription local_desc(kIceUfrag1, kIcePwd1);
  ASSERT_TRUE(transport_->SetLocalTransportDescription(local_desc,
                                                       cricket::CA_OFFER,
                                                       NULL));
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, transport_->ice_role());
  EXPECT_TRUE(SetupChannel());
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, channel_->GetIceRole());
  // Channels will be created in ICEFULL_MODE.
  EXPECT_EQ(cricket::ICEMODE_FULL, channel_->remote_ice_mode());
  cricket::TransportDescription remote_desc(
      std::vector<std::string>(), kIceUfrag1, kIcePwd1, cricket::ICEMODE_LITE,
      cricket::CONNECTIONROLE_NONE, NULL);
  ASSERT_TRUE(transport_->SetRemoteTransportDescription(remote_desc,
                                                        cricket::CA_ANSWER,
                                                        NULL));
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, channel_->GetIceRole());
  // After receiving remote description with ICEMODE_LITE, channel should
  // have mode set to ICEMODE_LITE.
  EXPECT_EQ(cricket::ICEMODE_LITE, channel_->remote_ice_mode());
}

TEST_F(TransportTest, TestGetStats) {
  EXPECT_TRUE(SetupChannel());
  cricket::TransportStats stats;
  EXPECT_TRUE(transport_->GetStats(&stats));
  // Note that this tests the behavior of a FakeTransportChannel.
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
TEST_F(TransportTest, TestVerifyCertificateFingerprint) {
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

// Tests that NegotiateRole sets the SSL role correctly.
TEST_F(TransportTest, TestNegotiateRole) {
  TransportDescription local_desc(kIceUfrag1, kIcePwd1);
  TransportDescription remote_desc(kIceUfrag2, kIcePwd2);

  struct NegotiateRoleParams {
    cricket::ConnectionRole local_role;
    cricket::ConnectionRole remote_role;
    cricket::ContentAction local_action;
    cricket::ContentAction remote_action;
  };

  rtc::SSLRole ssl_role;
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
    local_desc.connection_role = param.local_role;
    remote_desc.connection_role = param.remote_role;

    ASSERT_TRUE(transport_->SetRemoteTransportDescription(
        remote_desc, param.remote_action, nullptr));
    ASSERT_TRUE(transport_->SetLocalTransportDescription(
        local_desc, param.local_action, nullptr));
    EXPECT_TRUE(
        transport_->NegotiateRole(param.local_action, &ssl_role, &error_desc));
    EXPECT_EQ(rtc::SSL_CLIENT, ssl_role);
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
    local_desc.connection_role = param.local_role;
    remote_desc.connection_role = param.remote_role;

    ASSERT_TRUE(transport_->SetRemoteTransportDescription(
        remote_desc, param.remote_action, nullptr));
    ASSERT_TRUE(transport_->SetLocalTransportDescription(
        local_desc, param.local_action, nullptr));
    EXPECT_TRUE(
        transport_->NegotiateRole(param.local_action, &ssl_role, &error_desc));
    EXPECT_EQ(rtc::SSL_SERVER, ssl_role);
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
    local_desc.connection_role = param.local_role;
    remote_desc.connection_role = param.remote_role;

    ASSERT_TRUE(transport_->SetRemoteTransportDescription(
        remote_desc, param.remote_action, nullptr));
    ASSERT_TRUE(transport_->SetLocalTransportDescription(
        local_desc, param.local_action, nullptr));
    EXPECT_FALSE(
        transport_->NegotiateRole(param.local_action, &ssl_role, &error_desc));
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
    local_desc.connection_role = param.local_role;
    remote_desc.connection_role = param.remote_role;

    ASSERT_TRUE(transport_->SetRemoteTransportDescription(
        remote_desc, param.remote_action, nullptr));
    ASSERT_TRUE(transport_->SetLocalTransportDescription(
        local_desc, param.local_action, nullptr));
    EXPECT_FALSE(
        transport_->NegotiateRole(param.local_action, &ssl_role, &error_desc));
  }
}
