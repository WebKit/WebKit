/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/send_statistics_proxy.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "webrtc/system_wrappers/include/metrics.h"
#include "webrtc/system_wrappers/include/metrics_default.h"
#include "webrtc/test/gtest.h"

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
  SendStatisticsProxyTest()
      : fake_clock_(1234), config_(GetTestConfig()), avg_delay_ms_(0),
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
      EXPECT_EQ(a.rtcp_stats.cumulative_lost, b.rtcp_stats.cumulative_lost);
      EXPECT_EQ(a.rtcp_stats.extended_max_sequence_number,
                b.rtcp_stats.extended_max_sequence_number);
      EXPECT_EQ(a.rtcp_stats.jitter, b.rtcp_stats.jitter);
    }
  }

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
    ssrc_stats.rtcp_stats.cumulative_lost = offset;
    ssrc_stats.rtcp_stats.extended_max_sequence_number = offset + 1;
    ssrc_stats.rtcp_stats.fraction_lost = offset + 2;
    ssrc_stats.rtcp_stats.jitter = offset + 3;
    callback->StatisticsUpdated(ssrc_stats.rtcp_stats, ssrc);
  }
  for (const auto& ssrc : config_.rtp.rtx.ssrcs) {
    VideoSendStream::StreamStats& ssrc_stats = expected_.substreams[ssrc];

    // Add statistics with some arbitrary, but unique, numbers.
    uint32_t offset = ssrc * sizeof(RtcpStatistics);
    ssrc_stats.rtcp_stats.cumulative_lost = offset;
    ssrc_stats.rtcp_stats.extended_max_sequence_number = offset + 1;
    ssrc_stats.rtcp_stats.fraction_lost = offset + 2;
    ssrc_stats.rtcp_stats.jitter = offset + 3;
    callback->StatisticsUpdated(ssrc_stats.rtcp_stats, ssrc);
  }
  VideoSendStream::Stats stats = statistics_proxy_->GetStats();
  ExpectEqual(expected_, stats);
}

TEST_F(SendStatisticsProxyTest, EncodedBitrateAndFramerate) {
  int media_bitrate_bps = 500;
  int encode_fps = 29;

  statistics_proxy_->OnEncoderStatsUpdate(encode_fps, media_bitrate_bps);

  VideoSendStream::Stats stats = statistics_proxy_->GetStats();
  EXPECT_EQ(media_bitrate_bps, stats.media_bitrate_bps);
  EXPECT_EQ(encode_fps, stats.encode_frame_rate);
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
  EncodedImage encoded_image;
  encoded_image._encodedWidth = kWidth;
  encoded_image._encodedHeight = kHeight;
  for (int i = 0; i <= SendStatisticsProxy::kMinRequiredMetricsSamples; ++i) {
    encoded_image._timeStamp = i + 1;
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
  for (int i = 0; i <= frames; ++i) {
    fake_clock_.AdvanceTimeMilliseconds(1000 / kFps);
    encoded_image._timeStamp = i + 1;
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
  for (int i = 0; i <= frames; ++i) {
    fake_clock_.AdvanceTimeMilliseconds(1000 / kFps);
    encoded_image._timeStamp = i + 1;
    statistics_proxy_->OnSendEncodedImage(encoded_image, nullptr);
  }
  // Suspend.
  statistics_proxy_->OnSuspendChange(true);
  fake_clock_.AdvanceTimeMilliseconds(kSuspendTimeMs);

  for (int i = 0; i <= frames; ++i) {
    fake_clock_.AdvanceTimeMilliseconds(1000 / kFps);
    encoded_image._timeStamp = i + 1;
    statistics_proxy_->OnSendEncodedImage(encoded_image, nullptr);
  }
  // Suspended time interval should not affect the framerate.
  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.SentFramesPerSecond"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.SentFramesPerSecond", kFps));
}

TEST_F(SendStatisticsProxyTest, CpuLimitedResolutionUpdated) {
  for (int i = 0; i < SendStatisticsProxy::kMinRequiredMetricsSamples; ++i)
    statistics_proxy_->OnIncomingFrame(kWidth, kHeight);

  statistics_proxy_->OnCpuRestrictedResolutionChanged(true);

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
  EncodedImage encoded_image;
  statistics_proxy_->SetResolutionRestrictionStats(false /* scaling_enabled */,
                                                   0, 0);
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
  EncodedImage encoded_image;
  statistics_proxy_->OnQualityRestrictedResolutionChanged(kDownscales);
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
  statistics_proxy_->OnQualityRestrictedResolutionChanged(1);
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

}  // namespace webrtc
