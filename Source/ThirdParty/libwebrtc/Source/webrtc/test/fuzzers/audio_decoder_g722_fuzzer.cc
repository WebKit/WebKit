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

#include "modules/audio_coding/codecs/g722/audio_decoder_g722.h"
#include "test/fuzzers/audio_decoder_fuzzer.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {
void FuzzOneInput(FuzzDataHelper fuzz_data) {
  if (fuzz_data.size() > 10'000 || fuzz_data.size() < 1) {
    return;
  }

  std::unique_ptr<AudioDecoder> dec;
  size_t num_channels;
  if (fuzz_data.Read<uint8_t>() % 2) {
    dec = std::make_unique<AudioDecoderG722Impl>();
    num_channels = 1;
  } else {
    dec = std::make_unique<AudioDecoderG722StereoImpl>();
    num_channels = 2;
  }
  // Allocate a maximum output size of 100 ms.
  const int sample_rate_hz = dec->SampleRateHz();
  const size_t allocated_ouput_size_samples =
      sample_rate_hz / 10 * num_channels;
  std::unique_ptr<int16_t[]> output =
      std::make_unique<int16_t[]>(allocated_ouput_size_samples);
  FuzzAudioDecoder(
      DecoderFunctionType::kNormalDecode, fuzz_data, dec.get(), sample_rate_hz,
      allocated_ouput_size_samples * sizeof(int16_t), output.get());
}
}  // namespace webrtc
