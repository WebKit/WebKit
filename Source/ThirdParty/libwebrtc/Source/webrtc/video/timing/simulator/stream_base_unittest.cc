/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/simulator/stream_base.h"

#include <cstdint>
#include <vector>

#include "api/numerics/samples_stats_counter.h"
#include "api/units/data_size.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "video/timing/simulator/frame_base.h"

namespace webrtc::video_timing_simulator {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;

struct TestFrame : public FrameBase<TestFrame> {
  int num_packets = -1;
  DataSize size = DataSize::Zero();
  int64_t unwrapped_rtp_timestamp = -1;
  Timestamp assembled_timestamp = Timestamp::PlusInfinity();
  Timestamp ArrivalTimestampInternal() const { return assembled_timestamp; }
  TimeDelta frame_delay_variation = TimeDelta::PlusInfinity();
};

struct TestStream : public StreamBase<TestStream, TestFrame> {
  Timestamp creation_timestamp = Timestamp::PlusInfinity();
  uint32_t ssrc = 0;
  std::vector<TestFrame> frames;
};

TEST(StreamBaseTest, IsEmpty) {
  TestStream stream;
  EXPECT_TRUE(stream.IsEmpty());
}

TEST(StreamBaseTest, PopulateFrameDelayVariations) {
  // Four frames at 30fps => 3000 RTP ticks between sent frames.
  // Nominal inter-arrival-time is 33333us.

  // First frame becomes the initial baseline.
  TestFrame frame1{.unwrapped_rtp_timestamp = 3000,
                   .assembled_timestamp = Timestamp::Micros(33333)};
  // Second frame is delayed 1000us.
  TestFrame frame2{.unwrapped_rtp_timestamp = 6000,
                   .assembled_timestamp = Timestamp::Micros(66666 + 1000)};
  // Third frame is severely delayed, arriving back-to-back with the 4th frame.
  TestFrame frame3{.unwrapped_rtp_timestamp = 9000,
                   .assembled_timestamp = Timestamp::Micros(99999 + 33333)};
  // The 4th frame arrives on time.
  TestFrame frame4{.unwrapped_rtp_timestamp = 12000,
                   .assembled_timestamp = Timestamp::Micros(133332)};

  TestStream stream{.frames = {frame1, frame2, frame3, frame4}};
  stream.PopulateFrameDelayVariations();

  EXPECT_THAT(
      stream.frames,
      ElementsAre(
          Field(&TestFrame::frame_delay_variation, Eq(TimeDelta::Zero())),
          Field(&TestFrame::frame_delay_variation, Eq(TimeDelta::Micros(1000))),
          // Due to the non-integer 1000/90 factor in the timestamp
          // translation, we get a 33332us here instead of 33333us.
          Field(&TestFrame::frame_delay_variation,
                Eq(TimeDelta::Micros(33332))),
          Field(&TestFrame::frame_delay_variation, Eq(TimeDelta::Zero()))));
}

TEST(StreamBaseTest, DepartureDuration) {
  TestStream stream{.frames = {{.unwrapped_rtp_timestamp = 3000},
                               {.unwrapped_rtp_timestamp = 6000}}};

  EXPECT_EQ(stream.DepartureDuration(), TimeDelta::Micros(33333));
}

TEST(StreamBaseTest, ArrivalDuration) {
  TestStream stream{
      .frames = {{.assembled_timestamp = Timestamp::Micros(33333)},
                 {.assembled_timestamp = Timestamp::Micros(66666 + 1000)}}};

  EXPECT_EQ(stream.ArrivalDuration(), TimeDelta::Micros(34333));
}

TEST(StreamBaseTest, NumAssembledFrames) {
  TestStream stream{
      .frames = {{.assembled_timestamp = Timestamp::Micros(33333)},
                 {.assembled_timestamp = Timestamp::Micros(66666 + 1000)}}};

  EXPECT_EQ(stream.NumAssembledFrames(), 2);
}

TEST(StreamBaseTest, NumPackets) {
  TestStream stream{.frames = {{.num_packets = 1}, {.num_packets = 2}}};

  EXPECT_THAT(
      stream.NumPackets().GetTimedSamples(),
      ElementsAre(Field(&SamplesStatsCounter::StatsSample::value, Eq(1)),
                  Field(&SamplesStatsCounter::StatsSample::value, Eq(2))));
}

TEST(StreamBaseTest, SizeBytes) {
  TestStream stream{
      .frames = {{.size = DataSize::Bytes(10)}, {.size = DataSize::Bytes(20)}}};

  EXPECT_THAT(
      stream.SizeBytes().GetTimedSamples(),
      ElementsAre(Field(&SamplesStatsCounter::StatsSample::value, Eq(10)),
                  Field(&SamplesStatsCounter::StatsSample::value, Eq(20))));
}

TEST(StreamBaseTest, FrameDelayVariationMs) {
  TestStream stream{
      .frames = {{.frame_delay_variation = TimeDelta::Millis(10)},
                 {.frame_delay_variation = TimeDelta::Millis(20)}}};

  EXPECT_THAT(
      stream.FrameDelayVariationMs().GetTimedSamples(),
      ElementsAre(Field(&SamplesStatsCounter::StatsSample::value, Eq(10)),
                  Field(&SamplesStatsCounter::StatsSample::value, Eq(20))));
}

TEST(StreamBaseTest, InterDepartureTimeMs) {
  TestStream stream{.frames = {{.unwrapped_rtp_timestamp = 3000},
                               {.unwrapped_rtp_timestamp = 6000}}};

  EXPECT_THAT(
      stream.InterDepartureTimeMs().GetTimedSamples(),
      ElementsAre(Field(&SamplesStatsCounter::StatsSample::value, Eq(33.333))));
}

TEST(StreamBaseTest, InterArrivalTimeMs) {
  TestStream stream{
      .frames = {{.assembled_timestamp = Timestamp::Micros(33333)},
                 {.assembled_timestamp = Timestamp::Micros(66666 + 1000)}}};

  EXPECT_THAT(
      stream.InterArrivalTimeMs().GetTimedSamples(),
      ElementsAre(Field(&SamplesStatsCounter::StatsSample::value, Eq(34.333))));
}

TEST(StreamBaseTest, InterFrameDelayVariationMs) {
  TestStream stream{
      .frames = {{.unwrapped_rtp_timestamp = 3000,
                  .assembled_timestamp = Timestamp::Micros(33333)},
                 {.unwrapped_rtp_timestamp = 6000,
                  .assembled_timestamp = Timestamp::Micros(66666 + 1000)}}};

  EXPECT_THAT(
      stream.InterFrameDelayVariationMs().GetTimedSamples(),
      ElementsAre(Field(&SamplesStatsCounter::StatsSample::value, Eq(1.0))));
}

TEST(StreamBaseTest, InterAssembledTimeMs) {
  TestStream stream{
      .frames = {{.assembled_timestamp = Timestamp::Micros(33333)},
                 {.assembled_timestamp = Timestamp::Micros(66666 + 1000)}}};

  EXPECT_THAT(
      stream.InterAssembledTimeMs().GetTimedSamples(),
      ElementsAre(Field(&SamplesStatsCounter::StatsSample::value, Eq(34.333))));
}
}  // namespace
}  // namespace webrtc::video_timing_simulator
