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

#include "modules/audio_coding/codecs/g711/audio_decoder_pcm.h"
#include "test/fuzzers/audio_decoder_fuzzer.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {
void FuzzOneInput(FuzzDataHelper fuzz_data) {
  if (fuzz_data.size() > 10'000 || fuzz_data.size() < 2) {
    return;
  }

  const size_t num_channels = fuzz_data.Read<uint8_t>() % 16 + 1;

  std::unique_ptr<AudioDecoder> dec;
  if (fuzz_data.Read<uint8_t>() % 2) {
    dec = std::make_unique<AudioDecoderPcmU>(num_channels);
  } else {
    dec = std::make_unique<AudioDecoderPcmA>(num_channels);
  }

  // Allocate a maximum output size of 100 ms.
  const size_t allocated_ouput_size_samples =
      dec->SampleRateHz() * num_channels / 10;
  std::unique_ptr<int16_t[]> output =
      std::make_unique<int16_t[]>(allocated_ouput_size_samples);
  FuzzAudioDecoder(DecoderFunctionType::kNormalDecode, fuzz_data, dec.get(),
                   dec->SampleRateHz(),
                   allocated_ouput_size_samples * sizeof(int16_t),
                   output.get());
}
}  // namespace webrtc
