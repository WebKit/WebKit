/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/neteq/default_neteq_controller_factory.h"

#include <memory>
#include <utility>

#include "api/environment/environment.h"
#include "api/neteq/neteq_controller.h"
#include "modules/audio_coding/neteq/decision_logic.h"
#include "modules/audio_coding/neteq/delay_manager.h"

namespace webrtc {

DefaultNetEqControllerFactory::DefaultNetEqControllerFactory() = default;
DefaultNetEqControllerFactory::~DefaultNetEqControllerFactory() = default;

std::unique_ptr<NetEqController> DefaultNetEqControllerFactory::Create(
    const Environment& env,
    const NetEqController::Config& config) const {
  auto delay_manager = std::make_unique<DelayManager>(
      DelayManager::Config(env.field_trials()), config.tick_timer);
  return std::make_unique<DecisionLogic>(env, config, std::move(delay_manager));
}

}  // namespace webrtc
