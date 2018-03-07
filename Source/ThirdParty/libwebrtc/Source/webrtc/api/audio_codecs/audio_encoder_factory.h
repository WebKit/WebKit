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

#include "api/audio_codecs/audio_encoder.h"
#include "api/audio_codecs/audio_format.h"
#include "rtc_base/refcount.h"

namespace webrtc {

// A factory that creates AudioEncoders.
// NOTE: This class is still under development and may change without notice.
class AudioEncoderFactory : public rtc::RefCountInterface {
 public:
  // Returns a prioritized list of audio codecs, to use for signaling etc.
  virtual std::vector<AudioCodecSpec> GetSupportedEncoders() = 0;

  // Returns information about how this format would be encoded, provided it's
  // supported. More format and format variations may be supported than those
  // returned by GetSupportedEncoders().
  virtual rtc::Optional<AudioCodecInfo> QueryAudioEncoder(
      const SdpAudioFormat& format) = 0;

  // Creates an AudioEncoder for the specified format. The encoder will tags its
  // payloads with the specified payload type.
  // TODO(ossu): Try to avoid audio encoders having to know their payload type.
  virtual std::unique_ptr<AudioEncoder> MakeAudioEncoder(
      int payload_type,
      const SdpAudioFormat& format) = 0;
};

}  // namespace webrtc

#endif  // API_AUDIO_CODECS_AUDIO_ENCODER_FACTORY_H_
