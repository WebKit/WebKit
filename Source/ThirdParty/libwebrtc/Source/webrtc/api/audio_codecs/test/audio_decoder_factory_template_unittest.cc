/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_codecs/audio_decoder_factory_template.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "api/audio_codecs/L16/audio_decoder_L16.h"
#include "api/audio_codecs/audio_codec_pair_id.h"
#include "api/audio_codecs/audio_decoder.h"
#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_format.h"
#include "api/audio_codecs/g711/audio_decoder_g711.h"
#include "api/audio_codecs/g722/audio_decoder_g722.h"
#include "api/audio_codecs/opus/audio_decoder_opus.h"
#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "api/make_ref_counted.h"
#include "api/scoped_refptr.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/mock_audio_decoder.h"

namespace webrtc {
namespace {

using ::testing::NiceMock;
using ::testing::NotNull;
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
struct AudioDecoderFakeApi {
  struct Config {
    SdpAudioFormat audio_format;
  };

  static std::optional<Config> SdpToConfig(const SdpAudioFormat& audio_format) {
    if (Params::AudioFormat() == audio_format) {
      Config config = {audio_format};
      return config;
    } else {
      return std::nullopt;
    }
  }

  static void AppendSupportedDecoders(std::vector<AudioCodecSpec>* specs) {
    specs->push_back({Params::AudioFormat(), Params::CodecInfo()});
  }

  static AudioCodecInfo QueryAudioDecoder(const Config&) {
    return Params::CodecInfo();
  }

  static std::unique_ptr<AudioDecoder> MakeAudioDecoder(
      const Config&,
      std::optional<AudioCodecPairId> /*codec_pair_id*/ = std::nullopt) {
    auto dec = std::make_unique<testing::StrictMock<MockAudioDecoder>>();
    EXPECT_CALL(*dec, SampleRateHz())
        .WillOnce(::testing::Return(Params::CodecInfo().sample_rate_hz));
    EXPECT_CALL(*dec, Die());
    return std::move(dec);
  }
};

// Trait to pass as template parameter to `CreateAudioDecoderFactory` with
// all the functions except the functions to create the audio decoder.
struct BaseAudioDecoderApi {
  struct Config {};

  static SdpAudioFormat AudioFormat() { return {"fake", 16'000, 2, {}}; }

  static std::optional<Config> SdpToConfig(
      const SdpAudioFormat& /* audio_format */) {
    return Config();
  }

  static void AppendSupportedDecoders(std::vector<AudioCodecSpec>* specs) {
    specs->push_back({.format = AudioFormat(), .info = {16'000, 2, 23456}});
  }
};

struct TraitWithFourMakeAudioDecoders : BaseAudioDecoderApi {
  // Create Decoders with different sample rates depending if it is created
  // through one or another `MakeAudioDecoder` so that a test may detect which
  // method was used.
  static constexpr int kRateWithoutEnv = 10'000;
  static constexpr int kRateWithEnv = 20'000;
  static constexpr int kRateWithEnvWithoutCodecId = 30'000;
  static constexpr int kRateWithoutEnvWithoutCodecId = 40'000;

  static std::unique_ptr<AudioDecoder> MakeAudioDecoder(
      const Config& /* config */) {
    auto decoder = std::make_unique<NiceMock<MockAudioDecoder>>();
    ON_CALL(*decoder, SampleRateHz)
        .WillByDefault(Return(kRateWithoutEnvWithoutCodecId));
    return decoder;
  }

  static std::unique_ptr<AudioDecoder> MakeAudioDecoder(
      const Environment& /* env */,
      const Config& /* config */) {
    auto decoder = std::make_unique<NiceMock<MockAudioDecoder>>();
    ON_CALL(*decoder, SampleRateHz)
        .WillByDefault(Return(kRateWithEnvWithoutCodecId));
    return decoder;
  }
  // Testing backwards compatible case
  static std::unique_ptr<AudioDecoder> MakeAudioDecoder(
      const Config& /* config */,
      std::optional<AudioCodecPairId> /* codec_pair_id */) {
    auto decoder = std::make_unique<NiceMock<MockAudioDecoder>>();
    ON_CALL(*decoder, SampleRateHz).WillByDefault(Return(kRateWithoutEnv));
    return decoder;
  }

  static std::unique_ptr<AudioDecoder> MakeAudioDecoder(
      const Environment& /* env */,
      const Config& /* config */,
      std::optional<AudioCodecPairId> /* codec_pair_id */) {
    auto decoder = std::make_unique<NiceMock<MockAudioDecoder>>();
    ON_CALL(*decoder, SampleRateHz).WillByDefault(Return(kRateWithEnv));
    return decoder;
  }
};

TEST(AudioDecoderFactoryTemplateTest,
     PrefersToPassEnvironmentToMakeAudioDecoder) {
  const Environment env = CreateEnvironment();
  auto factory = CreateAudioDecoderFactory<TraitWithFourMakeAudioDecoders>();

  EXPECT_THAT(factory->Create(env, BaseAudioDecoderApi::AudioFormat(), {}),
              Pointer(Property(
                  &AudioDecoder::SampleRateHz,
                  TraitWithFourMakeAudioDecoders::kRateWithEnvWithoutCodecId)));
}

struct AudioDecoderApiWithV1Make : BaseAudioDecoderApi {
  static std::unique_ptr<AudioDecoder> MakeAudioDecoder(
      const Config& /* config */,
      std::optional<AudioCodecPairId> /* codec_pair_id */) {
    return std::make_unique<NiceMock<MockAudioDecoder>>();
  }
};

TEST(AudioDecoderFactoryTemplateTest,
     CanUseMakeAudioDecoderWithoutPassingEnvironment) {
  const Environment env = CreateEnvironment();
  auto factory = CreateAudioDecoderFactory<AudioDecoderApiWithV1Make>();
  EXPECT_THAT(factory->Create(env, BaseAudioDecoderApi::AudioFormat(), {}),
              NotNull());
}

TEST(AudioDecoderFactoryTemplateTest, NoDecoderTypes) {
  const Environment env = CreateEnvironment();
  scoped_refptr<AudioDecoderFactory> factory(
      make_ref_counted<
          audio_decoder_factory_template_impl::AudioDecoderFactoryT<>>());
  EXPECT_THAT(factory->GetSupportedDecoders(), ::testing::IsEmpty());
  EXPECT_FALSE(factory->IsSupportedDecoder({"foo", 8000, 1}));
  EXPECT_EQ(nullptr, factory->Create(env, {"bar", 16000, 1}, std::nullopt));
}

TEST(AudioDecoderFactoryTemplateTest, OneDecoderType) {
  const Environment env = CreateEnvironment();
  auto factory = CreateAudioDecoderFactory<AudioDecoderFakeApi<BogusParams>>();
  EXPECT_THAT(factory->GetSupportedDecoders(),
              ::testing::ElementsAre(
                  AudioCodecSpec{{"bogus", 8000, 1}, {8000, 1, 12345}}));
  EXPECT_FALSE(factory->IsSupportedDecoder({"foo", 8000, 1}));
  EXPECT_TRUE(factory->IsSupportedDecoder({"bogus", 8000, 1}));
  EXPECT_EQ(nullptr, factory->Create(env, {"bar", 16000, 1}, std::nullopt));
  auto dec = factory->Create(env, {"bogus", 8000, 1}, std::nullopt);
  ASSERT_NE(nullptr, dec);
  EXPECT_EQ(8000, dec->SampleRateHz());
}

TEST(AudioDecoderFactoryTemplateTest, TwoDecoderTypes) {
  const Environment env = CreateEnvironment();
  auto factory = CreateAudioDecoderFactory<AudioDecoderFakeApi<BogusParams>,
                                           AudioDecoderFakeApi<ShamParams>>();
  EXPECT_THAT(factory->GetSupportedDecoders(),
              ::testing::ElementsAre(
                  AudioCodecSpec{{"bogus", 8000, 1}, {8000, 1, 12345}},
                  AudioCodecSpec{{"sham", 16000, 2, {{"param", "value"}}},
                                 {16000, 2, 23456}}));
  EXPECT_FALSE(factory->IsSupportedDecoder({"foo", 8000, 1}));
  EXPECT_TRUE(factory->IsSupportedDecoder({"bogus", 8000, 1}));
  EXPECT_TRUE(
      factory->IsSupportedDecoder({"sham", 16000, 2, {{"param", "value"}}}));
  EXPECT_EQ(nullptr, factory->Create(env, {"bar", 16000, 1}, std::nullopt));
  auto dec1 = factory->Create(env, {"bogus", 8000, 1}, std::nullopt);
  ASSERT_NE(nullptr, dec1);
  EXPECT_EQ(8000, dec1->SampleRateHz());
  EXPECT_EQ(nullptr, factory->Create(env, {"sham", 16000, 2}, std::nullopt));
  auto dec2 = factory->Create(env, {"sham", 16000, 2, {{"param", "value"}}},
                              std::nullopt);
  ASSERT_NE(nullptr, dec2);
  EXPECT_EQ(16000, dec2->SampleRateHz());
}

TEST(AudioDecoderFactoryTemplateTest, G711) {
  const Environment env = CreateEnvironment();
  auto factory = CreateAudioDecoderFactory<AudioDecoderG711>();
  EXPECT_THAT(factory->GetSupportedDecoders(),
              ::testing::ElementsAre(
                  AudioCodecSpec{{"PCMU", 8000, 1}, {8000, 1, 64000}},
                  AudioCodecSpec{{"PCMA", 8000, 1}, {8000, 1, 64000}}));
  EXPECT_FALSE(factory->IsSupportedDecoder({"G711", 8000, 1}));
  EXPECT_TRUE(factory->IsSupportedDecoder({"PCMU", 8000, 1}));
  EXPECT_TRUE(factory->IsSupportedDecoder({"pcma", 8000, 1}));
  EXPECT_EQ(nullptr, factory->Create(env, {"pcmu", 16000, 1}, std::nullopt));
  auto dec1 = factory->Create(env, {"pcmu", 8000, 1}, std::nullopt);
  ASSERT_NE(nullptr, dec1);
  EXPECT_EQ(8000, dec1->SampleRateHz());
  auto dec2 = factory->Create(env, {"PCMA", 8000, 1}, std::nullopt);
  ASSERT_NE(nullptr, dec2);
  EXPECT_EQ(8000, dec2->SampleRateHz());
}

TEST(AudioDecoderFactoryTemplateTest, G722) {
  const Environment env = CreateEnvironment();
  auto factory = CreateAudioDecoderFactory<AudioDecoderG722>();
  EXPECT_THAT(factory->GetSupportedDecoders(),
              ::testing::ElementsAre(
                  AudioCodecSpec{{"G722", 8000, 1}, {16000, 1, 64000}}));
  EXPECT_FALSE(factory->IsSupportedDecoder({"foo", 8000, 1}));
  EXPECT_TRUE(factory->IsSupportedDecoder({"G722", 8000, 1}));
  EXPECT_EQ(nullptr, factory->Create(env, {"bar", 16000, 1}, std::nullopt));
  auto dec1 = factory->Create(env, {"G722", 8000, 1}, std::nullopt);
  ASSERT_NE(nullptr, dec1);
  EXPECT_EQ(16000, dec1->SampleRateHz());
  EXPECT_EQ(1u, dec1->Channels());
  auto dec2 = factory->Create(env, {"G722", 8000, 2}, std::nullopt);
  ASSERT_NE(nullptr, dec2);
  EXPECT_EQ(16000, dec2->SampleRateHz());
  EXPECT_EQ(2u, dec2->Channels());
  auto dec3 = factory->Create(env, {"G722", 8000, 3}, std::nullopt);
  ASSERT_EQ(nullptr, dec3);
}

TEST(AudioDecoderFactoryTemplateTest, L16) {
  const Environment env = CreateEnvironment();
  auto factory = CreateAudioDecoderFactory<AudioDecoderL16>();
  EXPECT_THAT(
      factory->GetSupportedDecoders(),
      ::testing::ElementsAre(
          AudioCodecSpec{{"L16", 8000, 1}, {8000, 1, 8000 * 16}},
          AudioCodecSpec{{"L16", 16000, 1}, {16000, 1, 16000 * 16}},
          AudioCodecSpec{{"L16", 32000, 1}, {32000, 1, 32000 * 16}},
          AudioCodecSpec{{"L16", 8000, 2}, {8000, 2, 8000 * 16 * 2}},
          AudioCodecSpec{{"L16", 16000, 2}, {16000, 2, 16000 * 16 * 2}},
          AudioCodecSpec{{"L16", 32000, 2}, {32000, 2, 32000 * 16 * 2}}));
  EXPECT_FALSE(factory->IsSupportedDecoder({"foo", 8000, 1}));
  EXPECT_TRUE(factory->IsSupportedDecoder({"L16", 48000, 1}));
  EXPECT_FALSE(factory->IsSupportedDecoder({"L16", 96000, 1}));
  EXPECT_EQ(nullptr, factory->Create(env, {"L16", 8000, 0}, std::nullopt));
  auto dec = factory->Create(env, {"L16", 48000, 2}, std::nullopt);
  ASSERT_NE(nullptr, dec);
  EXPECT_EQ(48000, dec->SampleRateHz());
}

TEST(AudioDecoderFactoryTemplateTest, Opus) {
  const Environment env = CreateEnvironment();
  auto factory = CreateAudioDecoderFactory<AudioDecoderOpus>();
  AudioCodecInfo opus_info{48000, 1, 64000, 6000, 510000};
  opus_info.allow_comfort_noise = false;
  opus_info.supports_network_adaption = true;
  const SdpAudioFormat opus_format(
      {"opus", 48000, 2, {{"minptime", "10"}, {"useinbandfec", "1"}}});
  EXPECT_THAT(factory->GetSupportedDecoders(),
              ::testing::ElementsAre(AudioCodecSpec{opus_format, opus_info}));
  EXPECT_FALSE(factory->IsSupportedDecoder({"opus", 48000, 1}));
  EXPECT_TRUE(factory->IsSupportedDecoder({"opus", 48000, 2}));
  EXPECT_EQ(nullptr, factory->Create(env, {"bar", 16000, 1}, std::nullopt));
  auto dec = factory->Create(env, {"opus", 48000, 2}, std::nullopt);
  ASSERT_NE(nullptr, dec);
  EXPECT_EQ(48000, dec->SampleRateHz());
}

TEST(AudioDecoderFactoryTemplateTest, G711TooManyChannels) {
  auto factory = CreateAudioDecoderFactory<AudioDecoderG711>();
  const Environment env = CreateEnvironment();
  EXPECT_EQ(nullptr, factory->Create(env,
                                     {"pcmu", 16000,
                                      /* num_channels=*/1000},
                                     std::nullopt));
}

}  // namespace
}  // namespace webrtc
