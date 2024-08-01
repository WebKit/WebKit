/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_REMOTE_BITRATE_ESTIMATOR_CONGESTION_CONTROL_FEEDBACK_GENERATOR_H_
#define MODULES_REMOTE_BITRATE_ESTIMATOR_CONGESTION_CONTROL_FEEDBACK_GENERATOR_H_

#include <map>
#include <memory>
#include <vector>

#include "api/environment/environment.h"
#include "api/sequence_checker.h"
#include "api/units/data_rate.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/remote_bitrate_estimator/rtp_transport_feedback_generator.h"
#include "modules/rtp_rtcp/source/rtcp_packet.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/experiments/field_trial_parser.h"
#include "rtc_base/numerics/sequence_number_unwrapper.h"

namespace webrtc {

// The class is responsible for generating RTCP feedback packets based on
// incoming media packets. Feedback format will comply with RFC 8888.
// https://datatracker.ietf.org/doc/rfc8888/

// Feedback should not use more than 5% of the configured send bandwidth
// estimate. Min and max duration between feedback is configurable using field
// trials, but per default, min is 25ms and max is 250ms.
// If possible, given the other constraints, feedback will be sent when a packet
// with marker bit is received in order to provide feedback as soon as possible
// after receiving a complete video frame. If no packet with marker bit is
// received, feedback can be delayed up to 25ms after the first packet since the
// last sent feedback. On good networks, this means that a sender may receive
// feedback for every sent frame.
class CongestionControlFeedbackGenerator
    : public RtpTransportFeedbackGenerator {
 public:
  CongestionControlFeedbackGenerator(
      const Environment& env,
      RtpTransportFeedbackGenerator::RtcpSender feedback_sender);
  ~CongestionControlFeedbackGenerator() = default;

  void OnReceivedPacket(const RtpPacketReceived& packet) override;

  void OnSendBandwidthEstimateChanged(DataRate estimate) override;

  TimeDelta Process(Timestamp now) override;

  void SetTransportOverhead(DataSize overhead_per_packet) override;

 private:
  struct PacketInfo {
    uint32_t ssrc;
    int64_t unwrapped_sequence_number = 0;
    Timestamp arrival_time;
    rtc::EcnMarking ecn = rtc::EcnMarking::kNotEct;
  };

  Timestamp NextFeedbackTime() const RTC_RUN_ON(sequence_checker_);

  void SendFeedback(Timestamp now) RTC_RUN_ON(sequence_checker_);

  void CalculateNextPossibleSendTime(DataSize feedback_size, Timestamp now)
      RTC_RUN_ON(sequence_checker_);

  const Environment env_;
  SequenceChecker sequence_checker_;
  const RtcpSender rtcp_sender_;

  FieldTrialParameter<TimeDelta> min_time_between_feedback_;
  FieldTrialParameter<TimeDelta> max_time_to_wait_for_packet_with_marker_;
  FieldTrialParameter<TimeDelta> max_time_between_feedback_;

  DataRate max_feedback_rate_ = DataRate::KilobitsPerSec(1000);
  DataSize packet_overhead_ = DataSize::Zero();
  DataSize send_rate_debt_ = DataSize::Zero();

  std::map</*ssrc=*/uint32_t, SeqNumUnwrapper<uint16_t>>
      sequence_number_unwrappers_;

  std::vector<PacketInfo> packets_;
  Timestamp last_feedback_sent_time_ = Timestamp::Zero();
  bool marker_bit_seen_ = false;
  Timestamp next_possible_feedback_send_time_ = Timestamp::Zero();
};

}  // namespace webrtc

#endif  // MODULES_REMOTE_BITRATE_ESTIMATOR_CONGESTION_CONTROL_FEEDBACK_GENERATOR_H_
