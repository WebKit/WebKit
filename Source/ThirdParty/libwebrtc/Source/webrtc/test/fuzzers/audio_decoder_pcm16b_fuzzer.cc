/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstddef>
#include <cstdint>
#include <memory>

#include "modules/audio_coding/codecs/pcm16b/audio_decoder_pcm16b.h"
#include "rtc_base/checks.h"
#include "test/fuzzers/audio_decoder_fuzzer.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {
void FuzzOneInput(FuzzDataHelper fuzz_data) {
  if (fuzz_data.size() > 10'000 || fuzz_data.size() < 2) {
    return;
  }

  int sample_rate_hz;
  switch (fuzz_data.Read<uint8_t>() % 4) {
    case 0:
      sample_rate_hz = 8000;
      break;
    case 1:
      sample_rate_hz = 16000;
      break;
    case 2:
      sample_rate_hz = 32000;
      break;
    case 3:
      sample_rate_hz = 48000;
      break;
    default:
      RTC_DCHECK_NOTREACHED();
      return;
  }
  const size_t num_channels = fuzz_data.Read<uint8_t>() % 16 + 1;

  AudioDecoderPcm16B dec(sample_rate_hz, num_channels);
  // Allocate a maximum output size of 100 ms.
  const size_t allocated_ouput_size_samples =
      sample_rate_hz * num_channels / 10;
  std::unique_ptr<int16_t[]> output =
      std::make_unique<int16_t[]>(allocated_ouput_size_samples);
  FuzzAudioDecoder(
      DecoderFunctionType::kNormalDecode, fuzz_data, &dec, sample_rate_hz,
      allocated_ouput_size_samples * sizeof(int16_t), output.get());
}
}  // namespace webrtc
