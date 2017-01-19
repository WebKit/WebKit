/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/receive_statistics_proxy.h"

#include <memory>

#include "webrtc/system_wrappers/include/metrics_default.h"
#include "webrtc/test/gtest.h"

namespace webrtc {
namespace {
const int64_t kFreqOffsetProcessIntervalInMs = 40000;
const uint32_t kLocalSsrc = 123;
const uint32_t kRemoteSsrc = 456;
const int kMinRequiredSamples = 200;
}  // namespace

// TODO(sakal): ReceiveStatisticsProxy is lacking unittesting.
class ReceiveStatisticsProxyTest : public ::testing::Test {
 public:
  ReceiveStatisticsProxyTest() : fake_clock_(1234), config_(GetTestConfig()) {}
  virtual ~ReceiveStatisticsProxyTest() {}

 protected:
  virtual void SetUp() {
    metrics::Reset();
    statistics_proxy_.reset(new ReceiveStatisticsProxy(&config_, &fake_clock_));
  }

  VideoReceiveStream::Config GetTestConfig() {
    VideoReceiveStream::Config config(nullptr);
    config.rtp.local_ssrc = kLocalSsrc;
    config.rtp.remote_ssrc = kRemoteSsrc;
    return config;
  }

  SimulatedClock fake_clock_;
  const VideoReceiveStream::Config config_;
  std::unique_ptr<ReceiveStatisticsProxy> statistics_proxy_;
};

TEST_F(ReceiveStatisticsProxyTest, OnDecodedFrameIncreasesFramesDecoded) {
  EXPECT_EQ(0u, statistics_proxy_->GetStats().frames_decoded);
  for (uint32_t i = 1; i <= 3; ++i) {
    statistics_proxy_->OnDecodedFrame();
    EXPECT_EQ(i, statistics_proxy_->GetStats().frames_decoded);
  }
}

TEST_F(ReceiveStatisticsProxyTest, GetStatsReportsSsrc) {
  EXPECT_EQ(kRemoteSsrc, statistics_proxy_->GetStats().ssrc);
}

TEST_F(ReceiveStatisticsProxyTest, GetStatsReportsIncomingPayloadType) {
  const int kPayloadType = 111;
  statistics_proxy_->OnIncomingPayloadType(kPayloadType);
  EXPECT_EQ(kPayloadType, statistics_proxy_->GetStats().current_payload_type);
}

TEST_F(ReceiveStatisticsProxyTest, GetStatsReportsIncomingRate) {
  const int kFramerate = 28;
  const int kBitrateBps = 311000;
  statistics_proxy_->OnIncomingRate(kFramerate, kBitrateBps);
  EXPECT_EQ(kFramerate, statistics_proxy_->GetStats().network_frame_rate);
  EXPECT_EQ(kBitrateBps, statistics_proxy_->GetStats().total_bitrate_bps);
}

TEST_F(ReceiveStatisticsProxyTest, GetStatsReportsDecodeTimingStats) {
  const int kDecodeMs = 1;
  const int kMaxDecodeMs = 2;
  const int kCurrentDelayMs = 3;
  const int kTargetDelayMs = 4;
  const int kJitterBufferMs = 5;
  const int kMinPlayoutDelayMs = 6;
  const int kRenderDelayMs = 7;
  const int64_t kRttMs = 8;
  statistics_proxy_->OnDecoderTiming(
      kDecodeMs, kMaxDecodeMs, kCurrentDelayMs, kTargetDelayMs, kJitterBufferMs,
      kMinPlayoutDelayMs, kRenderDelayMs, kRttMs);
  VideoReceiveStream::Stats stats = statistics_proxy_->GetStats();
  EXPECT_EQ(kDecodeMs, stats.decode_ms);
  EXPECT_EQ(kMaxDecodeMs, stats.max_decode_ms);
  EXPECT_EQ(kCurrentDelayMs, stats.current_delay_ms);
  EXPECT_EQ(kTargetDelayMs, stats.target_delay_ms);
  EXPECT_EQ(kJitterBufferMs, stats.jitter_buffer_ms);
  EXPECT_EQ(kMinPlayoutDelayMs, stats.min_playout_delay_ms);
  EXPECT_EQ(kRenderDelayMs, stats.render_delay_ms);
}

TEST_F(ReceiveStatisticsProxyTest, GetStatsReportsDiscardedPackets) {
  const int kDiscardedPackets = 12;
  statistics_proxy_->OnDiscardedPacketsUpdated(kDiscardedPackets);
  EXPECT_EQ(kDiscardedPackets, statistics_proxy_->GetStats().discarded_packets);
}

TEST_F(ReceiveStatisticsProxyTest, LifetimeHistogramIsUpdated) {
  const int64_t kTimeSec = 3;
  fake_clock_.AdvanceTimeMilliseconds(kTimeSec * 1000);
  // Histograms are updated when the statistics_proxy_ is deleted.
  statistics_proxy_.reset();
  EXPECT_EQ(1,
            metrics::NumSamples("WebRTC.Video.ReceiveStreamLifetimeInSeconds"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.ReceiveStreamLifetimeInSeconds",
                                  kTimeSec));
}

TEST_F(ReceiveStatisticsProxyTest, AvSyncOffsetHistogramIsUpdated) {
  const int64_t kSyncOffsetMs = 22;
  const double kFreqKhz = 90.0;
  for (int i = 0; i < kMinRequiredSamples; ++i)
    statistics_proxy_->OnSyncOffsetUpdated(kSyncOffsetMs, kFreqKhz);
  // Histograms are updated when the statistics_proxy_ is deleted.
  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.AVSyncOffsetInMs"));
  EXPECT_EQ(1,
            metrics::NumEvents("WebRTC.Video.AVSyncOffsetInMs", kSyncOffsetMs));
}

TEST_F(ReceiveStatisticsProxyTest, RtpToNtpFrequencyOffsetHistogramIsUpdated) {
  const int64_t kSyncOffsetMs = 22;
  const double kFreqKhz = 90.0;
  statistics_proxy_->OnSyncOffsetUpdated(kSyncOffsetMs, kFreqKhz);
  statistics_proxy_->OnSyncOffsetUpdated(kSyncOffsetMs, kFreqKhz + 2.2);
  fake_clock_.AdvanceTimeMilliseconds(kFreqOffsetProcessIntervalInMs);
  // Process interval passed, max diff: 2.
  statistics_proxy_->OnSyncOffsetUpdated(kSyncOffsetMs, kFreqKhz + 1.1);
  statistics_proxy_->OnSyncOffsetUpdated(kSyncOffsetMs, kFreqKhz - 4.2);
  statistics_proxy_->OnSyncOffsetUpdated(kSyncOffsetMs, kFreqKhz - 0.9);
  fake_clock_.AdvanceTimeMilliseconds(kFreqOffsetProcessIntervalInMs);
  // Process interval passed, max diff: 4.
  statistics_proxy_->OnSyncOffsetUpdated(kSyncOffsetMs, kFreqKhz);
  statistics_proxy_.reset();
  // Average reported: (2 + 4) / 2 = 3.
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.RtpToNtpFreqOffsetInKhz"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.RtpToNtpFreqOffsetInKhz", 3));
}

}  // namespace webrtc
