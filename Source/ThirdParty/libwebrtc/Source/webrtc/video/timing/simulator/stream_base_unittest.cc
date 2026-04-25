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

#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "video/timing/simulator/frame_base.h"

namespace webrtc::video_timing_simulator {
namespace {

using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Field;

struct TestFrame : public FrameBase<TestFrame> {
  int64_t unwrapped_rtp_timestamp = -1;
  Timestamp assembled_timestamp = Timestamp::PlusInfinity();
  Timestamp ArrivalTimestampInternal() const { return assembled_timestamp; }
  TimeDelta frame_delay_variation = TimeDelta::PlusInfinity();
};

struct TestStream : public StreamBase<TestStream> {
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
      ElementsAreArray({
          Field(&TestFrame::frame_delay_variation, Eq(TimeDelta::Zero())),
          Field(&TestFrame::frame_delay_variation, Eq(TimeDelta::Micros(1000))),
          // Due to the non-integer 1000/90 factor in the timestamp
          // translation, we get a 33332us here instead of 33333us.
          Field(&TestFrame::frame_delay_variation,
                Eq(TimeDelta::Micros(33332))),
          Field(&TestFrame::frame_delay_variation, Eq(TimeDelta::Zero())),
      }));
}

}  // namespace
}  // namespace webrtc::video_timing_simulator
