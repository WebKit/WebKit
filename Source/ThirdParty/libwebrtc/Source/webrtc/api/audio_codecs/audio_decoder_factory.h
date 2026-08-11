/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_CODECS_AUDIO_DECODER_FACTORY_H_
#define API_AUDIO_CODECS_AUDIO_DECODER_FACTORY_H_

#include <memory>
#include <optional>
#include <vector>

#include "absl/base/nullability.h"
#include "api/audio_codecs/audio_codec_pair_id.h"
#include "api/audio_codecs/audio_decoder.h"
#include "api/audio_codecs/audio_format.h"
#include "api/environment/environment.h"
#include "api/ref_count.h"

namespace webrtc {

// A factory that creates AudioDecoders.
class AudioDecoderFactory : public RefCountInterface {
 public:
  virtual std::vector<AudioCodecSpec> GetSupportedDecoders() = 0;

  virtual bool IsSupportedDecoder(const SdpAudioFormat& format) = 0;

  // Creates a new decoder instance.
  virtual absl_nullable std::unique_ptr<AudioDecoder> Create(
      const Environment& env,
      const SdpAudioFormat& format) {
    return Create(env, format, std::nullopt);
  }
  // Backwards compatible call format. The "codec_pair_id" refers to deleted
  // functionality for linking encoders to decoders; this is no longer used.
  // TODO: https://issues.webrtc.org/398550915 - remove when no longer used,
  // and make above method pure virtual.
  virtual absl_nullable std::unique_ptr<AudioDecoder> Create(
      const Environment& env,
      const SdpAudioFormat& format,
      std::optional<AudioCodecPairId> /* codec_pair_id */) {
    // Note: If neither method is implemented, this default implementation
    // will result in a stack overflow.
    return Create(env, format);
  }
};

}  // namespace webrtc

#endif  // API_AUDIO_CODECS_AUDIO_DECODER_FACTORY_H_
