/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/timing/simulator/log_classifiers.h"

#include <cstdint>
#include <memory>
#include <optional>

#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "test/gtest.h"
#include "video/timing/simulator/test/parsed_rtc_event_log_builder.h"

namespace webrtc::video_timing_simulator {
namespace {

constexpr uint32_t kSsrc = 123;
constexpr uint32_t kRtxSsrc = 456;
constexpr uint16_t kRtxOsn1 = 1;
constexpr uint16_t kRtxOsn2 = 2;

TEST(GetRtxOsnLoggingStatusTest, EmptyLogIsUndeterminable) {
  ParsedRtcEventLogBuilder builder;
  std::unique_ptr<ParsedRtcEventLog> parsed_log = builder.Build();

  EXPECT_EQ(GetRtxOsnLoggingStatus(*parsed_log), std::nullopt);
}

TEST(GetRtxOsnLoggingStatusTest, SinglePacketWithoutRtxOsnIskNoRtxOsnLogged) {
  ParsedRtcEventLogBuilder builder;
  builder.LogVideoRecvConfig(kSsrc, kRtxSsrc);
  builder.LogRtpPacketIncoming(kRtxSsrc, std::nullopt);
  std::unique_ptr<ParsedRtcEventLog> parsed_log = builder.Build();

  EXPECT_EQ(GetRtxOsnLoggingStatus(*parsed_log),
            RtxOsnLoggingStatus::kNoRtxOsnLogged);
}

TEST(GetRtxOsnLoggingStatusTest, SinglePacketWithRtxOsnIskAllRtxOsnLogged) {
  ParsedRtcEventLogBuilder builder;
  builder.LogVideoRecvConfig(kSsrc, kRtxSsrc);
  builder.LogRtpPacketIncoming(kRtxSsrc, kRtxOsn1);
  std::unique_ptr<ParsedRtcEventLog> parsed_log = builder.Build();

  EXPECT_EQ(GetRtxOsnLoggingStatus(*parsed_log),
            RtxOsnLoggingStatus::kAllRtxOsnLogged);
}

TEST(GetRtxOsnLoggingStatusTest, TwoPacketsWithoutRtxOsnIskNoRtxOsnLogged) {
  ParsedRtcEventLogBuilder builder;
  builder.LogVideoRecvConfig(kSsrc, kRtxSsrc);
  builder.LogRtpPacketIncoming(kRtxSsrc, std::nullopt);
  builder.LogRtpPacketIncoming(kRtxSsrc, std::nullopt);
  std::unique_ptr<ParsedRtcEventLog> parsed_log = builder.Build();

  EXPECT_EQ(GetRtxOsnLoggingStatus(*parsed_log),
            RtxOsnLoggingStatus::kNoRtxOsnLogged);
}

TEST(GetRtxOsnLoggingStatusTest,
     OnePacketWithRtxOsnOnePacketWithoutRtxOsnIskSomeRtxOsnLogged) {
  ParsedRtcEventLogBuilder builder;
  builder.LogVideoRecvConfig(kSsrc, kRtxSsrc);
  builder.LogRtpPacketIncoming(kRtxSsrc, kRtxOsn1);
  builder.LogRtpPacketIncoming(kRtxSsrc, std::nullopt);
  std::unique_ptr<ParsedRtcEventLog> parsed_log = builder.Build();

  EXPECT_EQ(GetRtxOsnLoggingStatus(*parsed_log),
            RtxOsnLoggingStatus::kSomeRtxOsnLogged);
}

TEST(GetRtxOsnLoggingStatusTest, TwoPacketsWithRtxOsnIskAllRtxOsnLogged) {
  ParsedRtcEventLogBuilder builder;
  builder.LogVideoRecvConfig(kSsrc, kRtxSsrc);
  builder.LogRtpPacketIncoming(kRtxSsrc, kRtxOsn1);
  builder.LogRtpPacketIncoming(kRtxSsrc, kRtxOsn2);
  std::unique_ptr<ParsedRtcEventLog> parsed_log = builder.Build();

  EXPECT_EQ(GetRtxOsnLoggingStatus(*parsed_log),
            RtxOsnLoggingStatus::kAllRtxOsnLogged);
}

}  // namespace
}  // namespace webrtc::video_timing_simulator
