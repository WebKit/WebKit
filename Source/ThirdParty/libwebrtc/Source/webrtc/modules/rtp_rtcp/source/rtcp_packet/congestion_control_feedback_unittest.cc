/*
 *  Copyright (c) 2024 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/rtp_rtcp/source/rtcp_packet/congestion_control_feedback.h"

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "api/function_view.h"
#include "api/transport/ecn_marking.h"
#include "api/units/time_delta.h"
#include "modules/rtp_rtcp/source/rtcp_packet/common_header.h"
#include "modules/rtp_rtcp/source/rtcp_packet/rtpfb.h"
#include "rtc_base/buffer.h"
#include "rtc_base/logging.h"
#include "test/gmock.h"
#include "test/gtest.h"

namespace webrtc {
namespace rtcp {

using ::testing::IsEmpty;

// PacketInfo is equal after serializing-deserializing if members are equal
// except for arrival time offset that may differ because of conversion back and
// forth to CompactNtp.
bool PacketInfoEqual(const CongestionControlFeedback::PacketInfo& a,
                     const CongestionControlFeedback::PacketInfo& b) {
  bool arrival_time_offset_equal =
      (a.arrival_time_offset.IsInfinite() &&
       b.arrival_time_offset.IsInfinite()) ||
      (a.arrival_time_offset.IsFinite() && b.arrival_time_offset.IsFinite() &&
       (a.arrival_time_offset - b.arrival_time_offset).Abs() <
           TimeDelta::Seconds(1) / 1024);

  bool equal = a.ssrc == b.ssrc && a.sequence_number == b.sequence_number &&
               arrival_time_offset_equal && a.ecn == b.ecn;
  RTC_LOG_IF(LS_INFO, !equal)
      << " Not equal got ssrc: " << a.ssrc << ", seq: " << a.sequence_number
      << " arrival_time_offset: " << a.arrival_time_offset.ms_or(-1)
      << " ecn: " << a.ecn << " expected ssrc:" << b.ssrc
      << ", seq: " << b.sequence_number
      << " arrival_time_offset: " << b.arrival_time_offset.ms_or(-1)
      << " ecn: " << b.ecn;
  return equal;
}

MATCHER_P(PacketInfoEqual, expected_vector, "") {
  if (expected_vector.size() != arg.size()) {
    RTC_LOG(LS_INFO) << " Wrong size, expected: " << expected_vector.size()
                     << " got: " << arg.size();
    return false;
  }
  for (size_t i = 0; i < expected_vector.size(); ++i) {
    if (!PacketInfoEqual(arg[i], expected_vector[i])) {
      return false;
    }
  }
  return true;
}

TEST(CongestionControlFeedbackTest, BlockLengthNoPackets) {
  CongestionControlFeedback fb({}, /*compact_ntp_timestamp=*/1);
  EXPECT_EQ(fb.BlockLength(),
            /*common header */ 4u /*sender ssrc*/ + 4u + /*timestamp*/ 4u);
}

TEST(CongestionControlFeedbackTest, BlockLengthTwoSsrcOnePacketEach) {
  std::vector<CongestionControlFeedback::PacketInfo> packets = {
      {.ssrc = 1, .sequence_number = 1}, {.ssrc = 2, .sequence_number = 1}};

  CongestionControlFeedback fb(std::move(packets), /*compact_ntp_timestamp=*/1);
  EXPECT_EQ(fb.BlockLength(),
            /*common header */ 4u + /*sender ssrc*/
                4u +
                /*timestamp*/ 4u +
                /*per ssrc header*/ 2 * 8u +
                /* padded packet info per ssrc*/ 2 * 4u);
}

TEST(CongestionControlFeedbackTest, BlockLengthTwoSsrcTwoPacketsEach) {
  std::vector<CongestionControlFeedback::PacketInfo> packets = {
      {.ssrc = 1, .sequence_number = 1},
      {.ssrc = 1, .sequence_number = 2},
      {.ssrc = 2, .sequence_number = 1},
      {.ssrc = 2, .sequence_number = 2}};

  CongestionControlFeedback fb(std::move(packets), /*compact_ntp_timestamp=*/1);
  EXPECT_EQ(fb.BlockLength(),
            /*common header */ 4u + /*sender ssrc*/
                4u +
                /*timestamp*/ 4u +
                /*per ssrc header*/ 2 * 8u +
                /*packet info per ssrc*/ 2 * 4u);
}

TEST(CongestionControlFeedbackTest, BlockLengthMissingPackets) {
  std::vector<CongestionControlFeedback::PacketInfo> packets = {
      {.ssrc = 1, .sequence_number = 1},
      {.ssrc = 1, .sequence_number = 4},
  };

  CongestionControlFeedback fb(std::move(packets), /*compact_ntp_timestamp=*/1);
  EXPECT_EQ(fb.BlockLength(),
            /*common header */ 4u + /*sender ssrc*/
                4u +
                /*timestamp*/ 4u +
                /*per ssrc header*/ 1 * 8u +
                /*packet info per ssrc*/ 2 * 4u);
}

TEST(CongestionControlFeedbackTest, CreateReturnsTrueForBasicPacket) {
  std::vector<CongestionControlFeedback::PacketInfo> packets = {
      {.ssrc = 1,
       .sequence_number = 1,
       .arrival_time_offset = TimeDelta::Millis(1)},
      {.ssrc = 2,
       .sequence_number = 2,
       .arrival_time_offset = TimeDelta::Millis(2)}};
  CongestionControlFeedback fb(std::move(packets), /*compact_ntp_timestamp=*/1);

  Buffer buf = Buffer::CreateWithCapacity(fb.BlockLength());
  buf.AppendData(fb.BlockLength(), [&](ArrayView<uint8_t> buf_view) {
    size_t position = 0;
    FunctionView<void(ArrayView<const uint8_t> packet)> callback;
    EXPECT_TRUE(
        fb.Create(buf_view.data(), &position, buf_view.size(), callback));
    return position;
  });
}

TEST(CongestionControlFeedbackTest, CanCreateAndParseWithoutPackets) {
  const std::vector<CongestionControlFeedback::PacketInfo> kPackets = {};
  uint32_t kCompactNtp = 1234;
  CongestionControlFeedback fb(kPackets, kCompactNtp);

  Buffer buffer = fb.Build();
  CongestionControlFeedback parsed_fb;
  CommonHeader header;
  EXPECT_TRUE(header.Parse(buffer.data(), buffer.size()));
  EXPECT_TRUE(parsed_fb.Parse(header));
  EXPECT_THAT(parsed_fb.packets(), IsEmpty());

  EXPECT_EQ(parsed_fb.report_timestamp_compact_ntp(), kCompactNtp);
  EXPECT_THAT(parsed_fb.packets(), PacketInfoEqual(kPackets));
}

TEST(CongestionControlFeedbackTest, CanCreateAndParsePacketsWithTwoSsrc) {
  const std::vector<CongestionControlFeedback::PacketInfo> kPackets = {
      {.ssrc = 1,
       .sequence_number = 1,
       .arrival_time_offset = TimeDelta::Millis(1)},
      {.ssrc = 2,
       .sequence_number = 1,
       .arrival_time_offset = TimeDelta::Millis(3)}};
  uint32_t kCompactNtp = 1234;
  CongestionControlFeedback fb(kPackets, kCompactNtp);

  Buffer buffer = fb.Build();
  CongestionControlFeedback parsed_fb;
  CommonHeader header;
  EXPECT_TRUE(header.Parse(buffer.data(), buffer.size()));
  EXPECT_EQ(header.fmt(), CongestionControlFeedback::kFeedbackMessageType);
  EXPECT_EQ(header.type(), Rtpfb::kPacketType);
  EXPECT_TRUE(parsed_fb.Parse(header));

  EXPECT_EQ(parsed_fb.report_timestamp_compact_ntp(), kCompactNtp);
  EXPECT_THAT(parsed_fb.packets(), PacketInfoEqual(kPackets));
}

TEST(CongestionControlFeedbackTest, CanCreateAndParsePacketWithEcnCe) {
  const std::vector<CongestionControlFeedback::PacketInfo> kPackets = {
      {.ssrc = 1,
       .sequence_number = 1,
       .arrival_time_offset = TimeDelta::Millis(1),
       .ecn = EcnMarking::kCe}};
  uint32_t kCompactNtp = 1234;
  CongestionControlFeedback fb(kPackets, kCompactNtp);

  Buffer buffer = fb.Build();
  CongestionControlFeedback parsed_fb;
  CommonHeader header;
  EXPECT_TRUE(header.Parse(buffer.data(), buffer.size()));
  EXPECT_TRUE(parsed_fb.Parse(header));
  EXPECT_THAT(parsed_fb.packets(), PacketInfoEqual(kPackets));
}

TEST(CongestionControlFeedbackTest, CanCreateAndParsePacketWithEct1) {
  const std::vector<CongestionControlFeedback::PacketInfo> kPackets = {
      {.ssrc = 1,
       .sequence_number = 1,
       .arrival_time_offset = TimeDelta::Millis(1),
       .ecn = EcnMarking::kEct1}};
  uint32_t kCompactNtp = 1234;
  CongestionControlFeedback fb(kPackets, kCompactNtp);

  Buffer buffer = fb.Build();
  CongestionControlFeedback parsed_fb;
  CommonHeader header;
  EXPECT_TRUE(header.Parse(buffer.data(), buffer.size()));
  EXPECT_TRUE(parsed_fb.Parse(header));
  EXPECT_THAT(parsed_fb.packets(), PacketInfoEqual(kPackets));
}

TEST(CongestionControlFeedbackTest, CanCreateAndParseWithMissingPackets) {
  const std::vector<CongestionControlFeedback::PacketInfo> kPackets = {
      {.ssrc = 1,
       .sequence_number = 0xFFFE,
       .arrival_time_offset = TimeDelta::Millis(1)},
      {.ssrc = 1,
       .sequence_number = 0xFFFF,
       // Packet lost
       .arrival_time_offset = TimeDelta::MinusInfinity()},
      {.ssrc = 1,
       .sequence_number = 0,
       // Packet lost
       .arrival_time_offset = TimeDelta::MinusInfinity()},
      {.ssrc = 1,
       .sequence_number = 1,
       .arrival_time_offset = TimeDelta::Millis(1)}};
  uint32_t kCompactNtp = 1234;
  CongestionControlFeedback fb(kPackets, kCompactNtp);

  Buffer buffer = fb.Build();
  CongestionControlFeedback parsed_fb;
  CommonHeader header;
  EXPECT_TRUE(header.Parse(buffer.data(), buffer.size()));
  EXPECT_TRUE(parsed_fb.Parse(header));
  EXPECT_THAT(parsed_fb.packets(), PacketInfoEqual(kPackets));
}

}  // namespace rtcp
}  // namespace webrtc
