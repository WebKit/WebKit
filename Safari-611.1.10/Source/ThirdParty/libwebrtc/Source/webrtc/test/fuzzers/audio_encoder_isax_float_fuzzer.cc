/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_codecs/isac/audio_encoder_isac_float.h"
#include "rtc_base/checks.h"
#include "test/fuzzers/audio_encoder_fuzzer.h"

namespace webrtc {

void FuzzOneInput(const uint8_t* data, size_t size) {
  AudioEncoderIsacFloat::Config config;
  config.sample_rate_hz = 16000;
  RTC_CHECK(config.IsOk());
  constexpr int kPayloadType = 100;
  FuzzAudioEncoder(/*data_view=*/{data, size},
                   /*encoder=*/AudioEncoderIsacFloat::MakeAudioEncoder(
                       config, kPayloadType));
}

}  // namespace webrtc
