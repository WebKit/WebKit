/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_FEC_CONTROLLER_RPLR_BASED_H_
#define MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_FEC_CONTROLLER_RPLR_BASED_H_

#include <memory>

#include "modules/audio_coding/audio_network_adaptor/controller.h"
#include "modules/audio_coding/audio_network_adaptor/util/threshold_curve.h"
#include "rtc_base/constructormagic.h"

namespace webrtc {

class FecControllerRplrBased final : public Controller {
 public:
  struct Config {
    // |fec_enabling_threshold| defines a curve, above which FEC should be
    // enabled. |fec_disabling_threshold| defines a curve, under which FEC
    // should be disabled. See below
    //
    // recoverable
    // packet-loss ^   |  |
    //             |   |  |   FEC
    //             |    \  \   ON
    //             | FEC \  \_______ fec_enabling_threshold
    //             | OFF  \_________ fec_disabling_threshold
    //             |-----------------> bandwidth
    Config(bool initial_fec_enabled,
           const ThresholdCurve& fec_enabling_threshold,
           const ThresholdCurve& fec_disabling_threshold);
    bool initial_fec_enabled;
    ThresholdCurve fec_enabling_threshold;
    ThresholdCurve fec_disabling_threshold;
  };

  explicit FecControllerRplrBased(const Config& config);

  ~FecControllerRplrBased() override;

  void UpdateNetworkMetrics(const NetworkMetrics& network_metrics) override;

  void MakeDecision(AudioEncoderRuntimeConfig* config) override;

 private:
  bool FecEnablingDecision() const;
  bool FecDisablingDecision() const;

  const Config config_;
  bool fec_enabled_;
  absl::optional<int> uplink_bandwidth_bps_;
  absl::optional<float> uplink_recoverable_packet_loss_;

  RTC_DISALLOW_COPY_AND_ASSIGN(FecControllerRplrBased);
};

}  // namespace webrtc

#endif  // MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_FEC_CONTROLLER_RPLR_BASED_H_
