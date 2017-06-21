/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_AUDIO_CODECS_G722_AUDIO_DECODER_G722_H_
#define WEBRTC_API_AUDIO_CODECS_G722_AUDIO_DECODER_G722_H_

#include <memory>
#include <vector>

#include "webrtc/api/audio_codecs/audio_decoder.h"
#include "webrtc/api/audio_codecs/audio_format.h"
#include "webrtc/base/optional.h"

namespace webrtc {

// G722 decoder API for use as a template parameter to
// CreateAudioDecoderFactory<...>().
//
// NOTE: This struct is still under development and may change without notice.
struct AudioDecoderG722 {
  struct Config {};  // Empty---no config values needed!
  static rtc::Optional<Config> SdpToConfig(const SdpAudioFormat& audio_format);
  static void AppendSupportedDecoders(std::vector<AudioCodecSpec>* specs);
  static std::unique_ptr<AudioDecoder> MakeAudioDecoder(Config config);
};

}  // namespace webrtc

#endif  // WEBRTC_API_AUDIO_CODECS_G722_AUDIO_DECODER_G722_H_
