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

#ifndef WEBRTC_MODULES_BITRATE_CONTROLLER_SEND_SIDE_BANDWIDTH_ESTIMATION_H_
#define WEBRTC_MODULES_BITRATE_CONTROLLER_SEND_SIDE_BANDWIDTH_ESTIMATION_H_

#include <deque>
#include <utility>
#include <vector>

#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/system_wrappers/include/critical_section_wrapper.h"

namespace webrtc {

class RtcEventLog;

class SendSideBandwidthEstimation {
 public:
  SendSideBandwidthEstimation() = delete;
  explicit SendSideBandwidthEstimation(RtcEventLog* event_log);
  virtual ~SendSideBandwidthEstimation();

  void CurrentEstimate(int* bitrate, uint8_t* loss, int64_t* rtt) const;

  // Call periodically to update estimate.
  void UpdateEstimate(int64_t now_ms);

  // Call when we receive a RTCP message with TMMBR or REMB.
  void UpdateReceiverEstimate(int64_t now_ms, uint32_t bandwidth);

  // Call when a new delay-based estimate is available.
  void UpdateDelayBasedEstimate(int64_t now_ms, uint32_t bitrate_bps);

  // Call when we receive a RTCP message with a ReceiveBlock.
  void UpdateReceiverBlock(uint8_t fraction_loss,
                           int64_t rtt,
                           int number_of_packets,
                           int64_t now_ms);

  void SetBitrates(int send_bitrate,
                   int min_bitrate,
                   int max_bitrate);
  void SetSendBitrate(int bitrate);
  void SetMinMaxBitrate(int min_bitrate, int max_bitrate);
  int GetMinBitrate() const;

 private:
  enum UmaState { kNoUpdate, kFirstDone, kDone };

  bool IsInStartPhase(int64_t now_ms) const;

  void UpdateUmaStats(int64_t now_ms, int64_t rtt, int lost_packets);

  // Returns the input bitrate capped to the thresholds defined by the max,
  // min and incoming bandwidth.
  uint32_t CapBitrateToThresholds(int64_t now_ms, uint32_t bitrate);

  // Updates history of min bitrates.
  // After this method returns min_bitrate_history_.front().second contains the
  // min bitrate used during last kBweIncreaseIntervalMs.
  void UpdateMinHistory(int64_t now_ms);

  std::deque<std::pair<int64_t, uint32_t> > min_bitrate_history_;

  // incoming filters
  int lost_packets_since_last_loss_update_Q8_;
  int expected_packets_since_last_loss_update_;

  uint32_t bitrate_;
  uint32_t min_bitrate_configured_;
  uint32_t max_bitrate_configured_;
  int64_t last_low_bitrate_log_ms_;

  bool has_decreased_since_last_fraction_loss_;
  int64_t last_feedback_ms_;
  int64_t last_packet_report_ms_;
  int64_t last_timeout_ms_;
  uint8_t last_fraction_loss_;
  uint8_t last_logged_fraction_loss_;
  int64_t last_round_trip_time_ms_;

  uint32_t bwe_incoming_;
  uint32_t delay_based_bitrate_bps_;
  int64_t time_last_decrease_ms_;
  int64_t first_report_time_ms_;
  int initially_lost_packets_;
  int bitrate_at_2_seconds_kbps_;
  UmaState uma_update_state_;
  std::vector<bool> rampup_uma_stats_updated_;
  RtcEventLog* event_log_;
  int64_t last_rtc_event_log_ms_;
  bool in_timeout_experiment_;
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_BITRATE_CONTROLLER_SEND_SIDE_BANDWIDTH_ESTIMATION_H_
