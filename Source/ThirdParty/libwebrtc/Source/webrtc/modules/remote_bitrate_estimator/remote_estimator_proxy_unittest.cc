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

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/types/optional.h"
#include "api/transport/network_types.h"
#include "api/transport/test/mock_network_control.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "modules/rtp_rtcp/source/rtcp_packet/transport_feedback.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::Return;
using ::testing::SizeIs;

constexpr uint32_t kMediaSsrc = 456;
constexpr uint16_t kBaseSeq = 10;
constexpr Timestamp kBaseTime = Timestamp::Millis(123);
constexpr TimeDelta kBaseTimeWrapAround =
    rtcp::TransportFeedback::kDeltaTick * (int64_t{1} << 32);
constexpr TimeDelta kMaxSmallDelta = rtcp::TransportFeedback::kDeltaTick * 0xFF;

constexpr TimeDelta kBackWindow = TimeDelta::Millis(500);
constexpr TimeDelta kMinSendInterval = TimeDelta::Millis(50);
constexpr TimeDelta kMaxSendInterval = TimeDelta::Millis(250);
constexpr TimeDelta kDefaultSendInterval = TimeDelta::Millis(100);

std::vector<uint16_t> SequenceNumbers(
    const rtcp::TransportFeedback& feedback_packet) {
  std::vector<uint16_t> sequence_numbers;
  for (const auto& rtp_packet_received : feedback_packet.GetReceivedPackets()) {
    sequence_numbers.push_back(rtp_packet_received.sequence_number());
  }
  return sequence_numbers;
}

std::vector<Timestamp> Timestamps(
    const rtcp::TransportFeedback& feedback_packet) {
  std::vector<Timestamp> timestamps;
  Timestamp timestamp = feedback_packet.BaseTime();
  // rtcp::TransportFeedback makes no promises about epoch of the base time,
  // It may add several kBaseTimeWrapAround periods to make it large enough and
  // thus to support negative deltas. Align it close to the kBaseTime to make
  // tests expectations simpler.
  if (timestamp > kBaseTime) {
    timestamp -= (timestamp - kBaseTime).RoundTo(kBaseTimeWrapAround);
  }
  for (const auto& rtp_packet_received : feedback_packet.GetReceivedPackets()) {
    timestamp += rtp_packet_received.delta();
    timestamps.push_back(timestamp);
  }
  return timestamps;
}

class RemoteEstimatorProxyTest : public ::testing::Test {
 public:
  RemoteEstimatorProxyTest()
      : clock_(0),
        proxy_(feedback_sender_.AsStdFunction(), &network_state_estimator_) {}

 protected:
  void IncomingPacket(uint16_t seq,
                      Timestamp arrival_time,
                      absl::optional<uint32_t> abs_send_time = absl::nullopt) {
    RtpHeaderExtensionMap map;
    map.Register<TransportSequenceNumber>(1);
    map.Register<AbsoluteSendTime>(2);
    RtpPacketReceived packet(&map, arrival_time);
    packet.SetSsrc(kMediaSsrc);
    packet.SetExtension<TransportSequenceNumber>(seq);
    if (abs_send_time) {
      packet.SetExtension<AbsoluteSendTime>(*abs_send_time);
    }
    proxy_.IncomingPacket(packet);
  }

  void IncomingPacketV2(
      uint16_t seq,
      Timestamp arrival_time,
      absl::optional<FeedbackRequest> feedback_request = absl::nullopt) {
    RtpHeaderExtensionMap map;
    map.Register<TransportSequenceNumberV2>(1);
    RtpPacketReceived packet(&map, arrival_time);
    packet.SetSsrc(kMediaSsrc);
    packet.SetExtension<webrtc::TransportSequenceNumberV2>(seq,
                                                           feedback_request);
    proxy_.IncomingPacket(packet);
  }

  void Process() {
    clock_.AdvanceTime(kDefaultSendInterval);
    proxy_.Process(clock_.CurrentTime());
  }

  SimulatedClock clock_;
  MockFunction<void(std::vector<std::unique_ptr<rtcp::RtcpPacket>>)>
      feedback_sender_;
  ::testing::NiceMock<MockNetworkStateEstimator> network_state_estimator_;
  RemoteEstimatorProxy proxy_;
};

TEST_F(RemoteEstimatorProxyTest, SendsSinglePacketFeedback) {
  IncomingPacket(kBaseSeq, kBaseTime);

  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(Invoke(
          [](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kBaseSeq, feedback_packet->GetBaseSequence());
            EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

            EXPECT_THAT(SequenceNumbers(*feedback_packet),
                        ElementsAre(kBaseSeq));
            EXPECT_THAT(Timestamps(*feedback_packet), ElementsAre(kBaseTime));
          }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, DuplicatedPackets) {
  IncomingPacket(kBaseSeq, kBaseTime);
  IncomingPacket(kBaseSeq, kBaseTime + TimeDelta::Seconds(1));

  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(Invoke(
          [](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kBaseSeq, feedback_packet->GetBaseSequence());
            EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

            EXPECT_THAT(SequenceNumbers(*feedback_packet),
                        ElementsAre(kBaseSeq));
            EXPECT_THAT(Timestamps(*feedback_packet), ElementsAre(kBaseTime));
            return true;
          }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, FeedbackWithMissingStart) {
  // First feedback.
  IncomingPacket(kBaseSeq, kBaseTime);
  IncomingPacket(kBaseSeq + 1, kBaseTime + TimeDelta::Seconds(1));
  EXPECT_CALL(feedback_sender_, Call);
  Process();

  // Second feedback starts with a missing packet (DROP kBaseSeq + 2).
  IncomingPacket(kBaseSeq + 3, kBaseTime + TimeDelta::Seconds(3));

  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(Invoke(
          [](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kBaseSeq + 2, feedback_packet->GetBaseSequence());
            EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

            EXPECT_THAT(SequenceNumbers(*feedback_packet),
                        ElementsAre(kBaseSeq + 3));
            EXPECT_THAT(Timestamps(*feedback_packet),
                        ElementsAre(kBaseTime + TimeDelta::Seconds(3)));
          }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, SendsFeedbackWithVaryingDeltas) {
  IncomingPacket(kBaseSeq, kBaseTime);
  IncomingPacket(kBaseSeq + 1, kBaseTime + kMaxSmallDelta);
  IncomingPacket(kBaseSeq + 2,
                 kBaseTime + (2 * kMaxSmallDelta) + TimeDelta::Millis(1));

  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(Invoke(
          [](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kBaseSeq, feedback_packet->GetBaseSequence());
            EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

            EXPECT_THAT(SequenceNumbers(*feedback_packet),
                        ElementsAre(kBaseSeq, kBaseSeq + 1, kBaseSeq + 2));
            EXPECT_THAT(Timestamps(*feedback_packet),
                        ElementsAre(kBaseTime, kBaseTime + kMaxSmallDelta,
                                    kBaseTime + (2 * kMaxSmallDelta) +
                                        TimeDelta::Millis(1)));
          }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, SendsFragmentedFeedback) {
  static constexpr TimeDelta kTooLargeDelta =
      rtcp::TransportFeedback::kDeltaTick * (1 << 16);

  IncomingPacket(kBaseSeq, kBaseTime);
  IncomingPacket(kBaseSeq + 1, kBaseTime + kTooLargeDelta);

  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(Invoke(
          [](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kBaseSeq, feedback_packet->GetBaseSequence());
            EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

            EXPECT_THAT(SequenceNumbers(*feedback_packet),
                        ElementsAre(kBaseSeq));
            EXPECT_THAT(Timestamps(*feedback_packet), ElementsAre(kBaseTime));
          }))
      .WillOnce(Invoke(
          [](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kBaseSeq + 1, feedback_packet->GetBaseSequence());
            EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

            EXPECT_THAT(SequenceNumbers(*feedback_packet),
                        ElementsAre(kBaseSeq + 1));
            EXPECT_THAT(Timestamps(*feedback_packet),
                        ElementsAre(kBaseTime + kTooLargeDelta));
          }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, HandlesReorderingAndWrap) {
  const TimeDelta kDelta = TimeDelta::Seconds(1);
  const uint16_t kLargeSeq = 62762;
  IncomingPacket(kBaseSeq, kBaseTime);
  IncomingPacket(kLargeSeq, kBaseTime + kDelta);

  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(Invoke(
          [&](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kLargeSeq, feedback_packet->GetBaseSequence());
            EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

            EXPECT_THAT(Timestamps(*feedback_packet),
                        ElementsAre(kBaseTime + kDelta, kBaseTime));
          }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, HandlesMalformedSequenceNumbers) {
  // This test generates incoming packets with large jumps in sequence numbers.
  // When unwrapped, the sequeunce numbers of these 30 incoming packets, will
  // span a range of roughly 650k packets. Test that we only send feedback for
  // the last packets. Test for regression found in chromium:949020.
  const TimeDelta kDelta = TimeDelta::Seconds(1);
  for (int i = 0; i < 10; ++i) {
    IncomingPacket(kBaseSeq + i, kBaseTime + 3 * i * kDelta);
    IncomingPacket(kBaseSeq + 20000 + i, kBaseTime + (3 * i + 1) * kDelta);
    IncomingPacket(kBaseSeq + 40000 + i, kBaseTime + (3 * i + 2) * kDelta);
  }

  // Only expect feedback for the last two packets.
  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(Invoke(
          [&](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());
            EXPECT_THAT(SequenceNumbers(*feedback_packet),
                        ElementsAre(kBaseSeq + 20009, kBaseSeq + 40009));
            EXPECT_THAT(
                Timestamps(*feedback_packet),
                ElementsAre(kBaseTime + 28 * kDelta, kBaseTime + 29 * kDelta));
          }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, HandlesBackwardsWrappingSequenceNumbers) {
  // This test is like HandlesMalformedSequenceNumbers but for negative wrap
  // arounds. Test that we only send feedback for the packets with highest
  // sequence numbers.  Test for regression found in chromium:949020.
  const TimeDelta kDelta = TimeDelta::Seconds(1);
  for (int i = 0; i < 10; ++i) {
    IncomingPacket(kBaseSeq + i, kBaseTime + 3 * i * kDelta);
    IncomingPacket(kBaseSeq + 40000 + i, kBaseTime + (3 * i + 1) * kDelta);
    IncomingPacket(kBaseSeq + 20000 + i, kBaseTime + (3 * i + 2) * kDelta);
  }

  // Only expect feedback for the first two packets.
  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(Invoke(
          [&](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kBaseSeq + 40000, feedback_packet->GetBaseSequence());
            EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());
            EXPECT_THAT(SequenceNumbers(*feedback_packet),
                        ElementsAre(kBaseSeq + 40000, kBaseSeq));
            EXPECT_THAT(Timestamps(*feedback_packet),
                        ElementsAre(kBaseTime + kDelta, kBaseTime));
          }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, ResendsTimestampsOnReordering) {
  IncomingPacket(kBaseSeq, kBaseTime);
  IncomingPacket(kBaseSeq + 2, kBaseTime + TimeDelta::Millis(2));

  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(Invoke(
          [](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kBaseSeq, feedback_packet->GetBaseSequence());
            EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

            EXPECT_THAT(SequenceNumbers(*feedback_packet),
                        ElementsAre(kBaseSeq, kBaseSeq + 2));
            EXPECT_THAT(
                Timestamps(*feedback_packet),
                ElementsAre(kBaseTime, kBaseTime + TimeDelta::Millis(2)));
          }));

  Process();

  IncomingPacket(kBaseSeq + 1, kBaseTime + TimeDelta::Millis(1));

  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(Invoke(
          [](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kBaseSeq + 1, feedback_packet->GetBaseSequence());
            EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

            EXPECT_THAT(SequenceNumbers(*feedback_packet),
                        ElementsAre(kBaseSeq + 1, kBaseSeq + 2));
            EXPECT_THAT(Timestamps(*feedback_packet),
                        ElementsAre(kBaseTime + TimeDelta::Millis(1),
                                    kBaseTime + TimeDelta::Millis(2)));
          }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, RemovesTimestampsOutOfScope) {
  const Timestamp kTimeoutTime = kBaseTime + kBackWindow;

  IncomingPacket(kBaseSeq + 2, kBaseTime);

  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(Invoke(
          [](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kBaseSeq + 2, feedback_packet->GetBaseSequence());

            EXPECT_THAT(Timestamps(*feedback_packet), ElementsAre(kBaseTime));
          }));

  Process();

  IncomingPacket(kBaseSeq + 3, kTimeoutTime);  // kBaseSeq + 2 times out here.

  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(Invoke(
          [&](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kBaseSeq + 3, feedback_packet->GetBaseSequence());

            EXPECT_THAT(Timestamps(*feedback_packet),
                        ElementsAre(kTimeoutTime));
          }));

  Process();

  // New group, with sequence starting below the first so that they may be
  // retransmitted.
  IncomingPacket(kBaseSeq, kBaseTime - TimeDelta::Millis(1));
  IncomingPacket(kBaseSeq + 1, kTimeoutTime - TimeDelta::Millis(1));

  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(Invoke(
          [&](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kBaseSeq, feedback_packet->GetBaseSequence());

            EXPECT_THAT(SequenceNumbers(*feedback_packet),
                        ElementsAre(kBaseSeq, kBaseSeq + 1, kBaseSeq + 3));
            EXPECT_THAT(
                Timestamps(*feedback_packet),
                ElementsAre(kBaseTime - TimeDelta::Millis(1),
                            kTimeoutTime - TimeDelta::Millis(1), kTimeoutTime));
          }));

  Process();
}

TEST_F(RemoteEstimatorProxyTest, TimeUntilNextProcessIsDefaultOnUnkownBitrate) {
  EXPECT_EQ(proxy_.Process(clock_.CurrentTime()), kDefaultSendInterval);
}

TEST_F(RemoteEstimatorProxyTest, TimeUntilNextProcessIsMinIntervalOn300kbps) {
  proxy_.OnBitrateChanged(300'000);
  EXPECT_EQ(proxy_.Process(clock_.CurrentTime()), kMinSendInterval);
}

TEST_F(RemoteEstimatorProxyTest, TimeUntilNextProcessIsMaxIntervalOn0kbps) {
  // TimeUntilNextProcess should be limited by `kMaxSendIntervalMs` when
  // bitrate is small. We choose 0 bps as a special case, which also tests
  // erroneous behaviors like division-by-zero.
  proxy_.OnBitrateChanged(0);
  EXPECT_EQ(proxy_.Process(clock_.CurrentTime()), kMaxSendInterval);
}

TEST_F(RemoteEstimatorProxyTest, TimeUntilNextProcessIsMaxIntervalOn20kbps) {
  proxy_.OnBitrateChanged(20'000);
  EXPECT_EQ(proxy_.Process(clock_.CurrentTime()), kMaxSendInterval);
}

TEST_F(RemoteEstimatorProxyTest, TwccReportsUse5PercentOfAvailableBandwidth) {
  proxy_.OnBitrateChanged(80'000);
  // 80kbps * 0.05 = TwccReportSize(68B * 8b/B) * 1000ms / SendInterval(136ms)
  EXPECT_EQ(proxy_.Process(clock_.CurrentTime()), TimeDelta::Millis(136));
}

//////////////////////////////////////////////////////////////////////////////
// Tests for the extended protocol where the feedback is explicitly requested
// by the sender.
//////////////////////////////////////////////////////////////////////////////
typedef RemoteEstimatorProxyTest RemoteEstimatorProxyOnRequestTest;
TEST_F(RemoteEstimatorProxyOnRequestTest, DisablesPeriodicProcess) {
  IncomingPacketV2(kBaseSeq, kBaseTime);
  EXPECT_EQ(proxy_.Process(clock_.CurrentTime()), TimeDelta::PlusInfinity());
}

TEST_F(RemoteEstimatorProxyOnRequestTest, ProcessDoesNotSendFeedback) {
  IncomingPacketV2(kBaseSeq, kBaseTime);
  EXPECT_CALL(feedback_sender_, Call).Times(0);
  Process();
}

TEST_F(RemoteEstimatorProxyOnRequestTest, RequestSinglePacketFeedback) {
  IncomingPacketV2(kBaseSeq, kBaseTime);
  IncomingPacketV2(kBaseSeq + 1, kBaseTime + kMaxSmallDelta);
  IncomingPacketV2(kBaseSeq + 2, kBaseTime + 2 * kMaxSmallDelta);

  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(Invoke(
          [](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kBaseSeq + 3, feedback_packet->GetBaseSequence());
            EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

            EXPECT_THAT(SequenceNumbers(*feedback_packet),
                        ElementsAre(kBaseSeq + 3));
            EXPECT_THAT(Timestamps(*feedback_packet),
                        ElementsAre(kBaseTime + 3 * kMaxSmallDelta));
          }));

  constexpr FeedbackRequest kSinglePacketFeedbackRequest = {
      /*include_timestamps=*/true, /*sequence_count=*/1};
  IncomingPacketV2(kBaseSeq + 3, kBaseTime + 3 * kMaxSmallDelta,
                   kSinglePacketFeedbackRequest);
}

TEST_F(RemoteEstimatorProxyOnRequestTest, RequestLastFivePacketFeedback) {
  int i = 0;
  for (; i < 10; ++i) {
    IncomingPacketV2(kBaseSeq + i, kBaseTime + i * kMaxSmallDelta);
  }

  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(Invoke(
          [](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kBaseSeq + 6, feedback_packet->GetBaseSequence());
            EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

            EXPECT_THAT(SequenceNumbers(*feedback_packet),
                        ElementsAre(kBaseSeq + 6, kBaseSeq + 7, kBaseSeq + 8,
                                    kBaseSeq + 9, kBaseSeq + 10));
            EXPECT_THAT(Timestamps(*feedback_packet),
                        ElementsAre(kBaseTime + 6 * kMaxSmallDelta,
                                    kBaseTime + 7 * kMaxSmallDelta,
                                    kBaseTime + 8 * kMaxSmallDelta,
                                    kBaseTime + 9 * kMaxSmallDelta,
                                    kBaseTime + 10 * kMaxSmallDelta));
          }));

  constexpr FeedbackRequest kFivePacketsFeedbackRequest = {
      /*include_timestamps=*/true, /*sequence_count=*/5};
  IncomingPacketV2(kBaseSeq + i, kBaseTime + i * kMaxSmallDelta,
                   kFivePacketsFeedbackRequest);
}

TEST_F(RemoteEstimatorProxyOnRequestTest,
       RequestLastFivePacketFeedbackMissingPackets) {
  int i = 0;
  for (; i < 10; ++i) {
    if (i != 7 && i != 9)
      IncomingPacketV2(kBaseSeq + i, kBaseTime + i * kMaxSmallDelta);
  }

  EXPECT_CALL(feedback_sender_, Call)
      .WillOnce(Invoke(
          [](std::vector<std::unique_ptr<rtcp::RtcpPacket>> feedback_packets) {
            rtcp::TransportFeedback* feedback_packet =
                static_cast<rtcp::TransportFeedback*>(
                    feedback_packets[0].get());
            EXPECT_EQ(kBaseSeq + 6, feedback_packet->GetBaseSequence());
            EXPECT_EQ(kMediaSsrc, feedback_packet->media_ssrc());

            EXPECT_THAT(SequenceNumbers(*feedback_packet),
                        ElementsAre(kBaseSeq + 6, kBaseSeq + 8, kBaseSeq + 10));
            EXPECT_THAT(Timestamps(*feedback_packet),
                        ElementsAre(kBaseTime + 6 * kMaxSmallDelta,
                                    kBaseTime + 8 * kMaxSmallDelta,
                                    kBaseTime + 10 * kMaxSmallDelta));
          }));

  constexpr FeedbackRequest kFivePacketsFeedbackRequest = {
      /*include_timestamps=*/true, /*sequence_count=*/5};
  IncomingPacketV2(kBaseSeq + i, kBaseTime + i * kMaxSmallDelta,
                   kFivePacketsFeedbackRequest);
}

TEST_F(RemoteEstimatorProxyTest, ReportsIncomingPacketToNetworkStateEstimator) {
  const DataSize kPacketOverhead = DataSize::Bytes(38);
  proxy_.SetTransportOverhead(kPacketOverhead);

  EXPECT_CALL(network_state_estimator_, OnReceivedPacket)
      .WillOnce([&](const PacketResult& packet) {
        EXPECT_EQ(packet.receive_time, kBaseTime);
        EXPECT_GT(packet.sent_packet.size, kPacketOverhead);
        // Expect first send time to be equal to the arrival time.
        EXPECT_EQ(packet.sent_packet.send_time, kBaseTime);
      });
  IncomingPacket(kBaseSeq, kBaseTime, AbsoluteSendTime::To24Bits(kBaseTime));
}

TEST_F(RemoteEstimatorProxyTest, IncomingPacketHandlesWrapInAbsSendTime) {
  // abs send time use 24bit precision.
  const uint32_t kFirstAbsSendTime =
      AbsoluteSendTime::To24Bits(Timestamp::Millis((1 << 24) - 30));
  // Second abs send time has wrapped.
  const uint32_t kSecondAbsSendTime =
      AbsoluteSendTime::To24Bits(Timestamp::Millis(1 << 24));
  const TimeDelta kExpectedAbsSendTimeDelta = TimeDelta::Millis(30);

  Timestamp first_send_timestamp = Timestamp::Zero();
  EXPECT_CALL(network_state_estimator_, OnReceivedPacket)
      .WillOnce([&](const PacketResult& packet) {
        EXPECT_EQ(packet.receive_time, kBaseTime);
        first_send_timestamp = packet.sent_packet.send_time;
      });
  IncomingPacket(kBaseSeq, kBaseTime, kFirstAbsSendTime);

  EXPECT_CALL(network_state_estimator_, OnReceivedPacket)
      .WillOnce([&](const PacketResult& packet) {
        EXPECT_EQ(packet.receive_time, kBaseTime + TimeDelta::Millis(123));
        EXPECT_EQ(packet.sent_packet.send_time.ms(),
                  (first_send_timestamp + kExpectedAbsSendTimeDelta).ms());
      });
  IncomingPacket(kBaseSeq + 1, kBaseTime + TimeDelta::Millis(123),
                 kSecondAbsSendTime);
}

TEST_F(RemoteEstimatorProxyTest, IncomingPacketHandlesReorderedPackets) {
  const uint32_t kFirstAbsSendTime =
      AbsoluteSendTime::To24Bits(Timestamp::Millis((1 << 12)));
  Timestamp first_send_timestamp = Timestamp::Zero();
  EXPECT_CALL(network_state_estimator_, OnReceivedPacket)
      .WillOnce([&](const PacketResult& packet) {
        EXPECT_EQ(packet.receive_time, kBaseTime);
        first_send_timestamp = packet.sent_packet.send_time;
      });
  IncomingPacket(kBaseSeq + 1, kBaseTime, kFirstAbsSendTime);

  const TimeDelta kExpectedAbsSendTimeDelta = -TimeDelta::Millis(30);
  const uint32_t kSecondAbsSendTime = AbsoluteSendTime::To24Bits(
      Timestamp::Millis(1 << 12) + kExpectedAbsSendTimeDelta);
  EXPECT_CALL(network_state_estimator_, OnReceivedPacket)
      .WillOnce([&](const PacketResult& packet) {
        EXPECT_EQ(packet.sent_packet.send_time.ms(),
                  (first_send_timestamp + kExpectedAbsSendTimeDelta).ms());
      });
  IncomingPacket(kBaseSeq, kBaseTime + TimeDelta::Millis(123),
                 kSecondAbsSendTime);
}

TEST_F(RemoteEstimatorProxyTest,
       IncomingPacketResetSendTimeToArrivalTimeAfterLargeArrivaltimeDelta) {
  const uint32_t kFirstAbsSendTime =
      AbsoluteSendTime::To24Bits(Timestamp::Millis((1 << 12)));
  EXPECT_CALL(network_state_estimator_, OnReceivedPacket)
      .WillOnce([&](const PacketResult& packet) {
        EXPECT_EQ(packet.receive_time, kBaseTime);
        EXPECT_EQ(packet.sent_packet.send_time, kBaseTime);
      });
  IncomingPacket(kBaseSeq + 1, kBaseTime, kFirstAbsSendTime);

  EXPECT_CALL(network_state_estimator_, OnReceivedPacket)
      .WillOnce([&](const PacketResult& packet) {
        EXPECT_EQ(packet.receive_time, kBaseTime + TimeDelta::Seconds(20));
        EXPECT_EQ(packet.sent_packet.send_time,
                  kBaseTime + TimeDelta::Seconds(20));
      });
  IncomingPacket(kBaseSeq, kBaseTime + TimeDelta::Seconds(20),
                 kFirstAbsSendTime + 123);
}

TEST_F(RemoteEstimatorProxyTest, SendTransportFeedbackAndNetworkStateUpdate) {
  IncomingPacket(kBaseSeq, kBaseTime,
                 AbsoluteSendTime::To24Bits(kBaseTime - TimeDelta::Millis(1)));

  EXPECT_CALL(network_state_estimator_, GetCurrentEstimate())
      .WillOnce(Return(NetworkStateEstimate()));
  EXPECT_CALL(feedback_sender_, Call(SizeIs(2)));
  Process();
}

}  // namespace
}  // namespace webrtc
