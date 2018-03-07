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

#include "common_audio/vad/include/vad.h"
#include "common_audio/wav_file.h"
#include "rtc_base/flags.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace test {
namespace {

// The allowed values are 10, 20 or 30 ms.
constexpr uint8_t kAudioFrameLengthMilliseconds = 30;
constexpr int kMaxSampleRate = 48000;
constexpr size_t kMaxFrameLen =
    kAudioFrameLengthMilliseconds * kMaxSampleRate / 1000;

constexpr uint8_t kBitmaskBuffSize = 8;

DEFINE_string(i, "", "Input wav file");
DEFINE_string(o, "", "VAD output file");

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
  const size_t audio_frame_length = rtc::CheckedDivExact(
      kAudioFrameLengthMilliseconds * wav_reader.sample_rate(), 1000);
  if (audio_frame_length > kMaxFrameLen) {
    RTC_LOG(LS_ERROR) << "The frame size and/or the sample rate are too large.";
    return 1;
  }

  // Create output file and write header.
  std::ofstream out_file(FLAG_o, std::ofstream::binary);
  const char audio_frame_length_ms = kAudioFrameLengthMilliseconds;
  out_file.write(&audio_frame_length_ms, 1);  // Header.

  // Run VAD and write decisions.
  std::unique_ptr<Vad> vad = CreateVad(Vad::Aggressiveness::kVadNormal);
  std::array<int16_t, kMaxFrameLen> samples;
  char buff = 0;     // Buffer to write one bit per frame.
  uint8_t next = 0;  // Points to the next bit to write in |buff|.
  while (true) {
    // Process frame.
    const auto read_samples =
        wav_reader.ReadSamples(audio_frame_length, samples.data());
    if (read_samples < audio_frame_length)
      break;
    const auto is_speech = vad->VoiceActivity(
        samples.data(), audio_frame_length, wav_reader.sample_rate());

    // Write output.
    buff = is_speech ? buff | (1 << next) : buff & ~(1 << next);
    if (++next == kBitmaskBuffSize) {
      out_file.write(&buff, 1);  // Flush.
      buff = 0;                  // Reset.
      next = 0;
    }
  }

  // Finalize.
  char extra_bits = 0;
  if (next > 0) {
    extra_bits = kBitmaskBuffSize - next;
    out_file.write(&buff, 1);  // Flush.
  }
  out_file.write(&extra_bits, 1);
  out_file.close();

  return 0;
}

}  // namespace
}  // namespace test
}  // namespace webrtc

int main(int argc, char* argv[]) {
  return webrtc::test::main(argc, argv);
}
