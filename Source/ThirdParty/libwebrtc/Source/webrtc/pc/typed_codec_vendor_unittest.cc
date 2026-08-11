/*
 *  Copyright 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/typed_codec_vendor.h"

#include <vector>

#include "api/field_trials.h"
#include "api/media_types.h"
#include "api/payload_type.h"
#include "media/base/codec.h"
#include "media/base/fake_media_engine.h"
#include "pc/codec_configuration.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::Contains;
using ::testing::Field;
using ::testing::Property;

TEST(TypedCodecVendorTest, VideoCodecsFromFactoryWhenTrialEnabled) {
  FieldTrials trials(
      "WebRTC-PayloadTypesInTransport/Enabled/"
      "WebRTC-FlexFEC-03-Advertised/Enabled/");
  FakeMediaEngine media_engine;
  std::vector<Codec> video_codecs({
      CreateVideoCodec(97, "vp8"),
      CreateVideoRtxCodec(98, 97),
      CreateVideoCodec(100, "red"),
      CreateVideoCodec(101, "ulpfec"),
      CreateVideoCodec(102, "flexfec-03"),
  });
  media_engine.SetVideoSendCodecs(video_codecs);

  TypedCodecVendor vendor(&media_engine, MediaType::VIDEO, /*is_sender=*/true,
                          /*rtx_enabled=*/true, trials);

  const auto& codecs = vendor.codecs().codecs();
  EXPECT_THAT(codecs, Contains(Field("name", &Codec::name, "vp8")));

  for (const auto& codec : codecs) {
    EXPECT_EQ(codec.id, PayloadType::NotSet());
  }

  const auto& configurations = vendor.configurations();
  EXPECT_THAT(configurations,
              Contains(Field("codec", &CodecConfiguration::codec,
                             Field("name", &Codec::name, "vp8"))));
  for (const auto& config : configurations) {
    if (config.codec.name == "vp8") {
      EXPECT_TRUE(config.resiliency.rtx);
      EXPECT_TRUE(config.resiliency.red);
      EXPECT_TRUE(config.resiliency.flexfec);
      // Verify feedback params (added by AddDefaultFeedbackParams)
      EXPECT_THAT(config.codec.feedback_params.params(),
                  Contains(Property(&FeedbackParam::id, "goog-remb")));
      EXPECT_THAT(config.codec.feedback_params.params(),
                  Contains(Property(&FeedbackParam::id, "transport-cc")));
    }
  }
}

TEST(TypedCodecVendorTest, VideoCodecsFromFactoryWhenResiliencyAbsent) {
  FieldTrials trials("WebRTC-PayloadTypesInTransport/Enabled/");
  FakeMediaEngine media_engine;
  std::vector<Codec> video_codecs({
      CreateVideoCodec(97, "vp8"),
  });
  media_engine.SetVideoSendCodecs(video_codecs);

  TypedCodecVendor vendor(&media_engine, MediaType::VIDEO, /*is_sender=*/true,
                          /*rtx_enabled=*/true, trials);

  const auto& configurations = vendor.configurations();
  EXPECT_THAT(configurations,
              Contains(Field("codec", &CodecConfiguration::codec,
                             Field("name", &Codec::name, "vp8"))));
  for (const auto& config : configurations) {
    if (config.codec.name == "vp8") {
      EXPECT_FALSE(config.resiliency.rtx);
      EXPECT_FALSE(config.resiliency.red);
      EXPECT_FALSE(config.resiliency.ulpfec);
      EXPECT_FALSE(config.resiliency.flexfec);
    }
  }
}

TEST(TypedCodecVendorTest, VideoCodecsLegacyWhenTrialDisabled) {
  FieldTrials trials("WebRTC-PayloadTypesInTransport/Disabled/");
  FakeMediaEngine media_engine;
  std::vector<Codec> video_codecs({
      CreateVideoCodec(97, "vp8"),
  });
  media_engine.SetVideoSendCodecs(video_codecs);

  TypedCodecVendor vendor(&media_engine, MediaType::VIDEO, /*is_sender=*/true,
                          /*rtx_enabled=*/false, trials);

  const auto& codecs = vendor.codecs().codecs();
  ASSERT_EQ(codecs.size(), 1u);
  EXPECT_EQ(codecs[0].name, "vp8");
  EXPECT_EQ(codecs[0].id, PayloadType(97));

  EXPECT_TRUE(vendor.configurations().empty());
}

TEST(TypedCodecVendorTest, AudioCodecsFromFactoryWhenTrialEnabled) {
  FieldTrials trials("WebRTC-PayloadTypesInTransport/Enabled/");
  FakeMediaEngine media_engine;
  std::vector<Codec> audio_codecs({
      CreateAudioCodec(111, "opus", 48000, 2),
      CreateAudioCodec(63, "red", 48000, 2),
  });
  media_engine.SetAudioSendCodecs(audio_codecs);

  TypedCodecVendor vendor(&media_engine, MediaType::AUDIO, /*is_sender=*/true,
                          /*rtx_enabled=*/false, trials);

  const auto& codecs = vendor.codecs().codecs();
  EXPECT_THAT(codecs, Contains(Field("name", &Codec::name, "opus")));
  EXPECT_THAT(codecs, Contains(Field("name", &Codec::name, "red")));

  for (const auto& codec : codecs) {
    EXPECT_EQ(codec.id, PayloadType::NotSet());
  }

  const auto& configurations = vendor.configurations();
  EXPECT_THAT(configurations,
              Contains(Field("codec", &CodecConfiguration::codec,
                             Field("name", &Codec::name, "opus"))));
  for (const auto& config : configurations) {
    if (config.codec.name == "opus") {
      EXPECT_TRUE(config.resiliency.red);
    }
  }
}

}  // namespace
}  // namespace webrtc
