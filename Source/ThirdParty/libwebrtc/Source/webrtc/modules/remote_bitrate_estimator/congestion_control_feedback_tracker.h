/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_REMOTE_BITRATE_ESTIMATOR_CONGESTION_CONTROL_FEEDBACK_TRACKER_H_
#define MODULES_REMOTE_BITRATE_ESTIMATOR_CONGESTION_CONTROL_FEEDBACK_TRACKER_H_

#include <cstdint>
#include <vector>

#include "api/transport/ecn_marking.h"
#include "api/units/timestamp.h"
#include "modules/congestion_controller/rtp/congestion_controller_feedback_stats.h"
#include "modules/rtp_rtcp/source/rtcp_packet/congestion_control_feedback.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "rtc_base/numerics/sequence_number_unwrapper.h"

namespace webrtc {

// CongestionControlFeedbackTracker is reponsible for creating and keeping track
// of feedback sent for a specific SSRC when feedback is sent according to
// https://datatracker.ietf.org/doc/rfc8888/
class CongestionControlFeedbackTracker {
 public:
  explicit CongestionControlFeedbackTracker(uint32_t ssrc) : ssrc_(ssrc) {}

  void ReceivedPacket(const RtpPacketReceived& packet);

  // Adds received packets to `packet_feedback`
  // RTP sequence numbers are continous from the last created feedback unless
  // reordering has occured between feedback packets. If so, the sequence
  // number range may overlap with previousely sent feedback.
  void AddPacketsToFeedback(
      Timestamp feedback_time,
      std::vector<rtcp::CongestionControlFeedback::PacketInfo>&
          packet_feedback);

  SentCongestionControllerFeedbackStats GetStats() const { return stats_; }

 private:
  struct PacketInfo {
    bool received() const { return arrival_time != Timestamp::MinusInfinity(); }

    Timestamp arrival_time = Timestamp::MinusInfinity();
    EcnMarking ecn = EcnMarking::kNotEct;

    // Indicates if packet was reported as lost last time it was reported in a
    // `AddPacketsToFeedback`.
    bool last_reported_as_lost = false;
  };

  // Returns a `PacketInfo` entry for `sequence_number`,
  // Returns nullptr if an entry can't be allocated due to `sequence_number`
  // being too much out of order of already stored packet infos.
  PacketInfo* FindOrCreatePacketInfo(int64_t sequence_number);

  const uint32_t ssrc_;
  SeqNumUnwrapper<uint16_t> unwrapper_;

  // Contains info relevant for producing feedback for a received or missed RTP
  // packet. Entry with index `i` represent information about packet with
  // RTP sequence number `first_sequence_number_in_packets_ + i`
  std::vector<PacketInfo> packets_;

  // Unwrapped RTP sequence number of the first element in `packets_`.
  // Mustn't be used when `packets_.empty()`
  int64_t first_sequence_number_in_packets_ = -1;

  // Unwrapped RTP sequence number of a packet to start next feedback with.
  // Mustn't be used when `packets_.empty()`
  int64_t next_sequence_number_in_feedback_ = -1;

  // Number of packets discarded by `ReceivedPacket` function since last call
  // to `AddPacketsToFeedback`.
  int num_ignored_packets_since_last_feedback_ = 0;

  SentCongestionControllerFeedbackStats stats_;
};

}  // namespace webrtc

#endif  // MODULES_REMOTE_BITRATE_ESTIMATOR_CONGESTION_CONTROL_FEEDBACK_TRACKER_H_
