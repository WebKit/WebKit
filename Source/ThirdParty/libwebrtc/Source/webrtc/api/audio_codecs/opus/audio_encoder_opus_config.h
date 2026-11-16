/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_CODECS_OPUS_AUDIO_ENCODER_OPUS_CONFIG_H_
#define API_AUDIO_CODECS_OPUS_AUDIO_ENCODER_OPUS_CONFIG_H_

#include <stddef.h>

#include <optional>
#include <vector>

#include "rtc_base/system/rtc_export.h"

namespace webrtc {

struct RTC_EXPORT AudioEncoderOpusConfig {
  static const int kDefaultLowRateComplexity;
  static constexpr int kDefaultFrameSizeMs = 20;

  // Opus API allows a min bitrate of 500bps, but Opus documentation suggests
  // bitrate should be in the range of 6000 to 510000, inclusive.
  static constexpr int kMinBitrateBps = 6'000;
  static constexpr int kMaxBitrateBps = 510'000;
#if defined(WEBRTC_ANDROID) || defined(WEBRTC_IOS)
  static constexpr int kDefaultComplexity = 5;
#else
  static constexpr int kDefaultComplexity = 9;
#endif

  bool IsOk() const;  // Checks if the values are currently OK.

  int frame_size_ms = kDefaultFrameSizeMs;
  int sample_rate_hz = 48'000;
  size_t num_channels = 1;
  enum class ApplicationMode { kVoip, kAudio };
  ApplicationMode application = ApplicationMode::kVoip;

  // NOTE: This member must always be set.
  // TODO(kwiberg): Turn it into just an int.
  std::optional<int> bitrate_bps = 32'000;

  bool fec_enabled = false;
  bool cbr_enabled = false;
  int max_playback_rate_hz = 48'000;

  // `complexity` is used when the bitrate goes above
  // `complexity_threshold_bps` + `complexity_threshold_window_bps`;
  // `low_rate_complexity` is used when the bitrate falls below
  // `complexity_threshold_bps` - `complexity_threshold_window_bps`. In the
  // interval in the middle, we keep using the most recent of the two
  // complexity settings.
  int complexity = kDefaultComplexity;
  int low_rate_complexity = kDefaultLowRateComplexity;
  int complexity_threshold_bps = 12500;
  int complexity_threshold_window_bps = 1500;

  bool dtx_enabled = false;
  std::vector<int> supported_frame_lengths_ms;
  int uplink_bandwidth_update_interval_ms = 200;

  // NOTE: This member isn't necessary, and will soon go away. See
  // https://bugs.chromium.org/p/webrtc/issues/detail?id=7847
  int payload_type = -1;
};

}  // namespace webrtc

#endif  // API_AUDIO_CODECS_OPUS_AUDIO_ENCODER_OPUS_CONFIG_H_
