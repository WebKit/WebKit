/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/remote_bitrate_estimator/remote_estimator_proxy.h"

#include "api/transport/field_trial_based_config.h"
#include "modules/pacing/packet_router.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::Return;

namespace webrtc {
namespace {

constexpr size_t kDefaultPacketSize = 100;
constexpr uint32_t kMediaSsrc = 456;
constexpr uint16_t kBaseSeq = 10;
constexpr int64_t kBaseTimeMs = 123;
constexpr int64_t kMaxSmallDeltaMs =
    (rtcp::TransportFeedback::kDeltaScaleFactor * 0xFF) / 1000;

constexpr int kBackWindowMs = 500;
constexpr int kMinSendIntervalMs = 50;
constexpr int kMaxSendIntervalMs = 250;
constexpr int kDefaultSendIntervalMs = 100;

std::vector<uint16_t> SequenceNumbers(
    const rtcp::TransportFeedback& feedback_packet) {
  std::vector<uint16_t> sequence_numbers;
  for (const auto& rtp_packet_received : feedback_packet.GetReceivedPackets()) {
    sequence_numbers.push_back(rtp_packet_received.sequence_number());
  }
  return sequence_numbers;
}

std::vector<int64_t> TimestampsMs(
    const rtcp::TransportFeedback& feedback_packet) {
  std::vector<int64_t> timestamps;
  int64_t timestamp_us = feedback_packet.GetBaseTimeUs();
  for (const auto& rtp_packet_received : feedback_packet.GetReceivedPackets()) {
    timestamp_us += rtp_packet_received.delta_us();
    timestamps.push_back(timestamp_us / 1000);
  }
  return timestamps;
}

class MockTransportFeedbackSender : public TransportFeedbackSenderInterface {
 public:
  MOCK_METHOD1(SendTransportFeedback,
               bool(rtcp::TransportFeedback* feedback_packet));
};

class RemoteEstimatorProxyTest : public ::testing::Test {
 public:
  RemoteEstimatorProxyTest()
      : clock_(0), proxy_(&clock_, &router_, &field_trial_config_) {}

 protected:
  void IncomingPacket(uint16_t seq,
                      int64_t time_ms,
                      absl::optional<FeedbackRequest> feedback_request) {
    RTPHeader header;
    header.extension.hasTransportSequenceNumber = true;
    header.extension.transportSequenceNumber = seq;
    header.extension.feedback_request = feedback_request;
    header.ssrc = kMediaSsrc;
    proxy_.IncomingPacket(time_ms, kDefaultPacketSize, header);
  }

  void IncomingPacket(uint16_t seq, int64_t time_ms) {
    IncomingPacket(seq, time_ms, absl::nullopt);
  }

  void Process() {
    clock_.AdvanceTimeMilliseconds(kDefaultSendIntervalMs);
    proxy_.Process();
  }

  FieldTrialBasedConfig field_trial_config_;
  SimulatedClock clock_;
  ::testing::StrictMock<MockTransportFeedbackSender> router_;
  RemoteEstimatorProxy proxy_;
};

TEST_F(RemoteEstimatorProxyTest, SendsSinglePacketFeedback) {
  IncomingPacket(kBaseSeq, kBaseTimeMs);

  EXPECT_CALL(router_, SendTransportFeedback(_))
      .WillOnce(Invoke([](rtcp::TransportFeedback* feedback_packet) {
        EXPECT_EQ(kBaseSeq, feedback_packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

        EXPECT_THAT(SequenceNumbers(*feedback_packet), ElementsAre(kBaseSeq));
        EXPECT_THAT(TimestampsMs(*feedback_packet), ElementsAre(kBaseTimeMs));
        return true;
      }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, DuplicatedPackets) {
  IncomingPacket(kBaseSeq, kBaseTimeMs);
  IncomingPacket(kBaseSeq, kBaseTimeMs + 1000);

  EXPECT_CALL(router_, SendTransportFeedback(_))
      .WillOnce(Invoke([](rtcp::TransportFeedback* feedback_packet) {
        EXPECT_EQ(kBaseSeq, feedback_packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

        EXPECT_THAT(SequenceNumbers(*feedback_packet), ElementsAre(kBaseSeq));
        EXPECT_THAT(TimestampsMs(*feedback_packet), ElementsAre(kBaseTimeMs));
        return true;
      }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, FeedbackWithMissingStart) {
  // First feedback.
  IncomingPacket(kBaseSeq, kBaseTimeMs);
  IncomingPacket(kBaseSeq + 1, kBaseTimeMs + 1000);
  EXPECT_CALL(router_, SendTransportFeedback(_)).WillOnce(Return(true));
  Process();

  // Second feedback starts with a missing packet (DROP kBaseSeq + 2).
  IncomingPacket(kBaseSeq + 3, kBaseTimeMs + 3000);

  EXPECT_CALL(router_, SendTransportFeedback(_))
      .WillOnce(Invoke([](rtcp::TransportFeedback* feedback_packet) {
        EXPECT_EQ(kBaseSeq + 2, feedback_packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

        EXPECT_THAT(SequenceNumbers(*feedback_packet),
                    ElementsAre(kBaseSeq + 3));
        EXPECT_THAT(TimestampsMs(*feedback_packet),
                    ElementsAre(kBaseTimeMs + 3000));
        return true;
      }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, SendsFeedbackWithVaryingDeltas) {
  IncomingPacket(kBaseSeq, kBaseTimeMs);
  IncomingPacket(kBaseSeq + 1, kBaseTimeMs + kMaxSmallDeltaMs);
  IncomingPacket(kBaseSeq + 2, kBaseTimeMs + (2 * kMaxSmallDeltaMs) + 1);

  EXPECT_CALL(router_, SendTransportFeedback(_))
      .WillOnce(Invoke([](rtcp::TransportFeedback* feedback_packet) {
        EXPECT_EQ(kBaseSeq, feedback_packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

        EXPECT_THAT(SequenceNumbers(*feedback_packet),
                    ElementsAre(kBaseSeq, kBaseSeq + 1, kBaseSeq + 2));
        EXPECT_THAT(TimestampsMs(*feedback_packet),
                    ElementsAre(kBaseTimeMs, kBaseTimeMs + kMaxSmallDeltaMs,
                                kBaseTimeMs + (2 * kMaxSmallDeltaMs) + 1));
        return true;
      }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, SendsFragmentedFeedback) {
  static constexpr int64_t kTooLargeDelta =
      rtcp::TransportFeedback::kDeltaScaleFactor * (1 << 16);

  IncomingPacket(kBaseSeq, kBaseTimeMs);
  IncomingPacket(kBaseSeq + 1, kBaseTimeMs + kTooLargeDelta);

  EXPECT_CALL(router_, SendTransportFeedback(_))
      .WillOnce(Invoke([](rtcp::TransportFeedback* feedback_packet) {
        EXPECT_EQ(kBaseSeq, feedback_packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

        EXPECT_THAT(SequenceNumbers(*feedback_packet), ElementsAre(kBaseSeq));
        EXPECT_THAT(TimestampsMs(*feedback_packet), ElementsAre(kBaseTimeMs));
        return true;
      }))
      .WillOnce(Invoke([](rtcp::TransportFeedback* feedback_packet) {
        EXPECT_EQ(kBaseSeq + 1, feedback_packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

        EXPECT_THAT(SequenceNumbers(*feedback_packet),
                    ElementsAre(kBaseSeq + 1));
        EXPECT_THAT(TimestampsMs(*feedback_packet),
                    ElementsAre(kBaseTimeMs + kTooLargeDelta));
        return true;
      }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, HandlesReorderingAndWrap) {
  const int64_t kDeltaMs = 1000;
  const uint16_t kLargeSeq = 62762;
  IncomingPacket(kBaseSeq, kBaseTimeMs);
  IncomingPacket(kLargeSeq, kBaseTimeMs + kDeltaMs);

  EXPECT_CALL(router_, SendTransportFeedback(_))
      .WillOnce(Invoke([&](rtcp::TransportFeedback* feedback_packet) {
        EXPECT_EQ(kLargeSeq, feedback_packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

        EXPECT_THAT(TimestampsMs(*feedback_packet),
                    ElementsAre(kBaseTimeMs + kDeltaMs, kBaseTimeMs));
        return true;
      }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, HandlesMalformedSequenceNumbers) {
  // This test generates incoming packets with large jumps in sequence numbers.
  // When unwrapped, the sequeunce numbers of these 30 incoming packets, will
  // span a range of roughly 650k packets. Test that we only send feedback for
  // the last packets. Test for regression found in chromium:949020.
  const int64_t kDeltaMs = 1000;
  for (int i = 0; i < 10; ++i) {
    IncomingPacket(kBaseSeq + i, kBaseTimeMs + 3 * i * kDeltaMs);
    IncomingPacket(kBaseSeq + 20000 + i, kBaseTimeMs + (3 * i + 1) * kDeltaMs);
    IncomingPacket(kBaseSeq + 40000 + i, kBaseTimeMs + (3 * i + 2) * kDeltaMs);
  }

  // Only expect feedback for the last two packets.
  EXPECT_CALL(router_, SendTransportFeedback(_))
      .WillOnce(Invoke([&](rtcp::TransportFeedback* feedback_packet) {
        EXPECT_EQ(kBaseSeq + 20000 + 9, feedback_packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());
        EXPECT_THAT(SequenceNumbers(*feedback_packet),
                    ElementsAre(kBaseSeq + 20009, kBaseSeq + 40009));
        EXPECT_THAT(TimestampsMs(*feedback_packet),
                    ElementsAre(kBaseTimeMs + 28 * kDeltaMs,
                                kBaseTimeMs + 29 * kDeltaMs));
        return true;
      }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, HandlesBackwardsWrappingSequenceNumbers) {
  // This test is like HandlesMalformedSequenceNumbers but for negative wrap
  // arounds. Test that we only send feedback for the packets with highest
  // sequence numbers.  Test for regression found in chromium:949020.
  const int64_t kDeltaMs = 1000;
  for (int i = 0; i < 10; ++i) {
    IncomingPacket(kBaseSeq + i, kBaseTimeMs + 3 * i * kDeltaMs);
    IncomingPacket(kBaseSeq + 40000 + i, kBaseTimeMs + (3 * i + 1) * kDeltaMs);
    IncomingPacket(kBaseSeq + 20000 + i, kBaseTimeMs + (3 * i + 2) * kDeltaMs);
  }

  // Only expect feedback for the first two packets.
  EXPECT_CALL(router_, SendTransportFeedback(_))
      .WillOnce(Invoke([&](rtcp::TransportFeedback* feedback_packet) {
        EXPECT_EQ(kBaseSeq + 40000, feedback_packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());
        EXPECT_THAT(SequenceNumbers(*feedback_packet),
                    ElementsAre(kBaseSeq + 40000, kBaseSeq));
        EXPECT_THAT(TimestampsMs(*feedback_packet),
                    ElementsAre(kBaseTimeMs + kDeltaMs, kBaseTimeMs));
        return true;
      }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, ResendsTimestampsOnReordering) {
  IncomingPacket(kBaseSeq, kBaseTimeMs);
  IncomingPacket(kBaseSeq + 2, kBaseTimeMs + 2);

  EXPECT_CALL(router_, SendTransportFeedback(_))
      .WillOnce(Invoke([](rtcp::TransportFeedback* feedback_packet) {
        EXPECT_EQ(kBaseSeq, feedback_packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

        EXPECT_THAT(SequenceNumbers(*feedback_packet),
                    ElementsAre(kBaseSeq, kBaseSeq + 2));
        EXPECT_THAT(TimestampsMs(*feedback_packet),
                    ElementsAre(kBaseTimeMs, kBaseTimeMs + 2));
        return true;
      }));

  Process();

  IncomingPacket(kBaseSeq + 1, kBaseTimeMs + 1);

  EXPECT_CALL(router_, SendTransportFeedback(_))
      .WillOnce(Invoke([](rtcp::TransportFeedback* feedback_packet) {
        EXPECT_EQ(kBaseSeq + 1, feedback_packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

        EXPECT_THAT(SequenceNumbers(*feedback_packet),
                    ElementsAre(kBaseSeq + 1, kBaseSeq + 2));
        EXPECT_THAT(TimestampsMs(*feedback_packet),
                    ElementsAre(kBaseTimeMs + 1, kBaseTimeMs + 2));
        return true;
      }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, RemovesTimestampsOutOfScope) {
  const int64_t kTimeoutTimeMs = kBaseTimeMs + kBackWindowMs;

  IncomingPacket(kBaseSeq + 2, kBaseTimeMs);

  EXPECT_CALL(router_, SendTransportFeedback(_))
      .WillOnce(Invoke([](rtcp::TransportFeedback* feedback_packet) {
        EXPECT_EQ(kBaseSeq + 2, feedback_packet->GetBaseSequence());

        EXPECT_THAT(TimestampsMs(*feedback_packet), ElementsAre(kBaseTimeMs));
        return true;
      }));

  Process();

  IncomingPacket(kBaseSeq + 3, kTimeoutTimeMs);  // kBaseSeq + 2 times out here.

  EXPECT_CALL(router_, SendTransportFeedback(_))
      .WillOnce(
          Invoke([kTimeoutTimeMs](rtcp::TransportFeedback* feedback_packet) {
            EXPECT_EQ(kBaseSeq + 3, feedback_packet->GetBaseSequence());

            EXPECT_THAT(TimestampsMs(*feedback_packet),
                        ElementsAre(kTimeoutTimeMs));
            return true;
          }));

  Process();

  // New group, with sequence starting below the first so that they may be
  // retransmitted.
  IncomingPacket(kBaseSeq, kBaseTimeMs - 1);
  IncomingPacket(kBaseSeq + 1, kTimeoutTimeMs - 1);

  EXPECT_CALL(router_, SendTransportFeedback(_))
      .WillOnce(
          Invoke([kTimeoutTimeMs](rtcp::TransportFeedback* feedback_packet) {
            EXPECT_EQ(kBaseSeq, feedback_packet->GetBaseSequence());

            EXPECT_THAT(SequenceNumbers(*feedback_packet),
                        ElementsAre(kBaseSeq, kBaseSeq + 1, kBaseSeq + 3));
            EXPECT_THAT(TimestampsMs(*feedback_packet),
                        ElementsAre(kBaseTimeMs - 1, kTimeoutTimeMs - 1,
                                    kTimeoutTimeMs));
            return true;
          }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, TimeUntilNextProcessIsZeroBeforeFirstProcess) {
  EXPECT_EQ(0, proxy_.TimeUntilNextProcess());
}

TEST_F(RemoteEstimatorProxyTest, TimeUntilNextProcessIsDefaultOnUnkownBitrate) {
  Process();
  EXPECT_EQ(kDefaultSendIntervalMs, proxy_.TimeUntilNextProcess());
}

TEST_F(RemoteEstimatorProxyTest, TimeUntilNextProcessIsMinIntervalOn300kbps) {
  Process();
  proxy_.OnBitrateChanged(300000);
  EXPECT_EQ(kMinSendIntervalMs, proxy_.TimeUntilNextProcess());
}

TEST_F(RemoteEstimatorProxyTest, TimeUntilNextProcessIsMaxIntervalOn0kbps) {
  Process();
  // TimeUntilNextProcess should be limited by |kMaxSendIntervalMs| when
  // bitrate is small. We choose 0 bps as a special case, which also tests
  // erroneous behaviors like division-by-zero.
  proxy_.OnBitrateChanged(0);
  EXPECT_EQ(kMaxSendIntervalMs, proxy_.TimeUntilNextProcess());
}

TEST_F(RemoteEstimatorProxyTest, TimeUntilNextProcessIsMaxIntervalOn20kbps) {
  Process();
  proxy_.OnBitrateChanged(20000);
  EXPECT_EQ(kMaxSendIntervalMs, proxy_.TimeUntilNextProcess());
}

TEST_F(RemoteEstimatorProxyTest, TwccReportsUse5PercentOfAvailableBandwidth) {
  Process();
  proxy_.OnBitrateChanged(80000);
  // 80kbps * 0.05 = TwccReportSize(68B * 8b/B) * 1000ms / SendInterval(136ms)
  EXPECT_EQ(136, proxy_.TimeUntilNextProcess());
}

//////////////////////////////////////////////////////////////////////////////
// Tests for the extended protocol where the feedback is explicitly requested
// by the sender.
//////////////////////////////////////////////////////////////////////////////
typedef RemoteEstimatorProxyTest RemoteEstimatorProxyOnRequestTest;
TEST_F(RemoteEstimatorProxyOnRequestTest, TimeUntilNextProcessIsHigh) {
  proxy_.SetSendPeriodicFeedback(false);
  EXPECT_GE(proxy_.TimeUntilNextProcess(), 60 * 60 * 1000);
}

TEST_F(RemoteEstimatorProxyOnRequestTest, ProcessDoesNotSendFeedback) {
  proxy_.SetSendPeriodicFeedback(false);
  IncomingPacket(kBaseSeq, kBaseTimeMs);
  EXPECT_CALL(router_, SendTransportFeedback(_)).Times(0);
  Process();
}

TEST_F(RemoteEstimatorProxyOnRequestTest, RequestSinglePacketFeedback) {
  proxy_.SetSendPeriodicFeedback(false);
  IncomingPacket(kBaseSeq, kBaseTimeMs);
  IncomingPacket(kBaseSeq + 1, kBaseTimeMs + kMaxSmallDeltaMs);
  IncomingPacket(kBaseSeq + 2, kBaseTimeMs + 2 * kMaxSmallDeltaMs);

  EXPECT_CALL(router_, SendTransportFeedback(_))
      .WillOnce(Invoke([](rtcp::TransportFeedback* feedback_packet) {
        EXPECT_EQ(kBaseSeq + 3, feedback_packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

        EXPECT_THAT(SequenceNumbers(*feedback_packet),
                    ElementsAre(kBaseSeq + 3));
        EXPECT_THAT(TimestampsMs(*feedback_packet),
                    ElementsAre(kBaseTimeMs + 3 * kMaxSmallDeltaMs));
        return true;
      }));

  constexpr FeedbackRequest kSinglePacketFeedbackRequest = {
      /*include_timestamps=*/true, /*sequence_count=*/1};
  IncomingPacket(kBaseSeq + 3, kBaseTimeMs + 3 * kMaxSmallDeltaMs,
                 kSinglePacketFeedbackRequest);
}

TEST_F(RemoteEstimatorProxyOnRequestTest, RequestLastFivePacketFeedback) {
  proxy_.SetSendPeriodicFeedback(false);
  int i = 0;
  for (; i < 10; ++i) {
    IncomingPacket(kBaseSeq + i, kBaseTimeMs + i * kMaxSmallDeltaMs);
  }

  EXPECT_CALL(router_, SendTransportFeedback(_))
      .WillOnce(Invoke([](rtcp::TransportFeedback* feedback_packet) {
        EXPECT_EQ(kBaseSeq + 6, feedback_packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

        EXPECT_THAT(SequenceNumbers(*feedback_packet),
                    ElementsAre(kBaseSeq + 6, kBaseSeq + 7, kBaseSeq + 8,
                                kBaseSeq + 9, kBaseSeq + 10));
        EXPECT_THAT(TimestampsMs(*feedback_packet),
                    ElementsAre(kBaseTimeMs + 6 * kMaxSmallDeltaMs,
                                kBaseTimeMs + 7 * kMaxSmallDeltaMs,
                                kBaseTimeMs + 8 * kMaxSmallDeltaMs,
                                kBaseTimeMs + 9 * kMaxSmallDeltaMs,
                                kBaseTimeMs + 10 * kMaxSmallDeltaMs));
        return true;
      }));

  constexpr FeedbackRequest kFivePacketsFeedbackRequest = {
      /*include_timestamps=*/true, /*sequence_count=*/5};
  IncomingPacket(kBaseSeq + i, kBaseTimeMs + i * kMaxSmallDeltaMs,
                 kFivePacketsFeedbackRequest);
}

TEST_F(RemoteEstimatorProxyOnRequestTest,
       RequestLastFivePacketFeedbackMissingPackets) {
  proxy_.SetSendPeriodicFeedback(false);
  int i = 0;
  for (; i < 10; ++i) {
    if (i != 7 && i != 9)
      IncomingPacket(kBaseSeq + i, kBaseTimeMs + i * kMaxSmallDeltaMs);
  }

  EXPECT_CALL(router_, SendTransportFeedback(_))
      .WillOnce(Invoke([](rtcp::TransportFeedback* feedback_packet) {
        EXPECT_EQ(kBaseSeq + 6, feedback_packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

        EXPECT_THAT(SequenceNumbers(*feedback_packet),
                    ElementsAre(kBaseSeq + 6, kBaseSeq + 8, kBaseSeq + 10));
        EXPECT_THAT(TimestampsMs(*feedback_packet),
                    ElementsAre(kBaseTimeMs + 6 * kMaxSmallDeltaMs,
                                kBaseTimeMs + 8 * kMaxSmallDeltaMs,
                                kBaseTimeMs + 10 * kMaxSmallDeltaMs));
        return true;
      }));

  constexpr FeedbackRequest kFivePacketsFeedbackRequest = {
      /*include_timestamps=*/true, /*sequence_count=*/5};
  IncomingPacket(kBaseSeq + i, kBaseTimeMs + i * kMaxSmallDeltaMs,
                 kFivePacketsFeedbackRequest);
}

}  // namespace
}  // namespace webrtc
