/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdint>
#include <memory>

#include "absl/memory/memory.h"
#include "api/call/call_factory_interface.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_event_log/rtc_event_log_factory.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "call/simulated_network.h"
#include "media/engine/webrtc_media_engine.h"
#include "media/engine/webrtc_media_engine_defaults.h"
#include "modules/audio_device/include/test_audio_device.h"
#include "p2p/client/basic_port_allocator.h"
#include "pc/peer_connection_wrapper.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/network/network_emulation.h"
#include "test/network/network_emulation_manager.h"

namespace webrtc {
namespace test {
namespace {

constexpr int kDefaultTimeoutMs = 1000;
constexpr int kMaxAptitude = 32000;
constexpr int kSamplingFrequency = 48000;
constexpr char kSignalThreadName[] = "signaling_thread";

bool AddIceCandidates(PeerConnectionWrapper* peer,
                      std::vector<const IceCandidateInterface*> candidates) {
  bool success = true;
  for (const auto candidate : candidates) {
    if (!peer->pc()->AddIceCandidate(candidate)) {
      success = false;
    }
  }
  return success;
}

rtc::scoped_refptr<PeerConnectionFactoryInterface> CreatePeerConnectionFactory(
    rtc::Thread* signaling_thread,
    rtc::Thread* network_thread) {
  PeerConnectionFactoryDependencies pcf_deps;
  pcf_deps.task_queue_factory = CreateDefaultTaskQueueFactory();
  pcf_deps.call_factory = CreateCallFactory();
  pcf_deps.event_log_factory =
      absl::make_unique<RtcEventLogFactory>(pcf_deps.task_queue_factory.get());
  pcf_deps.network_thread = network_thread;
  pcf_deps.signaling_thread = signaling_thread;
  cricket::MediaEngineDependencies media_deps;
  media_deps.task_queue_factory = pcf_deps.task_queue_factory.get();
  media_deps.adm = TestAudioDeviceModule::Create(
      media_deps.task_queue_factory,
      TestAudioDeviceModule::CreatePulsedNoiseCapturer(kMaxAptitude,
                                                       kSamplingFrequency),
      TestAudioDeviceModule::CreateDiscardRenderer(kSamplingFrequency),
      /*speed=*/1.f);
  SetMediaEngineDefaults(&media_deps);
  pcf_deps.media_engine = cricket::CreateMediaEngine(std::move(media_deps));
  return CreateModularPeerConnectionFactory(std::move(pcf_deps));
}

rtc::scoped_refptr<PeerConnectionInterface> CreatePeerConnection(
    const rtc::scoped_refptr<PeerConnectionFactoryInterface>& pcf,
    PeerConnectionObserver* observer,
    rtc::NetworkManager* network_manager) {
  PeerConnectionDependencies pc_deps(observer);
  auto port_allocator =
      absl::make_unique<cricket::BasicPortAllocator>(network_manager);

  // This test does not support TCP
  int flags = cricket::PORTALLOCATOR_DISABLE_TCP;
  port_allocator->set_flags(port_allocator->flags() | flags);

  pc_deps.allocator = std::move(port_allocator);
  PeerConnectionInterface::RTCConfiguration rtc_configuration;
  rtc_configuration.sdp_semantics = SdpSemantics::kUnifiedPlan;

  return pcf->CreatePeerConnection(rtc_configuration, std::move(pc_deps));
}

}  // namespace

TEST(NetworkEmulationManagerPCTest, Run) {
  std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  signaling_thread->SetName(kSignalThreadName, nullptr);
  signaling_thread->Start();

  // Setup emulated network
  NetworkEmulationManagerImpl emulation;

  EmulatedNetworkNode* alice_node = emulation.CreateEmulatedNode(
      absl::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
  EmulatedNetworkNode* bob_node = emulation.CreateEmulatedNode(
      absl::make_unique<SimulatedNetwork>(BuiltInNetworkBehaviorConfig()));
  EmulatedEndpoint* alice_endpoint =
      emulation.CreateEndpoint(EmulatedEndpointConfig());
  EmulatedEndpoint* bob_endpoint =
      emulation.CreateEndpoint(EmulatedEndpointConfig());
  emulation.CreateRoute(alice_endpoint, {alice_node}, bob_endpoint);
  emulation.CreateRoute(bob_endpoint, {bob_node}, alice_endpoint);

  EmulatedNetworkManagerInterface* alice_network =
      emulation.CreateEmulatedNetworkManagerInterface({alice_endpoint});
  EmulatedNetworkManagerInterface* bob_network =
      emulation.CreateEmulatedNetworkManagerInterface({bob_endpoint});

  // Setup peer connections.
  rtc::scoped_refptr<PeerConnectionFactoryInterface> alice_pcf;
  rtc::scoped_refptr<PeerConnectionInterface> alice_pc;
  std::unique_ptr<MockPeerConnectionObserver> alice_observer =
      absl::make_unique<MockPeerConnectionObserver>();

  rtc::scoped_refptr<PeerConnectionFactoryInterface> bob_pcf;
  rtc::scoped_refptr<PeerConnectionInterface> bob_pc;
  std::unique_ptr<MockPeerConnectionObserver> bob_observer =
      absl::make_unique<MockPeerConnectionObserver>();

  signaling_thread->Invoke<void>(RTC_FROM_HERE, [&]() {
    alice_pcf = CreatePeerConnectionFactory(signaling_thread.get(),
                                            alice_network->network_thread());
    alice_pc = CreatePeerConnection(alice_pcf, alice_observer.get(),
                                    alice_network->network_manager());

    bob_pcf = CreatePeerConnectionFactory(signaling_thread.get(),
                                          bob_network->network_thread());
    bob_pc = CreatePeerConnection(bob_pcf, bob_observer.get(),
                                  bob_network->network_manager());
  });

  std::unique_ptr<PeerConnectionWrapper> alice =
      absl::make_unique<PeerConnectionWrapper>(alice_pcf, alice_pc,
                                               std::move(alice_observer));
  std::unique_ptr<PeerConnectionWrapper> bob =
      absl::make_unique<PeerConnectionWrapper>(bob_pcf, bob_pc,
                                               std::move(bob_observer));

  signaling_thread->Invoke<void>(RTC_FROM_HERE, [&]() {
    rtc::scoped_refptr<webrtc::AudioSourceInterface> source =
        alice_pcf->CreateAudioSource(cricket::AudioOptions());
    rtc::scoped_refptr<AudioTrackInterface> track =
        alice_pcf->CreateAudioTrack("audio", source);
    alice->AddTransceiver(track);

    // Connect peers.
    ASSERT_TRUE(alice->ExchangeOfferAnswerWith(bob.get()));
    // Do the SDP negotiation, and also exchange ice candidates.
    ASSERT_TRUE_WAIT(
        alice->signaling_state() == PeerConnectionInterface::kStable,
        kDefaultTimeoutMs);
    ASSERT_TRUE_WAIT(alice->IsIceGatheringDone(), kDefaultTimeoutMs);
    ASSERT_TRUE_WAIT(bob->IsIceGatheringDone(), kDefaultTimeoutMs);

    // Connect an ICE candidate pairs.
    ASSERT_TRUE(
        AddIceCandidates(bob.get(), alice->observer()->GetAllCandidates()));
    ASSERT_TRUE(
        AddIceCandidates(alice.get(), bob->observer()->GetAllCandidates()));
    // This means that ICE and DTLS are connected.
    ASSERT_TRUE_WAIT(bob->IsIceConnected(), kDefaultTimeoutMs);
    ASSERT_TRUE_WAIT(alice->IsIceConnected(), kDefaultTimeoutMs);

    // Close peer connections
    alice->pc()->Close();
    bob->pc()->Close();

    // Delete peers.
    alice.reset();
    bob.reset();
  });
}

}  // namespace test
}  // namespace webrtc
