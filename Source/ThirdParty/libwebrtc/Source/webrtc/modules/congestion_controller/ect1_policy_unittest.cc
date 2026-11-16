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

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

constexpr TimeDelta kTimeoutBeforeAck = TimeDelta::Millis(500);
constexpr TimeDelta kTimeoutAfterAck = TimeDelta::Seconds(5);
constexpr TimeDelta kRetryDelay = TimeDelta::Seconds(10);
// Shorter than the largest gap that counts as time spent sending, so that
// every interval counts in full.
constexpr TimeDelta kSendInterval = TimeDelta::Millis(100);
constexpr Timestamp kStartTime = Timestamp::Seconds(1000);
// When sending ECT(1) from kStartTime without feedback stops the marking.
constexpr Timestamp kDropDetectedTime =
    kStartTime + kTimeoutBeforeAck + kSendInterval;

Ect1Policy CreateEnabledPolicy() {
  Ect1Policy policy;
  policy.SetFeedbackSupportsEcn(true);
  policy.SetCongestionControllerSupportsEcn(true);
  return policy;
}

// Sends an ECT(1) packet every kSendInterval in [start, end].
void SendEct1Packets(Timestamp start, Timestamp end, Ect1Policy& policy) {
  for (Timestamp now = start; now <= end; now += kSendInterval) {
    policy.OnPacketSent(now, /*sent_as_ect1=*/true);
  }
}

Ect1Policy CreatePolicyThatDetectedDroppedEct1() {
  Ect1Policy policy = CreateEnabledPolicy();
  SendEct1Packets(kStartTime, kDropDetectedTime, policy);
  return policy;
}

TEST(Ect1PolicyTest, DoesNotSendEct1ByDefault) {
  Ect1Policy policy;
  EXPECT_FALSE(policy.ShouldSendEct1());
}

TEST(Ect1PolicyTest, SendsEct1IfFeedbackAndCongestionControllerSupportEcn) {
  EXPECT_TRUE(CreateEnabledPolicy().ShouldSendEct1());
}

TEST(Ect1PolicyTest, DoesNotSendEct1IfFeedbackDoesNotSupportEcn) {
  Ect1Policy policy;
  policy.SetFeedbackSupportsEcn(false);
  policy.SetCongestionControllerSupportsEcn(true);
  EXPECT_FALSE(policy.ShouldSendEct1());
}

TEST(Ect1PolicyTest, DoesNotSendEct1IfCongestionControllerDoesNotSupportEcn) {
  Ect1Policy policy;
  policy.SetFeedbackSupportsEcn(true);
  policy.SetCongestionControllerSupportsEcn(false);
  EXPECT_FALSE(policy.ShouldSendEct1());
}

TEST(Ect1PolicyTest, StopsSendingEct1IfFeedbackReportsBleaching) {
  Ect1Policy policy = CreateEnabledPolicy();
  policy.OnPacketsFeedback(/*has_bleached_ect1=*/true,
                           /*has_preserved_ect1=*/false);
  EXPECT_FALSE(policy.ShouldSendEct1());
}

TEST(Ect1PolicyTest, KeepsSendingEct1IfFeedbackReportsNoBleaching) {
  Ect1Policy policy = CreateEnabledPolicy();
  policy.OnPacketsFeedback(/*has_bleached_ect1=*/false,
                           /*has_preserved_ect1=*/false);
  EXPECT_TRUE(policy.ShouldSendEct1());
}

TEST(Ect1PolicyTest, RouteChangeReEnablesEct1AfterBleaching) {
  Ect1Policy policy = CreateEnabledPolicy();
  policy.OnPacketsFeedback(/*has_bleached_ect1=*/true,
                           /*has_preserved_ect1=*/true);
  ASSERT_FALSE(policy.ShouldSendEct1());

  policy.OnNetworkRouteChanged();
  EXPECT_TRUE(policy.ShouldSendEct1());
}

TEST(Ect1PolicyTest, RenegotiationDoesNotReEnableEct1AfterBleaching) {
  Ect1Policy policy = CreateEnabledPolicy();
  policy.OnPacketsFeedback(/*has_bleached_ect1=*/true,
                           /*has_preserved_ect1=*/true);
  ASSERT_FALSE(policy.ShouldSendEct1());

  policy.SetFeedbackSupportsEcn(true);
  EXPECT_FALSE(policy.ShouldSendEct1());
}

TEST(Ect1PolicyTest, StopsSendingEct1IfNoFeedbackWhileSendingEct1) {
  Ect1Policy policy = CreateEnabledPolicy();
  Timestamp last_send = kStartTime + kTimeoutBeforeAck;
  SendEct1Packets(kStartTime, last_send, policy);
  EXPECT_TRUE(policy.ShouldSendEct1());

  // One more packet takes the time spent sending past the timeout.
  policy.OnPacketSent(last_send + kSendInterval, /*sent_as_ect1=*/true);
  EXPECT_FALSE(policy.ShouldSendEct1());
}

TEST(Ect1PolicyTest, PacketsSentAsNotEctDoNotStopSendingEct1) {
  Ect1Policy policy = CreateEnabledPolicy();
  for (Timestamp now = kStartTime; now <= kStartTime + TimeDelta::Seconds(30);
       now += kSendInterval) {
    policy.OnPacketSent(now, /*sent_as_ect1=*/false);
  }
  EXPECT_TRUE(policy.ShouldSendEct1());
}

TEST(Ect1PolicyTest, KeepsSendingEct1WhileTheMarkingIsPreserved) {
  Ect1Policy policy = CreateEnabledPolicy();
  Timestamp now = kStartTime;
  for (int i = 0; i < 100; ++i) {
    policy.OnPacketSent(now, /*sent_as_ect1=*/true);
    policy.OnPacketsFeedback(/*has_bleached_ect1=*/false,
                             /*has_preserved_ect1=*/true);
    now += kSendInterval;
  }
  EXPECT_TRUE(policy.ShouldSendEct1());
}

TEST(Ect1PolicyTest, FeedbackForPacketsSentBeforeEct1KeepsEct1Enabled) {
  // Just after ECT(1) is enabled, feedback still reports packets that were
  // sent as Not-ECT. It proves that packets reach the receiver, so it must
  // stop the timeout just like feedback that acknowledges an ECT(1) packet.
  Ect1Policy policy = CreateEnabledPolicy();
  Timestamp last_send = kStartTime + kTimeoutBeforeAck;
  SendEct1Packets(kStartTime, last_send, policy);
  policy.OnPacketsFeedback(/*has_bleached_ect1=*/false,
                           /*has_preserved_ect1=*/false);

  // Without the feedback this packet would stop the marking.
  policy.OnPacketSent(last_send + kSendInterval, /*sent_as_ect1=*/true);
  EXPECT_TRUE(policy.ShouldSendEct1());
}

TEST(Ect1PolicyTest, UsesLongerTimeoutAfterTheMarkingIsPreserved) {
  Ect1Policy policy = CreateEnabledPolicy();
  policy.OnPacketSent(kStartTime, /*sent_as_ect1=*/true);
  policy.OnPacketsFeedback(/*has_bleached_ect1=*/false,
                           /*has_preserved_ect1=*/true);

  // Sending past the short timeout is no longer enough.
  Timestamp start = kStartTime + kSendInterval;
  SendEct1Packets(start, start + kTimeoutBeforeAck + kSendInterval, policy);
  EXPECT_TRUE(policy.ShouldSendEct1());

  // Sending past the long timeout is.
  SendEct1Packets(start + kTimeoutBeforeAck + 2 * kSendInterval,
                  start + kTimeoutAfterAck + kSendInterval, policy);
  EXPECT_FALSE(policy.ShouldSendEct1());
}

TEST(Ect1PolicyTest, DoesNotStopSendingEct1AfterAPauseInSending) {
  // Nothing can be concluded from a period where nothing was sent, however
  // long it is.
  Ect1Policy policy = CreateEnabledPolicy();
  policy.OnPacketSent(kStartTime, /*sent_as_ect1=*/true);
  policy.OnPacketSent(kStartTime + TimeDelta::Seconds(30),
                      /*sent_as_ect1=*/true);
  EXPECT_TRUE(policy.ShouldSendEct1());
}

TEST(Ect1PolicyTest, DoesNotStopSendingEct1AfterAPauseFollowingALostPacket) {
  Ect1Policy policy = CreateEnabledPolicy();
  // ECT(1) is known to work on this route.
  policy.OnPacketSent(kStartTime, /*sent_as_ect1=*/true);
  policy.OnPacketsFeedback(/*has_bleached_ect1=*/false,
                           /*has_preserved_ect1=*/true);

  // A packet is sent and lost to ordinary loss, then sending pauses for 5s.
  // That is longer than the timeout but says nothing about the path.
  Timestamp last_send = kStartTime + kSendInterval;
  policy.OnPacketSent(last_send, /*sent_as_ect1=*/true);
  policy.OnPacketSent(last_send + TimeDelta::Seconds(5),
                      /*sent_as_ect1=*/true);
  EXPECT_TRUE(policy.ShouldSendEct1());
}

TEST(Ect1PolicyTest, StopsSendingEct1WhenCongestedAndNoFeedbackArrives) {
  // Without feedback the congestion window fills up and the pacer only sends
  // a packet every kCongestedPacketInterval. Detection must still happen.
  Ect1Policy policy = CreateEnabledPolicy();
  Timestamp now = kStartTime;
  for (int i = 0; i < 10 && policy.ShouldSendEct1(); ++i) {
    policy.OnPacketSent(now, /*sent_as_ect1=*/true);
    now += TimeDelta::Millis(500);
  }
  EXPECT_FALSE(policy.ShouldSendEct1());
}

TEST(Ect1PolicyTest, RouteChangeReEnablesEct1AfterEct1PacketsWereDropped) {
  Ect1Policy policy = CreatePolicyThatDetectedDroppedEct1();
  ASSERT_FALSE(policy.ShouldSendEct1());

  policy.OnNetworkRouteChanged();
  EXPECT_TRUE(policy.ShouldSendEct1());
}

TEST(Ect1PolicyTest, RetriesEct1AfterTheRetryDelay) {
  Ect1Policy policy = CreatePolicyThatDetectedDroppedEct1();
  ASSERT_FALSE(policy.ShouldSendEct1());
  // Sending continues as Not-ECT while ECT(1) is disabled.
  policy.OnPacketSent(kDropDetectedTime + kRetryDelay + kSendInterval,
                      /*sent_as_ect1=*/false);
  EXPECT_TRUE(policy.ShouldSendEct1());
}

TEST(Ect1PolicyTest, DoesNotRetryEct1BeforeTheRetryDelay) {
  Ect1Policy policy = CreatePolicyThatDetectedDroppedEct1();
  ASSERT_FALSE(policy.ShouldSendEct1());
  policy.OnPacketSent(kDropDetectedTime + kRetryDelay - kSendInterval,
                      /*sent_as_ect1=*/false);
  EXPECT_FALSE(policy.ShouldSendEct1());
}

TEST(Ect1PolicyTest, RetriesEct1OnlyOncePerRoute) {
  Ect1Policy policy = CreatePolicyThatDetectedDroppedEct1();
  ASSERT_FALSE(policy.ShouldSendEct1());
  Timestamp retry_time = kDropDetectedTime + kRetryDelay + kSendInterval;
  policy.OnPacketSent(retry_time, /*sent_as_ect1=*/false);
  ASSERT_TRUE(policy.ShouldSendEct1());

  // The retry finds that the packets are still dropped.
  Timestamp detected_again = retry_time + kTimeoutBeforeAck + kSendInterval;
  SendEct1Packets(retry_time, detected_again, policy);
  ASSERT_FALSE(policy.ShouldSendEct1());

  policy.OnPacketSent(detected_again + kRetryDelay + kSendInterval,
                      /*sent_as_ect1=*/false);
  EXPECT_FALSE(policy.ShouldSendEct1());
}

TEST(Ect1PolicyTest, RouteChangeAllowsANewRetry) {
  Ect1Policy policy = CreatePolicyThatDetectedDroppedEct1();
  ASSERT_FALSE(policy.ShouldSendEct1());

  policy.OnNetworkRouteChanged();

  Timestamp start = kDropDetectedTime + kSendInterval;
  Timestamp detected_again = start + kTimeoutBeforeAck + kSendInterval;
  SendEct1Packets(start, detected_again, policy);
  ASSERT_FALSE(policy.ShouldSendEct1());

  policy.OnPacketSent(detected_again + kRetryDelay + kSendInterval,
                      /*sent_as_ect1=*/false);
  EXPECT_TRUE(policy.ShouldSendEct1());
}

TEST(Ect1PolicyTest, DoesNotRetryEct1AfterBleaching) {
  Ect1Policy policy = CreateEnabledPolicy();
  policy.OnPacketsFeedback(/*has_bleached_ect1=*/true,
                           /*has_preserved_ect1=*/true);
  ASSERT_FALSE(policy.ShouldSendEct1());

  policy.OnPacketSent(kStartTime + kRetryDelay + kSendInterval,
                      /*sent_as_ect1=*/false);
  EXPECT_FALSE(policy.ShouldSendEct1());
}

TEST(Ect1PolicyTest, RouteChangeRestoresTheShortTimeout) {
  Ect1Policy policy = CreateEnabledPolicy();
  policy.OnPacketSent(kStartTime, /*sent_as_ect1=*/true);
  policy.OnPacketsFeedback(/*has_bleached_ect1=*/false,
                           /*has_preserved_ect1=*/true);
  policy.OnNetworkRouteChanged();

  SendEct1Packets(kStartTime, kDropDetectedTime, policy);
  EXPECT_FALSE(policy.ShouldSendEct1());
}

}  // namespace
}  // namespace webrtc
