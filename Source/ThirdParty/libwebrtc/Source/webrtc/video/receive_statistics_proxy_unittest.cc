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

#include "webrtc/api/video/video_frame.h"
#include "webrtc/api/video/video_rotation.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/system_wrappers/include/metrics.h"
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
    statistics_proxy_->OnDecodedFrame(rtc::Optional<uint8_t>());
    EXPECT_EQ(i, statistics_proxy_->GetStats().frames_decoded);
  }
}

TEST_F(ReceiveStatisticsProxyTest, OnDecodedFrameWithQpResetsFramesDecoded) {
  EXPECT_EQ(0u, statistics_proxy_->GetStats().frames_decoded);
  for (uint32_t i = 1; i <= 3; ++i) {
    statistics_proxy_->OnDecodedFrame(rtc::Optional<uint8_t>());
    EXPECT_EQ(i, statistics_proxy_->GetStats().frames_decoded);
  }
  statistics_proxy_->OnDecodedFrame(rtc::Optional<uint8_t>(1u));
  EXPECT_EQ(1u, statistics_proxy_->GetStats().frames_decoded);
}

TEST_F(ReceiveStatisticsProxyTest, OnDecodedFrameIncreasesQpSum) {
  EXPECT_EQ(rtc::Optional<uint64_t>(), statistics_proxy_->GetStats().qp_sum);
  statistics_proxy_->OnDecodedFrame(rtc::Optional<uint8_t>(3u));
  EXPECT_EQ(rtc::Optional<uint64_t>(3u), statistics_proxy_->GetStats().qp_sum);
  statistics_proxy_->OnDecodedFrame(rtc::Optional<uint8_t>(127u));
  EXPECT_EQ(rtc::Optional<uint64_t>(130u),
            statistics_proxy_->GetStats().qp_sum);
}

TEST_F(ReceiveStatisticsProxyTest, OnDecodedFrameWithoutQpQpSumWontExist) {
  EXPECT_EQ(rtc::Optional<uint64_t>(), statistics_proxy_->GetStats().qp_sum);
  statistics_proxy_->OnDecodedFrame(rtc::Optional<uint8_t>());
  EXPECT_EQ(rtc::Optional<uint64_t>(), statistics_proxy_->GetStats().qp_sum);
}

TEST_F(ReceiveStatisticsProxyTest, OnDecodedFrameWithoutQpResetsQpSum) {
  EXPECT_EQ(rtc::Optional<uint64_t>(), statistics_proxy_->GetStats().qp_sum);
  statistics_proxy_->OnDecodedFrame(rtc::Optional<uint8_t>(3u));
  EXPECT_EQ(rtc::Optional<uint64_t>(3u), statistics_proxy_->GetStats().qp_sum);
  statistics_proxy_->OnDecodedFrame(rtc::Optional<uint8_t>());
  EXPECT_EQ(rtc::Optional<uint64_t>(), statistics_proxy_->GetStats().qp_sum);
}

TEST_F(ReceiveStatisticsProxyTest, OnRenderedFrameIncreasesFramesRendered) {
  EXPECT_EQ(0u, statistics_proxy_->GetStats().frames_rendered);
  webrtc::VideoFrame frame(
      webrtc::I420Buffer::Create(1, 1), 0, 0, webrtc::kVideoRotation_0);
  for (uint32_t i = 1; i <= 3; ++i) {
    statistics_proxy_->OnRenderedFrame(frame);
    EXPECT_EQ(i, statistics_proxy_->GetStats().frames_rendered);
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

TEST_F(ReceiveStatisticsProxyTest, GetStatsReportsDecoderImplementationName) {
  const char* kName = "decoderName";
  statistics_proxy_->OnDecoderImplementationName(kName);
  EXPECT_STREQ(
      kName, statistics_proxy_->GetStats().decoder_implementation_name.c_str());
}

TEST_F(ReceiveStatisticsProxyTest, GetStatsReportsOnCompleteFrame) {
  const int kFrameSizeBytes = 1000;
  statistics_proxy_->OnCompleteFrame(true, kFrameSizeBytes);
  VideoReceiveStream::Stats stats = statistics_proxy_->GetStats();
  EXPECT_EQ(1, stats.network_frame_rate);
  EXPECT_EQ(kFrameSizeBytes * 8, stats.total_bitrate_bps);
  EXPECT_EQ(1, stats.frame_counts.key_frames);
  EXPECT_EQ(0, stats.frame_counts.delta_frames);
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
  statistics_proxy_->OnRttUpdate(kRttMs, 0);
  statistics_proxy_->OnFrameBufferTimingsUpdated(
      kDecodeMs, kMaxDecodeMs, kCurrentDelayMs, kTargetDelayMs, kJitterBufferMs,
      kMinPlayoutDelayMs, kRenderDelayMs);
  VideoReceiveStream::Stats stats = statistics_proxy_->GetStats();
  EXPECT_EQ(kDecodeMs, stats.decode_ms);
  EXPECT_EQ(kMaxDecodeMs, stats.max_decode_ms);
  EXPECT_EQ(kCurrentDelayMs, stats.current_delay_ms);
  EXPECT_EQ(kTargetDelayMs, stats.target_delay_ms);
  EXPECT_EQ(kJitterBufferMs, stats.jitter_buffer_ms);
  EXPECT_EQ(kMinPlayoutDelayMs, stats.min_playout_delay_ms);
  EXPECT_EQ(kRenderDelayMs, stats.render_delay_ms);
}

TEST_F(ReceiveStatisticsProxyTest, GetStatsReportsRtcpPacketTypeCounts) {
  const uint32_t kFirPackets = 33;
  const uint32_t kPliPackets = 44;
  const uint32_t kNackPackets = 55;
  RtcpPacketTypeCounter counter;
  counter.fir_packets = kFirPackets;
  counter.pli_packets = kPliPackets;
  counter.nack_packets = kNackPackets;
  statistics_proxy_->RtcpPacketTypesCounterUpdated(kRemoteSsrc, counter);
  VideoReceiveStream::Stats stats = statistics_proxy_->GetStats();
  EXPECT_EQ(kFirPackets, stats.rtcp_packet_type_counts.fir_packets);
  EXPECT_EQ(kPliPackets, stats.rtcp_packet_type_counts.pli_packets);
  EXPECT_EQ(kNackPackets, stats.rtcp_packet_type_counts.nack_packets);
}

TEST_F(ReceiveStatisticsProxyTest,
       GetStatsReportsNoRtcpPacketTypeCountsForUnknownSsrc) {
  RtcpPacketTypeCounter counter;
  counter.fir_packets = 33;
  statistics_proxy_->RtcpPacketTypesCounterUpdated(kRemoteSsrc + 1, counter);
  EXPECT_EQ(0u,
            statistics_proxy_->GetStats().rtcp_packet_type_counts.fir_packets);
}

TEST_F(ReceiveStatisticsProxyTest, GetStatsReportsFrameCounts) {
  const int kKeyFrames = 3;
  const int kDeltaFrames = 22;
  FrameCounts frame_counts;
  frame_counts.key_frames = kKeyFrames;
  frame_counts.delta_frames = kDeltaFrames;
  statistics_proxy_->OnFrameCountsUpdated(frame_counts);
  VideoReceiveStream::Stats stats = statistics_proxy_->GetStats();
  EXPECT_EQ(kKeyFrames, stats.frame_counts.key_frames);
  EXPECT_EQ(kDeltaFrames, stats.frame_counts.delta_frames);
}

TEST_F(ReceiveStatisticsProxyTest, GetStatsReportsDiscardedPackets) {
  const int kDiscardedPackets = 12;
  statistics_proxy_->OnDiscardedPacketsUpdated(kDiscardedPackets);
  EXPECT_EQ(kDiscardedPackets, statistics_proxy_->GetStats().discarded_packets);
}

TEST_F(ReceiveStatisticsProxyTest, GetStatsReportsRtcpStats) {
  const uint8_t kFracLost = 0;
  const uint32_t kCumLost = 1;
  const uint32_t kExtSeqNum = 10;
  const uint32_t kJitter = 4;

  RtcpStatistics rtcp_stats;
  rtcp_stats.fraction_lost = kFracLost;
  rtcp_stats.cumulative_lost = kCumLost;
  rtcp_stats.extended_max_sequence_number = kExtSeqNum;
  rtcp_stats.jitter = kJitter;
  statistics_proxy_->StatisticsUpdated(rtcp_stats, kRemoteSsrc);

  VideoReceiveStream::Stats stats = statistics_proxy_->GetStats();
  EXPECT_EQ(kFracLost, stats.rtcp_stats.fraction_lost);
  EXPECT_EQ(kCumLost, stats.rtcp_stats.cumulative_lost);
  EXPECT_EQ(kExtSeqNum, stats.rtcp_stats.extended_max_sequence_number);
  EXPECT_EQ(kJitter, stats.rtcp_stats.jitter);
}

TEST_F(ReceiveStatisticsProxyTest, GetStatsReportsCName) {
  const char* kName = "cName";
  statistics_proxy_->CNameChanged(kName, kRemoteSsrc);
  EXPECT_STREQ(kName, statistics_proxy_->GetStats().c_name.c_str());
}

TEST_F(ReceiveStatisticsProxyTest, GetStatsReportsNoCNameForUnknownSsrc) {
  const char* kName = "cName";
  statistics_proxy_->CNameChanged(kName, kRemoteSsrc + 1);
  EXPECT_STREQ("", statistics_proxy_->GetStats().c_name.c_str());
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

TEST_F(ReceiveStatisticsProxyTest, BadCallHistogramsAreUpdated) {
  // Based on the tuning parameters this will produce 7 uncertain states,
  // then 10 certainly bad states. There has to be 10 certain states before
  // any histograms are recorded.
  const int kNumBadSamples = 17;

  StreamDataCounters counters;
  counters.first_packet_time_ms = fake_clock_.TimeInMilliseconds();
  statistics_proxy_->DataCountersUpdated(counters, config_.rtp.remote_ssrc);

  for (int i = 0; i < kNumBadSamples; ++i) {
    // Since OnRenderedFrame is never called the fps in each sample will be 0,
    // i.e. bad
    fake_clock_.AdvanceTimeMilliseconds(1000);
    statistics_proxy_->OnIncomingRate(0, 0);
  }
  // Histograms are updated when the statistics_proxy_ is deleted.
  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.BadCall.Any"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.BadCall.Any", 100));

  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.BadCall.FrameRate"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.BadCall.FrameRate", 100));

  EXPECT_EQ(0, metrics::NumSamples("WebRTC.Video.BadCall.FrameRateVariance"));

  EXPECT_EQ(0, metrics::NumSamples("WebRTC.Video.BadCall.Qp"));
}

TEST_F(ReceiveStatisticsProxyTest, PacketLossHistogramIsUpdated) {
  const uint32_t kCumLost1 = 1;
  const uint32_t kExtSeqNum1 = 10;
  const uint32_t kCumLost2 = 2;
  const uint32_t kExtSeqNum2 = 20;

  // One report block received.
  RtcpStatistics rtcp_stats1;
  rtcp_stats1.cumulative_lost = kCumLost1;
  rtcp_stats1.extended_max_sequence_number = kExtSeqNum1;
  statistics_proxy_->StatisticsUpdated(rtcp_stats1, kRemoteSsrc);

  // Two report blocks received.
  RtcpStatistics rtcp_stats2;
  rtcp_stats2.cumulative_lost = kCumLost2;
  rtcp_stats2.extended_max_sequence_number = kExtSeqNum2;
  statistics_proxy_->StatisticsUpdated(rtcp_stats2, kRemoteSsrc);

  // Two received report blocks but min run time has not passed.
  fake_clock_.AdvanceTimeMilliseconds(metrics::kMinRunTimeInSeconds * 1000 - 1);
  SetUp();  // Reset stat proxy causes histograms to be updated.
  EXPECT_EQ(0,
            metrics::NumSamples("WebRTC.Video.ReceivedPacketsLostInPercent"));

  // Two report blocks received.
  statistics_proxy_->StatisticsUpdated(rtcp_stats1, kRemoteSsrc);
  statistics_proxy_->StatisticsUpdated(rtcp_stats2, kRemoteSsrc);

  // Two received report blocks and min run time has passed.
  fake_clock_.AdvanceTimeMilliseconds(metrics::kMinRunTimeInSeconds * 1000);
  SetUp();
  EXPECT_EQ(1,
            metrics::NumSamples("WebRTC.Video.ReceivedPacketsLostInPercent"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.ReceivedPacketsLostInPercent",
                                  (kCumLost2 - kCumLost1) * 100 /
                                      (kExtSeqNum2 - kExtSeqNum1)));
}

TEST_F(ReceiveStatisticsProxyTest,
       PacketLossHistogramIsNotUpdatedIfLessThanTwoReportBlocksAreReceived) {
  RtcpStatistics rtcp_stats1;
  rtcp_stats1.cumulative_lost = 1;
  rtcp_stats1.extended_max_sequence_number = 10;

  // Min run time has passed but no received report block.
  fake_clock_.AdvanceTimeMilliseconds(metrics::kMinRunTimeInSeconds * 1000);
  SetUp();  // Reset stat proxy causes histograms to be updated.
  EXPECT_EQ(0,
            metrics::NumSamples("WebRTC.Video.ReceivedPacketsLostInPercent"));

  // Min run time has passed but only one received report block.
  statistics_proxy_->StatisticsUpdated(rtcp_stats1, kRemoteSsrc);
  fake_clock_.AdvanceTimeMilliseconds(metrics::kMinRunTimeInSeconds * 1000);
  SetUp();
  EXPECT_EQ(0,
            metrics::NumSamples("WebRTC.Video.ReceivedPacketsLostInPercent"));
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

TEST_F(ReceiveStatisticsProxyTest, Vp8QpHistogramIsUpdated) {
  const int kQp = 22;
  EncodedImage encoded_image;
  encoded_image.qp_ = kQp;
  CodecSpecificInfo codec_info;
  codec_info.codecType = kVideoCodecVP8;

  for (int i = 0; i < kMinRequiredSamples; ++i)
    statistics_proxy_->OnPreDecode(encoded_image, &codec_info);

  statistics_proxy_.reset();
  EXPECT_EQ(1, metrics::NumSamples("WebRTC.Video.Decoded.Vp8.Qp"));
  EXPECT_EQ(1, metrics::NumEvents("WebRTC.Video.Decoded.Vp8.Qp", kQp));
}

TEST_F(ReceiveStatisticsProxyTest, Vp8QpHistogramIsNotUpdatedForTooFewSamples) {
  EncodedImage encoded_image;
  encoded_image.qp_ = 22;
  CodecSpecificInfo codec_info;
  codec_info.codecType = kVideoCodecVP8;

  for (int i = 0; i < kMinRequiredSamples - 1; ++i)
    statistics_proxy_->OnPreDecode(encoded_image, &codec_info);

  statistics_proxy_.reset();
  EXPECT_EQ(0, metrics::NumSamples("WebRTC.Video.Decoded.Vp8.Qp"));
}

TEST_F(ReceiveStatisticsProxyTest, Vp8QpHistogramIsNotUpdatedIfNoQpValue) {
  EncodedImage encoded_image;
  CodecSpecificInfo codec_info;
  codec_info.codecType = kVideoCodecVP8;

  for (int i = 0; i < kMinRequiredSamples; ++i)
    statistics_proxy_->OnPreDecode(encoded_image, &codec_info);

  statistics_proxy_.reset();
  EXPECT_EQ(0, metrics::NumSamples("WebRTC.Video.Decoded.Vp8.Qp"));
}

}  // namespace webrtc
