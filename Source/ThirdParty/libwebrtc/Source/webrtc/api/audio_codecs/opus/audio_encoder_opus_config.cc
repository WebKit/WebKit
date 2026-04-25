/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_codecs/opus/audio_encoder_opus_config.h"

#include "api/audio_codecs/audio_encoder.h"

namespace webrtc {

const int AudioEncoderOpusConfig::kDefaultLowRateComplexity =
    WEBRTC_OPUS_VARIABLE_COMPLEXITY ? 9 : kDefaultComplexity;

bool AudioEncoderOpusConfig::IsOk() const {
  if (frame_size_ms <= 0 || frame_size_ms % 10 != 0)
    return false;
  if (sample_rate_hz != 16000 && sample_rate_hz != 48000) {
    // Unsupported input sample rate. (libopus supports a few other rates as
    // well; we can add support for them when needed.)
    return false;
  }
  if (num_channels > AudioEncoder::kMaxNumberOfChannels) {
    return false;
  }
  if (!bitrate_bps)
    return false;
  if (*bitrate_bps < kMinBitrateBps || *bitrate_bps > kMaxBitrateBps)
    return false;
  if (complexity < 0 || complexity > 10)
    return false;
  if (low_rate_complexity < 0 || low_rate_complexity > 10)
    return false;
  return true;
}
}  // namespace webrtc
