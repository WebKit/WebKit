/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <map>
#include <memory>

#include "p2p/base/dtlstransport.h"
#include "p2p/base/fakeportallocator.h"
#include "p2p/base/p2ptransportchannel.h"
#include "p2p/base/portallocator.h"
#include "pc/test/faketransportcontroller.h"
#include "pc/transportcontroller.h"
#include "rtc_base/fakesslidentity.h"
#include "rtc_base/gunit.h"
#include "rtc_base/helpers.h"
#include "rtc_base/sslidentity.h"
#include "rtc_base/thread.h"

static const int kTimeout = 100;
static const char kIceUfrag1[] = "TESTICEUFRAG0001";
static const char kIcePwd1[] = "TESTICEPWD00000000000001";
static const char kIceUfrag2[] = "TESTICEUFRAG0002";
static const char kIcePwd2[] = "TESTICEPWD00000000000002";
static const char kIceUfrag3[] = "TESTICEUFRAG0003";
static const char kIcePwd3[] = "TESTICEPWD00000000000003";

namespace cricket {

// Only subclassing from FakeTransportController because currently that's the
// only way to have a TransportController with fake ICE/DTLS transports.
//
// TODO(deadbeef): Pass a "TransportFactory" or something similar into
// TransportController, instead of using inheritance in this way for testing.
typedef FakeTransportController TransportControllerForTest;

class TransportControllerTest : public testing::Test,
                                public sigslot::has_slots<> {
 public:
  TransportControllerTest()
      : transport_controller_(new TransportControllerForTest()),
        signaling_thread_(rtc::Thread::Current()) {
    ConnectTransportControllerSignals();
  }

  void CreateTransportControllerWithNetworkThread() {
    if (!network_thread_) {
      network_thread_ = rtc::Thread::CreateWithSocketServer();
      network_thread_->Start();
    }
    transport_controller_.reset(
        new TransportControllerForTest(network_thread_.get()));
    ConnectTransportControllerSignals();
  }

  void ConnectTransportControllerSignals() {
    transport_controller_->SignalConnectionState.connect(
        this, &TransportControllerTest::OnConnectionState);
    transport_controller_->SignalReceiving.connect(
        this, &TransportControllerTest::OnReceiving);
    transport_controller_->SignalGatheringState.connect(
        this, &TransportControllerTest::OnGatheringState);
    transport_controller_->SignalCandidatesGathered.connect(
        this, &TransportControllerTest::OnCandidatesGathered);
  }

  FakeDtlsTransport* CreateFakeDtlsTransport(const std::string& content,
                                             int component) {
    DtlsTransportInternal* transport =
        transport_controller_->CreateDtlsTransport_n(content, component);
    return static_cast<FakeDtlsTransport*>(transport);
  }

  void DestroyFakeDtlsTransport(const std::string& content, int component) {
    transport_controller_->DestroyDtlsTransport_n(content, component);
  }

  Candidate CreateCandidate(int component) {
    Candidate c;
    c.set_address(rtc::SocketAddress("192.168.1.1", 8000));
    c.set_component(1);
    c.set_protocol(UDP_PROTOCOL_NAME);
    c.set_priority(1);
    return c;
  }

  // Used for thread hopping test.
  void CreateFakeDtlsTransportsAndCompleteConnectionOnNetworkThread() {
    network_thread_->Invoke<void>(
        RTC_FROM_HERE,
        rtc::Bind(&TransportControllerTest::
                      CreateFakeDtlsTransportsAndCompleteConnection_w,
                  this));
  }

  void CreateFakeDtlsTransportsAndCompleteConnection_w() {
    transport_controller_->SetIceRole(ICEROLE_CONTROLLING);
    FakeDtlsTransport* transport1 = CreateFakeDtlsTransport("audio", 1);
    ASSERT_NE(nullptr, transport1);
    FakeDtlsTransport* transport2 = CreateFakeDtlsTransport("video", 1);
    ASSERT_NE(nullptr, transport2);

    TransportDescription local_desc(std::vector<std::string>(), kIceUfrag1,
                                    kIcePwd1, ICEMODE_FULL,
                                    CONNECTIONROLE_ACTPASS, nullptr);
    std::string err;
    transport_controller_->SetLocalTransportDescription("audio", local_desc,
                                                        CA_OFFER, &err);
    transport_controller_->SetLocalTransportDescription("video", local_desc,
                                                        CA_OFFER, &err);
    transport_controller_->MaybeStartGathering();
    transport1->fake_ice_transport()->SignalCandidateGathered(
        transport1->fake_ice_transport(), CreateCandidate(1));
    transport2->fake_ice_transport()->SignalCandidateGathered(
        transport2->fake_ice_transport(), CreateCandidate(1));
    transport1->fake_ice_transport()->SetCandidatesGatheringComplete();
    transport2->fake_ice_transport()->SetCandidatesGatheringComplete();
    transport1->fake_ice_transport()->SetConnectionCount(2);
    transport2->fake_ice_transport()->SetConnectionCount(2);
    transport1->SetReceiving(true);
    transport2->SetReceiving(true);
    transport1->SetWritable(true);
    transport2->SetWritable(true);
    transport1->fake_ice_transport()->SetConnectionCount(1);
    transport2->fake_ice_transport()->SetConnectionCount(1);
  }

  IceConfig CreateIceConfig(
      int receiving_timeout,
      ContinualGatheringPolicy continual_gathering_policy) {
    IceConfig config;
    config.receiving_timeout = receiving_timeout;
    config.continual_gathering_policy = continual_gathering_policy;
    return config;
  }

 protected:
  void OnConnectionState(IceConnectionState state) {
    if (!signaling_thread_->IsCurrent()) {
      signaled_on_non_signaling_thread_ = true;
    }
    connection_state_ = state;
    ++connection_state_signal_count_;
  }

  void OnReceiving(bool receiving) {
    if (!signaling_thread_->IsCurrent()) {
      signaled_on_non_signaling_thread_ = true;
    }
    receiving_ = receiving;
    ++receiving_signal_count_;
  }

  void OnGatheringState(IceGatheringState state) {
    if (!signaling_thread_->IsCurrent()) {
      signaled_on_non_signaling_thread_ = true;
    }
    gathering_state_ = state;
    ++gathering_state_signal_count_;
  }

  void OnCandidatesGathered(const std::string& transport_name,
                            const Candidates& candidates) {
    if (!signaling_thread_->IsCurrent()) {
      signaled_on_non_signaling_thread_ = true;
    }
    candidates_[transport_name].insert(candidates_[transport_name].end(),
                                       candidates.begin(), candidates.end());
    ++candidates_signal_count_;
  }

  std::unique_ptr<rtc::Thread> network_thread_;  // Not used for most tests.
  std::unique_ptr<TransportControllerForTest> transport_controller_;

  // Information received from signals from transport controller.
  IceConnectionState connection_state_ = kIceConnectionConnecting;
  bool receiving_ = false;
  IceGatheringState gathering_state_ = kIceGatheringNew;
  // transport_name => candidates
  std::map<std::string, Candidates> candidates_;
  // Counts of each signal emitted.
  int connection_state_signal_count_ = 0;
  int receiving_signal_count_ = 0;
  int gathering_state_signal_count_ = 0;
  int candidates_signal_count_ = 0;

  // Used to make sure signals only come on signaling thread.
  rtc::Thread* const signaling_thread_ = nullptr;
  bool signaled_on_non_signaling_thread_ = false;
};

TEST_F(TransportControllerTest, TestSetIceConfig) {
  FakeDtlsTransport* transport1 = CreateFakeDtlsTransport("audio", 1);
  ASSERT_NE(nullptr, transport1);

  transport_controller_->SetIceConfig(
      CreateIceConfig(1000, GATHER_CONTINUALLY));
  EXPECT_EQ(1000, transport1->fake_ice_transport()->receiving_timeout());
  EXPECT_TRUE(transport1->fake_ice_transport()->gather_continually());

  transport_controller_->SetIceConfig(
      CreateIceConfig(1000, GATHER_CONTINUALLY_AND_RECOVER));
  // Test that value stored in controller is applied to new transports.
  FakeDtlsTransport* transport2 = CreateFakeDtlsTransport("video", 1);
  ASSERT_NE(nullptr, transport2);
  EXPECT_EQ(1000, transport2->fake_ice_transport()->receiving_timeout());
  EXPECT_TRUE(transport2->fake_ice_transport()->gather_continually());
}

TEST_F(TransportControllerTest, TestSetSslMaxProtocolVersion) {
  EXPECT_TRUE(transport_controller_->SetSslMaxProtocolVersion(
      rtc::SSL_PROTOCOL_DTLS_12));
  FakeDtlsTransport* transport = CreateFakeDtlsTransport("audio", 1);

  ASSERT_NE(nullptr, transport);
  EXPECT_EQ(rtc::SSL_PROTOCOL_DTLS_12, transport->ssl_max_protocol_version());

  // Setting max version after transport is created should fail.
  EXPECT_FALSE(transport_controller_->SetSslMaxProtocolVersion(
      rtc::SSL_PROTOCOL_DTLS_10));
}

TEST_F(TransportControllerTest, TestSetIceRole) {
  FakeDtlsTransport* transport1 = CreateFakeDtlsTransport("audio", 1);
  ASSERT_NE(nullptr, transport1);

  transport_controller_->SetIceRole(ICEROLE_CONTROLLING);
  EXPECT_EQ(ICEROLE_CONTROLLING,
            transport1->fake_ice_transport()->GetIceRole());
  transport_controller_->SetIceRole(ICEROLE_CONTROLLED);
  EXPECT_EQ(ICEROLE_CONTROLLED, transport1->fake_ice_transport()->GetIceRole());

  // Test that value stored in controller is applied to new transports.
  FakeDtlsTransport* transport2 = CreateFakeDtlsTransport("video", 1);
  ASSERT_NE(nullptr, transport2);
  EXPECT_EQ(ICEROLE_CONTROLLED, transport2->fake_ice_transport()->GetIceRole());
}

// Test that when one transport encounters a role conflict, the ICE role is
// swapped on every transport.
TEST_F(TransportControllerTest, TestIceRoleConflict) {
  FakeDtlsTransport* transport1 = CreateFakeDtlsTransport("audio", 1);
  ASSERT_NE(nullptr, transport1);
  FakeDtlsTransport* transport2 = CreateFakeDtlsTransport("video", 1);
  ASSERT_NE(nullptr, transport2);

  transport_controller_->SetIceRole(ICEROLE_CONTROLLING);
  EXPECT_EQ(ICEROLE_CONTROLLING,
            transport1->fake_ice_transport()->GetIceRole());
  EXPECT_EQ(ICEROLE_CONTROLLING,
            transport2->fake_ice_transport()->GetIceRole());

  transport1->fake_ice_transport()->SignalRoleConflict(
      transport1->fake_ice_transport());
  EXPECT_EQ(ICEROLE_CONTROLLED, transport1->fake_ice_transport()->GetIceRole());
  EXPECT_EQ(ICEROLE_CONTROLLED, transport2->fake_ice_transport()->GetIceRole());

  // Should be able to handle a second role conflict. The remote endpoint can
  // change its role/tie-breaker when it does an ICE restart.
  transport2->fake_ice_transport()->SignalRoleConflict(
      transport2->fake_ice_transport());
  EXPECT_EQ(ICEROLE_CONTROLLING,
            transport1->fake_ice_transport()->GetIceRole());
  EXPECT_EQ(ICEROLE_CONTROLLING,
            transport2->fake_ice_transport()->GetIceRole());
}

TEST_F(TransportControllerTest, TestGetSslRole) {
  rtc::SSLRole role;
  CreateFakeDtlsTransport("audio", 1);

  // Should return false before role has been negotiated.
  EXPECT_FALSE(transport_controller_->GetSslRole("audio", &role));

  // To negotiate an SSL role, need to set a local certificate, and
  // local/remote transport descriptions with DTLS info.
  rtc::scoped_refptr<rtc::RTCCertificate> certificate =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("testing", rtc::KT_ECDSA)));
  std::unique_ptr<rtc::SSLFingerprint> fingerprint(
      rtc::SSLFingerprint::CreateFromCertificate(certificate));
  transport_controller_->SetLocalCertificate(certificate);

  // Set the same fingerprint on both sides since the remote fingerprint
  // doesn't really matter for this test.
  TransportDescription local_desc(std::vector<std::string>(), kIceUfrag1,
                                  kIcePwd1, ICEMODE_FULL,
                                  CONNECTIONROLE_ACTPASS, fingerprint.get());
  TransportDescription remote_desc(std::vector<std::string>(), kIceUfrag2,
                                   kIcePwd2, ICEMODE_FULL,
                                   CONNECTIONROLE_ACTIVE, fingerprint.get());
  std::string err;
  EXPECT_TRUE(transport_controller_->SetLocalTransportDescription(
      "audio", local_desc, cricket::CA_OFFER, &err));
  EXPECT_TRUE(transport_controller_->SetRemoteTransportDescription(
      "audio", remote_desc, cricket::CA_ANSWER, &err));

  // Finally we can get the role. Should be "server" since the remote
  // endpoint's role was "active".
  EXPECT_TRUE(transport_controller_->GetSslRole("audio", &role));
  EXPECT_EQ(rtc::SSL_SERVER, role);

  // Lastly, test that GetSslRole returns false for a nonexistent transport.
  EXPECT_FALSE(transport_controller_->GetSslRole("video", &role));
}

TEST_F(TransportControllerTest, TestSetAndGetLocalCertificate) {
  rtc::scoped_refptr<rtc::RTCCertificate> certificate1 =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("session1", rtc::KT_DEFAULT)));
  rtc::scoped_refptr<rtc::RTCCertificate> certificate2 =
      rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
          rtc::SSLIdentity::Generate("session2", rtc::KT_DEFAULT)));
  rtc::scoped_refptr<rtc::RTCCertificate> returned_certificate;

  FakeDtlsTransport* transport1 = CreateFakeDtlsTransport("audio", 1);
  ASSERT_NE(nullptr, transport1);

  EXPECT_TRUE(transport_controller_->SetLocalCertificate(certificate1));
  EXPECT_TRUE(transport_controller_->GetLocalCertificate(
      "audio", &returned_certificate));
  EXPECT_EQ(certificate1->identity()->certificate().ToPEMString(),
            returned_certificate->identity()->certificate().ToPEMString());

  // Should fail if called for a nonexistant transport.
  EXPECT_FALSE(transport_controller_->GetLocalCertificate(
      "video", &returned_certificate));

  // Test that identity stored in controller is applied to new transports.
  FakeDtlsTransport* transport2 = CreateFakeDtlsTransport("video", 1);
  ASSERT_NE(nullptr, transport2);
  EXPECT_TRUE(transport_controller_->GetLocalCertificate(
      "video", &returned_certificate));
  EXPECT_EQ(certificate1->identity()->certificate().ToPEMString(),
            returned_certificate->identity()->certificate().ToPEMString());

  // Shouldn't be able to change the identity once set.
  EXPECT_FALSE(transport_controller_->SetLocalCertificate(certificate2));
}

TEST_F(TransportControllerTest, TestGetRemoteSSLCertificate) {
  rtc::FakeSSLCertificate fake_certificate("fake_data");

  FakeDtlsTransport* transport = CreateFakeDtlsTransport("audio", 1);
  ASSERT_NE(nullptr, transport);

  transport->SetRemoteSSLCertificate(&fake_certificate);
  std::unique_ptr<rtc::SSLCertificate> returned_certificate =
      transport_controller_->GetRemoteSSLCertificate("audio");
  EXPECT_TRUE(returned_certificate);
  EXPECT_EQ(fake_certificate.ToPEMString(),
            returned_certificate->ToPEMString());

  // Should fail if called for a nonexistant transport.
  EXPECT_FALSE(transport_controller_->GetRemoteSSLCertificate("video"));
}

TEST_F(TransportControllerTest, TestSetLocalTransportDescription) {
  FakeDtlsTransport* transport = CreateFakeDtlsTransport("audio", 1);
  ASSERT_NE(nullptr, transport);
  TransportDescription local_desc(std::vector<std::string>(), kIceUfrag1,
                                  kIcePwd1, ICEMODE_FULL,
                                  CONNECTIONROLE_ACTPASS, nullptr);
  std::string err;
  EXPECT_TRUE(transport_controller_->SetLocalTransportDescription(
      "audio", local_desc, CA_OFFER, &err));
  // Check that ICE ufrag and pwd were propagated to transport.
  EXPECT_EQ(kIceUfrag1, transport->fake_ice_transport()->ice_ufrag());
  EXPECT_EQ(kIcePwd1, transport->fake_ice_transport()->ice_pwd());
  // After setting local description, we should be able to start gathering
  // candidates.
  transport_controller_->MaybeStartGathering();
  EXPECT_EQ_WAIT(kIceGatheringGathering, gathering_state_, kTimeout);
  EXPECT_EQ(1, gathering_state_signal_count_);
}

TEST_F(TransportControllerTest, TestSetRemoteTransportDescription) {
  FakeDtlsTransport* transport = CreateFakeDtlsTransport("audio", 1);
  ASSERT_NE(nullptr, transport);
  TransportDescription remote_desc(std::vector<std::string>(), kIceUfrag1,
                                   kIcePwd1, ICEMODE_FULL,
                                   CONNECTIONROLE_ACTPASS, nullptr);
  std::string err;
  EXPECT_TRUE(transport_controller_->SetRemoteTransportDescription(
      "audio", remote_desc, CA_OFFER, &err));
  // Check that ICE ufrag and pwd were propagated to transport.
  EXPECT_EQ(kIceUfrag1, transport->fake_ice_transport()->remote_ice_ufrag());
  EXPECT_EQ(kIcePwd1, transport->fake_ice_transport()->remote_ice_pwd());
}

TEST_F(TransportControllerTest, TestAddRemoteCandidates) {
  FakeDtlsTransport* transport = CreateFakeDtlsTransport("audio", 1);
  ASSERT_NE(nullptr, transport);
  Candidates candidates;
  candidates.push_back(CreateCandidate(1));
  std::string err;
  EXPECT_TRUE(
      transport_controller_->AddRemoteCandidates("audio", candidates, &err));
  EXPECT_EQ(1U, transport->fake_ice_transport()->remote_candidates().size());
}

TEST_F(TransportControllerTest, TestReadyForRemoteCandidates) {
  FakeDtlsTransport* transport = CreateFakeDtlsTransport("audio", 1);
  ASSERT_NE(nullptr, transport);
  // We expect to be ready for remote candidates only after local and remote
  // descriptions are set.
  EXPECT_FALSE(transport_controller_->ReadyForRemoteCandidates("audio"));

  std::string err;
  TransportDescription remote_desc(std::vector<std::string>(), kIceUfrag1,
                                   kIcePwd1, ICEMODE_FULL,
                                   CONNECTIONROLE_ACTPASS, nullptr);
  EXPECT_TRUE(transport_controller_->SetRemoteTransportDescription(
      "audio", remote_desc, CA_OFFER, &err));
  EXPECT_FALSE(transport_controller_->ReadyForRemoteCandidates("audio"));

  TransportDescription local_desc(std::vector<std::string>(), kIceUfrag2,
                                  kIcePwd2, ICEMODE_FULL,
                                  CONNECTIONROLE_ACTPASS, nullptr);
  EXPECT_TRUE(transport_controller_->SetLocalTransportDescription(
      "audio", local_desc, CA_ANSWER, &err));
  EXPECT_TRUE(transport_controller_->ReadyForRemoteCandidates("audio"));
}

TEST_F(TransportControllerTest, TestGetStats) {
  FakeDtlsTransport* transport1 = CreateFakeDtlsTransport("audio", 1);
  ASSERT_NE(nullptr, transport1);
  FakeDtlsTransport* transport2 = CreateFakeDtlsTransport("audio", 2);
  ASSERT_NE(nullptr, transport2);
  FakeDtlsTransport* transport3 = CreateFakeDtlsTransport("video", 1);
  ASSERT_NE(nullptr, transport3);

  TransportStats stats;
  EXPECT_TRUE(transport_controller_->GetStats("audio", &stats));
  EXPECT_EQ("audio", stats.transport_name);
  EXPECT_EQ(2U, stats.channel_stats.size());
}

// Test that a "transport" from a stats perspective (combination of RTP/RTCP
// transports) goes away when all references to its transports are gone.
TEST_F(TransportControllerTest, TestCreateAndDestroyFakeDtlsTransport) {
  FakeDtlsTransport* transport1 = CreateFakeDtlsTransport("audio", 1);
  ASSERT_NE(nullptr, transport1);
  FakeDtlsTransport* transport2 = CreateFakeDtlsTransport("audio", 1);
  ASSERT_NE(nullptr, transport2);
  ASSERT_EQ(transport1, transport2);
  FakeDtlsTransport* transport3 = CreateFakeDtlsTransport("audio", 2);
  ASSERT_NE(nullptr, transport3);

  // Using GetStats to check if transport is destroyed from an outside class's
  // perspective.
  TransportStats stats;
  EXPECT_TRUE(transport_controller_->GetStats("audio", &stats));
  DestroyFakeDtlsTransport("audio", 2);
  DestroyFakeDtlsTransport("audio", 1);
  EXPECT_TRUE(transport_controller_->GetStats("audio", &stats));
  DestroyFakeDtlsTransport("audio", 1);
  EXPECT_FALSE(transport_controller_->GetStats("audio", &stats));
}

TEST_F(TransportControllerTest, TestSignalConnectionStateFailed) {
  // Need controlling ICE role to get in failed state.
  transport_controller_->SetIceRole(ICEROLE_CONTROLLING);
  FakeDtlsTransport* transport1 = CreateFakeDtlsTransport("audio", 1);
  ASSERT_NE(nullptr, transport1);
  FakeDtlsTransport* transport2 = CreateFakeDtlsTransport("video", 1);
  ASSERT_NE(nullptr, transport2);

  // Should signal "failed" if any transport failed; transport is considered
  // failed
  // if it previously had a connection but now has none, and gathering is
  // complete.
  transport1->fake_ice_transport()->SetCandidatesGatheringComplete();
  transport1->fake_ice_transport()->SetConnectionCount(1);
  transport1->fake_ice_transport()->SetConnectionCount(0);
  EXPECT_EQ_WAIT(kIceConnectionFailed, connection_state_, kTimeout);
  EXPECT_EQ(1, connection_state_signal_count_);
}

TEST_F(TransportControllerTest, TestSignalConnectionStateConnected) {
  transport_controller_->SetIceRole(ICEROLE_CONTROLLING);
  FakeDtlsTransport* transport1 = CreateFakeDtlsTransport("audio", 1);
  ASSERT_NE(nullptr, transport1);
  FakeDtlsTransport* transport2 = CreateFakeDtlsTransport("video", 1);
  ASSERT_NE(nullptr, transport2);
  FakeDtlsTransport* transport3 = CreateFakeDtlsTransport("video", 2);
  ASSERT_NE(nullptr, transport3);

  // First, have one transport connect, and another fail, to ensure that
  // the first transport connecting didn't trigger a "connected" state signal.
  // We should only get a signal when all are connected.
  transport1->fake_ice_transport()->SetConnectionCount(2);
  transport1->SetWritable(true);
  transport3->fake_ice_transport()->SetCandidatesGatheringComplete();
  transport3->fake_ice_transport()->SetConnectionCount(1);
  transport3->fake_ice_transport()->SetConnectionCount(0);
  EXPECT_EQ_WAIT(kIceConnectionFailed, connection_state_, kTimeout);
  // Signal count of 1 means that the only signal emitted was "failed".
  EXPECT_EQ(1, connection_state_signal_count_);

  // Destroy the failed transport to return to "connecting" state.
  DestroyFakeDtlsTransport("video", 2);
  EXPECT_EQ_WAIT(kIceConnectionConnecting, connection_state_, kTimeout);
  EXPECT_EQ(2, connection_state_signal_count_);

  // Make the remaining transport reach a connected state.
  transport2->fake_ice_transport()->SetConnectionCount(2);
  transport2->SetWritable(true);
  EXPECT_EQ_WAIT(kIceConnectionConnected, connection_state_, kTimeout);
  EXPECT_EQ(3, connection_state_signal_count_);
}

TEST_F(TransportControllerTest, TestSignalConnectionStateComplete) {
  transport_controller_->SetIceRole(ICEROLE_CONTROLLING);
  FakeDtlsTransport* transport1 = CreateFakeDtlsTransport("audio", 1);
  ASSERT_NE(nullptr, transport1);
  FakeDtlsTransport* transport2 = CreateFakeDtlsTransport("video", 1);
  ASSERT_NE(nullptr, transport2);
  FakeDtlsTransport* transport3 = CreateFakeDtlsTransport("video", 2);
  ASSERT_NE(nullptr, transport3);

  // Similar to above test, but we're now reaching the completed state, which
  // means only one connection per FakeDtlsTransport.
  transport1->fake_ice_transport()->SetCandidatesGatheringComplete();
  transport1->fake_ice_transport()->SetConnectionCount(1);
  transport1->SetWritable(true);
  transport3->fake_ice_transport()->SetCandidatesGatheringComplete();
  transport3->fake_ice_transport()->SetConnectionCount(1);
  transport3->fake_ice_transport()->SetConnectionCount(0);
  EXPECT_EQ_WAIT(kIceConnectionFailed, connection_state_, kTimeout);
  // Signal count of 1 means that the only signal emitted was "failed".
  EXPECT_EQ(1, connection_state_signal_count_);

  // Destroy the failed transport to return to "connecting" state.
  DestroyFakeDtlsTransport("video", 2);
  EXPECT_EQ_WAIT(kIceConnectionConnecting, connection_state_, kTimeout);
  EXPECT_EQ(2, connection_state_signal_count_);

  // Make the remaining transport reach a connected state.
  transport2->fake_ice_transport()->SetCandidatesGatheringComplete();
  transport2->fake_ice_transport()->SetConnectionCount(2);
  transport2->SetWritable(true);
  EXPECT_EQ_WAIT(kIceConnectionConnected, connection_state_, kTimeout);
  EXPECT_EQ(3, connection_state_signal_count_);

  // Finally, transition to completed state.
  transport2->fake_ice_transport()->SetConnectionCount(1);
  EXPECT_EQ_WAIT(kIceConnectionCompleted, connection_state_, kTimeout);
  EXPECT_EQ(4, connection_state_signal_count_);
}

// Make sure that if we're "connected" and remove a transport, we stay in the
// "connected" state.
TEST_F(TransportControllerTest, TestDestroyTransportAndStayConnected) {
  FakeDtlsTransport* transport1 = CreateFakeDtlsTransport("audio", 1);
  ASSERT_NE(nullptr, transport1);
  FakeDtlsTransport* transport2 = CreateFakeDtlsTransport("video", 1);
  ASSERT_NE(nullptr, transport2);

  transport1->fake_ice_transport()->SetCandidatesGatheringComplete();
  transport1->fake_ice_transport()->SetConnectionCount(2);
  transport1->SetWritable(true);
  transport2->fake_ice_transport()->SetCandidatesGatheringComplete();
  transport2->fake_ice_transport()->SetConnectionCount(2);
  transport2->SetWritable(true);
  EXPECT_EQ_WAIT(kIceConnectionConnected, connection_state_, kTimeout);
  EXPECT_EQ(1, connection_state_signal_count_);

  // Destroy one transport, then "complete" the other one, so we reach
  // a known state.
  DestroyFakeDtlsTransport("video", 1);
  transport1->fake_ice_transport()->SetConnectionCount(1);
  EXPECT_EQ_WAIT(kIceConnectionCompleted, connection_state_, kTimeout);
  // Signal count of 2 means the deletion didn't cause any unexpected signals
  EXPECT_EQ(2, connection_state_signal_count_);
}

// If we destroy the last/only transport, we should simply transition to
// "connecting".
TEST_F(TransportControllerTest, TestDestroyLastTransportWhileConnected) {
  FakeDtlsTransport* transport = CreateFakeDtlsTransport("audio", 1);
  ASSERT_NE(nullptr, transport);

  transport->fake_ice_transport()->SetCandidatesGatheringComplete();
  transport->fake_ice_transport()->SetConnectionCount(2);
  transport->SetWritable(true);
  EXPECT_EQ_WAIT(kIceConnectionConnected, connection_state_, kTimeout);
  EXPECT_EQ(1, connection_state_signal_count_);

  DestroyFakeDtlsTransport("audio", 1);
  EXPECT_EQ_WAIT(kIceConnectionConnecting, connection_state_, kTimeout);
  // Signal count of 2 means the deletion didn't cause any unexpected signals
  EXPECT_EQ(2, connection_state_signal_count_);
}

TEST_F(TransportControllerTest, TestSignalReceiving) {
  FakeDtlsTransport* transport1 = CreateFakeDtlsTransport("audio", 1);
  ASSERT_NE(nullptr, transport1);
  FakeDtlsTransport* transport2 = CreateFakeDtlsTransport("video", 1);
  ASSERT_NE(nullptr, transport2);

  // Should signal receiving as soon as any transport is receiving.
  transport1->SetReceiving(true);
  EXPECT_TRUE_WAIT(receiving_, kTimeout);
  EXPECT_EQ(1, receiving_signal_count_);

  transport2->SetReceiving(true);
  transport1->SetReceiving(false);
  transport2->SetReceiving(false);
  EXPECT_TRUE_WAIT(!receiving_, kTimeout);
  EXPECT_EQ(2, receiving_signal_count_);
}

TEST_F(TransportControllerTest, TestSignalGatheringStateGathering) {
  FakeDtlsTransport* transport = CreateFakeDtlsTransport("audio", 1);
  ASSERT_NE(nullptr, transport);
  transport->fake_ice_transport()->MaybeStartGathering();
  // Should be in the gathering state as soon as any transport starts gathering.
  EXPECT_EQ_WAIT(kIceGatheringGathering, gathering_state_, kTimeout);
  EXPECT_EQ(1, gathering_state_signal_count_);
}

TEST_F(TransportControllerTest, TestSignalGatheringStateComplete) {
  FakeDtlsTransport* transport1 = CreateFakeDtlsTransport("audio", 1);
  ASSERT_NE(nullptr, transport1);
  FakeDtlsTransport* transport2 = CreateFakeDtlsTransport("video", 1);
  ASSERT_NE(nullptr, transport2);
  FakeDtlsTransport* transport3 = CreateFakeDtlsTransport("data", 1);
  ASSERT_NE(nullptr, transport3);

  transport3->fake_ice_transport()->MaybeStartGathering();
  EXPECT_EQ_WAIT(kIceGatheringGathering, gathering_state_, kTimeout);
  EXPECT_EQ(1, gathering_state_signal_count_);

  // Have one transport finish gathering, then destroy it, to make sure
  // gathering
  // completion wasn't signalled if only one transport finished gathering.
  transport3->fake_ice_transport()->SetCandidatesGatheringComplete();
  DestroyFakeDtlsTransport("data", 1);
  EXPECT_EQ_WAIT(kIceGatheringNew, gathering_state_, kTimeout);
  EXPECT_EQ(2, gathering_state_signal_count_);

  // Make remaining transports start and then finish gathering.
  transport1->fake_ice_transport()->MaybeStartGathering();
  transport2->fake_ice_transport()->MaybeStartGathering();
  EXPECT_EQ_WAIT(kIceGatheringGathering, gathering_state_, kTimeout);
  EXPECT_EQ(3, gathering_state_signal_count_);

  transport1->fake_ice_transport()->SetCandidatesGatheringComplete();
  transport2->fake_ice_transport()->SetCandidatesGatheringComplete();
  EXPECT_EQ_WAIT(kIceGatheringComplete, gathering_state_, kTimeout);
  EXPECT_EQ(4, gathering_state_signal_count_);
}

// Test that when the last transport that hasn't finished connecting and/or
// gathering is destroyed, the aggregate state jumps to "completed". This can
// happen if, for example, we have an audio and video transport, the audio
// transport completes, then we start bundling video on the audio transport.
TEST_F(TransportControllerTest,
       TestSignalingWhenLastIncompleteTransportDestroyed) {
  transport_controller_->SetIceRole(ICEROLE_CONTROLLING);
  FakeDtlsTransport* transport1 = CreateFakeDtlsTransport("audio", 1);
  ASSERT_NE(nullptr, transport1);
  FakeDtlsTransport* transport2 = CreateFakeDtlsTransport("video", 1);
  ASSERT_NE(nullptr, transport2);

  transport1->fake_ice_transport()->SetCandidatesGatheringComplete();
  EXPECT_EQ_WAIT(kIceGatheringGathering, gathering_state_, kTimeout);
  EXPECT_EQ(1, gathering_state_signal_count_);

  transport1->fake_ice_transport()->SetConnectionCount(1);
  transport1->SetWritable(true);
  DestroyFakeDtlsTransport("video", 1);
  EXPECT_EQ_WAIT(kIceConnectionCompleted, connection_state_, kTimeout);
  EXPECT_EQ(1, connection_state_signal_count_);
  EXPECT_EQ_WAIT(kIceGatheringComplete, gathering_state_, kTimeout);
  EXPECT_EQ(2, gathering_state_signal_count_);
}

TEST_F(TransportControllerTest, TestSignalCandidatesGathered) {
  FakeDtlsTransport* transport = CreateFakeDtlsTransport("audio", 1);
  ASSERT_NE(nullptr, transport);

  // Transport won't signal candidates until it has a local description.
  TransportDescription local_desc(std::vector<std::string>(), kIceUfrag1,
                                  kIcePwd1, ICEMODE_FULL,
                                  CONNECTIONROLE_ACTPASS, nullptr);
  std::string err;
  EXPECT_TRUE(transport_controller_->SetLocalTransportDescription(
      "audio", local_desc, CA_OFFER, &err));
  transport_controller_->MaybeStartGathering();

  transport->fake_ice_transport()->SignalCandidateGathered(
      transport->fake_ice_transport(), CreateCandidate(1));
  EXPECT_EQ_WAIT(1, candidates_signal_count_, kTimeout);
  EXPECT_EQ(1U, candidates_["audio"].size());
}

TEST_F(TransportControllerTest, TestSignalingOccursOnSignalingThread) {
  CreateTransportControllerWithNetworkThread();
  CreateFakeDtlsTransportsAndCompleteConnectionOnNetworkThread();

  // connecting --> connected --> completed
  EXPECT_EQ_WAIT(kIceConnectionCompleted, connection_state_, kTimeout);
  EXPECT_EQ(2, connection_state_signal_count_);

  EXPECT_TRUE_WAIT(receiving_, kTimeout);
  EXPECT_EQ(1, receiving_signal_count_);

  // new --> gathering --> complete
  EXPECT_EQ_WAIT(kIceGatheringComplete, gathering_state_, kTimeout);
  EXPECT_EQ(2, gathering_state_signal_count_);

  EXPECT_EQ_WAIT(1U, candidates_["audio"].size(), kTimeout);
  EXPECT_EQ_WAIT(1U, candidates_["video"].size(), kTimeout);
  EXPECT_EQ(2, candidates_signal_count_);

  EXPECT_TRUE(!signaled_on_non_signaling_thread_);
}

// Older versions of Chrome expect the ICE role to be re-determined when an
// ICE restart occurs, and also don't perform conflict resolution correctly,
// so for now we can't safely stop doing this.
// See: https://bugs.chromium.org/p/chromium/issues/detail?id=628676
// TODO(deadbeef): Remove this when these old versions of Chrome reach a low
// enough population.
TEST_F(TransportControllerTest, IceRoleRedeterminedOnIceRestartByDefault) {
  FakeDtlsTransport* transport = CreateFakeDtlsTransport("audio", 1);
  ASSERT_NE(nullptr, transport);
  std::string err;
  // Do an initial offer answer, so that the next offer is an ICE restart.
  transport_controller_->SetIceRole(ICEROLE_CONTROLLED);
  TransportDescription remote_desc(std::vector<std::string>(), kIceUfrag1,
                                   kIcePwd1, ICEMODE_FULL,
                                   CONNECTIONROLE_ACTPASS, nullptr);
  EXPECT_TRUE(transport_controller_->SetRemoteTransportDescription(
      "audio", remote_desc, CA_OFFER, &err));
  TransportDescription local_desc(std::vector<std::string>(), kIceUfrag2,
                                  kIcePwd2, ICEMODE_FULL,
                                  CONNECTIONROLE_ACTPASS, nullptr);
  EXPECT_TRUE(transport_controller_->SetLocalTransportDescription(
      "audio", local_desc, CA_ANSWER, &err));
  EXPECT_EQ(ICEROLE_CONTROLLED, transport->fake_ice_transport()->GetIceRole());

  // The endpoint that initiated an ICE restart should take the controlling
  // role.
  TransportDescription ice_restart_desc(std::vector<std::string>(), kIceUfrag3,
                                        kIcePwd3, ICEMODE_FULL,
                                        CONNECTIONROLE_ACTPASS, nullptr);
  EXPECT_TRUE(transport_controller_->SetLocalTransportDescription(
      "audio", ice_restart_desc, CA_OFFER, &err));
  EXPECT_EQ(ICEROLE_CONTROLLING, transport->fake_ice_transport()->GetIceRole());
}

// Test that if the TransportController was created with the
// |redetermine_role_on_ice_restart| parameter set to false, the role is *not*
// redetermined on an ICE restart.
TEST_F(TransportControllerTest, IceRoleNotRedetermined) {
  bool redetermine_role = false;
  transport_controller_.reset(new TransportControllerForTest(redetermine_role));
  FakeDtlsTransport* transport = CreateFakeDtlsTransport("audio", 1);
  ASSERT_NE(nullptr, transport);
  std::string err;
  // Do an initial offer answer, so that the next offer is an ICE restart.
  transport_controller_->SetIceRole(ICEROLE_CONTROLLED);
  TransportDescription remote_desc(std::vector<std::string>(), kIceUfrag1,
                                   kIcePwd1, ICEMODE_FULL,
                                   CONNECTIONROLE_ACTPASS, nullptr);
  EXPECT_TRUE(transport_controller_->SetRemoteTransportDescription(
      "audio", remote_desc, CA_OFFER, &err));
  TransportDescription local_desc(std::vector<std::string>(), kIceUfrag2,
                                  kIcePwd2, ICEMODE_FULL,
                                  CONNECTIONROLE_ACTPASS, nullptr);
  EXPECT_TRUE(transport_controller_->SetLocalTransportDescription(
      "audio", local_desc, CA_ANSWER, &err));
  EXPECT_EQ(ICEROLE_CONTROLLED, transport->fake_ice_transport()->GetIceRole());

  // The endpoint that initiated an ICE restart should keep the existing role.
  TransportDescription ice_restart_desc(std::vector<std::string>(), kIceUfrag3,
                                        kIcePwd3, ICEMODE_FULL,
                                        CONNECTIONROLE_ACTPASS, nullptr);
  EXPECT_TRUE(transport_controller_->SetLocalTransportDescription(
      "audio", ice_restart_desc, CA_OFFER, &err));
  EXPECT_EQ(ICEROLE_CONTROLLED, transport->fake_ice_transport()->GetIceRole());
}

// Tests ICE role is reversed after receiving ice-lite from remote.
TEST_F(TransportControllerTest, TestSetRemoteIceLiteInOffer) {
  FakeDtlsTransport* transport = CreateFakeDtlsTransport("audio", 1);
  ASSERT_NE(nullptr, transport);
  std::string err;

  transport_controller_->SetIceRole(ICEROLE_CONTROLLED);
  TransportDescription remote_desc(std::vector<std::string>(), kIceUfrag1,
                                   kIcePwd1, ICEMODE_LITE,
                                   CONNECTIONROLE_ACTPASS, nullptr);
  EXPECT_TRUE(transport_controller_->SetRemoteTransportDescription(
      "audio", remote_desc, CA_OFFER, &err));
  TransportDescription local_desc(kIceUfrag1, kIcePwd1);
  ASSERT_TRUE(transport_controller_->SetLocalTransportDescription(
      "audio", local_desc, CA_ANSWER, nullptr));

  EXPECT_EQ(ICEROLE_CONTROLLING, transport->fake_ice_transport()->GetIceRole());
  EXPECT_EQ(ICEMODE_LITE, transport->fake_ice_transport()->remote_ice_mode());
}

// Tests ice-lite in remote answer.
TEST_F(TransportControllerTest, TestSetRemoteIceLiteInAnswer) {
  FakeDtlsTransport* transport = CreateFakeDtlsTransport("audio", 1);
  ASSERT_NE(nullptr, transport);
  std::string err;

  transport_controller_->SetIceRole(ICEROLE_CONTROLLING);
  TransportDescription local_desc(kIceUfrag1, kIcePwd1);
  ASSERT_TRUE(transport_controller_->SetLocalTransportDescription(
      "audio", local_desc, CA_OFFER, nullptr));
  EXPECT_EQ(ICEROLE_CONTROLLING, transport->fake_ice_transport()->GetIceRole());
  // Transports will be created in ICEFULL_MODE.
  EXPECT_EQ(ICEMODE_FULL, transport->fake_ice_transport()->remote_ice_mode());
  TransportDescription remote_desc(std::vector<std::string>(), kIceUfrag1,
                                   kIcePwd1, ICEMODE_LITE, CONNECTIONROLE_NONE,
                                   nullptr);
  ASSERT_TRUE(transport_controller_->SetRemoteTransportDescription(
      "audio", remote_desc, CA_ANSWER, nullptr));
  EXPECT_EQ(ICEROLE_CONTROLLING, transport->fake_ice_transport()->GetIceRole());
  // After receiving remote description with ICEMODE_LITE, transport should
  // have mode set to ICEMODE_LITE.
  EXPECT_EQ(ICEMODE_LITE, transport->fake_ice_transport()->remote_ice_mode());
}

// Tests that the ICE role remains "controlling" if a subsequent offer that
// does an ICE restart is received from an ICE lite endpoint. Regression test
// for: https://crbug.com/710760
TEST_F(TransportControllerTest,
       IceRoleIsControllingAfterIceRestartFromIceLiteEndpoint) {
  FakeDtlsTransport* transport = CreateFakeDtlsTransport("audio", 1);
  ASSERT_NE(nullptr, transport);
  std::string err;

  // Initial offer/answer.
  TransportDescription remote_desc(std::vector<std::string>(), kIceUfrag1,
                                   kIcePwd1, ICEMODE_LITE,
                                   CONNECTIONROLE_ACTPASS, nullptr);
  TransportDescription local_desc(kIceUfrag1, kIcePwd1);
  ASSERT_TRUE(transport_controller_->SetRemoteTransportDescription(
      "audio", remote_desc, CA_OFFER, &err));
  ASSERT_TRUE(transport_controller_->SetLocalTransportDescription(
      "audio", local_desc, CA_ANSWER, nullptr));
  // Subsequent ICE restart offer/answer.
  remote_desc.ice_ufrag = kIceUfrag2;
  remote_desc.ice_pwd = kIcePwd2;
  local_desc.ice_ufrag = kIceUfrag2;
  local_desc.ice_pwd = kIcePwd2;
  ASSERT_TRUE(transport_controller_->SetRemoteTransportDescription(
      "audio", remote_desc, CA_OFFER, &err));
  ASSERT_TRUE(transport_controller_->SetLocalTransportDescription(
      "audio", local_desc, CA_ANSWER, nullptr));

  EXPECT_EQ(ICEROLE_CONTROLLING, transport->fake_ice_transport()->GetIceRole());
}

// Tests SetNeedsIceRestartFlag and NeedsIceRestart, setting the flag and then
// initiating an ICE restart for one of the transports.
TEST_F(TransportControllerTest, NeedsIceRestart) {
  CreateFakeDtlsTransport("audio", 1);
  CreateFakeDtlsTransport("video", 1);

  // Do initial offer/answer so there's something to restart.
  TransportDescription local_desc(kIceUfrag1, kIcePwd1);
  TransportDescription remote_desc(kIceUfrag1, kIcePwd1);
  ASSERT_TRUE(transport_controller_->SetLocalTransportDescription(
      "audio", local_desc, CA_OFFER, nullptr));
  ASSERT_TRUE(transport_controller_->SetLocalTransportDescription(
      "video", local_desc, CA_OFFER, nullptr));
  ASSERT_TRUE(transport_controller_->SetRemoteTransportDescription(
      "audio", remote_desc, CA_ANSWER, nullptr));
  ASSERT_TRUE(transport_controller_->SetRemoteTransportDescription(
      "video", remote_desc, CA_ANSWER, nullptr));

  // Initially NeedsIceRestart should return false.
  EXPECT_FALSE(transport_controller_->NeedsIceRestart("audio"));
  EXPECT_FALSE(transport_controller_->NeedsIceRestart("video"));

  // Set the needs-ice-restart flag and verify NeedsIceRestart starts returning
  // true.
  transport_controller_->SetNeedsIceRestartFlag();
  EXPECT_TRUE(transport_controller_->NeedsIceRestart("audio"));
  EXPECT_TRUE(transport_controller_->NeedsIceRestart("video"));
  // For a nonexistent transport, false should be returned.
  EXPECT_FALSE(transport_controller_->NeedsIceRestart("deadbeef"));

  // Do ICE restart but only for audio.
  TransportDescription ice_restart_local_desc(kIceUfrag2, kIcePwd2);
  ASSERT_TRUE(transport_controller_->SetLocalTransportDescription(
      "audio", ice_restart_local_desc, CA_OFFER, nullptr));
  ASSERT_TRUE(transport_controller_->SetLocalTransportDescription(
      "video", local_desc, CA_OFFER, nullptr));
  // NeedsIceRestart should still be true for video.
  EXPECT_FALSE(transport_controller_->NeedsIceRestart("audio"));
  EXPECT_TRUE(transport_controller_->NeedsIceRestart("video"));
}

}  // namespace cricket
