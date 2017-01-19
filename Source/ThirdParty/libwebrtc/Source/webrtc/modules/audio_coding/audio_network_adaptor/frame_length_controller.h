/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_FRAME_LENGTH_CONTROLLER_H_
#define WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_FRAME_LENGTH_CONTROLLER_H_

#include <map>
#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/audio_coding/audio_network_adaptor/controller.h"

namespace webrtc {

// Determines target frame length based on the network metrics and the decision
// of FEC controller.
class FrameLengthController final : public Controller {
 public:
  struct Config {
    Config(const std::vector<int>& encoder_frame_lengths_ms,
           int initial_frame_length_ms,
           float fl_increasing_packet_loss_fraction,
           float fl_decreasing_packet_loss_fraction,
           int fl_20ms_to_60ms_bandwidth_bps,
           int fl_60ms_to_20ms_bandwidth_bps);
    Config(const Config& other);
    ~Config();
    std::vector<int> encoder_frame_lengths_ms;
    int initial_frame_length_ms;
    // Uplink packet loss fraction below which frame length can increase.
    float fl_increasing_packet_loss_fraction;
    // Uplink packet loss fraction below which frame length should decrease.
    float fl_decreasing_packet_loss_fraction;
    // Uplink bandwidth below which frame length can switch from 20ms to 60ms.
    int fl_20ms_to_60ms_bandwidth_bps;
    // Uplink bandwidth above which frame length should switch from 60ms to
    // 20ms.
    int fl_60ms_to_20ms_bandwidth_bps;
  };

  explicit FrameLengthController(const Config& config);

  ~FrameLengthController() override;

  void MakeDecision(const NetworkMetrics& metrics,
                    AudioNetworkAdaptor::EncoderRuntimeConfig* config) override;

 private:
  friend class FrameLengthControllerForTest;

  struct FrameLengthChange {
    FrameLengthChange(int from_frame_length_ms, int to_frame_length_ms);
    ~FrameLengthChange();
    bool operator<(const FrameLengthChange& rhs) const;
    int from_frame_length_ms;
    int to_frame_length_ms;
  };

  bool FrameLengthIncreasingDecision(
      const NetworkMetrics& metrics,
      const AudioNetworkAdaptor::EncoderRuntimeConfig& config) const;

  bool FrameLengthDecreasingDecision(
      const NetworkMetrics& metrics,
      const AudioNetworkAdaptor::EncoderRuntimeConfig& config) const;

  const Config config_;

  std::vector<int>::const_iterator frame_length_ms_;

  std::map<FrameLengthChange, int> frame_length_change_criteria_;

  RTC_DISALLOW_COPY_AND_ASSIGN(FrameLengthController);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_AUDIO_NETWORK_ADAPTOR_FRAME_LENGTH_CONTROLLER_H_
