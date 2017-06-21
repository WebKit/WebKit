/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/audio_codecs/audio_decoder_factory_template.h"
#include "webrtc/api/audio_codecs/g722/audio_decoder_g722.h"
#include "webrtc/base/ptr_util.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/mock_audio_decoder.h"

namespace webrtc {

namespace {

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

struct MyLittleConfig {
  SdpAudioFormat audio_format;
};

template <typename Params>
struct AudioDecoderFakeApi {
  static rtc::Optional<MyLittleConfig> SdpToConfig(
      const SdpAudioFormat& audio_format) {
    if (Params::AudioFormat() == audio_format) {
      MyLittleConfig config = {audio_format};
      return rtc::Optional<MyLittleConfig>(config);
    } else {
      return rtc::Optional<MyLittleConfig>();
    }
  }

  static void AppendSupportedDecoders(std::vector<AudioCodecSpec>* specs) {
    specs->push_back({Params::AudioFormat(), Params::CodecInfo()});
  }

  static AudioCodecInfo QueryAudioDecoder(const MyLittleConfig&) {
    return Params::CodecInfo();
  }

  static std::unique_ptr<AudioDecoder> MakeAudioDecoder(const MyLittleConfig&) {
    auto dec = rtc::MakeUnique<testing::StrictMock<MockAudioDecoder>>();
    EXPECT_CALL(*dec, SampleRateHz())
        .WillOnce(testing::Return(Params::CodecInfo().sample_rate_hz));
    EXPECT_CALL(*dec, Die());
    return std::move(dec);
  }
};

}  // namespace

TEST(AudioDecoderFactoryTemplateTest, NoDecoderTypes) {
  rtc::scoped_refptr<AudioDecoderFactory> factory(
      new rtc::RefCountedObject<
          audio_decoder_factory_template_impl::AudioDecoderFactoryT<>>());
  EXPECT_THAT(factory->GetSupportedDecoders(), testing::IsEmpty());
  EXPECT_FALSE(factory->IsSupportedDecoder({"foo", 8000, 1}));
  EXPECT_EQ(nullptr, factory->MakeAudioDecoder({"bar", 16000, 1}));
}

TEST(AudioDecoderFactoryTemplateTest, OneDecoderType) {
  auto factory = CreateAudioDecoderFactory<AudioDecoderFakeApi<BogusParams>>();
  EXPECT_THAT(factory->GetSupportedDecoders(),
              testing::ElementsAre(
                  AudioCodecSpec{{"bogus", 8000, 1}, {8000, 1, 12345}}));
  EXPECT_FALSE(factory->IsSupportedDecoder({"foo", 8000, 1}));
  EXPECT_TRUE(factory->IsSupportedDecoder({"bogus", 8000, 1}));
  EXPECT_EQ(nullptr, factory->MakeAudioDecoder({"bar", 16000, 1}));
  auto dec = factory->MakeAudioDecoder({"bogus", 8000, 1});
  ASSERT_NE(nullptr, dec);
  EXPECT_EQ(8000, dec->SampleRateHz());
}

TEST(AudioDecoderFactoryTemplateTest, TwoDecoderTypes) {
  auto factory = CreateAudioDecoderFactory<AudioDecoderFakeApi<BogusParams>,
                                           AudioDecoderFakeApi<ShamParams>>();
  EXPECT_THAT(factory->GetSupportedDecoders(),
              testing::ElementsAre(
                  AudioCodecSpec{{"bogus", 8000, 1}, {8000, 1, 12345}},
                  AudioCodecSpec{{"sham", 16000, 2, {{"param", "value"}}},
                                 {16000, 2, 23456}}));
  EXPECT_FALSE(factory->IsSupportedDecoder({"foo", 8000, 1}));
  EXPECT_TRUE(factory->IsSupportedDecoder({"bogus", 8000, 1}));
  EXPECT_TRUE(
      factory->IsSupportedDecoder({"sham", 16000, 2, {{"param", "value"}}}));
  EXPECT_EQ(nullptr, factory->MakeAudioDecoder({"bar", 16000, 1}));
  auto dec1 = factory->MakeAudioDecoder({"bogus", 8000, 1});
  ASSERT_NE(nullptr, dec1);
  EXPECT_EQ(8000, dec1->SampleRateHz());
  EXPECT_EQ(nullptr, factory->MakeAudioDecoder({"sham", 16000, 2}));
  auto dec2 =
      factory->MakeAudioDecoder({"sham", 16000, 2, {{"param", "value"}}});
  ASSERT_NE(nullptr, dec2);
  EXPECT_EQ(16000, dec2->SampleRateHz());
}

TEST(AudioDecoderFactoryTemplateTest, G722) {
  auto factory = CreateAudioDecoderFactory<AudioDecoderG722>();
  EXPECT_THAT(factory->GetSupportedDecoders(),
              testing::ElementsAre(
                  AudioCodecSpec{{"g722", 8000, 1}, {16000, 1, 64000}}));
  EXPECT_FALSE(factory->IsSupportedDecoder({"foo", 8000, 1}));
  EXPECT_TRUE(factory->IsSupportedDecoder({"g722", 8000, 1}));
  EXPECT_EQ(nullptr, factory->MakeAudioDecoder({"bar", 16000, 1}));
  auto dec = factory->MakeAudioDecoder({"g722", 8000, 1});
  ASSERT_NE(nullptr, dec);
  EXPECT_EQ(16000, dec->SampleRateHz());
}

}  // namespace webrtc
