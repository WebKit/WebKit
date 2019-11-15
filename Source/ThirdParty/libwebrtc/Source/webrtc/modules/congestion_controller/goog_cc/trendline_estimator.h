/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_CONGESTION_CONTROLLER_GOOG_CC_TRENDLINE_ESTIMATOR_H_
#define MODULES_CONGESTION_CONTROLLER_GOOG_CC_TRENDLINE_ESTIMATOR_H_

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <memory>
#include <utility>

#include "api/network_state_predictor.h"
#include "api/transport/webrtc_key_value_config.h"
#include "modules/congestion_controller/goog_cc/delay_increase_detector_interface.h"
#include "modules/remote_bitrate_estimator/include/bwe_defines.h"
#include "rtc_base/constructor_magic.h"
#include "rtc_base/experiments/struct_parameters_parser.h"

namespace webrtc {

struct BweIgnoreSmallPacketsSettings {
  static constexpr char kKey[] = "WebRTC-BweIgnoreSmallPackets";

  BweIgnoreSmallPacketsSettings() = default;
  explicit BweIgnoreSmallPacketsSettings(
      const WebRtcKeyValueConfig* key_value_config);

  double smoothing_factor = 0.1;
  double min_fraction_large_packets = 1.0;
  unsigned large_packet_size = 0;
  unsigned ignored_size = 0;

  std::unique_ptr<StructParametersParser> Parser();
};

class TrendlineEstimator : public DelayIncreaseDetectorInterface {
 public:
  TrendlineEstimator(const WebRtcKeyValueConfig* key_value_config,
                     NetworkStatePredictor* network_state_predictor);

  ~TrendlineEstimator() override;

  // Update the estimator with a new sample. The deltas should represent deltas
  // between timestamp groups as defined by the InterArrival class.
  void Update(double recv_delta_ms,
              double send_delta_ms,
              int64_t send_time_ms,
              int64_t arrival_time_ms,
              size_t packet_size,
              bool calculated_deltas) override;

  void UpdateTrendline(double recv_delta_ms,
                       double send_delta_ms,
                       int64_t send_time_ms,
                       int64_t arrival_time_ms,
                       size_t packet_size);

  BandwidthUsage State() const override;

 private:
  friend class GoogCcStatePrinter;

  void Detect(double trend, double ts_delta, int64_t now_ms);

  void UpdateThreshold(double modified_offset, int64_t now_ms);

  // Filtering out small packets. (Intention is to base the detection only
  // on video packets even if we have TWCC sequence number for audio.)
  BweIgnoreSmallPacketsSettings ignore_small_packets_;
  double fraction_large_packets_;

  // Parameters.
  const size_t window_size_;
  const double smoothing_coef_;
  const double threshold_gain_;
  // Used by the existing threshold.
  int num_of_deltas_;
  // Keep the arrival times small by using the change from the first packet.
  int64_t first_arrival_time_ms_;
  // Exponential backoff filtering.
  double accumulated_delay_;
  double smoothed_delay_;
  // Linear least squares regression.
  std::deque<std::pair<double, double>> delay_hist_;

  const double k_up_;
  const double k_down_;
  double overusing_time_threshold_;
  double threshold_;
  double prev_modified_trend_;
  int64_t last_update_ms_;
  double prev_trend_;
  double time_over_using_;
  int overuse_counter_;
  BandwidthUsage hypothesis_;
  BandwidthUsage hypothesis_predicted_;
  NetworkStatePredictor* network_state_predictor_;

  RTC_DISALLOW_COPY_AND_ASSIGN(TrendlineEstimator);
};
}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_GOOG_CC_TRENDLINE_ESTIMATOR_H_
