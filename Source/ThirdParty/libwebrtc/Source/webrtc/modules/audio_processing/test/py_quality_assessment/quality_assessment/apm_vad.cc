// Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <array>
#include <fstream>
#include <memory>

#include "common_audio/wav_file.h"
#include "modules/audio_processing/vad/voice_activity_detector.h"
#include "rtc_base/flags.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace test {
namespace {

constexpr uint8_t kAudioFrameLengthMilliseconds = 10;
constexpr int kMaxSampleRate = 48000;
constexpr size_t kMaxFrameLen =
    kAudioFrameLengthMilliseconds * kMaxSampleRate / 1000;

DEFINE_string(i, "", "Input wav file");
DEFINE_string(o_probs, "", "VAD probabilities output file");
DEFINE_string(o_rms, "", "VAD output file");

int main(int argc, char* argv[]) {
  if (rtc::FlagList::SetFlagsFromCommandLine(&argc, argv, true))
    return 1;

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
  const size_t audio_frame_len = rtc::CheckedDivExact(
      kAudioFrameLengthMilliseconds * wav_reader.sample_rate(), 1000);
  if (audio_frame_len > kMaxFrameLen) {
    RTC_LOG(LS_ERROR) << "The frame size and/or the sample rate are too large.";
    return 1;
  }

  // Create output file and write header.
  std::ofstream out_probs_file(FLAG_o_probs, std::ofstream::binary);
  std::ofstream out_rms_file(FLAG_o_rms, std::ofstream::binary);

  // Run VAD and write decisions.
  VoiceActivityDetector vad;
  std::array<int16_t, kMaxFrameLen> samples;

  while (true) {
    // Process frame.
    const auto read_samples =
        wav_reader.ReadSamples(audio_frame_len, samples.data());
    if (read_samples < audio_frame_len) {
      break;
    }
    vad.ProcessChunk(samples.data(), audio_frame_len, wav_reader.sample_rate());
    // Write output.
    auto probs = vad.chunkwise_voice_probabilities();
    auto rms = vad.chunkwise_rms();
    RTC_CHECK_EQ(probs.size(), rms.size());
    RTC_CHECK_EQ(sizeof(double), 8);

    for (const auto& p : probs) {
      out_probs_file.write(reinterpret_cast<const char*>(&p), 8);
    }
    for (const auto& r : rms) {
      out_rms_file.write(reinterpret_cast<const char*>(&r), 8);
    }
  }

  out_probs_file.close();
  out_rms_file.close();
  return 0;
}

}  // namespace
}  // namespace test
}  // namespace webrtc

int main(int argc, char* argv[]) {
  return webrtc::test::main(argc, argv);
}
