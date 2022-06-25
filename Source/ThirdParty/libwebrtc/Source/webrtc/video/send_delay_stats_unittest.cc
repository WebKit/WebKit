/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/send_delay_stats.h"

#include <cstdint>
#include <vector>

#include "call/rtp_config.h"
#include "system_wrappers/include/metrics.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
const uint32_t kSsrc1 = 17;
const uint32_t kSsrc2 = 42;
const uint32_t kRtxSsrc1 = 18;
const uint32_t kRtxSsrc2 = 43;
const uint16_t kPacketId = 2345;
const int64_t kMaxPacketDelayMs = 11000;
const int kMinRequiredPeriodicSamples = 5;
const int kProcessIntervalMs = 2000;
}  // namespace

class SendDelayStatsTest : public ::testing::Test {
 public:
  SendDelayStatsTest() : clock_(1234), config_(CreateConfig()) {}
  virtual ~SendDelayStatsTest() {}

 protected:
  virtual void SetUp() {
    stats_.reset(new SendDelayStats(&clock_));
    stats_->AddSsrcs(config_);
  }

  VideoSendStream::Config CreateConfig() {
    VideoSendStream::Config config(nullptr);
    config.rtp.ssrcs.push_back(kSsrc1);
    config.rtp.ssrcs.push_back(kSsrc2);
    config.rtp.rtx.ssrcs.push_back(kRtxSsrc1);
    config.rtp.rtx.ssrcs.push_back(kRtxSsrc2);
    return config;
  }

  void OnSendPacket(uint16_t id, uint32_t ssrc) {
    OnSendPacket(id, ssrc, clock_.TimeInMilliseconds());
  }

  void OnSendPacket(uint16_t id, uint32_t ssrc, int64_t capture_ms) {
    SendPacketObserver* observer = stats_.get();
    observer->OnSendPacket(id, capture_ms, ssrc);
  }

  bool OnSentPacket(uint16_t id) {
    return stats_->OnSentPacket(id, clock_.TimeInMilliseconds());
  }

  SimulatedClock clock_;
  VideoSendStream::Config config_;
  std::unique_ptr<SendDelayStats> stats_;
};

TEST_F(SendDelayStatsTest, SentPacketFound) {
  EXPECT_FALSE(OnSentPacket(kPacketId));
  OnSendPacket(kPacketId, kSsrc1);
  EXPECT_TRUE(OnSentPacket(kPacketId));   // Packet found.
  EXPECT_FALSE(OnSentPacket(kPacketId));  // Packet removed when found.
}

TEST_F(SendDelayStatsTest, SentPacketNotFoundForNonRegisteredSsrc) {
  OnSendPacket(kPacketId, kSsrc1);
  EXPECT_TRUE(OnSentPacket(kPacketId));
  OnSendPacket(kPacketId + 1, kSsrc2);
  EXPECT_TRUE(OnSentPacket(kPacketId + 1));
  OnSendPacket(kPacketId + 2, kRtxSsrc1);  // RTX SSRC not registered.
  EXPECT_FALSE(OnSentPacket(kPacketId + 2));
}

TEST_F(SendDelayStatsTest, SentPacketFoundWithMaxSendDelay) {
  OnSendPacket(kPacketId, kSsrc1);
  clock_.AdvanceTimeMilliseconds(kMaxPacketDelayMs - 1);
  OnSendPacket(kPacketId + 1, kSsrc1);       // kPacketId -> not old/removed.
  EXPECT_TRUE(OnSentPacket(kPacketId));      // Packet found.
  EXPECT_TRUE(OnSentPacket(kPacketId + 1));  // Packet found.
}

TEST_F(SendDelayStatsTest, OldPacketsRemoved) {
  const int64_t kCaptureTimeMs = clock_.TimeInMilliseconds();
  OnSendPacket(0xffffu, kSsrc1, kCaptureTimeMs);
  OnSendPacket(0u, kSsrc1, kCaptureTimeMs);
  OnSendPacket(1u, kSsrc1, kCaptureTimeMs + 1);
  clock_.AdvanceTimeMilliseconds(kMaxPacketDelayMs);  // 0xffff, 0 -> old.
  OnSendPacket(2u, kSsrc1, kCaptureTimeMs + 2);

  EXPECT_FALSE(OnSentPacket(0xffffu));  // Old removed.
  EXPECT_FALSE(OnSentPacket(0u));       // Old removed.
  EXPECT_TRUE(OnSentPacket(1u));
  EXPECT_TRUE(OnSentPacket(2u));
}

TEST_F(SendDelayStatsTest, HistogramsAreUpdated) {
  metrics::Reset();
  const int64_t kDelayMs1 = 5;
  const int64_t kDelayMs2 = 15;
  const int kNumSamples = kMinRequiredPeriodicSamples * kProcessIntervalMs /
                              (kDelayMs1 + kDelayMs2) +
                          1;

  uint16_t id = 0;
  for (int i = 0; i < kNumSamples; ++i) {
    OnSendPacket(++id, kSsrc1);
    clock_.AdvanceTimeMilliseconds(kDelayMs1);
    EXPECT_TRUE(OnSentPacket(id));
    OnSendPacket(++id, kSsrc2);
    clock_.AdvanceTimeMilliseconds(kDelayMs2);
    EXPECT_TRUE(OnSentPacket(id));
  }
  stats_.reset();
  EXPECT_METRIC_EQ(2, metrics::NumSamples("WebRTC.Video.SendDelayInMs"));
  EXPECT_METRIC_EQ(1,
                   metrics::NumEvents("WebRTC.Video.SendDelayInMs", kDelayMs1));
  EXPECT_METRIC_EQ(1,
                   metrics::NumEvents("WebRTC.Video.SendDelayInMs", kDelayMs2));
}

}  // namespace webrtc
