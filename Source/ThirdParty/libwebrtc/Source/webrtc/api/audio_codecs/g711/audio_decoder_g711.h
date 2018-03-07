/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_CODECS_G711_AUDIO_DECODER_G711_H_
#define API_AUDIO_CODECS_G711_AUDIO_DECODER_G711_H_

#include <memory>
#include <vector>

#include "api/audio_codecs/audio_decoder.h"
#include "api/audio_codecs/audio_format.h"
#include "api/optional.h"

namespace webrtc {

// G711 decoder API for use as a template parameter to
// CreateAudioDecoderFactory<...>().
//
// NOTE: This struct is still under development and may change without notice.
struct AudioDecoderG711 {
  struct Config {
    enum class Type { kPcmU, kPcmA };
    bool IsOk() const {
      return (type == Type::kPcmU || type == Type::kPcmA) && num_channels >= 1;
    }
    Type type;
    int num_channels;
  };
  static rtc::Optional<Config> SdpToConfig(const SdpAudioFormat& audio_format);
  static void AppendSupportedDecoders(std::vector<AudioCodecSpec>* specs);
  static std::unique_ptr<AudioDecoder> MakeAudioDecoder(const Config& config);
};

}  // namespace webrtc

#endif  // API_AUDIO_CODECS_G711_AUDIO_DECODER_G711_H_
