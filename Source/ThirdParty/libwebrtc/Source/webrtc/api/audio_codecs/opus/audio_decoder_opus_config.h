/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_CODECS_OPUS_AUDIO_DECODER_OPUS_CONFIG_H_
#define API_AUDIO_CODECS_OPUS_AUDIO_DECODER_OPUS_CONFIG_H_

#include <optional>

#include "rtc_base/system/rtc_export.h"

namespace webrtc {

struct RTC_EXPORT AudioDecoderOpusConfig {
  bool IsOk() const {
    if (sample_rate_hz != 16'000 && sample_rate_hz != 48'000) {
      return false;
    }
    return !num_channels.has_value() || *num_channels == 1 ||
           *num_channels == 2;
  }

  int sample_rate_hz = 48'000;
  std::optional<int> num_channels;
};

}  // namespace webrtc

#endif  // API_AUDIO_CODECS_OPUS_AUDIO_DECODER_OPUS_CONFIG_H_
