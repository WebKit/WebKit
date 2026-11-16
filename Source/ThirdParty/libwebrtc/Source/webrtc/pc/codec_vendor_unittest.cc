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

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "api/environment/environment.h"
#include "api/field_trials.h"
#include "api/media_types.h"
#include "api/payload_type.h"
#include "api/rtc_error.h"
#include "api/rtp_parameters.h"
#include "api/rtp_transceiver_direction.h"
#include "api/test/rtc_error_matchers.h"
#include "api/video_codecs/sdp_video_format.h"
#include "call/fake_payload_type_suggester.h"
#include "media/base/codec.h"
#include "media/base/codec_list.h"
#include "media/base/fake_media_engine.h"
#include "media/base/media_constants.h"
#include "media/base/test_utils.h"
#include "pc/media_options.h"
#include "pc/rtp_parameters_conversion.h"
#include "pc/session_description.h"
#include "pc/test/full_codec_matrix.h"
#include "rtc_base/checks.h"
#include "test/create_test_environment.h"
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
using ::testing::StrCaseEq;

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

// Occupies the dynamic payload type space except for the `free_payload_types`
// lowest payload types. A peer that supports many codecs has no payload types
// to spare for codecs that it does not put in the offer or answer, so tests
// that leave room for the expected codecs only fail if a codec that should be
// excluded is assigned a payload type.
void OccupyDynamicPayloadTypes(FakePayloadTypeSuggester& pt_suggester,
                               MediaType media_type,
                               int free_payload_types) {
  int filler = 0;
  int left_free = 0;
  for (auto [first, last] : {std::pair{35, 63}, std::pair{96, 127}}) {
    for (int pt = first; pt <= last; ++pt) {
      if (left_free < free_payload_types) {
        ++left_free;
        continue;
      }
      std::string name = absl::StrCat("filler", filler++);
      pt_suggester.AddLocalMapping("filler", PayloadType(pt),
                                   media_type == MediaType::AUDIO
                                       ? CreateAudioCodec(pt, name, 16000, 1)
                                       : CreateVideoCodec(pt, name));
    }
  }
}

// Occupies the payload types in [`first`, `last`] with codecs that the media
// engine does not know about, as an endpoint whose peer has munged codecs of
// its own into the session description would have done.
void OccupyPayloadTypeRange(FakePayloadTypeSuggester& pt_suggester,
                            MediaType media_type,
                            int first,
                            int last) {
  for (int pt = first; pt <= last; ++pt) {
    std::string name = absl::StrCat("filler", pt);
    pt_suggester.AddLocalMapping("filler", PayloadType(pt),
                                 media_type == MediaType::AUDIO
                                     ? CreateAudioCodec(pt, name, 16000, 1)
                                     : CreateVideoCodec(pt, name));
  }
}

TEST(CodecVendorTest, TestSetAudioCodecs) {
  FieldTrials trials =
      CreateTestFieldTrials("WebRTC-PayloadTypesInTransport/Disabled/");
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
    EXPECT_EQ(no_codecs.codecs(), codec_vendor.audio_send_codecs().codecs());
    EXPECT_EQ(no_codecs.codecs(), codec_vendor.audio_recv_codecs().codecs());
    EXPECT_EQ(no_codecs.codecs(),
              codec_vendor.audio_sendrecv_codecs().codecs());
  }
}

TEST(CodecVendorTest, VideoRtxIsIncludedWhenAskedFor) {
  Environment env = CreateTestEnvironment(
      {.field_trials = "WebRTC-PayloadTypesInTransport/Disabled/"});
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
  Environment env = CreateTestEnvironment(
      {.field_trials = "WebRTC-PayloadTypesInTransport/Disabled/"});
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
  Environment env = CreateTestEnvironment(
      {.field_trials = "WebRTC-PayloadTypesInTransport/Disabled/"});
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
      ToRtpCodecCapability(CreateVideoCodec(PayloadType::NotSet(), "vp9")),
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

// Regression test for a crash in re-offers: the codecs of the previous local
// description used to be added to the offer with the payload types they had
// then, while the remaining codecs were added with the payload types of the
// newly merged codec list. If a payload type had been reassigned to another
// codec in between, the two disagreed and the offer ended up with one payload
// type used by two codecs.
TEST(CodecVendorTest, ReOfferUsesPayloadTypesOfMergedCodecList) {
  Environment env = CreateTestEnvironment(
      {.field_trials = "WebRTC-PayloadTypesInTransport/Disabled/"});
  FakeMediaEngine media_engine;
  std::vector<Codec> video_codecs({
      CreateVideoCodec(97, "vp8"),
      CreateVideoCodec(99, "vp9"),
  });
  media_engine.SetVideoSendCodecs(video_codecs);
  CodecVendor codec_vendor(&media_engine, /* rtx_enabled= */ false,
                           env.field_trials());
  MediaDescriptionOptions options(MediaType::VIDEO, "mid",
                                  RtpTransceiverDirection::kSendOnly, false);
  FakePayloadTypeSuggester pt_suggester;
  // Record mappings where vp8 has moved to payload type 120, and the payload
  // type it used to have (97) now belongs to vp9.
  ASSERT_TRUE(
      pt_suggester.AddLocalMapping("mid", 120, CreateVideoCodec(120, "vp8"))
          .ok());
  ASSERT_TRUE(
      pt_suggester.AddLocalMapping("mid", 97, CreateVideoCodec(97, "vp9"))
          .ok());

  // The previous local description still has vp8 on payload type 97.
  auto video_description = std::make_unique<VideoContentDescription>();
  video_description->set_codecs({CreateVideoCodec(97, "vp8")});
  ContentInfo current_content(MediaProtocolType::kRtp, "mid",
                              std::move(video_description));

  RTCErrorOr<std::vector<Codec>> offered_codecs =
      codec_vendor.GetNegotiatedCodecsForOffer(options, MediaSessionOptions(),
                                               &current_content, pt_suggester);
  ASSERT_TRUE(offered_codecs.ok());
  // Both codecs are offered, each with the payload type of the merged codec
  // list, so no payload type is used twice.
  EXPECT_THAT(offered_codecs.value(),
              ElementsAre(AllOf(Field("name", &Codec::name, "vp8"),
                                Field("id", &Codec::id, 120)),
                          AllOf(Field("name", &Codec::name, "vp9"),
                                Field("id", &Codec::id, 97))));
}

TEST(CodecVendorTest, GetNegotiatedCodecsForAnswerSimple) {
  Environment env = CreateTestEnvironment(
      {.field_trials = "WebRTC-PayloadTypesInTransport/Disabled/"});
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
  Environment env = CreateTestEnvironment(
      {.field_trials = "WebRTC-PayloadTypesInTransport/Disabled/"});
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
  if (CreateTestFieldTrials().IsEnabled("WebRTC-PayloadTypesInTransport")) {
    GTEST_SKIP();
  }
  CodecList reference_codecs;
  const std::string mid = "mid";
  CodecList merged_codecs;
  FakePayloadTypeSuggester pt_suggester;
  RTCError error =
      MergeCodecsForTesting(reference_codecs, mid, merged_codecs, pt_suggester);
  EXPECT_TRUE(error.ok());
}

TEST(CodecVendorMergeTest, IdenticalListsMergeWithNoChange) {
  if (CreateTestFieldTrials().IsEnabled("WebRTC-PayloadTypesInTransport")) {
    GTEST_SKIP();
  }
  CodecList reference_codecs;
  const std::string mid = "mid";
  CodecList merged_codecs;
  FakePayloadTypeSuggester pt_suggester;
  Codec some_codec = CreateVideoCodec(97, "foo");
  RTCErrorOr<PayloadType> pt_or_error =
      pt_suggester.SuggestPayloadType(mid, some_codec, false);
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
  if (CreateTestFieldTrials().IsEnabled("WebRTC-PayloadTypesInTransport")) {
    GTEST_SKIP();
  }
  CodecList reference_codecs;
  const std::string mid = "mid";
  CodecList merged_codecs;
  FakePayloadTypeSuggester pt_suggester;
  Codec some_codec = CreateVideoCodec(97, "foo");
  RTCErrorOr<PayloadType> pt_or_error =
      pt_suggester.SuggestPayloadType(mid, some_codec, false);
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
  if (CreateTestFieldTrials().IsEnabled("WebRTC-PayloadTypesInTransport")) {
    GTEST_SKIP();
  }
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
  red_codec.SetParam(kCodecParamNotInNameValueFormat, "102/102");
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
  if (CreateTestFieldTrials().IsEnabled("WebRTC-PayloadTypesInTransport")) {
    GTEST_SKIP();
  }
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
  red_codec.SetParam(kCodecParamNotInNameValueFormat, "102/102");
  reference_codecs.push_back(red_codec);
  // Push the same red codec into `merged_codecs` with the 100 id
  red_codec.SetParam(kCodecParamNotInNameValueFormat, "100/100");
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
  if (CreateTestFieldTrials().IsEnabled("WebRTC-PayloadTypesInTransport")) {
    GTEST_SKIP();
  }
  CodecList reference_codecs;
  const std::string mid = "mid";
  CodecList merged_codecs;
  FakePayloadTypeSuggester pt_suggester;
  Codec some_codec = CreateAudioCodec(100, "foo", 8000, 1);
  Codec red_codec = CreateAudioCodec(101, "red", 8000, 1);
  // Adds a RED codec that refers to codec 102, which does not exist.
  red_codec.SetParam(kCodecParamNotInNameValueFormat, "100/102");
  reference_codecs.push_back(some_codec);
  reference_codecs.push_back(red_codec);
  // The bogus RED codec should result in an error return.
  RTCError error =
      MergeCodecsForTesting(reference_codecs, mid, merged_codecs, pt_suggester);
  EXPECT_FALSE(error.ok());
  EXPECT_THAT(error.type(), Eq(RTCErrorType::INTERNAL_ERROR));
}

TEST(CodecVendorMergeTest, MergeWithCollisionPicksFromTop) {
  if (CreateTestFieldTrials().IsEnabled("WebRTC-PayloadTypesInTransport")) {
    GTEST_SKIP();
  }
  CodecList reference_codecs;
  const std::string mid = "mid";
  CodecList merged_codecs;
  FakePayloadTypeSuggester pt_suggester;
  // Existing codec with PT 97
  Codec some_codec = CreateVideoCodec(97, "foo");
  merged_codecs.push_back(some_codec);
  RTC_CHECK(pt_suggester.AddLocalMapping(mid, 97, some_codec).ok());

  // New codec in reference that also wants PT 97
  Codec some_other_codec = CreateVideoCodec(97, "bar");
  reference_codecs.push_back(some_other_codec);

  // When merging with pick_from_top_of_range = true, it should pick 127
  RTCError error =
      MergeCodecsForTesting(reference_codecs, mid, merged_codecs, pt_suggester,
                            /*pick_from_top_of_range=*/true);
  EXPECT_TRUE(error.ok());
  EXPECT_THAT(merged_codecs.size(), Eq(2));
  EXPECT_THAT(merged_codecs.codecs(),
              Contains(AllOf(Field("name", &Codec::name, "bar"),
                             Field("id", &Codec::id, 127))));
}

#if GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
// A payload type conflict while merging means that codecs from different
// payload type mappings have been combined, which is an internal error rather
// than something an application can cause. It is fatal in debug builds; in
// release builds the conflicting codec is left out of the list.
TEST(CodecVendorMergeDeathTest, MergeAssertsOnPayloadTypeConflict) {
  if (CreateTestFieldTrials().IsEnabled("WebRTC-PayloadTypesInTransport")) {
    GTEST_SKIP();
  }
  CodecList reference_codecs;
  const std::string mid = "mid";
  CodecList merged_codecs;
  FakePayloadTypeSuggester pt_suggester;
  // Existing codec with PT 97.
  Codec some_codec = CreateVideoCodec(97, "foo");
  merged_codecs.push_back(some_codec);
  RTC_CHECK(pt_suggester.AddLocalMapping(mid, 97, some_codec).ok());

  // Force the suggester to hand out the payload type of "foo" for "bar", so
  // that the merge ends up with two codecs claiming the same payload type.
  pt_suggester.SetSuggestion(mid, "bar", PayloadType(97));
  reference_codecs.push_back(CreateVideoCodec(97, "bar"));

#if RTC_DCHECK_IS_ON
  EXPECT_DEATH(
      MergeCodecsForTesting(reference_codecs, mid, merged_codecs, pt_suggester)
          .ok(),
      "codec_vendor.cc");
#else
  RTCError error =
      MergeCodecsForTesting(reference_codecs, mid, merged_codecs, pt_suggester);
  EXPECT_TRUE(error.ok());
  // "bar" was not added to the list.
  EXPECT_THAT(merged_codecs.size(), Eq(1));
#endif
}
#endif

TEST(CodecVendorTest, ModifyVideoCodecsReplacesCodec) {
  FieldTrials trials =
      CreateTestFieldTrials("WebRTC-PayloadTypesInTransport/Disabled/");
  FakeMediaEngine media_engine;
  std::vector<Codec> video_codecs({
      CreateVideoCodec(97, "vp8"),
      CreateVideoCodec(99, "vp9"),
  });
  media_engine.SetVideoSendCodecs(video_codecs);
  media_engine.SetVideoRecvCodecs(video_codecs);

  CodecVendor codec_vendor(&media_engine, false, trials);

  Codec original_codec = video_codecs[0];
  Codec modified_codec = original_codec;
  modified_codec.name = "modified_name";

  Codec second_codec = video_codecs[1];

  std::vector<std::pair<Codec, Codec>> changes;
  changes.push_back({original_codec, modified_codec});

  codec_vendor.ModifyVideoCodecs(changes);

  const CodecList& new_send_codecs = codec_vendor.video_send_codecs();
  EXPECT_THAT(new_send_codecs.codecs(), Contains(modified_codec));
  EXPECT_THAT(new_send_codecs.codecs(), Not(Contains(original_codec)));

  // Check that the second codec is NOT changed.
  EXPECT_THAT(new_send_codecs.codecs(), Contains(second_codec));
}

// Once every dynamic payload type is taken, offer creation must report the
// exhaustion to the caller. Silently omitting the codecs that could not be
// assigned a payload type produces an offer that mixes two payload type
// mappings, which is how a BUNDLE group ends up using one payload type for two
// different codecs.
TEST(CodecVendorTest, OfferFailsWhenPayloadTypeSpaceIsExhausted) {
  Environment env = CreateTestEnvironment(
      {.field_trials = "WebRTC-PayloadTypesInTransport/Disabled/"});
  FakeMediaEngine media_engine;
  media_engine.SetVideoSendCodecs({CreateVideoCodec(97, "vp8")});
  media_engine.SetVideoRecvCodecs({CreateVideoCodec(97, "vp8")});
  CodecVendor codec_vendor(&media_engine, /* rtx_enabled= */ false,
                           env.field_trials());

  // Occupy the whole dynamic payload type space with codecs that the media
  // engine does not offer, so that no payload type is left for "vp8".
  FakePayloadTypeSuggester pt_suggester;
  OccupyDynamicPayloadTypes(pt_suggester, MediaType::VIDEO,
                            /*free_payload_types=*/0);

  RTCErrorOr<std::vector<Codec>> offered_codecs =
      codec_vendor.GetNegotiatedCodecsForOffer(
          MediaDescriptionOptions(MediaType::VIDEO, "mid",
                                  RtpTransceiverDirection::kSendOnly, false),
          MediaSessionOptions(), nullptr, pt_suggester);
  EXPECT_THAT(offered_codecs.error(),
              IsRtcErrorWithType(RTCErrorType::RESOURCE_EXHAUSTED));
}

// Codecs that the direction of the media section rules out never reach the
// offer, so they must not be assigned a payload type either. A peer that
// supports many codecs has no payload types to spare for codecs it does not
// offer.
TEST(CodecVendorTest, OfferDoesNotAssignPayloadTypesToUnofferedCodecs) {
  Environment env = CreateTestEnvironment(
      {.field_trials = "WebRTC-PayloadTypesInTransport/Disabled/"});
  FakeMediaEngine media_engine;
  media_engine.SetVideoSendCodecs({CreateVideoCodec(96, "vp8")});
  // A send only media section cannot offer the receive-only codecs.
  media_engine.SetVideoRecvCodecs({CreateVideoCodec(96, "vp8"),
                                   CreateVideoCodec(97, "vp9"),
                                   CreateVideoCodec(98, "av1")});
  CodecVendor codec_vendor(&media_engine, /* rtx_enabled= */ false,
                           env.field_trials());

  // Occupy the dynamic payload type space except for a single payload type,
  // which leaves room for the one codec that the offer can contain.
  FakePayloadTypeSuggester pt_suggester;
  OccupyDynamicPayloadTypes(pt_suggester, MediaType::VIDEO,
                            /*free_payload_types=*/1);

  RTCErrorOr<std::vector<Codec>> offered_codecs =
      codec_vendor.GetNegotiatedCodecsForOffer(
          MediaDescriptionOptions(MediaType::VIDEO, "mid",
                                  RtpTransceiverDirection::kSendOnly, false),
          MediaSessionOptions(), nullptr, pt_suggester);
  ASSERT_TRUE(offered_codecs.ok()) << offered_codecs.error().message();
  EXPECT_THAT(offered_codecs.value(),
              ElementsAre(Field(&Codec::name, Eq("vp8"))));
}

// Runs a test both with the legacy payload type assignment and with the
// redesigned one that the WebRTC-PayloadTypesInTransport field trial enables.
class CodecVendorPayloadTypeTest : public ::testing::TestWithParam<bool> {
 protected:
  bool payload_types_in_transport() const { return GetParam(); }

  const Environment env_ = CreateTestEnvironment(
      {.field_trials = absl::string_view(
           GetParam() ? "WebRTC-PayloadTypesInTransport/Enabled/"
                      : "WebRTC-PayloadTypesInTransport/Disabled/")});
};

// Codecs excluded by transceiver codec preferences must not be assigned a
// payload type when creating an offer.
TEST_P(CodecVendorPayloadTypeTest,
       OfferDoesNotAssignPayloadTypesToExcludedCodecs) {
  FakeMediaEngine media_engine;
  // "av1" comes last, so it only gets a payload type if the codecs that the
  // codec preferences exclude do not take it first.
  media_engine.SetVideoCodecs({CreateVideoCodec(96, "vp8"),
                               CreateVideoCodec(97, "vp9"),
                               CreateVideoCodec(98, "av1")});
  CodecVendor codec_vendor(&media_engine, /* rtx_enabled= */ false,
                           env_.field_trials());

  FakePayloadTypeSuggester pt_suggester;
  OccupyDynamicPayloadTypes(pt_suggester, MediaType::VIDEO,
                            /*free_payload_types=*/1);

  MediaDescriptionOptions options(MediaType::VIDEO, "mid",
                                  RtpTransceiverDirection::kSendRecv, false);
  RtpCodecCapability pref;
  pref.name = "av1";
  pref.kind = MediaType::VIDEO;
  pref.clock_rate = 90000;
  options.codec_preferences = {pref};

  RTCErrorOr<std::vector<Codec>> offered_codecs =
      codec_vendor.GetNegotiatedCodecsForOffer(options, MediaSessionOptions(),
                                               nullptr, pt_suggester);
  ASSERT_TRUE(offered_codecs.ok()) << offered_codecs.error().message();
  EXPECT_THAT(offered_codecs.value(),
              ElementsAre(Field(&Codec::name, Eq("av1"))));
}

// RED codecs whose primary codecs are excluded by transceiver codec
// preferences must not be assigned a payload type when creating an offer.
TEST(CodecVendorTest, OfferDoesNotAssignPayloadTypesToDanglingRedCodecs) {
  Environment env = CreateTestEnvironment(
      {.field_trials = "WebRTC-PayloadTypesInTransport/Disabled/"});
  FakeMediaEngine media_engine;
  media_engine.SetAudioCodecs({CreateAudioCodec(111, "opus", 48000, 2),
                               CreateRedAudioCodec("111"),
                               CreateAudioCodec(102, "G722", 16000, 1)});
  CodecVendor codec_vendor(&media_engine, /* rtx_enabled= */ false,
                           env.field_trials());

  FakePayloadTypeSuggester pt_suggester;
  OccupyDynamicPayloadTypes(pt_suggester, MediaType::AUDIO,
                            /*free_payload_types=*/1);

  MediaDescriptionOptions options(MediaType::AUDIO, "mid",
                                  RtpTransceiverDirection::kSendRecv, false);
  RtpCodecCapability red_pref;
  red_pref.name = "red";
  red_pref.kind = MediaType::AUDIO;
  red_pref.clock_rate = 48000;
  red_pref.num_channels = 2;
  RtpCodecCapability g722_pref;
  g722_pref.name = "G722";
  g722_pref.kind = MediaType::AUDIO;
  g722_pref.clock_rate = 16000;
  g722_pref.num_channels = 1;
  options.codec_preferences = {red_pref, g722_pref};

  RTCErrorOr<std::vector<Codec>> offered_codecs =
      codec_vendor.GetNegotiatedCodecsForOffer(options, MediaSessionOptions(),
                                               nullptr, pt_suggester);
  ASSERT_TRUE(offered_codecs.ok()) << offered_codecs.error().message();
  EXPECT_THAT(offered_codecs.value(),
              ElementsAre(Field(&Codec::name, Eq("G722"))));
}

// Codecs excluded by `codecs_to_include` must not be assigned a payload type
// when creating an offer.
TEST(CodecVendorTest, OfferRespectsCodecsToInclude) {
  Environment env = CreateTestEnvironment(
      {.field_trials = "WebRTC-PayloadTypesInTransport/Disabled/"});
  FakeMediaEngine media_engine;
  media_engine.SetVideoCodecs({CreateVideoCodec(96, "vp8"),
                               CreateVideoCodec(97, "vp9"),
                               CreateVideoCodec(98, "av1")});
  CodecVendor codec_vendor(&media_engine, /* rtx_enabled= */ false,
                           env.field_trials());

  FakePayloadTypeSuggester pt_suggester;
  OccupyDynamicPayloadTypes(pt_suggester, MediaType::VIDEO,
                            /*free_payload_types=*/1);

  MediaDescriptionOptions options(MediaType::VIDEO, "mid",
                                  RtpTransceiverDirection::kSendRecv, false);
  options.codecs_to_include = {CreateVideoCodec(96, "vp8")};

  RTCErrorOr<std::vector<Codec>> offered_codecs =
      codec_vendor.GetNegotiatedCodecsForOffer(options, MediaSessionOptions(),
                                               nullptr, pt_suggester);
  ASSERT_TRUE(offered_codecs.ok()) << offered_codecs.error().message();
  EXPECT_THAT(offered_codecs.value(),
              ElementsAre(Field(&Codec::name, Eq("vp8"))));
}

// When WebRTC-PayloadTypesInTransport is enabled, unoffered codecs must not be
// assigned a payload type when creating an offer.
TEST(CodecVendorTest, OfferRespectsDirectionWithPayloadTypesInTransport) {
  Environment env = CreateTestEnvironment(
      {.field_trials = "WebRTC-PayloadTypesInTransport/Enabled/"});
  FakeMediaEngine media_engine;
  media_engine.SetVideoSendCodecs({CreateVideoCodec(96, "vp8")});
  media_engine.SetVideoRecvCodecs({CreateVideoCodec(96, "vp8"),
                                   CreateVideoCodec(97, "vp9"),
                                   CreateVideoCodec(98, "av1")});
  CodecVendor codec_vendor(&media_engine, /* rtx_enabled= */ false,
                           env.field_trials());

  FakePayloadTypeSuggester pt_suggester;
  OccupyDynamicPayloadTypes(pt_suggester, MediaType::VIDEO,
                            /*free_payload_types=*/1);

  RTCErrorOr<std::vector<Codec>> offered_codecs =
      codec_vendor.GetNegotiatedCodecsForOffer(
          MediaDescriptionOptions(MediaType::VIDEO, "mid",
                                  RtpTransceiverDirection::kSendOnly, false),
          MediaSessionOptions(), nullptr, pt_suggester);
  ASSERT_TRUE(offered_codecs.ok()) << offered_codecs.error().message();
  EXPECT_THAT(offered_codecs.value(),
              ElementsAre(Field(&Codec::name, Eq("vp8"))));
}

// Comfort noise codecs must not be assigned a payload type when the
// application does not want them in the offer.
TEST_P(CodecVendorPayloadTypeTest,
       OfferDoesNotAssignPayloadTypesToComfortNoise) {
  FakeMediaEngine media_engine;
  // "CN" comes first, so it takes a payload type that is needed by the codecs
  // that follow it unless it is excluded from the offer.
  media_engine.SetAudioCodecs({CreateAudioCodec(107, "CN", 48000, 1),
                               CreateAudioCodec(111, "opus", 48000, 2)});
  CodecVendor codec_vendor(&media_engine, /* rtx_enabled= */ false,
                           env_.field_trials());

  // Leave room for exactly the codecs that the offer is expected to contain:
  // "opus", plus the "red" and "telephone-event" codecs that accompany it in
  // the redesigned payload type assignment.
  FakePayloadTypeSuggester pt_suggester;
  OccupyDynamicPayloadTypes(
      pt_suggester, MediaType::AUDIO,
      /*free_payload_types=*/payload_types_in_transport() ? 3 : 1);

  MediaSessionOptions session_options;
  session_options.vad_enabled = false;
  RTCErrorOr<std::vector<Codec>> offered_codecs =
      codec_vendor.GetNegotiatedCodecsForOffer(
          MediaDescriptionOptions(MediaType::AUDIO, "mid",
                                  RtpTransceiverDirection::kSendRecv, false),
          session_options, nullptr, pt_suggester);
  ASSERT_TRUE(offered_codecs.ok()) << offered_codecs.error().message();
  EXPECT_THAT(offered_codecs.value(),
              Not(Contains(Field(&Codec::name, StrCaseEq("CN")))));
  EXPECT_THAT(offered_codecs.value(),
              Contains(Field(&Codec::name, Eq("opus"))));
}

// Codecs excluded by transceiver codec preferences must not be assigned a
// payload type when creating an answer.
TEST_P(CodecVendorPayloadTypeTest,
       AnswerDoesNotAssignPayloadTypesToExcludedCodecs) {
  FakeMediaEngine media_engine;
  // "av1" comes last, so it only gets a payload type if the codecs that the
  // codec preferences exclude do not take it first.
  media_engine.SetVideoCodecs({CreateVideoCodec(96, "vp8"),
                               CreateVideoCodec(97, "vp9"),
                               CreateVideoCodec(98, "av1")});
  CodecVendor codec_vendor(&media_engine, /* rtx_enabled= */ false,
                           env_.field_trials());

  FakePayloadTypeSuggester pt_suggester;
  OccupyDynamicPayloadTypes(pt_suggester, MediaType::VIDEO,
                            /*free_payload_types=*/1);

  MediaDescriptionOptions options(MediaType::VIDEO, "mid",
                                  RtpTransceiverDirection::kSendRecv, false);
  RtpCodecCapability pref;
  pref.name = "av1";
  pref.kind = MediaType::VIDEO;
  pref.clock_rate = 90000;
  options.codec_preferences = {pref};

  std::vector<Codec> codecs_from_offer = {CreateVideoCodec(96, "vp8"),
                                          CreateVideoCodec(98, "av1")};
  RTCErrorOr<std::vector<Codec>> answered_codecs =
      codec_vendor.GetNegotiatedCodecsForAnswer(
          options, MediaSessionOptions(), RtpTransceiverDirection::kSendRecv,
          RtpTransceiverDirection::kSendRecv, /*current_content=*/nullptr,
          codecs_from_offer, pt_suggester);
  ASSERT_TRUE(answered_codecs.ok()) << answered_codecs.error().message();
  EXPECT_THAT(answered_codecs.value(),
              ElementsAre(Field(&Codec::name, Eq("av1"))));
}

// Comfort noise codecs must not be assigned a payload type when the
// application does not want them in the answer.
TEST(CodecVendorTest, AnswerDoesNotAssignPayloadTypesToComfortNoise) {
  Environment env = CreateTestEnvironment(
      {.field_trials = "WebRTC-PayloadTypesInTransport/Disabled/"});
  FakeMediaEngine media_engine;
  // "CN" comes first, so it takes the last free payload type unless it is
  // excluded from the answer.
  media_engine.SetAudioCodecs({CreateAudioCodec(107, "CN", 48000, 1),
                               CreateAudioCodec(111, "opus", 48000, 2)});
  CodecVendor codec_vendor(&media_engine, /* rtx_enabled= */ false,
                           env.field_trials());

  FakePayloadTypeSuggester pt_suggester;
  OccupyDynamicPayloadTypes(pt_suggester, MediaType::AUDIO,
                            /*free_payload_types=*/1);

  MediaSessionOptions session_options;
  session_options.vad_enabled = false;
  std::vector<Codec> codecs_from_offer = {
      CreateAudioCodec(111, "opus", 48000, 2)};
  RTCErrorOr<std::vector<Codec>> answered_codecs =
      codec_vendor.GetNegotiatedCodecsForAnswer(
          MediaDescriptionOptions(MediaType::AUDIO, "mid",
                                  RtpTransceiverDirection::kSendRecv, false),
          session_options, RtpTransceiverDirection::kSendRecv,
          RtpTransceiverDirection::kSendRecv, /*current_content=*/nullptr,
          codecs_from_offer, pt_suggester);
  ASSERT_TRUE(answered_codecs.ok()) << answered_codecs.error().message();
  EXPECT_THAT(answered_codecs.value(),
              ElementsAre(Field(&Codec::name, Eq("opus"))));
}

// The forward error correction mechanisms that the transceiver codec
// preferences do not include must not be assigned a payload type.
TEST(CodecVendorWithPayloadTypesInTransportTest,
     OfferDoesNotAssignPayloadTypesToExcludedFecCodecs) {
  Environment env = CreateTestEnvironment(
      {.field_trials = "WebRTC-PayloadTypesInTransport/Enabled/"});
  FakeMediaEngine media_engine;
  // The FEC codecs make the codec configuration of "vp8" carry ULPFEC and
  // FlexFEC, neither of which the codec preferences below ask for.
  media_engine.SetVideoCodecs({CreateVideoCodec(96, "vp8"),
                               CreateVideoCodec(97, kUlpfecCodecName),
                               CreateVideoCodec(98, kFlexfecCodecName)});
  CodecVendor codec_vendor(&media_engine, /* rtx_enabled= */ false,
                           env.field_trials());

  // Leave room for "vp8" only, so that the offer can only be created if the
  // FEC codecs are left without a payload type.
  FakePayloadTypeSuggester pt_suggester;
  OccupyDynamicPayloadTypes(pt_suggester, MediaType::VIDEO,
                            /*free_payload_types=*/1);

  MediaDescriptionOptions options(MediaType::VIDEO, "mid",
                                  RtpTransceiverDirection::kSendRecv, false);
  RtpCodecCapability pref;
  pref.name = "vp8";
  pref.kind = MediaType::VIDEO;
  pref.clock_rate = 90000;
  options.codec_preferences = {pref};

  RTCErrorOr<std::vector<Codec>> offered_codecs =
      codec_vendor.GetNegotiatedCodecsForOffer(options, MediaSessionOptions(),
                                               nullptr, pt_suggester);
  ASSERT_TRUE(offered_codecs.ok()) << offered_codecs.error().message();
  EXPECT_THAT(offered_codecs.value(),
              ElementsAre(Field(&Codec::name, Eq("vp8"))));
}

// Builds the transceiver codec preferences of an application that wants VP8
// and AV1 only, with retransmission for both.
std::vector<RtpCodecCapability> Vp8AndAv1Preferences() {
  const std::vector<SdpVideoFormat> formats = test::HardwareVideoFormats();
  RtpCodecCapability rtx;
  rtx.name = kRtxCodecName;
  rtx.kind = MediaType::VIDEO;
  rtx.clock_rate = kVideoCodecClockrate;
  return {
      ToRtpCodecCapability(
          CreateVideoCodec(PayloadType::NotSet(), formats.front())),
      ToRtpCodecCapability(
          CreateVideoCodec(PayloadType::NotSet(), formats.back())),
      rtx,
  };
}

// The offer of a peer that only has the two codecs that the preferences above
// ask for.
std::vector<Codec> Vp8AndAv1Offer() {
  const std::vector<SdpVideoFormat> formats = test::HardwareVideoFormats();
  return {CreateVideoCodec(120, formats.front()), CreateVideoRtxCodec(121, 120),
          CreateVideoCodec(122, formats.back()), CreateVideoRtxCodec(123, 122)};
}

// Regression test for https://issues.webrtc.org/564178627, where an endpoint
// with hardware codecs ran out of dynamic payload types: it supports 14 video
// formats, each with a retransmission codec of its own, the lower range that
// AV1 prefers has been used up, and the application has munged codecs of its
// own into the upper range. The payload types that are left are only enough
// for the codecs that the answer can contain, so AV1, which comes last, only
// survives if the codecs that the transceiver's codec preferences exclude do
// not take them first.
TEST_P(CodecVendorPayloadTypeTest,
       AnswerWithHardwareCodecMatrixUnderPayloadTypePressure) {
  FakeMediaEngine media_engine;
  media_engine.SetVideoCodecs(test::HardwareVideoCodecs());
  CodecVendor codec_vendor(&media_engine, /* rtx_enabled= */ true,
                           env_.field_trials());

  // Eight payload types are left, which is enough for the four codecs of the
  // answer but nowhere near enough for the 28 codecs of the matrix.
  FakePayloadTypeSuggester pt_suggester;
  OccupyPayloadTypeRange(pt_suggester, MediaType::VIDEO, 35, 63);
  OccupyPayloadTypeRange(pt_suggester, MediaType::VIDEO, 96, 119);

  MediaDescriptionOptions options(MediaType::VIDEO, "mid",
                                  RtpTransceiverDirection::kSendRecv, false);
  options.codec_preferences = Vp8AndAv1Preferences();

  RTCErrorOr<std::vector<Codec>> answered_codecs =
      codec_vendor.GetNegotiatedCodecsForAnswer(
          options, MediaSessionOptions(), RtpTransceiverDirection::kSendRecv,
          RtpTransceiverDirection::kSendRecv, /*current_content=*/nullptr,
          Vp8AndAv1Offer(), pt_suggester);
  ASSERT_TRUE(answered_codecs.ok()) << answered_codecs.error().message();
  EXPECT_THAT(answered_codecs.value(),
              Contains(Field(&Codec::name, StrCaseEq("VP8"))));
  EXPECT_THAT(answered_codecs.value(),
              Contains(Field(&Codec::name, StrCaseEq("AV1"))));
}

INSTANTIATE_TEST_SUITE_P(PayloadTypesInTransport,
                         CodecVendorPayloadTypeTest,
                         ::testing::Bool(),
                         [](const ::testing::TestParamInfo<bool>& info)
                             -> std::string {
                           return info.param ? "Enabled" : "Disabled";
                         });

}  // namespace
}  // namespace webrtc
