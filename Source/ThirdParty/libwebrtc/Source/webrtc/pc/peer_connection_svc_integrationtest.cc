/*
 *  Copyright 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Integration tests for PeerConnection.
// These tests exercise a full stack for the SVC extension.

#include <stdint.h>

#include <algorithm>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "api/media_types.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "api/rtp_parameters.h"
#include "api/rtp_transceiver_interface.h"
#include "api/scoped_refptr.h"
#include "api/test/rtc_error_matchers.h"
#include "media/base/media_constants.h"
#include "pc/test/integration_test_helpers.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/wait_until.h"

namespace webrtc {

namespace {

class PeerConnectionSVCIntegrationTest
    : public PeerConnectionIntegrationBaseTest {
 protected:
  PeerConnectionSVCIntegrationTest()
      : PeerConnectionIntegrationBaseTest(SdpSemantics::kUnifiedPlan) {}

  RTCError SetCodecPreferences(
      scoped_refptr<RtpTransceiverInterface> transceiver,
      absl::string_view codec_name) {
    RtpCapabilities capabilities =
        caller()->pc_factory()->GetRtpReceiverCapabilities(MediaType::VIDEO);
    std::vector<RtpCodecCapability> codecs;
    for (const RtpCodecCapability& codec_capability : capabilities.codecs) {
      if (codec_capability.name == codec_name)
        codecs.push_back(codec_capability);
    }
    return transceiver->SetCodecPreferences(codecs);
  }
};

TEST_F(PeerConnectionSVCIntegrationTest, AddTransceiverAcceptsL1T1) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  RtpTransceiverInit init;
  RtpEncodingParameters encoding_parameters;
  encoding_parameters.scalability_mode = "L1T1";
  init.send_encodings.push_back(encoding_parameters);
  auto transceiver_or_error =
      caller()->pc()->AddTransceiver(caller()->CreateLocalVideoTrack(), init);
  EXPECT_TRUE(transceiver_or_error.ok());
}

TEST_F(PeerConnectionSVCIntegrationTest, AddTransceiverAcceptsL3T3) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  RtpTransceiverInit init;
  RtpEncodingParameters encoding_parameters;
  encoding_parameters.scalability_mode = "L3T3";
  init.send_encodings.push_back(encoding_parameters);
  auto transceiver_or_error =
      caller()->pc()->AddTransceiver(caller()->CreateLocalVideoTrack(), init);
  EXPECT_TRUE(transceiver_or_error.ok());
}

TEST_F(PeerConnectionSVCIntegrationTest,
       AddTransceiverRejectsUnknownScalabilityMode) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();
  RtpTransceiverInit init;
  RtpEncodingParameters encoding_parameters;
  encoding_parameters.scalability_mode = "FOOBAR";
  init.send_encodings.push_back(encoding_parameters);
  auto transceiver_or_error =
      caller()->pc()->AddTransceiver(caller()->CreateLocalVideoTrack(), init);
  EXPECT_FALSE(transceiver_or_error.ok());
  EXPECT_EQ(transceiver_or_error.error().type(),
            RTCErrorType::UNSUPPORTED_OPERATION);
}

TEST_F(PeerConnectionSVCIntegrationTest, SetParametersAcceptsL1T3WithVP8) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  RtpCapabilities capabilities =
      caller()->pc_factory()->GetRtpReceiverCapabilities(MediaType::VIDEO);
  std::vector<RtpCodecCapability> vp8_codec;
  for (const RtpCodecCapability& codec_capability : capabilities.codecs) {
    if (codec_capability.name == kVp8CodecName)
      vp8_codec.push_back(codec_capability);
  }

  RtpTransceiverInit init;
  RtpEncodingParameters encoding_parameters;
  init.send_encodings.push_back(encoding_parameters);
  auto transceiver_or_error =
      caller()->pc()->AddTransceiver(caller()->CreateLocalVideoTrack(), init);
  ASSERT_TRUE(transceiver_or_error.ok());
  auto transceiver = transceiver_or_error.MoveValue();
  EXPECT_TRUE(transceiver->SetCodecPreferences(vp8_codec).ok());

  RtpParameters parameters = transceiver->sender()->GetParameters();
  ASSERT_EQ(parameters.encodings.size(), 1u);
  parameters.encodings[0].scalability_mode = "L1T3";
  auto result = transceiver->sender()->SetParameters(parameters);
  EXPECT_TRUE(result.ok());
}

TEST_F(PeerConnectionSVCIntegrationTest,
       SetParametersAcceptsL1T3WithVP8AfterNegotiation) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  RtpTransceiverInit init;
  RtpEncodingParameters encoding_parameters;
  init.send_encodings.push_back(encoding_parameters);
  auto transceiver_or_error =
      caller()->pc()->AddTransceiver(caller()->CreateLocalVideoTrack(), init);
  ASSERT_TRUE(transceiver_or_error.ok());
  auto transceiver = transceiver_or_error.MoveValue();
  EXPECT_TRUE(SetCodecPreferences(transceiver, kVp8CodecName).ok());

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_THAT(
      WaitUntil([&] { return SignalingStateStable(); }, ::testing::IsTrue()),
      IsRtcOk());

  RtpParameters parameters = transceiver->sender()->GetParameters();
  ASSERT_EQ(parameters.encodings.size(), 1u);
  parameters.encodings[0].scalability_mode = "L1T3";
  auto result = transceiver->sender()->SetParameters(parameters);
  EXPECT_TRUE(result.ok());
}

TEST_F(PeerConnectionSVCIntegrationTest,
       SetParametersAcceptsL3T3WithVP9AfterNegotiation) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  RtpTransceiverInit init;
  RtpEncodingParameters encoding_parameters;
  init.send_encodings.push_back(encoding_parameters);
  auto transceiver_or_error =
      caller()->pc()->AddTransceiver(caller()->CreateLocalVideoTrack(), init);
  ASSERT_TRUE(transceiver_or_error.ok());
  auto transceiver = transceiver_or_error.MoveValue();
  EXPECT_TRUE(SetCodecPreferences(transceiver, kVp9CodecName).ok());

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_THAT(
      WaitUntil([&] { return SignalingStateStable(); }, ::testing::IsTrue()),
      IsRtcOk());

  RtpParameters parameters = transceiver->sender()->GetParameters();
  ASSERT_EQ(parameters.encodings.size(), 1u);
  parameters.encodings[0].scalability_mode = "L3T3";
  auto result = transceiver->sender()->SetParameters(parameters);
  EXPECT_TRUE(result.ok());
}

TEST_F(PeerConnectionSVCIntegrationTest,
       SetParametersRejectsL3T3WithVP8AfterNegotiation) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  RtpTransceiverInit init;
  RtpEncodingParameters encoding_parameters;
  init.send_encodings.push_back(encoding_parameters);
  auto transceiver_or_error =
      caller()->pc()->AddTransceiver(caller()->CreateLocalVideoTrack(), init);
  ASSERT_TRUE(transceiver_or_error.ok());
  auto transceiver = transceiver_or_error.MoveValue();
  EXPECT_TRUE(SetCodecPreferences(transceiver, kVp8CodecName).ok());

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_THAT(
      WaitUntil([&] { return SignalingStateStable(); }, ::testing::IsTrue()),
      IsRtcOk());

  RtpParameters parameters = transceiver->sender()->GetParameters();
  ASSERT_EQ(parameters.encodings.size(), 1u);
  parameters.encodings[0].scalability_mode = "L3T3";
  auto result = transceiver->sender()->SetParameters(parameters);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.type(), RTCErrorType::INVALID_MODIFICATION);
}

TEST_F(PeerConnectionSVCIntegrationTest,
       SetParametersRejectsInvalidModeWithVP9AfterNegotiation) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  RtpTransceiverInit init;
  RtpEncodingParameters encoding_parameters;
  init.send_encodings.push_back(encoding_parameters);
  auto transceiver_or_error =
      caller()->pc()->AddTransceiver(caller()->CreateLocalVideoTrack(), init);
  ASSERT_TRUE(transceiver_or_error.ok());
  auto transceiver = transceiver_or_error.MoveValue();
  EXPECT_TRUE(SetCodecPreferences(transceiver, kVp9CodecName).ok());

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_THAT(
      WaitUntil([&] { return SignalingStateStable(); }, ::testing::IsTrue()),
      IsRtcOk());

  RtpParameters parameters = transceiver->sender()->GetParameters();
  ASSERT_EQ(parameters.encodings.size(), 1u);
  parameters.encodings[0].scalability_mode = "FOOBAR";
  auto result = transceiver->sender()->SetParameters(parameters);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.type(), RTCErrorType::INVALID_MODIFICATION);
}

TEST_F(PeerConnectionSVCIntegrationTest, FallbackToL1Tx) {
  ASSERT_TRUE(CreatePeerConnectionWrappers());
  ConnectFakeSignaling();

  RtpTransceiverInit init;
  RtpEncodingParameters encoding_parameters;
  init.send_encodings.push_back(encoding_parameters);
  auto transceiver_or_error =
      caller()->pc()->AddTransceiver(caller()->CreateLocalVideoTrack(), init);
  ASSERT_TRUE(transceiver_or_error.ok());
  auto caller_transceiver = transceiver_or_error.MoveValue();

  RtpCapabilities capabilities =
      caller()->pc_factory()->GetRtpReceiverCapabilities(MediaType::VIDEO);
  std::vector<RtpCodecCapability> send_codecs = capabilities.codecs;
  // Only keep VP9 in the caller
  send_codecs.erase(std::partition(send_codecs.begin(), send_codecs.end(),
                                   [](const auto& codec) -> bool {
                                     return codec.name == kVp9CodecName;
                                   }),
                    send_codecs.end());
  ASSERT_FALSE(send_codecs.empty());
  caller_transceiver->SetCodecPreferences(send_codecs);

  // L3T3 should be supported by VP9
  RtpParameters parameters = caller_transceiver->sender()->GetParameters();
  ASSERT_EQ(parameters.encodings.size(), 1u);
  parameters.encodings[0].scalability_mode = "L3T3";
  auto result = caller_transceiver->sender()->SetParameters(parameters);
  EXPECT_TRUE(result.ok());

  caller()->CreateAndSetAndSignalOffer();
  ASSERT_THAT(
      WaitUntil([&] { return SignalingStateStable(); }, ::testing::IsTrue()),
      IsRtcOk());

  parameters = caller_transceiver->sender()->GetParameters();
  ASSERT_TRUE(parameters.encodings[0].scalability_mode.has_value());
  EXPECT_TRUE(
      absl::StartsWith(*parameters.encodings[0].scalability_mode, "L3T3"));

  // Keep only VP8 in the caller
  send_codecs = capabilities.codecs;
  send_codecs.erase(std::partition(send_codecs.begin(), send_codecs.end(),
                                   [](const auto& codec) -> bool {
                                     return codec.name == kVp8CodecName;
                                   }),
                    send_codecs.end());
  ASSERT_FALSE(send_codecs.empty());
  caller_transceiver->SetCodecPreferences(send_codecs);

  // Renegotiate to force the new codec list to be used
  caller()->CreateAndSetAndSignalOffer();
  ASSERT_THAT(
      WaitUntil([&] { return SignalingStateStable(); }, ::testing::IsTrue()),
      IsRtcOk());

  // Fallback should happen and L3T3 is not used anymore
  parameters = caller_transceiver->sender()->GetParameters();
  ASSERT_TRUE(parameters.encodings[0].scalability_mode.has_value());
  EXPECT_TRUE(
      absl::StartsWith(*parameters.encodings[0].scalability_mode, "L1T"));
}
}  // namespace

}  // namespace webrtc
