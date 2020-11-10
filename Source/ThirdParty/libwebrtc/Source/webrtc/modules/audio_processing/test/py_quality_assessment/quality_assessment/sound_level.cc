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

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "common_audio/include/audio_util.h"
#include "common_audio/wav_file.h"
#include "rtc_base/logging.h"

ABSL_FLAG(std::string, i, "", "Input wav file");
ABSL_FLAG(std::string, oc, "", "Config output file");
ABSL_FLAG(std::string, ol, "", "Levels output file");
ABSL_FLAG(float, a, 5.f, "Attack (ms)");
ABSL_FLAG(float, d, 20.f, "Decay (ms)");
ABSL_FLAG(int, f, 10, "Frame length (ms)");

namespace webrtc {
namespace test {
namespace {

constexpr int kMaxSampleRate = 48000;
constexpr uint8_t kMaxFrameLenMs = 30;
constexpr size_t kMaxFrameLen = kMaxFrameLenMs * kMaxSampleRate / 1000;

const double kOneDbReduction = DbToRatio(-1.0);

int main(int argc, char* argv[]) {
  absl::ParseCommandLine(argc, argv);
  // Check parameters.
  if (absl::GetFlag(FLAGS_f) < 1 || absl::GetFlag(FLAGS_f) > kMaxFrameLenMs) {
    RTC_LOG(LS_ERROR) << "Invalid frame length (min: 1, max: " << kMaxFrameLenMs
                      << ")";
    return 1;
  }
  if (absl::GetFlag(FLAGS_a) < 0 || absl::GetFlag(FLAGS_d) < 0) {
    RTC_LOG(LS_ERROR) << "Attack and decay must be non-negative";
    return 1;
  }

  // Open wav input file and check properties.
  const std::string input_file = absl::GetFlag(FLAGS_i);
  const std::string config_output_file = absl::GetFlag(FLAGS_oc);
  const std::string levels_output_file = absl::GetFlag(FLAGS_ol);
  WavReader wav_reader(input_file);
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
  const size_t audio_frame_length = rtc::CheckedDivExact(
      absl::GetFlag(FLAGS_f) * wav_reader.sample_rate(), 1000);
  auto time_const = [](double c) {
    return std::pow(kOneDbReduction, absl::GetFlag(FLAGS_f) / c);
  };
  const float attack =
      absl::GetFlag(FLAGS_a) == 0.0 ? 0.0 : time_const(absl::GetFlag(FLAGS_a));
  const float decay =
      absl::GetFlag(FLAGS_d) == 0.0 ? 0.0 : time_const(absl::GetFlag(FLAGS_d));

  // Write config to file.
  std::ofstream out_config(config_output_file);
  out_config << "{"
                "'frame_len_ms': "
             << absl::GetFlag(FLAGS_f)
             << ", "
                "'attack_ms': "
             << absl::GetFlag(FLAGS_a)
             << ", "
                "'decay_ms': "
             << absl::GetFlag(FLAGS_d) << "}\n";
  out_config.close();

  // Measure level frame-by-frame.
  std::ofstream out_levels(levels_output_file, std::ofstream::binary);
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
    const int16_t peak_level = *std::max_element(
        samples.cbegin(), samples.cbegin() + audio_frame_length);
    const float level_curr = static_cast<float>(peak_level) / 32768.f;

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
