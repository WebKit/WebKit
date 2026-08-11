/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstddef>

#include "api/audio_codecs/opus/audio_encoder_opus.h"
#include "api/environment/environment.h"
#include "api/environment/environment_factory.h"
#include "rtc_base/checks.h"
#include "test/fuzzers/audio_encoder_fuzzer.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {

void FuzzOneInput(FuzzDataHelper fuzz_data) {
  // Create Environment once because creating it for each input noticably
  // reduces the speed of the fuzzer.
  static const Environment* const env = new Environment(CreateEnvironment());

  AudioEncoderOpus::Config config;
  config.frame_size_ms = 20;
  RTC_CHECK(config.IsOk());

  FuzzAudioEncoder(
      /*data=*/fuzz_data,
      /*encoder=*/AudioEncoderOpus::MakeAudioEncoder(*env, std::move(config),
                                                     {.payload_type = 100}));
}

}  // namespace webrtc
