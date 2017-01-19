/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_AUDIO_FORMAT_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_AUDIO_FORMAT_H_

#include <map>
#include <ostream>
#include <string>
#include <utility>

namespace webrtc {

// SDP specification for a single audio codec.
// NOTE: This class is still under development and may change without notice.
struct SdpAudioFormat {
  using Parameters = std::map<std::string, std::string>;

  SdpAudioFormat(const SdpAudioFormat&);
  SdpAudioFormat(SdpAudioFormat&&);
  SdpAudioFormat(const char* name, int clockrate_hz, int num_channels);
  SdpAudioFormat(const char* name,
                 int clockrate_hz,
                 int num_channels,
                 Parameters&& param);
  ~SdpAudioFormat();

  SdpAudioFormat& operator=(const SdpAudioFormat&);
  SdpAudioFormat& operator=(SdpAudioFormat&&);

  friend bool operator==(const SdpAudioFormat& a, const SdpAudioFormat& b);
  friend bool operator!=(const SdpAudioFormat& a, const SdpAudioFormat& b) {
    return !(a == b);
  }

  std::string name;
  int clockrate_hz;
  int num_channels;
  Parameters parameters;
};

void swap(SdpAudioFormat& a, SdpAudioFormat& b);
std::ostream& operator<<(std::ostream& os, const SdpAudioFormat& saf);

struct AudioCodecSpec {
  SdpAudioFormat format;
  bool allow_comfort_noise;  // This encoder can be used with an external
                             // comfort noise generator.
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_CODECS_AUDIO_FORMAT_H_
