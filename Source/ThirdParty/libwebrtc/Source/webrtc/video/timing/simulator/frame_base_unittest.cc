/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/simulator/frame_base.h"

#include <cstdint>
#include <vector>

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "test/gtest.h"

namespace webrtc::video_timing_simulator {
namespace {

constexpr int64_t kMicrosPerMillis = 1000;
constexpr int64_t kRtpVideoTicksPerMillis = 90;

struct TestFrame : public FrameBase<TestFrame> {
  int64_t unwrapped_rtp_timestamp = -1;
  Timestamp assembled_timestamp = Timestamp::PlusInfinity();
  Timestamp ArrivalTimestampInternal() const { return assembled_timestamp; }
};

TEST(FrameBaseTest, DepartureTimestampIsInvalidForUnsetRtpTimestamp) {
  TestFrame frame;
  EXPECT_FALSE(frame.DepartureTimestamp().IsFinite());
}

TEST(FrameBaseTest, DepartureTimestamp) {
  TestFrame frame{.unwrapped_rtp_timestamp = 3000};
  EXPECT_EQ(frame.DepartureTimestamp(), Timestamp::Micros(33333));
}

TEST(FrameBaseTest, DepartureTimestampWithOffset) {
  int64_t rtp_timestamp_offset = 123456789;
  Timestamp departure_timestamp_offset = Timestamp::Micros(
      (rtp_timestamp_offset * kMicrosPerMillis) / kRtpVideoTicksPerMillis);
  TestFrame frame{.unwrapped_rtp_timestamp = rtp_timestamp_offset + 3000};
  EXPECT_EQ(frame.DepartureTimestamp(departure_timestamp_offset),
            Timestamp::Micros(33333));
}

TEST(FrameBaseTest, ArrivalTimestampIsInvalidForUnsetRtpTimestamp) {
  TestFrame frame;
  EXPECT_FALSE(frame.ArrivalTimestamp().IsFinite());
}

TEST(FrameBaseTest, ArrivalTimestamp) {
  TestFrame frame{.assembled_timestamp = Timestamp::Micros(33333)};
  EXPECT_EQ(frame.ArrivalTimestamp(), Timestamp::Micros(33333));
}

TEST(FrameBaseTest, ArrivalTimestampWithOffset) {
  Timestamp arrival_timestamp_offset = Timestamp::Seconds(123456789);
  TestFrame frame{.assembled_timestamp =
                      Timestamp::Micros(arrival_timestamp_offset.us() + 33333)};
  EXPECT_EQ(frame.ArrivalTimestamp(arrival_timestamp_offset),
            Timestamp::Micros(33333));
}

TEST(FrameBaseTest, OneWayDelayWithZeroOffsets) {
  TestFrame frame1{.unwrapped_rtp_timestamp = 3000,
                   .assembled_timestamp = Timestamp::Micros(33333)};
  EXPECT_EQ(frame1.OneWayDelay(
                /*arrival_offset=*/Timestamp::Zero(),
                /*departure_offset=*/Timestamp::Zero()),
            TimeDelta::Zero());

  // Delayed 1000us relative to its nominal arrival time.
  TestFrame frame2{.unwrapped_rtp_timestamp = 6000,
                   .assembled_timestamp = Timestamp::Micros(67666)};
  EXPECT_EQ(frame2.OneWayDelay(
                /*arrival_offset=*/Timestamp::Zero(),
                /*departure_offset=*/Timestamp::Zero()),
            TimeDelta::Micros(1000));
}

TEST(FrameBaseTest, OneWayDelayWithOffsets) {
  int64_t rtp_timestamp_offset = 123456789;
  Timestamp departure_timestamp_offset = Timestamp::Micros(
      (rtp_timestamp_offset * kMicrosPerMillis) / kRtpVideoTicksPerMillis);
  Timestamp arrival_timestamp_offset = Timestamp::Seconds(123456789);

  TestFrame frame1{.unwrapped_rtp_timestamp = rtp_timestamp_offset + 3000,
                   .assembled_timestamp = Timestamp::Micros(
                       arrival_timestamp_offset.us() + 33333)};
  EXPECT_EQ(
      frame1.OneWayDelay(arrival_timestamp_offset, departure_timestamp_offset),
      TimeDelta::Zero());

  // Delayed 1000us relative to its nominal arrival time.
  TestFrame frame2{.unwrapped_rtp_timestamp = rtp_timestamp_offset + 6000,
                   .assembled_timestamp = Timestamp::Micros(
                       arrival_timestamp_offset.us() + 67666)};
  EXPECT_EQ(
      frame2.OneWayDelay(arrival_timestamp_offset, departure_timestamp_offset),
      TimeDelta::Micros(1000));
}

TEST(FrameBaseTest, SortingTemplatesCompile) {
  std::vector<TestFrame> test_frames = {
      TestFrame{.unwrapped_rtp_timestamp = 3000}};
  SortByDepartureOrder(test_frames);
  SortByArrivalOrder(test_frames);
  SortByAssembledOrder(test_frames);
}

}  // namespace
}  // namespace webrtc::video_timing_simulator
