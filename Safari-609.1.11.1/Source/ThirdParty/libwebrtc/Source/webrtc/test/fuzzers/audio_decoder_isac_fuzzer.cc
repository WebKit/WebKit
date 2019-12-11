/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/codecs/isac/main/include/audio_decoder_isac.h"
#include "test/fuzzers/audio_decoder_fuzzer.h"

namespace webrtc {
void FuzzOneInput(const uint8_t* data, size_t size) {
  if (size > 20000) {
    return;
  }
  const int sample_rate_hz = size % 2 == 0 ? 16000 : 32000;     // 16 or 32 kHz.
  static const size_t kAllocatedOuputSizeSamples = 32000 / 10;  // 100 ms.
  int16_t output[kAllocatedOuputSizeSamples];
  AudioDecoderIsacFloatImpl dec(sample_rate_hz);
  FuzzAudioDecoder(DecoderFunctionType::kNormalDecode, data, size, &dec,
                   sample_rate_hz, sizeof(output), output);
}
}  // namespace webrtc
