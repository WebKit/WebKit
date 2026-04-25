/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "api/transport/test/create_feedback_generator.h"

#include <memory>

#include "api/test/network_emulation_manager.h"
#include "api/transport/test/feedback_generator_interface.h"
#include "test/network/feedback_generator.h"

namespace webrtc {

std::unique_ptr<FeedbackGenerator> CreateFeedbackGenerator(
    FeedbackGenerator::Config confg) {
  return std::make_unique<FeedbackGeneratorImpl>(confg);
}

std::unique_ptr<FeedbackGeneratorWithoutNetwork>
CreateFeedbackGeneratorWithoutNetwork(
    FeedbackGeneratorWithoutNetwork::Config config,
    NetworkEmulationManager& network_emulation_manager) {
  return std::make_unique<FeedbackGeneratorWithoutNetworkImpl>(
      config, network_emulation_manager);
}

}  // namespace webrtc
