/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <optional>

#include "logging/rtc_event_log/rtc_event_log_parser.h"
#include "test/gtest.h"
#include "video/timing/simulator/log_classifiers.h"
#include "video/timing/simulator/test/parsed_rtc_event_log_from_resources.h"

namespace webrtc::video_timing_simulator {
namespace {

// The tests in this file verify that the test resources logs have the expected
// RTX OSN logging status.
//
// Logs prior to https://webrtc-review.googlesource.com/c/src/+/442320 should
// be either std::nullopt (if no RTX recovery/padding was received), or
// `kNoRtxOsnLogged` (if RTX was received).
//
// Logs after https://webrtc-review.googlesource.com/c/src/+/442320 should be
// either std::nullopt (if no RTX recovery/padding was received), or
// `kAllRtxOsnLogged` (if RTX was received).

// Before https://webrtc-review.googlesource.com/c/src/+/442320.
TEST(TestResourcesTest, VideoRecvVp8Pt96IsUnset) {
  std::unique_ptr<ParsedRtcEventLog> parsed_log =
      ParsedRtcEventLogFromResources("video_recv_vp8_pt96");

  EXPECT_EQ(GetRtxOsnLoggingStatus(*parsed_log), std::nullopt);
}

// After https://webrtc-review.googlesource.com/c/src/+/442320.
TEST(TestResourcesTest, VideoRecvVp8Pt96LossyIskAllRtxOsnLogged) {
  std::unique_ptr<ParsedRtcEventLog> parsed_log =
      ParsedRtcEventLogFromResources("video_recv_vp8_pt96_lossy");

  EXPECT_EQ(GetRtxOsnLoggingStatus(*parsed_log),
            RtxOsnLoggingStatus::kAllRtxOsnLogged);
}

// Before https://webrtc-review.googlesource.com/c/src/+/442320.
TEST(TestResourcesTest, VideoRecvVp9Pt98IskNoRtxOsnLogged) {
  std::unique_ptr<ParsedRtcEventLog> parsed_log =
      ParsedRtcEventLogFromResources("video_recv_vp9_pt98");

  EXPECT_EQ(GetRtxOsnLoggingStatus(*parsed_log),
            RtxOsnLoggingStatus::kNoRtxOsnLogged);
}

// Before https://webrtc-review.googlesource.com/c/src/+/442320.
TEST(TestResourcesTest, VideoRecvAv1Pt45IskNoRtxOsnLogged) {
  std::unique_ptr<ParsedRtcEventLog> parsed_log =
      ParsedRtcEventLogFromResources("video_recv_av1_pt45");

  EXPECT_EQ(GetRtxOsnLoggingStatus(*parsed_log),
            RtxOsnLoggingStatus::kNoRtxOsnLogged);
}

// Before https://webrtc-review.googlesource.com/c/src/+/442320.
TEST(TestResourcesTest, VideoRecvSequentialJoinVp8Vp9Av1IsUnset) {
  std::unique_ptr<ParsedRtcEventLog> parsed_log =
      ParsedRtcEventLogFromResources("video_recv_sequential_join_vp8_vp9_av1");

  EXPECT_EQ(GetRtxOsnLoggingStatus(*parsed_log), std::nullopt);
}

}  // namespace
}  // namespace webrtc::video_timing_simulator
