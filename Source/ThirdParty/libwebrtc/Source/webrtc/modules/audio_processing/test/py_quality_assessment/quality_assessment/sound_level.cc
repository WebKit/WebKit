// Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>

#include "common_audio/include/audio_util.h"
#include "common_audio/wav_file.h"
#include "rtc_base/flags.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace test {
namespace {

constexpr int kMaxSampleRate = 48000;
constexpr uint8_t kMaxFrameLenMs = 30;
constexpr size_t kMaxFrameLen = kMaxFrameLenMs * kMaxSampleRate / 1000;

const double kOneDbReduction = DbToRatio(-1.0);

WEBRTC_DEFINE_string(i, "", "Input wav file");
WEBRTC_DEFINE_string(oc, "", "Config output file");
WEBRTC_DEFINE_string(ol, "", "Levels output file");
WEBRTC_DEFINE_float(a, 5.f, "Attack (ms)");
WEBRTC_DEFINE_float(d, 20.f, "Decay (ms)");
WEBRTC_DEFINE_int(f, 10, "Frame length (ms)");
WEBRTC_DEFINE_bool(help, false, "prints this message");

int main(int argc, char* argv[]) {
  if (rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true)) {
    rtc::FlagList::Print(nullptr, false);
    return 1;
  }
  if (FLAG_help) {
    rtc::FlagList::Print(nullptr, false);
    return 0;
  }

  // Check parameters.
  if (FLAG_f < 1 || FLAG_f > kMaxFrameLenMs) {
    RTC_LOG(LS_ERROR) << "Invalid frame length (min: 1, max: " << kMaxFrameLenMs
                      << ")";
    return 1;
  }
  if (FLAG_a < 0 || FLAG_d < 0) {
    RTC_LOG(LS_ERROR) << "Attack and decay must be non-negative";
    return 1;
  }

  // Open wav input file and check properties.
  WavReader wav_reader(FLAG_i);
  if (wav_reader.num_channels() != 1) {
    RTC_LOG(LS_ERROR) << "Only mono wav files supported";
    return 1;
  }
  if (wav_reader.sample_rate() > kMaxSampleRate) {
    RTC_LOG(LS_ERROR) << "Beyond maximum sample rate (" << kMaxSampleRate
                      << ")";
    return 1;
  }

  // Map from milliseconds to samples.
  const size_t audio_frame_length =
      rtc::CheckedDivExact(FLAG_f * wav_reader.sample_rate(), 1000);
  auto time_const = [](double c) {
    return std::pow(kOneDbReduction, FLAG_f / c);
  };
  const float attack = FLAG_a == 0.0 ? 0.0 : time_const(FLAG_a);
  const float decay = FLAG_d == 0.0 ? 0.0 : time_const(FLAG_d);

  // Write config to file.
  std::ofstream out_config(FLAG_oc);
  out_config << "{"
             << "'frame_len_ms': " << FLAG_f << ", "
             << "'attack_ms': " << FLAG_a << ", "
             << "'decay_ms': " << FLAG_d << "}\n";
  out_config.close();

  // Measure level frame-by-frame.
  std::ofstream out_levels(FLAG_ol, std::ofstream::binary);
  std::array<int16_t, kMaxFrameLen> samples;
  float level_prev = 0.f;
  while (true) {
    // Process frame.
    const auto read_samples =
        wav_reader.ReadSamples(audio_frame_length, samples.data());
    if (read_samples < audio_frame_length)
      break;  // EOF.

    // Frame peak level.
    std::transform(samples.begin(), samples.begin() + audio_frame_length,
                   samples.begin(), [](int16_t s) { return std::abs(s); });
    const auto* peak_level =
        std::max_element(samples.begin(), samples.begin() + audio_frame_length);
    const float level_curr = static_cast<float>(*peak_level) / 32768.f;

    // Temporal smoothing.
    auto smooth = [&level_prev, &level_curr](float c) {
      return (1.0 - c) * level_curr + c * level_prev;
    };
    level_prev = smooth(level_curr > level_prev ? attack : decay);

    // Write output.
    out_levels.write(reinterpret_cast<const char*>(&level_prev), sizeof(float));
  }
  out_levels.close();

  return 0;
}

}  // namespace
}  // namespace test
}  // namespace webrtc

int main(int argc, char* argv[]) {
  return webrtc::test::main(argc, argv);
}
