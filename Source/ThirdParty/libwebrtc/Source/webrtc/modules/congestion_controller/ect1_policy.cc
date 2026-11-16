/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "modules/congestion_controller/ect1_policy.h"

#include <algorithm>
#include <optional>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "rtc_base/logging.h"

namespace webrtc {
namespace {

// How long ECT(1) packets must be sent without any feedback arriving before
// assuming the path drops them. The timeout is short until an ECT(1) packet
// has been acknowledged, so that a path that drops them from the start is
// detected quickly. Once the path is known to deliver them, the timeout
// matches the ICE unwritable timeout, so that an outage affecting all packets
// has made ICE look for another route before the marking is blamed for it.
constexpr TimeDelta kEct1FeedbackTimeoutBeforeAck = TimeDelta::Millis(500);
constexpr TimeDelta kEct1FeedbackTimeoutAfterAck = TimeDelta::Seconds(5);

// The most a single gap between two sent packets can contribute to the time
// spent sending, so that idle time does not count toward the timeout. A sender
// that sends rarely, such as a screencast of static content without audio,
// therefore needs longer to reach the timeout. That delays detection but can
// not cause a false one.
constexpr TimeDelta kMaxCountedSendGap = TimeDelta::Millis(200);

// Resuming after a pause can then never by itself trigger detection. A sender
// that gets no feedback still reaches the timeout, because the pacer keeps
// sending a packet every kCongestedPacketInterval.
static_assert(kMaxCountedSendGap < kEct1FeedbackTimeoutBeforeAck);

// Sending ECT(1) is retried once per route this long after the path was judged
// to drop the marking. A round trip time longer than the timeout used before
// the first acknowledgement makes that judgement wrong, and the round trip
// time can also improve during a call.
constexpr TimeDelta kEct1RetryDelay = TimeDelta::Seconds(10);

}  // namespace

void Ect1Policy::SetFeedbackSupportsEcn(bool supports_ecn) {
  feedback_supports_ecn_ = supports_ecn;
  LogIfDecisionChanged();
}

void Ect1Policy::SetCongestionControllerSupportsEcn(bool supports_ecn) {
  congestion_controller_supports_ecn_ = supports_ecn;
  LogIfDecisionChanged();
}

void Ect1Policy::OnNetworkRouteChanged() {
  // The new path may handle the ECT(1) marking even if the old one did not.
  route_bleaches_ect1_ = false;
  ect1_drop_detected_time_ = std::nullopt;
  ect1_drop_retried_ = false;
  ect1_preserved_on_route_ = false;
  time_sending_ect1_without_feedback_ = TimeDelta::Zero();
  LogIfDecisionChanged();
}

void Ect1Policy::OnPacketsFeedback(bool has_bleached_ect1,
                                   bool has_preserved_ect1) {
  // Feedback of any kind proves that packets reach the receiver, so the path
  // is not dropping them and the time spent sending starts over.
  time_sending_ect1_without_feedback_ = TimeDelta::Zero();
  if (has_preserved_ect1) {
    ect1_preserved_on_route_ = true;
  }
  if (has_bleached_ect1 && !route_bleaches_ect1_) {
    route_bleaches_ect1_ = true;
    RTC_LOG(LS_INFO) << "Transport does not preserve the ECT(1) marking. Stop "
                        "sending ECT(1) on this route.";
  }
  LogIfDecisionChanged();
}

void Ect1Policy::OnPacketSent(Timestamp send_time, bool sent_as_ect1) {
  if (ect1_drop_detected_time_.has_value() && !ect1_drop_retried_ &&
      send_time - *ect1_drop_detected_time_ > kEct1RetryDelay) {
    ect1_drop_retried_ = true;
    // Clearing the drop detection time makes ShouldSendEct1() true again.
    ect1_drop_detected_time_ = std::nullopt;
    time_sending_ect1_without_feedback_ = TimeDelta::Zero();
    RTC_LOG(LS_INFO) << "Retrying ECT(1) on this route.";
  }

  if (sent_as_ect1) {
    if (last_send_time_.has_value()) {
      time_sending_ect1_without_feedback_ +=
          std::min(send_time - *last_send_time_, kMaxCountedSendGap);
    }
    TimeDelta timeout = Ect1FeedbackTimeout();
    if (!ect1_drop_detected_time_.has_value() &&
        time_sending_ect1_without_feedback_ > timeout) {
      ect1_drop_detected_time_ = send_time;
      RTC_LOG(LS_INFO) << "No feedback while sending ECT(1) packets during "
                       << ToString(timeout)
                       << ". Assuming the transport drops them. Stop sending "
                          "ECT(1) on this route.";
    }
  }
  last_send_time_ = send_time;

  LogIfDecisionChanged();
}

TimeDelta Ect1Policy::Ect1FeedbackTimeout() const {
  return ect1_preserved_on_route_ ? kEct1FeedbackTimeoutAfterAck
                                  : kEct1FeedbackTimeoutBeforeAck;
}

bool Ect1Policy::ShouldSendEct1() const {
  return feedback_supports_ecn_ && congestion_controller_supports_ecn_ &&
         !route_bleaches_ect1_ && !ect1_drop_detected_time_.has_value();
}

void Ect1Policy::LogIfDecisionChanged() {
  bool send_ect1 = ShouldSendEct1();
  if (send_ect1 == logged_send_ect1_) {
    return;
  }
  logged_send_ect1_ = send_ect1;
  RTC_LOG(LS_INFO) << "Sending packets as "
                   << (send_ect1 ? "ECT(1)." : "Not-ECT.");
}

}  // namespace webrtc
