/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/audio_codecs/opus/audio_encoder_opus.h"
#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "rtc_base/checks.h"
#include "test/fuzzers/audio_encoder_fuzzer.h"

namespace webrtc {

void FuzzOneInput(const uint8_t* data, size_t size) {
  // Create Environment once because creating it for each input noticably
  // reduces the speed of the fuzzer.
  static const Environment* const env = new Environment(CreateEnvironment());

  AudioEncoderOpus::Config config;
  config.frame_size_ms = 20;
  RTC_CHECK(config.IsOk());

  FuzzAudioEncoder(
      /*data_view=*/{data, size},
      /*encoder=*/AudioEncoderOpus::MakeAudioEncoder(*env, config,
                                                     {.payload_type = 100}));
}

}  // namespace webrtc
