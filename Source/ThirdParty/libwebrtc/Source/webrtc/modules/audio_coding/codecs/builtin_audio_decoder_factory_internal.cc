/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/codecs/builtin_audio_decoder_factory_internal.h"

#include <memory>
#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/base/optional.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/audio_coding/codecs/cng/webrtc_cng.h"
#include "webrtc/modules/audio_coding/codecs/g711/audio_decoder_pcm.h"
#ifdef WEBRTC_CODEC_G722
#include "webrtc/modules/audio_coding/codecs/g722/audio_decoder_g722.h"
#endif
#ifdef WEBRTC_CODEC_ILBC
#include "webrtc/modules/audio_coding/codecs/ilbc/audio_decoder_ilbc.h"
#endif
#ifdef WEBRTC_CODEC_ISACFX
#include "webrtc/modules/audio_coding/codecs/isac/fix/include/audio_decoder_isacfix.h"  // nogncheck
#endif
#ifdef WEBRTC_CODEC_ISAC
#include "webrtc/modules/audio_coding/codecs/isac/main/include/audio_decoder_isac.h"  // nogncheck
#endif
#ifdef WEBRTC_CODEC_OPUS
#include "webrtc/modules/audio_coding/codecs/opus/audio_decoder_opus.h"
#endif
#include "webrtc/modules/audio_coding/codecs/pcm16b/audio_decoder_pcm16b.h"

namespace webrtc {

namespace {

struct NamedDecoderConstructor {
  const char* name;

  // If |format| is good, return true and (if |out| isn't null) reset |*out| to
  // a new decoder object. If the |format| is not good, return false.
  bool (*constructor)(const SdpAudioFormat& format,
                      std::unique_ptr<AudioDecoder>* out);
};

// TODO(kwiberg): These factory functions should probably be moved to each
// decoder.
NamedDecoderConstructor decoder_constructors[] = {
    {"pcmu",
     [](const SdpAudioFormat& format, std::unique_ptr<AudioDecoder>* out) {
       if (format.clockrate_hz == 8000 && format.num_channels >= 1) {
         if (out) {
           out->reset(new AudioDecoderPcmU(format.num_channels));
         }
         return true;
       } else {
         return false;
       }
     }},
    {"pcma",
     [](const SdpAudioFormat& format, std::unique_ptr<AudioDecoder>* out) {
       if (format.clockrate_hz == 8000 && format.num_channels >= 1) {
         if (out) {
           out->reset(new AudioDecoderPcmA(format.num_channels));
         }
         return true;
       } else {
         return false;
       }
     }},
#ifdef WEBRTC_CODEC_ILBC
    {"ilbc",
     [](const SdpAudioFormat& format, std::unique_ptr<AudioDecoder>* out) {
       if (format.clockrate_hz == 8000 && format.num_channels == 1) {
         if (out) {
           out->reset(new AudioDecoderIlbc);
         }
         return true;
       } else {
         return false;
       }
     }},
#endif
#if defined(WEBRTC_CODEC_ISACFX)
    {"isac",
     [](const SdpAudioFormat& format, std::unique_ptr<AudioDecoder>* out) {
       if (format.clockrate_hz == 16000 && format.num_channels == 1) {
         if (out) {
           out->reset(new AudioDecoderIsacFix(format.clockrate_hz));
         }
         return true;
       } else {
         return false;
       }
     }},
#elif defined(WEBRTC_CODEC_ISAC)
    {"isac",
     [](const SdpAudioFormat& format, std::unique_ptr<AudioDecoder>* out) {
       if ((format.clockrate_hz == 16000 || format.clockrate_hz == 32000) &&
           format.num_channels == 1) {
         if (out) {
           out->reset(new AudioDecoderIsac(format.clockrate_hz));
         }
         return true;
       } else {
         return false;
       }
     }},
#endif
    {"l16",
     [](const SdpAudioFormat& format, std::unique_ptr<AudioDecoder>* out) {
       if (format.num_channels >= 1) {
         if (out) {
           out->reset(new AudioDecoderPcm16B(format.clockrate_hz,
                                             format.num_channels));
         }
         return true;
       } else {
         return false;
       }
     }},
#ifdef WEBRTC_CODEC_G722
    {"g722",
     [](const SdpAudioFormat& format, std::unique_ptr<AudioDecoder>* out) {
       if (format.clockrate_hz == 8000) {
         if (format.num_channels == 1) {
           if (out) {
             out->reset(new AudioDecoderG722);
           }
           return true;
         } else if (format.num_channels == 2) {
           if (out) {
             out->reset(new AudioDecoderG722Stereo);
           }
           return true;
         }
       }
       return false;
     }},
#endif
#ifdef WEBRTC_CODEC_OPUS
    {"opus",
     [](const SdpAudioFormat& format, std::unique_ptr<AudioDecoder>* out) {
       const rtc::Optional<int> num_channels = [&] {
         auto stereo = format.parameters.find("stereo");
         if (stereo != format.parameters.end()) {
           if (stereo->second == "0") {
             return rtc::Optional<int>(1);
           } else if (stereo->second == "1") {
             return rtc::Optional<int>(2);
           } else {
             return rtc::Optional<int>();  // Bad stereo parameter.
           }
         }
         return rtc::Optional<int>(1);  // Default to mono.
       }();
       if (format.clockrate_hz == 48000 && format.num_channels == 2 &&
           num_channels) {
         if (out) {
           out->reset(new AudioDecoderOpus(*num_channels));
         }
         return true;
       } else {
         return false;
       }
     }},
#endif
};

class BuiltinAudioDecoderFactory : public AudioDecoderFactory {
 public:
  std::vector<AudioCodecSpec> GetSupportedDecoders() override {
    // Although this looks a bit strange, it means specs need only be
    // initialized once, and that that initialization is thread-safe.
    static std::vector<AudioCodecSpec> specs = [] {
      std::vector<AudioCodecSpec> specs;
#ifdef WEBRTC_CODEC_OPUS
      // clang-format off
      AudioCodecSpec opus({"opus", 48000, 2, {
                             {"minptime", "10"},
                             {"useinbandfec", "1"}
                           }});
      // clang-format on
      opus.allow_comfort_noise = false;
      opus.supports_network_adaption = true;
      specs.push_back(opus);
#endif
#if (defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX))
      specs.push_back(AudioCodecSpec({"isac", 16000, 1}));
#endif
#if (defined(WEBRTC_CODEC_ISAC))
      specs.push_back(AudioCodecSpec({"isac", 32000, 1}));
#endif
#ifdef WEBRTC_CODEC_G722
      specs.push_back(AudioCodecSpec({"G722", 8000, 1}));
#endif
#ifdef WEBRTC_CODEC_ILBC
      specs.push_back(AudioCodecSpec({"iLBC", 8000, 1}));
#endif
      specs.push_back(AudioCodecSpec({"PCMU", 8000, 1}));
      specs.push_back(AudioCodecSpec({"PCMA", 8000, 1}));
      return specs;
    }();
    return specs;
  }

  bool IsSupportedDecoder(const SdpAudioFormat& format) override {
    for (const auto& dc : decoder_constructors) {
      if (STR_CASE_CMP(format.name.c_str(), dc.name) == 0) {
        return dc.constructor(format, nullptr);
      }
    }
    return false;
  }

  std::unique_ptr<AudioDecoder> MakeAudioDecoder(
      const SdpAudioFormat& format) override {
    for (const auto& dc : decoder_constructors) {
      if (STR_CASE_CMP(format.name.c_str(), dc.name) == 0) {
        std::unique_ptr<AudioDecoder> decoder;
        bool ok = dc.constructor(format, &decoder);
        RTC_DCHECK_EQ(ok, decoder != nullptr);
        if (decoder) {
          const int expected_sample_rate_hz =
              STR_CASE_CMP(format.name.c_str(), "g722") == 0
                  ? 2 * format.clockrate_hz
                  : format.clockrate_hz;
          RTC_CHECK_EQ(expected_sample_rate_hz, decoder->SampleRateHz());
        }
        return decoder;
      }
    }
    return nullptr;
  }
};

}  // namespace

rtc::scoped_refptr<AudioDecoderFactory>
CreateBuiltinAudioDecoderFactoryInternal() {
  return rtc::scoped_refptr<AudioDecoderFactory>(
      new rtc::RefCountedObject<BuiltinAudioDecoderFactory>);
}

}  // namespace webrtc
