/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/audio/echo_canceller3_config.h"

namespace webrtc {

EchoCanceller3Config::EchoCanceller3Config() = default;
EchoCanceller3Config::EchoCanceller3Config(const EchoCanceller3Config& e) =
    default;
EchoCanceller3Config::Mask::Mask() = default;
EchoCanceller3Config::Mask::Mask(const EchoCanceller3Config::Mask& m) = default;

EchoCanceller3Config::EchoModel::EchoModel() = default;
EchoCanceller3Config::EchoModel::EchoModel(
    const EchoCanceller3Config::EchoModel& e) = default;

}  // namespace webrtc
