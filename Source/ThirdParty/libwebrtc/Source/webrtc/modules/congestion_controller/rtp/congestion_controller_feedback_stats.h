/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_CONGESTION_CONTROLLER_RTP_CONGESTION_CONTROLLER_FEEDBACK_STATS_H_
#define MODULES_CONGESTION_CONTROLLER_RTP_CONGESTION_CONTROLLER_FEEDBACK_STATS_H_

#include <cstdint>

namespace webrtc {

// Helper struct to pass around stats computed from sent RFC8888 reports.
// https://github.com/w3c/webrtc-stats/blob/refs/heads/main/l4s-explainer.md#suggested-counters-and-where-to-attach-them
struct SentCongestionControllerFeedbackStats {
  // Total number of packets reported as lost in a produced feedback.
  // https://w3c.github.io/webrtc-stats/#dom-rtcreceivedrtpstreamstats-packetsreportedaslost
  int64_t num_packets_reported_lost = 0;

  // Total number of packets reported first as lost in a produced feedback, but
  // that were also reported as received in a later feedback.
  // https://w3c.github.io/webrtc-stats/#dom-rtcreceivedrtpstreamstats-packetsreportedaslostbutrecovered
  int64_t num_packets_reported_recovered = 0;
};

// Helper struct to pass around stats computed from received RFC8888 reports.
struct ReceivedCongestionControlFeedbackStats {
  // Total number of packets reported as received with the "ECT(1)" marking.
  // https://w3c.github.io/webrtc-stats/#dom-rtcreceivedrtpstreamstats-packetsreceivedwithect1
  int64_t num_packets_received_with_ect1 = 0;

  // Total number of packets reported as received with the "CE" marking.
  // https://w3c.github.io/webrtc-stats/#dom-rtcreceivedrtpstreamstats-packetsreceivedwithce
  int64_t num_packets_received_with_ce = 0;

  // Total number of packets reported as lost in received feedback.
  // https://w3c.github.io/webrtc-stats/#dom-rtcreceivedrtpstreamstats-packetsreportedaslost
  int64_t num_packets_reported_as_lost = 0;

  // Total number of packets reported first as lost in received feedback, but
  // that were also reported as received in a later feedback.
  // https://w3c.github.io/webrtc-stats/#dom-rtcreceivedrtpstreamstats-packetsreportedaslostbutrecovered
  int64_t num_packets_reported_as_lost_but_recovered = 0;

  // Total number of packets that were sent with ECT(1) markings, but were
  // reported as received with "not-ECT" marking.
  // https://w3c.github.io/webrtc-stats/#dom-rtcremoteinboundrtpstreamstats-packetswithbleachedect1marking
  int64_t num_packets_with_bleached_ect1_marking = 0;
};

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_RTP_CONGESTION_CONTROLLER_FEEDBACK_STATS_H_
