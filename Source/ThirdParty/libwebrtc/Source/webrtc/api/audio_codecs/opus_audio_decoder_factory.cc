/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_codecs/opus_audio_decoder_factory.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "api/audio_codecs/audio_codec_pair_id.h"
#include "api/audio_codecs/audio_decoder.h"
#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/audio_decoder_factory_template.h"
#include "api/audio_codecs/audio_format.h"
#include "api/audio_codecs/opus/audio_decoder_multi_channel_opus.h"
#include "api/audio_codecs/opus/audio_decoder_opus.h"
#include "api/scoped_refptr.h"

namespace webrtc {

namespace {

// Modify an audio decoder to not advertise support for anything.
template <typename T>
struct NotAdvertised {
  using Config = typename T::Config;
  static std::optional<Config> SdpToConfig(const SdpAudioFormat& audio_format) {
    return T::SdpToConfig(audio_format);
  }
  static void AppendSupportedDecoders(
      std::vector<AudioCodecSpec>* /* specs */) {
    // Don't advertise support for anything.
  }
  static std::unique_ptr<AudioDecoder> MakeAudioDecoder(
      Config config,
      std::optional<AudioCodecPairId> codec_pair_id = std::nullopt) {
    return T::MakeAudioDecoder(std::move(config), codec_pair_id);
  }
};

}  // namespace

scoped_refptr<AudioDecoderFactory> CreateOpusAudioDecoderFactory() {
  return CreateAudioDecoderFactory<
      AudioDecoderOpus, NotAdvertised<AudioDecoderMultiChannelOpus>>();
}

}  // namespace webrtc
