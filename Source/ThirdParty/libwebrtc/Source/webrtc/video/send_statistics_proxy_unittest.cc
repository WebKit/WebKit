/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/send_statistics_proxy.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "system_wrappers/include/metrics.h"
#include "system_wrappers/include/metrics_default.h"
#include "test/field_trial.h"
#include "test/gtest.h"

namespace webrtc {
namespace {
const uint32_t kFirstSsrc = 17;
const uint32_t kSecondSsrc = 42;
const uint32_t kFirstRtxSsrc = 18;
const uint32_t kSecondRtxSsrc = 43;
const uint32_t kFlexFecSsrc = 55;
const int kFpsPeriodicIntervalMs = 2000;
const int kWidth = 640;
const int kHeight = 480;
const int kQpIdx0 = 21;
const int kQpIdx1 = 39;
const CodecSpecificInfo kDefaultCodecInfo = []() {
  CodecSpecificInfo codec_info;
  codec_info.codecType = kVideoCodecVP8;
  codec_info.codecSpecific.VP8.simulcastIdx = 0;
  return codec_info;
}();
}  // namespace

class SendStatisticsProxyTest : public ::testing::Test {
 public:
  SendStatisticsProxyTest() : SendStatisticsProxyTest("") {}
  explicit SendStatisticsProxyTest(const std::string& field_trials)
      : override_field_trials_(field_trials),
        fake_clock_(1234),
        config_(GetTestConfig()),
        avg_delay_ms_(0),
        max_delay_ms_(0) {}
  virtual ~SendStatisticsProxyTest() {}

 protected:
  virtual void SetUp() {
    metrics::Reset();
    statistics_proxy_.reset(new SendStatisticsProxy(
        &fake_clock_, GetTestConfig(),
        VideoEncoderConfig::ContentType::kRealtimeVideo));
    expected_ = VideoSendStream::Stats();
    for (const auto& ssrc : config_.rtp.ssrcs)
      expected_.substreams[ssrc].is_rtx = false;
    for (const auto& ssrc : config_.rtp.rtx.ssrcs)
      expected_.substreams[ssrc].is_rtx = true;
  }

  VideoSendStream::Config GetTestConfig() {
    VideoSendStream::Config config(nullptr);
    config.rtp.ssrcs.push_back(kFirstSsrc);
    config.rtp.ssrcs.push_back(kSecondSsrc);
    config.rtp.rtx.ssrcs.push_back(kFirstRtxSsrc);
    config.rtp.rtx.ssrcs.push_back(kSecondRtxSsrc);
    config.rtp.ulpfec.red_payload_type = 17;
    return config;
  }

  VideoSendStream::Config GetTestConfigWithFlexFec() {
    VideoSendStream::Config config(nullptr);
    config.rtp.ssrcs.push_back(kFirstSsrc);
    config.rtp.ssrcs.push_back(kSecondSsrc);
    config.rtp.rtx.ssrcs.push_back(kFirstRtxSsrc);
    config.rtp.rtx.ssrcs.push_back(kSecondRtxSsrc);
    config.rtp.flexfec.payload_type = 50;
    config.rtp.flexfec.ssrc = kFlexFecSsrc;
    return config;
  }

  VideoSendStream::StreamStats GetStreamStats(uint32_t ssrc) {
    VideoSendStream::Stats stats = statistics_proxy_->GetStats();
    std::map<uint32_t, VideoSendStream::StreamStats>::iterator it =
        stats.substreams.find(ssrc);
    EXPECT_NE(it, stats.substreams.end());
    return it->second;
  }

  void UpdateDataCounters(uint32_t ssrc) {
    StreamDataCountersCallback* proxy =
        static_cast<StreamDataCountersCallback*>(statistics_proxy_.get());
    StreamDataCounters counters;
    proxy->DataCountersUpdated(counters, ssrc);
  }

  void ExpectEqual(VideoSendStream::Stats one, VideoSendStream::Stats other) {
    EXPECT_EQ(one.input_frame_rate, other.input_frame_rate);
    EXPECT_EQ(one.encode_frame_rate, other.encode_frame_rate);
    EXPECT_EQ(one.media_bitrate_bps, other.media_bitrate_bps);
    EXPECT_EQ(one.preferred_media_bitrate_bps,
              other.preferred_media_bitrate_bps);
    EXPECT_EQ(one.suspended, other.suspended);

    EXPECT_EQ(one.substreams.size(), other.substreams.size());
    for (std::map<uint32_t, VideoSendStream::StreamStats>::const_iterator it =
             one.substreams.begin();
         it != one.substreams.end(); ++it) {
      std::map<uint32_t, VideoSendStream::StreamStats>::const_iterator
          corresponding_it = other.substreams.find(it->first);
      ASSERT_TRUE(corresponding_it != other.substreams.end());
      const VideoSendStream::StreamStats& a = it->second;
      const VideoSendStream::StreamStats& b = corresponding_it->second;

      EXPECT_EQ(a.is_rtx, b.is_rtx);
      EXPECT_EQ(a.frame_counts.key_frames, b.frame_counts.key_frames);
      EXPECT_EQ(a.frame_counts.delta_frames, b.frame_counts.delta_frames);
      EXPECT_EQ(a.total_bitrate_bps, b.total_bitrate_bps);
      EXPECT_EQ(a.avg_delay_ms, b.avg_delay_ms);
      EXPECT_EQ(a.max_delay_ms, b.max_delay_ms);

      EXPECT_EQ(a.rtp_stats.transmitted.payload_bytes,
                b.rtp_stats.transmitted.payload_bytes);
      EXPECT_EQ(a.rtp_stats.transmitted.header_bytes,
                b.rtp_stats.transmitted.header_bytes);
      EXPECT_EQ(a.rtp_stats.transmitted.padding_bytes,
                b.rtp_stats.transmitted.padding_bytes);
      EXPECT_EQ(a.rtp_stats.transmitted.packets,
                b.rtp_stats.transmitted.packets);
      EXPECT_EQ(a.rtp_stats.retransmitted.packets,
                b.rtp_stats.retransmitted.packets);
      EXPECT_EQ(a.rtp_stats.fec.packets, b.rtp_stats.fec.packets);

      EXPECT_EQ(a.rtcp_stats.fraction_lost, b.rtcp_stats.fraction_lost);
      EXPECT_EQ(a.rtcp_stats.packets_lost, b.rtcp_stats.packets_lost);
      EXPECT_EQ(a.rtcp_stats.extended_highest_sequence_number,
                b.rtcp_stats.extended_highest_sequence_number);
      EXPECT_EQ(a.rtcp_stats.jitter, b.rtcp_stats.jitter);
    }
  }

  test::ScopedFieldTrials override_field_trials_;
  SimulatedClock fake_clock_;
  std::unique_ptr<SendStatisticsProxy> statistics_proxy_;
  VideoSendStream::Config config_;
  int avg_delay_ms_;
  int max_delay_ms_;
  VideoSendStream::Stats expected_;
  typedef std::map<uint32_t, VideoSendStream::StreamStats>::const_iterator
      StreamIterator;
};

TEST_F(SendStatisticsProxyTest, RtcpStatistics) {
  RtcpStatisticsCallback* callback = statistics_proxy_.get();
  for (const auto& ssrc : config_.rtp.ssrcs) {
    VideoSendStream::StreamStats& ssrc_stats = expected_.substreams[ssrc];

    // Add statistics with some arbitrary, but unique, numbers.
    uint32_t offset = ssrc * sizeof(RtcpStatistics);
    ssrc_stats.rtcp_stats.packets_lost = offset;
    ssrc_stats.rtcp_stats.extended_highest_sequence_number = offset + 1;
    ssrc_stats.rtcp_stats.fraction_lost = offset + 2;
    ssrc_stats.rtcp_stats.jitter = offset + 3;
    callback->StatisticsUpdated(ssrc_stats.rtcp_stats, ssrc);
  }
  for (const auto& ssrc : config_.rtp.rtx.ssrcs) {
    VideoSendStream::StreamStats& ssrc_stats = expected_.substreams[ssrc];

    // Add statistics with some arbitrary, but unique, numbers.
    uint32_t offset = ssrc * sizeof(RtcpStatistics);
    ssrc_stats.rtcp_stats.packets_lost = offset;
    ssrc_stats.rtcp_stats.extended_highest_sequence_number = offset + 1;
    ssrc_stats.rtcp_stats.fraction_lost = offset + 2;
    ssrc_stats.rtcp_stats.jitter = offset + 3;
    callback->StatisticsUpdated(ssrc_stats.rtcp_stats, ssrc);
  }
  VideoSendStream::Stats stats = statistics_proxy_->GetStats();
  ExpectEqual(expected_, stats);
}

TEST_F(SendStatisticsProxyTest, Suspended) {
  // Verify that the value is false by default.
  EXPECT_FALSE(statistics_proxy_->GetStats().suspended);

  // Verify that we can set it to true.
  statistics_proxy_->OnSuspendChange(true);
  EXPECT_TRUE(statistics_proxy_->GetStats().suspended);

  // Verify that we can set it back to false again.
  statistics_proxy_->OnSuspendChange(false);
  EXPECT_FALSE(statistics_proxy_->GetStats().suspended);
}

TEST_F(SendStatisticsProxyTest, FrameCounts) {
  FrameCountObserver* observer = statistics_proxy_.get();
  for (const auto& ssrc : config_.rtp.ssrcs) {
    // Add statistics with some arbitrary, but unique, numbers.
    VideoSendStream::StreamStats& stats = expected_.substreams[ssrc];
    uint32_t offset = ssrc * sizeof(VideoSendStream::StreamStats);
    FrameCounts frame_counts;
    frame_counts.key_frames = offset;
    frame_counts.delta_frames = offset + 1;
    stats.frame_counts = frame_counts;
    observer->FrameCountUpdated(frame_counts, ssrc);
  }
  for (const auto& ssrc : config_.rtp.rtx.ssrcs) {
    // Add statistics with some arbitrary, but unique, numbers.
    VideoSendStream::StreamStats& stats = expected_.substreams[ssrc];
    uint32_t offset = ssrc * sizeof(VideoSendStream::StreamStats);
    FrameCounts frame_counts;
    frame_counts.key_frames = offset;
    frame_counts.delta_frames = offset + 1;
    stats.frame_counts = frame_counts;
    observer->FrameCountUpdated(frame_counts, ssrc);
  }

  VideoSendStream::Stats stats = statistics_proxy_->GetStats();
  ExpectEqual(expected_, stats);
}

TEST_F(SendStatisticsProxyTest, DataCounters) {
  StreamDataCountersCallback* callback = statistics_proxy_.get();
  for (const auto& ssrc : config_.rtp.ssrcs) {
    StreamDataCounters& counters = expected_.substreams[ssrc].rtp_stats;
    // Add statistics with some arbitrary, but unique, numbers.
    size_t offset = ssrc * sizeof(StreamDataCounters);
    uint32_t offset_uint32 = static_cast<uint32_t>(offset);
    counters.transmitted.payload_bytes = offset;
    counters.transmitted.header_bytes = offset + 1;
    counters.fec.packets = offset_uint32 + 2;
    counters.transmitted.padding_bytes = offset + 3;
    counters.retransmitted.packets = offset_uint32 + 4;
    counters.transmitted.packets = offset_uint32 + 5;
    callback->DataCountersUpdated(counters, ssrc);
  }
  for (const auto& ssrc : config_.rtp.rtx.ssrcs) {
    StreamDataCounters& counters = expected_.substreams[ssrc].rtp_stats;
    // Add statistics with some arbitrary, but unique, numbers.
    size_t offset = ssrc * sizeof(StreamDataCounters);
    uint32_t offset_uint32 = static_cast<uint32_t>(offset);
    counters.transmitted.payload_bytes = offset;
    counters.transmitted.header_bytes = offset + 1;
    counters.fec.packets = offset_uint32 + 2;
    counters.transmitted.padding_bytes = offset + 3;
    counters.retransmitted.packets = offset_uint32 + 4;
    counters.transmitted.packets = offset_uint32 + 5;
    callback->DataCountersUpdated(counters, ssrc);
  }

  VideoSendStream::Stats stats = statistics_proxy_->GetStats();
  ExpectEqual(expected_, stats);
}

TEST_F(SendStatisticsProxyTest, Bitrate) {
  BitrateStatisticsObserver* observer = statistics_proxy_.get();
  for (const auto& ssrc : config_.rtp.ssrcs) {
    uint32_t total;
    uint32_t retransmit;
    // Use ssrc as bitrate_bps to get a unique value for each stream.
    total = ssrc;
    retransmit = ssrc + 1;
    observer->Notify(total, retransmit, ssrc);
    expected_.substreams[ssrc].total_bitrate_bps = total;
    expected_.substreams[ssrc].retransmit_bitrate_bps = retransmit;
  }
  for (const auto& ssrc : config_.rtp.rtx.ssrcs) {
    uint32_t total;
    uint32_t retransmit;
    // Use ssrc as bitrate_bps to get a unique value for each stream.
    total = ssrc;
    retransmit = ssrc + 1;
    observer->Notify(total, retransmit, ssrc);
    expected_.substreams[ssrc].total_bitrate_bps = total;
    expected_.substreams[ssrc].retransmit_bitrate_bps = retransmit;
  }

  VideoSendStream::Stats stats = statistics_proxy_->GetStats();
  ExpectEqual(expected_, stats);
}

TEST_F(SendStatisticsProxyTest, SendSideDelay) {
  SendSideDelayObserver* observer = statistics_proxy_.get();
  for (const auto& ssrc : config_.rtp.ssrcs) {
    // Use ssrc as avg_delay_ms and max_delay_ms to get a unique value for each
    // stream.
    int avg_delay_ms = ssrc;
    int max_delay_ms = ssrc + 1;
    observer->SendSideDelayUpdated(avg_delay_ms, max_delay_ms, ssrc);
    expected_.substreams[ssrc].avg_delay_ms = avg_delay_ms;
    expected_.substreams[ssrc].max_delay_ms = max_delay_ms;
  }
  for (const auto& ssrc : config_.rtp.rtx.ssrcs) {
    // Use ssrc as avg_delay_ms and max_delay_ms to get a unique value for each
    // stream.
    int avg_delay_ms = ssrc;
    int max_delay_ms = ssrc + 1;
    observer->SendSideDelayUpdated(avg_delay_ms, max_delay_ms, ssrc);
    expected_.substreams[ssrc].avg_delay_ms = avg_delay_ms;
    expected_.substreams[ssrc].max_delay_ms = max_delay_ms;
  }
  VideoSendStream::Stats stats = statistics_proxy_->GetStats();
  ExpectEqual(expected_, stats);
}

TEST_F(SendStatisticsProxyTest, OnEncodedFrameTimeMeasured) {
  const int kEncodeTimeMs = 11;
  CpuOveruseMetrics metrics;
  metrics.encode_usage_percent = 80;
  statistics_proxy_->OnEncodedFrameTimeMeasured(kEncodeTimeMs, metrics);

  VideoSendStream::Stats stats = statistics_proxy_->GetStats();
  EXPECT_EQ(kEncodeTimeMs, stats.avg_encode_time_ms);
  EXPECT_EQ(metrics.encode_usage_percent, stats.encode_usage_percent);
}

TEST_F(SendStatisticsProxyTest, OnEncoderReconfiguredChangePreferredBitrate) {
  VideoSendStream::Stats stats = statistics_proxy_->GetStats();
  EXPECT_EQ(0, stats.preferred_media_bitrate_bps);
  const int kPreferredMediaBitrateBps = 50;

  VideoEncoderConfig config;
  statistics_proxy_->OnEncoderReconfigured(config, kPreferredMediaBitrateBps);
  stats = statistics_proxy_->GetStats();
  EXPECT_EQ(kPreferredMediaBitrateBps, stats.preferred_media_bitrate_bps);
}

TEST_F(SendStatisticsProxyTest, OnSendEncodedImageIncreasesFramesEncoded) {
  EncodedImage encoded_image;
  CodecSpecificInfo codec_info;
  EXPECT_EQ(0u, statistics_proxy_->GetStats().frames_encoded);
  for (uint32_t i = 1; i <= 3; ++i) {
    statistics_proxy_->OnSendEncodedImage(encoded_image, &codec_info);
    EXPECT_EQ(i, statistics_proxy_->GetStats().frames_encoded);
  }
}

TEST_F(SendStatisticsProxyTest, OnSendEncodedImageIncreasesQpSum) {
  EncodedImage encoded_image;
  CodecSpecificInfo codec_info;
  EXPECT_EQ(rtc::Optional<uint64_t>(), statistics_proxy_->GetStats().qp_sum);
  encoded_image.qp_ = 3;
  statistics_proxy_->OnSendEncodedImage(encoded_image, &codec_info);
  EXPECT_EQ(rtc::Optional<uint64_t>(3u), statistics_proxy_->GetStats().qp_sum);
  encoded_image.qp_ = 127;
  statistics_proxy_->OnSendEncodedImage(encoded_image, &codec_info);
  EXPECT_EQ(rtc::Optional<uint64_t>(130u),
            statistics_proxy_->GetStats().qp_sum);
}

TEST_F(SendStatisticsProxyTest, OnSendEncodedImageWithoutQpQpSumWontExist) {
  EncodedImage encoded_image;
  CodecSpecificInfo codec_info;
  encoded_image.qp_ = -1;
  EXPECT_EQ(rtc::Optional<uint64_t>(), statistics_proxy_->GetStats().qp_sum);
  statistics_proxy_->OnSendEncodedImage(encoded_image, &codec_info);
  EXPECT_EQ(rtc::Optional<uint64_t>(), statistics_proxy_->GetStats().qp_sum);
}

TEST_F(SendStatisticsProxyTest, GetCpuAdaptationStats) {
  VideoStreamEncoder::AdaptCounts cpu_counts;
  VideoStreamEncoder::AdaptCounts quality_counts;
  EXPECT_FALSE(statistics_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_FALSE(statistics_proxy_->GetStats().cpu_limited_resolution);
  cpu_counts.fps = 1;
  cpu_counts.resolution = 0;
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);
  EXPECT_TRUE(statistics_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_FALSE(statistics_proxy_->GetStats().cpu_limited_resolution);
  cpu_counts.fps = 0;
  cpu_counts.resolution = 1;
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);
  EXPECT_FALSE(statistics_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_TRUE(statistics_proxy_->GetStats().cpu_limited_resolution);
  cpu_counts.fps = 1;
  cpu_counts.resolution = -1;
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);
  EXPECT_TRUE(statistics_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_FALSE(statistics_proxy_->GetStats().cpu_limited_resolution);
  cpu_counts.fps = -1;
  cpu_counts.resolution = -1;
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);
  EXPECT_FALSE(statistics_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_FALSE(statistics_proxy_->GetStats().cpu_limited_resolution);
}

TEST_F(SendStatisticsProxyTest, GetQualityAdaptationStats) {
  VideoStreamEncoder::AdaptCounts cpu_counts;
  VideoStreamEncoder::AdaptCounts quality_counts;
  EXPECT_FALSE(statistics_proxy_->GetStats().bw_limited_framerate);
  EXPECT_FALSE(statistics_proxy_->GetStats().bw_limited_resolution);
  quality_counts.fps = 1;
  quality_counts.resolution = 0;
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);
  EXPECT_TRUE(statistics_proxy_->GetStats().bw_limited_framerate);
  EXPECT_FALSE(statistics_proxy_->GetStats().bw_limited_resolution);
  quality_counts.fps = 0;
  quality_counts.resolution = 1;
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);
  EXPECT_FALSE(statistics_proxy_->GetStats().bw_limited_framerate);
  EXPECT_TRUE(statistics_proxy_->GetStats().bw_limited_resolution);
  quality_counts.fps = 1;
  quality_counts.resolution = -1;
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);
  EXPECT_TRUE(statistics_proxy_->GetStats().bw_limited_framerate);
  EXPECT_FALSE(statistics_proxy_->GetStats().bw_limited_resolution);
  quality_counts.fps = -1;
  quality_counts.resolution = -1;
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);
  EXPECT_FALSE(statistics_proxy_->GetStats().bw_limited_framerate);
  EXPECT_FALSE(statistics_proxy_->GetStats().bw_limited_resolution);
}

TEST_F(SendStatisticsProxyTest, GetStatsReportsCpuAdaptChanges) {
  VideoStreamEncoder::AdaptCounts cpu_counts;
  VideoStreamEncoder::AdaptCounts quality_counts;
  EXPECT_EQ(0, statistics_proxy_->GetStats().number_of_cpu_adapt_changes);

  cpu_counts.resolution = 1;
  statistics_proxy_->OnCpuAdaptationChanged(cpu_counts, quality_counts);
  EXPECT_FALSE(statistics_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_TRUE(statistics_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(1, statistics_proxy_->GetStats().number_of_cpu_adapt_changes);

  cpu_counts.resolution = 2;
  statistics_proxy_->OnCpuAdaptationChanged(cpu_counts, quality_counts);
  EXPECT_FALSE(statistics_proxy_->GetStats().cpu_limited_framerate);
  EXPECT_TRUE(statistics_proxy_->GetStats().cpu_limited_resolution);
  EXPECT_EQ(2, statistics_proxy_->GetStats().number_of_cpu_adapt_changes);
  EXPECT_EQ(0, statistics_proxy_->GetStats().number_of_quality_adapt_changes);
}

TEST_F(SendStatisticsProxyTest, GetStatsReportsQualityAdaptChanges) {
  VideoStreamEncoder::AdaptCounts cpu_counts;
  VideoStreamEncoder::AdaptCounts quality_counts;
  EXPECT_EQ(0, statistics_proxy_->GetStats().number_of_quality_adapt_changes);

  quality_counts.fps = 1;
  statistics_proxy_->OnQualityAdaptationChanged(cpu_counts, quality_counts);
  EXPECT_TRUE(statistics_proxy_->GetStats().bw_limited_framerate);
  EXPECT_FALSE(statistics_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(1, statistics_proxy_->GetStats().number_of_quality_adapt_changes);

  quality_counts.fps = 0;
  statistics_proxy_->OnQualityAdaptationChanged(cpu_counts, quality_counts);
  EXPECT_FALSE(statistics_proxy_->GetStats().bw_limited_framerate);
  EXPECT_FALSE(statistics_proxy_->GetStats().bw_limited_resolution);
  EXPECT_EQ(2, statistics_proxy_->GetStats().number_of_quality_adapt_changes);
  EXPECT_EQ(0, statistics_proxy_->GetStats().number_of_cpu_adapt_changes);
}

TEST_F(SendStatisticsProxyTest, AdaptChangesNotReported_AdaptationNotEnabled) {
  // First RTP packet sent.
  UpdateDataCounters(kFirstSsrc);
  // Min runtime has passed.
  fake_clock_.AdvanceTimeMilliseconds(metrics::kMinRunTimeInSeconds * 1000);
  statistics_proxy_.reset();
  EXPECT_EQ(0, metrics::NumSamples("WebRTC.Video.AdaptChangesPerMinute.Cpu"));
  EXPECT_EQ(0,
            metrics::NumSamples("WebRTC.Video.AdaptChangesPerMinute.Quality"));
}

TEST_F(SendStatisticsProxyTest, AdaptChangesNotReported_MinRuntimeNotPassed) {
  // First RTP packet sent.
  UpdateDataCounters(kFirstSsrc);
  // Enable adaptation.
  VideoStreamEncoder::AdaptCounts cpu_counts;
  VideoStreamEncoder::AdaptCounts quality_counts;
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);
  // Min runtime has not passed.
  fake_clock_.AdvanceTimeMilliseconds(metrics::kMinRunTimeInSeconds * 1000 - 1);
  statistics_proxy_.reset();
  EXPECT_EQ(0, metrics::NumSamples("WebRTC.Video.AdaptChangesPerMinute.Cpu"));
  EXPECT_EQ(0,
            metrics::NumSamples("WebRTC.Video.AdaptChangesPerMinute.Quality"));
}

TEST_F(SendStatisticsProxyTest, ZeroAdaptChangesReported) {
  // First RTP packet sent.
  UpdateDataCounters(kFirstSsrc);
  // Enable adaptation.
  VideoStreamEncoder::AdaptCounts cpu_counts;
  VideoStreamEncoder::AdaptCounts quality_counts;
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);
  // Min runtime has passed.
  fake_clock_.AdvanceTimeMilliseconds(metrics::kMinRunTimeInSeconds * 1000);
  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.AdaptChangesPerMinute.Cpu"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.AdaptChangesPerMinute.Cpu", 0));
  EXPECT_EQ(1,
            metrics::NumSamples("WebRTC.Video.AdaptChangesPerMinute.Quality"));
  EXPECT_EQ(
      1, metrics::NumEvents("WebRTC.Video.AdaptChangesPerMinute.Quality", 0));
}

TEST_F(SendStatisticsProxyTest, CpuAdaptChangesReported) {
  // First RTP packet sent.
  UpdateDataCounters(kFirstSsrc);
  // Enable adaptation.
  VideoStreamEncoder::AdaptCounts cpu_counts;
  VideoStreamEncoder::AdaptCounts quality_counts;
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);
  // Adapt changes: 1, elapsed time: 10 sec => 6 per minute.
  statistics_proxy_->OnCpuAdaptationChanged(cpu_counts, quality_counts);
  fake_clock_.AdvanceTimeMilliseconds(10000);
  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.AdaptChangesPerMinute.Cpu"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.AdaptChangesPerMinute.Cpu", 6));
}

TEST_F(SendStatisticsProxyTest, AdaptChangesStatsExcludesDisabledTime) {
  // First RTP packet sent.
  UpdateDataCounters(kFirstSsrc);

  // Disable quality adaptation.
  VideoStreamEncoder::AdaptCounts cpu_counts;
  VideoStreamEncoder::AdaptCounts quality_counts;
  quality_counts.fps = -1;
  quality_counts.resolution = -1;
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);
  fake_clock_.AdvanceTimeMilliseconds(10000);

  // Enable quality adaptation.
  // Adapt changes: 2, elapsed time: 20 sec.
  quality_counts.fps = 0;
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);
  fake_clock_.AdvanceTimeMilliseconds(5000);
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);
  fake_clock_.AdvanceTimeMilliseconds(9000);
  statistics_proxy_->OnQualityAdaptationChanged(cpu_counts, quality_counts);
  fake_clock_.AdvanceTimeMilliseconds(6000);
  statistics_proxy_->OnQualityAdaptationChanged(cpu_counts, quality_counts);

  // Disable quality adaptation.
  quality_counts.fps = -1;
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);
  fake_clock_.AdvanceTimeMilliseconds(30000);

  // Enable quality adaptation.
  // Adapt changes: 1, elapsed time: 10 sec.
  quality_counts.resolution = 0;
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);
  statistics_proxy_->OnQualityAdaptationChanged(cpu_counts, quality_counts);
  fake_clock_.AdvanceTimeMilliseconds(10000);

  // Disable quality adaptation.
  quality_counts.resolution = -1;
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);
  fake_clock_.AdvanceTimeMilliseconds(5000);
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);
  fake_clock_.AdvanceTimeMilliseconds(20000);

  // Adapt changes: 3, elapsed time: 30 sec => 6 per minute.
  statistics_proxy_.reset();
  EXPECT_EQ(1,
            metrics::NumSamples("WebRTC.Video.AdaptChangesPerMinute.Quality"));
  EXPECT_EQ(
      1, metrics::NumEvents("WebRTC.Video.AdaptChangesPerMinute.Quality", 6));
}

TEST_F(SendStatisticsProxyTest,
       AdaptChangesNotReported_ScalingNotEnabledVideoResumed) {
  // First RTP packet sent.
  UpdateDataCounters(kFirstSsrc);

  // Suspend and resume video.
  statistics_proxy_->OnSuspendChange(true);
  fake_clock_.AdvanceTimeMilliseconds(5000);
  statistics_proxy_->OnSuspendChange(false);

  // Min runtime has passed but scaling not enabled.
  fake_clock_.AdvanceTimeMilliseconds(metrics::kMinRunTimeInSeconds * 1000);
  statistics_proxy_.reset();
  EXPECT_EQ(0, metrics::NumSamples("WebRTC.Video.AdaptChangesPerMinute.Cpu"));
  EXPECT_EQ(0,
            metrics::NumSamples("WebRTC.Video.AdaptChangesPerMinute.Quality"));
}

TEST_F(SendStatisticsProxyTest, QualityAdaptChangesStatsExcludesSuspendedTime) {
  // First RTP packet sent.
  UpdateDataCounters(kFirstSsrc);

  // Enable adaptation.
  VideoStreamEncoder::AdaptCounts cpu_counts;
  VideoStreamEncoder::AdaptCounts quality_counts;
  // Adapt changes: 2, elapsed time: 20 sec.
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);
  fake_clock_.AdvanceTimeMilliseconds(20000);
  statistics_proxy_->OnQualityAdaptationChanged(cpu_counts, quality_counts);
  statistics_proxy_->OnQualityAdaptationChanged(cpu_counts, quality_counts);

  // Suspend and resume video.
  statistics_proxy_->OnSuspendChange(true);
  fake_clock_.AdvanceTimeMilliseconds(30000);
  statistics_proxy_->OnSuspendChange(false);

  // Adapt changes: 1, elapsed time: 10 sec.
  statistics_proxy_->OnQualityAdaptationChanged(cpu_counts, quality_counts);
  fake_clock_.AdvanceTimeMilliseconds(10000);

  // Adapt changes: 3, elapsed time: 30 sec => 6 per minute.
  statistics_proxy_.reset();
  EXPECT_EQ(1,
            metrics::NumSamples("WebRTC.Video.AdaptChangesPerMinute.Quality"));
  EXPECT_EQ(
      1, metrics::NumEvents("WebRTC.Video.AdaptChangesPerMinute.Quality", 6));
}

TEST_F(SendStatisticsProxyTest, CpuAdaptChangesStatsExcludesSuspendedTime) {
  // First RTP packet sent.
  UpdateDataCounters(kFirstSsrc);

  // Video not suspended.
  statistics_proxy_->OnSuspendChange(false);
  fake_clock_.AdvanceTimeMilliseconds(30000);

  // Enable adaptation.
  VideoStreamEncoder::AdaptCounts cpu_counts;
  VideoStreamEncoder::AdaptCounts quality_counts;
  // Adapt changes: 1, elapsed time: 20 sec.
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);
  fake_clock_.AdvanceTimeMilliseconds(10000);
  statistics_proxy_->OnCpuAdaptationChanged(cpu_counts, quality_counts);

  // Video not suspended, stats time already started.
  statistics_proxy_->OnSuspendChange(false);
  fake_clock_.AdvanceTimeMilliseconds(10000);

  // Disable adaptation.
  cpu_counts.fps = -1;
  cpu_counts.resolution = -1;
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);
  fake_clock_.AdvanceTimeMilliseconds(30000);

  // Suspend and resume video, stats time not started when scaling not enabled.
  statistics_proxy_->OnSuspendChange(true);
  fake_clock_.AdvanceTimeMilliseconds(30000);
  statistics_proxy_->OnSuspendChange(false);
  fake_clock_.AdvanceTimeMilliseconds(30000);

  // Enable adaptation.
  // Adapt changes: 1, elapsed time: 10 sec.
  cpu_counts.fps = 0;
  cpu_counts.resolution = 0;
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);
  fake_clock_.AdvanceTimeMilliseconds(10000);
  statistics_proxy_->OnCpuAdaptationChanged(cpu_counts, quality_counts);

  // Adapt changes: 2, elapsed time: 30 sec => 4 per minute.
  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.AdaptChangesPerMinute.Cpu"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.AdaptChangesPerMinute.Cpu", 4));
}

TEST_F(SendStatisticsProxyTest, AdaptChangesStatsNotStartedIfVideoSuspended) {
  // First RTP packet sent.
  UpdateDataCounters(kFirstSsrc);

  // Video suspended.
  statistics_proxy_->OnSuspendChange(true);

  // Enable adaptation, stats time not started when suspended.
  VideoStreamEncoder::AdaptCounts cpu_counts;
  VideoStreamEncoder::AdaptCounts quality_counts;
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);
  fake_clock_.AdvanceTimeMilliseconds(10000);

  // Resume video, stats time started.
  // Adapt changes: 1, elapsed time: 10 sec.
  statistics_proxy_->OnSuspendChange(false);
  fake_clock_.AdvanceTimeMilliseconds(10000);
  statistics_proxy_->OnCpuAdaptationChanged(cpu_counts, quality_counts);

  // Adapt changes: 1, elapsed time: 10 sec => 6 per minute.
  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.AdaptChangesPerMinute.Cpu"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.AdaptChangesPerMinute.Cpu", 6));
}

TEST_F(SendStatisticsProxyTest, AdaptChangesStatsRestartsOnFirstSentPacket) {
  // Send first packet, adaptation enabled.
  // Elapsed time before first packet is sent should be excluded.
  VideoStreamEncoder::AdaptCounts cpu_counts;
  VideoStreamEncoder::AdaptCounts quality_counts;
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);
  fake_clock_.AdvanceTimeMilliseconds(10000);
  UpdateDataCounters(kFirstSsrc);

  // Adapt changes: 1, elapsed time: 10 sec.
  fake_clock_.AdvanceTimeMilliseconds(10000);
  statistics_proxy_->OnQualityAdaptationChanged(cpu_counts, quality_counts);
  UpdateDataCounters(kFirstSsrc);

  // Adapt changes: 1, elapsed time: 10 sec => 6 per minute.
  statistics_proxy_.reset();
  EXPECT_EQ(1,
            metrics::NumSamples("WebRTC.Video.AdaptChangesPerMinute.Quality"));
  EXPECT_EQ(
      1, metrics::NumEvents("WebRTC.Video.AdaptChangesPerMinute.Quality", 6));
}

TEST_F(SendStatisticsProxyTest, AdaptChangesStatsStartedAfterFirstSentPacket) {
  // Enable and disable adaptation.
  VideoStreamEncoder::AdaptCounts cpu_counts;
  VideoStreamEncoder::AdaptCounts quality_counts;
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);
  fake_clock_.AdvanceTimeMilliseconds(60000);
  cpu_counts.fps = -1;
  cpu_counts.resolution = -1;
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);

  // Send first packet, scaling disabled.
  // Elapsed time before first packet is sent should be excluded.
  UpdateDataCounters(kFirstSsrc);
  fake_clock_.AdvanceTimeMilliseconds(60000);

  // Enable adaptation.
  cpu_counts.resolution = 0;
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);
  fake_clock_.AdvanceTimeMilliseconds(10000);
  UpdateDataCounters(kFirstSsrc);

  // Adapt changes: 1, elapsed time: 20 sec.
  fake_clock_.AdvanceTimeMilliseconds(10000);
  statistics_proxy_->OnCpuAdaptationChanged(cpu_counts, quality_counts);

  // Adapt changes: 1, elapsed time: 20 sec => 3 per minute.
  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.AdaptChangesPerMinute.Cpu"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.AdaptChangesPerMinute.Cpu", 3));
}

TEST_F(SendStatisticsProxyTest, AdaptChangesReportedAfterContentSwitch) {
  // First RTP packet sent, cpu adaptation enabled.
  UpdateDataCounters(kFirstSsrc);
  VideoStreamEncoder::AdaptCounts cpu_counts;
  VideoStreamEncoder::AdaptCounts quality_counts;
  quality_counts.fps = -1;
  quality_counts.resolution = -1;
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);

  // Adapt changes: 2, elapsed time: 15 sec => 8 per minute.
  statistics_proxy_->OnCpuAdaptationChanged(cpu_counts, quality_counts);
  fake_clock_.AdvanceTimeMilliseconds(6000);
  statistics_proxy_->OnCpuAdaptationChanged(cpu_counts, quality_counts);
  fake_clock_.AdvanceTimeMilliseconds(9000);

  // Switch content type, real-time stats should be updated.
  VideoEncoderConfig config;
  config.content_type = VideoEncoderConfig::ContentType::kScreen;
  statistics_proxy_->OnEncoderReconfigured(config, 50);
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.AdaptChangesPerMinute.Cpu"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.AdaptChangesPerMinute.Cpu", 8));
  EXPECT_EQ(0,
            metrics::NumSamples("WebRTC.Video.AdaptChangesPerMinute.Quality"));

  // First RTP packet sent, scaling enabled.
  UpdateDataCounters(kFirstSsrc);
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);

  // Adapt changes: 4, elapsed time: 120 sec => 2 per minute.
  statistics_proxy_->OnCpuAdaptationChanged(cpu_counts, quality_counts);
  statistics_proxy_->OnCpuAdaptationChanged(cpu_counts, quality_counts);
  statistics_proxy_->OnCpuAdaptationChanged(cpu_counts, quality_counts);
  statistics_proxy_->OnCpuAdaptationChanged(cpu_counts, quality_counts);
  fake_clock_.AdvanceTimeMilliseconds(120000);

  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples(
                   "WebRTC.Video.Screenshare.AdaptChangesPerMinute.Cpu"));
  EXPECT_EQ(1, metrics::NumEvents(
                   "WebRTC.Video.Screenshare.AdaptChangesPerMinute.Cpu", 2));
  EXPECT_EQ(0, metrics::NumSamples(
                   "WebRTC.Video.Screenshare.AdaptChangesPerMinute.Quality"));
}

TEST_F(SendStatisticsProxyTest, SwitchContentTypeUpdatesHistograms) {
  for (int i = 0; i < SendStatisticsProxy::kMinRequiredMetricsSamples; ++i)
    statistics_proxy_->OnIncomingFrame(kWidth, kHeight);

  // No switch, stats should not be updated.
  VideoEncoderConfig config;
  config.content_type = VideoEncoderConfig::ContentType::kRealtimeVideo;
  statistics_proxy_->OnEncoderReconfigured(config, 50);
  EXPECT_EQ(0, metrics::NumSamples("WebRTC.Video.InputWidthInPixels"));

  // Switch to screenshare, real-time stats should be updated.
  config.content_type = VideoEncoderConfig::ContentType::kScreen;
  statistics_proxy_->OnEncoderReconfigured(config, 50);
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.InputWidthInPixels"));
}

TEST_F(SendStatisticsProxyTest, InputResolutionHistogramsAreUpdated) {
  for (int i = 0; i < SendStatisticsProxy::kMinRequiredMetricsSamples; ++i)
    statistics_proxy_->OnIncomingFrame(kWidth, kHeight);

  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.InputWidthInPixels"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.InputWidthInPixels", kWidth));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.InputHeightInPixels"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.InputHeightInPixels", kHeight));
}

TEST_F(SendStatisticsProxyTest, SentResolutionHistogramsAreUpdated) {
  const int64_t kMaxEncodedFrameWindowMs = 800;
  const int kFps = 20;
  const int kNumFramesPerWindow = kFps * kMaxEncodedFrameWindowMs / 1000;
  const int kMinSamples =  // Sample added when removed from EncodedFrameMap.
      SendStatisticsProxy::kMinRequiredMetricsSamples + kNumFramesPerWindow;
  EncodedImage encoded_image;

  // Not enough samples, stats should not be updated.
  for (int i = 0; i < kMinSamples - 1; ++i) {
    fake_clock_.AdvanceTimeMilliseconds(1000 / kFps);
    ++encoded_image._timeStamp;
    statistics_proxy_->OnSendEncodedImage(encoded_image, nullptr);
  }
  SetUp();  // Reset stats proxy also causes histograms to be reported.
  EXPECT_EQ(0, metrics::NumSamples("WebRTC.Video.SentWidthInPixels"));
  EXPECT_EQ(0, metrics::NumSamples("WebRTC.Video.SentHeightInPixels"));

  // Enough samples, max resolution per frame should be reported.
  encoded_image._timeStamp = 0xfffffff0;  // Will wrap.
  for (int i = 0; i < kMinSamples; ++i) {
    fake_clock_.AdvanceTimeMilliseconds(1000 / kFps);
    ++encoded_image._timeStamp;
    encoded_image._encodedWidth = kWidth;
    encoded_image._encodedHeight = kHeight;
    statistics_proxy_->OnSendEncodedImage(encoded_image, nullptr);
    encoded_image._encodedWidth = kWidth / 2;
    encoded_image._encodedHeight = kHeight / 2;
    statistics_proxy_->OnSendEncodedImage(encoded_image, nullptr);
  }

  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.SentWidthInPixels"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.SentWidthInPixels", kWidth));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.SentHeightInPixels"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.SentHeightInPixels", kHeight));
}

TEST_F(SendStatisticsProxyTest, InputFpsHistogramIsUpdated) {
  const int kFps = 20;
  const int kMinPeriodicSamples = 6;
  int frames = kMinPeriodicSamples * kFpsPeriodicIntervalMs * kFps / 1000;
  for (int i = 0; i <= frames; ++i) {
    fake_clock_.AdvanceTimeMilliseconds(1000 / kFps);
    statistics_proxy_->OnIncomingFrame(kWidth, kHeight);
  }
  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.InputFramesPerSecond"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.InputFramesPerSecond", kFps));
}

TEST_F(SendStatisticsProxyTest, SentFpsHistogramIsUpdated) {
  EncodedImage encoded_image;
  const int kFps = 20;
  const int kMinPeriodicSamples = 6;
  int frames = kMinPeriodicSamples * kFpsPeriodicIntervalMs * kFps / 1000 + 1;
  for (int i = 0; i < frames; ++i) {
    fake_clock_.AdvanceTimeMilliseconds(1000 / kFps);
    ++encoded_image._timeStamp;
    statistics_proxy_->OnSendEncodedImage(encoded_image, nullptr);
    // Frame with same timestamp should not be counted.
    statistics_proxy_->OnSendEncodedImage(encoded_image, nullptr);
  }
  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.SentFramesPerSecond"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.SentFramesPerSecond", kFps));
}

TEST_F(SendStatisticsProxyTest, InputFpsHistogramExcludesSuspendedTime) {
  const int kFps = 20;
  const int kSuspendTimeMs = 10000;
  const int kMinPeriodicSamples = 6;
  int frames = kMinPeriodicSamples * kFpsPeriodicIntervalMs * kFps / 1000;
  for (int i = 0; i < frames; ++i) {
    fake_clock_.AdvanceTimeMilliseconds(1000 / kFps);
    statistics_proxy_->OnIncomingFrame(kWidth, kHeight);
  }
  // Suspend.
  statistics_proxy_->OnSuspendChange(true);
  fake_clock_.AdvanceTimeMilliseconds(kSuspendTimeMs);

  for (int i = 0; i < frames; ++i) {
    fake_clock_.AdvanceTimeMilliseconds(1000 / kFps);
    statistics_proxy_->OnIncomingFrame(kWidth, kHeight);
  }
  // Suspended time interval should not affect the framerate.
  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.InputFramesPerSecond"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.InputFramesPerSecond", kFps));
}

TEST_F(SendStatisticsProxyTest, SentFpsHistogramExcludesSuspendedTime) {
  EncodedImage encoded_image;
  const int kFps = 20;
  const int kSuspendTimeMs = 10000;
  const int kMinPeriodicSamples = 6;
  int frames = kMinPeriodicSamples * kFpsPeriodicIntervalMs * kFps / 1000;
  for (int i = 0; i < frames; ++i) {
    fake_clock_.AdvanceTimeMilliseconds(1000 / kFps);
    encoded_image._timeStamp = i + 1;
    statistics_proxy_->OnSendEncodedImage(encoded_image, nullptr);
  }
  // Suspend.
  statistics_proxy_->OnSuspendChange(true);
  fake_clock_.AdvanceTimeMilliseconds(kSuspendTimeMs);

  for (int i = 0; i < frames; ++i) {
    fake_clock_.AdvanceTimeMilliseconds(1000 / kFps);
    encoded_image._timeStamp = i + 1;
    statistics_proxy_->OnSendEncodedImage(encoded_image, nullptr);
  }
  // Suspended time interval should not affect the framerate.
  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.SentFramesPerSecond"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.SentFramesPerSecond", kFps));
}

TEST_F(SendStatisticsProxyTest, CpuLimitedHistogramNotUpdatedWhenDisabled) {
  VideoStreamEncoder::AdaptCounts cpu_counts;
  VideoStreamEncoder::AdaptCounts quality_counts;
  cpu_counts.resolution = -1;
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);

  for (int i = 0; i < SendStatisticsProxy::kMinRequiredMetricsSamples; ++i)
    statistics_proxy_->OnIncomingFrame(kWidth, kHeight);

  statistics_proxy_.reset();
  EXPECT_EQ(0,
            metrics::NumSamples("WebRTC.Video.CpuLimitedResolutionInPercent"));
}

TEST_F(SendStatisticsProxyTest, CpuLimitedHistogramUpdated) {
  VideoStreamEncoder::AdaptCounts cpu_counts;
  VideoStreamEncoder::AdaptCounts quality_counts;
  cpu_counts.resolution = 0;
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);

  for (int i = 0; i < SendStatisticsProxy::kMinRequiredMetricsSamples; ++i)
    statistics_proxy_->OnIncomingFrame(kWidth, kHeight);

  cpu_counts.resolution = 1;
  statistics_proxy_->OnCpuAdaptationChanged(cpu_counts, quality_counts);

  for (int i = 0; i < SendStatisticsProxy::kMinRequiredMetricsSamples; ++i)
    statistics_proxy_->OnIncomingFrame(kWidth, kHeight);

  statistics_proxy_.reset();
  EXPECT_EQ(1,
            metrics::NumSamples("WebRTC.Video.CpuLimitedResolutionInPercent"));
  EXPECT_EQ(
      1, metrics::NumEvents("WebRTC.Video.CpuLimitedResolutionInPercent", 50));
}

TEST_F(SendStatisticsProxyTest, LifetimeHistogramIsUpdated) {
  const int64_t kTimeSec = 3;
  fake_clock_.AdvanceTimeMilliseconds(kTimeSec * 1000);
  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.SendStreamLifetimeInSeconds"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.SendStreamLifetimeInSeconds",
                                  kTimeSec));
}

TEST_F(SendStatisticsProxyTest, CodecTypeHistogramIsUpdated) {
  fake_clock_.AdvanceTimeMilliseconds(metrics::kMinRunTimeInSeconds * 1000);
  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.Encoder.CodecType"));
}

TEST_F(SendStatisticsProxyTest, PauseEventHistogramIsUpdated) {
  // First RTP packet sent.
  UpdateDataCounters(kFirstSsrc);

  // Min runtime has passed.
  fake_clock_.AdvanceTimeMilliseconds(metrics::kMinRunTimeInSeconds * 1000);
  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.NumberOfPauseEvents"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.NumberOfPauseEvents", 0));
}

TEST_F(SendStatisticsProxyTest,
       PauseEventHistogramIsNotUpdatedIfMinRuntimeHasNotPassed) {
  // First RTP packet sent.
  UpdateDataCounters(kFirstSsrc);

  // Min runtime has not passed.
  fake_clock_.AdvanceTimeMilliseconds(metrics::kMinRunTimeInSeconds * 1000 - 1);
  statistics_proxy_.reset();
  EXPECT_EQ(0, metrics::NumSamples("WebRTC.Video.NumberOfPauseEvents"));
  EXPECT_EQ(0, metrics::NumSamples("WebRTC.Video.PausedTimeInPercent"));
}

TEST_F(SendStatisticsProxyTest,
       PauseEventHistogramIsNotUpdatedIfNoMediaIsSent) {
  // First RTP packet not sent.
  fake_clock_.AdvanceTimeMilliseconds(metrics::kMinRunTimeInSeconds * 1000);
  statistics_proxy_.reset();
  EXPECT_EQ(0, metrics::NumSamples("WebRTC.Video.NumberOfPauseEvents"));
}

TEST_F(SendStatisticsProxyTest, NoPauseEvent) {
  // First RTP packet sent and min runtime passed.
  UpdateDataCounters(kFirstSsrc);

  // No change. Video: 10000 ms, paused: 0 ms (0%).
  statistics_proxy_->OnSetEncoderTargetRate(50000);
  fake_clock_.AdvanceTimeMilliseconds(metrics::kMinRunTimeInSeconds * 1000);
  statistics_proxy_->OnSetEncoderTargetRate(0);  // VideoSendStream::Stop

  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.NumberOfPauseEvents"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.NumberOfPauseEvents", 0));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.PausedTimeInPercent"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.PausedTimeInPercent", 0));
}

TEST_F(SendStatisticsProxyTest, OnePauseEvent) {
  // First RTP packet sent and min runtime passed.
  UpdateDataCounters(kFirstSsrc);

  // One change. Video: 7000 ms, paused: 3000 ms (30%).
  statistics_proxy_->OnSetEncoderTargetRate(50000);
  fake_clock_.AdvanceTimeMilliseconds(7000);
  statistics_proxy_->OnSetEncoderTargetRate(0);
  fake_clock_.AdvanceTimeMilliseconds(3000);
  statistics_proxy_->OnSetEncoderTargetRate(0);  // VideoSendStream::Stop

  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.NumberOfPauseEvents"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.NumberOfPauseEvents", 1));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.PausedTimeInPercent"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.PausedTimeInPercent", 30));
}

TEST_F(SendStatisticsProxyTest, TwoPauseEvents) {
  // First RTP packet sent.
  UpdateDataCounters(kFirstSsrc);

  // Two changes. Video: 19000 ms, paused: 1000 ms (5%).
  statistics_proxy_->OnSetEncoderTargetRate(0);
  fake_clock_.AdvanceTimeMilliseconds(1000);
  statistics_proxy_->OnSetEncoderTargetRate(50000);  // Starts on bitrate > 0.
  fake_clock_.AdvanceTimeMilliseconds(7000);
  statistics_proxy_->OnSetEncoderTargetRate(60000);
  fake_clock_.AdvanceTimeMilliseconds(3000);
  statistics_proxy_->OnSetEncoderTargetRate(0);
  fake_clock_.AdvanceTimeMilliseconds(250);
  statistics_proxy_->OnSetEncoderTargetRate(0);
  fake_clock_.AdvanceTimeMilliseconds(750);
  statistics_proxy_->OnSetEncoderTargetRate(60000);
  fake_clock_.AdvanceTimeMilliseconds(5000);
  statistics_proxy_->OnSetEncoderTargetRate(50000);
  fake_clock_.AdvanceTimeMilliseconds(4000);
  statistics_proxy_->OnSetEncoderTargetRate(0);  // VideoSendStream::Stop

  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.NumberOfPauseEvents"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.NumberOfPauseEvents", 2));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.PausedTimeInPercent"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.PausedTimeInPercent", 5));
}

TEST_F(SendStatisticsProxyTest,
       PausedTimeHistogramIsNotUpdatedIfMinRuntimeHasNotPassed) {
  // First RTP packet sent.
  UpdateDataCounters(kFirstSsrc);
  fake_clock_.AdvanceTimeMilliseconds(metrics::kMinRunTimeInSeconds * 1000);

  // Min runtime has not passed.
  statistics_proxy_->OnSetEncoderTargetRate(50000);
  fake_clock_.AdvanceTimeMilliseconds(metrics::kMinRunTimeInSeconds * 1000 - 1);
  statistics_proxy_->OnSetEncoderTargetRate(0);  // VideoSendStream::Stop

  statistics_proxy_.reset();
  EXPECT_EQ(0, metrics::NumSamples("WebRTC.Video.PausedTimeInPercent"));
}

TEST_F(SendStatisticsProxyTest, VerifyQpHistogramStats_Vp8) {
  EncodedImage encoded_image;
  CodecSpecificInfo codec_info;
  codec_info.codecType = kVideoCodecVP8;

  for (int i = 0; i < SendStatisticsProxy::kMinRequiredMetricsSamples; ++i) {
    codec_info.codecSpecific.VP8.simulcastIdx = 0;
    encoded_image.qp_ = kQpIdx0;
    statistics_proxy_->OnSendEncodedImage(encoded_image, &codec_info);
    codec_info.codecSpecific.VP8.simulcastIdx = 1;
    encoded_image.qp_ = kQpIdx1;
    statistics_proxy_->OnSendEncodedImage(encoded_image, &codec_info);
  }
  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.Encoded.Qp.Vp8.S0"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.Encoded.Qp.Vp8.S0", kQpIdx0));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.Encoded.Qp.Vp8.S1"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.Encoded.Qp.Vp8.S1", kQpIdx1));
}

TEST_F(SendStatisticsProxyTest, VerifyQpHistogramStats_Vp8OneSsrc) {
  VideoSendStream::Config config(nullptr);
  config.rtp.ssrcs.push_back(kFirstSsrc);
  statistics_proxy_.reset(new SendStatisticsProxy(
      &fake_clock_, config, VideoEncoderConfig::ContentType::kRealtimeVideo));

  EncodedImage encoded_image;
  CodecSpecificInfo codec_info;
  codec_info.codecType = kVideoCodecVP8;

  for (int i = 0; i < SendStatisticsProxy::kMinRequiredMetricsSamples; ++i) {
    codec_info.codecSpecific.VP8.simulcastIdx = 0;
    encoded_image.qp_ = kQpIdx0;
    statistics_proxy_->OnSendEncodedImage(encoded_image, &codec_info);
  }
  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.Encoded.Qp.Vp8"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.Encoded.Qp.Vp8", kQpIdx0));
}

TEST_F(SendStatisticsProxyTest, VerifyQpHistogramStats_Vp9) {
  EncodedImage encoded_image;
  CodecSpecificInfo codec_info;
  codec_info.codecType = kVideoCodecVP9;
  codec_info.codecSpecific.VP9.num_spatial_layers = 2;

  for (int i = 0; i < SendStatisticsProxy::kMinRequiredMetricsSamples; ++i) {
    encoded_image.qp_ = kQpIdx0;
    codec_info.codecSpecific.VP9.spatial_idx = 0;
    statistics_proxy_->OnSendEncodedImage(encoded_image, &codec_info);
    encoded_image.qp_ = kQpIdx1;
    codec_info.codecSpecific.VP9.spatial_idx = 1;
    statistics_proxy_->OnSendEncodedImage(encoded_image, &codec_info);
  }
  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.Encoded.Qp.Vp9.S0"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.Encoded.Qp.Vp9.S0", kQpIdx0));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.Encoded.Qp.Vp9.S1"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.Encoded.Qp.Vp9.S1", kQpIdx1));
}

TEST_F(SendStatisticsProxyTest, VerifyQpHistogramStats_Vp9OneSpatialLayer) {
  VideoSendStream::Config config(nullptr);
  config.rtp.ssrcs.push_back(kFirstSsrc);
  statistics_proxy_.reset(new SendStatisticsProxy(
      &fake_clock_, config, VideoEncoderConfig::ContentType::kRealtimeVideo));

  EncodedImage encoded_image;
  CodecSpecificInfo codec_info;
  codec_info.codecType = kVideoCodecVP9;
  codec_info.codecSpecific.VP9.num_spatial_layers = 1;

  for (int i = 0; i < SendStatisticsProxy::kMinRequiredMetricsSamples; ++i) {
    encoded_image.qp_ = kQpIdx0;
    codec_info.codecSpecific.VP9.spatial_idx = 0;
    statistics_proxy_->OnSendEncodedImage(encoded_image, &codec_info);
  }
  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.Encoded.Qp.Vp9"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.Encoded.Qp.Vp9", kQpIdx0));
}

TEST_F(SendStatisticsProxyTest, VerifyQpHistogramStats_H264) {
  EncodedImage encoded_image;
  CodecSpecificInfo codec_info;
  codec_info.codecType = kVideoCodecH264;

  for (int i = 0; i < SendStatisticsProxy::kMinRequiredMetricsSamples; ++i) {
    encoded_image.qp_ = kQpIdx0;
    statistics_proxy_->OnSendEncodedImage(encoded_image, &codec_info);
  }
  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.Encoded.Qp.H264"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.Encoded.Qp.H264", kQpIdx0));
}

TEST_F(SendStatisticsProxyTest,
       BandwidthLimitedHistogramsNotUpdatedWhenDisabled) {
  EncodedImage encoded_image;
  // encoded_image.adapt_reason_.bw_resolutions_disabled by default: -1
  for (int i = 0; i < SendStatisticsProxy::kMinRequiredMetricsSamples; ++i)
    statistics_proxy_->OnSendEncodedImage(encoded_image, nullptr);

  // Histograms are updated when the statistics_proxy_ is deleted.
  statistics_proxy_.reset();
  EXPECT_EQ(0, metrics::NumSamples(
                   "WebRTC.Video.BandwidthLimitedResolutionInPercent"));
  EXPECT_EQ(0, metrics::NumSamples(
                   "WebRTC.Video.BandwidthLimitedResolutionsDisabled"));
}

TEST_F(SendStatisticsProxyTest,
       BandwidthLimitedHistogramsUpdatedWhenEnabled_NoResolutionDisabled) {
  const int kResolutionsDisabled = 0;
  EncodedImage encoded_image;
  encoded_image.adapt_reason_.bw_resolutions_disabled = kResolutionsDisabled;
  for (int i = 0; i < SendStatisticsProxy::kMinRequiredMetricsSamples; ++i)
    statistics_proxy_->OnSendEncodedImage(encoded_image, nullptr);

  // Histograms are updated when the statistics_proxy_ is deleted.
  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples(
                   "WebRTC.Video.BandwidthLimitedResolutionInPercent"));
  EXPECT_EQ(1, metrics::NumEvents(
                   "WebRTC.Video.BandwidthLimitedResolutionInPercent", 0));
  // No resolution disabled.
  EXPECT_EQ(0, metrics::NumSamples(
                   "WebRTC.Video.BandwidthLimitedResolutionsDisabled"));
}

TEST_F(SendStatisticsProxyTest,
       BandwidthLimitedHistogramsUpdatedWhenEnabled_OneResolutionDisabled) {
  const int kResolutionsDisabled = 1;
  EncodedImage encoded_image;
  encoded_image.adapt_reason_.bw_resolutions_disabled = kResolutionsDisabled;
  for (int i = 0; i < SendStatisticsProxy::kMinRequiredMetricsSamples; ++i)
    statistics_proxy_->OnSendEncodedImage(encoded_image, nullptr);

  // Histograms are updated when the statistics_proxy_ is deleted.
  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples(
                   "WebRTC.Video.BandwidthLimitedResolutionInPercent"));
  EXPECT_EQ(1, metrics::NumEvents(
                   "WebRTC.Video.BandwidthLimitedResolutionInPercent", 100));
  // Resolutions disabled.
  EXPECT_EQ(1, metrics::NumSamples(
                   "WebRTC.Video.BandwidthLimitedResolutionsDisabled"));
  EXPECT_EQ(
      1, metrics::NumEvents("WebRTC.Video.BandwidthLimitedResolutionsDisabled",
                            kResolutionsDisabled));
}

TEST_F(SendStatisticsProxyTest,
       QualityLimitedHistogramsNotUpdatedWhenDisabled) {
  VideoStreamEncoder::AdaptCounts cpu_counts;
  VideoStreamEncoder::AdaptCounts quality_counts;
  quality_counts.resolution = -1;
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);
  EncodedImage encoded_image;
  for (int i = 0; i < SendStatisticsProxy::kMinRequiredMetricsSamples; ++i)
    statistics_proxy_->OnSendEncodedImage(encoded_image, &kDefaultCodecInfo);

  // Histograms are updated when the statistics_proxy_ is deleted.
  statistics_proxy_.reset();
  EXPECT_EQ(
      0, metrics::NumSamples("WebRTC.Video.QualityLimitedResolutionInPercent"));
  EXPECT_EQ(0, metrics::NumSamples(
                   "WebRTC.Video.QualityLimitedResolutionDownscales"));
}

TEST_F(SendStatisticsProxyTest,
       QualityLimitedHistogramsUpdatedWhenEnabled_NoResolutionDownscale) {
  VideoStreamEncoder::AdaptCounts cpu_counts;
  VideoStreamEncoder::AdaptCounts quality_counts;
  quality_counts.resolution = 0;
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);
  EncodedImage encoded_image;
  for (int i = 0; i < SendStatisticsProxy::kMinRequiredMetricsSamples; ++i)
    statistics_proxy_->OnSendEncodedImage(encoded_image, &kDefaultCodecInfo);

  // Histograms are updated when the statistics_proxy_ is deleted.
  statistics_proxy_.reset();
  EXPECT_EQ(
      1, metrics::NumSamples("WebRTC.Video.QualityLimitedResolutionInPercent"));
  EXPECT_EQ(1, metrics::NumEvents(
                   "WebRTC.Video.QualityLimitedResolutionInPercent", 0));
  // No resolution downscale.
  EXPECT_EQ(0, metrics::NumSamples(
                   "WebRTC.Video.QualityLimitedResolutionDownscales"));
}

TEST_F(SendStatisticsProxyTest,
       QualityLimitedHistogramsUpdatedWhenEnabled_TwoResolutionDownscales) {
  const int kDownscales = 2;
  VideoStreamEncoder::AdaptCounts cpu_counts;
  VideoStreamEncoder::AdaptCounts quality_counts;
  quality_counts.resolution = kDownscales;
  statistics_proxy_->SetAdaptationStats(cpu_counts, quality_counts);
  EncodedImage encoded_image;
  for (int i = 0; i < SendStatisticsProxy::kMinRequiredMetricsSamples; ++i)
    statistics_proxy_->OnSendEncodedImage(encoded_image, &kDefaultCodecInfo);
  // Histograms are updated when the statistics_proxy_ is deleted.
  statistics_proxy_.reset();
  EXPECT_EQ(
      1, metrics::NumSamples("WebRTC.Video.QualityLimitedResolutionInPercent"));
  EXPECT_EQ(1, metrics::NumEvents(
                   "WebRTC.Video.QualityLimitedResolutionInPercent", 100));
  // Resolution downscales.
  EXPECT_EQ(1, metrics::NumSamples(
                   "WebRTC.Video.QualityLimitedResolutionDownscales"));
  EXPECT_EQ(
      1, metrics::NumEvents("WebRTC.Video.QualityLimitedResolutionDownscales",
                            kDownscales));
}

TEST_F(SendStatisticsProxyTest, GetStatsReportsBandwidthLimitedResolution) {
  // Initially false.
  EXPECT_FALSE(statistics_proxy_->GetStats().bw_limited_resolution);
  // No resolution scale by default.
  EncodedImage encoded_image;
  statistics_proxy_->OnSendEncodedImage(encoded_image, nullptr);
  EXPECT_FALSE(statistics_proxy_->GetStats().bw_limited_resolution);

  // Simulcast disabled resolutions
  encoded_image.adapt_reason_.bw_resolutions_disabled = 1;
  statistics_proxy_->OnSendEncodedImage(encoded_image, nullptr);
  EXPECT_TRUE(statistics_proxy_->GetStats().bw_limited_resolution);

  encoded_image.adapt_reason_.bw_resolutions_disabled = 0;
  statistics_proxy_->OnSendEncodedImage(encoded_image, nullptr);
  EXPECT_FALSE(statistics_proxy_->GetStats().bw_limited_resolution);

  // Resolution scaled due to quality.
  VideoStreamEncoder::AdaptCounts cpu_counts;
  VideoStreamEncoder::AdaptCounts quality_counts;
  quality_counts.resolution = 1;
  statistics_proxy_->OnQualityAdaptationChanged(cpu_counts, quality_counts);
  statistics_proxy_->OnSendEncodedImage(encoded_image, nullptr);
  EXPECT_TRUE(statistics_proxy_->GetStats().bw_limited_resolution);
}

TEST_F(SendStatisticsProxyTest, GetStatsReportsTargetMediaBitrate) {
  // Initially zero.
  EXPECT_EQ(0, statistics_proxy_->GetStats().target_media_bitrate_bps);

  const int kBitrate = 100000;
  statistics_proxy_->OnSetEncoderTargetRate(kBitrate);
  EXPECT_EQ(kBitrate, statistics_proxy_->GetStats().target_media_bitrate_bps);

  statistics_proxy_->OnSetEncoderTargetRate(0);
  EXPECT_EQ(0, statistics_proxy_->GetStats().target_media_bitrate_bps);
}

TEST_F(SendStatisticsProxyTest, NoSubstreams) {
  uint32_t excluded_ssrc =
      std::max(
          *std::max_element(config_.rtp.ssrcs.begin(), config_.rtp.ssrcs.end()),
          *std::max_element(config_.rtp.rtx.ssrcs.begin(),
                            config_.rtp.rtx.ssrcs.end())) +
      1;
  // From RtcpStatisticsCallback.
  RtcpStatistics rtcp_stats;
  RtcpStatisticsCallback* rtcp_callback = statistics_proxy_.get();
  rtcp_callback->StatisticsUpdated(rtcp_stats, excluded_ssrc);

  // From BitrateStatisticsObserver.
  uint32_t total = 0;
  uint32_t retransmit = 0;
  BitrateStatisticsObserver* bitrate_observer = statistics_proxy_.get();
  bitrate_observer->Notify(total, retransmit, excluded_ssrc);

  // From FrameCountObserver.
  FrameCountObserver* fps_observer = statistics_proxy_.get();
  FrameCounts frame_counts;
  frame_counts.key_frames = 1;
  fps_observer->FrameCountUpdated(frame_counts, excluded_ssrc);

  VideoSendStream::Stats stats = statistics_proxy_->GetStats();
  EXPECT_TRUE(stats.substreams.empty());
}

TEST_F(SendStatisticsProxyTest, EncodedResolutionTimesOut) {
  static const int kEncodedWidth = 123;
  static const int kEncodedHeight = 81;
  EncodedImage encoded_image;
  encoded_image._encodedWidth = kEncodedWidth;
  encoded_image._encodedHeight = kEncodedHeight;

  CodecSpecificInfo codec_info;
  codec_info.codecType = kVideoCodecVP8;
  codec_info.codecSpecific.VP8.simulcastIdx = 0;

  statistics_proxy_->OnSendEncodedImage(encoded_image, &codec_info);
  codec_info.codecSpecific.VP8.simulcastIdx = 1;
  statistics_proxy_->OnSendEncodedImage(encoded_image, &codec_info);

  VideoSendStream::Stats stats = statistics_proxy_->GetStats();
  EXPECT_EQ(kEncodedWidth, stats.substreams[config_.rtp.ssrcs[0]].width);
  EXPECT_EQ(kEncodedHeight, stats.substreams[config_.rtp.ssrcs[0]].height);
  EXPECT_EQ(kEncodedWidth, stats.substreams[config_.rtp.ssrcs[1]].width);
  EXPECT_EQ(kEncodedHeight, stats.substreams[config_.rtp.ssrcs[1]].height);

  // Forward almost to timeout, this should not have removed stats.
  fake_clock_.AdvanceTimeMilliseconds(SendStatisticsProxy::kStatsTimeoutMs - 1);
  stats = statistics_proxy_->GetStats();
  EXPECT_EQ(kEncodedWidth, stats.substreams[config_.rtp.ssrcs[0]].width);
  EXPECT_EQ(kEncodedHeight, stats.substreams[config_.rtp.ssrcs[0]].height);

  // Update the first SSRC with bogus RTCP stats to make sure that encoded
  // resolution still times out (no global timeout for all stats).
  RtcpStatistics rtcp_statistics;
  RtcpStatisticsCallback* rtcp_stats = statistics_proxy_.get();
  rtcp_stats->StatisticsUpdated(rtcp_statistics, config_.rtp.ssrcs[0]);

  // Report stats for second SSRC to make sure it's not outdated along with the
  // first SSRC.
  codec_info.codecSpecific.VP8.simulcastIdx = 1;
  statistics_proxy_->OnSendEncodedImage(encoded_image, &codec_info);

  // Forward 1 ms, reach timeout, substream 0 should have no resolution
  // reported, but substream 1 should.
  fake_clock_.AdvanceTimeMilliseconds(1);
  stats = statistics_proxy_->GetStats();
  EXPECT_EQ(0, stats.substreams[config_.rtp.ssrcs[0]].width);
  EXPECT_EQ(0, stats.substreams[config_.rtp.ssrcs[0]].height);
  EXPECT_EQ(kEncodedWidth, stats.substreams[config_.rtp.ssrcs[1]].width);
  EXPECT_EQ(kEncodedHeight, stats.substreams[config_.rtp.ssrcs[1]].height);
}

TEST_F(SendStatisticsProxyTest, ClearsResolutionFromInactiveSsrcs) {
  static const int kEncodedWidth = 123;
  static const int kEncodedHeight = 81;
  EncodedImage encoded_image;
  encoded_image._encodedWidth = kEncodedWidth;
  encoded_image._encodedHeight = kEncodedHeight;

  CodecSpecificInfo codec_info;
  codec_info.codecType = kVideoCodecVP8;
  codec_info.codecSpecific.VP8.simulcastIdx = 0;

  statistics_proxy_->OnSendEncodedImage(encoded_image, &codec_info);
  codec_info.codecSpecific.VP8.simulcastIdx = 1;
  statistics_proxy_->OnSendEncodedImage(encoded_image, &codec_info);

  statistics_proxy_->OnInactiveSsrc(config_.rtp.ssrcs[1]);
  VideoSendStream::Stats stats = statistics_proxy_->GetStats();
  EXPECT_EQ(kEncodedWidth, stats.substreams[config_.rtp.ssrcs[0]].width);
  EXPECT_EQ(kEncodedHeight, stats.substreams[config_.rtp.ssrcs[0]].height);
  EXPECT_EQ(0, stats.substreams[config_.rtp.ssrcs[1]].width);
  EXPECT_EQ(0, stats.substreams[config_.rtp.ssrcs[1]].height);
}

TEST_F(SendStatisticsProxyTest, ClearsBitratesFromInactiveSsrcs) {
  uint32_t bitrate = 42;
  BitrateStatisticsObserver* observer = statistics_proxy_.get();
  observer->Notify(bitrate, bitrate, config_.rtp.ssrcs[0]);
  observer->Notify(bitrate, bitrate, config_.rtp.ssrcs[1]);

  statistics_proxy_->OnInactiveSsrc(config_.rtp.ssrcs[1]);

  VideoSendStream::Stats stats = statistics_proxy_->GetStats();
  EXPECT_EQ(static_cast<int>(bitrate),
            stats.substreams[config_.rtp.ssrcs[0]].total_bitrate_bps);
  EXPECT_EQ(static_cast<int>(bitrate),
            stats.substreams[config_.rtp.ssrcs[0]].retransmit_bitrate_bps);
  EXPECT_EQ(0, stats.substreams[config_.rtp.ssrcs[1]].total_bitrate_bps);
  EXPECT_EQ(0, stats.substreams[config_.rtp.ssrcs[1]].retransmit_bitrate_bps);
}

TEST_F(SendStatisticsProxyTest, ResetsRtcpCountersOnContentChange) {
  RtcpPacketTypeCounterObserver* proxy =
      static_cast<RtcpPacketTypeCounterObserver*>(statistics_proxy_.get());
  RtcpPacketTypeCounter counters;
  counters.first_packet_time_ms = fake_clock_.TimeInMilliseconds();
  proxy->RtcpPacketTypesCounterUpdated(kFirstSsrc, counters);
  proxy->RtcpPacketTypesCounterUpdated(kSecondSsrc, counters);

  fake_clock_.AdvanceTimeMilliseconds(1000 * metrics::kMinRunTimeInSeconds);

  counters.nack_packets += 1 * metrics::kMinRunTimeInSeconds;
  counters.fir_packets += 2 * metrics::kMinRunTimeInSeconds;
  counters.pli_packets += 3 * metrics::kMinRunTimeInSeconds;
  counters.unique_nack_requests += 4 * metrics::kMinRunTimeInSeconds;
  counters.nack_requests += 5 * metrics::kMinRunTimeInSeconds;

  proxy->RtcpPacketTypesCounterUpdated(kFirstSsrc, counters);
  proxy->RtcpPacketTypesCounterUpdated(kSecondSsrc, counters);

  // Changing content type causes histograms to be reported.
  VideoEncoderConfig config;
  config.content_type = VideoEncoderConfig::ContentType::kScreen;
  statistics_proxy_->OnEncoderReconfigured(config, 50);

  EXPECT_EQ(1,
            metrics::NumSamples("WebRTC.Video.NackPacketsReceivedPerMinute"));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.FirPacketsReceivedPerMinute"));
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.PliPacketsReceivedPerMinute"));
  EXPECT_EQ(1, metrics::NumSamples(
                   "WebRTC.Video.UniqueNackRequestsReceivedInPercent"));

  const int kRate = 60 * 2;  // Packets per minute with two streams.

  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.NackPacketsReceivedPerMinute",
                                  1 * kRate));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.FirPacketsReceivedPerMinute",
                                  2 * kRate));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.PliPacketsReceivedPerMinute",
                                  3 * kRate));
  EXPECT_EQ(
      1, metrics::NumEvents("WebRTC.Video.UniqueNackRequestsReceivedInPercent",
                            4 * 100 / 5));

  // New start time but same counter values.
  proxy->RtcpPacketTypesCounterUpdated(kFirstSsrc, counters);
  proxy->RtcpPacketTypesCounterUpdated(kSecondSsrc, counters);

  fake_clock_.AdvanceTimeMilliseconds(1000 * metrics::kMinRunTimeInSeconds);

  counters.nack_packets += 1 * metrics::kMinRunTimeInSeconds;
  counters.fir_packets += 2 * metrics::kMinRunTimeInSeconds;
  counters.pli_packets += 3 * metrics::kMinRunTimeInSeconds;
  counters.unique_nack_requests += 4 * metrics::kMinRunTimeInSeconds;
  counters.nack_requests += 5 * metrics::kMinRunTimeInSeconds;

  proxy->RtcpPacketTypesCounterUpdated(kFirstSsrc, counters);
  proxy->RtcpPacketTypesCounterUpdated(kSecondSsrc, counters);

  SetUp();  // Reset stats proxy also causes histograms to be reported.

  EXPECT_EQ(1, metrics::NumSamples(
                   "WebRTC.Video.Screenshare.NackPacketsReceivedPerMinute"));
  EXPECT_EQ(1, metrics::NumSamples(
                   "WebRTC.Video.Screenshare.FirPacketsReceivedPerMinute"));
  EXPECT_EQ(1, metrics::NumSamples(
                   "WebRTC.Video.Screenshare.PliPacketsReceivedPerMinute"));
  EXPECT_EQ(
      1, metrics::NumSamples(
             "WebRTC.Video.Screenshare.UniqueNackRequestsReceivedInPercent"));

  EXPECT_EQ(1, metrics::NumEvents(
                   "WebRTC.Video.Screenshare.NackPacketsReceivedPerMinute",
                   1 * kRate));
  EXPECT_EQ(1, metrics::NumEvents(
                   "WebRTC.Video.Screenshare.FirPacketsReceivedPerMinute",
                   2 * kRate));
  EXPECT_EQ(1, metrics::NumEvents(
                   "WebRTC.Video.Screenshare.PliPacketsReceivedPerMinute",
                   3 * kRate));
  EXPECT_EQ(1,
            metrics::NumEvents(
                "WebRTC.Video.Screenshare.UniqueNackRequestsReceivedInPercent",
                4 * 100 / 5));
}

TEST_F(SendStatisticsProxyTest, GetStatsReportsIsFlexFec) {
  statistics_proxy_.reset(
      new SendStatisticsProxy(&fake_clock_, GetTestConfigWithFlexFec(),
                              VideoEncoderConfig::ContentType::kRealtimeVideo));

  StreamDataCountersCallback* proxy =
      static_cast<StreamDataCountersCallback*>(statistics_proxy_.get());
  StreamDataCounters counters;
  proxy->DataCountersUpdated(counters, kFirstSsrc);
  proxy->DataCountersUpdated(counters, kFlexFecSsrc);

  EXPECT_FALSE(GetStreamStats(kFirstSsrc).is_flexfec);
  EXPECT_TRUE(GetStreamStats(kFlexFecSsrc).is_flexfec);
}

TEST_F(SendStatisticsProxyTest, SendBitratesAreReportedWithFlexFecEnabled) {
  statistics_proxy_.reset(
      new SendStatisticsProxy(&fake_clock_, GetTestConfigWithFlexFec(),
                              VideoEncoderConfig::ContentType::kRealtimeVideo));

  StreamDataCountersCallback* proxy =
      static_cast<StreamDataCountersCallback*>(statistics_proxy_.get());
  StreamDataCounters counters;
  StreamDataCounters rtx_counters;

  const int kMinRequiredPeriodSamples = 8;
  const int kPeriodIntervalMs = 2000;
  for (int i = 0; i < kMinRequiredPeriodSamples; ++i) {
    counters.transmitted.packets += 20;
    counters.transmitted.header_bytes += 500;
    counters.transmitted.padding_bytes += 1000;
    counters.transmitted.payload_bytes += 2000;
    counters.retransmitted.packets += 2;
    counters.retransmitted.header_bytes += 25;
    counters.retransmitted.padding_bytes += 100;
    counters.retransmitted.payload_bytes += 250;
    counters.fec = counters.retransmitted;
    rtx_counters.transmitted = counters.transmitted;
    // Advance one interval and update counters.
    fake_clock_.AdvanceTimeMilliseconds(kPeriodIntervalMs);
    proxy->DataCountersUpdated(counters, kFirstSsrc);
    proxy->DataCountersUpdated(counters, kSecondSsrc);
    proxy->DataCountersUpdated(rtx_counters, kFirstRtxSsrc);
    proxy->DataCountersUpdated(rtx_counters, kSecondRtxSsrc);
    proxy->DataCountersUpdated(counters, kFlexFecSsrc);
  }

  statistics_proxy_.reset();
  // Interval: 3500 bytes * 4 / 2 sec = 7000 bytes / sec  = 56 kbps
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.BitrateSentInKbps"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.BitrateSentInKbps", 56));
  // Interval: 3500 bytes * 2 / 2 sec = 3500 bytes / sec  = 28 kbps
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.RtxBitrateSentInKbps"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.RtxBitrateSentInKbps", 28));
  // Interval: (2000 - 2 * 250) bytes / 2 sec = 1500 bytes / sec  = 12 kbps
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.MediaBitrateSentInKbps"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.MediaBitrateSentInKbps", 12));
  // Interval: 1000 bytes * 4 / 2 sec = 2000 bytes / sec = 16 kbps
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.PaddingBitrateSentInKbps"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.PaddingBitrateSentInKbps", 16));
  // Interval: 375 bytes * 2 / 2 sec = 375 bytes / sec = 3 kbps
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.FecBitrateSentInKbps"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.FecBitrateSentInKbps", 3));
  // Interval: 375 bytes * 2 / 2 sec = 375 bytes / sec = 3 kbps
  EXPECT_EQ(1,
            metrics::NumSamples("WebRTC.Video.RetransmittedBitrateSentInKbps"));
  EXPECT_EQ(
      1, metrics::NumEvents("WebRTC.Video.RetransmittedBitrateSentInKbps", 3));
}

TEST_F(SendStatisticsProxyTest, ResetsRtpCountersOnContentChange) {
  StreamDataCountersCallback* proxy =
      static_cast<StreamDataCountersCallback*>(statistics_proxy_.get());
  StreamDataCounters counters;
  StreamDataCounters rtx_counters;
  counters.first_packet_time_ms = fake_clock_.TimeInMilliseconds();

  const int kMinRequiredPeriodSamples = 8;
  const int kPeriodIntervalMs = 2000;
  for (int i = 0; i < kMinRequiredPeriodSamples; ++i) {
    counters.transmitted.packets += 20;
    counters.transmitted.header_bytes += 500;
    counters.transmitted.padding_bytes += 1000;
    counters.transmitted.payload_bytes += 2000;
    counters.retransmitted.packets += 2;
    counters.retransmitted.header_bytes += 25;
    counters.retransmitted.padding_bytes += 100;
    counters.retransmitted.payload_bytes += 250;
    counters.fec = counters.retransmitted;
    rtx_counters.transmitted = counters.transmitted;
    // Advance one interval and update counters.
    fake_clock_.AdvanceTimeMilliseconds(kPeriodIntervalMs);
    proxy->DataCountersUpdated(counters, kFirstSsrc);
    proxy->DataCountersUpdated(counters, kSecondSsrc);
    proxy->DataCountersUpdated(rtx_counters, kFirstRtxSsrc);
    proxy->DataCountersUpdated(rtx_counters, kSecondRtxSsrc);
  }

  // Changing content type causes histograms to be reported.
  VideoEncoderConfig config;
  config.content_type = VideoEncoderConfig::ContentType::kScreen;
  statistics_proxy_->OnEncoderReconfigured(config, 50000);

  // Interval: 3500 bytes * 4 / 2 sec = 7000 bytes / sec  = 56 kbps
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.BitrateSentInKbps"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.BitrateSentInKbps", 56));
  // Interval: 3500 bytes * 2 / 2 sec = 3500 bytes / sec  = 28 kbps
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.RtxBitrateSentInKbps"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.RtxBitrateSentInKbps", 28));
  // Interval: (2000 - 2 * 250) bytes / 2 sec = 1500 bytes / sec  = 12 kbps
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.MediaBitrateSentInKbps"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.MediaBitrateSentInKbps", 12));
  // Interval: 1000 bytes * 4 / 2 sec = 2000 bytes / sec = 16 kbps
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.PaddingBitrateSentInKbps"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.PaddingBitrateSentInKbps", 16));
  // Interval: 375 bytes * 2 / 2 sec = 375 bytes / sec = 3 kbps
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.FecBitrateSentInKbps"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.FecBitrateSentInKbps", 3));
  // Interval: 375 bytes * 2 / 2 sec = 375 bytes / sec = 3 kbps
  EXPECT_EQ(1,
            metrics::NumSamples("WebRTC.Video.RetransmittedBitrateSentInKbps"));
  EXPECT_EQ(
      1, metrics::NumEvents("WebRTC.Video.RetransmittedBitrateSentInKbps", 3));

  // New metric counters but same data counters.
  // Double counter values, this should result in the same counts as before but
  // with new histogram names.
  for (int i = 0; i < kMinRequiredPeriodSamples; ++i) {
    counters.transmitted.packets += 20;
    counters.transmitted.header_bytes += 500;
    counters.transmitted.padding_bytes += 1000;
    counters.transmitted.payload_bytes += 2000;
    counters.retransmitted.packets += 2;
    counters.retransmitted.header_bytes += 25;
    counters.retransmitted.padding_bytes += 100;
    counters.retransmitted.payload_bytes += 250;
    counters.fec = counters.retransmitted;
    rtx_counters.transmitted = counters.transmitted;
    // Advance one interval and update counters.
    fake_clock_.AdvanceTimeMilliseconds(kPeriodIntervalMs);
    proxy->DataCountersUpdated(counters, kFirstSsrc);
    proxy->DataCountersUpdated(counters, kSecondSsrc);
    proxy->DataCountersUpdated(rtx_counters, kFirstRtxSsrc);
    proxy->DataCountersUpdated(rtx_counters, kSecondRtxSsrc);
  }

  // Reset stats proxy also causes histograms to be reported.
  statistics_proxy_.reset();

  // Interval: 3500 bytes * 4 / 2 sec = 7000 bytes / sec  = 56 kbps
  EXPECT_EQ(1,
            metrics::NumSamples("WebRTC.Video.Screenshare.BitrateSentInKbps"));
  EXPECT_EQ(
      1, metrics::NumEvents("WebRTC.Video.Screenshare.BitrateSentInKbps", 56));
  // Interval: 3500 bytes * 2 / 2 sec = 3500 bytes / sec  = 28 kbps
  EXPECT_EQ(
      1, metrics::NumSamples("WebRTC.Video.Screenshare.RtxBitrateSentInKbps"));
  EXPECT_EQ(1, metrics::NumEvents(
                   "WebRTC.Video.Screenshare.RtxBitrateSentInKbps", 28));
  // Interval: (2000 - 2 * 250) bytes / 2 sec = 1500 bytes / sec  = 12 kbps
  EXPECT_EQ(1, metrics::NumSamples(
                   "WebRTC.Video.Screenshare.MediaBitrateSentInKbps"));
  EXPECT_EQ(1, metrics::NumEvents(
                   "WebRTC.Video.Screenshare.MediaBitrateSentInKbps", 12));
  // Interval: 1000 bytes * 4 / 2 sec = 2000 bytes / sec = 16 kbps
  EXPECT_EQ(1, metrics::NumSamples(
                   "WebRTC.Video.Screenshare.PaddingBitrateSentInKbps"));
  EXPECT_EQ(1, metrics::NumEvents(
                   "WebRTC.Video.Screenshare.PaddingBitrateSentInKbps", 16));
  // Interval: 375 bytes * 2 / 2 sec = 375 bytes / sec = 3 kbps
  EXPECT_EQ(
      1, metrics::NumSamples("WebRTC.Video.Screenshare.FecBitrateSentInKbps"));
  EXPECT_EQ(1, metrics::NumEvents(
                   "WebRTC.Video.Screenshare.FecBitrateSentInKbps", 3));
  // Interval: 375 bytes * 2 / 2 sec = 375 bytes / sec = 3 kbps
  EXPECT_EQ(1, metrics::NumSamples(
                   "WebRTC.Video.Screenshare.RetransmittedBitrateSentInKbps"));
  EXPECT_EQ(1,
            metrics::NumEvents(
                "WebRTC.Video.Screenshare.RetransmittedBitrateSentInKbps", 3));
}

TEST_F(SendStatisticsProxyTest, RtxBitrateIsZeroWhenEnabledAndNoRtxDataIsSent) {
  StreamDataCountersCallback* proxy =
      static_cast<StreamDataCountersCallback*>(statistics_proxy_.get());
  StreamDataCounters counters;
  StreamDataCounters rtx_counters;

  const int kMinRequiredPeriodSamples = 8;
  const int kPeriodIntervalMs = 2000;
  for (int i = 0; i < kMinRequiredPeriodSamples; ++i) {
    counters.transmitted.packets += 20;
    counters.transmitted.header_bytes += 500;
    counters.transmitted.payload_bytes += 2000;
    counters.fec = counters.retransmitted;
    // Advance one interval and update counters.
    fake_clock_.AdvanceTimeMilliseconds(kPeriodIntervalMs);
    proxy->DataCountersUpdated(counters, kFirstSsrc);
  }

  // RTX enabled. No data sent over RTX.
  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.RtxBitrateSentInKbps"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.RtxBitrateSentInKbps", 0));
}

TEST_F(SendStatisticsProxyTest, RtxBitrateNotReportedWhenNotEnabled) {
  VideoSendStream::Config config(nullptr);
  config.rtp.ssrcs.push_back(kFirstSsrc);  // RTX not configured.
  statistics_proxy_.reset(new SendStatisticsProxy(
      &fake_clock_, config, VideoEncoderConfig::ContentType::kRealtimeVideo));

  StreamDataCountersCallback* proxy =
      static_cast<StreamDataCountersCallback*>(statistics_proxy_.get());
  StreamDataCounters counters;

  const int kMinRequiredPeriodSamples = 8;
  const int kPeriodIntervalMs = 2000;
  for (int i = 0; i < kMinRequiredPeriodSamples; ++i) {
    counters.transmitted.packets += 20;
    counters.transmitted.header_bytes += 500;
    counters.transmitted.payload_bytes += 2000;
    counters.fec = counters.retransmitted;
    // Advance one interval and update counters.
    fake_clock_.AdvanceTimeMilliseconds(kPeriodIntervalMs);
    proxy->DataCountersUpdated(counters, kFirstSsrc);
  }

  // RTX not enabled.
  statistics_proxy_.reset();
  EXPECT_EQ(0, metrics::NumSamples("WebRTC.Video.RtxBitrateSentInKbps"));
}

TEST_F(SendStatisticsProxyTest, FecBitrateIsZeroWhenEnabledAndNoFecDataIsSent) {
  StreamDataCountersCallback* proxy =
      static_cast<StreamDataCountersCallback*>(statistics_proxy_.get());
  StreamDataCounters counters;
  StreamDataCounters rtx_counters;

  const int kMinRequiredPeriodSamples = 8;
  const int kPeriodIntervalMs = 2000;
  for (int i = 0; i < kMinRequiredPeriodSamples; ++i) {
    counters.transmitted.packets += 20;
    counters.transmitted.header_bytes += 500;
    counters.transmitted.payload_bytes += 2000;
    // Advance one interval and update counters.
    fake_clock_.AdvanceTimeMilliseconds(kPeriodIntervalMs);
    proxy->DataCountersUpdated(counters, kFirstSsrc);
  }

  // FEC enabled. No FEC data sent.
  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.FecBitrateSentInKbps"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.FecBitrateSentInKbps", 0));
}

TEST_F(SendStatisticsProxyTest, FecBitrateNotReportedWhenNotEnabled) {
  VideoSendStream::Config config(nullptr);
  config.rtp.ssrcs.push_back(kFirstSsrc);  // FEC not configured.
  statistics_proxy_.reset(new SendStatisticsProxy(
      &fake_clock_, config, VideoEncoderConfig::ContentType::kRealtimeVideo));

  StreamDataCountersCallback* proxy =
      static_cast<StreamDataCountersCallback*>(statistics_proxy_.get());
  StreamDataCounters counters;

  const int kMinRequiredPeriodSamples = 8;
  const int kPeriodIntervalMs = 2000;
  for (int i = 0; i < kMinRequiredPeriodSamples; ++i) {
    counters.transmitted.packets += 20;
    counters.transmitted.header_bytes += 500;
    counters.transmitted.payload_bytes += 2000;
    counters.fec = counters.retransmitted;
    // Advance one interval and update counters.
    fake_clock_.AdvanceTimeMilliseconds(kPeriodIntervalMs);
    proxy->DataCountersUpdated(counters, kFirstSsrc);
  }

  // FEC not enabled.
  statistics_proxy_.reset();
  EXPECT_EQ(0, metrics::NumSamples("WebRTC.Video.FecBitrateSentInKbps"));
}

TEST_F(SendStatisticsProxyTest, GetStatsReportsEncoderImplementationName) {
  const char* kName = "encoderName";
  EncodedImage encoded_image;
  CodecSpecificInfo codec_info;
  codec_info.codec_name = kName;
  statistics_proxy_->OnSendEncodedImage(encoded_image, &codec_info);
  EXPECT_STREQ(
      kName, statistics_proxy_->GetStats().encoder_implementation_name.c_str());
}

class ForcedFallbackTest : public SendStatisticsProxyTest {
 public:
  explicit ForcedFallbackTest(const std::string& field_trials)
      : SendStatisticsProxyTest(field_trials) {
    codec_info_.codecType = kVideoCodecVP8;
    codec_info_.codecSpecific.VP8.simulcastIdx = 0;
    codec_info_.codecSpecific.VP8.temporalIdx = 0;
    codec_info_.codec_name = "fake_codec";
    encoded_image_._encodedWidth = kWidth;
    encoded_image_._encodedHeight = kHeight;
  }

  ~ForcedFallbackTest() override {}

 protected:
  void InsertEncodedFrames(int num_frames, int interval_ms) {
    // First frame is not updating stats, insert initial frame.
    if (statistics_proxy_->GetStats().frames_encoded == 0) {
      statistics_proxy_->OnSendEncodedImage(encoded_image_, &codec_info_);
    }
    for (int i = 0; i < num_frames; ++i) {
      statistics_proxy_->OnSendEncodedImage(encoded_image_, &codec_info_);
      fake_clock_.AdvanceTimeMilliseconds(interval_ms);
    }
    // Add frame to include last time interval.
    statistics_proxy_->OnSendEncodedImage(encoded_image_, &codec_info_);
  }

  EncodedImage encoded_image_;
  CodecSpecificInfo codec_info_;
  const std::string kPrefix = "WebRTC.Video.Encoder.ForcedSw";
  const int kFrameIntervalMs = 1000;
  const int kMinFrames = 20;  // Min run time 20 sec.
};

class ForcedFallbackDisabled : public ForcedFallbackTest {
 public:
  ForcedFallbackDisabled()
      : ForcedFallbackTest("WebRTC-VP8-Forced-Fallback-Encoder-v2/Disabled-1," +
                           std::to_string(kWidth * kHeight) + ",3/") {}
};

class ForcedFallbackEnabled : public ForcedFallbackTest {
 public:
  ForcedFallbackEnabled()
      : ForcedFallbackTest("WebRTC-VP8-Forced-Fallback-Encoder-v2/Enabled-1," +
                           std::to_string(kWidth * kHeight) + ",3/") {}
};

TEST_F(ForcedFallbackEnabled, StatsNotUpdatedIfMinRunTimeHasNotPassed) {
  InsertEncodedFrames(kMinFrames, kFrameIntervalMs - 1);
  statistics_proxy_.reset();
  EXPECT_EQ(0, metrics::NumSamples(kPrefix + "FallbackTimeInPercent.Vp8"));
  EXPECT_EQ(0, metrics::NumSamples(kPrefix + "FallbackChangesPerMinute.Vp8"));
}

TEST_F(ForcedFallbackEnabled, StatsUpdated) {
  InsertEncodedFrames(kMinFrames, kFrameIntervalMs);
  EXPECT_FALSE(statistics_proxy_->GetStats().has_entered_low_resolution);
  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples(kPrefix + "FallbackTimeInPercent.Vp8"));
  EXPECT_EQ(1, metrics::NumEvents(kPrefix + "FallbackTimeInPercent.Vp8", 0));
  EXPECT_EQ(1, metrics::NumSamples(kPrefix + "FallbackChangesPerMinute.Vp8"));
  EXPECT_EQ(1, metrics::NumEvents(kPrefix + "FallbackChangesPerMinute.Vp8", 0));
}

TEST_F(ForcedFallbackEnabled, StatsNotUpdatedIfNotVp8) {
  codec_info_.codecType = kVideoCodecVP9;
  InsertEncodedFrames(kMinFrames, kFrameIntervalMs);
  statistics_proxy_.reset();
  EXPECT_EQ(0, metrics::NumSamples(kPrefix + "FallbackTimeInPercent.Vp8"));
  EXPECT_EQ(0, metrics::NumSamples(kPrefix + "FallbackChangesPerMinute.Vp8"));
}

TEST_F(ForcedFallbackEnabled, StatsNotUpdatedForTemporalLayers) {
  codec_info_.codecSpecific.VP8.temporalIdx = 1;
  InsertEncodedFrames(kMinFrames, kFrameIntervalMs);
  statistics_proxy_.reset();
  EXPECT_EQ(0, metrics::NumSamples(kPrefix + "FallbackTimeInPercent.Vp8"));
  EXPECT_EQ(0, metrics::NumSamples(kPrefix + "FallbackChangesPerMinute.Vp8"));
}

TEST_F(ForcedFallbackEnabled, StatsNotUpdatedForSimulcast) {
  codec_info_.codecSpecific.VP8.simulcastIdx = 1;
  InsertEncodedFrames(kMinFrames, kFrameIntervalMs);
  statistics_proxy_.reset();
  EXPECT_EQ(0, metrics::NumSamples(kPrefix + "FallbackTimeInPercent.Vp8"));
  EXPECT_EQ(0, metrics::NumSamples(kPrefix + "FallbackChangesPerMinute.Vp8"));
}

TEST_F(ForcedFallbackDisabled, StatsNotUpdatedIfNoFieldTrial) {
  InsertEncodedFrames(kMinFrames, kFrameIntervalMs);
  statistics_proxy_.reset();
  EXPECT_EQ(0, metrics::NumSamples(kPrefix + "FallbackTimeInPercent.Vp8"));
  EXPECT_EQ(0, metrics::NumSamples(kPrefix + "FallbackChangesPerMinute.Vp8"));
}

TEST_F(ForcedFallbackDisabled, EnteredLowResolutionSetIfAtMaxPixels) {
  InsertEncodedFrames(1, kFrameIntervalMs);
  EXPECT_TRUE(statistics_proxy_->GetStats().has_entered_low_resolution);
}

TEST_F(ForcedFallbackEnabled, EnteredLowResolutionNotSetIfNotLibvpx) {
  InsertEncodedFrames(1, kFrameIntervalMs);
  EXPECT_FALSE(statistics_proxy_->GetStats().has_entered_low_resolution);
}

TEST_F(ForcedFallbackEnabled, EnteredLowResolutionSetIfLibvpx) {
  codec_info_.codec_name = "libvpx";
  InsertEncodedFrames(1, kFrameIntervalMs);
  EXPECT_TRUE(statistics_proxy_->GetStats().has_entered_low_resolution);
}

TEST_F(ForcedFallbackDisabled, EnteredLowResolutionNotSetIfAboveMaxPixels) {
  encoded_image_._encodedWidth = kWidth + 1;
  InsertEncodedFrames(1, kFrameIntervalMs);
  EXPECT_FALSE(statistics_proxy_->GetStats().has_entered_low_resolution);
}

TEST_F(ForcedFallbackDisabled, EnteredLowResolutionNotSetIfLibvpx) {
  codec_info_.codec_name = "libvpx";
  InsertEncodedFrames(1, kFrameIntervalMs);
  EXPECT_FALSE(statistics_proxy_->GetStats().has_entered_low_resolution);
}

TEST_F(ForcedFallbackDisabled,
       EnteredLowResolutionSetIfOnMinPixelLimitReached) {
  encoded_image_._encodedWidth = kWidth + 1;
  statistics_proxy_->OnMinPixelLimitReached();
  InsertEncodedFrames(1, kFrameIntervalMs);
  EXPECT_TRUE(statistics_proxy_->GetStats().has_entered_low_resolution);
}

TEST_F(ForcedFallbackEnabled, OneFallbackEvent) {
  // One change. Video: 20000 ms, fallback: 5000 ms (25%).
  EXPECT_FALSE(statistics_proxy_->GetStats().has_entered_low_resolution);
  InsertEncodedFrames(15, 1000);
  EXPECT_FALSE(statistics_proxy_->GetStats().has_entered_low_resolution);
  codec_info_.codec_name = "libvpx";
  InsertEncodedFrames(5, 1000);
  EXPECT_TRUE(statistics_proxy_->GetStats().has_entered_low_resolution);

  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples(kPrefix + "FallbackTimeInPercent.Vp8"));
  EXPECT_EQ(1, metrics::NumEvents(kPrefix + "FallbackTimeInPercent.Vp8", 25));
  EXPECT_EQ(1, metrics::NumSamples(kPrefix + "FallbackChangesPerMinute.Vp8"));
  EXPECT_EQ(1, metrics::NumEvents(kPrefix + "FallbackChangesPerMinute.Vp8", 3));
}

TEST_F(ForcedFallbackEnabled, ThreeFallbackEvents) {
  codec_info_.codecSpecific.VP8.temporalIdx = kNoTemporalIdx;  // Should work.
  const int kMaxFrameDiffMs = 2000;

  // Three changes. Video: 60000 ms, fallback: 15000 ms (25%).
  InsertEncodedFrames(10, 1000);
  EXPECT_FALSE(statistics_proxy_->GetStats().has_entered_low_resolution);
  codec_info_.codec_name = "libvpx";
  InsertEncodedFrames(15, 500);
  EXPECT_TRUE(statistics_proxy_->GetStats().has_entered_low_resolution);
  codec_info_.codec_name = "notlibvpx";
  InsertEncodedFrames(20, 1000);
  InsertEncodedFrames(3, kMaxFrameDiffMs);  // Should not be included.
  InsertEncodedFrames(10, 1000);
  EXPECT_TRUE(statistics_proxy_->GetStats().has_entered_low_resolution);
  codec_info_.codec_name = "notlibvpx2";
  InsertEncodedFrames(10, 500);
  EXPECT_TRUE(statistics_proxy_->GetStats().has_entered_low_resolution);
  codec_info_.codec_name = "libvpx";
  InsertEncodedFrames(15, 500);
  EXPECT_TRUE(statistics_proxy_->GetStats().has_entered_low_resolution);

  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples(kPrefix + "FallbackTimeInPercent.Vp8"));
  EXPECT_EQ(1, metrics::NumEvents(kPrefix + "FallbackTimeInPercent.Vp8", 25));
  EXPECT_EQ(1, metrics::NumSamples(kPrefix + "FallbackChangesPerMinute.Vp8"));
  EXPECT_EQ(1, metrics::NumEvents(kPrefix + "FallbackChangesPerMinute.Vp8", 3));
}

TEST_F(ForcedFallbackEnabled, NoFallbackIfAboveMaxPixels) {
  encoded_image_._encodedWidth = kWidth + 1;
  codec_info_.codec_name = "libvpx";
  InsertEncodedFrames(kMinFrames, kFrameIntervalMs);

  EXPECT_FALSE(statistics_proxy_->GetStats().has_entered_low_resolution);
  statistics_proxy_.reset();
  EXPECT_EQ(0, metrics::NumSamples(kPrefix + "FallbackTimeInPercent.Vp8"));
  EXPECT_EQ(0, metrics::NumSamples(kPrefix + "FallbackChangesPerMinute.Vp8"));
}

TEST_F(ForcedFallbackEnabled, FallbackIfAtMaxPixels) {
  encoded_image_._encodedWidth = kWidth;
  codec_info_.codec_name = "libvpx";
  InsertEncodedFrames(kMinFrames, kFrameIntervalMs);

  EXPECT_TRUE(statistics_proxy_->GetStats().has_entered_low_resolution);
  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples(kPrefix + "FallbackTimeInPercent.Vp8"));
  EXPECT_EQ(1, metrics::NumSamples(kPrefix + "FallbackChangesPerMinute.Vp8"));
}

}  // namespace webrtc
