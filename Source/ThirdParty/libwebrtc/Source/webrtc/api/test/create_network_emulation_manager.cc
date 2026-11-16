
/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/create_network_emulation_manager.h"

#include <memory>
#include <utility>

#include "api/field_trials_view.h"
#include "api/test/network_emulation_manager.h"
#include "test/network/network_emulation_manager.h"

namespace webrtc {

std::unique_ptr<NetworkEmulationManager> CreateNetworkEmulationManager(
    NetworkEmulationManagerConfig config) {
  return std::make_unique<test::NetworkEmulationManagerImpl>(std::move(config));
}

std::unique_ptr<NetworkEmulationManager> CreateNetworkEmulationManager(
    TimeMode time_mode,
    EmulatedNetworkStatsGatheringMode stats_gathering_mode,
    const FieldTrialsView* field_trials) {
  return CreateNetworkEmulationManager(
      {.time_mode = time_mode,
       .stats_gathering_mode = stats_gathering_mode,
       .field_trials = field_trials});
}

}  // namespace webrtc
