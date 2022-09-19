/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_frames_comparator.h"

#include <map>
#include <vector>

#include "api/units/timestamp.h"
#include "rtc_base/strings/string_builder.h"
#include "system_wrappers/include/clock.h"
#include "test/gtest.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_cpu_measurer.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_shared_objects.h"

namespace webrtc {
namespace {

using StatsSample = ::webrtc::SamplesStatsCounter::StatsSample;

constexpr int kMaxFramesInFlightPerStream = 10;

DefaultVideoQualityAnalyzerOptions AnalyzerOptionsForTest() {
  DefaultVideoQualityAnalyzerOptions options;
  options.compute_psnr = false;
  options.compute_ssim = false;
  options.adjust_cropping_before_comparing_frames = false;
  options.max_frames_in_flight_per_stream_count = kMaxFramesInFlightPerStream;
  return options;
}

StreamCodecInfo Vp8CodecForOneFrame(uint16_t frame_id, Timestamp time) {
  StreamCodecInfo info;
  info.codec_name = "VP8";
  info.first_frame_id = frame_id;
  info.last_frame_id = frame_id;
  info.switched_on_at = time;
  info.switched_from_at = time;
  return info;
}

FrameStats FrameStatsWith10msDeltaBetweenPhasesAnd10x10Frame(
    Timestamp captured_time) {
  FrameStats frame_stats(captured_time);
  frame_stats.pre_encode_time = captured_time + TimeDelta::Millis(10);
  frame_stats.encoded_time = captured_time + TimeDelta::Millis(20);
  frame_stats.received_time = captured_time + TimeDelta::Millis(30);
  frame_stats.decode_start_time = captured_time + TimeDelta::Millis(40);
  // Decode time is in microseconds.
  frame_stats.decode_end_time = captured_time + TimeDelta::Micros(40010);
  frame_stats.rendered_time = captured_time + TimeDelta::Millis(60);
  frame_stats.used_encoder = Vp8CodecForOneFrame(1, frame_stats.encoded_time);
  frame_stats.used_decoder =
      Vp8CodecForOneFrame(1, frame_stats.decode_end_time);
  frame_stats.rendered_frame_width = 10;
  frame_stats.rendered_frame_height = 10;
  return frame_stats;
}

FrameStats ShiftStatsOn(const FrameStats& stats, TimeDelta delta) {
  FrameStats frame_stats(stats.captured_time + delta);
  frame_stats.pre_encode_time = stats.pre_encode_time + delta;
  frame_stats.encoded_time = stats.encoded_time + delta;
  frame_stats.received_time = stats.received_time + delta;
  frame_stats.decode_start_time = stats.decode_start_time + delta;
  frame_stats.decode_end_time = stats.decode_end_time + delta;
  frame_stats.rendered_time = stats.rendered_time + delta;

  frame_stats.used_encoder = stats.used_encoder;
  frame_stats.used_decoder = stats.used_decoder;
  frame_stats.rendered_frame_width = stats.rendered_frame_width;
  frame_stats.rendered_frame_height = stats.rendered_frame_height;

  return frame_stats;
}

double GetFirstOrDie(const SamplesStatsCounter& counter) {
  EXPECT_TRUE(!counter.IsEmpty()) << "Counter has to be not empty";
  return counter.GetSamples()[0];
}

std::string ToString(const SamplesStatsCounter& counter) {
  rtc::StringBuilder out;
  for (const StatsSample& s : counter.GetTimedSamples()) {
    out << "{ time_ms=" << s.time.ms() << "; value=" << s.value << "}, ";
  }
  return out.str();
}

TEST(DefaultVideoQualityAnalyzerFramesComparatorTest,
     StatsPresentedAfterAddingOneComparison) {
  DefaultVideoQualityAnalyzerCpuMeasurer cpu_measurer;
  DefaultVideoQualityAnalyzerFramesComparator comparator(
      Clock::GetRealTimeClock(), cpu_measurer, AnalyzerOptionsForTest());

  Timestamp stream_start_time = Clock::GetRealTimeClock()->CurrentTime();
  size_t stream = 0;
  size_t sender = 0;
  size_t receiver = 1;
  size_t peers_count = 2;
  InternalStatsKey stats_key(stream, sender, receiver);

  FrameStats frame_stats =
      FrameStatsWith10msDeltaBetweenPhasesAnd10x10Frame(stream_start_time);

  comparator.Start(1);
  comparator.EnsureStatsForStream(stream, sender, peers_count,
                                  stream_start_time, stream_start_time);
  comparator.AddComparison(stats_key,
                           /*captured=*/absl::nullopt,
                           /*rendered=*/absl::nullopt,
                           FrameComparisonType::kRegular, frame_stats);
  comparator.Stop(/*last_rendered_frame_times=*/{});

  std::map<InternalStatsKey, StreamStats> stats = comparator.stream_stats();
  EXPECT_DOUBLE_EQ(GetFirstOrDie(stats.at(stats_key).transport_time_ms), 20.0);
  EXPECT_DOUBLE_EQ(
      GetFirstOrDie(stats.at(stats_key).total_delay_incl_transport_ms), 60.0);
  EXPECT_DOUBLE_EQ(GetFirstOrDie(stats.at(stats_key).encode_time_ms), 10.0);
  EXPECT_DOUBLE_EQ(GetFirstOrDie(stats.at(stats_key).decode_time_ms), 0.01);
  EXPECT_DOUBLE_EQ(GetFirstOrDie(stats.at(stats_key).receive_to_render_time_ms),
                   30.0);
  EXPECT_DOUBLE_EQ(
      GetFirstOrDie(stats.at(stats_key).resolution_of_rendered_frame), 100.0);
}

TEST(DefaultVideoQualityAnalyzerFramesComparatorTest,
     MultiFrameStatsPresentedAfterAddingTwoComparisonWith10msDelay) {
  DefaultVideoQualityAnalyzerCpuMeasurer cpu_measurer;
  DefaultVideoQualityAnalyzerFramesComparator comparator(
      Clock::GetRealTimeClock(), cpu_measurer, AnalyzerOptionsForTest());

  Timestamp stream_start_time = Clock::GetRealTimeClock()->CurrentTime();
  size_t stream = 0;
  size_t sender = 0;
  size_t receiver = 1;
  size_t peers_count = 2;
  InternalStatsKey stats_key(stream, sender, receiver);

  FrameStats frame_stats1 =
      FrameStatsWith10msDeltaBetweenPhasesAnd10x10Frame(stream_start_time);
  FrameStats frame_stats2 = FrameStatsWith10msDeltaBetweenPhasesAnd10x10Frame(
      stream_start_time + TimeDelta::Millis(15));
  frame_stats2.prev_frame_rendered_time = frame_stats1.rendered_time;

  comparator.Start(1);
  comparator.EnsureStatsForStream(stream, sender, peers_count,
                                  stream_start_time, stream_start_time);
  comparator.AddComparison(stats_key,
                           /*captured=*/absl::nullopt,
                           /*rendered=*/absl::nullopt,
                           FrameComparisonType::kRegular, frame_stats1);
  comparator.AddComparison(stats_key,
                           /*captured=*/absl::nullopt,
                           /*rendered=*/absl::nullopt,
                           FrameComparisonType::kRegular, frame_stats2);
  comparator.Stop(/*last_rendered_frame_times=*/{});

  std::map<InternalStatsKey, StreamStats> stats = comparator.stream_stats();
  EXPECT_DOUBLE_EQ(
      GetFirstOrDie(stats.at(stats_key).time_between_rendered_frames_ms), 15.0);
  EXPECT_DOUBLE_EQ(stats.at(stats_key).encode_frame_rate.GetEventsPerSecond(),
                   2.0 / 15 * 1000)
      << "There should be 2 events with interval of 15 ms";
  ;
}

TEST(DefaultVideoQualityAnalyzerFramesComparatorTest,
     FrameInFlightStatsAreHandledCorrectly) {
  DefaultVideoQualityAnalyzerCpuMeasurer cpu_measurer;
  DefaultVideoQualityAnalyzerFramesComparator comparator(
      Clock::GetRealTimeClock(), cpu_measurer, AnalyzerOptionsForTest());

  Timestamp stream_start_time = Clock::GetRealTimeClock()->CurrentTime();
  size_t stream = 0;
  size_t sender = 0;
  size_t receiver = 1;
  size_t peers_count = 2;
  InternalStatsKey stats_key(stream, sender, receiver);

  // There are 7 different timings inside frame stats: captured, pre_encode,
  // encoded, received, decode_start, decode_end, rendered. captured is always
  // set and received is set together with decode_start. So we create 6
  // different frame stats with interval of 15 ms, where for each stat next
  // timings will be set
  //   * 1st - captured
  //   * 2nd - captured, pre_encode
  //   * 3rd - captured, pre_encode, encoded
  //   * 4th - captured, pre_encode, encoded, received, decode_start
  //   * 5th - captured, pre_encode, encoded, received, decode_start, decode_end
  //   * 6th - all of them set
  std::vector<FrameStats> stats;
  // 1st stat
  FrameStats frame_stats(stream_start_time);
  stats.push_back(frame_stats);
  // 2nd stat
  frame_stats = ShiftStatsOn(frame_stats, TimeDelta::Millis(15));
  frame_stats.pre_encode_time =
      frame_stats.captured_time + TimeDelta::Millis(10);
  stats.push_back(frame_stats);
  // 3rd stat
  frame_stats = ShiftStatsOn(frame_stats, TimeDelta::Millis(15));
  frame_stats.encoded_time = frame_stats.captured_time + TimeDelta::Millis(20);
  frame_stats.used_encoder = Vp8CodecForOneFrame(1, frame_stats.encoded_time);
  stats.push_back(frame_stats);
  // 4th stat
  frame_stats = ShiftStatsOn(frame_stats, TimeDelta::Millis(15));
  frame_stats.received_time = frame_stats.captured_time + TimeDelta::Millis(30);
  frame_stats.decode_start_time =
      frame_stats.captured_time + TimeDelta::Millis(40);
  stats.push_back(frame_stats);
  // 5th stat
  frame_stats = ShiftStatsOn(frame_stats, TimeDelta::Millis(15));
  frame_stats.decode_end_time =
      frame_stats.captured_time + TimeDelta::Millis(50);
  frame_stats.used_decoder =
      Vp8CodecForOneFrame(1, frame_stats.decode_end_time);
  stats.push_back(frame_stats);
  // 6th stat
  frame_stats = ShiftStatsOn(frame_stats, TimeDelta::Millis(15));
  frame_stats.rendered_time = frame_stats.captured_time + TimeDelta::Millis(60);
  frame_stats.rendered_frame_width = 10;
  frame_stats.rendered_frame_height = 10;
  stats.push_back(frame_stats);

  comparator.Start(1);
  comparator.EnsureStatsForStream(stream, sender, peers_count,
                                  stream_start_time, stream_start_time);
  for (size_t i = 0; i < stats.size() - 1; ++i) {
    comparator.AddComparison(stats_key,
                             /*captured=*/absl::nullopt,
                             /*rendered=*/absl::nullopt,
                             FrameComparisonType::kFrameInFlight, stats[i]);
  }
  comparator.AddComparison(stats_key,
                           /*captured=*/absl::nullopt,
                           /*rendered=*/absl::nullopt,
                           FrameComparisonType::kRegular,
                           stats[stats.size() - 1]);
  comparator.Stop(/*last_rendered_frame_times=*/{});

  EXPECT_EQ(comparator.stream_stats().size(), 1lu);
  StreamStats result_stats = comparator.stream_stats().at(stats_key);

  EXPECT_DOUBLE_EQ(result_stats.transport_time_ms.GetAverage(), 20.0)
      << ToString(result_stats.transport_time_ms);
  EXPECT_EQ(result_stats.transport_time_ms.NumSamples(), 3);

  EXPECT_DOUBLE_EQ(result_stats.total_delay_incl_transport_ms.GetAverage(),
                   60.0)
      << ToString(result_stats.total_delay_incl_transport_ms);
  EXPECT_EQ(result_stats.total_delay_incl_transport_ms.NumSamples(), 1);

  EXPECT_DOUBLE_EQ(result_stats.encode_time_ms.GetAverage(), 10)
      << ToString(result_stats.encode_time_ms);
  EXPECT_EQ(result_stats.encode_time_ms.NumSamples(), 4);

  EXPECT_DOUBLE_EQ(result_stats.decode_time_ms.GetAverage(), 10)
      << ToString(result_stats.decode_time_ms);
  EXPECT_EQ(result_stats.decode_time_ms.NumSamples(), 2);

  EXPECT_DOUBLE_EQ(result_stats.receive_to_render_time_ms.GetAverage(), 30)
      << ToString(result_stats.receive_to_render_time_ms);
  EXPECT_EQ(result_stats.receive_to_render_time_ms.NumSamples(), 1);

  EXPECT_DOUBLE_EQ(result_stats.resolution_of_rendered_frame.GetAverage(), 100)
      << ToString(result_stats.resolution_of_rendered_frame);
  EXPECT_EQ(result_stats.resolution_of_rendered_frame.NumSamples(), 1);

  EXPECT_DOUBLE_EQ(result_stats.encode_frame_rate.GetEventsPerSecond(),
                   4.0 / 45 * 1000)
      << "There should be 4 events with interval of 15 ms";
}

}  // namespace
}  // namespace webrtc
