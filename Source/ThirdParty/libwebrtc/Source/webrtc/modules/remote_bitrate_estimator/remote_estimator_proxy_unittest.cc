/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/pacing/packet_router.h"
#include "webrtc/modules/remote_bitrate_estimator/remote_estimator_proxy.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/test/gmock.h"
#include "webrtc/test/gtest.h"

using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Return;

namespace webrtc {

class MockPacketRouter : public PacketRouter {
 public:
  MOCK_METHOD1(SendFeedback, bool(rtcp::TransportFeedback* packet));
};

class RemoteEstimatorProxyTest : public ::testing::Test {
 public:
  RemoteEstimatorProxyTest() : clock_(0), proxy_(&clock_, &router_) {}

 protected:
  void IncomingPacket(uint16_t seq, int64_t time_ms) {
    RTPHeader header;
    header.extension.hasTransportSequenceNumber = true;
    header.extension.transportSequenceNumber = seq;
    header.ssrc = kMediaSsrc;
    proxy_.IncomingPacket(time_ms, kDefaultPacketSize, header);
  }

  void Process() {
    clock_.AdvanceTimeMilliseconds(
        RemoteEstimatorProxy::kDefaultSendIntervalMs);
    proxy_.Process();
  }

  SimulatedClock clock_;
  testing::StrictMock<MockPacketRouter> router_;
  RemoteEstimatorProxy proxy_;

  const size_t kDefaultPacketSize = 100;
  const uint32_t kMediaSsrc = 456;
  const uint16_t kBaseSeq = 10;
  const int64_t kBaseTimeMs = 123;
  const int64_t kMaxSmallDeltaMs =
      (rtcp::TransportFeedback::kDeltaScaleFactor * 0xFF) / 1000;
};

TEST_F(RemoteEstimatorProxyTest, SendsSinglePacketFeedback) {
  IncomingPacket(kBaseSeq, kBaseTimeMs);

  EXPECT_CALL(router_, SendFeedback(_))
      .Times(1)
      .WillOnce(Invoke([this](rtcp::TransportFeedback* packet) {
        packet->Build();
        EXPECT_EQ(kBaseSeq, packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, packet->media_ssrc());

        std::vector<rtcp::TransportFeedback::StatusSymbol> status_vec =
            packet->GetStatusVector();
        EXPECT_EQ(1u, status_vec.size());
        EXPECT_EQ(rtcp::TransportFeedback::StatusSymbol::kReceivedSmallDelta,
                  status_vec[0]);
        std::vector<int64_t> delta_vec = packet->GetReceiveDeltasUs();
        EXPECT_EQ(1u, delta_vec.size());
        EXPECT_EQ(kBaseTimeMs, (packet->GetBaseTimeUs() + delta_vec[0]) / 1000);
        return true;
      }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, DuplicatedPackets) {
  IncomingPacket(kBaseSeq, kBaseTimeMs);
  IncomingPacket(kBaseSeq, kBaseTimeMs + 1000);

  EXPECT_CALL(router_, SendFeedback(_))
      .Times(1)
      .WillOnce(Invoke([this](rtcp::TransportFeedback* packet) {
        packet->Build();
        EXPECT_EQ(kBaseSeq, packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, packet->media_ssrc());

        std::vector<rtcp::TransportFeedback::StatusSymbol> status_vec =
            packet->GetStatusVector();
        EXPECT_EQ(1u, status_vec.size());
        EXPECT_EQ(rtcp::TransportFeedback::StatusSymbol::kReceivedSmallDelta,
                  status_vec[0]);
        std::vector<int64_t> delta_vec = packet->GetReceiveDeltasUs();
        EXPECT_EQ(1u, delta_vec.size());
        EXPECT_EQ(kBaseTimeMs, (packet->GetBaseTimeUs() + delta_vec[0]) / 1000);
        return true;
      }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, FeedbackWithMissingStart) {
  // First feedback.
  IncomingPacket(kBaseSeq, kBaseTimeMs);
  IncomingPacket(kBaseSeq + 1, kBaseTimeMs + 1000);
  EXPECT_CALL(router_, SendFeedback(_)).Times(1).WillOnce(Return(true));
  Process();

  // Second feedback starts with a missing packet (DROP kBaseSeq + 2).
  IncomingPacket(kBaseSeq + 3, kBaseTimeMs + 3000);

  EXPECT_CALL(router_, SendFeedback(_))
      .Times(1)
      .WillOnce(Invoke([this](rtcp::TransportFeedback* packet) {
        packet->Build();
        EXPECT_EQ(kBaseSeq + 2, packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, packet->media_ssrc());

        std::vector<rtcp::TransportFeedback::StatusSymbol> status_vec =
            packet->GetStatusVector();
        EXPECT_EQ(2u, status_vec.size());
        EXPECT_EQ(rtcp::TransportFeedback::StatusSymbol::kNotReceived,
                  status_vec[0]);
        EXPECT_EQ(rtcp::TransportFeedback::StatusSymbol::kReceivedSmallDelta,
                  status_vec[1]);
        std::vector<int64_t> delta_vec = packet->GetReceiveDeltasUs();
        EXPECT_EQ(1u, delta_vec.size());
        EXPECT_EQ(kBaseTimeMs + 3000,
                  (packet->GetBaseTimeUs() + delta_vec[0]) / 1000);
        return true;
      }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, SendsFeedbackWithVaryingDeltas) {
  IncomingPacket(kBaseSeq, kBaseTimeMs);
  IncomingPacket(kBaseSeq + 1, kBaseTimeMs + kMaxSmallDeltaMs);
  IncomingPacket(kBaseSeq + 2, kBaseTimeMs + (2 * kMaxSmallDeltaMs) + 1);

  EXPECT_CALL(router_, SendFeedback(_))
      .Times(1)
      .WillOnce(Invoke([this](rtcp::TransportFeedback* packet) {
        packet->Build();
        EXPECT_EQ(kBaseSeq, packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, packet->media_ssrc());

        std::vector<rtcp::TransportFeedback::StatusSymbol> status_vec =
            packet->GetStatusVector();
        EXPECT_EQ(3u, status_vec.size());
        EXPECT_EQ(rtcp::TransportFeedback::StatusSymbol::kReceivedSmallDelta,
                  status_vec[0]);
        EXPECT_EQ(rtcp::TransportFeedback::StatusSymbol::kReceivedSmallDelta,
                  status_vec[1]);
        EXPECT_EQ(rtcp::TransportFeedback::StatusSymbol::kReceivedLargeDelta,
                  status_vec[2]);

        std::vector<int64_t> delta_vec = packet->GetReceiveDeltasUs();
        EXPECT_EQ(3u, delta_vec.size());
        EXPECT_EQ(kBaseTimeMs, (packet->GetBaseTimeUs() + delta_vec[0]) / 1000);
        EXPECT_EQ(kMaxSmallDeltaMs, delta_vec[1] / 1000);
        EXPECT_EQ(kMaxSmallDeltaMs + 1, delta_vec[2] / 1000);
        return true;
      }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, SendsFragmentedFeedback) {
  const int64_t kTooLargeDelta =
      rtcp::TransportFeedback::kDeltaScaleFactor * (1 << 16);

  IncomingPacket(kBaseSeq, kBaseTimeMs);
  IncomingPacket(kBaseSeq + 1, kBaseTimeMs + kTooLargeDelta);

  InSequence s;
  EXPECT_CALL(router_, SendFeedback(_))
      .Times(1)
      .WillOnce(Invoke([kTooLargeDelta, this](rtcp::TransportFeedback* packet) {
        packet->Build();
        EXPECT_EQ(kBaseSeq, packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, packet->media_ssrc());

        std::vector<rtcp::TransportFeedback::StatusSymbol> status_vec =
            packet->GetStatusVector();
        EXPECT_EQ(1u, status_vec.size());
        EXPECT_EQ(rtcp::TransportFeedback::StatusSymbol::kReceivedSmallDelta,
                  status_vec[0]);
        std::vector<int64_t> delta_vec = packet->GetReceiveDeltasUs();
        EXPECT_EQ(1u, delta_vec.size());
        EXPECT_EQ(kBaseTimeMs, (packet->GetBaseTimeUs() + delta_vec[0]) / 1000);
        return true;
      }))
      .RetiresOnSaturation();

  EXPECT_CALL(router_, SendFeedback(_))
      .Times(1)
      .WillOnce(Invoke([kTooLargeDelta, this](rtcp::TransportFeedback* packet) {
        packet->Build();
        EXPECT_EQ(kBaseSeq + 1, packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, packet->media_ssrc());

        std::vector<rtcp::TransportFeedback::StatusSymbol> status_vec =
            packet->GetStatusVector();
        EXPECT_EQ(1u, status_vec.size());
        EXPECT_EQ(rtcp::TransportFeedback::StatusSymbol::kReceivedSmallDelta,
                  status_vec[0]);
        std::vector<int64_t> delta_vec = packet->GetReceiveDeltasUs();
        EXPECT_EQ(1u, delta_vec.size());
        EXPECT_EQ(kBaseTimeMs + kTooLargeDelta,
                  (packet->GetBaseTimeUs() + delta_vec[0]) / 1000);
        return true;
      }))
      .RetiresOnSaturation();

  Process();
}

TEST_F(RemoteEstimatorProxyTest, GracefullyHandlesReorderingAndWrap) {
  const int64_t kDeltaMs = 1000;
  const uint16_t kLargeSeq = 62762;
  IncomingPacket(kBaseSeq, kBaseTimeMs);
  IncomingPacket(kLargeSeq, kBaseTimeMs + kDeltaMs);

  EXPECT_CALL(router_, SendFeedback(_))
      .Times(1)
      .WillOnce(Invoke([this](rtcp::TransportFeedback* packet) {
        packet->Build();
        EXPECT_EQ(kBaseSeq, packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, packet->media_ssrc());

        std::vector<int64_t> delta_vec = packet->GetReceiveDeltasUs();
        EXPECT_EQ(1u, delta_vec.size());
        EXPECT_EQ(kBaseTimeMs, (packet->GetBaseTimeUs() + delta_vec[0]) / 1000);
        return true;
      }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, ResendsTimestampsOnReordering) {
  IncomingPacket(kBaseSeq, kBaseTimeMs);
  IncomingPacket(kBaseSeq + 2, kBaseTimeMs + 2);

  EXPECT_CALL(router_, SendFeedback(_))
      .Times(1)
      .WillOnce(Invoke([this](rtcp::TransportFeedback* packet) {
        packet->Build();
        EXPECT_EQ(kBaseSeq, packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, packet->media_ssrc());

        std::vector<int64_t> delta_vec = packet->GetReceiveDeltasUs();
        EXPECT_EQ(2u, delta_vec.size());
        EXPECT_EQ(kBaseTimeMs, (packet->GetBaseTimeUs() + delta_vec[0]) / 1000);
        EXPECT_EQ(2, delta_vec[1] / 1000);
        return true;
      }));

  Process();

  IncomingPacket(kBaseSeq + 1, kBaseTimeMs + 1);

  EXPECT_CALL(router_, SendFeedback(_))
      .Times(1)
      .WillOnce(Invoke([this](rtcp::TransportFeedback* packet) {
        packet->Build();
        EXPECT_EQ(kBaseSeq + 1, packet->GetBaseSequence());
        EXPECT_EQ(kMediaSsrc, packet->media_ssrc());

        std::vector<int64_t> delta_vec = packet->GetReceiveDeltasUs();
        EXPECT_EQ(2u, delta_vec.size());
        EXPECT_EQ(kBaseTimeMs + 1,
                  (packet->GetBaseTimeUs() + delta_vec[0]) / 1000);
        EXPECT_EQ(1, delta_vec[1] / 1000);
        return true;
      }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, RemovesTimestampsOutOfScope) {
  const int64_t kTimeoutTimeMs =
      kBaseTimeMs + RemoteEstimatorProxy::kBackWindowMs;

  IncomingPacket(kBaseSeq + 2, kBaseTimeMs);

  EXPECT_CALL(router_, SendFeedback(_))
      .Times(1)
      .WillOnce(Invoke([kTimeoutTimeMs, this](rtcp::TransportFeedback* packet) {
        packet->Build();
        EXPECT_EQ(kBaseSeq + 2, packet->GetBaseSequence());

        std::vector<int64_t> delta_vec = packet->GetReceiveDeltasUs();
        EXPECT_EQ(1u, delta_vec.size());
        EXPECT_EQ(kBaseTimeMs, (packet->GetBaseTimeUs() + delta_vec[0]) / 1000);
        return true;
      }));

  Process();

  IncomingPacket(kBaseSeq + 3, kTimeoutTimeMs);  // kBaseSeq + 2 times out here.

  EXPECT_CALL(router_, SendFeedback(_))
      .Times(1)
      .WillOnce(Invoke([kTimeoutTimeMs, this](rtcp::TransportFeedback* packet) {
        packet->Build();
        EXPECT_EQ(kBaseSeq + 3, packet->GetBaseSequence());

        std::vector<int64_t> delta_vec = packet->GetReceiveDeltasUs();
        EXPECT_EQ(1u, delta_vec.size());
        EXPECT_EQ(kTimeoutTimeMs,
                  (packet->GetBaseTimeUs() + delta_vec[0]) / 1000);
        return true;
      }));

  Process();

  // New group, with sequence starting below the first so that they may be
  // retransmitted.
  IncomingPacket(kBaseSeq, kBaseTimeMs - 1);
  IncomingPacket(kBaseSeq + 1, kTimeoutTimeMs - 1);

  EXPECT_CALL(router_, SendFeedback(_))
      .Times(1)
      .WillOnce(Invoke([kTimeoutTimeMs, this](rtcp::TransportFeedback* packet) {
        packet->Build();
        EXPECT_EQ(kBaseSeq, packet->GetBaseSequence());

        // Four status entries (kBaseSeq + 3 missing).
        EXPECT_EQ(4u, packet->GetStatusVector().size());

        // Only three actual timestamps.
        std::vector<int64_t> delta_vec = packet->GetReceiveDeltasUs();
        EXPECT_EQ(3u, delta_vec.size());
        EXPECT_EQ(kBaseTimeMs - 1,
                  (packet->GetBaseTimeUs() + delta_vec[0]) / 1000);
        EXPECT_EQ(kTimeoutTimeMs - kBaseTimeMs, delta_vec[1] / 1000);
        EXPECT_EQ(1, delta_vec[2] / 1000);
        return true;
      }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, TimeUntilNextProcessIsZeroBeforeFirstProcess) {
  EXPECT_EQ(0, proxy_.TimeUntilNextProcess());
}

TEST_F(RemoteEstimatorProxyTest, TimeUntilNextProcessIsDefaultOnUnkownBitrate) {
  Process();
  EXPECT_EQ(RemoteEstimatorProxy::kDefaultSendIntervalMs,
            proxy_.TimeUntilNextProcess());
}

TEST_F(RemoteEstimatorProxyTest, TimeUntilNextProcessIsMinIntervalOn300kbps) {
  Process();
  proxy_.OnBitrateChanged(300000);
  EXPECT_EQ(RemoteEstimatorProxy::kMinSendIntervalMs,
            proxy_.TimeUntilNextProcess());
}

TEST_F(RemoteEstimatorProxyTest, TimeUntilNextProcessIsMaxIntervalOn0kbps) {
  Process();
  // TimeUntilNextProcess should be limited by |kMaxSendIntervalMs| when
  // bitrate is small. We choose 0 bps as a special case, which also tests
  // erroneous behaviors like division-by-zero.
  proxy_.OnBitrateChanged(0);
  EXPECT_EQ(RemoteEstimatorProxy::kMaxSendIntervalMs,
            proxy_.TimeUntilNextProcess());
}

TEST_F(RemoteEstimatorProxyTest, TimeUntilNextProcessIsMaxIntervalOn20kbps) {
  Process();
  proxy_.OnBitrateChanged(20000);
  EXPECT_EQ(RemoteEstimatorProxy::kMaxSendIntervalMs,
            proxy_.TimeUntilNextProcess());
}

TEST_F(RemoteEstimatorProxyTest, TwccReportsUse5PercentOfAvailableBandwidth) {
  Process();
  proxy_.OnBitrateChanged(80000);
  // 80kbps * 0.05 = TwccReportSize(68B * 8b/B) * 1000ms / SendInterval(136ms)
  EXPECT_EQ(136, proxy_.TimeUntilNextProcess());
}

}  // namespace webrtc
