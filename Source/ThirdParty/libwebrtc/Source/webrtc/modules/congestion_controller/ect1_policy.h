/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef MODULES_CONGESTION_CONTROLLER_ECT1_POLICY_H_
#define MODULES_CONGESTION_CONTROLLER_ECT1_POLICY_H_

#include <optional>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"

namespace webrtc {

// Decides if RTP packets should be sent with an ECT(1) marking, for example to
// support L4S as described in RFC 9331.
//
// ECT(1) is used if the negotiated feedback format reports the ECN marking of
// received packets and the congestion controller adapts to ECN. It is disabled
// until the network route changes if the network path bleaches the marking to
// Not-ECT, or if feedback stops arriving while packets are sent as ECT(1). The
// latter can happen when feedback is lost due to temporary network issues, so
// it is retried once per route.
//
// A path that drops ECT(1) from the start is detected quickly, since no
// feedback has been received on the route yet. Once ECT(1) is known to work,
// a much longer time without feedback is needed, so that a temporary outage
// is not mistaken for the same thing. A working path can start dropping the
// marking mid call, since traffic can be rerouted over a different middlebox
// without the ICE candidate pair changing.
class Ect1Policy {
 public:
  // Whether the remote peer reports the ECN marking of received packets back to
  // the sender, for example with the RTCP feedback message in RFC 8888.
  void SetFeedbackSupportsEcn(bool supports_ecn);
  // Whether the congestion controller reacts to reported CE markings.
  void SetCongestionControllerSupportsEcn(bool supports_ecn);
  void OnNetworkRouteChanged();
  // `has_bleached_ect1` is true if a feedback report showed at least one
  // packet that was sent as ECT(1) arriving as Not-ECT. `has_preserved_ect1`
  // is true if it showed at least one arriving still ECN capable. Both are
  // false if the report said nothing about the marking, which is the case
  // until the first packet sent as ECT(1) is reported received.
  void OnPacketsFeedback(bool has_bleached_ect1, bool has_preserved_ect1);

  // Must be called for every sent RTP packet. `sent_as_ect1` is whether the
  // packet actually carried the marking.
  void OnPacketSent(Timestamp send_time, bool sent_as_ect1);

  bool ShouldSendEct1() const;

 private:
  void LogIfDecisionChanged();

  // How long to wait for feedback before assuming ECT(1) packets are dropped.
  TimeDelta Ect1FeedbackTimeout() const;

  bool feedback_supports_ecn_ = false;
  bool congestion_controller_supports_ecn_ = false;
  bool route_bleaches_ect1_ = false;
  // When the route was judged to drop ECT(1) packets. Unset if it was not.
  std::optional<Timestamp> ect1_drop_detected_time_;
  // Whether the single retry allowed on this route has been used.
  bool ect1_drop_retried_ = false;
  bool ect1_preserved_on_route_ = false;
  // Does not count time when nothing was sent.
  TimeDelta time_sending_ect1_without_feedback_ = TimeDelta::Zero();
  // Send time of the last RTP packet, whatever its marking.
  std::optional<Timestamp> last_send_time_;
  bool logged_send_ect1_ = false;
};

}  // namespace webrtc

#endif  // MODULES_CONGESTION_CONTROLLER_ECT1_POLICY_H_
