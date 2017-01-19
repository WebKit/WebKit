/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/audio_network_adaptor/frame_length_controller.h"

#include <utility>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"

namespace webrtc {

FrameLengthController::Config::Config(
    const std::vector<int>& encoder_frame_lengths_ms,
    int initial_frame_length_ms,
    float fl_increasing_packet_loss_fraction,
    float fl_decreasing_packet_loss_fraction,
    int fl_20ms_to_60ms_bandwidth_bps,
    int fl_60ms_to_20ms_bandwidth_bps)
    : encoder_frame_lengths_ms(encoder_frame_lengths_ms),
      initial_frame_length_ms(initial_frame_length_ms),
      fl_increasing_packet_loss_fraction(fl_increasing_packet_loss_fraction),
      fl_decreasing_packet_loss_fraction(fl_decreasing_packet_loss_fraction),
      fl_20ms_to_60ms_bandwidth_bps(fl_20ms_to_60ms_bandwidth_bps),
      fl_60ms_to_20ms_bandwidth_bps(fl_60ms_to_20ms_bandwidth_bps) {}

FrameLengthController::Config::Config(const Config& other) = default;

FrameLengthController::Config::~Config() = default;

FrameLengthController::FrameLengthController(const Config& config)
    : config_(config) {
  frame_length_ms_ = std::find(config_.encoder_frame_lengths_ms.begin(),
                               config_.encoder_frame_lengths_ms.end(),
                               config_.initial_frame_length_ms);
  // |encoder_frame_lengths_ms| must contain |initial_frame_length_ms|.
  RTC_DCHECK(frame_length_ms_ != config_.encoder_frame_lengths_ms.end());

  frame_length_change_criteria_.insert(std::make_pair(
      FrameLengthChange(20, 60), config_.fl_20ms_to_60ms_bandwidth_bps));
  frame_length_change_criteria_.insert(std::make_pair(
      FrameLengthChange(60, 20), config_.fl_60ms_to_20ms_bandwidth_bps));
}

FrameLengthController::~FrameLengthController() = default;

void FrameLengthController::MakeDecision(
    const NetworkMetrics& metrics,
    AudioNetworkAdaptor::EncoderRuntimeConfig* config) {
  // Decision on |frame_length_ms| should not have been made.
  RTC_DCHECK(!config->frame_length_ms);

  if (FrameLengthIncreasingDecision(metrics, *config)) {
    ++frame_length_ms_;
  } else if (FrameLengthDecreasingDecision(metrics, *config)) {
    --frame_length_ms_;
  }
  config->frame_length_ms = rtc::Optional<int>(*frame_length_ms_);
}

FrameLengthController::FrameLengthChange::FrameLengthChange(
    int from_frame_length_ms,
    int to_frame_length_ms)
    : from_frame_length_ms(from_frame_length_ms),
      to_frame_length_ms(to_frame_length_ms) {}

FrameLengthController::FrameLengthChange::~FrameLengthChange() = default;

bool FrameLengthController::FrameLengthChange::operator<(
    const FrameLengthChange& rhs) const {
  return from_frame_length_ms < rhs.from_frame_length_ms ||
         (from_frame_length_ms == rhs.from_frame_length_ms &&
          to_frame_length_ms < rhs.to_frame_length_ms);
}

bool FrameLengthController::FrameLengthIncreasingDecision(
    const NetworkMetrics& metrics,
    const AudioNetworkAdaptor::EncoderRuntimeConfig& config) const {
  // Increase frame length if
  // 1. longer frame length is available AND
  // 2. |uplink_bandwidth_bps| is known to be smaller than a threshold AND
  // 3. |uplink_packet_loss_fraction| is known to be smaller than a threshold
  //    AND
  // 4. FEC is not decided or is OFF.
  auto longer_frame_length_ms = std::next(frame_length_ms_);
  if (longer_frame_length_ms == config_.encoder_frame_lengths_ms.end())
    return false;

  auto increase_threshold = frame_length_change_criteria_.find(
      FrameLengthChange(*frame_length_ms_, *longer_frame_length_ms));

  if (increase_threshold == frame_length_change_criteria_.end())
    return false;

  return (metrics.uplink_bandwidth_bps &&
          *metrics.uplink_bandwidth_bps <= increase_threshold->second) &&
         (metrics.uplink_packet_loss_fraction &&
          *metrics.uplink_packet_loss_fraction <=
              config_.fl_increasing_packet_loss_fraction) &&
         !config.enable_fec.value_or(false);
}

bool FrameLengthController::FrameLengthDecreasingDecision(
    const NetworkMetrics& metrics,
    const AudioNetworkAdaptor::EncoderRuntimeConfig& config) const {
  // Decrease frame length if
  // 1. shorter frame length is available AND one or more of the followings:
  // 2. |uplink_bandwidth_bps| is known to be larger than a threshold,
  // 3. |uplink_packet_loss_fraction| is known to be larger than a threshold,
  // 4. FEC is decided ON.
  if (frame_length_ms_ == config_.encoder_frame_lengths_ms.begin())
    return false;

  auto shorter_frame_length_ms = std::prev(frame_length_ms_);
  auto decrease_threshold = frame_length_change_criteria_.find(
      FrameLengthChange(*frame_length_ms_, *shorter_frame_length_ms));

  if (decrease_threshold == frame_length_change_criteria_.end())
    return false;

  return (metrics.uplink_bandwidth_bps &&
          *metrics.uplink_bandwidth_bps >= decrease_threshold->second) ||
         (metrics.uplink_packet_loss_fraction &&
          *metrics.uplink_packet_loss_fraction >=
              config_.fl_decreasing_packet_loss_fraction) ||
         config.enable_fec.value_or(false);
}

}  // namespace webrtc
