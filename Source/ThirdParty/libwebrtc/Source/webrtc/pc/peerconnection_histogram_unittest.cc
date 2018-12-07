/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <tuple>

#include "absl/memory/memory.h"
#include "api/jsep.h"
#include "api/peerconnectionproxy.h"
#include "media/base/fakemediaengine.h"
#include "p2p/client/basicportallocator.h"
#include "pc/mediasession.h"
#include "pc/peerconnection.h"
#include "pc/peerconnectionfactory.h"
#include "pc/peerconnectionwrapper.h"
#include "pc/sdputils.h"
#include "pc/test/fakesctptransport.h"
#include "rtc_base/fakenetwork.h"
#include "rtc_base/gunit.h"
#include "rtc_base/virtualsocketserver.h"
#include "system_wrappers/include/metrics.h"

namespace webrtc {

using RTCConfiguration = PeerConnectionInterface::RTCConfiguration;
using RTCOfferAnswerOptions = PeerConnectionInterface::RTCOfferAnswerOptions;
using ::testing::Values;

static const char kUsagePatternMetric[] = "WebRTC.PeerConnection.UsagePattern";
static constexpr int kDefaultTimeout = 10000;
static const rtc::SocketAddress kDefaultLocalAddress("1.1.1.1", 0);
static const rtc::SocketAddress kPrivateLocalAddress("10.1.1.1", 0);

int MakeUsageFingerprint(std::set<PeerConnection::UsageEvent> events) {
  int signature = 0;
  for (const auto it : events) {
    signature |= static_cast<int>(it);
  }
  return signature;
}

class PeerConnectionFactoryForUsageHistogramTest
    : public rtc::RefCountedObject<PeerConnectionFactory> {
 public:
  PeerConnectionFactoryForUsageHistogramTest()
      : rtc::RefCountedObject<PeerConnectionFactory>(
            rtc::Thread::Current(),
            rtc::Thread::Current(),
            rtc::Thread::Current(),
            absl::make_unique<cricket::FakeMediaEngine>(),
            CreateCallFactory(),
            nullptr) {}

  void ActionsBeforeInitializeForTesting(PeerConnectionInterface* pc) override {
    PeerConnection* internal_pc = static_cast<PeerConnection*>(pc);
    if (return_histogram_very_quickly_) {
      internal_pc->ReturnHistogramVeryQuicklyForTesting();
    }
  }

  void ReturnHistogramVeryQuickly() { return_histogram_very_quickly_ = true; }

 private:
  bool return_histogram_very_quickly_ = false;
};

class PeerConnectionWrapperForUsageHistogramTest;
typedef PeerConnectionWrapperForUsageHistogramTest* RawWrapperPtr;

class ObserverForUsageHistogramTest : public MockPeerConnectionObserver {
 public:
  void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override;

  void OnInterestingUsage(int usage_pattern) override {
    interesting_usage_detected_ = usage_pattern;
  }

  void PrepareToExchangeCandidates(RawWrapperPtr other) {
    candidate_target_ = other;
  }

  bool HaveDataChannel() { return last_datachannel_; }

  absl::optional<int> interesting_usage_detected() {
    return interesting_usage_detected_;
  }

  void ClearInterestingUsageDetector() {
    interesting_usage_detected_ = absl::optional<int>();
  }

 private:
  absl::optional<int> interesting_usage_detected_;
  RawWrapperPtr candidate_target_;  // Note: Not thread-safe against deletions.
};

class PeerConnectionWrapperForUsageHistogramTest
    : public PeerConnectionWrapper {
 public:
  using PeerConnectionWrapper::PeerConnectionWrapper;

  PeerConnection* GetInternalPeerConnection() {
    auto* pci =
        static_cast<PeerConnectionProxyWithInternal<PeerConnectionInterface>*>(
            pc());
    return static_cast<PeerConnection*>(pci->internal());
  }

  // Override with different return type
  ObserverForUsageHistogramTest* observer() {
    return static_cast<ObserverForUsageHistogramTest*>(
        PeerConnectionWrapper::observer());
  }

  void PrepareToExchangeCandidates(
      PeerConnectionWrapperForUsageHistogramTest* other) {
    observer()->PrepareToExchangeCandidates(other);
    other->observer()->PrepareToExchangeCandidates(this);
  }

  bool IsConnected() {
    return pc()->ice_connection_state() ==
               PeerConnectionInterface::kIceConnectionConnected ||
           pc()->ice_connection_state() ==
               PeerConnectionInterface::kIceConnectionCompleted;
  }

  bool HaveDataChannel() {
    return static_cast<ObserverForUsageHistogramTest*>(observer())
        ->HaveDataChannel();
  }
  void AddOrBufferIceCandidate(const webrtc::IceCandidateInterface* candidate) {
    if (!pc()->AddIceCandidate(candidate)) {
      std::string sdp;
      EXPECT_TRUE(candidate->ToString(&sdp));
      std::unique_ptr<webrtc::IceCandidateInterface> candidate_copy(
          CreateIceCandidate(candidate->sdp_mid(), candidate->sdp_mline_index(),
                             sdp, nullptr));
      buffered_candidates_.push_back(std::move(candidate_copy));
    }
  }

  void AddBufferedIceCandidates() {
    for (const auto& candidate : buffered_candidates_) {
      EXPECT_TRUE(pc()->AddIceCandidate(candidate.get()));
    }
    buffered_candidates_.clear();
  }

  bool ConnectTo(PeerConnectionWrapperForUsageHistogramTest* callee) {
    PrepareToExchangeCandidates(callee);
    if (!ExchangeOfferAnswerWith(callee)) {
      return false;
    }
    AddBufferedIceCandidates();
    callee->AddBufferedIceCandidates();
    WAIT(IsConnected(), kDefaultTimeout);
    WAIT(callee->IsConnected(), kDefaultTimeout);
    return IsConnected() && callee->IsConnected();
  }

  bool GenerateOfferAndCollectCandidates() {
    auto offer = CreateOffer(RTCOfferAnswerOptions());
    if (!offer) {
      return false;
    }
    bool set_local_offer =
        SetLocalDescription(CloneSessionDescription(offer.get()));
    EXPECT_TRUE(set_local_offer);
    if (!set_local_offer) {
      return false;
    }
    EXPECT_TRUE_WAIT(observer()->ice_gathering_complete_, kDefaultTimeout);
    return true;
  }

 private:
  // Candidates that have been sent but not yet configured
  std::vector<std::unique_ptr<webrtc::IceCandidateInterface>>
      buffered_candidates_;
};

void ObserverForUsageHistogramTest::OnIceCandidate(
    const webrtc::IceCandidateInterface* candidate) {
  if (candidate_target_) {
    this->candidate_target_->AddOrBufferIceCandidate(candidate);
  }
  // If target is not set, ignore. This happens in one-ended unit tests.
}

class PeerConnectionUsageHistogramTest : public ::testing::Test {
 protected:
  typedef std::unique_ptr<PeerConnectionWrapperForUsageHistogramTest>
      WrapperPtr;

  PeerConnectionUsageHistogramTest()
      : vss_(new rtc::VirtualSocketServer()), main_(vss_.get()) {
    webrtc::metrics::Reset();
  }

  WrapperPtr CreatePeerConnection() {
    return CreatePeerConnection(RTCConfiguration(),
                                PeerConnectionFactoryInterface::Options(),
                                nullptr, false);
  }

  WrapperPtr CreatePeerConnection(const RTCConfiguration& config) {
    return CreatePeerConnection(
        config, PeerConnectionFactoryInterface::Options(), nullptr, false);
  }

  WrapperPtr CreatePeerConnectionWithImmediateReport() {
    return CreatePeerConnection(RTCConfiguration(),
                                PeerConnectionFactoryInterface::Options(),
                                nullptr, true);
  }

  WrapperPtr CreatePeerConnectionWithPrivateLocalAddresses() {
    fake_network_manager_.reset(new rtc::FakeNetworkManager());
    fake_network_manager_->AddInterface(kDefaultLocalAddress);
    fake_network_manager_->AddInterface(kPrivateLocalAddress);
    std::unique_ptr<cricket::BasicPortAllocator> port_allocator(
        new cricket::BasicPortAllocator(fake_network_manager_.get()));
    return CreatePeerConnection(RTCConfiguration(),
                                PeerConnectionFactoryInterface::Options(),
                                std::move(port_allocator), false);
  }

  WrapperPtr CreatePeerConnection(
      const RTCConfiguration& config,
      const PeerConnectionFactoryInterface::Options factory_options,
      std::unique_ptr<cricket::PortAllocator> allocator,
      bool immediate_report) {
    rtc::scoped_refptr<PeerConnectionFactoryForUsageHistogramTest> pc_factory(
        new PeerConnectionFactoryForUsageHistogramTest());
    pc_factory->SetOptions(factory_options);
    RTC_CHECK(pc_factory->Initialize());
    if (immediate_report) {
      pc_factory->ReturnHistogramVeryQuickly();
    }
    auto observer = absl::make_unique<ObserverForUsageHistogramTest>();
    auto pc = pc_factory->CreatePeerConnection(config, std::move(allocator),
                                               nullptr, observer.get());
    if (!pc) {
      return nullptr;
    }

    observer->SetPeerConnectionInterface(pc.get());
    auto wrapper =
        absl::make_unique<PeerConnectionWrapperForUsageHistogramTest>(
            pc_factory, pc, std::move(observer));
    return wrapper;
  }

  int ObservedFingerprint() {
    // This works correctly only if there is only one sample value
    // that has been counted.
    // Returns -1 for "not found".
    return webrtc::metrics::MinSample(kUsagePatternMetric);
  }

  std::unique_ptr<rtc::FakeNetworkManager> fake_network_manager_;
  std::unique_ptr<rtc::VirtualSocketServer> vss_;
  rtc::AutoSocketServerThread main_;
};

TEST_F(PeerConnectionUsageHistogramTest, UsageFingerprintHistogramFromTimeout) {
  auto pc = CreatePeerConnectionWithImmediateReport();

  int expected_fingerprint = MakeUsageFingerprint({});
  ASSERT_EQ_WAIT(1, webrtc::metrics::NumSamples(kUsagePatternMetric),
                 kDefaultTimeout);
  EXPECT_EQ(
      1, webrtc::metrics::NumEvents(kUsagePatternMetric, expected_fingerprint));
}

#ifndef WEBRTC_ANDROID
// These tests do not work on Android. Why is unclear.
// https://bugs.webrtc.org/9461

// Test getting the usage fingerprint for an audio/video connection.
TEST_F(PeerConnectionUsageHistogramTest, FingerprintAudioVideo) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  caller->AddAudioTrack("audio");
  caller->AddVideoTrack("video");
  ASSERT_TRUE(caller->ConnectTo(callee.get()));
  caller->pc()->Close();
  callee->pc()->Close();
  int expected_fingerprint = MakeUsageFingerprint(
      {PeerConnection::UsageEvent::AUDIO_ADDED,
       PeerConnection::UsageEvent::VIDEO_ADDED,
       PeerConnection::UsageEvent::SET_LOCAL_DESCRIPTION_CALLED,
       PeerConnection::UsageEvent::SET_REMOTE_DESCRIPTION_CALLED,
       PeerConnection::UsageEvent::CANDIDATE_COLLECTED,
       PeerConnection::UsageEvent::REMOTE_CANDIDATE_ADDED,
       PeerConnection::UsageEvent::ICE_STATE_CONNECTED,
       PeerConnection::UsageEvent::CLOSE_CALLED});
  // In this case, we may or may not have PRIVATE_CANDIDATE_COLLECTED,
  // depending on the machine configuration.
  EXPECT_EQ(2, webrtc::metrics::NumSamples(kUsagePatternMetric));
  EXPECT_TRUE(
      webrtc::metrics::NumEvents(kUsagePatternMetric, expected_fingerprint) ==
          2 ||
      webrtc::metrics::NumEvents(
          kUsagePatternMetric,
          expected_fingerprint |
              static_cast<int>(
                  PeerConnection::UsageEvent::PRIVATE_CANDIDATE_COLLECTED)) ==
          2);
}

// Test getting the usage fingerprint when there are no host candidates.
TEST_F(PeerConnectionUsageHistogramTest, FingerprintWithNoHostCandidates) {
  RTCConfiguration config;
  config.type = PeerConnectionInterface::kNoHost;
  auto caller = CreatePeerConnection(config);
  auto callee = CreatePeerConnection(config);
  caller->AddAudioTrack("audio");
  caller->AddVideoTrack("video");
  // Under some bot configurations, this will fail - presumably bots where
  // no working non-host addresses exist.
  if (!caller->ConnectTo(callee.get())) {
    return;
  }
  // If we manage to connect, we should get this precise fingerprint.
  caller->pc()->Close();
  callee->pc()->Close();
  int expected_fingerprint = MakeUsageFingerprint(
      {PeerConnection::UsageEvent::AUDIO_ADDED,
       PeerConnection::UsageEvent::VIDEO_ADDED,
       PeerConnection::UsageEvent::SET_LOCAL_DESCRIPTION_CALLED,
       PeerConnection::UsageEvent::SET_REMOTE_DESCRIPTION_CALLED,
       PeerConnection::UsageEvent::CANDIDATE_COLLECTED,
       PeerConnection::UsageEvent::REMOTE_CANDIDATE_ADDED,
       PeerConnection::UsageEvent::ICE_STATE_CONNECTED,
       PeerConnection::UsageEvent::CLOSE_CALLED});
  EXPECT_EQ(2, webrtc::metrics::NumSamples(kUsagePatternMetric));
  EXPECT_EQ(
      2, webrtc::metrics::NumEvents(kUsagePatternMetric, expected_fingerprint));
}

#ifdef HAVE_SCTP
TEST_F(PeerConnectionUsageHistogramTest, FingerprintDataOnly) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  caller->CreateDataChannel("foodata");
  ASSERT_TRUE(caller->ConnectTo(callee.get()));
  ASSERT_TRUE_WAIT(callee->HaveDataChannel(), kDefaultTimeout);
  caller->pc()->Close();
  callee->pc()->Close();
  int expected_fingerprint = MakeUsageFingerprint(
      {PeerConnection::UsageEvent::DATA_ADDED,
       PeerConnection::UsageEvent::SET_LOCAL_DESCRIPTION_CALLED,
       PeerConnection::UsageEvent::SET_REMOTE_DESCRIPTION_CALLED,
       PeerConnection::UsageEvent::CANDIDATE_COLLECTED,
       PeerConnection::UsageEvent::REMOTE_CANDIDATE_ADDED,
       PeerConnection::UsageEvent::ICE_STATE_CONNECTED,
       PeerConnection::UsageEvent::CLOSE_CALLED});
  EXPECT_EQ(2, webrtc::metrics::NumSamples(kUsagePatternMetric));
  EXPECT_TRUE(
      webrtc::metrics::NumEvents(kUsagePatternMetric, expected_fingerprint) ==
          2 ||
      webrtc::metrics::NumEvents(
          kUsagePatternMetric,
          expected_fingerprint |
              static_cast<int>(
                  PeerConnection::UsageEvent::PRIVATE_CANDIDATE_COLLECTED)) ==
          2);
}
#endif  // HAVE_SCTP
#endif  // WEBRTC_ANDROID

TEST_F(PeerConnectionUsageHistogramTest, FingerprintStunTurn) {
  RTCConfiguration configuration;
  PeerConnection::IceServer server;
  server.urls = {"stun:dummy.stun.server/"};
  configuration.servers.push_back(server);
  server.urls = {"turn:dummy.turn.server/"};
  server.username = "username";
  server.password = "password";
  configuration.servers.push_back(server);
  auto caller = CreatePeerConnection(configuration);
  ASSERT_TRUE(caller);
  caller->pc()->Close();
  int expected_fingerprint =
      MakeUsageFingerprint({PeerConnection::UsageEvent::STUN_SERVER_ADDED,
                            PeerConnection::UsageEvent::TURN_SERVER_ADDED,
                            PeerConnection::UsageEvent::CLOSE_CALLED});
  EXPECT_EQ(1, webrtc::metrics::NumSamples(kUsagePatternMetric));
  EXPECT_EQ(
      1, webrtc::metrics::NumEvents(kUsagePatternMetric, expected_fingerprint));
}

TEST_F(PeerConnectionUsageHistogramTest, FingerprintStunTurnInReconfiguration) {
  RTCConfiguration configuration;
  PeerConnection::IceServer server;
  server.urls = {"stun:dummy.stun.server/"};
  configuration.servers.push_back(server);
  server.urls = {"turn:dummy.turn.server/"};
  server.username = "username";
  server.password = "password";
  configuration.servers.push_back(server);
  auto caller = CreatePeerConnection();
  ASSERT_TRUE(caller);
  RTCError error;
  caller->pc()->SetConfiguration(configuration, &error);
  ASSERT_TRUE(error.ok());
  caller->pc()->Close();
  int expected_fingerprint =
      MakeUsageFingerprint({PeerConnection::UsageEvent::STUN_SERVER_ADDED,
                            PeerConnection::UsageEvent::TURN_SERVER_ADDED,
                            PeerConnection::UsageEvent::CLOSE_CALLED});
  EXPECT_EQ(1, webrtc::metrics::NumSamples(kUsagePatternMetric));
  EXPECT_EQ(
      1, webrtc::metrics::NumEvents(kUsagePatternMetric, expected_fingerprint));
}

TEST_F(PeerConnectionUsageHistogramTest, FingerprintWithPrivateIP) {
  auto caller = CreatePeerConnectionWithPrivateLocalAddresses();
  caller->AddAudioTrack("audio");
  ASSERT_TRUE(caller->GenerateOfferAndCollectCandidates());
  caller->pc()->Close();
  int expected_fingerprint = MakeUsageFingerprint(
      {PeerConnection::UsageEvent::AUDIO_ADDED,
       PeerConnection::UsageEvent::SET_LOCAL_DESCRIPTION_CALLED,
       PeerConnection::UsageEvent::CANDIDATE_COLLECTED,
       PeerConnection::UsageEvent::CLOSE_CALLED,
       PeerConnection::UsageEvent::PRIVATE_CANDIDATE_COLLECTED});
  EXPECT_EQ(1, webrtc::metrics::NumSamples(kUsagePatternMetric));
  EXPECT_EQ(
      1, webrtc::metrics::NumEvents(kUsagePatternMetric, expected_fingerprint));
}

#ifndef WEBRTC_ANDROID
#ifdef HAVE_SCTP
TEST_F(PeerConnectionUsageHistogramTest, NotableUsageNoted) {
  auto caller = CreatePeerConnection();
  caller->CreateDataChannel("foo");
  caller->GenerateOfferAndCollectCandidates();
  caller->pc()->Close();
  int expected_fingerprint = MakeUsageFingerprint(
      {PeerConnection::UsageEvent::DATA_ADDED,
       PeerConnection::UsageEvent::SET_LOCAL_DESCRIPTION_CALLED,
       PeerConnection::UsageEvent::CANDIDATE_COLLECTED,
       PeerConnection::UsageEvent::CLOSE_CALLED});
  EXPECT_EQ(1, webrtc::metrics::NumSamples(kUsagePatternMetric));
  EXPECT_TRUE(expected_fingerprint == ObservedFingerprint() ||
              (expected_fingerprint |
               static_cast<int>(
                   PeerConnection::UsageEvent::PRIVATE_CANDIDATE_COLLECTED)) ==
                  ObservedFingerprint());
  EXPECT_EQ(absl::make_optional(ObservedFingerprint()),
            caller->observer()->interesting_usage_detected());
}

TEST_F(PeerConnectionUsageHistogramTest, NotableUsageOnEventFiring) {
  auto caller = CreatePeerConnection();
  caller->CreateDataChannel("foo");
  caller->GenerateOfferAndCollectCandidates();
  int expected_fingerprint = MakeUsageFingerprint(
      {PeerConnection::UsageEvent::DATA_ADDED,
       PeerConnection::UsageEvent::SET_LOCAL_DESCRIPTION_CALLED,
       PeerConnection::UsageEvent::CANDIDATE_COLLECTED});
  EXPECT_EQ(0, webrtc::metrics::NumSamples(kUsagePatternMetric));
  caller->GetInternalPeerConnection()->RequestUsagePatternReportForTesting();
  EXPECT_EQ_WAIT(1, webrtc::metrics::NumSamples(kUsagePatternMetric),
                 kDefaultTimeout);
  EXPECT_TRUE(expected_fingerprint == ObservedFingerprint() ||
              (expected_fingerprint |
               static_cast<int>(
                   PeerConnection::UsageEvent::PRIVATE_CANDIDATE_COLLECTED)) ==
                  ObservedFingerprint());
  EXPECT_EQ(absl::make_optional(ObservedFingerprint()),
            caller->observer()->interesting_usage_detected());
}

TEST_F(PeerConnectionUsageHistogramTest,
       NoNotableUsageOnEventFiringAfterClose) {
  auto caller = CreatePeerConnection();
  caller->CreateDataChannel("foo");
  caller->GenerateOfferAndCollectCandidates();
  int expected_fingerprint = MakeUsageFingerprint(
      {PeerConnection::UsageEvent::DATA_ADDED,
       PeerConnection::UsageEvent::SET_LOCAL_DESCRIPTION_CALLED,
       PeerConnection::UsageEvent::CANDIDATE_COLLECTED,
       PeerConnection::UsageEvent::CLOSE_CALLED});
  EXPECT_EQ(0, webrtc::metrics::NumSamples(kUsagePatternMetric));
  caller->pc()->Close();
  EXPECT_EQ(1, webrtc::metrics::NumSamples(kUsagePatternMetric));
  caller->GetInternalPeerConnection()->RequestUsagePatternReportForTesting();
  caller->observer()->ClearInterestingUsageDetector();
  EXPECT_EQ_WAIT(2, webrtc::metrics::NumSamples(kUsagePatternMetric),
                 kDefaultTimeout);
  EXPECT_TRUE(expected_fingerprint == ObservedFingerprint() ||
              (expected_fingerprint |
               static_cast<int>(
                   PeerConnection::UsageEvent::PRIVATE_CANDIDATE_COLLECTED)) ==
                  ObservedFingerprint());
  // After close, the usage-detection callback should NOT have been called.
  EXPECT_FALSE(caller->observer()->interesting_usage_detected());
}
#endif
#endif

}  // namespace webrtc
