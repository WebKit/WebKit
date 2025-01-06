/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/codecs/opus/audio_decoder_opus.h"
#include "test/explicit_key_value_config.h"
#include "test/fuzzers/audio_decoder_fuzzer.h"

namespace webrtc {
void FuzzOneInput(const uint8_t* data, size_t size) {
  const size_t channels = (size % 2) + 1;  // 1 or 2 channels.
  const int kSampleRateHz = 48'000;
  AudioDecoderOpusImpl dec(test::ExplicitKeyValueConfig(""), channels,
                           kSampleRateHz);
  const size_t kAllocatedOuputSizeSamples = kSampleRateHz / 10;  // 100 ms.
  int16_t output[kAllocatedOuputSizeSamples];
  FuzzAudioDecoder(DecoderFunctionType::kRedundantDecode, data, size, &dec,
                   kSampleRateHz, sizeof(output), output);
}
}  // namespace webrtc
