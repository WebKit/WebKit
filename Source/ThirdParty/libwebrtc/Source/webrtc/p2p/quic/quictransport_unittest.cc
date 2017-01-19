/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/quic/quictransport.h"

#include <memory>
#include <string>
#include <vector>

#include "webrtc/base/gunit.h"
#include "webrtc/base/rtccertificate.h"
#include "webrtc/base/sslidentity.h"

using cricket::TransportChannelImpl;
using cricket::QuicTransport;
using cricket::Transport;
using cricket::TransportDescription;

static const char kIceUfrag1[] = "TESTICEUFRAG0001";
static const char kIcePwd1[] = "TESTICEPWD00000000000001";

static const char kIceUfrag2[] = "TESTICEUFRAG0002";
static const char kIcePwd2[] = "TESTICEPWD00000000000002";

static rtc::scoped_refptr<rtc::RTCCertificate> CreateCertificate(
    std::string name) {
  return rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
      rtc::SSLIdentity::Generate(name, rtc::KT_DEFAULT)));
}

static std::unique_ptr<rtc::SSLFingerprint> CreateFingerprint(
    rtc::RTCCertificate* cert) {
  std::string digest_algorithm;
  cert->ssl_certificate().GetSignatureDigestAlgorithm(&digest_algorithm);
  return std::unique_ptr<rtc::SSLFingerprint>(
      rtc::SSLFingerprint::Create(digest_algorithm, cert->identity()));
}

class QuicTransportTest : public testing::Test {
 public:
  QuicTransportTest() : transport_("testing", nullptr, nullptr) {}

  void SetTransportDescription(cricket::ConnectionRole local_role,
                               cricket::ConnectionRole remote_role,
                               cricket::ContentAction local_action,
                               cricket::ContentAction remote_action,
                               rtc::SSLRole expected_ssl_role) {
    TransportChannelImpl* channel = transport_.CreateChannel(1);
    ASSERT_NE(nullptr, channel);

    rtc::scoped_refptr<rtc::RTCCertificate> local_certificate(
        CreateCertificate("local"));
    ASSERT_NE(nullptr, local_certificate);
    transport_.SetLocalCertificate(local_certificate);

    std::unique_ptr<rtc::SSLFingerprint> local_fingerprint =
        CreateFingerprint(local_certificate.get());
    ASSERT_NE(nullptr, local_fingerprint);
    TransportDescription local_desc(std::vector<std::string>(), kIceUfrag1,
                                    kIcePwd1, cricket::ICEMODE_FULL, local_role,
                                    local_fingerprint.get());
    ASSERT_TRUE(transport_.SetLocalTransportDescription(local_desc,
                                                        local_action, nullptr));
    // The certificate is applied to QuicTransportChannel when the local
    // description is set.
    rtc::scoped_refptr<rtc::RTCCertificate> channel_local_certificate =
        channel->GetLocalCertificate();
    ASSERT_NE(nullptr, channel_local_certificate);
    EXPECT_EQ(local_certificate, channel_local_certificate);
    std::unique_ptr<rtc::SSLFingerprint> remote_fingerprint =
        CreateFingerprint(CreateCertificate("remote").get());
    // NegotiateTransportDescription was not called yet. The SSL role should
    // not be set and neither should the remote fingerprint.
    std::unique_ptr<rtc::SSLRole> role(new rtc::SSLRole());
    EXPECT_FALSE(channel->GetSslRole(role.get()));
    // Setting the remote description should set the SSL role.
    ASSERT_NE(nullptr, remote_fingerprint);
    TransportDescription remote_desc(std::vector<std::string>(), kIceUfrag2,
                                     kIcePwd2, cricket::ICEMODE_FULL,
                                     remote_role, remote_fingerprint.get());
    ASSERT_TRUE(transport_.SetRemoteTransportDescription(
        remote_desc, remote_action, nullptr));
    ASSERT_TRUE(channel->GetSslRole(role.get()));
    // SSL role should be client because the remote description is an ANSWER.
    EXPECT_EQ(expected_ssl_role, *role);
  }

 protected:
  QuicTransport transport_;
};

// Test setting the local certificate.
TEST_F(QuicTransportTest, SetLocalCertificate) {
  rtc::scoped_refptr<rtc::RTCCertificate> local_certificate(
      CreateCertificate("local"));
  ASSERT_NE(nullptr, local_certificate);
  rtc::scoped_refptr<rtc::RTCCertificate> transport_local_certificate;
  EXPECT_FALSE(transport_.GetLocalCertificate(&transport_local_certificate));
  transport_.SetLocalCertificate(local_certificate);
  ASSERT_TRUE(transport_.GetLocalCertificate(&transport_local_certificate));
  ASSERT_NE(nullptr, transport_local_certificate);
  EXPECT_EQ(local_certificate, transport_local_certificate);
}

// Test setting the ICE role.
TEST_F(QuicTransportTest, SetIceRole) {
  TransportChannelImpl* channel1 = transport_.CreateChannel(1);
  ASSERT_NE(nullptr, channel1);
  transport_.SetIceRole(cricket::ICEROLE_CONTROLLING);
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, transport_.ice_role());
  TransportChannelImpl* channel2 = transport_.CreateChannel(2);
  ASSERT_NE(nullptr, channel2);
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, channel1->GetIceRole());
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, channel2->GetIceRole());
}

// Test setting the ICE tie breaker.
TEST_F(QuicTransportTest, SetIceTiebreaker) {
  transport_.SetIceTiebreaker(1u);
  EXPECT_EQ(1u, transport_.IceTiebreaker());
}

// Test setting the local and remote descriptions for a SSL client.
TEST_F(QuicTransportTest, SetLocalAndRemoteTransportDescriptionClient) {
  SetTransportDescription(cricket::CONNECTIONROLE_ACTPASS,
                          cricket::CONNECTIONROLE_PASSIVE, cricket::CA_OFFER,
                          cricket::CA_ANSWER, rtc::SSL_CLIENT);
}

// Test setting the local and remote descriptions for a SSL server.
TEST_F(QuicTransportTest, SetLocalAndRemoteTransportDescriptionServer) {
  SetTransportDescription(cricket::CONNECTIONROLE_ACTPASS,
                          cricket::CONNECTIONROLE_ACTIVE, cricket::CA_OFFER,
                          cricket::CA_ANSWER, rtc::SSL_SERVER);
}

// Test creation and destruction of channels.
TEST_F(QuicTransportTest, CreateAndDestroyChannels) {
  TransportChannelImpl* channel1 = transport_.CreateChannel(1);
  ASSERT_NE(nullptr, channel1);
  EXPECT_TRUE(transport_.HasChannel(1));
  EXPECT_EQ(channel1, transport_.GetChannel(1));
  TransportChannelImpl* channel2 = transport_.CreateChannel(2);
  ASSERT_NE(nullptr, channel2);
  EXPECT_TRUE(transport_.HasChannel(2));
  EXPECT_EQ(channel2, transport_.GetChannel(2));
  transport_.DestroyChannel(1);
  EXPECT_FALSE(transport_.HasChannel(1));
  EXPECT_EQ(nullptr, transport_.GetChannel(1));
  transport_.DestroyChannel(2);
  EXPECT_FALSE(transport_.HasChannel(2));
  EXPECT_EQ(nullptr, transport_.GetChannel(2));
}
