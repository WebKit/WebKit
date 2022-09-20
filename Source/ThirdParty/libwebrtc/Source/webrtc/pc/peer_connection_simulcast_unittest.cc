/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <ostream>  // no-presubmit-check TODO(webrtc:8982)
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/strings/string_view.h"
#include "api/audio/audio_mixer.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/jsep.h"
#include "api/media_types.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/rtp_parameters.h"
#include "api/rtp_sender_interface.h"
#include "api/rtp_transceiver_direction.h"
#include "api/rtp_transceiver_interface.h"
#include "api/scoped_refptr.h"
#include "api/uma_metrics.h"
#include "api/video/video_codec_constants.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "media/base/rid_description.h"
#include "media/base/stream_params.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "pc/channel_interface.h"
#include "pc/peer_connection_wrapper.h"
#include "pc/session_description.h"
#include "pc/simulcast_description.h"
#include "pc/test/fake_audio_capture_module.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "rtc_base/checks.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/thread.h"
#include "rtc_base/unique_id_generator.h"
#include "system_wrappers/include/metrics.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::Contains;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Ne;
using ::testing::Pair;
using ::testing::Property;
using ::testing::SizeIs;

using cricket::MediaContentDescription;
using cricket::RidDescription;
using cricket::SimulcastDescription;
using cricket::SimulcastLayer;
using cricket::StreamParams;

namespace cricket {

std::ostream& operator<<(  // no-presubmit-check TODO(webrtc:8982)
    std::ostream& os,      // no-presubmit-check TODO(webrtc:8982)
    const SimulcastLayer& layer) {
  if (layer.is_paused) {
    os << "~";
  }
  return os << layer.rid;
}

}  // namespace cricket

namespace {

std::vector<SimulcastLayer> CreateLayers(const std::vector<std::string>& rids,
                                         const std::vector<bool>& active) {
  RTC_DCHECK_EQ(rids.size(), active.size());
  std::vector<SimulcastLayer> result;
  absl::c_transform(rids, active, std::back_inserter(result),
                    [](const std::string& rid, bool is_active) {
                      return SimulcastLayer(rid, !is_active);
                    });
  return result;
}
std::vector<SimulcastLayer> CreateLayers(const std::vector<std::string>& rids,
                                         bool active) {
  return CreateLayers(rids, std::vector<bool>(rids.size(), active));
}

#if RTC_METRICS_ENABLED
std::vector<SimulcastLayer> CreateLayers(int num_layers, bool active) {
  rtc::UniqueStringGenerator rid_generator;
  std::vector<std::string> rids;
  for (int i = 0; i < num_layers; ++i) {
    rids.push_back(rid_generator());
  }
  return CreateLayers(rids, active);
}
#endif

}  // namespace

namespace webrtc {

class PeerConnectionSimulcastTests : public ::testing::Test {
 public:
  PeerConnectionSimulcastTests()
      : pc_factory_(
            CreatePeerConnectionFactory(rtc::Thread::Current(),
                                        rtc::Thread::Current(),
                                        rtc::Thread::Current(),
                                        FakeAudioCaptureModule::Create(),
                                        CreateBuiltinAudioEncoderFactory(),
                                        CreateBuiltinAudioDecoderFactory(),
                                        CreateBuiltinVideoEncoderFactory(),
                                        CreateBuiltinVideoDecoderFactory(),
                                        nullptr,
                                        nullptr)) {}

  rtc::scoped_refptr<PeerConnectionInterface> CreatePeerConnection(
      MockPeerConnectionObserver* observer) {
    PeerConnectionInterface::RTCConfiguration config;
    config.sdp_semantics = SdpSemantics::kUnifiedPlan;
    PeerConnectionDependencies pcd(observer);
    auto result =
        pc_factory_->CreatePeerConnectionOrError(config, std::move(pcd));
    EXPECT_TRUE(result.ok());
    observer->SetPeerConnectionInterface(result.value().get());
    return result.MoveValue();
  }

  std::unique_ptr<PeerConnectionWrapper> CreatePeerConnectionWrapper() {
    auto observer = std::make_unique<MockPeerConnectionObserver>();
    auto pc = CreatePeerConnection(observer.get());
    return std::make_unique<PeerConnectionWrapper>(pc_factory_, pc,
                                                   std::move(observer));
  }

  void ExchangeOfferAnswer(PeerConnectionWrapper* local,
                           PeerConnectionWrapper* remote,
                           const std::vector<SimulcastLayer>& answer_layers) {
    auto offer = local->CreateOfferAndSetAsLocal();
    // Remove simulcast as the second peer connection won't support it.
    RemoveSimulcast(offer.get());
    std::string err;
    EXPECT_TRUE(remote->SetRemoteDescription(std::move(offer), &err)) << err;
    auto answer = remote->CreateAnswerAndSetAsLocal();
    // Setup the answer to look like a server response.
    auto mcd_answer = answer->description()->contents()[0].media_description();
    auto& receive_layers = mcd_answer->simulcast_description().receive_layers();
    for (const SimulcastLayer& layer : answer_layers) {
      receive_layers.AddLayer(layer);
    }
    EXPECT_TRUE(local->SetRemoteDescription(std::move(answer), &err)) << err;
  }

  RtpTransceiverInit CreateTransceiverInit(
      const std::vector<SimulcastLayer>& layers) {
    RtpTransceiverInit init;
    for (const SimulcastLayer& layer : layers) {
      RtpEncodingParameters encoding;
      encoding.rid = layer.rid;
      encoding.active = !layer.is_paused;
      init.send_encodings.push_back(encoding);
    }
    return init;
  }

  rtc::scoped_refptr<RtpTransceiverInterface> AddTransceiver(
      PeerConnectionWrapper* pc,
      const std::vector<SimulcastLayer>& layers,
      cricket::MediaType media_type = cricket::MEDIA_TYPE_VIDEO) {
    auto init = CreateTransceiverInit(layers);
    return pc->AddTransceiver(media_type, init);
  }

  SimulcastDescription RemoveSimulcast(SessionDescriptionInterface* sd) {
    auto mcd = sd->description()->contents()[0].media_description();
    auto result = mcd->simulcast_description();
    mcd->set_simulcast_description(SimulcastDescription());
    return result;
  }

  void AddRequestToReceiveSimulcast(const std::vector<SimulcastLayer>& layers,
                                    SessionDescriptionInterface* sd) {
    auto mcd = sd->description()->contents()[0].media_description();
    SimulcastDescription simulcast;
    auto& receive_layers = simulcast.receive_layers();
    for (const SimulcastLayer& layer : layers) {
      receive_layers.AddLayer(layer);
    }
    mcd->set_simulcast_description(simulcast);
  }

  void ValidateTransceiverParameters(
      rtc::scoped_refptr<RtpTransceiverInterface> transceiver,
      const std::vector<SimulcastLayer>& layers) {
    auto parameters = transceiver->sender()->GetParameters();
    std::vector<SimulcastLayer> result_layers;
    absl::c_transform(parameters.encodings, std::back_inserter(result_layers),
                      [](const RtpEncodingParameters& encoding) {
                        return SimulcastLayer(encoding.rid, !encoding.active);
                      });
    EXPECT_THAT(result_layers, ElementsAreArray(layers));
  }

 private:
  rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory_;
};

#if RTC_METRICS_ENABLED
// This class is used to test the metrics emitted for simulcast.
class PeerConnectionSimulcastMetricsTests
    : public PeerConnectionSimulcastTests,
      public ::testing::WithParamInterface<int> {
 protected:
  PeerConnectionSimulcastMetricsTests() { webrtc::metrics::Reset(); }

  std::map<int, int> LocalDescriptionSamples() {
    return metrics::Samples(
        "WebRTC.PeerConnection.Simulcast.ApplyLocalDescription");
  }
  std::map<int, int> RemoteDescriptionSamples() {
    return metrics::Samples(
        "WebRTC.PeerConnection.Simulcast.ApplyRemoteDescription");
  }
};
#endif

// Validates that RIDs are supported arguments when adding a transceiver.
TEST_F(PeerConnectionSimulcastTests, CanCreateTransceiverWithRid) {
  auto pc = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"f"}, true);
  auto transceiver = AddTransceiver(pc.get(), layers);
  ASSERT_TRUE(transceiver);
  auto parameters = transceiver->sender()->GetParameters();
  // Single RID should be removed.
  EXPECT_THAT(parameters.encodings,
              ElementsAre(Field("rid", &RtpEncodingParameters::rid, Eq(""))));
}

TEST_F(PeerConnectionSimulcastTests, CanCreateTransceiverWithSimulcast) {
  auto pc = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"f", "h", "q"}, true);
  auto transceiver = AddTransceiver(pc.get(), layers);
  ASSERT_TRUE(transceiver);
  ValidateTransceiverParameters(transceiver, layers);
}

TEST_F(PeerConnectionSimulcastTests, RidsAreAutogeneratedIfNotProvided) {
  auto pc = CreatePeerConnectionWrapper();
  auto init = CreateTransceiverInit(CreateLayers({"f", "h", "q"}, true));
  for (RtpEncodingParameters& parameters : init.send_encodings) {
    parameters.rid = "";
  }
  auto transceiver = pc->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, init);
  auto parameters = transceiver->sender()->GetParameters();
  ASSERT_EQ(3u, parameters.encodings.size());
  EXPECT_THAT(parameters.encodings,
              Each(Field("rid", &RtpEncodingParameters::rid, Ne(""))));
}

// Validates that an error is returned when there is a mix of supplied and not
// supplied RIDs in a call to AddTransceiver.
TEST_F(PeerConnectionSimulcastTests, MustSupplyAllOrNoRidsInSimulcast) {
  auto pc_wrapper = CreatePeerConnectionWrapper();
  auto pc = pc_wrapper->pc();
  // Cannot create a layer with empty RID. Remove the RID after init is created.
  auto layers = CreateLayers({"f", "h", "remove"}, true);
  auto init = CreateTransceiverInit(layers);
  init.send_encodings[2].rid = "";
  auto error = pc->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, init);
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER, error.error().type());
}

// Validates that an error is returned when illegal RIDs are supplied.
TEST_F(PeerConnectionSimulcastTests, ChecksForIllegalRidValues) {
  auto pc_wrapper = CreatePeerConnectionWrapper();
  auto pc = pc_wrapper->pc();
  auto layers = CreateLayers({"f", "h", "~q"}, true);
  auto init = CreateTransceiverInit(layers);
  auto error = pc->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, init);
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER, error.error().type());
}

// Validates that a single RID is removed from the encoding layer.
TEST_F(PeerConnectionSimulcastTests, SingleRidIsRemovedFromSessionDescription) {
  auto pc = CreatePeerConnectionWrapper();
  auto transceiver = AddTransceiver(pc.get(), CreateLayers({"1"}, true));
  auto offer = pc->CreateOfferAndSetAsLocal();
  ASSERT_TRUE(offer);
  auto contents = offer->description()->contents();
  ASSERT_EQ(1u, contents.size());
  EXPECT_THAT(contents[0].media_description()->streams(),
              ElementsAre(Property(&StreamParams::has_rids, false)));
}

TEST_F(PeerConnectionSimulcastTests, SimulcastLayersRemovedFromTail) {
  static_assert(
      kMaxSimulcastStreams < 8,
      "Test assumes that the platform does not allow 8 simulcast layers");
  auto pc = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"1", "2", "3", "4", "5", "6", "7", "8"}, true);
  std::vector<SimulcastLayer> expected_layers;
  std::copy_n(layers.begin(), kMaxSimulcastStreams,
              std::back_inserter(expected_layers));
  auto transceiver = AddTransceiver(pc.get(), layers);
  ValidateTransceiverParameters(transceiver, expected_layers);
}

// Checks that an offfer to send simulcast contains a SimulcastDescription.
TEST_F(PeerConnectionSimulcastTests, SimulcastAppearsInSessionDescription) {
  auto pc = CreatePeerConnectionWrapper();
  std::vector<std::string> rids({"f", "h", "q"});
  auto layers = CreateLayers(rids, true);
  auto transceiver = AddTransceiver(pc.get(), layers);
  auto offer = pc->CreateOffer();
  ASSERT_TRUE(offer);
  auto contents = offer->description()->contents();
  ASSERT_EQ(1u, contents.size());
  auto content = contents[0];
  auto mcd = content.media_description();
  ASSERT_TRUE(mcd->HasSimulcast());
  auto simulcast = mcd->simulcast_description();
  EXPECT_THAT(simulcast.receive_layers(), IsEmpty());
  // The size is validated separately because GetAllLayers() flattens the list.
  EXPECT_THAT(simulcast.send_layers(), SizeIs(3));
  std::vector<SimulcastLayer> result = simulcast.send_layers().GetAllLayers();
  EXPECT_THAT(result, ElementsAreArray(layers));
  auto streams = mcd->streams();
  ASSERT_EQ(1u, streams.size());
  auto stream = streams[0];
  EXPECT_FALSE(stream.has_ssrcs());
  EXPECT_TRUE(stream.has_rids());
  std::vector<std::string> result_rids;
  absl::c_transform(stream.rids(), std::back_inserter(result_rids),
                    [](const RidDescription& rid) { return rid.rid; });
  EXPECT_THAT(result_rids, ElementsAreArray(rids));
}

// Checks that Simulcast layers propagate to the sender parameters.
TEST_F(PeerConnectionSimulcastTests, SimulcastLayersAreSetInSender) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"f", "h", "q"}, true);
  auto transceiver = AddTransceiver(local.get(), layers);
  auto offer = local->CreateOfferAndSetAsLocal();
  {
    SCOPED_TRACE("after create offer");
    ValidateTransceiverParameters(transceiver, layers);
  }
  // Remove simulcast as the second peer connection won't support it.
  auto simulcast = RemoveSimulcast(offer.get());
  std::string error;
  EXPECT_TRUE(remote->SetRemoteDescription(std::move(offer), &error)) << error;
  auto answer = remote->CreateAnswerAndSetAsLocal();

  // Setup an answer that mimics a server accepting simulcast.
  auto mcd_answer = answer->description()->contents()[0].media_description();
  mcd_answer->mutable_streams().clear();
  auto simulcast_layers = simulcast.send_layers().GetAllLayers();
  auto& receive_layers = mcd_answer->simulcast_description().receive_layers();
  for (const auto& layer : simulcast_layers) {
    receive_layers.AddLayer(layer);
  }
  EXPECT_TRUE(local->SetRemoteDescription(std::move(answer), &error)) << error;
  {
    SCOPED_TRACE("after set remote");
    ValidateTransceiverParameters(transceiver, layers);
  }
}

// Checks that paused Simulcast layers propagate to the sender parameters.
TEST_F(PeerConnectionSimulcastTests, PausedSimulcastLayersAreDisabledInSender) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"f", "h", "q"}, {true, false, true});
  auto server_layers = CreateLayers({"f", "h", "q"}, {true, false, false});
  RTC_DCHECK_EQ(layers.size(), server_layers.size());
  auto transceiver = AddTransceiver(local.get(), layers);
  auto offer = local->CreateOfferAndSetAsLocal();
  {
    SCOPED_TRACE("after create offer");
    ValidateTransceiverParameters(transceiver, layers);
  }

  // Remove simulcast as the second peer connection won't support it.
  RemoveSimulcast(offer.get());
  std::string error;
  EXPECT_TRUE(remote->SetRemoteDescription(std::move(offer), &error)) << error;
  auto answer = remote->CreateAnswerAndSetAsLocal();

  // Setup an answer that mimics a server accepting simulcast.
  auto mcd_answer = answer->description()->contents()[0].media_description();
  mcd_answer->mutable_streams().clear();
  auto& receive_layers = mcd_answer->simulcast_description().receive_layers();
  for (const SimulcastLayer& layer : server_layers) {
    receive_layers.AddLayer(layer);
  }
  EXPECT_TRUE(local->SetRemoteDescription(std::move(answer), &error)) << error;
  {
    SCOPED_TRACE("after set remote");
    ValidateTransceiverParameters(transceiver, server_layers);
  }
}

// Checks that when Simulcast is not supported by the remote party, then all
// the layers (except the first) are removed.
TEST_F(PeerConnectionSimulcastTests, SimulcastRejectedRemovesExtraLayers) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"1", "2", "3", "4"}, true);
  auto transceiver = AddTransceiver(local.get(), layers);
  ExchangeOfferAnswer(local.get(), remote.get(), {});
  auto parameters = transceiver->sender()->GetParameters();
  // Should only have the first layer.
  EXPECT_THAT(parameters.encodings,
              ElementsAre(Field("rid", &RtpEncodingParameters::rid, Eq("1"))));
}

// Checks that if Simulcast is supported by remote party, but some layers are
// rejected, then only rejected layers are removed from the sender.
TEST_F(PeerConnectionSimulcastTests, RejectedSimulcastLayersAreDeactivated) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"1", "2", "3"}, true);
  auto expected_layers = CreateLayers({"2", "3"}, true);
  auto transceiver = AddTransceiver(local.get(), layers);
  auto offer = local->CreateOfferAndSetAsLocal();
  {
    SCOPED_TRACE("after create offer");
    ValidateTransceiverParameters(transceiver, layers);
  }
  // Remove simulcast as the second peer connection won't support it.
  auto removed_simulcast = RemoveSimulcast(offer.get());
  std::string error;
  EXPECT_TRUE(remote->SetRemoteDescription(std::move(offer), &error)) << error;
  auto answer = remote->CreateAnswerAndSetAsLocal();
  auto mcd_answer = answer->description()->contents()[0].media_description();
  // Setup the answer to look like a server response.
  // Remove one of the layers to reject it in the answer.
  auto simulcast_layers = removed_simulcast.send_layers().GetAllLayers();
  simulcast_layers.erase(simulcast_layers.begin());
  auto& receive_layers = mcd_answer->simulcast_description().receive_layers();
  for (const auto& layer : simulcast_layers) {
    receive_layers.AddLayer(layer);
  }
  ASSERT_TRUE(mcd_answer->HasSimulcast());
  EXPECT_TRUE(local->SetRemoteDescription(std::move(answer), &error)) << error;
  {
    SCOPED_TRACE("after set remote");
    ValidateTransceiverParameters(transceiver, expected_layers);
  }
}

// Checks that simulcast is set up correctly when the server sends an offer
// requesting to receive simulcast.
TEST_F(PeerConnectionSimulcastTests, ServerSendsOfferToReceiveSimulcast) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"f", "h", "q"}, true);
  AddTransceiver(local.get(), layers);
  auto offer = local->CreateOfferAndSetAsLocal();
  // Remove simulcast as a sender and set it up as a receiver.
  RemoveSimulcast(offer.get());
  AddRequestToReceiveSimulcast(layers, offer.get());
  std::string error;
  EXPECT_TRUE(remote->SetRemoteDescription(std::move(offer), &error)) << error;
  auto transceiver = remote->pc()->GetTransceivers()[0];
  transceiver->SetDirectionWithError(RtpTransceiverDirection::kSendRecv);
  EXPECT_TRUE(remote->CreateAnswerAndSetAsLocal());
  ValidateTransceiverParameters(transceiver, layers);
}

// Checks that SetRemoteDescription doesn't attempt to associate a transceiver
// when simulcast is requested by the server.
TEST_F(PeerConnectionSimulcastTests, TransceiverIsNotRecycledWithSimulcast) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"f", "h", "q"}, true);
  AddTransceiver(local.get(), layers);
  auto offer = local->CreateOfferAndSetAsLocal();
  // Remove simulcast as a sender and set it up as a receiver.
  RemoveSimulcast(offer.get());
  AddRequestToReceiveSimulcast(layers, offer.get());
  // Call AddTrack so that a transceiver is created.
  remote->AddVideoTrack("fake_track");
  std::string error;
  EXPECT_TRUE(remote->SetRemoteDescription(std::move(offer), &error)) << error;
  auto transceivers = remote->pc()->GetTransceivers();
  ASSERT_EQ(2u, transceivers.size());
  auto transceiver = transceivers[1];
  transceiver->SetDirectionWithError(RtpTransceiverDirection::kSendRecv);
  EXPECT_TRUE(remote->CreateAnswerAndSetAsLocal());
  ValidateTransceiverParameters(transceiver, layers);
}

// Checks that if the number of layers changes during negotiation, then any
// outstanding get/set parameters transaction is invalidated.
TEST_F(PeerConnectionSimulcastTests, ParametersAreInvalidatedWhenLayersChange) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"1", "2", "3"}, true);
  auto transceiver = AddTransceiver(local.get(), layers);
  auto parameters = transceiver->sender()->GetParameters();
  ASSERT_EQ(3u, parameters.encodings.size());
  // Response will reject simulcast altogether.
  ExchangeOfferAnswer(local.get(), remote.get(), {});
  auto result = transceiver->sender()->SetParameters(parameters);
  EXPECT_EQ(RTCErrorType::INVALID_STATE, result.type());
}

// Checks that even though negotiation modifies the sender's parameters, an
// outstanding get/set parameters transaction is not invalidated.
// This test negotiates twice because initial parameters before negotiation
// is missing critical information and cannot be set on the sender.
TEST_F(PeerConnectionSimulcastTests,
       NegotiationDoesNotInvalidateParameterTransactions) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"1", "2", "3"}, true);
  auto expected_layers = CreateLayers({"1", "2", "3"}, false);
  auto transceiver = AddTransceiver(local.get(), layers);
  ExchangeOfferAnswer(local.get(), remote.get(), expected_layers);

  // Verify that negotiation does not invalidate the parameters.
  auto parameters = transceiver->sender()->GetParameters();
  ExchangeOfferAnswer(local.get(), remote.get(), expected_layers);

  auto result = transceiver->sender()->SetParameters(parameters);
  EXPECT_TRUE(result.ok());
  ValidateTransceiverParameters(transceiver, expected_layers);
}

// Tests that simulcast is disabled if the RID extension is not negotiated
// regardless of if the RIDs and simulcast attribute were negotiated properly.
TEST_F(PeerConnectionSimulcastTests, NegotiationDoesNotHaveRidExtension) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"1", "2", "3"}, true);
  auto expected_layers = CreateLayers({"1"}, true);
  auto transceiver = AddTransceiver(local.get(), layers);
  auto offer = local->CreateOfferAndSetAsLocal();
  // Remove simulcast as the second peer connection won't support it.
  RemoveSimulcast(offer.get());
  std::string err;
  EXPECT_TRUE(remote->SetRemoteDescription(std::move(offer), &err)) << err;
  auto answer = remote->CreateAnswerAndSetAsLocal();
  // Setup the answer to look like a server response.
  // Drop the RID header extension.
  auto mcd_answer = answer->description()->contents()[0].media_description();
  auto& receive_layers = mcd_answer->simulcast_description().receive_layers();
  for (const SimulcastLayer& layer : layers) {
    receive_layers.AddLayer(layer);
  }
  cricket::RtpHeaderExtensions extensions;
  for (auto extension : mcd_answer->rtp_header_extensions()) {
    if (extension.uri != RtpExtension::kRidUri) {
      extensions.push_back(extension);
    }
  }
  mcd_answer->set_rtp_header_extensions(extensions);
  EXPECT_EQ(layers.size(), mcd_answer->simulcast_description()
                               .receive_layers()
                               .GetAllLayers()
                               .size());
  EXPECT_TRUE(local->SetRemoteDescription(std::move(answer), &err)) << err;
  ValidateTransceiverParameters(transceiver, expected_layers);
}

TEST_F(PeerConnectionSimulcastTests, SimulcastAudioRejected) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"1", "2", "3", "4"}, true);
  auto transceiver =
      AddTransceiver(local.get(), layers, cricket::MEDIA_TYPE_AUDIO);
  // Should only have the first layer.
  auto parameters = transceiver->sender()->GetParameters();
  EXPECT_EQ(1u, parameters.encodings.size());
  EXPECT_THAT(parameters.encodings,
              ElementsAre(Field("rid", &RtpEncodingParameters::rid, Eq(""))));
  ExchangeOfferAnswer(local.get(), remote.get(), {});
  // Still have a single layer after negotiation
  parameters = transceiver->sender()->GetParameters();
  EXPECT_EQ(1u, parameters.encodings.size());
  EXPECT_THAT(parameters.encodings,
              ElementsAre(Field("rid", &RtpEncodingParameters::rid, Eq(""))));
}

// Check that modifying the offer to remove simulcast and at the same
// time leaving in a RID line does not cause an exception.
TEST_F(PeerConnectionSimulcastTests, SimulcastSldModificationRejected) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"1", "2", "3"}, true);
  AddTransceiver(local.get(), layers);
  auto offer = local->CreateOffer();
  std::string as_string;
  EXPECT_TRUE(offer->ToString(&as_string));
  auto simulcast_marker = "a=rid:3 send\r\na=simulcast:send 1;2;3\r\n";
  auto pos = as_string.find(simulcast_marker);
  EXPECT_NE(pos, std::string::npos);
  as_string.erase(pos, strlen(simulcast_marker));
  SdpParseError parse_error;
  auto modified_offer =
      CreateSessionDescription(SdpType::kOffer, as_string, &parse_error);
  EXPECT_TRUE(modified_offer);
  EXPECT_TRUE(local->SetLocalDescription(std::move(modified_offer)));
}

#if RTC_METRICS_ENABLED
//
// Checks the logged metrics when simulcast is not used.
TEST_F(PeerConnectionSimulcastMetricsTests, NoSimulcastUsageIsLogged) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers(0, true);
  AddTransceiver(local.get(), layers);
  ExchangeOfferAnswer(local.get(), remote.get(), layers);

  EXPECT_THAT(LocalDescriptionSamples(),
              ElementsAre(Pair(kSimulcastApiVersionNone, 2)));
  EXPECT_THAT(RemoteDescriptionSamples(),
              ElementsAre(Pair(kSimulcastApiVersionNone, 2)));
}

// Checks the logged metrics when spec-compliant simulcast is used.
TEST_F(PeerConnectionSimulcastMetricsTests, SpecComplianceIsLogged) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers(3, true);
  AddTransceiver(local.get(), layers);
  ExchangeOfferAnswer(local.get(), remote.get(), layers);

  // Expecting 2 invocations of each, because we have 2 peer connections.
  // Only the local peer connection will be aware of simulcast.
  // The remote peer connection will think that there is no simulcast.
  EXPECT_THAT(LocalDescriptionSamples(),
              ElementsAre(Pair(kSimulcastApiVersionNone, 1),
                          Pair(kSimulcastApiVersionSpecCompliant, 1)));
  EXPECT_THAT(RemoteDescriptionSamples(),
              ElementsAre(Pair(kSimulcastApiVersionNone, 1),
                          Pair(kSimulcastApiVersionSpecCompliant, 1)));
}

// Checks the logged metrics when and incoming request to send spec-compliant
// simulcast is received from the remote party.
TEST_F(PeerConnectionSimulcastMetricsTests, IncomingSimulcastIsLogged) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers(3, true);
  AddTransceiver(local.get(), layers);
  auto offer = local->CreateOfferAndSetAsLocal();
  EXPECT_THAT(LocalDescriptionSamples(),
              ElementsAre(Pair(kSimulcastApiVersionSpecCompliant, 1)));

  // Remove simulcast as a sender and set it up as a receiver.
  RemoveSimulcast(offer.get());
  AddRequestToReceiveSimulcast(layers, offer.get());
  std::string error;
  EXPECT_TRUE(remote->SetRemoteDescription(std::move(offer), &error)) << error;
  EXPECT_THAT(RemoteDescriptionSamples(),
              ElementsAre(Pair(kSimulcastApiVersionSpecCompliant, 1)));

  auto transceiver = remote->pc()->GetTransceivers()[0];
  transceiver->SetDirectionWithError(RtpTransceiverDirection::kSendRecv);
  EXPECT_TRUE(remote->CreateAnswerAndSetAsLocal());
  EXPECT_THAT(LocalDescriptionSamples(),
              ElementsAre(Pair(kSimulcastApiVersionSpecCompliant, 2)));
}

// Checks that a spec-compliant simulcast offer that is rejected is logged.
TEST_F(PeerConnectionSimulcastMetricsTests, RejectedSimulcastIsLogged) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"1", "2", "3"}, true);
  AddTransceiver(local.get(), layers);
  auto offer = local->CreateOfferAndSetAsLocal();
  EXPECT_THAT(LocalDescriptionSamples(),
              ElementsAre(Pair(kSimulcastApiVersionSpecCompliant, 1)));
  RemoveSimulcast(offer.get());
  std::string error;
  EXPECT_TRUE(remote->SetRemoteDescription(std::move(offer), &error)) << error;
  EXPECT_THAT(RemoteDescriptionSamples(),
              ElementsAre(Pair(kSimulcastApiVersionNone, 1)));

  auto answer = remote->CreateAnswerAndSetAsLocal();
  EXPECT_THAT(LocalDescriptionSamples(),
              ElementsAre(Pair(kSimulcastApiVersionNone, 1),
                          Pair(kSimulcastApiVersionSpecCompliant, 1)));
  EXPECT_TRUE(local->SetRemoteDescription(std::move(answer), &error)) << error;
  EXPECT_THAT(RemoteDescriptionSamples(),
              ElementsAre(Pair(kSimulcastApiVersionNone, 2)));
}

// Checks the logged metrics when legacy munging simulcast technique is used.
TEST_F(PeerConnectionSimulcastMetricsTests, LegacySimulcastIsLogged) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers(0, true);
  AddTransceiver(local.get(), layers);
  auto offer = local->CreateOffer();
  // Munge the SDP to set up legacy simulcast.
  const std::string end_line = "\r\n";
  std::string sdp;
  offer->ToString(&sdp);
  rtc::StringBuilder builder(sdp);
  builder << "a=ssrc:1111 cname:slimshady" << end_line;
  builder << "a=ssrc:2222 cname:slimshady" << end_line;
  builder << "a=ssrc:3333 cname:slimshady" << end_line;
  builder << "a=ssrc-group:SIM 1111 2222 3333" << end_line;

  SdpParseError parse_error;
  auto sd =
      CreateSessionDescription(SdpType::kOffer, builder.str(), &parse_error);
  ASSERT_TRUE(sd) << parse_error.line << parse_error.description;
  std::string error;
  EXPECT_TRUE(local->SetLocalDescription(std::move(sd), &error)) << error;
  EXPECT_THAT(LocalDescriptionSamples(),
              ElementsAre(Pair(kSimulcastApiVersionLegacy, 1)));
  EXPECT_TRUE(remote->SetRemoteDescription(std::move(offer), &error)) << error;
  EXPECT_THAT(RemoteDescriptionSamples(),
              ElementsAre(Pair(kSimulcastApiVersionNone, 1)));
  auto answer = remote->CreateAnswerAndSetAsLocal();
  EXPECT_THAT(LocalDescriptionSamples(),
              ElementsAre(Pair(kSimulcastApiVersionNone, 1),
                          Pair(kSimulcastApiVersionLegacy, 1)));
  // Legacy simulcast is not signaled in remote description.
  EXPECT_TRUE(local->SetRemoteDescription(std::move(answer), &error)) << error;
  EXPECT_THAT(RemoteDescriptionSamples(),
              ElementsAre(Pair(kSimulcastApiVersionNone, 2)));
}

// Checks that disabling simulcast is logged in the metrics.
TEST_F(PeerConnectionSimulcastMetricsTests, SimulcastDisabledIsLogged) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"1", "2", "3"}, true);
  AddTransceiver(local.get(), layers);
  auto offer = local->CreateOfferAndSetAsLocal();
  RemoveSimulcast(offer.get());
  std::string error;
  EXPECT_TRUE(remote->SetRemoteDescription(std::move(offer), &error)) << error;
  auto answer = remote->CreateAnswerAndSetAsLocal();
  EXPECT_TRUE(local->SetRemoteDescription(std::move(answer), &error)) << error;

  EXPECT_EQ(1, metrics::NumSamples("WebRTC.PeerConnection.Simulcast.Disabled"));
  EXPECT_EQ(1,
            metrics::NumEvents("WebRTC.PeerConnection.Simulcast.Disabled", 1));
}

// Checks that the disabled metric is not logged if simulcast is not disabled.
TEST_F(PeerConnectionSimulcastMetricsTests, SimulcastDisabledIsNotLogged) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"1", "2", "3"}, true);
  AddTransceiver(local.get(), layers);
  ExchangeOfferAnswer(local.get(), remote.get(), layers);

  EXPECT_EQ(0, metrics::NumSamples("WebRTC.PeerConnection.Simulcast.Disabled"));
}

const int kMaxLayersInMetricsTest = 8;

// Checks that the number of send encodings is logged in a metric.
TEST_P(PeerConnectionSimulcastMetricsTests, NumberOfSendEncodingsIsLogged) {
  auto local = CreatePeerConnectionWrapper();
  auto num_layers = GetParam();
  auto layers = CreateLayers(num_layers, true);
  AddTransceiver(local.get(), layers);
  EXPECT_EQ(1, metrics::NumSamples(
                   "WebRTC.PeerConnection.Simulcast.NumberOfSendEncodings"));
  EXPECT_EQ(1, metrics::NumEvents(
                   "WebRTC.PeerConnection.Simulcast.NumberOfSendEncodings",
                   num_layers));
}

INSTANTIATE_TEST_SUITE_P(NumberOfSendEncodings,
                         PeerConnectionSimulcastMetricsTests,
                         ::testing::Range(0, kMaxLayersInMetricsTest));
#endif
}  // namespace webrtc
