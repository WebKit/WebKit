/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/codecs/ilbc/audio_decoder_ilbc.h"
#include "webrtc/test/fuzzers/audio_decoder_fuzzer.h"

namespace webrtc {
void FuzzOneInput(const uint8_t* data, size_t size) {
  AudioDecoderIlbc dec;
  static const int kSampleRateHz = 8000;
  static const size_t kAllocatedOuputSizeSamples = kSampleRateHz / 10;
  int16_t output[kAllocatedOuputSizeSamples];
  FuzzAudioDecoder(DecoderFunctionType::kNormalDecode, data, size, &dec,
                   kSampleRateHz, sizeof(output), output);
}
}  // namespace webrtc
