/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_CODECS_OPUS_AUDIO_ENCODER_MULTI_CHANNEL_OPUS_CONFIG_H_
#define API_AUDIO_CODECS_OPUS_AUDIO_ENCODER_MULTI_CHANNEL_OPUS_CONFIG_H_

#include <stddef.h>

#include <vector>

#include "rtc_base/system/rtc_export.h"

namespace webrtc {

struct RTC_EXPORT AudioEncoderMultiChannelOpusConfig {
  static constexpr int kDefaultFrameSizeMs = 20;
  static constexpr int kDefaultComplexity = 9;

  // Opus API allows a min bitrate of 500bps, but Opus documentation suggests
  // bitrate should be in the range of 6000 to 510000, inclusive.
  static constexpr int kMinBitrateBps = 6000;
  static constexpr int kMaxBitrateBps = 510000;

  bool IsOk() const;

  int frame_size_ms = kDefaultFrameSizeMs;
  size_t num_channels = 1;
  enum class ApplicationMode { kVoip, kAudio };
  ApplicationMode application = ApplicationMode::kVoip;
  int bitrate_bps = 32000;
  bool fec_enabled = false;
  bool cbr_enabled = false;
  bool dtx_enabled = false;
  int max_playback_rate_hz = 48000;
  std::vector<int> supported_frame_lengths_ms;

  int complexity = kDefaultComplexity;

  // Number of mono/stereo Opus streams.
  int num_streams = -1;

  // Number of channel pairs coupled together, see RFC 7845 section
  // 5.1.1. Has to be less than the number of streams
  int coupled_streams = -1;

  // Channel mapping table, defines the mapping from encoded streams to input
  // channels. See RFC 7845 section 5.1.1.
  std::vector<unsigned char> channel_mapping;
};

}  // namespace webrtc
#endif  // API_AUDIO_CODECS_OPUS_AUDIO_ENCODER_MULTI_CHANNEL_OPUS_CONFIG_H_
