/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TRANSPORT_GOOG_CC_FACTORY_H_
#define API_TRANSPORT_GOOG_CC_FACTORY_H_
#include <memory>

#include "absl/base/attributes.h"
#include "api/network_state_predictor.h"
#include "api/transport/network_control.h"

namespace webrtc {
class RtcEventLog;

struct GoogCcFactoryConfig {
  std::unique_ptr<NetworkStateEstimatorFactory>
      network_state_estimator_factory = nullptr;
  NetworkStatePredictorFactoryInterface* network_state_predictor_factory =
      nullptr;
  bool feedback_only = false;
};

class GoogCcNetworkControllerFactory
    : public NetworkControllerFactoryInterface {
 public:
  GoogCcNetworkControllerFactory() = default;
  ABSL_DEPRECATED("")
  explicit GoogCcNetworkControllerFactory(RtcEventLog* event_log);
  explicit GoogCcNetworkControllerFactory(
      NetworkStatePredictorFactoryInterface* network_state_predictor_factory);

  explicit GoogCcNetworkControllerFactory(GoogCcFactoryConfig config);
  std::unique_ptr<NetworkControllerInterface> Create(
      NetworkControllerConfig config) override;
  TimeDelta GetProcessInterval() const override;

 protected:
  RtcEventLog* const event_log_ = nullptr;
  GoogCcFactoryConfig factory_config_;
};

// Deprecated, use GoogCcFactoryConfig to enable feedback only mode instead.
// Factory to create packet feedback only GoogCC, this can be used for
// connections providing packet receive time feedback but no other reports.
class ABSL_DEPRECATED("use GoogCcFactoryConfig instead")
    GoogCcFeedbackNetworkControllerFactory
    : public GoogCcNetworkControllerFactory {
 public:
  explicit GoogCcFeedbackNetworkControllerFactory(RtcEventLog* event_log);
};

}  // namespace webrtc

#endif  // API_TRANSPORT_GOOG_CC_FACTORY_H_
