/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/audio/echo_canceller3_factory.h"

#include <memory>
#include <optional>

#include "absl/base/nullability.h"
#include "api/audio/echo_canceller3_config.h"
#include "api/audio/echo_control.h"
#include "api/audio/neural_residual_echo_estimator.h"
#include "api/environment/environment.h"
#include "modules/audio_processing/aec3/echo_canceller3.h"

namespace webrtc {

EchoCanceller3Factory::EchoCanceller3Factory() {}

EchoCanceller3Factory::EchoCanceller3Factory(const EchoCanceller3Config& config)
    : config_(config), multichannel_config_(std::nullopt) {}

EchoCanceller3Factory::EchoCanceller3Factory(
    const EchoCanceller3Config& config,
    std::optional<EchoCanceller3Config> multichannel_config)
    : config_(config), multichannel_config_(multichannel_config) {}

absl_nonnull std::unique_ptr<EchoControl> EchoCanceller3Factory::Create(
    const Environment& env,
    int sample_rate_hz,
    int num_render_channels,
    int num_capture_channels) {
  return std::make_unique<EchoCanceller3>(
      env, config_, multichannel_config_,
      /*neural_residual_echo_estimator=*/nullptr, sample_rate_hz,
      num_render_channels, num_capture_channels);
}

absl_nonnull std::unique_ptr<EchoControl> EchoCanceller3Factory::Create(
    const Environment& env,
    int sample_rate_hz,
    int num_render_channels,
    int num_capture_channels,
    NeuralResidualEchoEstimator* neural_residual_echo_estimator) {
  return std::make_unique<EchoCanceller3>(
      env, config_, multichannel_config_, neural_residual_echo_estimator,
      sample_rate_hz, num_render_channels, num_capture_channels);
}

}  // namespace webrtc
