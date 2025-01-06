/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "call/create_call.h"

#include <memory>
#include <optional>

#include "api/test/simulated_network.h"
#include "api/units/time_delta.h"
#include "call/call.h"

namespace webrtc {

std::unique_ptr<Call> CreateCall(CallConfig config) {
  std::vector<DegradedCall::TimeScopedNetworkConfig> send_degradation_configs =
      GetNetworkConfigs(config.env.field_trials(), /*send=*/true);
  std::vector<DegradedCall::TimeScopedNetworkConfig>
      receive_degradation_configs =
          GetNetworkConfigs(config.env.field_trials(), /*send=*/false);

  std::unique_ptr<Call> call = Call::Create(std::move(config));

  if (!send_degradation_configs.empty() ||
      !receive_degradation_configs.empty()) {
    return std::make_unique<DegradedCall>(
        std::move(call), send_degradation_configs, receive_degradation_configs);
  }

  return call;
}

}  // namespace webrtc
