/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_codecs/audio_encoder_factory_template.h"

#include <memory>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/audio_codecs/L16/audio_encoder_L16.h"
#include "api/audio_codecs/audio_codec_pair_id.h"
#include "api/audio_codecs/audio_encoder.h"
#include "api/audio_codecs/audio_encoder_factory.h"
#include "api/audio_codecs/audio_format.h"
#include "api/audio_codecs/g711/audio_encoder_g711.h"
#include "api/audio_codecs/g722/audio_encoder_g722.h"
#include "api/audio_codecs/ilbc/audio_encoder_ilbc.h"
#include "api/audio_codecs/opus/audio_encoder_opus.h"
#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_audio_encoder.h"
#include "test/scoped_key_value_config.h"

namespace webrtc {
namespace {

using ::testing::IsNull;
using ::testing::NiceMock;
using ::testing::Pointer;
using ::testing::Property;
using ::testing::Return;

struct BogusParams {
  static SdpAudioFormat AudioFormat() { return {"bogus", 8000, 1}; }
  static AudioCodecInfo CodecInfo() { return {8000, 1, 12345}; }
};

struct ShamParams {
  static SdpAudioFormat AudioFormat() {
    return {"sham", 16000, 2, {{"param", "value"}}};
  }
  static AudioCodecInfo CodecInfo() { return {16000, 2, 23456}; }
};

template <typename Params>
struct AudioEncoderFakeApi {
  struct Config {
    SdpAudioFormat audio_format;
  };

  static absl::optional<Config> SdpToConfig(
      const SdpAudioFormat& audio_format) {
    if (Params::AudioFormat() == audio_format) {
      Config config = {audio_format};
      return config;
    } else {
      return absl::nullopt;
    }
  }

  static void AppendSupportedEncoders(std::vector<AudioCodecSpec>* specs) {
    specs->push_back({Params::AudioFormat(), Params::CodecInfo()});
  }

  static AudioCodecInfo QueryAudioEncoder(const Config&) {
    return Params::CodecInfo();
  }

  static std::unique_ptr<AudioEncoder> MakeAudioEncoder(
      const Config&,
      int payload_type,
      absl::optional<AudioCodecPairId> /*codec_pair_id*/ = absl::nullopt) {
    auto enc = std::make_unique<testing::StrictMock<MockAudioEncoder>>();
    EXPECT_CALL(*enc, SampleRateHz())
        .WillOnce(::testing::Return(Params::CodecInfo().sample_rate_hz));
    return std::move(enc);
  }
};

// Trait to pass as template parameter to `CreateAudioEncoderFactory` with
// all the functions except the functions to create the audio encoder.
struct BaseAudioEncoderApi {
  // Create Encoders with different sample rates depending if it is created
  // through V1 or V2 method so that a test may detect which method was used.
  static constexpr int kV1SameRate = 10'000;
  static constexpr int kV2SameRate = 20'000;

  struct Config {};

  static SdpAudioFormat AudioFormat() { return {"fake", 16'000, 2, {}}; }
  static AudioCodecInfo CodecInfo() { return {16'000, 2, 23456}; }

  static absl::optional<Config> SdpToConfig(
      const SdpAudioFormat& audio_format) {
    return Config();
  }

  static void AppendSupportedEncoders(std::vector<AudioCodecSpec>* specs) {
    specs->push_back({AudioFormat(), CodecInfo()});
  }

  static AudioCodecInfo QueryAudioEncoder(const Config&) { return CodecInfo(); }
};

struct AudioEncoderApiWithV1Make : BaseAudioEncoderApi {
  static std::unique_ptr<AudioEncoder> MakeAudioEncoder(
      const Config&,
      int payload_type,
      absl::optional<AudioCodecPairId> codec_pair_id) {
    auto encoder = std::make_unique<NiceMock<MockAudioEncoder>>();
    ON_CALL(*encoder, SampleRateHz).WillByDefault(Return(kV1SameRate));
    return encoder;
  }
};

struct AudioEncoderApiWithV2Make : BaseAudioEncoderApi {
  static std::unique_ptr<AudioEncoder> MakeAudioEncoder(
      const Environment& env,
      const Config& config,
      const AudioEncoderFactory::Options& options) {
    auto encoder = std::make_unique<NiceMock<MockAudioEncoder>>();
    ON_CALL(*encoder, SampleRateHz).WillByDefault(Return(kV2SameRate));
    return encoder;
  }
};

struct AudioEncoderApiWithBothV1AndV2Make : BaseAudioEncoderApi {
  static std::unique_ptr<AudioEncoder> MakeAudioEncoder(
      const Config&,
      int payload_type,
      absl::optional<AudioCodecPairId> codec_pair_id) {
    auto encoder = std::make_unique<NiceMock<MockAudioEncoder>>();
    ON_CALL(*encoder, SampleRateHz).WillByDefault(Return(kV1SameRate));
    return encoder;
  }

  static std::unique_ptr<AudioEncoder> MakeAudioEncoder(
      const Environment& env,
      const Config& config,
      const AudioEncoderFactory::Options& options) {
    auto encoder = std::make_unique<NiceMock<MockAudioEncoder>>();
    ON_CALL(*encoder, SampleRateHz).WillByDefault(Return(kV2SameRate));
    return encoder;
  }
};

TEST(AudioEncoderFactoryTemplateTest,
     UsesV1MakeAudioEncoderWhenV2IsNotAvailable) {
  const Environment env = CreateEnvironment();
  auto factory = CreateAudioEncoderFactory<AudioEncoderApiWithV1Make>();

  EXPECT_THAT(
      factory->MakeAudioEncoder(17, BaseAudioEncoderApi::AudioFormat(), {}),
      Pointer(Property(&AudioEncoder::SampleRateHz,
                       BaseAudioEncoderApi::kV1SameRate)));

  EXPECT_THAT(factory->Create(env, BaseAudioEncoderApi::AudioFormat(), {}),
              Pointer(Property(&AudioEncoder::SampleRateHz,
                               BaseAudioEncoderApi::kV1SameRate)));
}

TEST(AudioEncoderFactoryTemplateTest,
     PreferV2MakeAudioEncoderWhenBothAreAvailable) {
  const Environment env = CreateEnvironment();
  auto factory =
      CreateAudioEncoderFactory<AudioEncoderApiWithBothV1AndV2Make>();

  EXPECT_THAT(factory->Create(env, BaseAudioEncoderApi::AudioFormat(), {}),
              Pointer(Property(&AudioEncoder::SampleRateHz,
                               BaseAudioEncoderApi::kV2SameRate)));

  // For backward compatibility legacy AudioEncoderFactory::MakeAudioEncoder
  // still can be used, and uses older signature of the Trait::MakeAudioEncoder.
  EXPECT_THAT(
      factory->MakeAudioEncoder(17, BaseAudioEncoderApi::AudioFormat(), {}),
      Pointer(Property(&AudioEncoder::SampleRateHz,
                       BaseAudioEncoderApi::kV1SameRate)));
}

TEST(AudioEncoderFactoryTemplateTest, CanUseTraitWithOnlyV2MakeAudioEncoder) {
  const Environment env = CreateEnvironment();
  auto factory = CreateAudioEncoderFactory<AudioEncoderApiWithV2Make>();
  EXPECT_THAT(factory->Create(env, BaseAudioEncoderApi::AudioFormat(), {}),
              Pointer(Property(&AudioEncoder::SampleRateHz,
                               BaseAudioEncoderApi::kV2SameRate)));
}

#if GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST(AudioEncoderFactoryTemplateTest, CrashesWhenV2OnlyTraitUsedWithOlderApi) {
  auto factory = CreateAudioEncoderFactory<AudioEncoderApiWithV2Make>();
  // V2 signature requires Environment that
  // AudioEncoderFactory::MakeAudioEncoder doesn't provide.
  EXPECT_DEATH(
      factory->MakeAudioEncoder(17, BaseAudioEncoderApi::AudioFormat(), {}),
      "");
}
#endif

TEST(AudioEncoderFactoryTemplateTest, NoEncoderTypes) {
  test::ScopedKeyValueConfig field_trials;
  const Environment env = CreateEnvironment(&field_trials);
  rtc::scoped_refptr<AudioEncoderFactory> factory(
      rtc::make_ref_counted<
          audio_encoder_factory_template_impl::AudioEncoderFactoryT<>>());
  EXPECT_THAT(factory->GetSupportedEncoders(), ::testing::IsEmpty());
  EXPECT_EQ(absl::nullopt, factory->QueryAudioEncoder({"foo", 8000, 1}));

  EXPECT_THAT(factory->Create(env, {"bar", 16000, 1}, {}), IsNull());
}

TEST(AudioEncoderFactoryTemplateTest, OneEncoderType) {
  const Environment env = CreateEnvironment();
  auto factory = CreateAudioEncoderFactory<AudioEncoderFakeApi<BogusParams>>();
  EXPECT_THAT(factory->GetSupportedEncoders(),
              ::testing::ElementsAre(
                  AudioCodecSpec{{"bogus", 8000, 1}, {8000, 1, 12345}}));
  EXPECT_EQ(absl::nullopt, factory->QueryAudioEncoder({"foo", 8000, 1}));
  EXPECT_EQ(AudioCodecInfo(8000, 1, 12345),
            factory->QueryAudioEncoder({"bogus", 8000, 1}));

  EXPECT_THAT(factory->Create(env, {"bar", 16000, 1}, {}), IsNull());
  EXPECT_THAT(factory->Create(env, {"bogus", 8000, 1}, {}),
              Pointer(Property(&AudioEncoder::SampleRateHz, 8000)));
}

TEST(AudioEncoderFactoryTemplateTest, TwoEncoderTypes) {
  const Environment env = CreateEnvironment();
  auto factory = CreateAudioEncoderFactory<AudioEncoderFakeApi<BogusParams>,
                                           AudioEncoderFakeApi<ShamParams>>();
  EXPECT_THAT(factory->GetSupportedEncoders(),
              ::testing::ElementsAre(
                  AudioCodecSpec{{"bogus", 8000, 1}, {8000, 1, 12345}},
                  AudioCodecSpec{{"sham", 16000, 2, {{"param", "value"}}},
                                 {16000, 2, 23456}}));
  EXPECT_EQ(absl::nullopt, factory->QueryAudioEncoder({"foo", 8000, 1}));
  EXPECT_EQ(AudioCodecInfo(8000, 1, 12345),
            factory->QueryAudioEncoder({"bogus", 8000, 1}));
  EXPECT_EQ(
      AudioCodecInfo(16000, 2, 23456),
      factory->QueryAudioEncoder({"sham", 16000, 2, {{"param", "value"}}}));

  EXPECT_THAT(factory->Create(env, {"bar", 16000, 1}, {}), IsNull());
  EXPECT_THAT(factory->Create(env, {"bogus", 8000, 1}, {}),
              Pointer(Property(&AudioEncoder::SampleRateHz, 8000)));
  EXPECT_THAT(factory->Create(env, {"sham", 16000, 2}, {}), IsNull());
  EXPECT_THAT(
      factory->Create(env, {"sham", 16000, 2, {{"param", "value"}}}, {}),
      Pointer(Property(&AudioEncoder::SampleRateHz, 16000)));
}

TEST(AudioEncoderFactoryTemplateTest, G711) {
  const Environment env = CreateEnvironment();
  auto factory = CreateAudioEncoderFactory<AudioEncoderG711>();
  EXPECT_THAT(factory->GetSupportedEncoders(),
              ::testing::ElementsAre(
                  AudioCodecSpec{{"PCMU", 8000, 1}, {8000, 1, 64000}},
                  AudioCodecSpec{{"PCMA", 8000, 1}, {8000, 1, 64000}}));
  EXPECT_EQ(absl::nullopt, factory->QueryAudioEncoder({"PCMA", 16000, 1}));
  EXPECT_EQ(AudioCodecInfo(8000, 1, 64000),
            factory->QueryAudioEncoder({"PCMA", 8000, 1}));

  EXPECT_THAT(factory->Create(env, {"PCMU", 16000, 1}, {}), IsNull());
  EXPECT_THAT(factory->Create(env, {"PCMU", 8000, 1}, {}),
              Pointer(Property(&AudioEncoder::SampleRateHz, 8000)));
  EXPECT_THAT(factory->Create(env, {"PCMA", 8000, 1}, {}),
              Pointer(Property(&AudioEncoder::SampleRateHz, 8000)));
}

TEST(AudioEncoderFactoryTemplateTest, G722) {
  const Environment env = CreateEnvironment();
  auto factory = CreateAudioEncoderFactory<AudioEncoderG722>();
  EXPECT_THAT(factory->GetSupportedEncoders(),
              ::testing::ElementsAre(
                  AudioCodecSpec{{"G722", 8000, 1}, {16000, 1, 64000}}));
  EXPECT_EQ(absl::nullopt, factory->QueryAudioEncoder({"foo", 8000, 1}));
  EXPECT_EQ(AudioCodecInfo(16000, 1, 64000),
            factory->QueryAudioEncoder({"G722", 8000, 1}));

  EXPECT_THAT(factory->Create(env, {"bar", 16000, 1}, {}), IsNull());
  EXPECT_THAT(factory->Create(env, {"G722", 8000, 1}, {}),
              Pointer(Property(&AudioEncoder::SampleRateHz, 16000)));
}

TEST(AudioEncoderFactoryTemplateTest, Ilbc) {
  const Environment env = CreateEnvironment();
  auto factory = CreateAudioEncoderFactory<AudioEncoderIlbc>();
  EXPECT_THAT(factory->GetSupportedEncoders(),
              ::testing::ElementsAre(
                  AudioCodecSpec{{"ILBC", 8000, 1}, {8000, 1, 13333}}));
  EXPECT_EQ(absl::nullopt, factory->QueryAudioEncoder({"foo", 8000, 1}));
  EXPECT_EQ(AudioCodecInfo(8000, 1, 13333),
            factory->QueryAudioEncoder({"ilbc", 8000, 1}));

  EXPECT_THAT(factory->Create(env, {"bar", 8000, 1}, {}), IsNull());
  EXPECT_THAT(factory->Create(env, {"ilbc", 8000, 1}, {}),
              Pointer(Property(&AudioEncoder::SampleRateHz, 8000)));
}

TEST(AudioEncoderFactoryTemplateTest, L16) {
  const Environment env = CreateEnvironment();
  auto factory = CreateAudioEncoderFactory<AudioEncoderL16>();
  EXPECT_THAT(
      factory->GetSupportedEncoders(),
      ::testing::ElementsAre(
          AudioCodecSpec{{"L16", 8000, 1}, {8000, 1, 8000 * 16}},
          AudioCodecSpec{{"L16", 16000, 1}, {16000, 1, 16000 * 16}},
          AudioCodecSpec{{"L16", 32000, 1}, {32000, 1, 32000 * 16}},
          AudioCodecSpec{{"L16", 8000, 2}, {8000, 2, 8000 * 16 * 2}},
          AudioCodecSpec{{"L16", 16000, 2}, {16000, 2, 16000 * 16 * 2}},
          AudioCodecSpec{{"L16", 32000, 2}, {32000, 2, 32000 * 16 * 2}}));
  EXPECT_EQ(absl::nullopt, factory->QueryAudioEncoder({"L16", 8000, 0}));
  EXPECT_EQ(AudioCodecInfo(48000, 1, 48000 * 16),
            factory->QueryAudioEncoder({"L16", 48000, 1}));

  EXPECT_THAT(factory->Create(env, {"L16", 8000, 0}, {}), IsNull());
  EXPECT_THAT(factory->Create(env, {"L16", 48000, 2}, {}),
              Pointer(Property(&AudioEncoder::SampleRateHz, 48000)));
}

TEST(AudioEncoderFactoryTemplateTest, Opus) {
  const Environment env = CreateEnvironment();
  auto factory = CreateAudioEncoderFactory<AudioEncoderOpus>();
  AudioCodecInfo info = {48000, 1, 32000, 6000, 510000};
  info.allow_comfort_noise = false;
  info.supports_network_adaption = true;
  EXPECT_THAT(
      factory->GetSupportedEncoders(),
      ::testing::ElementsAre(AudioCodecSpec{
          {"opus", 48000, 2, {{"minptime", "10"}, {"useinbandfec", "1"}}},
          info}));
  EXPECT_EQ(absl::nullopt, factory->QueryAudioEncoder({"foo", 8000, 1}));
  EXPECT_EQ(
      info,
      factory->QueryAudioEncoder(
          {"opus", 48000, 2, {{"minptime", "10"}, {"useinbandfec", "1"}}}));

  EXPECT_THAT(factory->Create(env, {"bar", 16000, 1}, {.payload_type = 17}),
              IsNull());
  EXPECT_THAT(factory->Create(env, {"opus", 48000, 2}, {.payload_type = 17}),
              Pointer(Property(&AudioEncoder::SampleRateHz, 48000)));
}

}  // namespace
}  // namespace webrtc
