/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/receive_statistics_proxy2.h"

#include <limits>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "absl/types/optional.h"
#include "api/scoped_refptr.h"
#include "api/units/frequency.h"
#include "api/units/time_delta.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_buffer.h"
#include "api/video/video_rotation.h"
#include "rtc_base/thread.h"
#include "system_wrappers/include/metrics.h"
#include "test/gtest.h"
#include "test/scoped_key_value_config.h"
#include "test/time_controller/simulated_time_controller.h"
#include "video/video_receive_stream2.h"

namespace webrtc {
namespace internal {
namespace {
const TimeDelta kFreqOffsetProcessInterval = TimeDelta::Seconds(40);
const uint32_t kRemoteSsrc = 456;
const int kMinRequiredSamples = 200;
const int kWidth = 1280;
const int kHeight = 720;
}  // namespace

// TODO(sakal): ReceiveStatisticsProxy is lacking unittesting.
class ReceiveStatisticsProxy2Test : public ::testing::Test {
 public:
  ReceiveStatisticsProxy2Test() : time_controller_(Timestamp::Millis(1234)) {
    metrics::Reset();
    statistics_proxy_.reset(
        new ReceiveStatisticsProxy(kRemoteSsrc, time_controller_.GetClock(),
                                   time_controller_.GetMainThread()));
  }

  ~ReceiveStatisticsProxy2Test() override { statistics_proxy_.reset(); }

 protected:
  // Convenience method to avoid too many explict flushes.
  VideoReceiveStreamInterface::Stats FlushAndGetStats() {
    time_controller_.AdvanceTime(TimeDelta::Zero());
    return statistics_proxy_->GetStats();
  }

  void FlushAndUpdateHistograms(absl::optional<int> fraction_lost,
                                const StreamDataCounters& rtp_stats,
                                const StreamDataCounters* rtx_stats) {
    time_controller_.AdvanceTime(TimeDelta::Zero());
    statistics_proxy_->UpdateHistograms(fraction_lost, rtp_stats, rtx_stats);
  }

  VideoFrame CreateFrame(int width, int height) {
    return CreateVideoFrame(width, height, 0);
  }

  VideoFrame CreateFrameWithRenderTime(Timestamp render_time) {
    return CreateFrameWithRenderTimeMs(render_time.ms());
  }

  VideoFrame CreateFrameWithRenderTimeMs(int64_t render_time_ms) {
    return CreateVideoFrame(kWidth, kHeight, render_time_ms);
  }

  VideoFrame CreateVideoFrame(int width, int height, int64_t render_time_ms) {
    VideoFrame frame =
        VideoFrame::Builder()
            .set_video_frame_buffer(I420Buffer::Create(width, height))
            .set_timestamp_rtp(0)
            .set_timestamp_ms(render_time_ms)
            .set_rotation(kVideoRotation_0)
            .build();
    frame.set_ntp_time_ms(
        time_controller_.GetClock()->CurrentNtpInMilliseconds());
    return frame;
  }

  // Return the current fake time as a Timestamp.
  Timestamp Now() { return time_controller_.GetClock()->CurrentTime(); }

  // Creates a VideoFrameMetaData instance with a timestamp.
  VideoFrameMetaData MetaData(const VideoFrame& frame, Timestamp ts) {
    return VideoFrameMetaData(frame, ts);
  }

  // Creates a VideoFrameMetaData instance with the current fake time.
  VideoFrameMetaData MetaData(const VideoFrame& frame) {
    return VideoFrameMetaData(frame, Now());
  }

  test::ScopedKeyValueConfig field_trials_;
  GlobalSimulatedTimeController time_controller_;
  std::unique_ptr<ReceiveStatisticsProxy> statistics_proxy_;
};

TEST_F(ReceiveStatisticsProxy2Test, OnDecodedFrameIncreasesFramesDecoded) {
  EXPECT_EQ(0u, statistics_proxy_->GetStats().frames_decoded);
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);
  for (uint32_t i = 1; i <= 3; ++i) {
    statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                      VideoContentType::UNSPECIFIED);
    EXPECT_EQ(i, FlushAndGetStats().frames_decoded);
  }
}

TEST_F(ReceiveStatisticsProxy2Test, DecodedFpsIsReported) {
  const Frequency kFps = Frequency::Hertz(20);
  const int kRequiredSamples =
      TimeDelta::Seconds(metrics::kMinRunTimeInSeconds) * kFps;
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);
  for (int i = 0; i < kRequiredSamples; ++i) {
    statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                      VideoContentType::UNSPECIFIED);
    time_controller_.AdvanceTime(1 / kFps);
  }
  FlushAndUpdateHistograms(absl::nullopt, StreamDataCounters(), nullptr);
  EXPECT_METRIC_EQ(1,
                   metrics::NumSamples("WebRTC.Video.DecodedFramesPerSecond"));
  EXPECT_METRIC_EQ(1, metrics::NumEvents("WebRTC.Video.DecodedFramesPerSecond",
                                         kFps.hertz()));
}

TEST_F(ReceiveStatisticsProxy2Test, DecodedFpsIsNotReportedForTooFewSamples) {
  const Frequency kFps = Frequency::Hertz(20);
  const int kRequiredSamples =
      TimeDelta::Seconds(metrics::kMinRunTimeInSeconds) * kFps;
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);
  for (int i = 0; i < kRequiredSamples - 1; ++i) {
    statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                      VideoContentType::UNSPECIFIED);
    time_controller_.AdvanceTime(1 / kFps);
  }
  FlushAndUpdateHistograms(absl::nullopt, StreamDataCounters(), nullptr);
  EXPECT_METRIC_EQ(0,
                   metrics::NumSamples("WebRTC.Video.DecodedFramesPerSecond"));
}

TEST_F(ReceiveStatisticsProxy2Test,
       OnDecodedFrameWithQpDoesNotResetFramesDecodedOrTotalDecodeTime) {
  EXPECT_EQ(0u, statistics_proxy_->GetStats().frames_decoded);
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);
  TimeDelta expected_total_decode_time = TimeDelta::Zero();
  unsigned int expected_frames_decoded = 0;
  for (uint32_t i = 1; i <= 3; ++i) {
    statistics_proxy_->OnDecodedFrame(frame, absl::nullopt,
                                      TimeDelta::Millis(1),
                                      VideoContentType::UNSPECIFIED);
    expected_total_decode_time += TimeDelta::Millis(1);
    ++expected_frames_decoded;
    time_controller_.AdvanceTime(TimeDelta::Zero());
    EXPECT_EQ(expected_frames_decoded,
              statistics_proxy_->GetStats().frames_decoded);
    EXPECT_EQ(expected_total_decode_time,
              statistics_proxy_->GetStats().total_decode_time);
  }
  statistics_proxy_->OnDecodedFrame(frame, 1u, TimeDelta::Millis(3),
                                    VideoContentType::UNSPECIFIED);
  ++expected_frames_decoded;
  expected_total_decode_time += TimeDelta::Millis(3);
  time_controller_.AdvanceTime(TimeDelta::Zero());
  EXPECT_EQ(expected_frames_decoded,
            statistics_proxy_->GetStats().frames_decoded);
  EXPECT_EQ(expected_total_decode_time,
            statistics_proxy_->GetStats().total_decode_time);
}

TEST_F(ReceiveStatisticsProxy2Test, OnDecodedFrameIncreasesProcessingDelay) {
  const TimeDelta kProcessingDelay = TimeDelta::Millis(10);
  EXPECT_EQ(0u, statistics_proxy_->GetStats().frames_decoded);
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);
  TimeDelta expected_total_processing_delay = TimeDelta::Zero();
  unsigned int expected_frames_decoded = 0;
  // We set receive time fixed and increase the clock by 10ms
  // in the loop which will increase the processing delay by
  // 10/20/30ms respectively.
  RtpPacketInfos::vector_type packet_infos = {
      RtpPacketInfo({}, {}, {}, {}, {}, Now())};
  frame.set_packet_infos(RtpPacketInfos(packet_infos));
  for (int i = 1; i <= 3; ++i) {
    time_controller_.AdvanceTime(kProcessingDelay);
    statistics_proxy_->OnDecodedFrame(frame, absl::nullopt,
                                      TimeDelta::Millis(1),
                                      VideoContentType::UNSPECIFIED);
    expected_total_processing_delay += i * kProcessingDelay;
    ++expected_frames_decoded;
    time_controller_.AdvanceTime(TimeDelta::Zero());
    EXPECT_EQ(expected_frames_decoded,
              statistics_proxy_->GetStats().frames_decoded);
    EXPECT_EQ(expected_total_processing_delay,
              statistics_proxy_->GetStats().total_processing_delay);
  }
  time_controller_.AdvanceTime(kProcessingDelay);
  statistics_proxy_->OnDecodedFrame(frame, 1u, TimeDelta::Millis(3),
                                    VideoContentType::UNSPECIFIED);
  ++expected_frames_decoded;
  expected_total_processing_delay += 4 * kProcessingDelay;
  time_controller_.AdvanceTime(TimeDelta::Zero());
  EXPECT_EQ(expected_frames_decoded,
            statistics_proxy_->GetStats().frames_decoded);
  EXPECT_EQ(expected_total_processing_delay,
            statistics_proxy_->GetStats().total_processing_delay);
}

TEST_F(ReceiveStatisticsProxy2Test, OnDecodedFrameIncreasesAssemblyTime) {
  const TimeDelta kAssemblyTime = TimeDelta::Millis(7);
  EXPECT_EQ(0u, statistics_proxy_->GetStats().frames_decoded);
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);
  TimeDelta expected_total_assembly_time = TimeDelta::Zero();
  unsigned int expected_frames_decoded = 0;
  unsigned int expected_frames_assembled_from_multiple_packets = 0;

  // A single-packet frame will not increase total assembly time
  // and frames assembled.
  RtpPacketInfos::vector_type single_packet_frame = {
      RtpPacketInfo({}, {}, {}, {}, {}, Now())};
  frame.set_packet_infos(RtpPacketInfos(single_packet_frame));
  statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Millis(1),
                                    VideoContentType::UNSPECIFIED);
  ++expected_frames_decoded;
  time_controller_.AdvanceTime(TimeDelta::Zero());
  EXPECT_EQ(expected_total_assembly_time,
            statistics_proxy_->GetStats().total_assembly_time);
  EXPECT_EQ(
      expected_frames_assembled_from_multiple_packets,
      statistics_proxy_->GetStats().frames_assembled_from_multiple_packets);

  // In an ordered frame the first and last packet matter.
  RtpPacketInfos::vector_type ordered_frame = {
      RtpPacketInfo({}, {}, {}, {}, {}, Now()),
      RtpPacketInfo({}, {}, {}, {}, {}, Now() + kAssemblyTime),
      RtpPacketInfo({}, {}, {}, {}, {}, Now() + 2 * kAssemblyTime),
  };
  frame.set_packet_infos(RtpPacketInfos(ordered_frame));
  statistics_proxy_->OnDecodedFrame(frame, 1u, TimeDelta::Millis(3),
                                    VideoContentType::UNSPECIFIED);
  ++expected_frames_decoded;
  ++expected_frames_assembled_from_multiple_packets;
  expected_total_assembly_time += 2 * kAssemblyTime;
  time_controller_.AdvanceTime(TimeDelta::Zero());
  EXPECT_EQ(expected_frames_decoded,
            statistics_proxy_->GetStats().frames_decoded);
  EXPECT_EQ(expected_total_assembly_time,
            statistics_proxy_->GetStats().total_assembly_time);
  EXPECT_EQ(
      expected_frames_assembled_from_multiple_packets,
      statistics_proxy_->GetStats().frames_assembled_from_multiple_packets);

  // "First" and "last" are in receive time, not sequence number.
  RtpPacketInfos::vector_type unordered_frame = {
      RtpPacketInfo({}, {}, {}, {}, {}, Now() + 2 * kAssemblyTime),
      RtpPacketInfo({}, {}, {}, {}, {}, Now()),
      RtpPacketInfo({}, {}, {}, {}, {}, Now() + kAssemblyTime),
  };
  frame.set_packet_infos(RtpPacketInfos(unordered_frame));
  statistics_proxy_->OnDecodedFrame(frame, 1u, TimeDelta::Millis(3),
                                    VideoContentType::UNSPECIFIED);
  ++expected_frames_decoded;
  ++expected_frames_assembled_from_multiple_packets;
  expected_total_assembly_time += 2 * kAssemblyTime;
  time_controller_.AdvanceTime(TimeDelta::Zero());
  EXPECT_EQ(expected_frames_decoded,
            statistics_proxy_->GetStats().frames_decoded);
  EXPECT_EQ(expected_total_assembly_time,
            statistics_proxy_->GetStats().total_assembly_time);
  EXPECT_EQ(
      expected_frames_assembled_from_multiple_packets,
      statistics_proxy_->GetStats().frames_assembled_from_multiple_packets);
}

TEST_F(ReceiveStatisticsProxy2Test, OnDecodedFrameIncreasesQpSum) {
  EXPECT_EQ(absl::nullopt, statistics_proxy_->GetStats().qp_sum);
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);
  statistics_proxy_->OnDecodedFrame(frame, 3u, TimeDelta::Zero(),
                                    VideoContentType::UNSPECIFIED);
  EXPECT_EQ(3u, FlushAndGetStats().qp_sum);
  statistics_proxy_->OnDecodedFrame(frame, 127u, TimeDelta::Zero(),
                                    VideoContentType::UNSPECIFIED);
  EXPECT_EQ(130u, FlushAndGetStats().qp_sum);
}

TEST_F(ReceiveStatisticsProxy2Test, OnDecodedFrameIncreasesTotalDecodeTime) {
  EXPECT_EQ(absl::nullopt, statistics_proxy_->GetStats().qp_sum);
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);
  statistics_proxy_->OnDecodedFrame(frame, 3u, TimeDelta::Millis(4),
                                    VideoContentType::UNSPECIFIED);
  EXPECT_EQ(4u, FlushAndGetStats().total_decode_time.ms());
  statistics_proxy_->OnDecodedFrame(frame, 127u, TimeDelta::Millis(7),
                                    VideoContentType::UNSPECIFIED);
  EXPECT_EQ(11u, FlushAndGetStats().total_decode_time.ms());
}

TEST_F(ReceiveStatisticsProxy2Test, ReportsContentType) {
  const std::string kRealtimeString("realtime");
  const std::string kScreenshareString("screen");
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);
  EXPECT_EQ(kRealtimeString, videocontenttypehelpers::ToString(
                                 statistics_proxy_->GetStats().content_type));
  statistics_proxy_->OnDecodedFrame(frame, 3u, TimeDelta::Zero(),
                                    VideoContentType::SCREENSHARE);
  EXPECT_EQ(kScreenshareString,
            videocontenttypehelpers::ToString(FlushAndGetStats().content_type));
  statistics_proxy_->OnDecodedFrame(frame, 3u, TimeDelta::Zero(),
                                    VideoContentType::UNSPECIFIED);
  EXPECT_EQ(kRealtimeString,
            videocontenttypehelpers::ToString(FlushAndGetStats().content_type));
}

TEST_F(ReceiveStatisticsProxy2Test, ReportsMaxTotalInterFrameDelay) {
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);
  const TimeDelta kInterFrameDelay1 = TimeDelta::Millis(100);
  const TimeDelta kInterFrameDelay2 = TimeDelta::Millis(200);
  const TimeDelta kInterFrameDelay3 = TimeDelta::Millis(300);
  double expected_total_inter_frame_delay = 0;
  double expected_total_squared_inter_frame_delay = 0;
  EXPECT_EQ(expected_total_inter_frame_delay,
            statistics_proxy_->GetStats().total_inter_frame_delay);
  EXPECT_EQ(expected_total_squared_inter_frame_delay,
            statistics_proxy_->GetStats().total_squared_inter_frame_delay);

  statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                    VideoContentType::UNSPECIFIED);
  EXPECT_DOUBLE_EQ(expected_total_inter_frame_delay,
                   FlushAndGetStats().total_inter_frame_delay);
  EXPECT_DOUBLE_EQ(expected_total_squared_inter_frame_delay,
                   FlushAndGetStats().total_squared_inter_frame_delay);

  time_controller_.AdvanceTime(kInterFrameDelay1);
  statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                    VideoContentType::UNSPECIFIED);
  expected_total_inter_frame_delay += kInterFrameDelay1.seconds<double>();
  expected_total_squared_inter_frame_delay +=
      pow(kInterFrameDelay1.seconds<double>(), 2.0);
  EXPECT_DOUBLE_EQ(expected_total_inter_frame_delay,
                   FlushAndGetStats().total_inter_frame_delay);
  EXPECT_DOUBLE_EQ(
      expected_total_squared_inter_frame_delay,
      statistics_proxy_->GetStats().total_squared_inter_frame_delay);

  time_controller_.AdvanceTime(kInterFrameDelay2);
  statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                    VideoContentType::UNSPECIFIED);
  expected_total_inter_frame_delay += kInterFrameDelay2.seconds<double>();
  expected_total_squared_inter_frame_delay +=
      pow(kInterFrameDelay2.seconds<double>(), 2.0);
  EXPECT_DOUBLE_EQ(expected_total_inter_frame_delay,
                   FlushAndGetStats().total_inter_frame_delay);
  EXPECT_DOUBLE_EQ(
      expected_total_squared_inter_frame_delay,
      statistics_proxy_->GetStats().total_squared_inter_frame_delay);

  time_controller_.AdvanceTime(kInterFrameDelay3);
  statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                    VideoContentType::UNSPECIFIED);
  expected_total_inter_frame_delay += kInterFrameDelay3.seconds<double>();
  expected_total_squared_inter_frame_delay +=
      pow(kInterFrameDelay3.seconds<double>(), 2.0);
  EXPECT_DOUBLE_EQ(expected_total_inter_frame_delay,
                   FlushAndGetStats().total_inter_frame_delay);
  EXPECT_DOUBLE_EQ(
      expected_total_squared_inter_frame_delay,
      statistics_proxy_->GetStats().total_squared_inter_frame_delay);
}

TEST_F(ReceiveStatisticsProxy2Test, ReportsMaxInterframeDelay) {
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);
  const TimeDelta kInterframeDelay1 = TimeDelta::Millis(100);
  const TimeDelta kInterframeDelay2 = TimeDelta::Millis(200);
  const TimeDelta kInterframeDelay3 = TimeDelta::Millis(100);
  EXPECT_EQ(-1, statistics_proxy_->GetStats().interframe_delay_max_ms);
  statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                    VideoContentType::UNSPECIFIED);
  EXPECT_EQ(-1, FlushAndGetStats().interframe_delay_max_ms);

  time_controller_.AdvanceTime(kInterframeDelay1);
  statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                    VideoContentType::UNSPECIFIED);
  EXPECT_EQ(kInterframeDelay1.ms(), FlushAndGetStats().interframe_delay_max_ms);

  time_controller_.AdvanceTime(kInterframeDelay2);
  statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                    VideoContentType::UNSPECIFIED);
  EXPECT_EQ(kInterframeDelay2.ms(), FlushAndGetStats().interframe_delay_max_ms);

  time_controller_.AdvanceTime(kInterframeDelay3);
  statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                    VideoContentType::UNSPECIFIED);
  // kInterframeDelay3 is smaller than kInterframeDelay2.
  EXPECT_EQ(kInterframeDelay2.ms(), FlushAndGetStats().interframe_delay_max_ms);
}

TEST_F(ReceiveStatisticsProxy2Test, ReportInterframeDelayInWindow) {
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);
  const TimeDelta kInterframeDelay1 = TimeDelta::Millis(900);
  const TimeDelta kInterframeDelay2 = TimeDelta::Millis(750);
  const TimeDelta kInterframeDelay3 = TimeDelta::Millis(700);
  EXPECT_EQ(-1, statistics_proxy_->GetStats().interframe_delay_max_ms);
  statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                    VideoContentType::UNSPECIFIED);
  EXPECT_EQ(-1, FlushAndGetStats().interframe_delay_max_ms);

  time_controller_.AdvanceTime(kInterframeDelay1);
  statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                    VideoContentType::UNSPECIFIED);
  EXPECT_EQ(kInterframeDelay1.ms(), FlushAndGetStats().interframe_delay_max_ms);

  time_controller_.AdvanceTime(kInterframeDelay2);
  statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                    VideoContentType::UNSPECIFIED);
  // Still first delay is the maximum
  EXPECT_EQ(kInterframeDelay1.ms(), FlushAndGetStats().interframe_delay_max_ms);

  time_controller_.AdvanceTime(kInterframeDelay3);
  statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                    VideoContentType::UNSPECIFIED);
  // Now the first sample is out of the window, so the second is the maximum.
  EXPECT_EQ(kInterframeDelay2.ms(), FlushAndGetStats().interframe_delay_max_ms);
}

TEST_F(ReceiveStatisticsProxy2Test, ReportsFreezeMetrics) {
  const TimeDelta kFreezeDuration = TimeDelta::Seconds(1);

  VideoReceiveStreamInterface::Stats stats = statistics_proxy_->GetStats();
  EXPECT_EQ(0u, stats.freeze_count);
  EXPECT_FALSE(stats.total_freezes_duration_ms);

  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);
  for (size_t i = 0; i < VideoQualityObserver::kMinFrameSamplesToDetectFreeze;
       ++i) {
    time_controller_.AdvanceTime(TimeDelta::Millis(30));
    statistics_proxy_->OnRenderedFrame(MetaData(frame));
  }

  // Freeze.
  time_controller_.AdvanceTime(kFreezeDuration);
  statistics_proxy_->OnRenderedFrame(MetaData(frame));

  stats = statistics_proxy_->GetStats();
  EXPECT_EQ(1u, stats.freeze_count);
  EXPECT_EQ(kFreezeDuration.ms(), stats.total_freezes_duration_ms);
}

TEST_F(ReceiveStatisticsProxy2Test, ReportsPauseMetrics) {
  VideoReceiveStreamInterface::Stats stats = statistics_proxy_->GetStats();
  ASSERT_EQ(0u, stats.pause_count);
  ASSERT_EQ(0u, stats.total_pauses_duration_ms);

  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);
  statistics_proxy_->OnRenderedFrame(MetaData(frame));

  // Pause.
  time_controller_.AdvanceTime(TimeDelta::Millis(5432));
  statistics_proxy_->OnStreamInactive();
  statistics_proxy_->OnRenderedFrame(MetaData(frame));

  stats = statistics_proxy_->GetStats();
  EXPECT_EQ(1u, stats.pause_count);
  EXPECT_EQ(5432u, stats.total_pauses_duration_ms);
}

TEST_F(ReceiveStatisticsProxy2Test, PauseBeforeFirstAndAfterLastFrameIgnored) {
  VideoReceiveStreamInterface::Stats stats = statistics_proxy_->GetStats();
  ASSERT_EQ(0u, stats.pause_count);
  ASSERT_EQ(0u, stats.total_pauses_duration_ms);

  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);

  // Pause -> Frame -> Pause
  time_controller_.AdvanceTime(TimeDelta::Seconds(5));
  statistics_proxy_->OnStreamInactive();
  statistics_proxy_->OnRenderedFrame(MetaData(frame));

  time_controller_.AdvanceTime(TimeDelta::Millis(30));
  statistics_proxy_->OnRenderedFrame(MetaData(frame));

  time_controller_.AdvanceTime(TimeDelta::Seconds(5));
  statistics_proxy_->OnStreamInactive();

  stats = statistics_proxy_->GetStats();
  EXPECT_EQ(0u, stats.pause_count);
  EXPECT_EQ(0u, stats.total_pauses_duration_ms);
}

TEST_F(ReceiveStatisticsProxy2Test, ReportsFramesDuration) {
  VideoReceiveStreamInterface::Stats stats = statistics_proxy_->GetStats();
  ASSERT_EQ(0u, stats.total_frames_duration_ms);

  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);

  // Emulate delay before first frame is rendered. This is needed to ensure
  // that frame duration only covers time since first frame is rendered and
  // not the total time.
  time_controller_.AdvanceTime(TimeDelta::Millis(5432));
  for (int i = 0; i <= 10; ++i) {
    time_controller_.AdvanceTime(TimeDelta::Millis(30));
    statistics_proxy_->OnRenderedFrame(MetaData(frame));
  }

  stats = statistics_proxy_->GetStats();
  EXPECT_EQ(10 * 30u, stats.total_frames_duration_ms);
}

TEST_F(ReceiveStatisticsProxy2Test, ReportsSumSquaredFrameDurations) {
  VideoReceiveStreamInterface::Stats stats = statistics_proxy_->GetStats();
  ASSERT_EQ(0u, stats.sum_squared_frame_durations);

  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);
  for (int i = 0; i <= 10; ++i) {
    time_controller_.AdvanceTime(TimeDelta::Millis(30));
    statistics_proxy_->OnRenderedFrame(MetaData(frame));
  }

  stats = statistics_proxy_->GetStats();
  const double kExpectedSumSquaredFrameDurationsSecs =
      10 * (30 / 1000.0 * 30 / 1000.0);
  EXPECT_EQ(kExpectedSumSquaredFrameDurationsSecs,
            stats.sum_squared_frame_durations);
}

TEST_F(ReceiveStatisticsProxy2Test, OnDecodedFrameWithoutQpQpSumWontExist) {
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);
  EXPECT_EQ(absl::nullopt, statistics_proxy_->GetStats().qp_sum);
  statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                    VideoContentType::UNSPECIFIED);
  EXPECT_EQ(absl::nullopt, FlushAndGetStats().qp_sum);
}

TEST_F(ReceiveStatisticsProxy2Test, OnDecodedFrameWithoutQpResetsQpSum) {
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);
  EXPECT_EQ(absl::nullopt, statistics_proxy_->GetStats().qp_sum);
  statistics_proxy_->OnDecodedFrame(frame, 3u, TimeDelta::Zero(),
                                    VideoContentType::UNSPECIFIED);
  EXPECT_EQ(3u, FlushAndGetStats().qp_sum);
  statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                    VideoContentType::UNSPECIFIED);
  EXPECT_EQ(absl::nullopt, FlushAndGetStats().qp_sum);
}

TEST_F(ReceiveStatisticsProxy2Test, OnRenderedFrameIncreasesFramesRendered) {
  EXPECT_EQ(0u, statistics_proxy_->GetStats().frames_rendered);
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);
  for (uint32_t i = 1; i <= 3; ++i) {
    statistics_proxy_->OnRenderedFrame(MetaData(frame));
    EXPECT_EQ(i, statistics_proxy_->GetStats().frames_rendered);
  }
}

TEST_F(ReceiveStatisticsProxy2Test, GetStatsReportsSsrc) {
  EXPECT_EQ(kRemoteSsrc, statistics_proxy_->GetStats().ssrc);
}

TEST_F(ReceiveStatisticsProxy2Test, GetStatsReportsIncomingPayloadType) {
  const int kPayloadType = 111;
  statistics_proxy_->OnIncomingPayloadType(kPayloadType);
  time_controller_.AdvanceTime(TimeDelta::Zero());
  EXPECT_EQ(kPayloadType, statistics_proxy_->GetStats().current_payload_type);
}

TEST_F(ReceiveStatisticsProxy2Test, GetStatsReportsDecoderImplementationName) {
  const char* kName = "decoderName";
  statistics_proxy_->OnDecoderImplementationName(kName);
  time_controller_.AdvanceTime(TimeDelta::Zero());
  EXPECT_STREQ(
      kName, statistics_proxy_->GetStats().decoder_implementation_name.c_str());
}

TEST_F(ReceiveStatisticsProxy2Test, GetStatsReportsOnCompleteFrame) {
  const int kFrameSizeBytes = 1000;
  statistics_proxy_->OnCompleteFrame(true, kFrameSizeBytes,
                                     VideoContentType::UNSPECIFIED);
  VideoReceiveStreamInterface::Stats stats = statistics_proxy_->GetStats();
  EXPECT_EQ(1, stats.network_frame_rate);
  EXPECT_EQ(1, stats.frame_counts.key_frames);
  EXPECT_EQ(0, stats.frame_counts.delta_frames);
}

TEST_F(ReceiveStatisticsProxy2Test, GetStatsReportsOnDroppedFrame) {
  unsigned int dropped_frames = 0;
  for (int i = 0; i < 10; ++i) {
    statistics_proxy_->OnDroppedFrames(i);
    dropped_frames += i;
  }
  VideoReceiveStreamInterface::Stats stats = FlushAndGetStats();
  EXPECT_EQ(dropped_frames, stats.frames_dropped);
}

TEST_F(ReceiveStatisticsProxy2Test, GetStatsReportsDecodeTimingStats) {
  const int kMaxDecodeMs = 2;
  const int kCurrentDelayMs = 3;
  const int kTargetDelayMs = 4;
  const int kJitterBufferMs = 5;
  const int kMinPlayoutDelayMs = 6;
  const int kRenderDelayMs = 7;
  const int64_t kRttMs = 8;
  statistics_proxy_->OnRttUpdate(kRttMs);
  statistics_proxy_->OnFrameBufferTimingsUpdated(
      kMaxDecodeMs, kCurrentDelayMs, kTargetDelayMs, kJitterBufferMs,
      kMinPlayoutDelayMs, kRenderDelayMs);
  VideoReceiveStreamInterface::Stats stats = FlushAndGetStats();
  EXPECT_EQ(kMaxDecodeMs, stats.max_decode_ms);
  EXPECT_EQ(kCurrentDelayMs, stats.current_delay_ms);
  EXPECT_EQ(kTargetDelayMs, stats.target_delay_ms);
  EXPECT_EQ(kJitterBufferMs, stats.jitter_buffer_ms);
  EXPECT_EQ(kMinPlayoutDelayMs, stats.min_playout_delay_ms);
  EXPECT_EQ(kRenderDelayMs, stats.render_delay_ms);
}

TEST_F(ReceiveStatisticsProxy2Test, GetStatsReportsRtcpPacketTypeCounts) {
  const uint32_t kFirPackets = 33;
  const uint32_t kPliPackets = 44;
  const uint32_t kNackPackets = 55;
  RtcpPacketTypeCounter counter;
  counter.fir_packets = kFirPackets;
  counter.pli_packets = kPliPackets;
  counter.nack_packets = kNackPackets;
  statistics_proxy_->RtcpPacketTypesCounterUpdated(kRemoteSsrc, counter);
  VideoReceiveStreamInterface::Stats stats = statistics_proxy_->GetStats();
  EXPECT_EQ(kFirPackets, stats.rtcp_packet_type_counts.fir_packets);
  EXPECT_EQ(kPliPackets, stats.rtcp_packet_type_counts.pli_packets);
  EXPECT_EQ(kNackPackets, stats.rtcp_packet_type_counts.nack_packets);
}

TEST_F(ReceiveStatisticsProxy2Test,
       GetStatsReportsNoRtcpPacketTypeCountsForUnknownSsrc) {
  RtcpPacketTypeCounter counter;
  counter.fir_packets = 33;
  statistics_proxy_->RtcpPacketTypesCounterUpdated(kRemoteSsrc + 1, counter);
  EXPECT_EQ(0u,
            statistics_proxy_->GetStats().rtcp_packet_type_counts.fir_packets);
}

TEST_F(ReceiveStatisticsProxy2Test, GetStatsReportsFrameCounts) {
  const int kKeyFrames = 3;
  const int kDeltaFrames = 22;
  for (int i = 0; i < kKeyFrames; i++) {
    statistics_proxy_->OnCompleteFrame(true, 0, VideoContentType::UNSPECIFIED);
  }
  for (int i = 0; i < kDeltaFrames; i++) {
    statistics_proxy_->OnCompleteFrame(false, 0, VideoContentType::UNSPECIFIED);
  }

  VideoReceiveStreamInterface::Stats stats = statistics_proxy_->GetStats();
  EXPECT_EQ(kKeyFrames, stats.frame_counts.key_frames);
  EXPECT_EQ(kDeltaFrames, stats.frame_counts.delta_frames);
}

TEST_F(ReceiveStatisticsProxy2Test, GetStatsReportsCName) {
  const char* kName = "cName";
  statistics_proxy_->OnCname(kRemoteSsrc, kName);
  EXPECT_STREQ(kName, statistics_proxy_->GetStats().c_name.c_str());
}

TEST_F(ReceiveStatisticsProxy2Test, GetStatsReportsNoCNameForUnknownSsrc) {
  const char* kName = "cName";
  statistics_proxy_->OnCname(kRemoteSsrc + 1, kName);
  EXPECT_STREQ("", statistics_proxy_->GetStats().c_name.c_str());
}

TEST_F(ReceiveStatisticsProxy2Test, ReportsLongestTimingFrameInfo) {
  const int64_t kShortEndToEndDelay = 10;
  const int64_t kMedEndToEndDelay = 20;
  const int64_t kLongEndToEndDelay = 100;
  const uint32_t kExpectedRtpTimestamp = 2;
  TimingFrameInfo info;
  absl::optional<TimingFrameInfo> result;
  info.rtp_timestamp = kExpectedRtpTimestamp - 1;
  info.capture_time_ms = 0;
  info.decode_finish_ms = kShortEndToEndDelay;
  statistics_proxy_->OnTimingFrameInfoUpdated(info);
  info.rtp_timestamp =
      kExpectedRtpTimestamp;  // this frame should be reported in the end.
  info.capture_time_ms = 0;
  info.decode_finish_ms = kLongEndToEndDelay;
  statistics_proxy_->OnTimingFrameInfoUpdated(info);
  info.rtp_timestamp = kExpectedRtpTimestamp + 1;
  info.capture_time_ms = 0;
  info.decode_finish_ms = kMedEndToEndDelay;
  statistics_proxy_->OnTimingFrameInfoUpdated(info);
  result = FlushAndGetStats().timing_frame_info;
  EXPECT_TRUE(result);
  EXPECT_EQ(kExpectedRtpTimestamp, result->rtp_timestamp);
}

TEST_F(ReceiveStatisticsProxy2Test, RespectsReportingIntervalForTimingFrames) {
  TimingFrameInfo info;
  const int64_t kShortEndToEndDelay = 10;
  const uint32_t kExpectedRtpTimestamp = 2;
  const TimeDelta kShortDelay = TimeDelta::Seconds(1);
  const TimeDelta kLongDelay = TimeDelta::Seconds(10);
  absl::optional<TimingFrameInfo> result;
  info.rtp_timestamp = kExpectedRtpTimestamp;
  info.capture_time_ms = 0;
  info.decode_finish_ms = kShortEndToEndDelay;
  statistics_proxy_->OnTimingFrameInfoUpdated(info);
  time_controller_.AdvanceTime(kShortDelay);
  result = FlushAndGetStats().timing_frame_info;
  EXPECT_TRUE(result);
  EXPECT_EQ(kExpectedRtpTimestamp, result->rtp_timestamp);
  time_controller_.AdvanceTime(kLongDelay);
  result = statistics_proxy_->GetStats().timing_frame_info;
  EXPECT_FALSE(result);
}

TEST_F(ReceiveStatisticsProxy2Test, LifetimeHistogramIsUpdated) {
  const TimeDelta kLifetime = TimeDelta::Seconds(3);
  time_controller_.AdvanceTime(kLifetime);
  // Need at least one frame to report stream lifetime.
  statistics_proxy_->OnCompleteFrame(true, 1000, VideoContentType::UNSPECIFIED);
  statistics_proxy_->UpdateHistograms(absl::nullopt, StreamDataCounters(),
                                      nullptr);
  EXPECT_METRIC_EQ(
      1, metrics::NumSamples("WebRTC.Video.ReceiveStreamLifetimeInSeconds"));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.Video.ReceiveStreamLifetimeInSeconds",
                            kLifetime.seconds()));
}

TEST_F(ReceiveStatisticsProxy2Test,
       LifetimeHistogramNotReportedForEmptyStreams) {
  const TimeDelta kLifetime = TimeDelta::Seconds(3);
  time_controller_.AdvanceTime(kLifetime);
  // No frames received.
  statistics_proxy_->UpdateHistograms(absl::nullopt, StreamDataCounters(),
                                      nullptr);
  EXPECT_METRIC_EQ(
      0, metrics::NumSamples("WebRTC.Video.ReceiveStreamLifetimeInSeconds"));
}

TEST_F(ReceiveStatisticsProxy2Test, BadCallHistogramsAreUpdated) {
  // Based on the tuning parameters this will produce 7 uncertain states,
  // then 10 certainly bad states. There has to be 10 certain states before
  // any histograms are recorded.
  const int kNumBadSamples = 17;
  // We only count one sample per second.
  const TimeDelta kBadFameInterval = TimeDelta::Millis(1100);

  StreamDataCounters counters;
  counters.first_packet_time_ms = Now().ms();

  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);

  for (int i = 0; i < kNumBadSamples; ++i) {
    time_controller_.AdvanceTime(kBadFameInterval);
    statistics_proxy_->OnRenderedFrame(MetaData(frame));
  }
  statistics_proxy_->UpdateHistograms(absl::nullopt, counters, nullptr);
  EXPECT_METRIC_EQ(1, metrics::NumSamples("WebRTC.Video.BadCall.Any"));
  EXPECT_METRIC_EQ(1, metrics::NumEvents("WebRTC.Video.BadCall.Any", 100));

  EXPECT_METRIC_EQ(1, metrics::NumSamples("WebRTC.Video.BadCall.FrameRate"));
  EXPECT_METRIC_EQ(1,
                   metrics::NumEvents("WebRTC.Video.BadCall.FrameRate", 100));

  EXPECT_METRIC_EQ(
      0, metrics::NumSamples("WebRTC.Video.BadCall.FrameRateVariance"));

  EXPECT_METRIC_EQ(0, metrics::NumSamples("WebRTC.Video.BadCall.Qp"));
}

TEST_F(ReceiveStatisticsProxy2Test, PacketLossHistogramIsUpdated) {
  statistics_proxy_->UpdateHistograms(10, StreamDataCounters(), nullptr);
  EXPECT_METRIC_EQ(
      0, metrics::NumSamples("WebRTC.Video.ReceivedPacketsLostInPercent"));

  // Restart
  SetUp();

  // Min run time has passed.
  time_controller_.AdvanceTime(
      TimeDelta::Seconds(metrics::kMinRunTimeInSeconds));
  statistics_proxy_->UpdateHistograms(10, StreamDataCounters(), nullptr);
  EXPECT_METRIC_EQ(
      1, metrics::NumSamples("WebRTC.Video.ReceivedPacketsLostInPercent"));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.Video.ReceivedPacketsLostInPercent", 10));
}

TEST_F(ReceiveStatisticsProxy2Test, GetStatsReportsPlayoutTimestamp) {
  const int64_t kVideoNtpMs = 21;
  const int64_t kSyncOffsetMs = 22;
  const double kFreqKhz = 90.0;
  EXPECT_EQ(absl::nullopt,
            statistics_proxy_->GetStats().estimated_playout_ntp_timestamp_ms);
  statistics_proxy_->OnSyncOffsetUpdated(kVideoNtpMs, kSyncOffsetMs, kFreqKhz);
  EXPECT_EQ(kVideoNtpMs, FlushAndGetStats().estimated_playout_ntp_timestamp_ms);
  time_controller_.AdvanceTime(TimeDelta::Millis(13));
  EXPECT_EQ(kVideoNtpMs + 13,
            statistics_proxy_->GetStats().estimated_playout_ntp_timestamp_ms);
  time_controller_.AdvanceTime(TimeDelta::Millis(5));
  EXPECT_EQ(kVideoNtpMs + 13 + 5,
            statistics_proxy_->GetStats().estimated_playout_ntp_timestamp_ms);
}

TEST_F(ReceiveStatisticsProxy2Test, GetStatsReportsAvSyncOffset) {
  const int64_t kVideoNtpMs = 21;
  const int64_t kSyncOffsetMs = 22;
  const double kFreqKhz = 90.0;
  EXPECT_EQ(std::numeric_limits<int>::max(),
            statistics_proxy_->GetStats().sync_offset_ms);
  statistics_proxy_->OnSyncOffsetUpdated(kVideoNtpMs, kSyncOffsetMs, kFreqKhz);
  EXPECT_EQ(kSyncOffsetMs, FlushAndGetStats().sync_offset_ms);
}

TEST_F(ReceiveStatisticsProxy2Test, AvSyncOffsetHistogramIsUpdated) {
  const int64_t kVideoNtpMs = 21;
  const int64_t kSyncOffsetMs = 22;
  const double kFreqKhz = 90.0;
  for (int i = 0; i < kMinRequiredSamples; ++i) {
    statistics_proxy_->OnSyncOffsetUpdated(kVideoNtpMs, kSyncOffsetMs,
                                           kFreqKhz);
  }
  FlushAndUpdateHistograms(absl::nullopt, StreamDataCounters(), nullptr);
  EXPECT_METRIC_EQ(1, metrics::NumSamples("WebRTC.Video.AVSyncOffsetInMs"));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.Video.AVSyncOffsetInMs", kSyncOffsetMs));
}

TEST_F(ReceiveStatisticsProxy2Test, RtpToNtpFrequencyOffsetHistogramIsUpdated) {
  const int64_t kVideoNtpMs = 21;
  const int64_t kSyncOffsetMs = 22;
  const double kFreqKhz = 90.0;
  statistics_proxy_->OnSyncOffsetUpdated(kVideoNtpMs, kSyncOffsetMs, kFreqKhz);
  statistics_proxy_->OnSyncOffsetUpdated(kVideoNtpMs, kSyncOffsetMs,
                                         kFreqKhz + 2.2);
  time_controller_.AdvanceTime(kFreqOffsetProcessInterval);
  //) Process interval passed, max diff: 2.
  statistics_proxy_->OnSyncOffsetUpdated(kVideoNtpMs, kSyncOffsetMs,
                                         kFreqKhz + 1.1);
  statistics_proxy_->OnSyncOffsetUpdated(kVideoNtpMs, kSyncOffsetMs,
                                         kFreqKhz - 4.2);
  statistics_proxy_->OnSyncOffsetUpdated(kVideoNtpMs, kSyncOffsetMs,
                                         kFreqKhz - 0.9);
  time_controller_.AdvanceTime(kFreqOffsetProcessInterval);
  //) Process interval passed, max diff: 4.
  statistics_proxy_->OnSyncOffsetUpdated(kVideoNtpMs, kSyncOffsetMs, kFreqKhz);
  FlushAndUpdateHistograms(absl::nullopt, StreamDataCounters(), nullptr);
  // Average reported: (2 + 4) / 2 = 3.
  EXPECT_METRIC_EQ(1,
                   metrics::NumSamples("WebRTC.Video.RtpToNtpFreqOffsetInKhz"));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.Video.RtpToNtpFreqOffsetInKhz", 3));
}

TEST_F(ReceiveStatisticsProxy2Test, Vp8QpHistogramIsUpdated) {
  const int kQp = 22;

  for (int i = 0; i < kMinRequiredSamples; ++i)
    statistics_proxy_->OnPreDecode(kVideoCodecVP8, kQp);

  FlushAndUpdateHistograms(absl::nullopt, StreamDataCounters(), nullptr);
  EXPECT_METRIC_EQ(1, metrics::NumSamples("WebRTC.Video.Decoded.Vp8.Qp"));
  EXPECT_METRIC_EQ(1, metrics::NumEvents("WebRTC.Video.Decoded.Vp8.Qp", kQp));
}

TEST_F(ReceiveStatisticsProxy2Test,
       Vp8QpHistogramIsNotUpdatedForTooFewSamples) {
  const int kQp = 22;

  for (int i = 0; i < kMinRequiredSamples - 1; ++i)
    statistics_proxy_->OnPreDecode(kVideoCodecVP8, kQp);

  statistics_proxy_->UpdateHistograms(absl::nullopt, StreamDataCounters(),
                                      nullptr);
  EXPECT_METRIC_EQ(0, metrics::NumSamples("WebRTC.Video.Decoded.Vp8.Qp"));
}

TEST_F(ReceiveStatisticsProxy2Test, Vp8QpHistogramIsNotUpdatedIfNoQpValue) {
  for (int i = 0; i < kMinRequiredSamples; ++i)
    statistics_proxy_->OnPreDecode(kVideoCodecVP8, -1);

  statistics_proxy_->UpdateHistograms(absl::nullopt, StreamDataCounters(),
                                      nullptr);
  EXPECT_METRIC_EQ(0, metrics::NumSamples("WebRTC.Video.Decoded.Vp8.Qp"));
}

TEST_F(ReceiveStatisticsProxy2Test,
       KeyFrameHistogramNotUpdatedForTooFewSamples) {
  const bool kIsKeyFrame = false;
  const int kFrameSizeBytes = 1000;

  for (int i = 0; i < kMinRequiredSamples - 1; ++i)
    statistics_proxy_->OnCompleteFrame(kIsKeyFrame, kFrameSizeBytes,
                                       VideoContentType::UNSPECIFIED);

  EXPECT_EQ(0, statistics_proxy_->GetStats().frame_counts.key_frames);
  EXPECT_EQ(kMinRequiredSamples - 1,
            statistics_proxy_->GetStats().frame_counts.delta_frames);

  statistics_proxy_->UpdateHistograms(absl::nullopt, StreamDataCounters(),
                                      nullptr);
  EXPECT_METRIC_EQ(
      0, metrics::NumSamples("WebRTC.Video.KeyFramesReceivedInPermille"));
}

TEST_F(ReceiveStatisticsProxy2Test,
       KeyFrameHistogramUpdatedForMinRequiredSamples) {
  const bool kIsKeyFrame = false;
  const int kFrameSizeBytes = 1000;

  for (int i = 0; i < kMinRequiredSamples; ++i)
    statistics_proxy_->OnCompleteFrame(kIsKeyFrame, kFrameSizeBytes,
                                       VideoContentType::UNSPECIFIED);

  EXPECT_EQ(0, statistics_proxy_->GetStats().frame_counts.key_frames);
  EXPECT_EQ(kMinRequiredSamples,
            statistics_proxy_->GetStats().frame_counts.delta_frames);

  statistics_proxy_->UpdateHistograms(absl::nullopt, StreamDataCounters(),
                                      nullptr);
  EXPECT_METRIC_EQ(
      1, metrics::NumSamples("WebRTC.Video.KeyFramesReceivedInPermille"));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.Video.KeyFramesReceivedInPermille", 0));
}

TEST_F(ReceiveStatisticsProxy2Test, KeyFrameHistogramIsUpdated) {
  const int kFrameSizeBytes = 1000;

  for (int i = 0; i < kMinRequiredSamples; ++i)
    statistics_proxy_->OnCompleteFrame(true, kFrameSizeBytes,
                                       VideoContentType::UNSPECIFIED);

  for (int i = 0; i < kMinRequiredSamples; ++i)
    statistics_proxy_->OnCompleteFrame(false, kFrameSizeBytes,
                                       VideoContentType::UNSPECIFIED);

  EXPECT_EQ(kMinRequiredSamples,
            statistics_proxy_->GetStats().frame_counts.key_frames);
  EXPECT_EQ(kMinRequiredSamples,
            statistics_proxy_->GetStats().frame_counts.delta_frames);

  statistics_proxy_->UpdateHistograms(absl::nullopt, StreamDataCounters(),
                                      nullptr);
  EXPECT_METRIC_EQ(
      1, metrics::NumSamples("WebRTC.Video.KeyFramesReceivedInPermille"));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.Video.KeyFramesReceivedInPermille", 500));
}

TEST_F(ReceiveStatisticsProxy2Test,
       TimingHistogramsNotUpdatedForTooFewSamples) {
  const int kMaxDecodeMs = 2;
  const int kCurrentDelayMs = 3;
  const int kTargetDelayMs = 4;
  const int kJitterBufferMs = 5;
  const int kMinPlayoutDelayMs = 6;
  const int kRenderDelayMs = 7;

  for (int i = 0; i < kMinRequiredSamples - 1; ++i) {
    statistics_proxy_->OnFrameBufferTimingsUpdated(
        kMaxDecodeMs, kCurrentDelayMs, kTargetDelayMs, kJitterBufferMs,
        kMinPlayoutDelayMs, kRenderDelayMs);
  }

  statistics_proxy_->UpdateHistograms(absl::nullopt, StreamDataCounters(),
                                      nullptr);
  EXPECT_METRIC_EQ(0, metrics::NumSamples("WebRTC.Video.DecodeTimeInMs"));
  EXPECT_METRIC_EQ(0,
                   metrics::NumSamples("WebRTC.Video.JitterBufferDelayInMs"));
  EXPECT_METRIC_EQ(0, metrics::NumSamples("WebRTC.Video.TargetDelayInMs"));
  EXPECT_METRIC_EQ(0, metrics::NumSamples("WebRTC.Video.CurrentDelayInMs"));
  EXPECT_METRIC_EQ(0, metrics::NumSamples("WebRTC.Video.OnewayDelayInMs"));
}

TEST_F(ReceiveStatisticsProxy2Test, TimingHistogramsAreUpdated) {
  const int kMaxDecodeMs = 2;
  const int kCurrentDelayMs = 3;
  const int kTargetDelayMs = 4;
  const int kJitterBufferMs = 5;
  const int kMinPlayoutDelayMs = 6;
  const int kRenderDelayMs = 7;

  for (int i = 0; i < kMinRequiredSamples; ++i) {
    statistics_proxy_->OnFrameBufferTimingsUpdated(
        kMaxDecodeMs, kCurrentDelayMs, kTargetDelayMs, kJitterBufferMs,
        kMinPlayoutDelayMs, kRenderDelayMs);
  }

  FlushAndUpdateHistograms(absl::nullopt, StreamDataCounters(), nullptr);
  EXPECT_METRIC_EQ(1,
                   metrics::NumSamples("WebRTC.Video.JitterBufferDelayInMs"));
  EXPECT_METRIC_EQ(1, metrics::NumSamples("WebRTC.Video.TargetDelayInMs"));
  EXPECT_METRIC_EQ(1, metrics::NumSamples("WebRTC.Video.CurrentDelayInMs"));
  EXPECT_METRIC_EQ(1, metrics::NumSamples("WebRTC.Video.OnewayDelayInMs"));

  EXPECT_METRIC_EQ(1, metrics::NumEvents("WebRTC.Video.JitterBufferDelayInMs",
                                         kJitterBufferMs));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.Video.TargetDelayInMs", kTargetDelayMs));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.Video.CurrentDelayInMs", kCurrentDelayMs));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.Video.OnewayDelayInMs", kTargetDelayMs));
}

TEST_F(ReceiveStatisticsProxy2Test, DoesNotReportStaleFramerates) {
  const Frequency kDefaultFps = Frequency::Hertz(30);
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);

  for (int i = 0; i < kDefaultFps.hertz(); ++i) {
    // Since OnRenderedFrame is never called the fps in each sample will be 0,
    // i.e. bad
    frame.set_ntp_time_ms(
        time_controller_.GetClock()->CurrentNtpInMilliseconds());
    statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                      VideoContentType::UNSPECIFIED);
    statistics_proxy_->OnRenderedFrame(MetaData(frame));
    time_controller_.AdvanceTime(1 / kDefaultFps);
  }

  // Why -1? Because RateStatistics does not consider the first frame in the
  // rate as it will appear in the previous bucket.
  EXPECT_EQ(kDefaultFps.hertz() - 1,
            statistics_proxy_->GetStats().decode_frame_rate);
  EXPECT_EQ(kDefaultFps.hertz() - 1,
            statistics_proxy_->GetStats().render_frame_rate);

  // FPS trackers in stats proxy have a 1000ms sliding window.
  time_controller_.AdvanceTime(TimeDelta::Seconds(1));
  EXPECT_EQ(0, statistics_proxy_->GetStats().decode_frame_rate);
  EXPECT_EQ(0, statistics_proxy_->GetStats().render_frame_rate);
}

TEST_F(ReceiveStatisticsProxy2Test, GetStatsReportsReceivedFrameStats) {
  EXPECT_EQ(0, statistics_proxy_->GetStats().width);
  EXPECT_EQ(0, statistics_proxy_->GetStats().height);
  EXPECT_EQ(0u, statistics_proxy_->GetStats().frames_rendered);

  statistics_proxy_->OnRenderedFrame(MetaData(CreateFrame(kWidth, kHeight)));

  EXPECT_EQ(kWidth, statistics_proxy_->GetStats().width);
  EXPECT_EQ(kHeight, statistics_proxy_->GetStats().height);
  EXPECT_EQ(1u, statistics_proxy_->GetStats().frames_rendered);
}

TEST_F(ReceiveStatisticsProxy2Test,
       ReceivedFrameHistogramsAreNotUpdatedForTooFewSamples) {
  for (int i = 0; i < kMinRequiredSamples - 1; ++i) {
    statistics_proxy_->OnRenderedFrame(MetaData(CreateFrame(kWidth, kHeight)));
  }

  statistics_proxy_->UpdateHistograms(absl::nullopt, StreamDataCounters(),
                                      nullptr);
  EXPECT_METRIC_EQ(0,
                   metrics::NumSamples("WebRTC.Video.ReceivedWidthInPixels"));
  EXPECT_METRIC_EQ(0,
                   metrics::NumSamples("WebRTC.Video.ReceivedHeightInPixels"));
  EXPECT_METRIC_EQ(0,
                   metrics::NumSamples("WebRTC.Video.RenderFramesPerSecond"));
  EXPECT_METRIC_EQ(
      0, metrics::NumSamples("WebRTC.Video.RenderSqrtPixelsPerSecond"));
}

TEST_F(ReceiveStatisticsProxy2Test, ReceivedFrameHistogramsAreUpdated) {
  for (int i = 0; i < kMinRequiredSamples; ++i) {
    statistics_proxy_->OnRenderedFrame(MetaData(CreateFrame(kWidth, kHeight)));
  }

  statistics_proxy_->UpdateHistograms(absl::nullopt, StreamDataCounters(),
                                      nullptr);
  EXPECT_METRIC_EQ(1,
                   metrics::NumSamples("WebRTC.Video.ReceivedWidthInPixels"));
  EXPECT_METRIC_EQ(1,
                   metrics::NumSamples("WebRTC.Video.ReceivedHeightInPixels"));
  EXPECT_METRIC_EQ(1,
                   metrics::NumSamples("WebRTC.Video.RenderFramesPerSecond"));
  EXPECT_METRIC_EQ(
      1, metrics::NumSamples("WebRTC.Video.RenderSqrtPixelsPerSecond"));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.Video.ReceivedWidthInPixels", kWidth));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.Video.ReceivedHeightInPixels", kHeight));
}

TEST_F(ReceiveStatisticsProxy2Test, ZeroDelayReportedIfFrameNotDelayed) {
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);
  statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                    VideoContentType::UNSPECIFIED);

  // Frame not delayed, delayed frames to render: 0%.
  statistics_proxy_->OnRenderedFrame(
      MetaData(CreateFrameWithRenderTime(Now())));

  // Min run time has passed.
  time_controller_.AdvanceTime(
      TimeDelta::Seconds((metrics::kMinRunTimeInSeconds)));
  FlushAndUpdateHistograms(absl::nullopt, StreamDataCounters(), nullptr);
  EXPECT_METRIC_EQ(1,
                   metrics::NumSamples("WebRTC.Video.DelayedFramesToRenderer"));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.Video.DelayedFramesToRenderer", 0));
  EXPECT_METRIC_EQ(0, metrics::NumSamples(
                          "WebRTC.Video.DelayedFramesToRenderer_AvgDelayInMs"));
}

TEST_F(ReceiveStatisticsProxy2Test,
       DelayedFrameHistogramsAreNotUpdatedIfMinRuntimeHasNotPassed) {
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);
  statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                    VideoContentType::UNSPECIFIED);

  // Frame not delayed, delayed frames to render: 0%.
  statistics_proxy_->OnRenderedFrame(
      MetaData(CreateFrameWithRenderTime(Now())));

  // Min run time has not passed.
  time_controller_.AdvanceTime(
      TimeDelta::Seconds(metrics::kMinRunTimeInSeconds) - TimeDelta::Millis(1));
  statistics_proxy_->UpdateHistograms(absl::nullopt, StreamDataCounters(),
                                      nullptr);
  EXPECT_METRIC_EQ(0,
                   metrics::NumSamples("WebRTC.Video.DelayedFramesToRenderer"));
  EXPECT_METRIC_EQ(0, metrics::NumSamples(
                          "WebRTC.Video.DelayedFramesToRenderer_AvgDelayInMs"));
}

TEST_F(ReceiveStatisticsProxy2Test,
       DelayedFramesHistogramsAreNotUpdatedIfNoRenderedFrames) {
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);
  statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                    VideoContentType::UNSPECIFIED);

  // Min run time has passed. No rendered frames.
  time_controller_.AdvanceTime(
      TimeDelta::Seconds((metrics::kMinRunTimeInSeconds)));
  statistics_proxy_->UpdateHistograms(absl::nullopt, StreamDataCounters(),
                                      nullptr);
  EXPECT_METRIC_EQ(0,
                   metrics::NumSamples("WebRTC.Video.DelayedFramesToRenderer"));
  EXPECT_METRIC_EQ(0, metrics::NumSamples(
                          "WebRTC.Video.DelayedFramesToRenderer_AvgDelayInMs"));
}

TEST_F(ReceiveStatisticsProxy2Test, DelayReportedIfFrameIsDelayed) {
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);
  statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                    VideoContentType::UNSPECIFIED);

  // Frame delayed 1 ms, delayed frames to render: 100%.
  statistics_proxy_->OnRenderedFrame(
      MetaData(CreateFrameWithRenderTimeMs(Now().ms() - 1)));

  // Min run time has passed.
  time_controller_.AdvanceTime(
      TimeDelta::Seconds(metrics::kMinRunTimeInSeconds));
  FlushAndUpdateHistograms(absl::nullopt, StreamDataCounters(), nullptr);
  EXPECT_METRIC_EQ(1,
                   metrics::NumSamples("WebRTC.Video.DelayedFramesToRenderer"));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.Video.DelayedFramesToRenderer", 100));
  EXPECT_METRIC_EQ(1, metrics::NumSamples(
                          "WebRTC.Video.DelayedFramesToRenderer_AvgDelayInMs"));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.Video.DelayedFramesToRenderer_AvgDelayInMs",
                            1));
}

TEST_F(ReceiveStatisticsProxy2Test, AverageDelayOfDelayedFramesIsReported) {
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);
  statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                    VideoContentType::UNSPECIFIED);

  // Two frames delayed (6 ms, 10 ms), delayed frames to render: 50%.
  const int64_t kNowMs = Now().ms();

  statistics_proxy_->OnRenderedFrame(
      MetaData(CreateFrameWithRenderTimeMs(kNowMs - 10)));
  statistics_proxy_->OnRenderedFrame(
      MetaData(CreateFrameWithRenderTimeMs(kNowMs - 6)));
  statistics_proxy_->OnRenderedFrame(
      MetaData(CreateFrameWithRenderTimeMs(kNowMs)));
  statistics_proxy_->OnRenderedFrame(
      MetaData(CreateFrameWithRenderTimeMs(kNowMs + 1)));

  // Min run time has passed.
  time_controller_.AdvanceTime(
      TimeDelta::Seconds(metrics::kMinRunTimeInSeconds));
  FlushAndUpdateHistograms(absl::nullopt, StreamDataCounters(), nullptr);
  EXPECT_METRIC_EQ(1,
                   metrics::NumSamples("WebRTC.Video.DelayedFramesToRenderer"));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.Video.DelayedFramesToRenderer", 50));
  EXPECT_METRIC_EQ(1, metrics::NumSamples(
                          "WebRTC.Video.DelayedFramesToRenderer_AvgDelayInMs"));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.Video.DelayedFramesToRenderer_AvgDelayInMs",
                            8));
}

TEST_F(ReceiveStatisticsProxy2Test,
       RtcpHistogramsNotUpdatedIfMinRuntimeHasNotPassed) {
  StreamDataCounters data_counters;
  data_counters.first_packet_time_ms =
      time_controller_.GetClock()->TimeInMilliseconds();

  time_controller_.AdvanceTime(
      TimeDelta::Seconds(metrics::kMinRunTimeInSeconds) - TimeDelta::Millis(1));

  RtcpPacketTypeCounter counter;
  statistics_proxy_->RtcpPacketTypesCounterUpdated(kRemoteSsrc, counter);

  statistics_proxy_->UpdateHistograms(absl::nullopt, data_counters, nullptr);
  EXPECT_METRIC_EQ(0,
                   metrics::NumSamples("WebRTC.Video.FirPacketsSentPerMinute"));
  EXPECT_METRIC_EQ(0,
                   metrics::NumSamples("WebRTC.Video.PliPacketsSentPerMinute"));
  EXPECT_METRIC_EQ(
      0, metrics::NumSamples("WebRTC.Video.NackPacketsSentPerMinute"));
}

TEST_F(ReceiveStatisticsProxy2Test, RtcpHistogramsAreUpdated) {
  StreamDataCounters data_counters;
  data_counters.first_packet_time_ms =
      time_controller_.GetClock()->TimeInMilliseconds();
  time_controller_.AdvanceTime(
      TimeDelta::Seconds(metrics::kMinRunTimeInSeconds));

  const uint32_t kFirPackets = 100;
  const uint32_t kPliPackets = 200;
  const uint32_t kNackPackets = 300;

  RtcpPacketTypeCounter counter;
  counter.fir_packets = kFirPackets;
  counter.pli_packets = kPliPackets;
  counter.nack_packets = kNackPackets;
  statistics_proxy_->RtcpPacketTypesCounterUpdated(kRemoteSsrc, counter);

  statistics_proxy_->UpdateHistograms(absl::nullopt, data_counters, nullptr);
  EXPECT_METRIC_EQ(1,
                   metrics::NumSamples("WebRTC.Video.FirPacketsSentPerMinute"));
  EXPECT_METRIC_EQ(1,
                   metrics::NumSamples("WebRTC.Video.PliPacketsSentPerMinute"));
  EXPECT_METRIC_EQ(
      1, metrics::NumSamples("WebRTC.Video.NackPacketsSentPerMinute"));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.Video.FirPacketsSentPerMinute",
                            kFirPackets * 60 / metrics::kMinRunTimeInSeconds));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.Video.PliPacketsSentPerMinute",
                            kPliPackets * 60 / metrics::kMinRunTimeInSeconds));
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.Video.NackPacketsSentPerMinute",
                            kNackPackets * 60 / metrics::kMinRunTimeInSeconds));
}

class ReceiveStatisticsProxy2TestWithFreezeDuration
    : public ReceiveStatisticsProxy2Test,
      public ::testing::WithParamInterface<
          std::tuple<uint32_t, uint32_t, uint32_t>> {
 protected:
  const uint32_t frame_duration_ms_ = {std::get<0>(GetParam())};
  const uint32_t freeze_duration_ms_ = {std::get<1>(GetParam())};
  const uint32_t expected_freeze_count_ = {std::get<2>(GetParam())};
};

// It is a freeze if:
// frame_duration_ms >= max(3 * avg_frame_duration, avg_frame_duration + 150)
// where avg_frame_duration is average duration of last 30 frames including
// the current one.
//
// Condition 1: 3 * avg_frame_duration > avg_frame_duration + 150
const auto kFreezeDetectionCond1Freeze = std::make_tuple(150, 483, 1);
const auto kFreezeDetectionCond1NotFreeze = std::make_tuple(150, 482, 0);
// Condition 2: 3 * avg_frame_duration < avg_frame_duration + 150
const auto kFreezeDetectionCond2Freeze = std::make_tuple(30, 185, 1);
const auto kFreezeDetectionCond2NotFreeze = std::make_tuple(30, 184, 0);

INSTANTIATE_TEST_SUITE_P(_,
                         ReceiveStatisticsProxy2TestWithFreezeDuration,
                         ::testing::Values(kFreezeDetectionCond1Freeze,
                                           kFreezeDetectionCond1NotFreeze,
                                           kFreezeDetectionCond2Freeze,
                                           kFreezeDetectionCond2NotFreeze));

TEST_P(ReceiveStatisticsProxy2TestWithFreezeDuration, FreezeDetection) {
  VideoReceiveStreamInterface::Stats stats = statistics_proxy_->GetStats();
  EXPECT_EQ(0u, stats.freeze_count);
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);

  // Add a very long frame. This is need to verify that average frame
  // duration, which is supposed to be calculated as mean of durations of
  // last 30 frames, is calculated correctly.
  statistics_proxy_->OnRenderedFrame(MetaData(frame));
  time_controller_.AdvanceTime(TimeDelta::Seconds(2));
  for (size_t i = 0;
       i <= VideoQualityObserver::kAvgInterframeDelaysWindowSizeFrames; ++i) {
    time_controller_.AdvanceTime(TimeDelta::Millis(frame_duration_ms_));
    statistics_proxy_->OnRenderedFrame(MetaData(frame));
  }

  time_controller_.AdvanceTime(TimeDelta::Millis(freeze_duration_ms_));
  statistics_proxy_->OnRenderedFrame(MetaData(frame));

  stats = statistics_proxy_->GetStats();
  EXPECT_EQ(stats.freeze_count, expected_freeze_count_);
}

class ReceiveStatisticsProxy2TestWithContent
    : public ReceiveStatisticsProxy2Test,
      public ::testing::WithParamInterface<webrtc::VideoContentType> {
 protected:
  const webrtc::VideoContentType content_type_{GetParam()};
};

INSTANTIATE_TEST_SUITE_P(ContentTypes,
                         ReceiveStatisticsProxy2TestWithContent,
                         ::testing::Values(VideoContentType::UNSPECIFIED,
                                           VideoContentType::SCREENSHARE));

TEST_P(ReceiveStatisticsProxy2TestWithContent, InterFrameDelaysAreReported) {
  const TimeDelta kInterFrameDelay = TimeDelta::Millis(33);
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);

  for (int i = 0; i < kMinRequiredSamples; ++i) {
    statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                      content_type_);
    time_controller_.AdvanceTime(kInterFrameDelay);
  }
  // One extra with double the interval.
  time_controller_.AdvanceTime(kInterFrameDelay);
  statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                    content_type_);

  FlushAndUpdateHistograms(absl::nullopt, StreamDataCounters(), nullptr);
  const TimeDelta kExpectedInterFrame =
      (kInterFrameDelay * (kMinRequiredSamples - 1) + kInterFrameDelay * 2) /
      kMinRequiredSamples;
  if (videocontenttypehelpers::IsScreenshare(content_type_)) {
    EXPECT_METRIC_EQ(
        kExpectedInterFrame.ms(),
        metrics::MinSample("WebRTC.Video.Screenshare.InterframeDelayInMs"));
    EXPECT_METRIC_EQ(
        kInterFrameDelay.ms() * 2,
        metrics::MinSample("WebRTC.Video.Screenshare.InterframeDelayMaxInMs"));
  } else {
    EXPECT_METRIC_EQ(kExpectedInterFrame.ms(),
                     metrics::MinSample("WebRTC.Video.InterframeDelayInMs"));
    EXPECT_METRIC_EQ(kInterFrameDelay.ms() * 2,
                     metrics::MinSample("WebRTC.Video.InterframeDelayMaxInMs"));
  }
}

TEST_P(ReceiveStatisticsProxy2TestWithContent,
       InterFrameDelaysPercentilesAreReported) {
  const TimeDelta kInterFrameDelay = TimeDelta::Millis(33);
  const int kLastFivePercentsSamples = kMinRequiredSamples * 5 / 100;
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);

  for (int i = 0; i <= kMinRequiredSamples - kLastFivePercentsSamples; ++i) {
    time_controller_.AdvanceTime(kInterFrameDelay);
    statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                      content_type_);
  }
  // Last 5% of intervals are double in size.
  for (int i = 0; i < kLastFivePercentsSamples; ++i) {
    time_controller_.AdvanceTime(2 * kInterFrameDelay);
    statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                      content_type_);
  }
  // Final sample is outlier and 10 times as big.
  time_controller_.AdvanceTime(10 * kInterFrameDelay);
  statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                    content_type_);

  FlushAndUpdateHistograms(absl::nullopt, StreamDataCounters(), nullptr);
  const TimeDelta kExpectedInterFrame = kInterFrameDelay * 2;
  if (videocontenttypehelpers::IsScreenshare(content_type_)) {
    EXPECT_METRIC_EQ(
        kExpectedInterFrame.ms(),
        metrics::MinSample(
            "WebRTC.Video.Screenshare.InterframeDelay95PercentileInMs"));
  } else {
    EXPECT_METRIC_EQ(
        kExpectedInterFrame.ms(),
        metrics::MinSample("WebRTC.Video.InterframeDelay95PercentileInMs"));
  }
}

TEST_P(ReceiveStatisticsProxy2TestWithContent,
       MaxInterFrameDelayOnlyWithValidAverage) {
  const TimeDelta kInterFrameDelay = TimeDelta::Millis(33);
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);

  for (int i = 0; i < kMinRequiredSamples; ++i) {
    statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                      content_type_);
    time_controller_.AdvanceTime(kInterFrameDelay);
  }

  // `kMinRequiredSamples` samples, and thereby intervals, is required. That
  // means we're one frame short of having a valid data set.
  statistics_proxy_->UpdateHistograms(absl::nullopt, StreamDataCounters(),
                                      nullptr);
  EXPECT_METRIC_EQ(0, metrics::NumSamples("WebRTC.Video.InterframeDelayInMs"));
  EXPECT_METRIC_EQ(0,
                   metrics::NumSamples("WebRTC.Video.InterframeDelayMaxInMs"));
  EXPECT_METRIC_EQ(
      0, metrics::NumSamples("WebRTC.Video.Screenshare.InterframeDelayInMs"));
  EXPECT_METRIC_EQ(0, metrics::NumSamples(
                          "WebRTC.Video.Screenshare.InterframeDelayMaxInMs"));
}

TEST_P(ReceiveStatisticsProxy2TestWithContent,
       MaxInterFrameDelayOnlyWithPause) {
  const TimeDelta kInterFrameDelay = TimeDelta::Millis(33);
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);

  for (int i = 0; i <= kMinRequiredSamples; ++i) {
    statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                      content_type_);
    time_controller_.AdvanceTime(kInterFrameDelay);
  }

  // At this state, we should have a valid inter-frame delay.
  // Indicate stream paused and make a large jump in time.
  statistics_proxy_->OnStreamInactive();
  time_controller_.AdvanceTime(TimeDelta::Seconds(5));
  // Insert two more frames. The interval during the pause should be
  // disregarded in the stats.
  statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                    content_type_);
  time_controller_.AdvanceTime(kInterFrameDelay);
  statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                    content_type_);

  FlushAndUpdateHistograms(absl::nullopt, StreamDataCounters(), nullptr);
  if (videocontenttypehelpers::IsScreenshare(content_type_)) {
    EXPECT_METRIC_EQ(
        1, metrics::NumSamples("WebRTC.Video.Screenshare.InterframeDelayInMs"));
    EXPECT_METRIC_EQ(1, metrics::NumSamples(
                            "WebRTC.Video.Screenshare.InterframeDelayMaxInMs"));
    EXPECT_METRIC_EQ(
        kInterFrameDelay.ms(),
        metrics::MinSample("WebRTC.Video.Screenshare.InterframeDelayInMs"));
    EXPECT_METRIC_EQ(
        kInterFrameDelay.ms(),
        metrics::MinSample("WebRTC.Video.Screenshare.InterframeDelayMaxInMs"));
  } else {
    EXPECT_METRIC_EQ(1,
                     metrics::NumSamples("WebRTC.Video.InterframeDelayInMs"));
    EXPECT_METRIC_EQ(
        1, metrics::NumSamples("WebRTC.Video.InterframeDelayMaxInMs"));
    EXPECT_METRIC_EQ(kInterFrameDelay.ms(),
                     metrics::MinSample("WebRTC.Video.InterframeDelayInMs"));
    EXPECT_METRIC_EQ(kInterFrameDelay.ms(),
                     metrics::MinSample("WebRTC.Video.InterframeDelayMaxInMs"));
  }
}

TEST_P(ReceiveStatisticsProxy2TestWithContent, FreezesAreReported) {
  const TimeDelta kInterFrameDelay = TimeDelta::Millis(33);
  const TimeDelta kFreezeDelay = TimeDelta::Millis(200);
  const TimeDelta kCallDuration =
      kMinRequiredSamples * kInterFrameDelay + kFreezeDelay;
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);

  for (int i = 0; i < kMinRequiredSamples; ++i) {
    VideoFrameMetaData meta = MetaData(frame);
    statistics_proxy_->OnDecodedFrame(meta, absl::nullopt, TimeDelta::Zero(),
                                      TimeDelta::Zero(), TimeDelta::Zero(),
                                      content_type_);
    statistics_proxy_->OnRenderedFrame(meta);
    time_controller_.AdvanceTime(kInterFrameDelay);
  }
  // Add extra freeze.
  time_controller_.AdvanceTime(kFreezeDelay);
  VideoFrameMetaData meta = MetaData(frame);
  statistics_proxy_->OnDecodedFrame(meta, absl::nullopt, TimeDelta::Zero(),
                                    TimeDelta::Zero(), TimeDelta::Zero(),
                                    content_type_);
  statistics_proxy_->OnRenderedFrame(meta);

  FlushAndUpdateHistograms(absl::nullopt, StreamDataCounters(), nullptr);
  const TimeDelta kExpectedTimeBetweenFreezes =
      kInterFrameDelay * (kMinRequiredSamples - 1);
  const int kExpectedNumberFreezesPerMinute = 60 / kCallDuration.seconds();
  if (videocontenttypehelpers::IsScreenshare(content_type_)) {
    EXPECT_METRIC_EQ(
        (kFreezeDelay + kInterFrameDelay).ms(),
        metrics::MinSample("WebRTC.Video.Screenshare.MeanFreezeDurationMs"));
    EXPECT_METRIC_EQ(kExpectedTimeBetweenFreezes.ms(),
                     metrics::MinSample(
                         "WebRTC.Video.Screenshare.MeanTimeBetweenFreezesMs"));
    EXPECT_METRIC_EQ(
        kExpectedNumberFreezesPerMinute,
        metrics::MinSample("WebRTC.Video.Screenshare.NumberFreezesPerMinute"));
  } else {
    EXPECT_METRIC_EQ((kFreezeDelay + kInterFrameDelay).ms(),
                     metrics::MinSample("WebRTC.Video.MeanFreezeDurationMs"));
    EXPECT_METRIC_EQ(
        kExpectedTimeBetweenFreezes.ms(),
        metrics::MinSample("WebRTC.Video.MeanTimeBetweenFreezesMs"));
    EXPECT_METRIC_EQ(kExpectedNumberFreezesPerMinute,
                     metrics::MinSample("WebRTC.Video.NumberFreezesPerMinute"));
  }
}

TEST_P(ReceiveStatisticsProxy2TestWithContent, HarmonicFrameRateIsReported) {
  const TimeDelta kFrameDuration = TimeDelta::Millis(33);
  const TimeDelta kFreezeDuration = TimeDelta::Millis(200);
  const TimeDelta kPauseDuration = TimeDelta::Seconds(10);
  const TimeDelta kCallDuration =
      kMinRequiredSamples * kFrameDuration + kFreezeDuration + kPauseDuration;
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);

  for (int i = 0; i < kMinRequiredSamples; ++i) {
    time_controller_.AdvanceTime(kFrameDuration);
    statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                      content_type_);
    statistics_proxy_->OnRenderedFrame(MetaData(frame));
  }

  // Freezes and pauses should be included into harmonic frame rate.
  // Add freeze.
  time_controller_.AdvanceTime(kFreezeDuration);
  statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                    content_type_);
  statistics_proxy_->OnRenderedFrame(MetaData(frame));

  // Add pause.
  time_controller_.AdvanceTime(kPauseDuration);
  statistics_proxy_->OnStreamInactive();
  statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                    content_type_);
  statistics_proxy_->OnRenderedFrame(MetaData(frame));

  FlushAndUpdateHistograms(absl::nullopt, StreamDataCounters(), nullptr);
  double kSumSquaredFrameDurationSecs =
      (kMinRequiredSamples - 1) *
      (kFrameDuration.seconds<double>() * kFrameDuration.seconds<double>());
  kSumSquaredFrameDurationSecs +=
      kFreezeDuration.seconds<double>() * kFreezeDuration.seconds<double>();
  kSumSquaredFrameDurationSecs +=
      kPauseDuration.seconds<double>() * kPauseDuration.seconds<double>();
  const int kExpectedHarmonicFrameRateFps = std::round(
      kCallDuration.seconds<double>() / kSumSquaredFrameDurationSecs);
  if (videocontenttypehelpers::IsScreenshare(content_type_)) {
    EXPECT_METRIC_EQ(
        kExpectedHarmonicFrameRateFps,
        metrics::MinSample("WebRTC.Video.Screenshare.HarmonicFrameRate"));
  } else {
    EXPECT_METRIC_EQ(kExpectedHarmonicFrameRateFps,
                     metrics::MinSample("WebRTC.Video.HarmonicFrameRate"));
  }
}

TEST_P(ReceiveStatisticsProxy2TestWithContent, PausesAreIgnored) {
  const TimeDelta kInterFrameDelay = TimeDelta::Millis(33);
  const TimeDelta kPauseDuration = TimeDelta::Seconds(10);
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);

  for (int i = 0; i <= kMinRequiredSamples; ++i) {
    VideoFrameMetaData meta = MetaData(frame);
    statistics_proxy_->OnDecodedFrame(meta, absl::nullopt, TimeDelta::Zero(),
                                      TimeDelta::Zero(), TimeDelta::Zero(),
                                      content_type_);
    statistics_proxy_->OnRenderedFrame(meta);
    time_controller_.AdvanceTime(kInterFrameDelay);
  }
  // Add a pause.
  time_controller_.AdvanceTime(kPauseDuration);
  statistics_proxy_->OnStreamInactive();
  // Second playback interval with triple the length.
  for (int i = 0; i <= kMinRequiredSamples * 3; ++i) {
    VideoFrameMetaData meta = MetaData(frame);
    statistics_proxy_->OnDecodedFrame(meta, absl::nullopt, TimeDelta::Zero(),
                                      TimeDelta::Zero(), TimeDelta::Zero(),
                                      content_type_);
    statistics_proxy_->OnRenderedFrame(meta);
    time_controller_.AdvanceTime(kInterFrameDelay);
  }

  FlushAndUpdateHistograms(absl::nullopt, StreamDataCounters(), nullptr);
  // Average of two playback intervals.
  const TimeDelta kExpectedTimeBetweenFreezes =
      kInterFrameDelay * kMinRequiredSamples * 2;
  if (videocontenttypehelpers::IsScreenshare(content_type_)) {
    EXPECT_METRIC_EQ(-1, metrics::MinSample(
                             "WebRTC.Video.Screenshare.MeanFreezeDurationMs"));
    EXPECT_METRIC_EQ(kExpectedTimeBetweenFreezes.ms(),
                     metrics::MinSample(
                         "WebRTC.Video.Screenshare.MeanTimeBetweenFreezesMs"));
  } else {
    EXPECT_METRIC_EQ(-1,
                     metrics::MinSample("WebRTC.Video.MeanFreezeDurationMs"));
    EXPECT_METRIC_EQ(
        kExpectedTimeBetweenFreezes.ms(),
        metrics::MinSample("WebRTC.Video.MeanTimeBetweenFreezesMs"));
  }
}

TEST_P(ReceiveStatisticsProxy2TestWithContent, ManyPausesAtTheBeginning) {
  const TimeDelta kInterFrameDelay = TimeDelta::Millis(33);
  const TimeDelta kPauseDuration = TimeDelta::Seconds(10);
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);

  for (int i = 0; i <= kMinRequiredSamples; ++i) {
    statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                      content_type_);
    time_controller_.AdvanceTime(kInterFrameDelay);
    statistics_proxy_->OnStreamInactive();
    time_controller_.AdvanceTime(kPauseDuration);
    statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                      content_type_);
    time_controller_.AdvanceTime(kInterFrameDelay);
  }

  statistics_proxy_->UpdateHistograms(absl::nullopt, StreamDataCounters(),
                                      nullptr);
  // No freezes should be detected, as all long inter-frame delays were
  // pauses.
  if (videocontenttypehelpers::IsScreenshare(content_type_)) {
    EXPECT_METRIC_EQ(-1, metrics::MinSample(
                             "WebRTC.Video.Screenshare.MeanFreezeDurationMs"));
  } else {
    EXPECT_METRIC_EQ(-1,
                     metrics::MinSample("WebRTC.Video.MeanFreezeDurationMs"));
  }
}

TEST_P(ReceiveStatisticsProxy2TestWithContent, TimeInHdReported) {
  const TimeDelta kInterFrameDelay = TimeDelta::Millis(20);
  webrtc::VideoFrame frame_hd = CreateFrame(1280, 720);
  webrtc::VideoFrame frame_sd = CreateFrame(640, 360);

  // HD frames.
  for (int i = 0; i < kMinRequiredSamples; ++i) {
    VideoFrameMetaData meta = MetaData(frame_hd);
    statistics_proxy_->OnDecodedFrame(meta, absl::nullopt, TimeDelta::Zero(),
                                      TimeDelta::Zero(), TimeDelta::Zero(),
                                      content_type_);
    statistics_proxy_->OnRenderedFrame(meta);
    time_controller_.AdvanceTime(kInterFrameDelay);
  }
  // SD frames.
  for (int i = 0; i < 2 * kMinRequiredSamples; ++i) {
    VideoFrameMetaData meta = MetaData(frame_sd);
    statistics_proxy_->OnDecodedFrame(meta, absl::nullopt, TimeDelta::Zero(),
                                      TimeDelta::Zero(), TimeDelta::Zero(),
                                      content_type_);
    statistics_proxy_->OnRenderedFrame(meta);
    time_controller_.AdvanceTime(kInterFrameDelay);
  }
  // Extra last frame.
  statistics_proxy_->OnRenderedFrame(MetaData(frame_sd));

  statistics_proxy_->UpdateHistograms(absl::nullopt, StreamDataCounters(),
                                      nullptr);
  const int kExpectedTimeInHdPercents = 33;
  if (videocontenttypehelpers::IsScreenshare(content_type_)) {
    EXPECT_METRIC_EQ(
        kExpectedTimeInHdPercents,
        metrics::MinSample("WebRTC.Video.Screenshare.TimeInHdPercentage"));
  } else {
    EXPECT_METRIC_EQ(kExpectedTimeInHdPercents,
                     metrics::MinSample("WebRTC.Video.TimeInHdPercentage"));
  }
}

TEST_P(ReceiveStatisticsProxy2TestWithContent, TimeInBlockyVideoReported) {
  const TimeDelta kInterFrameDelay = TimeDelta::Millis(20);
  const int kHighQp = 80;
  const int kLowQp = 30;
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);

  // High quality frames.
  for (int i = 0; i < kMinRequiredSamples; ++i) {
    VideoFrameMetaData meta = MetaData(frame);
    statistics_proxy_->OnDecodedFrame(meta, kLowQp, TimeDelta::Zero(),
                                      TimeDelta::Zero(), TimeDelta::Zero(),
                                      content_type_);
    statistics_proxy_->OnRenderedFrame(meta);
    time_controller_.AdvanceTime(kInterFrameDelay);
  }
  // Blocky frames.
  for (int i = 0; i < 2 * kMinRequiredSamples; ++i) {
    VideoFrameMetaData meta = MetaData(frame);
    statistics_proxy_->OnDecodedFrame(meta, kHighQp, TimeDelta::Zero(),
                                      TimeDelta::Zero(), TimeDelta::Zero(),
                                      content_type_);
    statistics_proxy_->OnRenderedFrame(meta);
    time_controller_.AdvanceTime(kInterFrameDelay);
  }
  // Extra last frame.
  statistics_proxy_->OnDecodedFrame(frame, kHighQp, TimeDelta::Zero(),
                                    content_type_);
  statistics_proxy_->OnRenderedFrame(MetaData(frame));

  FlushAndUpdateHistograms(absl::nullopt, StreamDataCounters(), nullptr);
  const int kExpectedTimeInHdPercents = 66;
  if (videocontenttypehelpers::IsScreenshare(content_type_)) {
    EXPECT_METRIC_EQ(
        kExpectedTimeInHdPercents,
        metrics::MinSample(
            "WebRTC.Video.Screenshare.TimeInBlockyVideoPercentage"));
  } else {
    EXPECT_METRIC_EQ(
        kExpectedTimeInHdPercents,
        metrics::MinSample("WebRTC.Video.TimeInBlockyVideoPercentage"));
  }
}

TEST_P(ReceiveStatisticsProxy2TestWithContent, DownscalesReported) {
  // To ensure long enough call duration.
  const TimeDelta kInterFrameDelay = TimeDelta::Seconds(2);

  webrtc::VideoFrame frame_hd = CreateFrame(1280, 720);
  webrtc::VideoFrame frame_sd = CreateFrame(640, 360);
  webrtc::VideoFrame frame_ld = CreateFrame(320, 180);

  // Call once to pass content type.
  statistics_proxy_->OnDecodedFrame(frame_hd, absl::nullopt, TimeDelta::Zero(),
                                    content_type_);

  time_controller_.AdvanceTime(TimeDelta::Zero());
  statistics_proxy_->OnRenderedFrame(MetaData(frame_hd));
  time_controller_.AdvanceTime(kInterFrameDelay);
  // Downscale.
  statistics_proxy_->OnRenderedFrame(MetaData(frame_sd));
  time_controller_.AdvanceTime(kInterFrameDelay);
  // Downscale.
  statistics_proxy_->OnRenderedFrame(MetaData(frame_ld));
  time_controller_.AdvanceTime(kInterFrameDelay);
  statistics_proxy_->UpdateHistograms(absl::nullopt, StreamDataCounters(),
                                      nullptr);
  const int kExpectedDownscales = 30;  // 2 per 4 seconds = 30 per minute.
  if (videocontenttypehelpers::IsScreenshare(content_type_)) {
    EXPECT_METRIC_EQ(
        kExpectedDownscales,
        metrics::MinSample("WebRTC.Video.Screenshare."
                           "NumberResolutionDownswitchesPerMinute"));
  } else {
    EXPECT_METRIC_EQ(kExpectedDownscales,
                     metrics::MinSample(
                         "WebRTC.Video.NumberResolutionDownswitchesPerMinute"));
  }
}

TEST_P(ReceiveStatisticsProxy2TestWithContent, DecodeTimeReported) {
  const TimeDelta kInterFrameDelay = TimeDelta::Millis(20);
  const int kLowQp = 30;
  const TimeDelta kDecodeTime = TimeDelta::Millis(7);

  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);

  for (int i = 0; i < kMinRequiredSamples; ++i) {
    statistics_proxy_->OnDecodedFrame(frame, kLowQp, kDecodeTime,
                                      content_type_);
    time_controller_.AdvanceTime(kInterFrameDelay);
  }
  FlushAndUpdateHistograms(absl::nullopt, StreamDataCounters(), nullptr);
  EXPECT_METRIC_EQ(
      1, metrics::NumEvents("WebRTC.Video.DecodeTimeInMs", kDecodeTime.ms()));
}

TEST_P(ReceiveStatisticsProxy2TestWithContent,
       StatsAreSlicedOnSimulcastAndExperiment) {
  const uint8_t experiment_id = 1;
  webrtc::VideoContentType content_type = content_type_;
  videocontenttypehelpers::SetExperimentId(&content_type, experiment_id);
  const TimeDelta kInterFrameDelay1 = TimeDelta::Millis(30);
  const TimeDelta kInterFrameDelay2 = TimeDelta::Millis(50);
  webrtc::VideoFrame frame = CreateFrame(kWidth, kHeight);

  videocontenttypehelpers::SetSimulcastId(&content_type, 1);
  for (int i = 0; i <= kMinRequiredSamples; ++i) {
    time_controller_.AdvanceTime(kInterFrameDelay1);
    statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                      content_type);
  }

  videocontenttypehelpers::SetSimulcastId(&content_type, 2);
  for (int i = 0; i <= kMinRequiredSamples; ++i) {
    time_controller_.AdvanceTime(kInterFrameDelay2);
    statistics_proxy_->OnDecodedFrame(frame, absl::nullopt, TimeDelta::Zero(),
                                      content_type);
  }
  FlushAndUpdateHistograms(absl::nullopt, StreamDataCounters(), nullptr);

  if (videocontenttypehelpers::IsScreenshare(content_type)) {
    EXPECT_METRIC_EQ(
        1, metrics::NumSamples("WebRTC.Video.Screenshare.InterframeDelayInMs"));
    EXPECT_METRIC_EQ(1, metrics::NumSamples(
                            "WebRTC.Video.Screenshare.InterframeDelayMaxInMs"));
    EXPECT_METRIC_EQ(1, metrics::NumSamples(
                            "WebRTC.Video.Screenshare.InterframeDelayInMs.S0"));
    EXPECT_METRIC_EQ(1,
                     metrics::NumSamples(
                         "WebRTC.Video.Screenshare.InterframeDelayMaxInMs.S0"));
    EXPECT_METRIC_EQ(1, metrics::NumSamples(
                            "WebRTC.Video.Screenshare.InterframeDelayInMs.S1"));
    EXPECT_METRIC_EQ(1,
                     metrics::NumSamples(
                         "WebRTC.Video.Screenshare.InterframeDelayMaxInMs.S1"));
    EXPECT_METRIC_EQ(
        1, metrics::NumSamples("WebRTC.Video.Screenshare.InterframeDelayInMs"
                               ".ExperimentGroup0"));
    EXPECT_METRIC_EQ(
        1, metrics::NumSamples("WebRTC.Video.Screenshare.InterframeDelayMaxInMs"
                               ".ExperimentGroup0"));
    EXPECT_METRIC_EQ(
        kInterFrameDelay1.ms(),
        metrics::MinSample("WebRTC.Video.Screenshare.InterframeDelayInMs.S0"));
    EXPECT_METRIC_EQ(
        kInterFrameDelay2.ms(),
        metrics::MinSample("WebRTC.Video.Screenshare.InterframeDelayInMs.S1"));
    EXPECT_METRIC_EQ(
        ((kInterFrameDelay1 + kInterFrameDelay2) / 2).ms(),
        metrics::MinSample("WebRTC.Video.Screenshare.InterframeDelayInMs"));
    EXPECT_METRIC_EQ(
        kInterFrameDelay2.ms(),
        metrics::MinSample("WebRTC.Video.Screenshare.InterframeDelayMaxInMs"));
    EXPECT_METRIC_EQ(
        ((kInterFrameDelay1 + kInterFrameDelay2) / 2).ms(),
        metrics::MinSample(
            "WebRTC.Video.Screenshare.InterframeDelayInMs.ExperimentGroup0"));
  } else {
    EXPECT_METRIC_EQ(1,
                     metrics::NumSamples("WebRTC.Video.InterframeDelayInMs"));
    EXPECT_METRIC_EQ(
        1, metrics::NumSamples("WebRTC.Video.InterframeDelayMaxInMs"));
    EXPECT_METRIC_EQ(
        1, metrics::NumSamples("WebRTC.Video.InterframeDelayInMs.S0"));
    EXPECT_METRIC_EQ(
        1, metrics::NumSamples("WebRTC.Video.InterframeDelayMaxInMs.S0"));
    EXPECT_METRIC_EQ(
        1, metrics::NumSamples("WebRTC.Video.InterframeDelayInMs.S1"));
    EXPECT_METRIC_EQ(
        1, metrics::NumSamples("WebRTC.Video.InterframeDelayMaxInMs.S1"));
    EXPECT_METRIC_EQ(1, metrics::NumSamples("WebRTC.Video.InterframeDelayInMs"
                                            ".ExperimentGroup0"));
    EXPECT_METRIC_EQ(1,
                     metrics::NumSamples("WebRTC.Video.InterframeDelayMaxInMs"
                                         ".ExperimentGroup0"));
    EXPECT_METRIC_EQ(kInterFrameDelay1.ms(),
                     metrics::MinSample("WebRTC.Video.InterframeDelayInMs.S0"));
    EXPECT_METRIC_EQ(kInterFrameDelay2.ms(),
                     metrics::MinSample("WebRTC.Video.InterframeDelayInMs.S1"));
    EXPECT_METRIC_EQ((kInterFrameDelay1 + kInterFrameDelay2).ms() / 2,
                     metrics::MinSample("WebRTC.Video.InterframeDelayInMs"));
    EXPECT_METRIC_EQ(kInterFrameDelay2.ms(),
                     metrics::MinSample("WebRTC.Video.InterframeDelayMaxInMs"));
    EXPECT_METRIC_EQ((kInterFrameDelay1 + kInterFrameDelay2).ms() / 2,
                     metrics::MinSample(
                         "WebRTC.Video.InterframeDelayInMs.ExperimentGroup0"));
  }
}

}  // namespace internal
}  // namespace webrtc
