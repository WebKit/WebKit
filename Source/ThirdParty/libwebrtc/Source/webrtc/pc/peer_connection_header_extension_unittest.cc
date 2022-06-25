/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <tuple>

#include "api/rtc_event_log/rtc_event_log_factory.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "media/base/fake_media_engine.h"
#include "p2p/base/fake_port_allocator.h"
#include "pc/media_session.h"
#include "pc/peer_connection_wrapper.h"
#include "rtc_base/gunit.h"
#include "rtc_base/strings/string_builder.h"
#include "test/gmock.h"

namespace webrtc {

using ::testing::Combine;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Return;
using ::testing::Values;

class PeerConnectionHeaderExtensionTest
    : public ::testing::TestWithParam<
          std::tuple<cricket::MediaType, SdpSemantics>> {
 protected:
  PeerConnectionHeaderExtensionTest()
      : extensions_(
            {RtpHeaderExtensionCapability("uri1",
                                          1,
                                          RtpTransceiverDirection::kStopped),
             RtpHeaderExtensionCapability("uri2",
                                          2,
                                          RtpTransceiverDirection::kSendOnly),
             RtpHeaderExtensionCapability("uri3",
                                          3,
                                          RtpTransceiverDirection::kRecvOnly),
             RtpHeaderExtensionCapability(
                 "uri4",
                 4,
                 RtpTransceiverDirection::kSendRecv)}) {}

  std::unique_ptr<PeerConnectionWrapper> CreatePeerConnection(
      cricket::MediaType media_type,
      absl::optional<SdpSemantics> semantics) {
    auto voice = std::make_unique<cricket::FakeVoiceEngine>();
    auto video = std::make_unique<cricket::FakeVideoEngine>();
    if (media_type == cricket::MediaType::MEDIA_TYPE_AUDIO)
      voice->SetRtpHeaderExtensions(extensions_);
    else
      video->SetRtpHeaderExtensions(extensions_);
    auto media_engine = std::make_unique<cricket::CompositeMediaEngine>(
        std::move(voice), std::move(video));
    PeerConnectionFactoryDependencies factory_dependencies;
    factory_dependencies.network_thread = rtc::Thread::Current();
    factory_dependencies.worker_thread = rtc::Thread::Current();
    factory_dependencies.signaling_thread = rtc::Thread::Current();
    factory_dependencies.task_queue_factory = CreateDefaultTaskQueueFactory();
    factory_dependencies.media_engine = std::move(media_engine);
    factory_dependencies.call_factory = CreateCallFactory();
    factory_dependencies.event_log_factory =
        std::make_unique<RtcEventLogFactory>(
            factory_dependencies.task_queue_factory.get());

    auto pc_factory =
        CreateModularPeerConnectionFactory(std::move(factory_dependencies));

    auto fake_port_allocator = std::make_unique<cricket::FakePortAllocator>(
        rtc::Thread::Current(), nullptr);
    auto observer = std::make_unique<MockPeerConnectionObserver>();
    PeerConnectionInterface::RTCConfiguration config;
    if (semantics)
      config.sdp_semantics = *semantics;
    auto pc = pc_factory->CreatePeerConnection(
        config, std::move(fake_port_allocator), nullptr, observer.get());
    observer->SetPeerConnectionInterface(pc.get());
    return std::make_unique<PeerConnectionWrapper>(pc_factory, pc,
                                                   std::move(observer));
  }

  std::vector<RtpHeaderExtensionCapability> extensions_;
};

TEST_P(PeerConnectionHeaderExtensionTest, TransceiverOffersHeaderExtensions) {
  cricket::MediaType media_type;
  SdpSemantics semantics;
  std::tie(media_type, semantics) = GetParam();
  if (semantics != SdpSemantics::kUnifiedPlan)
    return;
  std::unique_ptr<PeerConnectionWrapper> wrapper =
      CreatePeerConnection(media_type, semantics);
  auto transceiver = wrapper->AddTransceiver(media_type);
  EXPECT_EQ(transceiver->HeaderExtensionsToOffer(), extensions_);
}

TEST_P(PeerConnectionHeaderExtensionTest,
       SenderReceiverCapabilitiesReturnNotStoppedExtensions) {
  cricket::MediaType media_type;
  SdpSemantics semantics;
  std::tie(media_type, semantics) = GetParam();
  std::unique_ptr<PeerConnectionWrapper> wrapper =
      CreatePeerConnection(media_type, semantics);
  EXPECT_THAT(wrapper->pc_factory()
                  ->GetRtpSenderCapabilities(media_type)
                  .header_extensions,
              ElementsAre(Field(&RtpHeaderExtensionCapability::uri, "uri2"),
                          Field(&RtpHeaderExtensionCapability::uri, "uri3"),
                          Field(&RtpHeaderExtensionCapability::uri, "uri4")));
  EXPECT_EQ(wrapper->pc_factory()
                ->GetRtpReceiverCapabilities(media_type)
                .header_extensions,
            wrapper->pc_factory()
                ->GetRtpSenderCapabilities(media_type)
                .header_extensions);
}

TEST_P(PeerConnectionHeaderExtensionTest, OffersUnstoppedDefaultExtensions) {
  cricket::MediaType media_type;
  SdpSemantics semantics;
  std::tie(media_type, semantics) = GetParam();
  if (semantics != SdpSemantics::kUnifiedPlan)
    return;
  std::unique_ptr<PeerConnectionWrapper> wrapper =
      CreatePeerConnection(media_type, semantics);
  auto transceiver = wrapper->AddTransceiver(media_type);
  auto session_description = wrapper->CreateOffer();
  EXPECT_THAT(session_description->description()
                  ->contents()[0]
                  .media_description()
                  ->rtp_header_extensions(),
              ElementsAre(Field(&RtpExtension::uri, "uri2"),
                          Field(&RtpExtension::uri, "uri3"),
                          Field(&RtpExtension::uri, "uri4")));
}

TEST_P(PeerConnectionHeaderExtensionTest, OffersUnstoppedModifiedExtensions) {
  cricket::MediaType media_type;
  SdpSemantics semantics;
  std::tie(media_type, semantics) = GetParam();
  if (semantics != SdpSemantics::kUnifiedPlan)
    return;
  std::unique_ptr<PeerConnectionWrapper> wrapper =
      CreatePeerConnection(media_type, semantics);
  auto transceiver = wrapper->AddTransceiver(media_type);
  auto modified_extensions = transceiver->HeaderExtensionsToOffer();
  modified_extensions[0].direction = RtpTransceiverDirection::kSendRecv;
  modified_extensions[3].direction = RtpTransceiverDirection::kStopped;
  EXPECT_TRUE(
      transceiver->SetOfferedRtpHeaderExtensions(modified_extensions).ok());
  auto session_description = wrapper->CreateOffer();
  EXPECT_THAT(session_description->description()
                  ->contents()[0]
                  .media_description()
                  ->rtp_header_extensions(),
              ElementsAre(Field(&RtpExtension::uri, "uri1"),
                          Field(&RtpExtension::uri, "uri2"),
                          Field(&RtpExtension::uri, "uri3")));
}

TEST_P(PeerConnectionHeaderExtensionTest, NegotiatedExtensionsAreAccessible) {
  cricket::MediaType media_type;
  SdpSemantics semantics;
  std::tie(media_type, semantics) = GetParam();
  if (semantics != SdpSemantics::kUnifiedPlan)
    return;
  std::unique_ptr<PeerConnectionWrapper> pc1 =
      CreatePeerConnection(media_type, semantics);
  auto transceiver1 = pc1->AddTransceiver(media_type);
  auto modified_extensions = transceiver1->HeaderExtensionsToOffer();
  modified_extensions[3].direction = RtpTransceiverDirection::kStopped;
  transceiver1->SetOfferedRtpHeaderExtensions(modified_extensions);
  auto offer = pc1->CreateOfferAndSetAsLocal(
      PeerConnectionInterface::RTCOfferAnswerOptions());

  std::unique_ptr<PeerConnectionWrapper> pc2 =
      CreatePeerConnection(media_type, semantics);
  auto transceiver2 = pc2->AddTransceiver(media_type);
  pc2->SetRemoteDescription(std::move(offer));
  auto answer = pc2->CreateAnswerAndSetAsLocal(
      PeerConnectionInterface::RTCOfferAnswerOptions());
  pc1->SetRemoteDescription(std::move(answer));

  // PC1 has exts 2-4 unstopped and PC2 has exts 1-3 unstopped -> ext 2, 3
  // survives.
  EXPECT_THAT(transceiver1->HeaderExtensionsNegotiated(),
              ElementsAre(Field(&RtpHeaderExtensionCapability::uri, "uri2"),
                          Field(&RtpHeaderExtensionCapability::uri, "uri3")));
}

INSTANTIATE_TEST_SUITE_P(
    ,
    PeerConnectionHeaderExtensionTest,
    Combine(Values(SdpSemantics::kPlanB, SdpSemantics::kUnifiedPlan),
            Values(cricket::MediaType::MEDIA_TYPE_AUDIO,
                   cricket::MediaType::MEDIA_TYPE_VIDEO)),
    [](const testing::TestParamInfo<
        PeerConnectionHeaderExtensionTest::ParamType>& info) {
      cricket::MediaType media_type;
      SdpSemantics semantics;
      std::tie(media_type, semantics) = info.param;
      return (rtc::StringBuilder("With")
              << (semantics == SdpSemantics::kPlanB ? "PlanB" : "UnifiedPlan")
              << "And"
              << (media_type == cricket::MediaType::MEDIA_TYPE_AUDIO ? "Voice"
                                                                     : "Video")
              << "Engine")
          .str();
    });

}  // namespace webrtc
