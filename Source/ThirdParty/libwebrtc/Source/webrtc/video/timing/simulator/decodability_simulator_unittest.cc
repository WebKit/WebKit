/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/simulator/decodability_simulator.h"

#include <memory>

#include "absl/algorithm/container.h"
#include "api/units/data_size.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "video/timing/simulator/frame_base.h"
#include "video/timing/simulator/test/parsed_rtc_event_log_from_resources.h"

namespace webrtc::video_timing_simulator {
namespace {

using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Matcher;
using ::testing::Ne;
using ::testing::SizeIs;

using Frame = DecodabilitySimulator::Frame;
using Stream = DecodabilitySimulator::Stream;

Matcher<const Frame&> EqualsFrame(const Frame& expected) {
  return AllOf(
      Field("num_packets", &Frame::num_packets, Eq(expected.num_packets)),
      Field("size", &Frame::size, Eq(expected.size)),
      Field("unwrapped_rtp_timestamp", &Frame::unwrapped_rtp_timestamp,
            Eq(expected.unwrapped_rtp_timestamp)),
      Field("assembled_timestamp", &Frame::assembled_timestamp,
            Eq(expected.assembled_timestamp)),
      Field("decodable_timestamp", &Frame::decodable_timestamp,
            Eq(expected.decodable_timestamp)));
}

TEST(DecodabilitySimulatorTest, VideoRecvVp8) {
  std::unique_ptr<ParsedRtcEventLog> parsed_log =
      ParsedRtcEventLogFromResources("video_recv_vp8_pt96");

  DecodabilitySimulator::Config config;
  DecodabilitySimulator simulator(config);
  DecodabilitySimulator::Results results = simulator.Simulate(*parsed_log);

  ASSERT_THAT(results.streams, SizeIs(1));
  const auto& stream = results.streams.front();
  EXPECT_THAT(stream.creation_timestamp, Eq(Timestamp::Millis(100942625)));
  EXPECT_THAT(stream.ssrc, Eq(3965119250));
  EXPECT_THAT(stream.frames, SizeIs(650));

  // Spot check the last frame.
  EXPECT_THAT(
      stream.frames.back(),
      EqualsFrame({.num_packets = 5,
                   .size = DataSize::Bytes(5582),
                   .unwrapped_rtp_timestamp = 2498236561,
                   .assembled_timestamp = Timestamp::Millis(100964194),
                   .decodable_timestamp = Timestamp::Millis(100964194)}));
}

TEST(DecodabilitySimulatorTest, VideoRecvVp9) {
  std::unique_ptr<ParsedRtcEventLog> parsed_log =
      ParsedRtcEventLogFromResources("video_recv_vp9_pt98");

  DecodabilitySimulator::Config config;
  DecodabilitySimulator simulator(config);
  DecodabilitySimulator::Results results = simulator.Simulate(*parsed_log);

  ASSERT_THAT(results.streams, SizeIs(1));
  const auto& stream = results.streams.front();
  EXPECT_THAT(stream.creation_timestamp, Eq(Timestamp::Millis(98718560)));
  EXPECT_THAT(stream.ssrc, Eq(2849747025));
  EXPECT_THAT(stream.frames, SizeIs(1493));

  // Spot check the last frame.
  EXPECT_THAT(
      stream.frames.back(),
      EqualsFrame({.num_packets = 6,
                   .size = DataSize::Bytes(6265),
                   .unwrapped_rtp_timestamp = 2236817278,
                   .assembled_timestamp = Timestamp::Millis(98768284),
                   .decodable_timestamp = Timestamp::Millis(98768284)}));
}

TEST(DecodabilitySimulatorTest, VideoRecvAv1) {
  std::unique_ptr<ParsedRtcEventLog> parsed_log =
      ParsedRtcEventLogFromResources("video_recv_av1_pt45");

  DecodabilitySimulator::Config config;
  DecodabilitySimulator simulator(config);
  DecodabilitySimulator::Results results = simulator.Simulate(*parsed_log);

  ASSERT_THAT(results.streams, SizeIs(1));
  const auto& stream = results.streams.front();
  EXPECT_THAT(stream.creation_timestamp, Eq(Timestamp::Millis(98821855)));
  EXPECT_THAT(stream.ssrc, Eq(2805827407));
  EXPECT_THAT(stream.frames, SizeIs(1412));

  // Spot check the last frame.
  // TODO: b/423646186 - The size values here seem unreasonable, look into this.
  EXPECT_THAT(
      stream.frames.back(),
      EqualsFrame({.num_packets = 16,
                   .size = DataSize::Bytes(17559),
                   .unwrapped_rtp_timestamp = 2213216087,
                   .assembled_timestamp = Timestamp::Millis(98868830),
                   .decodable_timestamp = Timestamp::Millis(98868830)}));
}

TEST(DecodabilitySimulatorTest, VideoRecvSequentialJoinVp8Vp9Av1) {
  std::unique_ptr<ParsedRtcEventLog> parsed_log =
      ParsedRtcEventLogFromResources("video_recv_sequential_join_vp8_vp9_av1");

  DecodabilitySimulator::Config config;
  DecodabilitySimulator simulator(config);
  DecodabilitySimulator::Results results = simulator.Simulate(*parsed_log);

  EXPECT_THAT(results.streams,
              ElementsAre(AllOf(Field(&Stream::ssrc, Eq(2827012235)),
                                Field(&Stream::frames, SizeIs(1746))),
                          AllOf(Field(&Stream::ssrc, Eq(1651489786)),
                                Field(&Stream::frames, SizeIs(1157))),
                          AllOf(Field(&Stream::ssrc, Eq(1934275846)),
                                Field(&Stream::frames, SizeIs(361)))));
}

// This log starts experiencing packet losses after half the duration.
TEST(DecodabilitySimulatorTest, VideoRecvVp8Lossy) {
  std::unique_ptr<ParsedRtcEventLog> parsed_log =
      ParsedRtcEventLogFromResources("video_recv_vp8_pt96_lossy");

  DecodabilitySimulator::Config config;
  DecodabilitySimulator simulator(config);
  DecodabilitySimulator::Results results = simulator.Simulate(*parsed_log);

  ASSERT_THAT(results.streams, SizeIs(1));
  const auto& stream = results.streams.front();
  EXPECT_THAT(stream.creation_timestamp, Eq(Timestamp::Millis(821417933)));
  EXPECT_THAT(stream.ssrc, Eq(4096673911));
  EXPECT_THAT(stream.frames, SizeIs(1145));
  // Number of decodable frames.
  EXPECT_THAT(absl::c_count_if(stream.frames,
                               [](const auto& e) {
                                 return e.ArrivalTimestamp().IsFinite();
                               }),
              Eq(1117));

  // Find the last decodable frame.
  EXPECT_TRUE(absl::c_is_sorted(stream.frames,
                                ArrivalOrder<DecodabilitySimulator::Frame>));
  auto it = stream.frames.rbegin();
  while (it != stream.frames.rend() && !it->ArrivalTimestamp().IsFinite()) {
    ++it;
  }
  ASSERT_THAT(it, Ne(stream.frames.rend()));

  // Spot check the last decodable frame.
  EXPECT_THAT(
      *it, EqualsFrame({.num_packets = 4,
                        .size = DataSize::Bytes(3902),
                        .unwrapped_rtp_timestamp = 2607363343,
                        .assembled_timestamp = Timestamp::Millis(821457158),
                        .decodable_timestamp = Timestamp::Millis(821457158)}));
}

}  // namespace
}  // namespace webrtc::video_timing_simulator
