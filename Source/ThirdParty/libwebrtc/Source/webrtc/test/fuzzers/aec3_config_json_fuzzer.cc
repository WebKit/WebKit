/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>

#include "api/audio/echo_canceller3_config.h"
#include "modules/audio_processing/test/echo_canceller3_config_json.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {
void FuzzOneInput(const uint8_t* data, size_t size) {
  if (size > 10000) {
    return;
  }
  std::string config_json(reinterpret_cast<const char*>(data), size);

  EchoCanceller3Config config;
  bool success;
  Aec3ConfigFromJsonString(config_json, &config, &success);
  EchoCanceller3Config::Validate(&config);
}

}  // namespace webrtc
