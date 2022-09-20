/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>

#include "modules/audio_coding/codecs/g711/audio_decoder_pcm.h"
#include "test/fuzzers/audio_decoder_fuzzer.h"

namespace webrtc {
void FuzzOneInput(const uint8_t* data, size_t size) {
  if (size > 10000 || size < 2) {
    return;
  }

  const size_t num_channels = data[0] % 16 + 1;

  std::unique_ptr<AudioDecoder> dec;
  if (data[1] % 2) {
    dec = std::make_unique<AudioDecoderPcmU>(num_channels);
  } else {
    dec = std::make_unique<AudioDecoderPcmA>(num_channels);
  }

  // Two first bytes of the data are used. Move forward.
  data += 2;
  size -= 2;

  // Allocate a maximum output size of 100 ms.
  const size_t allocated_ouput_size_samples =
      dec->SampleRateHz() * num_channels / 10;
  std::unique_ptr<int16_t[]> output =
      std::make_unique<int16_t[]>(allocated_ouput_size_samples);
  FuzzAudioDecoder(DecoderFunctionType::kNormalDecode, data, size, dec.get(),
                   dec->SampleRateHz(),
                   allocated_ouput_size_samples * sizeof(int16_t),
                   output.get());
}
}  // namespace webrtc
