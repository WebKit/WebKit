/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/neteq/delay_manager.h"

#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <memory>

#include "api/field_trials_view.h"
#include "api/neteq/tick_timer.h"
#include "modules/audio_coding/neteq/reorder_optimizer.h"
#include "rtc_base/experiments/struct_parameters_parser.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

constexpr int kStartDelayMs = 80;

std::unique_ptr<ReorderOptimizer> MaybeCreateReorderOptimizer(
    const DelayManager::Config& config) {
  if (!config.use_reorder_optimizer) {
    return nullptr;
  }
  return std::make_unique<ReorderOptimizer>(
      (1 << 15) * config.reorder_forget_factor, config.ms_per_loss_percent,
      config.start_forget_weight);
}

}  // namespace

DelayManager::Config::Config(const FieldTrialsView& field_trials) {
  StructParametersParser::Create(                       //
      "quantile", &quantile,                            //
      "forget_factor", &forget_factor,                  //
      "start_forget_weight", &start_forget_weight,      //
      "resample_interval_ms", &resample_interval_ms,    //
      "use_reorder_optimizer", &use_reorder_optimizer,  //
      "reorder_forget_factor", &reorder_forget_factor,  //
      "ms_per_loss_percent", &ms_per_loss_percent)
      ->Parse(field_trials.Lookup("WebRTC-Audio-NetEqDelayManagerConfig"));
}

void DelayManager::Config::Log() {
  RTC_LOG(LS_INFO) << "Delay manager config:"
                      " quantile="
                   << quantile << " forget_factor=" << forget_factor
                   << " start_forget_weight=" << start_forget_weight.value_or(0)
                   << " resample_interval_ms="
                   << resample_interval_ms.value_or(0)
                   << " use_reorder_optimizer=" << use_reorder_optimizer
                   << " reorder_forget_factor=" << reorder_forget_factor
                   << " ms_per_loss_percent=" << ms_per_loss_percent;
}

DelayManager::DelayManager(const Config& config, const TickTimer* tick_timer)
    : underrun_optimizer_(tick_timer,
                          (1 << 30) * config.quantile,
                          (1 << 15) * config.forget_factor,
                          config.start_forget_weight,
                          config.resample_interval_ms),
      reorder_optimizer_(MaybeCreateReorderOptimizer(config)),
      target_level_ms_(kStartDelayMs) {
  Reset();
}

DelayManager::~DelayManager() {}

void DelayManager::Update(int arrival_delay_ms, bool reordered) {
  if (!reorder_optimizer_ || !reordered) {
    underrun_optimizer_.Update(arrival_delay_ms);
  }
  target_level_ms_ =
      underrun_optimizer_.GetOptimalDelayMs().value_or(kStartDelayMs);
  if (reorder_optimizer_) {
    reorder_optimizer_->Update(arrival_delay_ms, reordered, target_level_ms_);
    target_level_ms_ = std::max(
        target_level_ms_, reorder_optimizer_->GetOptimalDelayMs().value_or(0));
  }
}

void DelayManager::Reset() {
  underrun_optimizer_.Reset();
  target_level_ms_ = kStartDelayMs;
  if (reorder_optimizer_) {
    reorder_optimizer_->Reset();
  }
}

int DelayManager::TargetDelayMs() const {
  return target_level_ms_;
}


}  // namespace webrtc
