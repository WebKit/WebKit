/*
 *  Copyright 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/codec_vendor.h"

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "api/field_trials.h"
#include "api/media_types.h"
#include "api/rtc_error.h"
#include "api/rtp_transceiver_direction.h"
#include "api/test/rtc_error_matchers.h"
#include "call/fake_payload_type_suggester.h"
#include "media/base/codec.h"
#include "media/base/codec_list.h"
#include "media/base/fake_media_engine.h"
#include "media/base/media_constants.h"
#include "media/base/test_utils.h"
#include "pc/media_options.h"
#include "pc/rtp_parameters_conversion.h"
#include "pc/session_description.h"
#include "rtc_base/checks.h"
#include "test/create_test_field_trials.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Ne;
using ::testing::Not;
using ::testing::Pair;

Codec CreateRedAudioCodec(absl::string_view encoding_id) {
  Codec red = CreateAudioCodec(63, "red", 48000, 2);
  red.SetParam(kCodecParamNotInNameValueFormat,
               std::string(encoding_id) + '/' + std::string(encoding_id));
  return red;
}

const Codec kAudioCodecs1[] = {CreateAudioCodec(111, "opus", 48000, 2),
                               CreateRedAudioCodec("111"),
                               CreateAudioCodec(102, "G722", 16000, 1),
                               CreateAudioCodec(0, "PCMU", 8000, 1),
                               CreateAudioCodec(8, "PCMA", 8000, 1),
                               CreateAudioCodec(107, "CN", 48000, 1)};

const Codec kAudioCodecs2[] = {
    CreateAudioCodec(126, "foo", 16000, 1),
    CreateAudioCodec(0, "PCMU", 8000, 1),
    CreateAudioCodec(127, "G722", 16000, 1),
};

const Codec kAudioCodecsAnswer[] = {
    CreateAudioCodec(102, "G722", 16000, 1),
    CreateAudioCodec(0, "PCMU", 8000, 1),
};

TEST(CodecVendorTest, TestSetAudioCodecs) {
  FieldTrials trials = CreateTestFieldTrials();
  std::vector<Codec> send_codecs = MAKE_VECTOR(kAudioCodecs1);
  std::vector<Codec> recv_codecs = MAKE_VECTOR(kAudioCodecs2);

  // The merged list of codecs should contain any send codecs that are also
  // nominally in the receive codecs list. Payload types should be picked from
  // the send codecs and a number-of-channels of 0 and 1 should be equivalent
  // (set to 1). This equals what happens when the send codecs are used in an
  // offer and the receive codecs are used in the following answer.
  const std::vector<Codec> sendrecv_codecs = MAKE_VECTOR(kAudioCodecsAnswer);
  RTC_CHECK_EQ(send_codecs[2].name, "G722")
      << "Please don't change shared test data!";
  RTC_CHECK_EQ(recv_codecs[2].name, "G722")
      << "Please don't change shared test data!";
  // Alter iLBC send codec to have zero channels, to test that that is handled
  // properly.
  send_codecs[2].channels = 0;

  // Alter PCMU receive codec to be lowercase, to test that case conversions
  // are handled properly.
  recv_codecs[1].name = "pcmu";

  // Test proper merge
  FakeMediaEngine media_engine;
  media_engine.SetAudioSendCodecs(send_codecs);
  media_engine.SetAudioRecvCodecs(recv_codecs);
  {
    CodecVendor codec_vendor(&media_engine, false, trials);
    EXPECT_EQ(send_codecs, codec_vendor.audio_send_codecs().codecs());
    EXPECT_EQ(recv_codecs, codec_vendor.audio_recv_codecs().codecs());
    EXPECT_EQ(sendrecv_codecs, codec_vendor.audio_sendrecv_codecs().codecs());
  }

  // Test empty send codecs list
  CodecList no_codecs;
  media_engine.SetAudioSendCodecs(no_codecs.codecs());
  media_engine.SetAudioRecvCodecs(recv_codecs);
  {
    CodecVendor codec_vendor(&media_engine, false, trials);
    EXPECT_EQ(no_codecs.codecs(), codec_vendor.audio_send_codecs().codecs());
    EXPECT_EQ(recv_codecs, codec_vendor.audio_recv_codecs().codecs());
    EXPECT_EQ(no_codecs.codecs(),
              codec_vendor.audio_sendrecv_codecs().codecs());
  }

  // Test empty recv codecs list
  media_engine.SetAudioSendCodecs(send_codecs);
  media_engine.SetAudioRecvCodecs(no_codecs.codecs());
  {
    CodecVendor codec_vendor(&media_engine, false, trials);
    EXPECT_EQ(send_codecs, codec_vendor.audio_send_codecs().codecs());
    EXPECT_EQ(no_codecs.codecs(), codec_vendor.audio_recv_codecs().codecs());
    EXPECT_EQ(no_codecs.codecs(),
              codec_vendor.audio_sendrecv_codecs().codecs());
  }

  // Test all empty codec lists
  media_engine.SetAudioSendCodecs(no_codecs.codecs());
  media_engine.SetAudioRecvCodecs(no_codecs.codecs());
  {
    CodecVendor codec_vendor(&media_engine, false, trials);
    EXPECT_EQ(no_codecs, codec_vendor.audio_send_codecs());
    EXPECT_EQ(no_codecs, codec_vendor.audio_recv_codecs());
    EXPECT_EQ(no_codecs, codec_vendor.audio_sendrecv_codecs());
  }
}

TEST(CodecVendorTest, VideoRtxIsIncludedWhenAskedFor) {
  Environment env = CreateEnvironment();
  FakeMediaEngine media_engine;
  std::vector<Codec> video_codecs({
      CreateVideoCodec(97, "vp8"),
      CreateVideoRtxCodec(98, 97),
  });
  media_engine.SetVideoSendCodecs(video_codecs);
  CodecVendor codec_vendor(&media_engine, /* rtx_enabled= */ true,
                           env.field_trials());
  FakePayloadTypeSuggester pt_suggester;
  RTCErrorOr<std::vector<Codec>> offered_codecs =
      codec_vendor.GetNegotiatedCodecsForOffer(
          MediaDescriptionOptions(MediaType::VIDEO, "mid",
                                  RtpTransceiverDirection::kSendOnly, false),
          MediaSessionOptions(), nullptr, pt_suggester);
  EXPECT_THAT(offered_codecs.value(),
              Contains(Field("name", &Codec::name, "rtx")));
}

TEST(CodecVendorTest, VideoRtxIsExcludedWhenNotAskedFor) {
  Environment env = CreateEnvironment();
  FakeMediaEngine media_engine;
  std::vector<Codec> video_codecs({
      CreateVideoCodec(97, "vp8"),
      CreateVideoRtxCodec(98, 97),
  });
  media_engine.SetVideoSendCodecs(video_codecs);
  CodecVendor codec_vendor(&media_engine, /* rtx_enabled= */ false,
                           env.field_trials());
  FakePayloadTypeSuggester pt_suggester;
  RTCErrorOr<std::vector<Codec>> offered_codecs =
      codec_vendor.GetNegotiatedCodecsForOffer(
          MediaDescriptionOptions(MediaType::VIDEO, "mid",
                                  RtpTransceiverDirection::kSendOnly, false),
          MediaSessionOptions(), nullptr, pt_suggester);
  EXPECT_THAT(offered_codecs.value(),
              Not(Contains(Field("name", &Codec::name, "rtx"))));
}

TEST(CodecVendorTest, PreferencesAffectCodecChoice) {
  Environment env = CreateEnvironment();
  FakeMediaEngine media_engine;
  std::vector<Codec> video_codecs({
      CreateVideoCodec(97, "vp8"),
      CreateVideoRtxCodec(98, 97),
      CreateVideoCodec(99, "vp9"),
      CreateVideoRtxCodec(100, 99),
  });
  media_engine.SetVideoSendCodecs(video_codecs);
  CodecVendor codec_vendor(&media_engine, /* rtx_enabled= */ false,
                           env.field_trials());
  MediaDescriptionOptions options(MediaType::VIDEO, "mid",
                                  RtpTransceiverDirection::kSendOnly, false);
  options.codec_preferences = {
      ToRtpCodecCapability(CreateVideoCodec(-1, "vp9")),
  };
  FakePayloadTypeSuggester pt_suggester;

  RTCErrorOr<std::vector<Codec>> offered_codecs =
      codec_vendor.GetNegotiatedCodecsForOffer(options, MediaSessionOptions(),
                                               nullptr, pt_suggester);
  ASSERT_TRUE(offered_codecs.ok());
  EXPECT_THAT(offered_codecs.value(),
              Contains(Field("name", &Codec::name, "vp9")));
  EXPECT_THAT(offered_codecs.value(),
              Not(Contains(Field("name", &Codec::name, "vp8"))));
  EXPECT_THAT(offered_codecs.value().size(), Eq(1));
}

TEST(CodecVendorTest, GetNegotiatedCodecsForAnswerSimple) {
  Environment env = CreateEnvironment();
  FakeMediaEngine media_engine;
  std::vector<Codec> video_codecs({
      CreateVideoCodec(97, "vp8"),
      CreateVideoRtxCodec(98, 97),
      CreateVideoCodec(99, "vp9"),
      CreateVideoRtxCodec(100, 99),
  });
  media_engine.SetVideoSendCodecs(video_codecs);
  CodecVendor codec_vendor(&media_engine, /* rtx_enabled= */ true,
                           env.field_trials());
  MediaDescriptionOptions options(MediaType::VIDEO, "mid",
                                  RtpTransceiverDirection::kSendOnly, false);
  FakePayloadTypeSuggester pt_suggester;
  ContentInfo* current_content = nullptr;
  RTCErrorOr<std::vector<Codec>> answered_codecs =
      codec_vendor.GetNegotiatedCodecsForAnswer(
          options, MediaSessionOptions(), RtpTransceiverDirection::kSendOnly,
          RtpTransceiverDirection::kSendOnly, current_content, video_codecs,
          pt_suggester);
  EXPECT_THAT(answered_codecs, IsRtcOkAndHolds(video_codecs));
}

TEST(CodecVendorTest, GetNegotiatedCodecsForAnswerWithCollision) {
  Environment env = CreateEnvironment();
  FakeMediaEngine media_engine;
  std::vector<Codec> video_codecs({
      CreateVideoCodec(97, "vp8"),
      CreateVideoCodec(99, "vp9"),
      CreateVideoCodec(101, "av1"),
  });
  std::vector<Codec> remote_codecs({
      CreateVideoCodec(97, "av1"),
      CreateVideoCodec(99, "vp9"),
  });
  media_engine.SetVideoSendCodecs(video_codecs);
  CodecVendor codec_vendor(&media_engine, /* rtx_enabled= */ false,
                           env.field_trials());
  MediaDescriptionOptions options(MediaType::VIDEO, "mid",
                                  RtpTransceiverDirection::kSendOnly, false);
  FakePayloadTypeSuggester pt_suggester;
  ContentInfo* current_content = nullptr;
  RTCErrorOr<std::vector<Codec>> answered_codecs =
      codec_vendor.GetNegotiatedCodecsForAnswer(
          options, MediaSessionOptions(), RtpTransceiverDirection::kSendOnly,
          RtpTransceiverDirection::kSendOnly, current_content, remote_codecs,
          pt_suggester);
  EXPECT_THAT(answered_codecs, IsRtcOkAndHolds(remote_codecs));
}

TEST(CodecVendorMergeTest, BasicTestSetup) {
  CodecList reference_codecs;
  const std::string mid = "mid";
  CodecList merged_codecs;
  FakePayloadTypeSuggester pt_suggester;
  RTCError error =
      MergeCodecsForTesting(reference_codecs, mid, merged_codecs, pt_suggester);
  EXPECT_TRUE(error.ok());
}

TEST(CodecVendorMergeTest, IdenticalListsMergeWithNoChange) {
  CodecList reference_codecs;
  const std::string mid = "mid";
  CodecList merged_codecs;
  FakePayloadTypeSuggester pt_suggester;
  Codec some_codec = CreateVideoCodec(97, "foo");
  auto pt_or_error = pt_suggester.SuggestPayloadType(mid, some_codec);
  ASSERT_THAT(pt_or_error.value(), Eq(97));
  reference_codecs.push_back(some_codec);
  merged_codecs.push_back(some_codec);
  RTCError error =
      MergeCodecsForTesting(reference_codecs, mid, merged_codecs, pt_suggester);
  EXPECT_TRUE(error.ok());
  EXPECT_THAT(merged_codecs.size(), Eq(1));
  EXPECT_THAT(merged_codecs[0].id, Eq(97));
}

TEST(CodecVendorMergeTest, MergeRenumbersAdditionalCodecs) {
  CodecList reference_codecs;
  const std::string mid = "mid";
  CodecList merged_codecs;
  FakePayloadTypeSuggester pt_suggester;
  Codec some_codec = CreateVideoCodec(97, "foo");
  auto pt_or_error = pt_suggester.SuggestPayloadType(mid, some_codec);
  ASSERT_THAT(pt_or_error.value(), Eq(97));
  merged_codecs.push_back(some_codec);
  // Use the same PT for a reference codec. This should be renumbered.
  Codec some_other_codec = CreateVideoCodec(97, "bar");
  reference_codecs.push_back(some_other_codec);
  RTCError error =
      MergeCodecsForTesting(reference_codecs, mid, merged_codecs, pt_suggester);
  EXPECT_TRUE(error.ok());
  EXPECT_THAT(merged_codecs.size(), Eq(2));
  // Both foo and bar should be present
  EXPECT_THAT(merged_codecs.codecs(),
              UnorderedElementsAre(Field("name", &Codec::name, "foo"),
                                   Field("name", &Codec::name, "bar")));
  // Foo should retain 97
  EXPECT_THAT(merged_codecs.codecs(),
              Contains(AllOf(Field("name", &Codec::name, "foo"),
                             Field("id", &Codec::id, 97))));
  // Bar should not have 97
  EXPECT_THAT(merged_codecs.codecs(),
              Contains(AllOf(Field("name", &Codec::name, "bar"),
                             Field("id", &Codec::id, Ne(97)))));
}

TEST(CodecVendorMergeTest, MergeRenumbersRedCodecArgument) {
  CodecList reference_codecs;
  const std::string mid = "mid";
  CodecList merged_codecs;
  FakePayloadTypeSuggester pt_suggester;
  Codec some_codec = CreateAudioCodec(100, "foo", 8000, 1);
  merged_codecs.push_back(some_codec);
  // Push into "reference" with a different ID
  some_codec.id = 102;
  reference_codecs.push_back(some_codec);
  Codec red_codec = CreateAudioCodec(101, "red", 8000, 1);
  ASSERT_EQ(red_codec.GetResiliencyType(), Codec::ResiliencyType::kRed);
  red_codec.params[kCodecParamNotInNameValueFormat] = "102/102";
  reference_codecs.push_back(red_codec);
  // Merging should add the RED codec with parameter 100/100
  RTCError error =
      MergeCodecsForTesting(reference_codecs, mid, merged_codecs, pt_suggester);
  EXPECT_TRUE(error.ok());
  EXPECT_THAT(merged_codecs.size(), Eq(2));
  EXPECT_THAT(
      merged_codecs.codecs(),
      Contains(AllOf(Field("name", &Codec::name, "red"),
                     Field("params", &Codec::params,
                           ElementsAre(Pair(kCodecParamNotInNameValueFormat,
                                            "100/100"))))));
}

TEST(CodecVendorMergeTest, MergeRenumbersRedCodecArgumentAndMerges) {
  CodecList reference_codecs;
  const std::string mid = "mid";
  CodecList merged_codecs;
  FakePayloadTypeSuggester pt_suggester;
  Codec some_codec = CreateAudioCodec(100, "foo", 8000, 1);
  merged_codecs.push_back(some_codec);
  // Push into "reference" with a different ID
  some_codec.id = 102;
  reference_codecs.push_back(some_codec);
  Codec red_codec = CreateAudioCodec(101, "red", 8000, 1);
  ASSERT_EQ(red_codec.GetResiliencyType(), Codec::ResiliencyType::kRed);
  red_codec.params[kCodecParamNotInNameValueFormat] = "102/102";
  reference_codecs.push_back(red_codec);
  // Push the same red codec into `merged_codecs` with the 100 id
  red_codec.params[kCodecParamNotInNameValueFormat] = "100/100";
  merged_codecs.push_back(red_codec);
  // Merging should note the duplication and not add another codec.
  RTCError error =
      MergeCodecsForTesting(reference_codecs, mid, merged_codecs, pt_suggester);
  EXPECT_TRUE(error.ok());
  EXPECT_THAT(merged_codecs.size(), Eq(2));
  EXPECT_THAT(
      merged_codecs.codecs(),
      Contains(AllOf(Field("name", &Codec::name, "red"),
                     Field("params", &Codec::params,
                           ElementsAre(Pair(kCodecParamNotInNameValueFormat,
                                            "100/100"))))));
}

TEST(CodecVendorMergeTest, MergeWithBrokenReferenceRedErrors) {
  CodecList reference_codecs;
  const std::string mid = "mid";
  CodecList merged_codecs;
  FakePayloadTypeSuggester pt_suggester;
  Codec some_codec = CreateAudioCodec(100, "foo", 8000, 1);
  Codec red_codec = CreateAudioCodec(101, "red", 8000, 1);
  // Adds a RED codec that refers to codec 102, which does not exist.
  red_codec.params[kCodecParamNotInNameValueFormat] = "100/102";
  reference_codecs.push_back(some_codec);
  reference_codecs.push_back(red_codec);
  // The bogus RED codec should result in an error return.
  RTCError error =
      MergeCodecsForTesting(reference_codecs, mid, merged_codecs, pt_suggester);
  EXPECT_FALSE(error.ok());
  EXPECT_THAT(error.type(), Eq(RTCErrorType::INTERNAL_ERROR));
}

}  // namespace
}  // namespace webrtc
