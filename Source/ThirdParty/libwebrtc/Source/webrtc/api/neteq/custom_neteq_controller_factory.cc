/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/neteq/custom_neteq_controller_factory.h"

#include <memory>
#include <utility>

#include "api/environment/environment.h"
#include "api/neteq/delay_manager_factory.h"
#include "api/neteq/neteq_controller.h"
#include "modules/audio_coding/neteq/decision_logic.h"
#include "rtc_base/checks.h"

namespace webrtc {

CustomNetEqControllerFactory::CustomNetEqControllerFactory(
    std::unique_ptr<DelayManagerFactory> delay_manager_factory)
    : delay_manager_factory_(std::move(delay_manager_factory)) {}
CustomNetEqControllerFactory::~CustomNetEqControllerFactory() = default;

std::unique_ptr<NetEqController> CustomNetEqControllerFactory::Create(
    const Environment& env,
    const NetEqController::Config& config) const {
  RTC_DCHECK(delay_manager_factory_);
  return std::make_unique<DecisionLogic>(
      env, config,
      delay_manager_factory_->Create(env.field_trials(), config.tick_timer));
}

}  // namespace webrtc
