/*
 *  Copyright (c) 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/simulator/rendering_simulator.h"

#include <memory>
#include <optional>

#include "absl/algorithm/container.h"
#include "absl/strings/string_view.h"
#include "api/numerics/samples_stats_counter.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "video/timing/simulator/frame_base.h"
#include "video/timing/simulator/test/parsed_rtc_event_log_from_resources.h"

namespace webrtc::video_timing_simulator {
namespace {

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::Ne;
using ::testing::Optional;
using ::testing::SizeIs;

using Frame = RenderingSimulator::Frame;
using Stream = RenderingSimulator::Stream;

TEST(RenderingSimulatorFrameTest, BufferDurationsAndMarginsForInTimeFrame) {
  RenderingSimulator::Frame frame{
      .first_packet_arrival_timestamp = Timestamp::Zero(),
      .assembled_timestamp = Timestamp::Millis(1),
      .render_timestamp =
          Timestamp::Millis(10) + RenderingSimulator::kRenderDelay,
      .decoded_timestamp = Timestamp::Millis(9),
      .rendered_timestamp = Timestamp::Millis(10)};

  // Buffer durations.
  EXPECT_THAT(frame.PacketBufferDuration(), Eq(TimeDelta::Millis(1)));
  EXPECT_THAT(frame.FrameBufferDuration(), Eq(TimeDelta::Millis(8)));
  EXPECT_THAT(frame.RenderBufferDuration(), Eq(TimeDelta::Millis(1)));
  EXPECT_THAT(frame.TotalBufferDuration(), Eq(TimeDelta::Millis(10)));

  // Margins.
  EXPECT_THAT(frame.AssembledMargin(), Eq(TimeDelta::Millis(9)));
  EXPECT_THAT(frame.AssembledInTime(), Optional(true));
  EXPECT_THAT(frame.AssembledLate(), Optional(false));
  EXPECT_THAT(frame.AssembledMarginExcess(), Optional(TimeDelta::Millis(9)));
  EXPECT_THAT(frame.AssembledMarginDeficit(), Eq(std::nullopt));
  EXPECT_THAT(frame.RenderedMargin(), Eq(TimeDelta::Zero()));
  EXPECT_THAT(frame.RenderedInTime(), Optional(true));
  EXPECT_THAT(frame.RenderedLate(), Optional(false));
  EXPECT_THAT(frame.RenderedMarginExcess(), Optional(TimeDelta::Zero()));
  EXPECT_THAT(frame.RenderedMarginDeficit(), Eq(std::nullopt));
}

TEST(RenderingSimulatorFrameTest, BufferDurationsAndMarginsForDelayedFrame) {
  RenderingSimulator::Frame frame{
      .first_packet_arrival_timestamp = Timestamp::Millis(12),
      .assembled_timestamp = Timestamp::Millis(12),
      .render_timestamp =
          Timestamp::Millis(10) + RenderingSimulator::kRenderDelay,
      .decoded_timestamp = Timestamp::Millis(12),
      .rendered_timestamp = Timestamp::Millis(12)};

  // Buffer durations.
  EXPECT_THAT(frame.PacketBufferDuration(), Eq(TimeDelta::Zero()));
  EXPECT_THAT(frame.FrameBufferDuration(), Eq(TimeDelta::Zero()));
  EXPECT_THAT(frame.RenderBufferDuration(), Eq(TimeDelta::Zero()));
  EXPECT_THAT(frame.TotalBufferDuration(), Eq(TimeDelta::Zero()));

  // Margins.
  EXPECT_THAT(frame.AssembledMargin(), Eq(TimeDelta::Millis(-2)));
  EXPECT_THAT(frame.AssembledInTime(), Optional(false));
  EXPECT_THAT(frame.AssembledLate(), Optional(true));
  EXPECT_THAT(frame.AssembledMarginExcess(), Eq(std::nullopt));
  EXPECT_THAT(frame.AssembledMarginDeficit(), Optional(TimeDelta::Millis(-2)));
  EXPECT_THAT(frame.RenderedMargin(), Eq(TimeDelta::Millis(-2)));
  EXPECT_THAT(frame.AssembledInTime(), Optional(false));
  EXPECT_THAT(frame.AssembledLate(), Optional(true));
  EXPECT_THAT(frame.RenderedMarginExcess(), Eq(std::nullopt));
  EXPECT_THAT(frame.RenderedMarginDeficit(), Optional(TimeDelta::Millis(-2)));
}

TEST(RenderingSimulatorFrameTest,
     InTimeHelpersReturnTrueWhenRenderTimestampRoundedDownToMsPrecision) {
  Timestamp processing_timestamp = Timestamp::Micros(10499);

  RenderingSimulator::Frame frame{.assembled_timestamp = processing_timestamp,
                                  .rendered_timestamp = processing_timestamp};
  // Simulate the rounding down in `SetRenderTime(render_time.ms());`
  frame.render_timestamp = Timestamp::Millis(processing_timestamp.ms()) +
                           RenderingSimulator::kRenderDelay;
  EXPECT_EQ(frame.render_timestamp,
            Timestamp::Millis(10) + RenderingSimulator::kRenderDelay);

  // Margins.
  EXPECT_EQ(frame.AssembledMargin(), TimeDelta::Micros(-499));
  EXPECT_TRUE(frame.AssembledInTime());
  EXPECT_EQ(frame.RenderedMargin(), TimeDelta::Micros(-499));
  EXPECT_TRUE(frame.RenderedInTime());
}

TEST(RenderingSimulatorFrameTest,
     InTimeHelpersReturnTrueWhenRenderTimestampRoundedUpToMsPrecision) {
  Timestamp processing_timestamp = Timestamp::Micros(10500);

  RenderingSimulator::Frame frame{.assembled_timestamp = processing_timestamp,
                                  .rendered_timestamp = processing_timestamp};
  // Simulate the rounding up in `SetRenderTime(render_time.ms());`
  frame.render_timestamp = Timestamp::Millis(processing_timestamp.ms()) +
                           RenderingSimulator::kRenderDelay;
  EXPECT_EQ(frame.render_timestamp,
            Timestamp::Millis(11) + RenderingSimulator::kRenderDelay);

  // Margins.
  EXPECT_EQ(frame.AssembledMargin(), TimeDelta::Micros(500));
  EXPECT_TRUE(frame.AssembledInTime());
  EXPECT_EQ(frame.RenderedMargin(), TimeDelta::Micros(500));
  EXPECT_TRUE(frame.RenderedInTime());
}

TEST(RenderingSimulatorStreamTest, NumAssembledInTimeAndLateFrames) {
  RenderingSimulator::Stream stream{
      .frames = {{.assembled_timestamp = Timestamp::Millis(0)},
                 {.assembled_timestamp = Timestamp::Millis(10),
                  .render_timestamp =
                      Timestamp::Millis(10) + RenderingSimulator::kRenderDelay},
                 {.assembled_timestamp = Timestamp::Millis(30),
                  .render_timestamp = Timestamp::Millis(20) +
                                      RenderingSimulator::kRenderDelay}}};

  EXPECT_EQ(stream.NumAssembledFrames(), 3);
  EXPECT_EQ(stream.NumAssembledInTimeFrames(), 1);
  EXPECT_EQ(stream.NumAssembledLateFrames(), 1);
}

TEST(RenderingSimulatorStreamTest, NumDecodedFrames) {
  RenderingSimulator::Stream stream{
      .frames = {{}, {.decoded_timestamp = Timestamp::Millis(10)}}};

  EXPECT_EQ(stream.NumDecodedFrames(), 1);
}

TEST(RenderingSimulatorStreamTest, NumRenderedInTimeAndLateFrames) {
  RenderingSimulator::Stream stream{
      .frames = {{},
                 {.render_timestamp =
                      Timestamp::Millis(10) + RenderingSimulator::kRenderDelay,
                  .rendered_timestamp = Timestamp::Millis(10)},
                 {.render_timestamp =
                      Timestamp::Millis(20) + RenderingSimulator::kRenderDelay,
                  .rendered_timestamp = Timestamp::Millis(30)}}};

  EXPECT_EQ(stream.NumRenderedFrames(), 2);
  EXPECT_EQ(stream.NumRenderedInTimeFrames(), 1);
  EXPECT_EQ(stream.NumRenderedLateFrames(), 1);
}

TEST(RenderingSimulatorStreamTest, NumDecoderDroppedFrames) {
  RenderingSimulator::Stream stream{
      .frames = {{.frames_dropped = 1}, {.frames_dropped = 2}}};

  EXPECT_EQ(stream.NumDecoderDroppedFrames(), 3);
}

TEST(RenderingSimulatorStreamTest, InterFrameDurations) {
  RenderingSimulator::Stream stream{
      .frames = {{.render_timestamp = Timestamp::Millis(1),
                  .decoded_timestamp = Timestamp::Millis(2),
                  .rendered_timestamp = Timestamp::Millis(3)},
                 {.render_timestamp = Timestamp::Millis(11),
                  .decoded_timestamp = Timestamp::Millis(102),
                  .rendered_timestamp = Timestamp::Millis(1003)}}};

  EXPECT_THAT(
      stream.InterRenderTimeMs().GetTimedSamples(),
      ElementsAre(Field(&SamplesStatsCounter::StatsSample::value, Eq(10))));
  EXPECT_THAT(
      stream.InterDecodedTimeMs().GetTimedSamples(),
      ElementsAre(Field(&SamplesStatsCounter::StatsSample::value, Eq(100))));
  EXPECT_THAT(
      stream.InterRenderedTimeMs().GetTimedSamples(),
      ElementsAre(Field(&SamplesStatsCounter::StatsSample::value, Eq(1000))));
}

TEST(RenderingSimulatorStreamTest, WebrtcStatsSamples) {
  RenderingSimulator::Stream stream{
      .frames = {{.rendered_timestamp = Timestamp::Zero(),
                  .jitter_buffer_minimum_delay = TimeDelta::Millis(20),
                  .jitter_buffer_delay = TimeDelta::Millis(30)}}};

  EXPECT_THAT(
      stream.JitterBufferMinimumDelayMs().GetTimedSamples(),
      ElementsAre(Field(&SamplesStatsCounter::StatsSample::value, Eq(20))));
  EXPECT_THAT(
      stream.JitterBufferDelayMs().GetTimedSamples(),
      ElementsAre(Field(&SamplesStatsCounter::StatsSample::value, Eq(30))));
}

TEST(RenderingSimulatorStreamTest, BufferDurations) {
  RenderingSimulator::Stream stream{
      .frames = {{.first_packet_arrival_timestamp = Timestamp::Millis(1),
                  .assembled_timestamp = Timestamp::Millis(3),
                  .decoded_timestamp = Timestamp::Millis(7),
                  .rendered_timestamp = Timestamp::Millis(14)}}};

  EXPECT_THAT(
      stream.PacketBufferDurationMs().GetTimedSamples(),
      ElementsAre(Field(&SamplesStatsCounter::StatsSample::value, Eq(2))));
  EXPECT_THAT(
      stream.FrameBufferDurationMs().GetTimedSamples(),
      ElementsAre(Field(&SamplesStatsCounter::StatsSample::value, Eq(4))));
  EXPECT_THAT(
      stream.RenderBufferDurationMs().GetTimedSamples(),
      ElementsAre(Field(&SamplesStatsCounter::StatsSample::value, Eq(7))));
  EXPECT_THAT(
      stream.TotalBufferDurationMs().GetTimedSamples(),
      ElementsAre(Field(&SamplesStatsCounter::StatsSample::value, Eq(13))));
}

TEST(RenderingSimulatorStreamTest, Margins) {
  RenderingSimulator::Stream stream{
      .frames = {{.assembled_timestamp = Timestamp::Millis(0),
                  .render_timestamp =
                      Timestamp::Millis(10) + RenderingSimulator::kRenderDelay,
                  .rendered_timestamp = Timestamp::Millis(15)}}};

  EXPECT_THAT(
      stream.AssembledMarginMs().GetTimedSamples(),
      ElementsAre(Field(&SamplesStatsCounter::StatsSample::value, Eq(10))));
  EXPECT_THAT(
      stream.AssembledMarginExcessMs().GetTimedSamples(),
      ElementsAre(Field(&SamplesStatsCounter::StatsSample::value, Eq(10))));
  EXPECT_THAT(stream.AssembledMarginDeficitMs().GetTimedSamples(), IsEmpty());

  EXPECT_THAT(
      stream.RenderedMarginMs().GetTimedSamples(),
      ElementsAre(Field(&SamplesStatsCounter::StatsSample::value, Eq(-5))));
  EXPECT_THAT(stream.RenderedMarginExcessMs().GetTimedSamples(), IsEmpty());
  EXPECT_THAT(
      stream.RenderedMarginDeficitMs().GetTimedSamples(),
      ElementsAre(Field(&SamplesStatsCounter::StatsSample::value, Eq(-5))));
}

Matcher<const Frame&> EqualsFrame(const Frame& expected) {
  return AllOf(
      Field("num_packets", &Frame::num_packets, Eq(expected.num_packets)),
      Field("size", &Frame::size, Eq(expected.size)),

      Field("payload_type", &Frame::payload_type, Eq(expected.payload_type)),
      Field("rtp_timestamp", &Frame::rtp_timestamp, Eq(expected.rtp_timestamp)),
      Field("unwrapped_rtp_timestamp", &Frame::unwrapped_rtp_timestamp,
            Eq(expected.unwrapped_rtp_timestamp)),

      Field("frame_id", &Frame::frame_id, Eq(expected.frame_id)),
      Field("spatial_id", &Frame::spatial_id, Eq(expected.spatial_id)),
      Field("temporal_id", &Frame::temporal_id, Eq(expected.temporal_id)),
      Field("num_references", &Frame::num_references,
            Eq(expected.num_references)),

      Field("first_packet_arrival_timestamp",
            &Frame::first_packet_arrival_timestamp,
            Eq(expected.first_packet_arrival_timestamp)),
      Field("last_packet_arrival_timestamp",
            &Frame::last_packet_arrival_timestamp,
            Eq(expected.last_packet_arrival_timestamp)),

      Field("assembled_timestamp", &Frame::assembled_timestamp,
            Eq(expected.assembled_timestamp)),
      Field("render_timestamp", &Frame::render_timestamp,
            Eq(expected.render_timestamp)),
      Field("decoded_timestamp", &Frame::decoded_timestamp,
            Eq(expected.decoded_timestamp)),
      Field("rendered_timestamp", &Frame::rendered_timestamp,
            Eq(expected.rendered_timestamp)),

      Field("frames_dropped", &Frame::frames_dropped,
            Eq(expected.frames_dropped)),
      Field("jitter_buffer_minimum_delay", &Frame::jitter_buffer_minimum_delay,
            Eq(expected.jitter_buffer_minimum_delay)),
      Field("jitter_buffer_target_delay", &Frame::jitter_buffer_target_delay,
            Eq(expected.jitter_buffer_target_delay)),
      Field("jitter_buffer_delay", &Frame::jitter_buffer_delay,
            Eq(expected.jitter_buffer_delay)));
}

TEST(RenderingSimulatorTest, VideoRecvVp8) {
  std::unique_ptr<ParsedRtcEventLog> parsed_log =
      ParsedRtcEventLogFromResources("video_recv_vp8_pt96");

  RenderingSimulator::Config config{.name = "vp8"};
  RenderingSimulator simulator(config);
  RenderingSimulator::Results results = simulator.Simulate(*parsed_log);

  EXPECT_THAT(results.config_name, Eq("vp8"));
  ASSERT_THAT(results.streams, SizeIs(1));
  const auto& stream = results.streams.front();
  EXPECT_THAT(stream.creation_timestamp, Eq(Timestamp::Millis(100942625)));
  EXPECT_THAT(stream.ssrc, Eq(3965119250));
  EXPECT_THAT(stream.frames, SizeIs(650));

  // Spot check the second to last frame.
  // (The last frame is actually non-decodable due to a VideoReceiveStream2
  // recreation in the log.)
  const auto& last_frame = stream.frames[stream.frames.size() - 2];
  EXPECT_THAT(
      last_frame,
      EqualsFrame(
          {// Frame information.
           .num_packets = 7,
           .size = DataSize::Bytes(7669),
           // RTP header information.
           .payload_type = 96,
           .rtp_timestamp = 2498233591,
           .unwrapped_rtp_timestamp = 2498233591,
           // Dependency descriptor information.
           .frame_id = 648,
           .spatial_id = 0,
           .temporal_id = 0,
           .num_references = 1,
           // Packet timestamps.
           .first_packet_arrival_timestamp = Timestamp::Millis(100964151),
           .last_packet_arrival_timestamp = Timestamp::Millis(100964166),
           // Frame timestamps.
           .assembled_timestamp = Timestamp::Millis(100964166),
           .render_timestamp = Timestamp::Millis(100964193),
           .decoded_timestamp = Timestamp::Micros(100964182726),
           .rendered_timestamp = Timestamp::Micros(100964182726),
           // Jitter buffer information.
           .frames_dropped = 0,
           .jitter_buffer_minimum_delay = TimeDelta::Micros(18861),
           .jitter_buffer_target_delay = TimeDelta::Micros(18861),
           .jitter_buffer_delay = TimeDelta::Micros(31726)}));
}

TEST(RenderingSimulatorTest, VideoRecvVp9) {
  std::unique_ptr<ParsedRtcEventLog> parsed_log =
      ParsedRtcEventLogFromResources("video_recv_vp9_pt98");

  RenderingSimulator::Config config{.name = "vp9"};
  RenderingSimulator simulator(config);
  RenderingSimulator::Results results = simulator.Simulate(*parsed_log);

  EXPECT_THAT(results.config_name, Eq("vp9"));
  ASSERT_THAT(results.streams, SizeIs(1));
  const auto& stream = results.streams.front();
  EXPECT_THAT(stream.ssrc, Eq(2849747025));
  EXPECT_THAT(stream.frames, SizeIs(1493));

  // Spot check the last frame.
  const auto& last_frame = stream.frames.back();
  EXPECT_THAT(
      last_frame,
      EqualsFrame(
          {// Frame information.
           .num_packets = 6,
           .size = DataSize::Bytes(6265),
           // RTP header information.
           .payload_type = 98,
           .rtp_timestamp = 2236817278,
           .unwrapped_rtp_timestamp = 2236817278,
           // Dependency descriptor information.
           .frame_id = 1493,
           .spatial_id = 0,
           .temporal_id = 1,
           .num_references = 1,
           // Packet timestamps.
           .first_packet_arrival_timestamp = Timestamp::Millis(98768274),
           .last_packet_arrival_timestamp = Timestamp::Millis(98768284),
           // Frame timestamps.
           .assembled_timestamp = Timestamp::Millis(98768284),
           .render_timestamp = Timestamp::Millis(98768325),
           .decoded_timestamp = Timestamp::Micros(98768315253),
           .rendered_timestamp = Timestamp::Micros(98768315253),
           // Jitter buffer state.
           .frames_dropped = 0,
           .jitter_buffer_minimum_delay = TimeDelta::Micros(26604),
           .jitter_buffer_target_delay = TimeDelta::Micros(26604),
           .jitter_buffer_delay = TimeDelta::Micros(41253)}));
}

TEST(RenderingSimulatorTest, VideoRecvAv1) {
  std::unique_ptr<ParsedRtcEventLog> parsed_log =
      ParsedRtcEventLogFromResources("video_recv_av1_pt45");

  RenderingSimulator::Config config{.name = "av1"};
  RenderingSimulator simulator(config);
  RenderingSimulator::Results results = simulator.Simulate(*parsed_log);

  EXPECT_THAT(results.config_name, Eq("av1"));
  ASSERT_THAT(results.streams, SizeIs(1));
  const auto& stream = results.streams.front();
  EXPECT_THAT(stream.ssrc, Eq(2805827407));
  EXPECT_THAT(stream.frames, SizeIs(1412));

  // Spot check the second to last frame.
  // (The last frame is actually non-decodable due to a VideoReceiveStream2
  // recreation in the log.)
  const auto& last_frame = stream.frames[stream.frames.size() - 2];
  EXPECT_THAT(
      last_frame,
      EqualsFrame(
          {// Frame information.
           .num_packets = 11,
           .size = DataSize::Bytes(12705),
           // RTP header information.
           .payload_type = 45,
           .rtp_timestamp = 2213213027,
           .unwrapped_rtp_timestamp = 2213213027,
           // Dependency descriptor information.
           .frame_id = 1410,
           .spatial_id = 0,
           .temporal_id = 0,
           .num_references = 1,
           // Packet timestamps.
           .first_packet_arrival_timestamp = Timestamp::Millis(98868775),
           .last_packet_arrival_timestamp = Timestamp::Millis(98868790),
           // Frame timestamps.
           .assembled_timestamp = Timestamp::Millis(98868790),
           .render_timestamp = Timestamp::Millis(98868822),
           .decoded_timestamp = Timestamp::Micros(98868811530),
           .rendered_timestamp = Timestamp::Micros(98868811530),
           // Jitter buffer state.
           .frames_dropped = 0,
           .jitter_buffer_minimum_delay = TimeDelta::Micros(23864),
           .jitter_buffer_target_delay = TimeDelta::Micros(23864),
           .jitter_buffer_delay = TimeDelta::Micros(36530)}));
}

TEST(RenderingSimulatorTest, VideoRecvSequentialJoinVp8Vp9Av1) {
  std::unique_ptr<ParsedRtcEventLog> parsed_log =
      ParsedRtcEventLogFromResources("video_recv_sequential_join_vp8_vp9_av1");

  RenderingSimulator::Config config;
  RenderingSimulator simulator(config);
  RenderingSimulator::Results results = simulator.Simulate(*parsed_log);

  EXPECT_THAT(results.streams,
              ElementsAre(AllOf(Field(&Stream::ssrc, Eq(2827012235)),
                                Field(&Stream::frames, SizeIs(1746))),
                          AllOf(Field(&Stream::ssrc, Eq(1651489786)),
                                Field(&Stream::frames, SizeIs(1157))),
                          AllOf(Field(&Stream::ssrc, Eq(1934275846)),
                                Field(&Stream::frames, SizeIs(361)))));
}

// This log starts experiencing packet losses after half the duration.
TEST(RenderingSimulatorTest, VideoRecvVp8Lossy) {
  std::unique_ptr<ParsedRtcEventLog> parsed_log =
      ParsedRtcEventLogFromResources("video_recv_vp8_pt96_lossy");

  RenderingSimulator::Config config{.name = "vp8"};
  RenderingSimulator simulator(config);
  RenderingSimulator::Results results = simulator.Simulate(*parsed_log);

  EXPECT_THAT(results.config_name, Eq("vp8"));
  ASSERT_THAT(results.streams, SizeIs(1));
  const auto& stream = results.streams.front();
  EXPECT_THAT(stream.creation_timestamp, Eq(Timestamp::Millis(821417933)));
  EXPECT_THAT(stream.ssrc, Eq(4096673911));
  EXPECT_THAT(stream.frames, SizeIs(1145));
  // Number of rendered frames.
  EXPECT_THAT(absl::c_count_if(stream.frames,
                               [](const auto& e) {
                                 return e.ArrivalTimestamp().IsFinite();
                               }),
              Eq(960));

  // Find the last rendered frame.
  EXPECT_TRUE(absl::c_is_sorted(stream.frames,
                                ArrivalOrder<RenderingSimulator::Frame>));
  auto it = stream.frames.rbegin();
  while (it != stream.frames.rend() && !it->ArrivalTimestamp().IsFinite()) {
    ++it;
  }
  ASSERT_THAT(it, Ne(stream.frames.rend()));

  // Spot check the last rendered frame.
  EXPECT_THAT(
      *it,
      EqualsFrame(
          {// Frame information.
           .num_packets = 4,
           .size = DataSize::Bytes(3902),
           // RTP header information.
           .payload_type = 96,
           .rtp_timestamp = 2607363343,
           .unwrapped_rtp_timestamp = 2607363343,
           // Dependency descriptor information.
           .frame_id = 1165,
           .spatial_id = 0,
           .temporal_id = 1,
           .num_references = 1,
           // Packet timestamps.
           .first_packet_arrival_timestamp = Timestamp::Millis(821457147),
           .last_packet_arrival_timestamp = Timestamp::Millis(821457158),
           // Frame timestamps.
           .assembled_timestamp = Timestamp::Millis(821457158),
           .render_timestamp = Timestamp::Millis(821457178),
           .decoded_timestamp = Timestamp::Micros(821457168065),
           .rendered_timestamp = Timestamp::Micros(821457168065),
           // Jitter buffer information.
           .frames_dropped = 0,
           // The value below is unreasonably low, we should fix this.
           .jitter_buffer_minimum_delay = TimeDelta::Micros(13085),
           .jitter_buffer_target_delay = TimeDelta::Micros(13085),
           .jitter_buffer_delay = TimeDelta::Micros(21065)}));
}

}  // namespace
}  // namespace webrtc::video_timing_simulator
