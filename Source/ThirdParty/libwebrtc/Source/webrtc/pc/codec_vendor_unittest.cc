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

#include <cstddef>
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

using testing::Contains;
using testing::Eq;
using testing::Field;

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
  CodecVendor codec_vendor(nullptr, false, trials);
  std::vector<Codec> send_codecs = MAKE_VECTOR(kAudioCodecs1);
  std::vector<Codec> recv_codecs = MAKE_VECTOR(kAudioCodecs2);

  // The merged list of codecs should contain any send codecs that are also
  // nominally in the receive codecs list. Payload types should be picked from
  // the send codecs and a number-of-channels of 0 and 1 should be equivalent
  // (set to 1). This equals what happens when the send codecs are used in an
  // offer and the receive codecs are used in the following answer.
  const std::vector<Codec> sendrecv_codecs = MAKE_VECTOR(kAudioCodecsAnswer);
  CodecList no_codecs;

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
  codec_vendor.set_audio_codecs(CodecList::CreateFromTrustedData(send_codecs),
                                CodecList::CreateFromTrustedData(recv_codecs));
  EXPECT_EQ(send_codecs, codec_vendor.audio_send_codecs().codecs());
  EXPECT_EQ(recv_codecs, codec_vendor.audio_recv_codecs().codecs());
  EXPECT_EQ(sendrecv_codecs, codec_vendor.audio_sendrecv_codecs().codecs());

  // Test empty send codecs list
  codec_vendor.set_audio_codecs(no_codecs,
                                CodecList::CreateFromTrustedData(recv_codecs));
  EXPECT_EQ(no_codecs.codecs(), codec_vendor.audio_send_codecs().codecs());
  EXPECT_EQ(recv_codecs, codec_vendor.audio_recv_codecs().codecs());
  EXPECT_EQ(no_codecs.codecs(), codec_vendor.audio_sendrecv_codecs().codecs());

  // Test empty recv codecs list
  codec_vendor.set_audio_codecs(CodecList::CreateFromTrustedData(send_codecs),
                                no_codecs);
  EXPECT_EQ(send_codecs, codec_vendor.audio_send_codecs().codecs());
  EXPECT_EQ(no_codecs.codecs(), codec_vendor.audio_recv_codecs().codecs());
  EXPECT_EQ(no_codecs.codecs(), codec_vendor.audio_sendrecv_codecs().codecs());

  // Test all empty codec lists
  codec_vendor.set_audio_codecs(no_codecs, no_codecs);
  EXPECT_EQ(no_codecs, codec_vendor.audio_send_codecs());
  EXPECT_EQ(no_codecs, codec_vendor.audio_recv_codecs());
  EXPECT_EQ(no_codecs, codec_vendor.audio_sendrecv_codecs());
}

TEST(CodecVendorTest, VideoRtxIsIncludedWhenAskedFor) {
  Environment env = CreateEnvironment();
  FakeMediaEngine media_engine;
  std::vector<Codec> video_codecs({
      CreateVideoCodec(97, "vp8"),
      CreateVideoRtxCodec(98, 97),
  });
  FakePayloadTypeSuggester pt_suggester;
  media_engine.SetVideoSendCodecs(video_codecs);
  CodecVendor codec_vendor(&media_engine, /* rtx_enabled= */ true,
                           env.field_trials());
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
  FakePayloadTypeSuggester pt_suggester;
  media_engine.SetVideoSendCodecs(video_codecs);
  CodecVendor codec_vendor(&media_engine, /* rtx_enabled= */ false,
                           env.field_trials());
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

}  // namespace
}  // namespace webrtc
