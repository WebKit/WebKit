/*
 *  Copyright 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/sdp_payload_type_suggester.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/strings/string_view.h"
#include "api/jsep.h"
#include "api/peer_connection_interface.h"
#include "api/rtc_error.h"
#include "call/payload_type.h"
#include "media/base/codec.h"
#include "media/base/media_constants.h"
#include "pc/session_description.h"
#include "test/gtest.h"

namespace webrtc {

namespace {

constexpr absl::string_view kAudioMid1 = "a1";

const PeerConnectionInterface::BundlePolicy kBundlePolicy =
    PeerConnectionInterface::kBundlePolicyMaxBundle;

class SdpPayloadTypeSuggesterTest : public testing::Test {
 public:
  void AddAudioSection(SessionDescription* description) {
    std::unique_ptr<AudioContentDescription> audio =
        std::make_unique<AudioContentDescription>();
    audio->set_rtcp_mux(true);
    description->AddContent(std::string(kAudioMid1), MediaProtocolType::kRtp,
                            /* rejected= */ false, std::move(audio));
  }

 protected:
  SdpPayloadTypeSuggester suggester_{kBundlePolicy};
};

TEST_F(SdpPayloadTypeSuggesterTest, SuggestPayloadTypeBasic) {
  Codec pcmu_codec = CreateAudioCodec(-1, kPcmuCodecName, 8000, 1);
  RTCErrorOr<PayloadType> pcmu_pt =
      suggester_.SuggestPayloadType("mid", pcmu_codec);
  ASSERT_TRUE(pcmu_pt.ok());
  EXPECT_EQ(pcmu_pt.value(), PayloadType(0));
}

TEST_F(SdpPayloadTypeSuggesterTest, SuggestPayloadTypeReusesRemotePayloadType) {
  const PayloadType remote_lyra_pt(99);
  Codec remote_lyra_codec = CreateAudioCodec(remote_lyra_pt, "lyra", 8000, 1);
  auto offer = std::make_unique<SessionDescription>();
  AddAudioSection(offer.get());
  offer->contents()[0].media_description()->set_codecs({remote_lyra_codec});
  EXPECT_TRUE(
      suggester_.Update(offer.get(), /* local= */ false, SdpType::kOffer).ok());
  Codec local_lyra_codec = CreateAudioCodec(-1, "lyra", 8000, 1);
  RTCErrorOr<PayloadType> lyra_pt =
      suggester_.SuggestPayloadType(kAudioMid1, local_lyra_codec);
  ASSERT_TRUE(lyra_pt.ok());
  EXPECT_EQ(lyra_pt.value(), remote_lyra_pt);
}

TEST_F(SdpPayloadTypeSuggesterTest,
       SuggestPayloadTypeAvoidsRemoteLocalConflict) {
  // libwebrtc will normally allocate 110 to DTMF/48000
  const PayloadType remote_opus_pt(110);
  Codec remote_opus_codec = CreateAudioCodec(remote_opus_pt, "opus", 48000, 2);
  auto offer = std::make_unique<SessionDescription>();
  AddAudioSection(offer.get());
  offer->contents()[0].media_description()->set_codecs({remote_opus_codec});
  EXPECT_TRUE(
      suggester_.Update(offer.get(), /* local= */ false, SdpType::kOffer).ok());
  // Check that we get the Opus codec back with the remote PT
  Codec local_opus_codec = CreateAudioCodec(-1, "opus", 48000, 2);
  RTCErrorOr<PayloadType> local_opus_pt =
      suggester_.SuggestPayloadType(kAudioMid1, local_opus_codec);
  EXPECT_EQ(local_opus_pt.value(), remote_opus_pt);
  // Check that we don't get 110 allocated for DTMF, since it's in use for opus
  Codec local_other_codec = CreateAudioCodec(-1, kDtmfCodecName, 48000, 1);
  RTCErrorOr<PayloadType> other_pt =
      suggester_.SuggestPayloadType(kAudioMid1, local_other_codec);
  ASSERT_TRUE(other_pt.ok());
  EXPECT_NE(other_pt.value(), remote_opus_pt);
}

}  // namespace
}  // namespace webrtc
