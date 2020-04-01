/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/congestion_controller/bbr/data_transfer_tracker.h"

#include <stdint.h>

#include "test/gtest.h"

namespace webrtc {
namespace bbr {
namespace test {
namespace {
struct ResultForTest {
  int64_t ack_span_ms;
  int64_t send_span_ms;
  int64_t acked_bytes;
};
class DataTransferTrackerForTest : public DataTransferTracker {
 public:
  void AddSample(int bytes, int send_time_ms, int ack_time_ms) {
    DataTransferTracker::AddSample(DataSize::Bytes(bytes),
                                   Timestamp::Millis(send_time_ms),
                                   Timestamp::Millis(ack_time_ms));
  }

  void ClearOldSamples(int excluding_end_ms) {
    DataTransferTracker::ClearOldSamples(Timestamp::Millis(excluding_end_ms));
  }
  ResultForTest GetRatesByAckTime(int covered_start_ms, int including_end_ms) {
    auto result = DataTransferTracker::GetRatesByAckTime(
        Timestamp::Millis(covered_start_ms),
        Timestamp::Millis(including_end_ms));
    return ResultForTest{result.ack_timespan.ms(), result.send_timespan.ms(),
                         result.acked_data.bytes()};
  }
};

}  // namespace

TEST(DataTransferTrackerTest, TracksData) {
  DataTransferTrackerForTest calc;
  // Since we dont have any previous reference for the first packet, it won't be
  // counted.
  calc.AddSample(5555, 100000, 100100);
  calc.AddSample(1000, 100020, 100120);
  calc.AddSample(1000, 100040, 100140);
  calc.AddSample(1000, 100060, 100160);

  auto result = calc.GetRatesByAckTime(100000, 100200);
  EXPECT_EQ(result.acked_bytes, 3000);
  EXPECT_EQ(result.ack_span_ms, 60);
  EXPECT_EQ(result.send_span_ms, 60);
}

TEST(DataTransferTrackerTest, CoversStartTime) {
  DataTransferTrackerForTest calc;
  calc.AddSample(5555, 100000, 100100);
  calc.AddSample(1000, 100020, 100120);
  calc.AddSample(1000, 100040, 100140);
  calc.AddSample(1000, 100060, 100160);
  calc.AddSample(1000, 100080, 100180);

  auto result = calc.GetRatesByAckTime(100140, 100200);
  EXPECT_EQ(result.acked_bytes, 3000);
  EXPECT_EQ(result.ack_span_ms, 60);
  EXPECT_EQ(result.send_span_ms, 60);
}

TEST(DataTransferTrackerTest, IncludesEndExcludesPastEnd) {
  DataTransferTrackerForTest calc;
  calc.AddSample(5555, 100000, 100100);
  calc.AddSample(1000, 100020, 100120);
  calc.AddSample(1000, 100040, 100140);
  calc.AddSample(1000, 100060, 100160);
  calc.AddSample(1000, 100080, 100180);

  auto result = calc.GetRatesByAckTime(100120, 100160);
  EXPECT_EQ(result.acked_bytes, 3000);
  EXPECT_EQ(result.ack_span_ms, 60);
  EXPECT_EQ(result.send_span_ms, 60);
}

TEST(DataTransferTrackerTest, AccumulatesDuplicates) {
  DataTransferTrackerForTest calc;
  calc.AddSample(5555, 100000, 100100);
  // Two packets at same time, should be accumulated.
  calc.AddSample(1000, 100020, 100120);
  calc.AddSample(1000, 100020, 100120);
  calc.AddSample(1000, 100060, 100160);
  // Two packets at same time, should be accumulated.
  calc.AddSample(1000, 100100, 100200);
  calc.AddSample(1000, 100100, 100200);
  calc.AddSample(1000, 100120, 100220);

  auto result = calc.GetRatesByAckTime(100120, 100200);
  EXPECT_EQ(result.acked_bytes, 5000);
  EXPECT_EQ(result.ack_span_ms, 100);
  EXPECT_EQ(result.send_span_ms, 100);
}

TEST(DataTransferTrackerTest, RemovesOldData) {
  DataTransferTrackerForTest calc;
  calc.AddSample(5555, 100000, 100100);
  calc.AddSample(1000, 100020, 100120);
  calc.AddSample(1000, 100040, 100140);
  calc.AddSample(1000, 100060, 100160);
  calc.AddSample(1000, 100080, 100180);
  {
    auto result = calc.GetRatesByAckTime(100120, 100200);
    EXPECT_EQ(result.acked_bytes, 4000);
    EXPECT_EQ(result.ack_span_ms, 80);
    EXPECT_EQ(result.send_span_ms, 80);
  }
  // Note that this operation means that the packet acked at 100140 will not be
  // counted any more, just used as time reference.
  calc.ClearOldSamples(100140);
  {
    auto result = calc.GetRatesByAckTime(100120, 100200);
    EXPECT_EQ(result.acked_bytes, 2000);
    EXPECT_EQ(result.ack_span_ms, 40);
    EXPECT_EQ(result.send_span_ms, 40);
  }
}
}  // namespace test
}  // namespace bbr
}  // namespace webrtc
