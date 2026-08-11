/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// TODO: bugs.webrtc.org/360058654 - Once the redesign is completed, this will
// be merged with codec_vendor_unittest.cc.

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/strings/str_cat.h"
#include "api/field_trials.h"
#include "api/media_types.h"
#include "api/payload_type.h"
#include "api/rtc_error.h"
#include "api/rtp_parameters.h"
#include "api/rtp_transceiver_direction.h"
#include "call/fake_payload_type_suggester.h"
#include "media/base/codec.h"
#include "media/base/codec_list.h"
#include "media/base/fake_media_engine.h"
#include "media/base/media_constants.h"
#include "pc/codec_vendor.h"
#include "pc/media_options.h"
#include "pc/session_description.h"
#include "test/create_test_field_trials.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::Contains;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::Not;
using ::testing::SizeIs;

class CodecVendorRedesignTest : public ::testing::Test {
 protected:
  CodecVendorRedesignTest()
      : trials_(
            CreateTestFieldTrials("WebRTC-PayloadTypesInTransport/Enabled/")) {
    std::vector<Codec> audio_codecs({
        CreateAudioCodec(111, "opus", 48000, 2),
        CreateAudioCodec(63, "red", 48000, 2),
    });
    media_engine_.SetAudioSendCodecs(audio_codecs);
    media_engine_.SetAudioRecvCodecs(audio_codecs);

    std::vector<Codec> video_codecs({
        CreateVideoCodec(97, "vp8"),
        CreateVideoRtxCodec(98, 97),
        CreateVideoCodec(100, "red"),
    });
    media_engine_.SetVideoSendCodecs(video_codecs);
    media_engine_.SetVideoRecvCodecs(video_codecs);

    vendor_ = std::make_unique<CodecVendor>(&media_engine_,
                                            /*rtx_enabled=*/true, trials_);
  }

  FieldTrials trials_;
  FakeMediaEngine media_engine_;
  std::unique_ptr<CodecVendor> vendor_;
  FakePayloadTypeSuggester pt_suggester_;
};

TEST_F(CodecVendorRedesignTest, AudioOfferIncludesRedAndAssignsIds) {
  MediaDescriptionOptions options(MediaType::AUDIO, "audio",
                                  RtpTransceiverDirection::kSendRecv,
                                  /*stopped=*/false);

  auto result = vendor_->GetNegotiatedCodecsForOffer(
      options, MediaSessionOptions(), /*current_content=*/nullptr,
      pt_suggester_);

  ASSERT_TRUE(result.ok());
  const auto& codecs = result.value();

  EXPECT_THAT(codecs, Contains(Field(&Codec::name, "opus")));
  EXPECT_THAT(codecs, Contains(Field(&Codec::name, "red")));

  // Verify IDs are assigned
  for (const auto& codec : codecs) {
    EXPECT_TRUE(codec.id.IsSet());
  }

  // Verify RED linking
  auto red_it =
      absl::c_find_if(codecs, [](const Codec& c) { return c.name == "red"; });
  auto opus_it =
      absl::c_find_if(codecs, [](const Codec& c) { return c.name == "opus"; });
  ASSERT_NE(red_it, codecs.end());
  ASSERT_NE(opus_it, codecs.end());

  std::string fmtp;
  EXPECT_TRUE(red_it->GetParam(kCodecParamNotInNameValueFormat, &fmtp));
  EXPECT_EQ(fmtp, absl::StrCat(opus_it->id, "/", opus_it->id));
}

TEST_F(CodecVendorRedesignTest, VideoOfferIncludesRtxAndRedAndAssignsIds) {
  MediaDescriptionOptions options(MediaType::VIDEO, "video",
                                  RtpTransceiverDirection::kSendRecv,
                                  /*stopped=*/false);

  // Force VP8 to PT 120 so that RTX can get 121.
  pt_suggester_.SetSuggestion("video", "vp8", PayloadType(120));

  auto result = vendor_->GetNegotiatedCodecsForOffer(
      options, MediaSessionOptions(), /*current_content=*/nullptr,
      pt_suggester_);

  ASSERT_TRUE(result.ok());
  const auto& codecs = result.value();

  EXPECT_THAT(codecs, Contains(Field(&Codec::name, "vp8")));
  EXPECT_THAT(codecs, Contains(Field(&Codec::name, "rtx")));
  EXPECT_THAT(codecs, Contains(Field(&Codec::name, "red")));

  // Verify RTX linking
  auto rtx_it = absl::c_find_if(codecs, [](const Codec& c) {
    std::string apt;
    return c.name == "rtx" &&
           c.GetParam(kCodecParamAssociatedPayloadType, &apt);
  });
  auto vp8_it =
      absl::c_find_if(codecs, [](const Codec& c) { return c.name == "vp8"; });
  ASSERT_NE(rtx_it, codecs.end());
  ASSERT_NE(vp8_it, codecs.end());

  std::string apt;
  EXPECT_TRUE(rtx_it->GetParam(kCodecParamAssociatedPayloadType, &apt));
  EXPECT_EQ(apt, absl::StrCat(vp8_it->id));

  // Verify conventional assignment: RTX_PT = primary_PT + 1
  EXPECT_EQ(rtx_it->id.value(), vp8_it->id.value() + 1);
}

TEST_F(CodecVendorRedesignTest,
       VideoOfferExcludesResiliencyWhenAbsentFromEngine) {
  // Clear the media engine and set only VP8 without any resiliency codecs.
  std::vector<Codec> video_codecs({
      CreateVideoCodec(97, "vp8"),
  });
  media_engine_.SetVideoSendCodecs(video_codecs);
  media_engine_.SetVideoRecvCodecs(video_codecs);

  // Re-instantiate the vendor to pick up the new engine state.
  vendor_ = std::make_unique<CodecVendor>(&media_engine_,
                                          /*rtx_enabled=*/true, trials_);

  MediaDescriptionOptions options(MediaType::VIDEO, "video",
                                  RtpTransceiverDirection::kSendRecv,
                                  /*stopped=*/false);

  auto result = vendor_->GetNegotiatedCodecsForOffer(
      options, MediaSessionOptions(), /*current_content=*/nullptr,
      pt_suggester_);

  ASSERT_TRUE(result.ok());
  const auto& codecs = result.value();

  EXPECT_THAT(codecs, Contains(Field(&Codec::name, "vp8")));
  EXPECT_THAT(codecs, Not(Contains(Field(&Codec::name, "rtx"))));
  EXPECT_THAT(codecs, Not(Contains(Field(&Codec::name, "red"))));
}

TEST_F(CodecVendorRedesignTest, VideoOfferWithRecvOnlyAndNoEncoderFactory) {
  media_engine_.SetVideoSendCodecs({});
  std::vector<Codec> video_codecs({
      CreateVideoCodec(97, "vp8"),
  });
  media_engine_.SetVideoRecvCodecs(video_codecs);
  vendor_ = std::make_unique<CodecVendor>(&media_engine_,
                                          /*rtx_enabled=*/true, trials_);
  MediaDescriptionOptions options(MediaType::VIDEO, "video",
                                  RtpTransceiverDirection::kRecvOnly,
                                  /*stopped=*/false);
  auto result = vendor_->GetNegotiatedCodecsForOffer(
      options, MediaSessionOptions(), /*current_content=*/nullptr,
      pt_suggester_);
  ASSERT_TRUE(result.ok());
  EXPECT_THAT(result.value(), Contains(Field(&Codec::name, "vp8")));
}

TEST_F(CodecVendorRedesignTest, OfferMaintainsStableIds) {
  MediaDescriptionOptions options(MediaType::AUDIO, "audio",
                                  RtpTransceiverDirection::kSendRecv,
                                  /*stopped=*/false);

  auto result1 = vendor_->GetNegotiatedCodecsForOffer(
      options, MediaSessionOptions(), /*current_content=*/nullptr,
      pt_suggester_);
  ASSERT_TRUE(result1.ok());

  // Simulate a second offer for the same MID
  auto result2 = vendor_->GetNegotiatedCodecsForOffer(
      options, MediaSessionOptions(), /*current_content=*/nullptr,
      pt_suggester_);
  ASSERT_TRUE(result2.ok());

  ASSERT_EQ(result1.value().size(), result2.value().size());
  for (size_t i = 0; i < result1.value().size(); ++i) {
    EXPECT_EQ(result1.value()[i].name, result2.value()[i].name);
    EXPECT_EQ(result1.value()[i].id, result2.value()[i].id);
  }
}

TEST_F(CodecVendorRedesignTest, ZeroChannelsAudioCodecReproduceCrash) {
  std::vector<Codec> audio_codecs({
      CreateAudioCodec(111, "opus", 48000, 0),
  });
  media_engine_.SetAudioSendCodecs(audio_codecs);
  vendor_ = std::make_unique<CodecVendor>(&media_engine_, /*rtx_enabled=*/true,
                                          trials_);

  MediaDescriptionOptions options(MediaType::AUDIO, "audio",
                                  RtpTransceiverDirection::kSendRecv,
                                  /*stopped=*/false);

  auto result = vendor_->GetNegotiatedCodecsForOffer(
      options, MediaSessionOptions(), /*current_content=*/nullptr,
      pt_suggester_);
}

TEST_F(CodecVendorRedesignTest, DuplicatePayloadTypesAreAvoided) {
  // Scenario:
  // 1. Current session description has G722 with PT 96.
  // 2. We generate an offer for Audio.
  // 3. Offerer supports ISAC.
  // 4. ISAC would naturally want PT 96 (first free dynamic PT in picker).
  // 5. If CodecVendor registers G722(96) with the suggester, ISAC should
  // get 97.

  // G722 is NOT in the configurations we want to negotiate, but it IS in the
  // engine so it can be carried over if present in current_content.
  std::vector<Codec> audio_codecs({
      CreateAudioCodec(96, "G722", 16000, 1),
      CreateAudioCodec(97, "ISAC", 16000, 1),
  });
  media_engine_.SetAudioSendCodecs(audio_codecs);
  media_engine_.SetAudioRecvCodecs(audio_codecs);

  vendor_ = std::make_unique<CodecVendor>(&media_engine_, /*rtx_enabled=*/false,
                                          trials_);

  // Create current_content with G722 at PT 96.
  auto audio_description = std::make_unique<AudioContentDescription>();
  audio_description->AddCodec(CreateAudioCodec(96, "G722", 16000, 1));
  ContentInfo current_content(MediaProtocolType::kRtp, "audio",
                              std::move(audio_description));

  MediaDescriptionOptions options(MediaType::AUDIO, "audio",
                                  RtpTransceiverDirection::kSendRecv,
                                  /*stopped=*/false);

  auto result = vendor_->GetNegotiatedCodecsForOffer(
      options, MediaSessionOptions(), &current_content, pt_suggester_);

  ASSERT_TRUE(result.ok());
  const auto& codecs = result.value();

  // Verify G722(96) is there.
  auto g722_it =
      absl::c_find_if(codecs, [](const Codec& c) { return c.name == "G722"; });
  ASSERT_NE(g722_it, codecs.end());
  EXPECT_EQ(g722_it->id, PayloadType(96));

  // Verify ISAC is there and does NOT have PT 96.
  auto isac_it =
      absl::c_find_if(codecs, [](const Codec& c) { return c.name == "ISAC"; });
  ASSERT_NE(isac_it, codecs.end());
  EXPECT_NE(isac_it->id, PayloadType(96));
}

TEST_F(CodecVendorRedesignTest, IntersectConfigurationsFixedCrash) {
  // Scenario:
  // 1. Offerer supports H264 sendonly.
  // 2. Offerer supports H264 recvonly.
  // 3. Offerer supports H264 sendrecv.
  // 4. We generate an offer for a sendrecv section.
  // 5. RED/RTX/FEC should be correctly linked even if some codecs are
  // unidirectional.

  std::vector<Codec> video_codecs;
  Codec h264_sendrecv = CreateVideoCodec(96, "H264");
  h264_sendrecv.params["profile-level-id"] = "42f00b";
  video_codecs.push_back(h264_sendrecv);

  Codec h264_sendonly = CreateVideoCodec(101, "H264");
  h264_sendonly.params["profile-level-id"] = "640034";
  video_codecs.push_back(h264_sendonly);

  Codec h264_recvonly = CreateVideoCodec(35, "H264");
  h264_recvonly.params["profile-level-id"] = "f4001f";
  video_codecs.push_back(h264_recvonly);

  // Mark them all as having RTX in the engine
  video_codecs.push_back(CreateVideoRtxCodec(97, 96));
  video_codecs.push_back(CreateVideoRtxCodec(99, 101));
  video_codecs.push_back(CreateVideoRtxCodec(36, 35));

  // Add RED/FEC
  video_codecs.push_back(CreateVideoCodec(98, "red"));
  video_codecs.push_back(CreateVideoCodec(100, "ulpfec"));

  media_engine_.SetVideoSendCodecs(video_codecs);
  media_engine_.SetVideoRecvCodecs(video_codecs);

  vendor_ = std::make_unique<CodecVendor>(&media_engine_, /*rtx_enabled=*/true,
                                          trials_);

  MediaDescriptionOptions options(MediaType::VIDEO, "video",
                                  RtpTransceiverDirection::kSendRecv,
                                  /*stopped=*/false);

  auto result = vendor_->GetNegotiatedCodecsForOffer(
      options, MediaSessionOptions(), /*current_content=*/nullptr,
      pt_suggester_);

  ASSERT_TRUE(result.ok());
}

TEST_F(CodecVendorRedesignTest, RespectsAudioCodecPreferences) {
  MediaDescriptionOptions options(MediaType::AUDIO, "audio",
                                  RtpTransceiverDirection::kSendRecv,
                                  /*stopped=*/false);

  // Set preferences: [RED, Opus]
  std::vector<RtpCodecCapability> preferences;
  for (const auto& codec : media_engine_.voice().LegacySendCodecs()) {
    if (codec.name == "red" || codec.name == "opus") {
      RtpCodecCapability cap;
      cap.name = codec.name;
      cap.kind = MediaType::AUDIO;
      cap.clock_rate = codec.clockrate;
      cap.num_channels = codec.channels;
      cap.parameters = codec.params;
      preferences.push_back(cap);
    }
  }
  // Ensure RED is first
  if (preferences.size() == 2 && preferences[0].name == "opus") {
    std::swap(preferences[0], preferences[1]);
  }
  options.codec_preferences = preferences;

  auto result = vendor_->GetNegotiatedCodecsForOffer(
      options, MediaSessionOptions(), /*current_content=*/nullptr,
      pt_suggester_);

  ASSERT_TRUE(result.ok());
  const auto& codecs = result.value();

  ASSERT_GE(codecs.size(), 2u);
  EXPECT_EQ(codecs[0].name, "red");
  EXPECT_EQ(codecs[1].name, "opus");
}

TEST_F(CodecVendorRedesignTest, MidRecyclingToDifferentTypeFails) {
  // 1. Generate a video offer for MID "0"
  MediaDescriptionOptions video_options(MediaType::VIDEO, "0",
                                        RtpTransceiverDirection::kSendRecv,
                                        /*stopped=*/false);
  auto video_result = vendor_->GetNegotiatedCodecsForOffer(
      video_options, MediaSessionOptions(), /*current_content=*/nullptr,
      pt_suggester_);
  ASSERT_TRUE(video_result.ok());

  // 2. Generate an audio offer for the same MID "0" (invalid recycling)
  MediaDescriptionOptions audio_options(MediaType::AUDIO, "0",
                                        RtpTransceiverDirection::kSendRecv,
                                        /*stopped=*/false);

  // We need to provide current_content to simulate recycling
  auto video_description = std::make_unique<VideoContentDescription>();
  video_description->set_codecs(video_result.value());
  ContentInfo current_content(MediaProtocolType::kRtp, "0",
                              std::move(video_description));

  auto audio_result = vendor_->GetNegotiatedCodecsForOffer(
      audio_options, MediaSessionOptions(), &current_content, pt_suggester_);

  // Verify that changing media type for the same MID is an error.
  ASSERT_FALSE(audio_result.ok());
  EXPECT_EQ(audio_result.error().type(), RTCErrorType::INTERNAL_ERROR);
}

TEST_F(CodecVendorRedesignTest, VideoOfferIncludesFecAndAssignsIds) {
  // Explicitly enable FlexFEC field trial
  FieldTrials flexfec_trials(
      CreateTestFieldTrials("WebRTC-FlexFEC-03-Advertised/Enabled/"
                            "WebRTC-PayloadTypesInTransport/Enabled/"));

  std::vector<Codec> video_codecs({
      CreateVideoCodec(97, "vp8"),
      CreateVideoCodec(100, "ulpfec"),
      CreateVideoCodec(101, "flexfec-03"),
  });
  media_engine_.SetVideoSendCodecs(video_codecs);
  media_engine_.SetVideoRecvCodecs(video_codecs);

  auto flexfec_vendor = std::make_unique<CodecVendor>(
      &media_engine_, /*rtx_enabled=*/true, flexfec_trials);

  MediaDescriptionOptions options(MediaType::VIDEO, "video",
                                  RtpTransceiverDirection::kSendRecv,
                                  /*stopped=*/false);

  auto result = flexfec_vendor->GetNegotiatedCodecsForOffer(
      options, MediaSessionOptions(), /*current_content=*/nullptr,
      pt_suggester_);

  ASSERT_TRUE(result.ok());
  const auto& codecs = result.value();

  EXPECT_THAT(codecs, Contains(Field(&Codec::name, "vp8")));
  EXPECT_THAT(codecs, Contains(Field(&Codec::name, "ulpfec")));
  EXPECT_THAT(codecs, Contains(Field(&Codec::name, "flexfec-03")));

  // Verify IDs are assigned
  for (const auto& codec : codecs) {
    EXPECT_TRUE(codec.id.IsSet());
  }
}

TEST_F(CodecVendorRedesignTest, SetRawPacketizationAffectsSubsequentOffers) {
  MediaDescriptionOptions options1(MediaType::VIDEO, "video1",
                                   RtpTransceiverDirection::kSendRecv,
                                   /*stopped=*/false);

  RTCErrorOr<std::vector<Codec>> result1 = vendor_->GetNegotiatedCodecsForOffer(
      options1, MediaSessionOptions(), /*current_content=*/nullptr,
      pt_suggester_);

  ASSERT_TRUE(result1.ok());

  // Find VP8 in the result
  auto vp8_it = absl::c_find_if(result1.value(),
                                [](const Codec& c) { return c.name == "vp8"; });
  ASSERT_NE(vp8_it, result1.value().end());

  // Call SetRawPacketization with the VP8 codec (simulating what SdpOfferAnswer
  // does)
  vendor_->SetRawPacketization(*vp8_it);

  // Now create an offer for a NEW media section
  MediaDescriptionOptions options2(MediaType::VIDEO, "video2",
                                   RtpTransceiverDirection::kSendRecv,
                                   /*stopped=*/false);

  RTCErrorOr<std::vector<Codec>> result2 = vendor_->GetNegotiatedCodecsForOffer(
      options2, MediaSessionOptions(), /*current_content=*/nullptr,
      pt_suggester_);

  ASSERT_TRUE(result2.ok());

  // Verify that VP8 in the new offer has packetization=raw
  auto vp8_it2 = absl::c_find_if(
      result2.value(), [](const Codec& c) { return c.name == "vp8"; });
  ASSERT_NE(vp8_it2, result2.value().end());
  EXPECT_EQ(vp8_it2->packetization, kPacketizationParamRaw);
}

TEST_F(CodecVendorRedesignTest, SetRawPacketizationUpdatesCodecs) {
  std::vector<Codec> video_codecs = vendor_->video_send_codecs().codecs();
  ASSERT_GE(video_codecs.size(), 2u);

  Codec vp8_codec = video_codecs[0];
  ASSERT_EQ(vp8_codec.name, "vp8");

  Codec second_codec = video_codecs[1];

  vendor_->SetRawPacketization(vp8_codec);

  const CodecList& new_send_codecs = vendor_->video_send_codecs();
  auto vp8_it = absl::c_find_if(new_send_codecs.codecs(),
                                [](const Codec& c) { return c.name == "vp8"; });
  ASSERT_NE(vp8_it, new_send_codecs.codecs().end());
  EXPECT_EQ(vp8_it->packetization, kPacketizationParamRaw);

  // Check that the second codec is NOT changed.
  auto second_it = absl::c_find_if(
      new_send_codecs.codecs(),
      [&second_codec](const Codec& c) { return c.name == second_codec.name; });
  ASSERT_NE(second_it, new_send_codecs.codecs().end());
  EXPECT_EQ(second_it->packetization, second_codec.packetization);
}

}  // namespace
}  // namespace webrtc
