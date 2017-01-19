/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_CONTROLLER_MANAGER_H_
#define WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_CONTROLLER_MANAGER_H_

#include <map>
#include <memory>
#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/audio_coding/audio_network_adaptor/controller.h"

namespace webrtc {

class Clock;

class ControllerManager {
 public:
  virtual ~ControllerManager() = default;

  // Sort controllers based on their significance.
  virtual std::vector<Controller*> GetSortedControllers(
      const Controller::NetworkMetrics& metrics) = 0;

  virtual std::vector<Controller*> GetControllers() const = 0;
};

class ControllerManagerImpl final : public ControllerManager {
 public:
  struct Config {
    Config(int min_reordering_time_ms,
           float min_reordering_squared_distance,
           const Clock* clock);
    ~Config();
    // Least time since last reordering for a new reordering to be made.
    int min_reordering_time_ms;
    // Least squared distance from last scoring point for a new reordering to be
    // made.
    float min_reordering_squared_distance;
    const Clock* clock;
  };

  static std::unique_ptr<ControllerManager> Create(
      const std::string& config_string,
      size_t num_encoder_channels,
      rtc::ArrayView<const int> encoder_frame_lengths_ms,
      size_t intial_channels_to_encode,
      int initial_frame_length_ms,
      int initial_bitrate_bps,
      bool initial_fec_enabled,
      bool initial_dtx_enabled,
      const Clock* clock);

  explicit ControllerManagerImpl(const Config& config);

  // Dependency injection for testing.
  ControllerManagerImpl(
      const Config& config,
      std::vector<std::unique_ptr<Controller>>&& controllers,
      const std::map<const Controller*, std::pair<int, float>>&
          chracteristic_points);

  ~ControllerManagerImpl() override;

  // Sort controllers based on their significance.
  std::vector<Controller*> GetSortedControllers(
      const Controller::NetworkMetrics& metrics) override;

  std::vector<Controller*> GetControllers() const override;

 private:
  // Scoring point is a subset of NetworkMetrics that is used for comparing the
  // significance of controllers.
  struct ScoringPoint {
    ScoringPoint(int uplink_bandwidth_bps, float uplink_packet_loss_fraction);

    // Calculate the normalized [0,1] distance between two scoring points.
    float SquaredDistanceTo(const ScoringPoint& scoring_point) const;

    int uplink_bandwidth_bps;
    float uplink_packet_loss_fraction;
  };

  const Config config_;

  std::vector<std::unique_ptr<Controller>> controllers_;

  rtc::Optional<int64_t> last_reordering_time_ms_;
  ScoringPoint last_scoring_point_;

  std::vector<Controller*> default_sorted_controllers_;

  std::vector<Controller*> sorted_controllers_;

  // |scoring_points_| saves the characteristic scoring points of various
  // controllers.
  std::map<const Controller*, ScoringPoint> controller_scoring_points_;

  RTC_DISALLOW_COPY_AND_ASSIGN(ControllerManagerImpl);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_CONTROLLER_MANAGER_H_
