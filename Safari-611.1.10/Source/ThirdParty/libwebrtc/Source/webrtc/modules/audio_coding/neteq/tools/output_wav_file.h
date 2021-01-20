/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_TOOLS_OUTPUT_WAV_FILE_H_
#define MODULES_AUDIO_CODING_NETEQ_TOOLS_OUTPUT_WAV_FILE_H_

#include <string>

#include "common_audio/wav_file.h"
#include "modules/audio_coding/neteq/tools/audio_sink.h"
#include "rtc_base/constructor_magic.h"

namespace webrtc {
namespace test {

class OutputWavFile : public AudioSink {
 public:
  // Creates an OutputWavFile, opening a file named |file_name| for writing.
  // The output file is a PCM encoded wav file.
  OutputWavFile(const std::string& file_name,
                int sample_rate_hz,
                int num_channels = 1)
      : wav_writer_(file_name, sample_rate_hz, num_channels) {}

  bool WriteArray(const int16_t* audio, size_t num_samples) override {
    wav_writer_.WriteSamples(audio, num_samples);
    return true;
  }

 private:
  WavWriter wav_writer_;

  RTC_DISALLOW_COPY_AND_ASSIGN(OutputWavFile);
};

}  // namespace test
}  // namespace webrtc
#endif  // MODULES_AUDIO_CODING_NETEQ_TOOLS_OUTPUT_WAV_FILE_H_
