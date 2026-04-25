/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "api/create_modular_peer_connection_factory.h"
#include "api/jsep.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/scoped_refptr.h"
#include "api/test/mock_async_dns_resolver.h"
#include "api/test/rtc_error_matchers.h"
#include "api/units/time_delta.h"
#include "api/webrtc_sdp.h"
#include "pc/peer_connection.h"
#include "pc/peer_connection_wrapper.h"
#include "pc/test/enable_fake_media.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "pc/usage_pattern.h"
#include "rtc_base/checks.h"
#include "rtc_base/fake_mdns_responder.h"
#include "rtc_base/fake_network.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/network.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/thread.h"
#include "rtc_base/virtual_socket_server.h"
#include "system_wrappers/include/metrics.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/wait_until.h"

namespace webrtc {
namespace {

using ::testing::NiceMock;
using RTCConfiguration = PeerConnectionInterface::RTCConfiguration;
using RTCOfferAnswerOptions = PeerConnectionInterface::RTCOfferAnswerOptions;

class PeerConnectionWrapperForUsageHistogramTest;
typedef PeerConnectionWrapperForUsageHistogramTest* RawWrapperPtr;

constexpr const char kBasicRemoteDescription[] = R"(v=0
o=- 0 0 IN IP4 127.0.0.1
s=-
t=0 0
m=audio 9 UDP/TLS/RTP/SAVPF 101
c=IN IP4 0.0.0.0
a=ice-ufrag:fooUfrag
a=ice-pwd:someRemotePasswordGeneratedString
a=fingerprint:sha-256 0A:B1:C2:D3:E4:F5:06:07:08:09:0A:0B:0C:0D:0E:0F:10:11:12:13:14:15:16:17:18:19:1A:1B:1C:1D:1E:1F
a=candidate:1 1 UDP 2130706431 %s 57892 typ host generation 0
a=setup:active
a=mid:0
a=sendrecv
a=rtcp-mux
a=rtpmap:101 fake_audio_codec/8000
)";

constexpr char kUsagePatternMetric[] = "WebRTC.PeerConnection.UsagePattern";
constexpr TimeDelta kDefaultTimeout = TimeDelta::Millis(10000);
const SocketAddress kLocalAddrs[2] = {SocketAddress("1.1.1.1", 0),
                                      SocketAddress("2.2.2.2", 0)};
const SocketAddress kPrivateLocalAddress("10.1.1.1", 0);
const SocketAddress kPrivateIpv6LocalAddress("fd12:3456:789a:1::1", 0);

int MakeUsageFingerprint(std::set<UsageEvent> events) {
  int signature = 0;
  for (const auto it : events) {
    signature |= static_cast<int>(it);
  }
  return signature;
}

class ObserverForUsageHistogramTest : public MockPeerConnectionObserver {
 public:
  void OnIceCandidate(const IceCandidate* candidate) override;

  void OnInterestingUsage(int usage_pattern) override {
    interesting_usage_detected_ = usage_pattern;
  }

  void PrepareToExchangeCandidates(RawWrapperPtr other) {
    candidate_target_ = other;
  }

  bool HaveDataChannel() { return last_datachannel_ != nullptr; }

  std::optional<int> interesting_usage_detected() {
    return interesting_usage_detected_;
  }

  void ClearInterestingUsageDetector() {
    interesting_usage_detected_ = std::optional<int>();
  }

  bool candidate_gathered() const { return candidate_gathered_; }

 private:
  std::optional<int> interesting_usage_detected_;
  bool candidate_gathered_ = false;
  RawWrapperPtr candidate_target_;  // Note: Not thread-safe against deletions.
};

class PeerConnectionWrapperForUsageHistogramTest
    : public PeerConnectionWrapper {
 public:
  using PeerConnectionWrapper::PeerConnectionWrapper;

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
  void BufferIceCandidate(const IceCandidate* candidate) {
    std::string sdp = candidate->ToString();
    std::unique_ptr<IceCandidate> candidate_copy(CreateIceCandidate(
        candidate->sdp_mid(), candidate->sdp_mline_index(), sdp, nullptr));
    buffered_candidates_.push_back(std::move(candidate_copy));
  }

  void AddBufferedIceCandidates() {
    for (const auto& candidate : buffered_candidates_) {
      EXPECT_TRUE(pc()->AddIceCandidate(candidate.get()));
    }
    buffered_candidates_.clear();
  }

  // This method performs the following actions in sequence:
  // 1. Exchange Offer and Answer.
  // 2. Exchange ICE candidates after both caller and callee complete
  // gathering.
  // 3. Wait for ICE to connect.
  //
  // This guarantees a deterministic sequence of events and also rules out the
  // occurrence of prflx candidates if the offer/answer signaling and the
  // candidate trickling race in order. In case prflx candidates need to be
  // simulated, see the approach used by tests below for that.
  bool ConnectTo(PeerConnectionWrapperForUsageHistogramTest* callee) {
    PrepareToExchangeCandidates(callee);
    if (!ExchangeOfferAnswerWith(callee)) {
      return false;
    }
    // Wait until the gathering completes before we signal the candidate.
    EXPECT_TRUE(WaitUntil([&] { return observer()->ice_gathering_complete_; },
                          {.timeout = kDefaultTimeout}));
    EXPECT_TRUE(
        WaitUntil([&] { return callee->observer()->ice_gathering_complete_; },
                  {.timeout = kDefaultTimeout}));
    AddBufferedIceCandidates();
    callee->AddBufferedIceCandidates();
    EXPECT_TRUE(
        WaitUntil([&] { return IsConnected(); }, {.timeout = kDefaultTimeout}));
    EXPECT_TRUE(WaitUntil([&] { return callee->IsConnected(); },
                          {.timeout = kDefaultTimeout}));
    return IsConnected() && callee->IsConnected();
  }

  bool GenerateOfferAndCollectCandidates() {
    std::unique_ptr<SessionDescriptionInterface> offer =
        CreateOffer(RTCOfferAnswerOptions());
    if (!offer) {
      return false;
    }
    bool set_local_offer = SetLocalDescription(offer->Clone());
    EXPECT_TRUE(set_local_offer);
    if (!set_local_offer) {
      return false;
    }
    EXPECT_THAT(WaitUntil([&] { return observer()->ice_gathering_complete_; },
                          ::testing::IsTrue()),
                IsRtcOk());
    return true;
  }

  PeerConnectionInterface::IceGatheringState ice_gathering_state() {
    return pc()->ice_gathering_state();
  }

 private:
  // Candidates that have been sent but not yet configured
  std::vector<std::unique_ptr<IceCandidate>> buffered_candidates_;
};

// Buffers candidates until we add them via AddBufferedIceCandidates.
void ObserverForUsageHistogramTest::OnIceCandidate(
    const IceCandidate* candidate) {
  // If target is not set, ignore. This happens in one-ended unit tests.
  if (candidate_target_) {
    this->candidate_target_->BufferIceCandidate(candidate);
  }
  candidate_gathered_ = true;
}

class PeerConnectionUsageHistogramTest : public ::testing::Test {
 protected:
  typedef std::unique_ptr<PeerConnectionWrapperForUsageHistogramTest>
      WrapperPtr;

  PeerConnectionUsageHistogramTest() : main_(&vss_) { metrics::Reset(); }

  WrapperPtr CreatePeerConnection() {
    RTCConfiguration config;
    config.sdp_semantics = SdpSemantics::kUnifiedPlan;
    return CreatePeerConnection(config);
  }

  WrapperPtr CreatePeerConnection(const RTCConfiguration& config) {
    return CreatePeerConnection(config,
                                PeerConnectionFactoryInterface::Options(),
                                PeerConnectionDependencies(nullptr), nullptr);
  }

  WrapperPtr CreatePeerConnectionWithMdns(const RTCConfiguration& config) {
    PeerConnectionDependencies deps(nullptr /* observer_in */);
    deps.async_dns_resolver_factory =
        std::make_unique<NiceMock<MockAsyncDnsResolverFactory>>();
    auto fake_network = std::make_unique<FakeNetworkManager>(Thread::Current());
    fake_network->set_mdns_responder(
        std::make_unique<FakeMdnsResponder>(Thread::Current()));
    fake_network->AddInterface(NextLocalAddress());

    return CreatePeerConnection(config,
                                PeerConnectionFactoryInterface::Options(),
                                std::move(deps), std::move(fake_network));
  }

  WrapperPtr CreatePeerConnectionWithImmediateReport() {
    RTCConfiguration configuration;
    configuration.sdp_semantics = SdpSemantics::kUnifiedPlan;
    configuration.report_usage_pattern_delay_ms = 0;
    return CreatePeerConnection(configuration);
  }

  WrapperPtr CreatePeerConnectionWithPrivateLocalAddresses() {
    auto fake_network = std::make_unique<FakeNetworkManager>(Thread::Current());
    fake_network->AddInterface(NextLocalAddress());
    fake_network->AddInterface(kPrivateLocalAddress);

    RTCConfiguration config;
    config.sdp_semantics = SdpSemantics::kUnifiedPlan;
    return CreatePeerConnection(
        config, PeerConnectionFactoryInterface::Options(),
        PeerConnectionDependencies(nullptr), std::move(fake_network));
  }

  WrapperPtr CreatePeerConnectionWithPrivateIpv6LocalAddresses() {
    auto fake_network = std::make_unique<FakeNetworkManager>(Thread::Current());
    fake_network->AddInterface(NextLocalAddress());
    fake_network->AddInterface(kPrivateIpv6LocalAddress);

    RTCConfiguration config;
    config.sdp_semantics = SdpSemantics::kUnifiedPlan;
    return CreatePeerConnection(
        config, PeerConnectionFactoryInterface::Options(),
        PeerConnectionDependencies(nullptr), std::move(fake_network));
  }

  WrapperPtr CreatePeerConnection(
      const RTCConfiguration& config,
      const PeerConnectionFactoryInterface::Options factory_options,
      PeerConnectionDependencies deps,
      std::unique_ptr<NetworkManager> network_manager) {
    PeerConnectionFactoryDependencies pcf_deps;
    pcf_deps.network_thread = Thread::Current();
    pcf_deps.worker_thread = Thread::Current();
    pcf_deps.signaling_thread = Thread::Current();
    pcf_deps.socket_factory = &vss_;
    if (network_manager != nullptr) {
      pcf_deps.network_manager = std::move(network_manager);
    } else {
      // If no network manager is provided, one will be created that uses the
      // host network. This doesn't work on all trybots.
      auto fake_network =
          std::make_unique<FakeNetworkManager>(pcf_deps.network_thread);
      fake_network->AddInterface(NextLocalAddress());
      pcf_deps.network_manager = std::move(fake_network);
    }
    EnableFakeMedia(pcf_deps);

    auto pc_factory = CreateModularPeerConnectionFactory(std::move(pcf_deps));
    pc_factory->SetOptions(factory_options);

    auto observer = std::make_unique<ObserverForUsageHistogramTest>();
    deps.observer = observer.get();

    auto result =
        pc_factory->CreatePeerConnectionOrError(config, std::move(deps));
    if (!result.ok()) {
      return nullptr;
    }

    observer->SetPeerConnectionInterface(result.value().get());
    auto wrapper = std::make_unique<PeerConnectionWrapperForUsageHistogramTest>(
        pc_factory, result.MoveValue(), std::move(observer));
    return wrapper;
  }

  int ObservedFingerprint() {
    // This works correctly only if there is only one sample value
    // that has been counted.
    // Returns -1 for "not found".
    return metrics::MinSample(kUsagePatternMetric);
  }

  SocketAddress NextLocalAddress() {
    RTC_DCHECK_LT(next_local_address_, std::ssize(kLocalAddrs));
    return kLocalAddrs[next_local_address_++];
  }

  int next_local_address_ = 0;
  VirtualSocketServer vss_;
  AutoSocketServerThread main_;
};

TEST_F(PeerConnectionUsageHistogramTest, UsageFingerprintHistogramFromTimeout) {
  auto pc = CreatePeerConnectionWithImmediateReport();

  int expected_fingerprint = MakeUsageFingerprint({});
  EXPECT_THAT(
      WaitUntil([&] { return metrics::NumSamples(kUsagePatternMetric); },
                ::testing::Eq(1)),
      IsRtcOk());
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents(kUsagePatternMetric, expected_fingerprint));
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
      {UsageEvent::AUDIO_ADDED, UsageEvent::VIDEO_ADDED,
       UsageEvent::SET_LOCAL_DESCRIPTION_SUCCEEDED,
       UsageEvent::SET_REMOTE_DESCRIPTION_SUCCEEDED,
       UsageEvent::CANDIDATE_COLLECTED, UsageEvent::ADD_ICE_CANDIDATE_SUCCEEDED,
       UsageEvent::ICE_STATE_CONNECTED, UsageEvent::REMOTE_CANDIDATE_ADDED,
       UsageEvent::DIRECT_CONNECTION_SELECTED, UsageEvent::CLOSE_CALLED});
  // In this case, we may or may not have PRIVATE_CANDIDATE_COLLECTED,
  // depending on the machine configuration.
  EXPECT_METRIC_EQ(2, metrics::NumSamples(kUsagePatternMetric));
  EXPECT_METRIC_TRUE(
      metrics::NumEvents(kUsagePatternMetric, expected_fingerprint) == 2 ||
      metrics::NumEvents(
          kUsagePatternMetric,
          expected_fingerprint |
              static_cast<int>(UsageEvent::PRIVATE_CANDIDATE_COLLECTED)) == 2);
}

// Test getting the usage fingerprint when the caller collects an mDNS
// candidate.
TEST_F(PeerConnectionUsageHistogramTest, FingerprintWithMdnsCaller) {
  RTCConfiguration config;
  config.sdp_semantics = SdpSemantics::kUnifiedPlan;

  // Enable hostname candidates with mDNS names.
  auto caller = CreatePeerConnectionWithMdns(config);
  auto callee = CreatePeerConnection(config);

  caller->AddAudioTrack("audio");
  caller->AddVideoTrack("video");
  ASSERT_TRUE(caller->ConnectTo(callee.get()));
  caller->pc()->Close();
  callee->pc()->Close();

  int expected_fingerprint_caller = MakeUsageFingerprint(
      {UsageEvent::AUDIO_ADDED, UsageEvent::VIDEO_ADDED,
       UsageEvent::SET_LOCAL_DESCRIPTION_SUCCEEDED,
       UsageEvent::SET_REMOTE_DESCRIPTION_SUCCEEDED,
       UsageEvent::CANDIDATE_COLLECTED, UsageEvent::MDNS_CANDIDATE_COLLECTED,
       UsageEvent::ADD_ICE_CANDIDATE_SUCCEEDED, UsageEvent::ICE_STATE_CONNECTED,
       UsageEvent::REMOTE_CANDIDATE_ADDED,
       UsageEvent::DIRECT_CONNECTION_SELECTED, UsageEvent::CLOSE_CALLED});

  // Without a resolver, the callee cannot resolve the received mDNS candidate
  // but can still connect with the caller via a prflx candidate. As a result,
  // the bit for the direct connection should not be logged.
  int expected_fingerprint_callee = MakeUsageFingerprint(
      {UsageEvent::AUDIO_ADDED, UsageEvent::VIDEO_ADDED,
       UsageEvent::SET_LOCAL_DESCRIPTION_SUCCEEDED,
       UsageEvent::SET_REMOTE_DESCRIPTION_SUCCEEDED,
       UsageEvent::CANDIDATE_COLLECTED, UsageEvent::ADD_ICE_CANDIDATE_SUCCEEDED,
       UsageEvent::REMOTE_MDNS_CANDIDATE_ADDED, UsageEvent::ICE_STATE_CONNECTED,
       UsageEvent::REMOTE_CANDIDATE_ADDED, UsageEvent::CLOSE_CALLED});
  EXPECT_METRIC_EQ(2, metrics::NumSamples(kUsagePatternMetric));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents(kUsagePatternMetric, expected_fingerprint_caller));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents(kUsagePatternMetric, expected_fingerprint_callee));
}

// Test getting the usage fingerprint when the callee collects an mDNS
// candidate.
TEST_F(PeerConnectionUsageHistogramTest, FingerprintWithMdnsCallee) {
  RTCConfiguration config;
  config.sdp_semantics = SdpSemantics::kUnifiedPlan;

  // Enable hostname candidates with mDNS names.
  auto caller = CreatePeerConnection(config);
  auto callee = CreatePeerConnectionWithMdns(config);

  caller->AddAudioTrack("audio");
  caller->AddVideoTrack("video");
  ASSERT_TRUE(caller->ConnectTo(callee.get()));
  caller->pc()->Close();
  callee->pc()->Close();

  // Similar to the test above, the caller connects with the callee via a prflx
  // candidate.
  int expected_fingerprint_caller = MakeUsageFingerprint(
      {UsageEvent::AUDIO_ADDED, UsageEvent::VIDEO_ADDED,
       UsageEvent::SET_LOCAL_DESCRIPTION_SUCCEEDED,
       UsageEvent::SET_REMOTE_DESCRIPTION_SUCCEEDED,
       UsageEvent::CANDIDATE_COLLECTED, UsageEvent::ADD_ICE_CANDIDATE_SUCCEEDED,
       UsageEvent::REMOTE_MDNS_CANDIDATE_ADDED, UsageEvent::ICE_STATE_CONNECTED,
       UsageEvent::REMOTE_CANDIDATE_ADDED, UsageEvent::CLOSE_CALLED});

  int expected_fingerprint_callee = MakeUsageFingerprint(
      {UsageEvent::AUDIO_ADDED, UsageEvent::VIDEO_ADDED,
       UsageEvent::SET_LOCAL_DESCRIPTION_SUCCEEDED,
       UsageEvent::SET_REMOTE_DESCRIPTION_SUCCEEDED,
       UsageEvent::CANDIDATE_COLLECTED, UsageEvent::MDNS_CANDIDATE_COLLECTED,
       UsageEvent::ADD_ICE_CANDIDATE_SUCCEEDED, UsageEvent::ICE_STATE_CONNECTED,
       UsageEvent::REMOTE_CANDIDATE_ADDED,
       UsageEvent::DIRECT_CONNECTION_SELECTED, UsageEvent::CLOSE_CALLED});
  EXPECT_METRIC_EQ(2, metrics::NumSamples(kUsagePatternMetric));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents(kUsagePatternMetric, expected_fingerprint_caller));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents(kUsagePatternMetric, expected_fingerprint_callee));
}

#ifdef WEBRTC_HAVE_SCTP
TEST_F(PeerConnectionUsageHistogramTest, FingerprintDataOnly) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnection();
  caller->CreateDataChannel("foodata");
  ASSERT_TRUE(caller->ConnectTo(callee.get()));
  ASSERT_THAT(
      WaitUntil([&] { return callee->HaveDataChannel(); }, ::testing::IsTrue()),
      IsRtcOk());
  caller->pc()->Close();
  callee->pc()->Close();
  int expected_fingerprint = MakeUsageFingerprint(
      {UsageEvent::DATA_ADDED, UsageEvent::SET_LOCAL_DESCRIPTION_SUCCEEDED,
       UsageEvent::SET_REMOTE_DESCRIPTION_SUCCEEDED,
       UsageEvent::CANDIDATE_COLLECTED, UsageEvent::ADD_ICE_CANDIDATE_SUCCEEDED,
       UsageEvent::ICE_STATE_CONNECTED, UsageEvent::REMOTE_CANDIDATE_ADDED,
       UsageEvent::DIRECT_CONNECTION_SELECTED, UsageEvent::CLOSE_CALLED});
  EXPECT_METRIC_EQ(2, metrics::NumSamples(kUsagePatternMetric));
  EXPECT_METRIC_TRUE(
      metrics::NumEvents(kUsagePatternMetric, expected_fingerprint) == 2 ||
      metrics::NumEvents(
          kUsagePatternMetric,
          expected_fingerprint |
              static_cast<int>(UsageEvent::PRIVATE_CANDIDATE_COLLECTED)) == 2);
}
#endif  // WEBRTC_HAVE_SCTP
#endif  // WEBRTC_ANDROID

TEST_F(PeerConnectionUsageHistogramTest, FingerprintStunTurn) {
  RTCConfiguration configuration;
  configuration.sdp_semantics = SdpSemantics::kUnifiedPlan;
  PeerConnection::IceServer server;
  server.urls = {"stun:dummy.stun.server"};
  configuration.servers.push_back(server);
  server.urls = {"turn:dummy.turn.server"};
  server.username = "username";
  server.password = "password";
  configuration.servers.push_back(server);
  auto caller = CreatePeerConnection(configuration);
  ASSERT_TRUE(caller);
  caller->pc()->Close();
  int expected_fingerprint = MakeUsageFingerprint(
      {UsageEvent::STUN_SERVER_ADDED, UsageEvent::TURN_SERVER_ADDED,
       UsageEvent::CLOSE_CALLED});
  EXPECT_METRIC_EQ(1, metrics::NumSamples(kUsagePatternMetric));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents(kUsagePatternMetric, expected_fingerprint));
}

TEST_F(PeerConnectionUsageHistogramTest, FingerprintStunTurnInReconfiguration) {
  RTCConfiguration configuration;
  configuration.sdp_semantics = SdpSemantics::kUnifiedPlan;
  PeerConnection::IceServer server;
  server.urls = {"stun:dummy.stun.server"};
  configuration.servers.push_back(server);
  server.urls = {"turn:dummy.turn.server"};
  server.username = "username";
  server.password = "password";
  configuration.servers.push_back(server);
  auto caller = CreatePeerConnection();
  ASSERT_TRUE(caller);
  ASSERT_TRUE(caller->pc()->SetConfiguration(configuration).ok());
  caller->pc()->Close();
  int expected_fingerprint = MakeUsageFingerprint(
      {UsageEvent::STUN_SERVER_ADDED, UsageEvent::TURN_SERVER_ADDED,
       UsageEvent::CLOSE_CALLED});
  EXPECT_METRIC_EQ(1, metrics::NumSamples(kUsagePatternMetric));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents(kUsagePatternMetric, expected_fingerprint));
}

TEST_F(PeerConnectionUsageHistogramTest, FingerprintWithPrivateIPCaller) {
  auto caller = CreatePeerConnectionWithPrivateLocalAddresses();
  auto callee = CreatePeerConnection();
  caller->AddAudioTrack("audio");
  ASSERT_TRUE(caller->ConnectTo(callee.get()));
  caller->pc()->Close();
  callee->pc()->Close();

  int expected_fingerprint_caller = MakeUsageFingerprint(
      {UsageEvent::AUDIO_ADDED, UsageEvent::SET_LOCAL_DESCRIPTION_SUCCEEDED,
       UsageEvent::SET_REMOTE_DESCRIPTION_SUCCEEDED,
       UsageEvent::CANDIDATE_COLLECTED, UsageEvent::PRIVATE_CANDIDATE_COLLECTED,
       UsageEvent::ADD_ICE_CANDIDATE_SUCCEEDED, UsageEvent::ICE_STATE_CONNECTED,
       UsageEvent::REMOTE_CANDIDATE_ADDED,
       UsageEvent::DIRECT_CONNECTION_SELECTED, UsageEvent::CLOSE_CALLED});

  int expected_fingerprint_callee = MakeUsageFingerprint(
      {UsageEvent::AUDIO_ADDED, UsageEvent::SET_LOCAL_DESCRIPTION_SUCCEEDED,
       UsageEvent::SET_REMOTE_DESCRIPTION_SUCCEEDED,
       UsageEvent::CANDIDATE_COLLECTED, UsageEvent::ADD_ICE_CANDIDATE_SUCCEEDED,
       UsageEvent::REMOTE_PRIVATE_CANDIDATE_ADDED,
       UsageEvent::ICE_STATE_CONNECTED, UsageEvent::REMOTE_CANDIDATE_ADDED,
       UsageEvent::DIRECT_CONNECTION_SELECTED, UsageEvent::CLOSE_CALLED});
  EXPECT_METRIC_EQ(2, metrics::NumSamples(kUsagePatternMetric));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents(kUsagePatternMetric, expected_fingerprint_caller));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents(kUsagePatternMetric, expected_fingerprint_callee));
}

TEST_F(PeerConnectionUsageHistogramTest, FingerprintWithPrivateIpv6Callee) {
  auto caller = CreatePeerConnection();
  auto callee = CreatePeerConnectionWithPrivateIpv6LocalAddresses();
  caller->AddAudioTrack("audio");
  ASSERT_TRUE(caller->ConnectTo(callee.get()));
  caller->pc()->Close();
  callee->pc()->Close();

  int expected_fingerprint_caller = MakeUsageFingerprint(
      {UsageEvent::AUDIO_ADDED, UsageEvent::SET_LOCAL_DESCRIPTION_SUCCEEDED,
       UsageEvent::SET_REMOTE_DESCRIPTION_SUCCEEDED,
       UsageEvent::CANDIDATE_COLLECTED, UsageEvent::ADD_ICE_CANDIDATE_SUCCEEDED,
       UsageEvent::REMOTE_PRIVATE_CANDIDATE_ADDED,
       UsageEvent::ICE_STATE_CONNECTED, UsageEvent::REMOTE_CANDIDATE_ADDED,
       UsageEvent::REMOTE_IPV6_CANDIDATE_ADDED,
       UsageEvent::DIRECT_CONNECTION_SELECTED, UsageEvent::CLOSE_CALLED});

  int expected_fingerprint_callee = MakeUsageFingerprint(
      {UsageEvent::AUDIO_ADDED, UsageEvent::SET_LOCAL_DESCRIPTION_SUCCEEDED,
       UsageEvent::SET_REMOTE_DESCRIPTION_SUCCEEDED,
       UsageEvent::CANDIDATE_COLLECTED, UsageEvent::PRIVATE_CANDIDATE_COLLECTED,
       UsageEvent::IPV6_CANDIDATE_COLLECTED,
       UsageEvent::ADD_ICE_CANDIDATE_SUCCEEDED,
       UsageEvent::REMOTE_CANDIDATE_ADDED, UsageEvent::ICE_STATE_CONNECTED,
       UsageEvent::DIRECT_CONNECTION_SELECTED, UsageEvent::CLOSE_CALLED});
  EXPECT_METRIC_EQ(2, metrics::NumSamples(kUsagePatternMetric));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents(kUsagePatternMetric, expected_fingerprint_caller));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents(kUsagePatternMetric, expected_fingerprint_callee));
}

struct IPAddressTypeTestConfig {
  absl::string_view address;
  IPAddressType address_type;
} const kAllCandidateIPAddressTypeTestConfigs[] = {
    {.address = "127.0.0.1", .address_type = IPAddressType::kLoopback},
    {.address = "::1", .address_type = IPAddressType::kLoopback},
    {.address = "localhost", .address_type = IPAddressType::kLoopback},
    {.address = "10.0.0.3", .address_type = IPAddressType::kPrivate},
    {.address = "FE80::3", .address_type = IPAddressType::kPrivate},
    {.address = "1.1.1.1", .address_type = IPAddressType::kPublic},
    {.address = "2001:4860:4860::8888", .address_type = IPAddressType::kPublic},
};

// Used by the test framework to print the param value for parameterized tests.
std::string PrintToString(const IPAddressTypeTestConfig& param) {
  return std::string(param.address);
}

class PeerConnectionCandidateIPAddressTypeHistogramTest
    : public PeerConnectionUsageHistogramTest,
      public ::testing::WithParamInterface<IPAddressTypeTestConfig> {};

// Tests that the correct IPAddressType is logged when adding candidates.
TEST_P(PeerConnectionCandidateIPAddressTypeHistogramTest,
       CandidateAddressType) {
  auto caller = CreatePeerConnection();

  caller->AddAudioTrack("audio");
  ASSERT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));

  // Set the remote description which includes a candidate using the IP Address
  // from the test's params.
  EXPECT_TRUE(caller->SetRemoteDescription(CreateSessionDescription(
      SdpType::kAnswer,
      absl::StrFormat(kBasicRemoteDescription, GetParam().address))));

  ASSERT_THAT(
      WaitUntil([&] { return caller->ice_gathering_state(); },
                ::testing::Eq(PeerConnectionInterface::kIceGatheringComplete)),
      IsRtcOk());
  ASSERT_TRUE(caller->observer()->candidate_gathered());

  auto samples = metrics::Samples("WebRTC.PeerConnection.CandidateAddressType");
  ASSERT_EQ(samples.size(), 1u);
  EXPECT_EQ(samples[static_cast<int>(GetParam().address_type)], 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PeerConnectionCandidateIPAddressTypeHistogramTest,
    ::testing::ValuesIn(kAllCandidateIPAddressTypeTestConfigs));

#ifndef WEBRTC_ANDROID
#ifdef WEBRTC_HAVE_SCTP
// Test that the usage pattern bits for adding remote (private IPv6) candidates
// are set when the remote candidates are retrieved from the Offer SDP instead
// of trickled ICE messages.
TEST_F(PeerConnectionUsageHistogramTest,
       AddRemoteCandidatesFromRemoteDescription) {
  // We construct the following data-channel-only scenario. The caller collects
  // IPv6 private local candidates and appends them in the Offer as in
  // non-trickled sessions. The callee collects mDNS candidates that are not
  // contained in the Answer as in Trickle ICE. Only the Offer and Answer are
  // signaled and we expect a connection with prflx remote candidates at the
  // caller side.
  auto caller = CreatePeerConnectionWithPrivateIpv6LocalAddresses();
  RTCConfiguration config;
  config.sdp_semantics = SdpSemantics::kUnifiedPlan;
  auto callee = CreatePeerConnectionWithMdns(config);
  caller->CreateDataChannel("test_channel");
  ASSERT_TRUE(caller->SetLocalDescription(caller->CreateOffer()));
  // Wait until the gathering completes so that the session description would
  // have contained ICE candidates.
  EXPECT_THAT(
      WaitUntil([&] { return caller->ice_gathering_state(); },
                ::testing::Eq(PeerConnectionInterface::kIceGatheringComplete)),
      IsRtcOk());
  EXPECT_TRUE(caller->observer()->candidate_gathered());
  // Get the current offer that contains candidates and pass it to the callee.
  //
  // Note that we cannot use CloneSessionDescription on `cur_offer` to obtain an
  // SDP with candidates. The method above does not strictly copy everything, in
  // particular, not copying the ICE candidates.
  // TODO(qingsi): Technically, this is a bug. Fix it.
  auto cur_offer = caller->pc()->local_description();
  ASSERT_TRUE(cur_offer);
  std::string sdp_with_candidates_str;
  cur_offer->ToString(&sdp_with_candidates_str);
  std::unique_ptr<SessionDescriptionInterface> offer =
      SdpDeserialize(SdpType::kOffer, sdp_with_candidates_str);
  ASSERT_TRUE(callee->SetRemoteDescription(std::move(offer)));

  // By default, the Answer created does not contain ICE candidates.
  std::unique_ptr<SessionDescriptionInterface> answer = callee->CreateAnswer();
  callee->SetLocalDescription(answer->Clone());
  caller->SetRemoteDescription(std::move(answer));
  EXPECT_THAT(
      WaitUntil([&] { return caller->IsConnected(); }, ::testing::IsTrue()),
      IsRtcOk());
  EXPECT_THAT(
      WaitUntil([&] { return callee->IsConnected(); }, ::testing::IsTrue()),
      IsRtcOk());
  // The callee needs to process the open message to have the data channel open.
  EXPECT_THAT(
      WaitUntil(
          [&] { return callee->observer()->last_datachannel_ != nullptr; },
          ::testing::IsTrue()),
      IsRtcOk());
  caller->pc()->Close();
  callee->pc()->Close();

  // The caller should not have added any remote candidate either via
  // AddIceCandidate or from the remote description. Also, the caller connects
  // with the callee via a prflx candidate and hence no direct connection bit
  // should be set.
  int expected_fingerprint_caller = MakeUsageFingerprint(
      {UsageEvent::DATA_ADDED, UsageEvent::SET_LOCAL_DESCRIPTION_SUCCEEDED,
       UsageEvent::SET_REMOTE_DESCRIPTION_SUCCEEDED,
       UsageEvent::CANDIDATE_COLLECTED, UsageEvent::PRIVATE_CANDIDATE_COLLECTED,
       UsageEvent::IPV6_CANDIDATE_COLLECTED, UsageEvent::ICE_STATE_CONNECTED,
       UsageEvent::CLOSE_CALLED});

  int expected_fingerprint_callee = MakeUsageFingerprint(
      {UsageEvent::DATA_ADDED, UsageEvent::SET_LOCAL_DESCRIPTION_SUCCEEDED,
       UsageEvent::SET_REMOTE_DESCRIPTION_SUCCEEDED,
       UsageEvent::CANDIDATE_COLLECTED, UsageEvent::MDNS_CANDIDATE_COLLECTED,
       UsageEvent::REMOTE_CANDIDATE_ADDED,
       UsageEvent::REMOTE_PRIVATE_CANDIDATE_ADDED,
       UsageEvent::REMOTE_IPV6_CANDIDATE_ADDED, UsageEvent::ICE_STATE_CONNECTED,
       UsageEvent::DIRECT_CONNECTION_SELECTED, UsageEvent::CLOSE_CALLED});
  EXPECT_METRIC_EQ(2, metrics::NumSamples(kUsagePatternMetric));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents(kUsagePatternMetric, expected_fingerprint_caller));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents(kUsagePatternMetric, expected_fingerprint_callee));
}

TEST_F(PeerConnectionUsageHistogramTest, NotableUsageNoted) {
  auto caller = CreatePeerConnection();
  caller->CreateDataChannel("foo");
  caller->GenerateOfferAndCollectCandidates();
  caller->pc()->Close();
  int expected_fingerprint = MakeUsageFingerprint(
      {UsageEvent::DATA_ADDED, UsageEvent::SET_LOCAL_DESCRIPTION_SUCCEEDED,
       UsageEvent::CANDIDATE_COLLECTED, UsageEvent::CLOSE_CALLED});
  EXPECT_METRIC_EQ(1, metrics::NumSamples(kUsagePatternMetric));
  EXPECT_METRIC_TRUE(
      expected_fingerprint == ObservedFingerprint() ||
      (expected_fingerprint |
       static_cast<int>(UsageEvent::PRIVATE_CANDIDATE_COLLECTED)) ==
          ObservedFingerprint());
  EXPECT_METRIC_EQ(std::make_optional(ObservedFingerprint()),
                   caller->observer()->interesting_usage_detected());
}

TEST_F(PeerConnectionUsageHistogramTest, NotableUsageOnEventFiring) {
  auto caller = CreatePeerConnection();
  caller->CreateDataChannel("foo");
  caller->GenerateOfferAndCollectCandidates();
  int expected_fingerprint = MakeUsageFingerprint(
      {UsageEvent::DATA_ADDED, UsageEvent::SET_LOCAL_DESCRIPTION_SUCCEEDED,
       UsageEvent::CANDIDATE_COLLECTED});
  EXPECT_METRIC_EQ(0, metrics::NumSamples(kUsagePatternMetric));
  caller->GetInternalPeerConnection()->RequestUsagePatternReportForTesting();
  EXPECT_THAT(
      WaitUntil([&] { return metrics::NumSamples(kUsagePatternMetric); },
                ::testing::Eq(1)),
      IsRtcOk());
  EXPECT_METRIC_TRUE(
      expected_fingerprint == ObservedFingerprint() ||
      (expected_fingerprint |
       static_cast<int>(UsageEvent::PRIVATE_CANDIDATE_COLLECTED)) ==
          ObservedFingerprint());
  EXPECT_METRIC_EQ(std::make_optional(ObservedFingerprint()),
                   caller->observer()->interesting_usage_detected());
}

TEST_F(PeerConnectionUsageHistogramTest,
       NoNotableUsageOnEventFiringAfterClose) {
  auto caller = CreatePeerConnection();
  caller->CreateDataChannel("foo");
  caller->GenerateOfferAndCollectCandidates();
  int expected_fingerprint = MakeUsageFingerprint(
      {UsageEvent::DATA_ADDED, UsageEvent::SET_LOCAL_DESCRIPTION_SUCCEEDED,
       UsageEvent::CANDIDATE_COLLECTED, UsageEvent::CLOSE_CALLED});
  EXPECT_METRIC_EQ(0, metrics::NumSamples(kUsagePatternMetric));
  caller->pc()->Close();
  EXPECT_METRIC_EQ(1, metrics::NumSamples(kUsagePatternMetric));
  caller->GetInternalPeerConnection()->RequestUsagePatternReportForTesting();
  caller->observer()->ClearInterestingUsageDetector();
  EXPECT_THAT(
      WaitUntil([&] { return metrics::NumSamples(kUsagePatternMetric); },
                ::testing::Eq(2)),
      IsRtcOk());
  EXPECT_METRIC_TRUE(
      expected_fingerprint == ObservedFingerprint() ||
      (expected_fingerprint |
       static_cast<int>(UsageEvent::PRIVATE_CANDIDATE_COLLECTED)) ==
          ObservedFingerprint());
  // After close, the usage-detection callback should NOT have been called.
  EXPECT_METRIC_FALSE(caller->observer()->interesting_usage_detected());
}
#endif
#endif

}  // namespace
}  // namespace webrtc
