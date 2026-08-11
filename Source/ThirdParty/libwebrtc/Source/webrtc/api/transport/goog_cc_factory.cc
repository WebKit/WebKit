/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/transport/goog_cc_factory.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "api/field_trials_view.h"
#include "api/transport/network_control.h"
#include "api/units/time_delta.h"
#include "modules/congestion_controller/goog_cc/goog_cc_network_control.h"
#include "modules/congestion_controller/goog_cc_scream_network_controller/goog_cc_scream_network_controller.h"
#include "modules/congestion_controller/scream/scream_network_controller.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/logging.h"

namespace webrtc {

namespace {

// If field trial value of the key WebRTC-Bwe-ScreamV2 contain:
//   Enabled - Scream is used if RFC 8888 congestion control is negotiated.
//   mode:always - Scream is used regardless of RTCP feedback.
//   mode:only_after_ce -  GoogCC is iniallally used, but Scream state is
//                         updated in parallel. If ECN CE marks is seen in the
//                         feedback, Scream is used instead of GoogCC for the
//                         remaining duration of the call.
//   mode:goog_cc_with_ect1 - Goog CC is always used, but the controller will
//                            claim to support ECN adaptation until the first
//                            CE mark is seen. After that, packets are not sent
//                            as ECT(1).
constexpr char kScreamFieldTrial[] = "WebRTC-Bwe-ScreamV2";

enum class Mode {
  kGoogCc,
  kScreamWithTwccOrRfc8888,
  kScreamWithRfc8888,
  kScreamAfterCe,
  kGoogCcWithEct1
};

Mode ParseMode(const FieldTrialsView& field_trials) {
  if (field_trials.IsEnabled(kScreamFieldTrial)) {
    return Mode::kScreamWithRfc8888;
  }
  FieldTrialParameter<std::string> mode("mode", "");
  ParseFieldTrial({&mode}, field_trials.Lookup(kScreamFieldTrial));
  if (mode->empty()) {
    return Mode::kGoogCc;
  }
  if (mode.Get() == "always") {
    RTC_LOG(LS_INFO) << "ScreamV2 enabled always";
    return Mode::kScreamWithTwccOrRfc8888;
  }
  if (mode.Get() == "only_after_ce") {
    RTC_LOG(LS_INFO) << "ScreamV2 used after first packet with CE marking.";
    return Mode::kScreamAfterCe;
  }
  if (mode.Get() == "goog_cc_with_ect1") {
    RTC_LOG(LS_INFO)
        << "GoogCC used. Sending packets as ECT1 until first seen CE marking.";
    return Mode::kGoogCcWithEct1;
  }
  return Mode::kGoogCc;
}

}  // namespace

GoogCcNetworkControllerFactory::GoogCcNetworkControllerFactory(
    GoogCcFactoryConfig config)
    : factory_config_(std::move(config)) {}

std::unique_ptr<NetworkControllerInterface>
GoogCcNetworkControllerFactory::Create(NetworkControllerConfig config) {
  GoogCcConfig goog_cc_config;
  if (factory_config_.network_state_estimator_factory) {
    goog_cc_config.network_state_estimator =
        factory_config_.network_state_estimator_factory->Create(
            &config.env.field_trials());
  }
  if (factory_config_.network_state_predictor_factory) {
    goog_cc_config.network_state_predictor =
        factory_config_.network_state_predictor_factory
            ->CreateNetworkStatePredictor();
  }
  Mode mode = ParseMode(config.env.field_trials());
  if (mode == Mode::kScreamWithTwccOrRfc8888 ||
      (mode == Mode::kScreamWithRfc8888 &&
       factory_config_.rfc_8888_feedback_negotiated)) {
    return std::make_unique<ScreamNetworkController>(config);
  }
  if (factory_config_.rfc_8888_feedback_negotiated &&
      (mode == Mode::kScreamAfterCe || mode == Mode::kGoogCcWithEct1)) {
    return std::make_unique<GoogCcScreamNetworkController>(
        config, std::move(goog_cc_config),
        mode == Mode::kScreamAfterCe
            ? GoogCcScreamNetworkController::Mode::kScreamAfterCe
            : GoogCcScreamNetworkController::Mode::kGoogCcWithEct1);
  }
  return std::make_unique<GoogCcNetworkController>(config,
                                                   std::move(goog_cc_config));
}

TimeDelta GoogCcNetworkControllerFactory::GetProcessInterval() const {
  const int64_t kUpdateIntervalMs = 25;
  return TimeDelta::Millis(kUpdateIntervalMs);
}

}  // namespace webrtc
