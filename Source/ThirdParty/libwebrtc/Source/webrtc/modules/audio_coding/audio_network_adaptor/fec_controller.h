/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_FEC_CONTROLLER_H_
#define WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_FEC_CONTROLLER_H_

#include <memory>

#include "webrtc/base/constructormagic.h"
#include "webrtc/common_audio/smoothing_filter.h"
#include "webrtc/modules/audio_coding/audio_network_adaptor/controller.h"

namespace webrtc {

class FecController final : public Controller {
 public:
  struct Config {
    struct Threshold {
      // Threshold defines a curve in the bandwidth/packet-loss domain. The
      // curve is characterized by the two conjunction points: A and B.
      //
      // packet ^  |
      //  loss  | A|
      //        |   \        A: (low_bandwidth_bps, low_bandwidth_packet_loss)
      //        |    \       B: (high_bandwidth_bps, high_bandwidth_packet_loss)
      //        |    B\________
      //        |---------------> bandwidth
      Threshold(int low_bandwidth_bps,
                float low_bandwidth_packet_loss,
                int high_bandwidth_bps,
                float high_bandwidth_packet_loss);
      int low_bandwidth_bps;
      float low_bandwidth_packet_loss;
      int high_bandwidth_bps;
      float high_bandwidth_packet_loss;
    };

    // |fec_enabling_threshold| defines a curve, above which FEC should be
    // enabled. |fec_disabling_threshold| defines a curve, under which FEC
    // should be disabled. See below
    //
    // packet-loss ^   |  |
    //             |   |  |   FEC
    //             |    \  \   ON
    //             | FEC \  \_______ fec_enabling_threshold
    //             | OFF  \_________ fec_disabling_threshold
    //             |-----------------> bandwidth
    Config(bool initial_fec_enabled,
           const Threshold& fec_enabling_threshold,
           const Threshold& fec_disabling_threshold,
           int time_constant_ms,
           const Clock* clock);
    bool initial_fec_enabled;
    Threshold fec_enabling_threshold;
    Threshold fec_disabling_threshold;
    int time_constant_ms;
    const Clock* clock;
  };

  // Dependency injection for testing.
  FecController(const Config& config,
                std::unique_ptr<SmoothingFilter> smoothing_filter);

  explicit FecController(const Config& config);

  ~FecController() override;

  void UpdateNetworkMetrics(const NetworkMetrics& network_metrics) override;

  void MakeDecision(AudioNetworkAdaptor::EncoderRuntimeConfig* config) override;

 private:
  // Characterize Threshold with packet_loss = slope * bandwidth + offset.
  struct ThresholdInfo {
    explicit ThresholdInfo(const Config::Threshold& threshold);
    float slope;
    float offset;
  };

  float GetPacketLossThreshold(int bandwidth_bps,
                               const Config::Threshold& threshold,
                               const ThresholdInfo& threshold_info) const;

  bool FecEnablingDecision(const rtc::Optional<float>& packet_loss) const;
  bool FecDisablingDecision(const rtc::Optional<float>& packet_loss) const;

  const Config config_;
  bool fec_enabled_;
  rtc::Optional<int> uplink_bandwidth_bps_;
  const std::unique_ptr<SmoothingFilter> packet_loss_smoother_;

  const ThresholdInfo fec_enabling_threshold_info_;
  const ThresholdInfo fec_disabling_threshold_info_;

  RTC_DISALLOW_COPY_AND_ASSIGN(FecController);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_FEC_CONTROLLER_H_
