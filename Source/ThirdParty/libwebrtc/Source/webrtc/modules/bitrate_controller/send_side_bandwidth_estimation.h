/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 *  FEC and NACK added bitrate is handled outside class
 */

#ifndef MODULES_BITRATE_CONTROLLER_SEND_SIDE_BANDWIDTH_ESTIMATION_H_
#define MODULES_BITRATE_CONTROLLER_SEND_SIDE_BANDWIDTH_ESTIMATION_H_

#include <stdint.h>
#include <deque>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/transport/network_types.h"
#include "api/units/data_rate.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/bitrate_controller/loss_based_bandwidth_estimation.h"
#include "modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "rtc_base/experiments/field_trial_parser.h"

namespace webrtc {

class RtcEventLog;

class LinkCapacityTracker {
 public:
  LinkCapacityTracker();
  ~LinkCapacityTracker();
  void OnOveruse(DataRate acknowledged_rate, Timestamp at_time);
  void OnStartingRate(DataRate start_rate);
  void OnRateUpdate(DataRate acknowledged, Timestamp at_time);
  void OnRttBackoff(DataRate backoff_rate, Timestamp at_time);
  DataRate estimate() const;

 private:
  FieldTrialParameter<TimeDelta> tracking_rate;
  double capacity_estimate_bps_ = 0;
  Timestamp last_link_capacity_update_ = Timestamp::MinusInfinity();
};

class RttBasedBackoff {
 public:
  RttBasedBackoff();
  ~RttBasedBackoff();
  void OnRouteChange();
  void UpdatePropagationRtt(Timestamp at_time, TimeDelta propagation_rtt);
  TimeDelta RttLowerBound(Timestamp at_time) const;

  FieldTrialParameter<TimeDelta> rtt_limit_;
  FieldTrialParameter<double> drop_fraction_;
  FieldTrialParameter<TimeDelta> drop_interval_;
  FieldTrialFlag persist_on_route_change_;

 public:
  Timestamp last_propagation_rtt_update_;
  TimeDelta last_propagation_rtt_;
};

class SendSideBandwidthEstimation {
 public:
  SendSideBandwidthEstimation() = delete;
  explicit SendSideBandwidthEstimation(RtcEventLog* event_log);
  ~SendSideBandwidthEstimation();

  void OnRouteChange();
  void CurrentEstimate(int* bitrate, uint8_t* loss, int64_t* rtt) const;
  DataRate GetEstimatedLinkCapacity() const;
  // Call periodically to update estimate.
  void UpdateEstimate(Timestamp at_time);
  void OnSentPacket(SentPacket sent_packet);
  void UpdatePropagationRtt(Timestamp at_time, TimeDelta propagation_rtt);

  // Call when we receive a RTCP message with TMMBR or REMB.
  void UpdateReceiverEstimate(Timestamp at_time, DataRate bandwidth);

  // Call when a new delay-based estimate is available.
  void UpdateDelayBasedEstimate(Timestamp at_time, DataRate bitrate);

  // Call when we receive a RTCP message with a ReceiveBlock.
  void UpdateReceiverBlock(uint8_t fraction_loss,
                           TimeDelta rtt_ms,
                           int number_of_packets,
                           Timestamp at_time);

  // Call when we receive a RTCP message with a ReceiveBlock.
  void UpdatePacketsLost(int packets_lost,
                         int number_of_packets,
                         Timestamp at_time);

  // Call when we receive a RTCP message with a ReceiveBlock.
  void UpdateRtt(TimeDelta rtt, Timestamp at_time);

  void SetBitrates(absl::optional<DataRate> send_bitrate,
                   DataRate min_bitrate,
                   DataRate max_bitrate,
                   Timestamp at_time);
  void SetSendBitrate(DataRate bitrate, Timestamp at_time);
  void SetMinMaxBitrate(DataRate min_bitrate, DataRate max_bitrate);
  int GetMinBitrate() const;
  void SetAcknowledgedRate(absl::optional<DataRate> acknowledged_rate,
                           Timestamp at_time);
  void IncomingPacketFeedbackVector(const TransportPacketsFeedback& report);

 private:
  enum UmaState { kNoUpdate, kFirstDone, kDone };

  bool IsInStartPhase(Timestamp at_time) const;

  void UpdateUmaStatsPacketsLost(Timestamp at_time, int packets_lost);

  // Updates history of min bitrates.
  // After this method returns min_bitrate_history_.front().second contains the
  // min bitrate used during last kBweIncreaseIntervalMs.
  void UpdateMinHistory(Timestamp at_time);

  DataRate MaybeRampupOrBackoff(DataRate new_bitrate, Timestamp at_time);

  // Cap |bitrate| to [min_bitrate_configured_, max_bitrate_configured_] and
  // set |current_bitrate_| to the capped value and updates the event log.
  void CapBitrateToThresholds(Timestamp at_time, DataRate bitrate);

  RttBasedBackoff rtt_backoff_;
  LinkCapacityTracker link_capacity_;

  std::deque<std::pair<Timestamp, DataRate> > min_bitrate_history_;

  // incoming filters
  int lost_packets_since_last_loss_update_;
  int expected_packets_since_last_loss_update_;

  absl::optional<DataRate> acknowledged_rate_;
  DataRate current_bitrate_;
  DataRate min_bitrate_configured_;
  DataRate max_bitrate_configured_;
  Timestamp last_low_bitrate_log_;

  bool has_decreased_since_last_fraction_loss_;
  Timestamp last_loss_feedback_;
  Timestamp last_loss_packet_report_;
  Timestamp last_timeout_;
  uint8_t last_fraction_loss_;
  uint8_t last_logged_fraction_loss_;
  TimeDelta last_round_trip_time_;

  DataRate bwe_incoming_;
  DataRate delay_based_bitrate_;
  Timestamp time_last_decrease_;
  Timestamp first_report_time_;
  int initially_lost_packets_;
  DataRate bitrate_at_2_seconds_;
  UmaState uma_update_state_;
  UmaState uma_rtt_state_;
  std::vector<bool> rampup_uma_stats_updated_;
  RtcEventLog* event_log_;
  Timestamp last_rtc_event_log_;
  bool in_timeout_experiment_;
  float low_loss_threshold_;
  float high_loss_threshold_;
  DataRate bitrate_threshold_;
  LossBasedBandwidthEstimation loss_based_bandwidth_estimation_;
};
}  // namespace webrtc
#endif  // MODULES_BITRATE_CONTROLLER_SEND_SIDE_BANDWIDTH_ESTIMATION_H_
