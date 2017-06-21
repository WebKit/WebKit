/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/codecs/builtin_audio_encoder_factory_internal.h"

#include <memory>
#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/optional.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/audio_coding/codecs/g711/audio_encoder_pcm.h"
#ifdef WEBRTC_CODEC_G722
#include "webrtc/modules/audio_coding/codecs/g722/audio_encoder_g722.h"
#endif
#ifdef WEBRTC_CODEC_ILBC
#include "webrtc/modules/audio_coding/codecs/ilbc/audio_encoder_ilbc.h"
#endif
#ifdef WEBRTC_CODEC_ISACFX
#include "webrtc/modules/audio_coding/codecs/isac/fix/include/audio_encoder_isacfix.h"  // nogncheck
#endif
#ifdef WEBRTC_CODEC_ISAC
#include "webrtc/modules/audio_coding/codecs/isac/main/include/audio_encoder_isac.h"  // nogncheck
#endif
#ifdef WEBRTC_CODEC_OPUS
#include "webrtc/modules/audio_coding/codecs/opus/audio_encoder_opus.h"
#endif
#include "webrtc/modules/audio_coding/codecs/pcm16b/audio_encoder_pcm16b.h"

namespace webrtc {

namespace {

struct NamedEncoderFactory {
  const char* name;
  rtc::Optional<AudioCodecInfo> (*QueryAudioEncoder)(
      const SdpAudioFormat& format);
  std::unique_ptr<AudioEncoder> (
      *MakeAudioEncoder)(int payload_type, const SdpAudioFormat& format);

  template <typename T>
  static NamedEncoderFactory ForEncoder() {
    auto constructor = [](int payload_type, const SdpAudioFormat& format) {
      auto opt_info = T::QueryAudioEncoder(format);
      if (opt_info) {
        return std::unique_ptr<AudioEncoder>(new T(payload_type, format));
      }
      return std::unique_ptr<AudioEncoder>();
    };

    return {T::GetPayloadName(), T::QueryAudioEncoder, constructor};
  }
};

NamedEncoderFactory encoder_factories[] = {
#ifdef WEBRTC_CODEC_G722
    NamedEncoderFactory::ForEncoder<AudioEncoderG722Impl>(),
#endif
#ifdef WEBRTC_CODEC_ILBC
    NamedEncoderFactory::ForEncoder<AudioEncoderIlbc>(),
#endif
#if defined(WEBRTC_CODEC_ISACFX)
    NamedEncoderFactory::ForEncoder<AudioEncoderIsacFix>(),
#elif defined(WEBRTC_CODEC_ISAC)
    NamedEncoderFactory::ForEncoder<AudioEncoderIsac>(),
#endif

#ifdef WEBRTC_CODEC_OPUS
    NamedEncoderFactory::ForEncoder<AudioEncoderOpus>(),
#endif
    NamedEncoderFactory::ForEncoder<AudioEncoderPcm16B>(),
    NamedEncoderFactory::ForEncoder<AudioEncoderPcmA>(),
    NamedEncoderFactory::ForEncoder<AudioEncoderPcmU>(),
};
}  // namespace

class BuiltinAudioEncoderFactory : public AudioEncoderFactory {
 public:
  std::vector<AudioCodecSpec> GetSupportedEncoders() override {
    static const SdpAudioFormat desired_encoders[] = {
        {"opus", 48000, 2, {{"minptime", "10"}, {"useinbandfec", "1"}}},
        {"ISAC", 16000, 1},
        {"ISAC", 32000, 1},
        {"G722", 8000, 1},
        {"ILBC", 8000, 1},
        {"PCMU", 8000, 1},
        {"PCMA", 8000, 1},
    };

    // Initialize thread-safely, once, on first use.
    static const std::vector<AudioCodecSpec> specs = [] {
      std::vector<AudioCodecSpec> specs;
      for (const auto& format : desired_encoders) {
        for (const auto& ef : encoder_factories) {
          if (STR_CASE_CMP(format.name.c_str(), ef.name) == 0) {
            auto opt_info = ef.QueryAudioEncoder(format);
            if (opt_info) {
              specs.push_back({format, *opt_info});
            }
          }
        }
      }
      return specs;
    }();
    return specs;
  }

  rtc::Optional<AudioCodecInfo> QueryAudioEncoder(
      const SdpAudioFormat& format) override {
    for (const auto& ef : encoder_factories) {
      if (STR_CASE_CMP(format.name.c_str(), ef.name) == 0) {
        return ef.QueryAudioEncoder(format);
      }
    }
    return rtc::Optional<AudioCodecInfo>();
  }

  std::unique_ptr<AudioEncoder> MakeAudioEncoder(
      int payload_type,
      const SdpAudioFormat& format) override {
    for (const auto& ef : encoder_factories) {
      if (STR_CASE_CMP(format.name.c_str(), ef.name) == 0) {
        return ef.MakeAudioEncoder(payload_type, format);
      }
    }
    return nullptr;
  }
};

rtc::scoped_refptr<AudioEncoderFactory>
CreateBuiltinAudioEncoderFactoryInternal() {
  return rtc::scoped_refptr<AudioEncoderFactory>(
      new rtc::RefCountedObject<BuiltinAudioEncoderFactory>());
}

}  // namespace webrtc
