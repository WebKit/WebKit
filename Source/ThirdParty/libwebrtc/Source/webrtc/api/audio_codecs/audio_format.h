/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_AUDIO_CODECS_AUDIO_FORMAT_H_
#define WEBRTC_API_AUDIO_CODECS_AUDIO_FORMAT_H_

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
  SdpAudioFormat(const std::string& name, int clockrate_hz, int num_channels);
  SdpAudioFormat(const char* name,
                 int clockrate_hz,
                 int num_channels,
                 const Parameters& param);
  SdpAudioFormat(const std::string& name,
                 int clockrate_hz,
                 int num_channels,
                 const Parameters& param);
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

// To avoid API breakage, and make the code clearer, AudioCodecSpec should not
// be directly initializable with any flags indicating optional support. If it
// were, these initializers would break any time a new flag was added. It's also
// more difficult to understand:
//   AudioCodecSpec spec{{"format", 8000, 1}, true, false, false, true, true};
// than
//   AudioCodecSpec spec({"format", 8000, 1});
//   spec.allow_comfort_noise = true;
//   spec.future_flag_b = true;
//   spec.future_flag_c = true;
struct AudioCodecSpec {
  explicit AudioCodecSpec(const SdpAudioFormat& format);
  explicit AudioCodecSpec(SdpAudioFormat&& format);
  ~AudioCodecSpec() = default;

  SdpAudioFormat format;
  bool allow_comfort_noise = true;  // This codec can be used with an external
                                    // comfort noise generator.
  bool supports_network_adaption = false;  // This codec can adapt to varying
                                           // network conditions.
};

}  // namespace webrtc

#endif  // WEBRTC_API_AUDIO_CODECS_AUDIO_FORMAT_H_
