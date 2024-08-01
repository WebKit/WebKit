/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_CODECS_AUDIO_ENCODER_FACTORY_H_
#define API_AUDIO_CODECS_AUDIO_ENCODER_FACTORY_H_

#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "api/audio_codecs/audio_codec_pair_id.h"
#include "api/audio_codecs/audio_encoder.h"
#include "api/audio_codecs/audio_format.h"
#include "api/environment/environment.h"
#include "rtc_base/checks.h"
#include "rtc_base/ref_count.h"

namespace webrtc {

// A factory that creates AudioEncoders.
class AudioEncoderFactory : public RefCountInterface {
 public:
  struct Options {
    // TODO(ossu): Try to avoid audio encoders having to know their payload
    // type.
    int payload_type = -1;
    absl::optional<AudioCodecPairId> codec_pair_id;
  };

  // Returns a prioritized list of audio codecs, to use for signaling etc.
  virtual std::vector<AudioCodecSpec> GetSupportedEncoders() = 0;

  // Returns information about how this format would be encoded, provided it's
  // supported. More format and format variations may be supported than those
  // returned by GetSupportedEncoders().
  virtual absl::optional<AudioCodecInfo> QueryAudioEncoder(
      const SdpAudioFormat& format) = 0;

  // Creates an AudioEncoder for the specified format. The encoder will tags its
  // payloads with the specified payload type. The `codec_pair_id` argument is
  // used to link encoders and decoders that talk to the same remote entity: if
  // a AudioEncoderFactory::MakeAudioEncoder() and a
  // AudioDecoderFactory::MakeAudioDecoder() call receive non-null IDs that
  // compare equal, the factory implementations may assume that the encoder and
  // decoder form a pair. (The intended use case for this is to set up
  // communication between the AudioEncoder and AudioDecoder instances, which is
  // needed for some codecs with built-in bandwidth adaptation.)
  //
  // Returns null if the format isn't supported.
  //
  // Note: Implementations need to be robust against combinations other than
  // one encoder, one decoder getting the same ID; such encoders must still
  // work.
  // TODO: bugs.webrtc.org/343086059 - make pure virtual when all
  // implementations of the `AudioEncoderFactory` are updated.
  virtual absl::Nullable<std::unique_ptr<AudioEncoder>>
  Create(const Environment& env, const SdpAudioFormat& format, Options options);

  // TODO: bugs.webrtc.org/343086059 - Update all callers to use `Create`
  // instead, update implementations not to override it, then delete.
  virtual std::unique_ptr<AudioEncoder> MakeAudioEncoder(
      int payload_type,
      const SdpAudioFormat& format,
      absl::optional<AudioCodecPairId> codec_pair_id);
};

//------------------------------------------------------------------------------
// Implementation details follow
//------------------------------------------------------------------------------

inline absl::Nullable<std::unique_ptr<AudioEncoder>>
AudioEncoderFactory::Create(const Environment& env,
                            const SdpAudioFormat& format,
                            Options options) {
  return MakeAudioEncoder(options.payload_type, format, options.codec_pair_id);
}

inline absl::Nullable<std::unique_ptr<AudioEncoder>>
AudioEncoderFactory::MakeAudioEncoder(
    int payload_type,
    const SdpAudioFormat& format,
    absl::optional<AudioCodecPairId> codec_pair_id) {
  // Newer shouldn't call it.
  // Older code should implement it.
  RTC_CHECK_NOTREACHED();
}

}  // namespace webrtc

#endif  // API_AUDIO_CODECS_AUDIO_ENCODER_FACTORY_H_
