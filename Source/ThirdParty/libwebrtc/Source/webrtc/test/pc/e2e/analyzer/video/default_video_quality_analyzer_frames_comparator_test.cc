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
#include <string>
#include <vector>

#include "api/test/create_frame_generator.h"
#include "api/units/timestamp.h"
#include "rtc_base/strings/string_builder.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_cpu_measurer.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer_shared_objects.h"

namespace webrtc {
namespace {

using ::testing::Contains;
using ::testing::DoubleEq;
using ::testing::Each;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::SizeIs;

using StatsSample = ::webrtc::SamplesStatsCounter::StatsSample;

DefaultVideoQualityAnalyzerOptions AnalyzerOptionsForTest() {
  DefaultVideoQualityAnalyzerOptions options;
  options.compute_psnr = false;
  options.compute_ssim = false;
  options.adjust_cropping_before_comparing_frames = false;
  return options;
}

VideoFrame CreateFrame(uint16_t frame_id,
                       int width,
                       int height,
                       Timestamp timestamp) {
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      test::CreateSquareFrameGenerator(width, height,
                                       /*type=*/std::nullopt,
                                       /*num_squares=*/std::nullopt);
  test::FrameGeneratorInterface::VideoFrameData frame_data =
      frame_generator->NextFrame();
  return VideoFrame::Builder()
      .set_id(frame_id)
      .set_video_frame_buffer(frame_data.buffer)
      .set_update_rect(frame_data.update_rect)
      .set_timestamp_us(timestamp.us())
      .build();
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
    uint16_t frame_id,
    Timestamp captured_time) {
  FrameStats frame_stats(frame_id, captured_time);
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
  frame_stats.decoded_frame_width = 10;
  frame_stats.decoded_frame_height = 10;
  return frame_stats;
}

FrameStats ShiftStatsOn(const FrameStats& stats, TimeDelta delta) {
  FrameStats frame_stats(stats.frame_id, stats.captured_time + delta);
  frame_stats.pre_encode_time = stats.pre_encode_time + delta;
  frame_stats.encoded_time = stats.encoded_time + delta;
  frame_stats.received_time = stats.received_time + delta;
  frame_stats.decode_start_time = stats.decode_start_time + delta;
  frame_stats.decode_end_time = stats.decode_end_time + delta;
  frame_stats.rendered_time = stats.rendered_time + delta;

  frame_stats.used_encoder = stats.used_encoder;
  frame_stats.used_decoder = stats.used_decoder;
  frame_stats.decoded_frame_width = stats.decoded_frame_width;
  frame_stats.decoded_frame_height = stats.decoded_frame_height;

  return frame_stats;
}

SamplesStatsCounter StatsCounter(
    const std::vector<std::pair<double, Timestamp>>& samples) {
  SamplesStatsCounter counter;
  for (const std::pair<double, Timestamp>& sample : samples) {
    counter.AddSample(SamplesStatsCounter::StatsSample{.value = sample.first,
                                                       .time = sample.second});
  }
  return counter;
}

double GetFirstOrDie(const SamplesStatsCounter& counter) {
  EXPECT_FALSE(counter.IsEmpty()) << "Counter has to be not empty";
  return counter.GetSamples()[0];
}

void AssertFirstMetadataHasField(const SamplesStatsCounter& counter,
                                 const std::string& field_name,
                                 const std::string& field_value) {
  EXPECT_FALSE(counter.IsEmpty()) << "Coutner has to be not empty";
  EXPECT_THAT(counter.GetTimedSamples()[0].metadata,
              Contains(Pair(field_name, field_value)));
}

std::string ToString(const SamplesStatsCounter& counter) {
  rtc::StringBuilder out;
  for (const StatsSample& s : counter.GetTimedSamples()) {
    out << "{ time_ms=" << s.time.ms() << "; value=" << s.value << "}, ";
  }
  return out.str();
}

void ExpectEmpty(const SamplesStatsCounter& counter) {
  EXPECT_TRUE(counter.IsEmpty())
      << "Expected empty SamplesStatsCounter, but got " << ToString(counter);
}

void ExpectEmpty(const SamplesRateCounter& counter) {
  EXPECT_TRUE(counter.IsEmpty())
      << "Expected empty SamplesRateCounter, but got "
      << counter.GetEventsPerSecond();
}

void ExpectSizeAndAllElementsAre(const SamplesStatsCounter& counter,
                                 int size,
                                 double value) {
  EXPECT_EQ(counter.NumSamples(), size);
  EXPECT_THAT(counter.GetSamples(), Each(DoubleEq(value)));
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

  FrameStats frame_stats = FrameStatsWith10msDeltaBetweenPhasesAnd10x10Frame(
      /*frame_id=*/1, stream_start_time);

  comparator.Start(/*max_threads_count=*/1);
  comparator.EnsureStatsForStream(stream, sender, peers_count,
                                  stream_start_time, stream_start_time);
  comparator.AddComparison(stats_key,
                           /*captured=*/std::nullopt,
                           /*rendered=*/std::nullopt,
                           FrameComparisonType::kRegular, frame_stats);
  comparator.Stop(/*last_rendered_frame_times=*/{});

  std::map<InternalStatsKey, StreamStats> stats = comparator.stream_stats();
  ExpectSizeAndAllElementsAre(stats.at(stats_key).transport_time_ms, /*size=*/1,
                              /*value=*/20.0);
  ExpectSizeAndAllElementsAre(stats.at(stats_key).total_delay_incl_transport_ms,
                              /*size=*/1, /*value=*/60.0);
  ExpectSizeAndAllElementsAre(stats.at(stats_key).encode_time_ms, /*size=*/1,
                              /*value=*/10.0);
  ExpectSizeAndAllElementsAre(stats.at(stats_key).decode_time_ms, /*size=*/1,
                              /*value=*/0.01);
  ExpectSizeAndAllElementsAre(stats.at(stats_key).receive_to_render_time_ms,
                              /*size=*/1, /*value=*/30.0);
  ExpectSizeAndAllElementsAre(stats.at(stats_key).resolution_of_decoded_frame,
                              /*size=*/1, /*value=*/100.0);
}

TEST(
    DefaultVideoQualityAnalyzerFramesComparatorTest,
    MultiFrameStatsPresentedWithMetadataAfterAddingTwoComparisonWith10msDelay) {
  DefaultVideoQualityAnalyzerCpuMeasurer cpu_measurer;
  DefaultVideoQualityAnalyzerFramesComparator comparator(
      Clock::GetRealTimeClock(), cpu_measurer, AnalyzerOptionsForTest());

  Timestamp stream_start_time = Clock::GetRealTimeClock()->CurrentTime();
  size_t stream = 0;
  size_t sender = 0;
  size_t receiver = 1;
  size_t peers_count = 2;
  InternalStatsKey stats_key(stream, sender, receiver);

  FrameStats frame_stats1 = FrameStatsWith10msDeltaBetweenPhasesAnd10x10Frame(
      /*frame_id=*/1, stream_start_time);
  FrameStats frame_stats2 = FrameStatsWith10msDeltaBetweenPhasesAnd10x10Frame(
      /*frame_id=*/2, stream_start_time + TimeDelta::Millis(15));
  frame_stats2.prev_frame_rendered_time = frame_stats1.rendered_time;
  frame_stats2.time_between_rendered_frames =
      frame_stats2.rendered_time - frame_stats1.rendered_time;

  comparator.Start(/*max_threads_count=*/1);
  comparator.EnsureStatsForStream(stream, sender, peers_count,
                                  stream_start_time, stream_start_time);
  comparator.AddComparison(stats_key,
                           /*captured=*/std::nullopt,
                           /*rendered=*/std::nullopt,
                           FrameComparisonType::kRegular, frame_stats1);
  comparator.AddComparison(stats_key,
                           /*captured=*/std::nullopt,
                           /*rendered=*/std::nullopt,
                           FrameComparisonType::kRegular, frame_stats2);
  comparator.Stop(/*last_rendered_frame_times=*/{});

  std::map<InternalStatsKey, StreamStats> stats = comparator.stream_stats();
  ExpectSizeAndAllElementsAre(
      stats.at(stats_key).time_between_rendered_frames_ms, /*size=*/1,
      /*value=*/15.0);
  AssertFirstMetadataHasField(
      stats.at(stats_key).time_between_rendered_frames_ms, "frame_id", "2");
  EXPECT_DOUBLE_EQ(stats.at(stats_key).encode_frame_rate.GetEventsPerSecond(),
                   2.0 / 15 * 1000)
      << "There should be 2 events with interval of 15 ms";
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
  FrameStats frame_stats(/*frame_id=*/1, stream_start_time);
  stats.push_back(frame_stats);
  // 2nd stat
  frame_stats = ShiftStatsOn(frame_stats, TimeDelta::Millis(15));
  frame_stats.frame_id = 2;
  frame_stats.pre_encode_time =
      frame_stats.captured_time + TimeDelta::Millis(10);
  stats.push_back(frame_stats);
  // 3rd stat
  frame_stats = ShiftStatsOn(frame_stats, TimeDelta::Millis(15));
  frame_stats.frame_id = 3;
  frame_stats.encoded_time = frame_stats.captured_time + TimeDelta::Millis(20);
  frame_stats.used_encoder = Vp8CodecForOneFrame(1, frame_stats.encoded_time);
  stats.push_back(frame_stats);
  // 4th stat
  frame_stats = ShiftStatsOn(frame_stats, TimeDelta::Millis(15));
  frame_stats.frame_id = 4;
  frame_stats.received_time = frame_stats.captured_time + TimeDelta::Millis(30);
  frame_stats.decode_start_time =
      frame_stats.captured_time + TimeDelta::Millis(40);
  stats.push_back(frame_stats);
  // 5th stat
  frame_stats = ShiftStatsOn(frame_stats, TimeDelta::Millis(15));
  frame_stats.frame_id = 5;
  frame_stats.decode_end_time =
      frame_stats.captured_time + TimeDelta::Millis(50);
  frame_stats.used_decoder =
      Vp8CodecForOneFrame(1, frame_stats.decode_end_time);
  frame_stats.decoded_frame_width = 10;
  frame_stats.decoded_frame_height = 10;
  stats.push_back(frame_stats);
  // 6th stat
  frame_stats = ShiftStatsOn(frame_stats, TimeDelta::Millis(15));
  frame_stats.frame_id = 6;
  frame_stats.rendered_time = frame_stats.captured_time + TimeDelta::Millis(60);
  stats.push_back(frame_stats);

  comparator.Start(/*max_threads_count=*/1);
  comparator.EnsureStatsForStream(stream, sender, peers_count,
                                  stream_start_time, stream_start_time);
  for (size_t i = 0; i < stats.size() - 1; ++i) {
    comparator.AddComparison(stats_key,
                             /*captured=*/std::nullopt,
                             /*rendered=*/std::nullopt,
                             FrameComparisonType::kFrameInFlight, stats[i]);
  }
  comparator.AddComparison(stats_key,
                           /*captured=*/std::nullopt,
                           /*rendered=*/std::nullopt,
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

  EXPECT_DOUBLE_EQ(result_stats.resolution_of_decoded_frame.GetAverage(), 100)
      << ToString(result_stats.resolution_of_decoded_frame);
  EXPECT_EQ(result_stats.resolution_of_decoded_frame.NumSamples(), 2);

  EXPECT_DOUBLE_EQ(result_stats.encode_frame_rate.GetEventsPerSecond(),
                   4.0 / 45 * 1000)
      << "There should be 4 events with interval of 15 ms";
}

// Tests to validate that stats for each possible input frame are computed
// correctly.
// Frame in flight start
TEST(DefaultVideoQualityAnalyzerFramesComparatorTest,
     CapturedOnlyInFlightFrameAccountedInStats) {
  DefaultVideoQualityAnalyzerCpuMeasurer cpu_measurer;
  DefaultVideoQualityAnalyzerFramesComparator comparator(
      Clock::GetRealTimeClock(), cpu_measurer,
      DefaultVideoQualityAnalyzerOptions());

  Timestamp captured_time = Clock::GetRealTimeClock()->CurrentTime();
  size_t stream = 0;
  size_t sender = 0;
  size_t receiver = 1;
  InternalStatsKey stats_key(stream, sender, receiver);

  // Frame captured
  FrameStats frame_stats(/*frame_id=*/1, captured_time);

  comparator.Start(/*max_threads_count=*/1);
  comparator.EnsureStatsForStream(stream, sender, /*peers_count=*/2,
                                  captured_time, captured_time);
  comparator.AddComparison(stats_key,
                           /*captured=*/std::nullopt,
                           /*rendered=*/std::nullopt,
                           FrameComparisonType::kFrameInFlight, frame_stats);
  comparator.Stop(/*last_rendered_frame_times=*/{});

  EXPECT_EQ(comparator.stream_stats().size(), 1lu);
  StreamStats stats = comparator.stream_stats().at(stats_key);
  EXPECT_EQ(stats.stream_started_time, captured_time);
  ExpectEmpty(stats.psnr);
  ExpectEmpty(stats.ssim);
  ExpectEmpty(stats.transport_time_ms);
  ExpectEmpty(stats.total_delay_incl_transport_ms);
  ExpectEmpty(stats.time_between_rendered_frames_ms);
  ExpectEmpty(stats.encode_frame_rate);
  ExpectEmpty(stats.encode_time_ms);
  ExpectEmpty(stats.decode_time_ms);
  ExpectEmpty(stats.receive_to_render_time_ms);
  ExpectEmpty(stats.skipped_between_rendered);
  ExpectSizeAndAllElementsAre(stats.freeze_time_ms, /*size=*/1, /*value=*/0);
  ExpectEmpty(stats.time_between_freezes_ms);
  ExpectEmpty(stats.resolution_of_decoded_frame);
  ExpectEmpty(stats.target_encode_bitrate);
  EXPECT_THAT(stats.spatial_layers_qp, IsEmpty());
  ExpectEmpty(stats.rendered_frame_qp);
  ExpectEmpty(stats.recv_key_frame_size_bytes);
  ExpectEmpty(stats.recv_delta_frame_size_bytes);
  EXPECT_EQ(stats.total_encoded_images_payload, 0);
  EXPECT_EQ(stats.num_send_key_frames, 0);
  EXPECT_EQ(stats.num_recv_key_frames, 0);
  EXPECT_THAT(stats.dropped_by_phase, Eq(std::map<FrameDropPhase, int64_t>{
                                          {FrameDropPhase::kBeforeEncoder, 0},
                                          {FrameDropPhase::kByEncoder, 0},
                                          {FrameDropPhase::kTransport, 0},
                                          {FrameDropPhase::kByDecoder, 0},
                                          {FrameDropPhase::kAfterDecoder, 0}}));
  EXPECT_THAT(stats.encoders, IsEmpty());
  EXPECT_THAT(stats.decoders, IsEmpty());
}

TEST(DefaultVideoQualityAnalyzerFramesComparatorTest,
     PreEncodedInFlightFrameAccountedInStats) {
  DefaultVideoQualityAnalyzerCpuMeasurer cpu_measurer;
  DefaultVideoQualityAnalyzerFramesComparator comparator(
      Clock::GetRealTimeClock(), cpu_measurer,
      DefaultVideoQualityAnalyzerOptions());

  Timestamp captured_time = Clock::GetRealTimeClock()->CurrentTime();
  size_t stream = 0;
  size_t sender = 0;
  size_t receiver = 1;
  InternalStatsKey stats_key(stream, sender, receiver);

  // Frame captured
  FrameStats frame_stats(/*frame_id=*/1, captured_time);
  // Frame pre encoded
  frame_stats.pre_encode_time = captured_time + TimeDelta::Millis(10);

  comparator.Start(/*max_threads_count=*/1);
  comparator.EnsureStatsForStream(stream, sender, /*peers_count=*/2,
                                  captured_time, captured_time);
  comparator.AddComparison(stats_key,
                           /*captured=*/std::nullopt,
                           /*rendered=*/std::nullopt,
                           FrameComparisonType::kFrameInFlight, frame_stats);
  comparator.Stop(/*last_rendered_frame_times=*/{});

  EXPECT_EQ(comparator.stream_stats().size(), 1lu);
  StreamStats stats = comparator.stream_stats().at(stats_key);
  EXPECT_EQ(stats.stream_started_time, captured_time);
  ExpectEmpty(stats.psnr);
  ExpectEmpty(stats.ssim);
  ExpectEmpty(stats.transport_time_ms);
  ExpectEmpty(stats.total_delay_incl_transport_ms);
  ExpectEmpty(stats.time_between_rendered_frames_ms);
  ExpectEmpty(stats.encode_frame_rate);
  ExpectEmpty(stats.encode_time_ms);
  ExpectEmpty(stats.decode_time_ms);
  ExpectEmpty(stats.receive_to_render_time_ms);
  ExpectEmpty(stats.skipped_between_rendered);
  ExpectSizeAndAllElementsAre(stats.freeze_time_ms, /*size=*/1, /*value=*/0);
  ExpectEmpty(stats.time_between_freezes_ms);
  ExpectEmpty(stats.resolution_of_decoded_frame);
  ExpectEmpty(stats.target_encode_bitrate);
  EXPECT_THAT(stats.spatial_layers_qp, IsEmpty());
  ExpectEmpty(stats.rendered_frame_qp);
  ExpectEmpty(stats.recv_key_frame_size_bytes);
  ExpectEmpty(stats.recv_delta_frame_size_bytes);
  EXPECT_EQ(stats.total_encoded_images_payload, 0);
  EXPECT_EQ(stats.num_send_key_frames, 0);
  EXPECT_EQ(stats.num_recv_key_frames, 0);
  EXPECT_THAT(stats.dropped_by_phase, Eq(std::map<FrameDropPhase, int64_t>{
                                          {FrameDropPhase::kBeforeEncoder, 0},
                                          {FrameDropPhase::kByEncoder, 0},
                                          {FrameDropPhase::kTransport, 0},
                                          {FrameDropPhase::kByDecoder, 0},
                                          {FrameDropPhase::kAfterDecoder, 0}}));
  EXPECT_THAT(stats.encoders, IsEmpty());
  EXPECT_THAT(stats.decoders, IsEmpty());
}

TEST(DefaultVideoQualityAnalyzerFramesComparatorTest,
     EncodedInFlightKeyFrameAccountedInStats) {
  DefaultVideoQualityAnalyzerCpuMeasurer cpu_measurer;
  DefaultVideoQualityAnalyzerFramesComparator comparator(
      Clock::GetRealTimeClock(), cpu_measurer,
      DefaultVideoQualityAnalyzerOptions());

  Timestamp captured_time = Clock::GetRealTimeClock()->CurrentTime();
  uint16_t frame_id = 1;
  size_t stream = 0;
  size_t sender = 0;
  size_t receiver = 1;
  InternalStatsKey stats_key(stream, sender, receiver);

  // Frame captured
  FrameStats frame_stats(/*frame_id=*/1, captured_time);
  // Frame pre encoded
  frame_stats.pre_encode_time = captured_time + TimeDelta::Millis(10);
  // Frame encoded
  frame_stats.encoded_time = captured_time + TimeDelta::Millis(20);
  frame_stats.used_encoder =
      Vp8CodecForOneFrame(frame_id, frame_stats.encoded_time);
  frame_stats.encoded_frame_type = VideoFrameType::kVideoFrameKey;
  frame_stats.encoded_image_size = DataSize::Bytes(1000);
  frame_stats.target_encode_bitrate = 2000;
  frame_stats.spatial_layers_qp = {
      {0, StatsCounter(
              /*samples=*/{{5, Timestamp::Seconds(1)},
                           {5, Timestamp::Seconds(2)}})}};

  comparator.Start(/*max_threads_count=*/1);
  comparator.EnsureStatsForStream(stream, sender, /*peers_count=*/2,
                                  captured_time, captured_time);
  comparator.AddComparison(stats_key,
                           /*captured=*/std::nullopt,
                           /*rendered=*/std::nullopt,
                           FrameComparisonType::kFrameInFlight, frame_stats);
  comparator.Stop(/*last_rendered_frame_times=*/{});

  EXPECT_EQ(comparator.stream_stats().size(), 1lu);
  StreamStats stats = comparator.stream_stats().at(stats_key);
  EXPECT_EQ(stats.stream_started_time, captured_time);
  ExpectEmpty(stats.psnr);
  ExpectEmpty(stats.ssim);
  ExpectEmpty(stats.transport_time_ms);
  ExpectEmpty(stats.total_delay_incl_transport_ms);
  ExpectEmpty(stats.time_between_rendered_frames_ms);
  ExpectEmpty(stats.encode_frame_rate);
  ExpectSizeAndAllElementsAre(stats.encode_time_ms, /*size=*/1, /*value=*/10.0);
  ExpectEmpty(stats.decode_time_ms);
  ExpectEmpty(stats.receive_to_render_time_ms);
  ExpectEmpty(stats.skipped_between_rendered);
  ExpectSizeAndAllElementsAre(stats.freeze_time_ms, /*size=*/1, /*value=*/0);
  ExpectEmpty(stats.time_between_freezes_ms);
  ExpectEmpty(stats.resolution_of_decoded_frame);
  ExpectSizeAndAllElementsAre(stats.target_encode_bitrate, /*size=*/1,
                              /*value=*/2000.0);
  EXPECT_THAT(stats.spatial_layers_qp, SizeIs(1));
  ExpectSizeAndAllElementsAre(stats.spatial_layers_qp[0], /*size=*/2,
                              /*value=*/5.0);
  ExpectEmpty(stats.rendered_frame_qp);
  ExpectEmpty(stats.recv_key_frame_size_bytes);
  ExpectEmpty(stats.recv_delta_frame_size_bytes);
  EXPECT_EQ(stats.total_encoded_images_payload, 1000);
  EXPECT_EQ(stats.num_send_key_frames, 1);
  EXPECT_EQ(stats.num_recv_key_frames, 0);
  EXPECT_THAT(stats.dropped_by_phase, Eq(std::map<FrameDropPhase, int64_t>{
                                          {FrameDropPhase::kBeforeEncoder, 0},
                                          {FrameDropPhase::kByEncoder, 0},
                                          {FrameDropPhase::kTransport, 0},
                                          {FrameDropPhase::kByDecoder, 0},
                                          {FrameDropPhase::kAfterDecoder, 0}}));
  EXPECT_EQ(stats.encoders,
            std::vector<StreamCodecInfo>{*frame_stats.used_encoder});
  EXPECT_THAT(stats.decoders, IsEmpty());
}

TEST(DefaultVideoQualityAnalyzerFramesComparatorTest,
     EncodedInFlightDeltaFrameAccountedInStats) {
  DefaultVideoQualityAnalyzerCpuMeasurer cpu_measurer;
  DefaultVideoQualityAnalyzerFramesComparator comparator(
      Clock::GetRealTimeClock(), cpu_measurer,
      DefaultVideoQualityAnalyzerOptions());

  Timestamp captured_time = Clock::GetRealTimeClock()->CurrentTime();
  uint16_t frame_id = 1;
  size_t stream = 0;
  size_t sender = 0;
  size_t receiver = 1;
  InternalStatsKey stats_key(stream, sender, receiver);

  // Frame captured
  FrameStats frame_stats(/*frame_id=*/1, captured_time);
  // Frame pre encoded
  frame_stats.pre_encode_time = captured_time + TimeDelta::Millis(10);
  // Frame encoded
  frame_stats.encoded_time = captured_time + TimeDelta::Millis(20);
  frame_stats.used_encoder =
      Vp8CodecForOneFrame(frame_id, frame_stats.encoded_time);
  frame_stats.encoded_frame_type = VideoFrameType::kVideoFrameDelta;
  frame_stats.encoded_image_size = DataSize::Bytes(1000);
  frame_stats.target_encode_bitrate = 2000;
  frame_stats.spatial_layers_qp = {
      {0, StatsCounter(
              /*samples=*/{{5, Timestamp::Seconds(1)},
                           {5, Timestamp::Seconds(2)}})}};

  comparator.Start(/*max_threads_count=*/1);
  comparator.EnsureStatsForStream(stream, sender, /*peers_count=*/2,
                                  captured_time, captured_time);
  comparator.AddComparison(stats_key,
                           /*captured=*/std::nullopt,
                           /*rendered=*/std::nullopt,
                           FrameComparisonType::kFrameInFlight, frame_stats);
  comparator.Stop(/*last_rendered_frame_times=*/{});

  EXPECT_EQ(comparator.stream_stats().size(), 1lu);
  StreamStats stats = comparator.stream_stats().at(stats_key);
  EXPECT_EQ(stats.stream_started_time, captured_time);
  ExpectEmpty(stats.psnr);
  ExpectEmpty(stats.ssim);
  ExpectEmpty(stats.transport_time_ms);
  ExpectEmpty(stats.total_delay_incl_transport_ms);
  ExpectEmpty(stats.time_between_rendered_frames_ms);
  ExpectEmpty(stats.encode_frame_rate);
  ExpectSizeAndAllElementsAre(stats.encode_time_ms, /*size=*/1, /*value=*/10.0);
  ExpectEmpty(stats.decode_time_ms);
  ExpectEmpty(stats.receive_to_render_time_ms);
  ExpectEmpty(stats.skipped_between_rendered);
  ExpectSizeAndAllElementsAre(stats.freeze_time_ms, /*size=*/1, /*value=*/0);
  ExpectEmpty(stats.time_between_freezes_ms);
  ExpectEmpty(stats.resolution_of_decoded_frame);
  ExpectSizeAndAllElementsAre(stats.target_encode_bitrate, /*size=*/1,
                              /*value=*/2000.0);
  EXPECT_THAT(stats.spatial_layers_qp, SizeIs(1));
  ExpectSizeAndAllElementsAre(stats.spatial_layers_qp[0], /*size=*/2,
                              /*value=*/5.0);
  ExpectEmpty(stats.rendered_frame_qp);
  ExpectEmpty(stats.recv_key_frame_size_bytes);
  ExpectEmpty(stats.recv_delta_frame_size_bytes);
  EXPECT_EQ(stats.total_encoded_images_payload, 1000);
  EXPECT_EQ(stats.num_send_key_frames, 0);
  EXPECT_EQ(stats.num_recv_key_frames, 0);
  EXPECT_THAT(stats.dropped_by_phase, Eq(std::map<FrameDropPhase, int64_t>{
                                          {FrameDropPhase::kBeforeEncoder, 0},
                                          {FrameDropPhase::kByEncoder, 0},
                                          {FrameDropPhase::kTransport, 0},
                                          {FrameDropPhase::kByDecoder, 0},
                                          {FrameDropPhase::kAfterDecoder, 0}}));
  EXPECT_EQ(stats.encoders,
            std::vector<StreamCodecInfo>{*frame_stats.used_encoder});
  EXPECT_THAT(stats.decoders, IsEmpty());
}

TEST(DefaultVideoQualityAnalyzerFramesComparatorTest,
     PreDecodedInFlightKeyFrameAccountedInStats) {
  DefaultVideoQualityAnalyzerCpuMeasurer cpu_measurer;
  DefaultVideoQualityAnalyzerFramesComparator comparator(
      Clock::GetRealTimeClock(), cpu_measurer,
      DefaultVideoQualityAnalyzerOptions());

  Timestamp captured_time = Clock::GetRealTimeClock()->CurrentTime();
  uint16_t frame_id = 1;
  size_t stream = 0;
  size_t sender = 0;
  size_t receiver = 1;
  InternalStatsKey stats_key(stream, sender, receiver);

  // Frame captured
  FrameStats frame_stats(/*frame_id=*/1, captured_time);
  // Frame pre encoded
  frame_stats.pre_encode_time = captured_time + TimeDelta::Millis(10);
  // Frame encoded
  frame_stats.encoded_time = captured_time + TimeDelta::Millis(20);
  frame_stats.used_encoder =
      Vp8CodecForOneFrame(frame_id, frame_stats.encoded_time);
  frame_stats.encoded_frame_type = VideoFrameType::kVideoFrameKey;
  frame_stats.encoded_image_size = DataSize::Bytes(1000);
  frame_stats.target_encode_bitrate = 2000;
  frame_stats.spatial_layers_qp = {
      {0, StatsCounter(
              /*samples=*/{{5, Timestamp::Seconds(1)},
                           {5, Timestamp::Seconds(2)}})}};
  // Frame pre decoded
  frame_stats.pre_decoded_frame_type = VideoFrameType::kVideoFrameKey;
  frame_stats.pre_decoded_image_size = DataSize::Bytes(500);
  frame_stats.received_time = captured_time + TimeDelta::Millis(30);
  frame_stats.decode_start_time = captured_time + TimeDelta::Millis(40);

  comparator.Start(/*max_threads_count=*/1);
  comparator.EnsureStatsForStream(stream, sender, /*peers_count=*/2,
                                  captured_time, captured_time);
  comparator.AddComparison(stats_key,
                           /*captured=*/std::nullopt,
                           /*rendered=*/std::nullopt,
                           FrameComparisonType::kFrameInFlight, frame_stats);
  comparator.Stop(/*last_rendered_frame_times=*/{});

  EXPECT_EQ(comparator.stream_stats().size(), 1lu);
  StreamStats stats = comparator.stream_stats().at(stats_key);
  EXPECT_EQ(stats.stream_started_time, captured_time);
  ExpectEmpty(stats.psnr);
  ExpectEmpty(stats.ssim);
  ExpectSizeAndAllElementsAre(stats.transport_time_ms, /*size=*/1,
                              /*value=*/20.0);
  ExpectEmpty(stats.total_delay_incl_transport_ms);
  ExpectEmpty(stats.time_between_rendered_frames_ms);
  ExpectEmpty(stats.encode_frame_rate);
  ExpectSizeAndAllElementsAre(stats.encode_time_ms, /*size=*/1, /*value=*/10.0);
  ExpectEmpty(stats.decode_time_ms);
  ExpectEmpty(stats.receive_to_render_time_ms);
  ExpectEmpty(stats.skipped_between_rendered);
  ExpectSizeAndAllElementsAre(stats.freeze_time_ms, /*size=*/1, /*value=*/0);
  ExpectEmpty(stats.time_between_freezes_ms);
  ExpectEmpty(stats.resolution_of_decoded_frame);
  ExpectSizeAndAllElementsAre(stats.target_encode_bitrate, /*size=*/1,
                              /*value=*/2000.0);
  EXPECT_THAT(stats.spatial_layers_qp, SizeIs(1));
  ExpectSizeAndAllElementsAre(stats.spatial_layers_qp[0], /*size=*/2,
                              /*value=*/5.0);
  ExpectEmpty(stats.rendered_frame_qp);
  ExpectSizeAndAllElementsAre(stats.recv_key_frame_size_bytes, /*size=*/1,
                              /*value=*/500.0);
  ExpectEmpty(stats.recv_delta_frame_size_bytes);
  EXPECT_EQ(stats.total_encoded_images_payload, 1000);
  EXPECT_EQ(stats.num_send_key_frames, 1);
  EXPECT_EQ(stats.num_recv_key_frames, 1);
  EXPECT_THAT(stats.dropped_by_phase, Eq(std::map<FrameDropPhase, int64_t>{
                                          {FrameDropPhase::kBeforeEncoder, 0},
                                          {FrameDropPhase::kByEncoder, 0},
                                          {FrameDropPhase::kTransport, 0},
                                          {FrameDropPhase::kByDecoder, 0},
                                          {FrameDropPhase::kAfterDecoder, 0}}));
  EXPECT_EQ(stats.encoders,
            std::vector<StreamCodecInfo>{*frame_stats.used_encoder});
  EXPECT_THAT(stats.decoders, IsEmpty());
}

TEST(DefaultVideoQualityAnalyzerFramesComparatorTest,
     DecodedInFlightKeyFrameAccountedInStats) {
  DefaultVideoQualityAnalyzerCpuMeasurer cpu_measurer;
  DefaultVideoQualityAnalyzerFramesComparator comparator(
      Clock::GetRealTimeClock(), cpu_measurer,
      DefaultVideoQualityAnalyzerOptions());

  Timestamp captured_time = Clock::GetRealTimeClock()->CurrentTime();
  uint16_t frame_id = 1;
  size_t stream = 0;
  size_t sender = 0;
  size_t receiver = 1;
  InternalStatsKey stats_key(stream, sender, receiver);

  // Frame captured
  FrameStats frame_stats(/*frame_id=*/1, captured_time);
  // Frame pre encoded
  frame_stats.pre_encode_time = captured_time + TimeDelta::Millis(10);
  // Frame encoded
  frame_stats.encoded_time = captured_time + TimeDelta::Millis(20);
  frame_stats.used_encoder =
      Vp8CodecForOneFrame(frame_id, frame_stats.encoded_time);
  frame_stats.encoded_frame_type = VideoFrameType::kVideoFrameKey;
  frame_stats.encoded_image_size = DataSize::Bytes(1000);
  frame_stats.target_encode_bitrate = 2000;
  frame_stats.spatial_layers_qp = {
      {0, StatsCounter(
              /*samples=*/{{5, Timestamp::Seconds(1)},
                           {5, Timestamp::Seconds(2)}})}};
  // Frame pre decoded
  frame_stats.pre_decoded_frame_type = VideoFrameType::kVideoFrameKey;
  frame_stats.pre_decoded_image_size = DataSize::Bytes(500);
  frame_stats.received_time = captured_time + TimeDelta::Millis(30);
  frame_stats.decode_start_time = captured_time + TimeDelta::Millis(40);
  // Frame decoded
  frame_stats.decode_end_time = captured_time + TimeDelta::Millis(50);
  frame_stats.decoded_frame_width = 200;
  frame_stats.decoded_frame_height = 100;
  frame_stats.decoded_frame_qp = 10;

  frame_stats.used_decoder =
      Vp8CodecForOneFrame(frame_id, frame_stats.decode_end_time);

  comparator.Start(/*max_threads_count=*/1);
  comparator.EnsureStatsForStream(stream, sender, /*peers_count=*/2,
                                  captured_time, captured_time);
  comparator.AddComparison(stats_key,
                           /*captured=*/std::nullopt,
                           /*rendered=*/std::nullopt,
                           FrameComparisonType::kFrameInFlight, frame_stats);
  comparator.Stop(/*last_rendered_frame_times=*/{});

  EXPECT_EQ(comparator.stream_stats().size(), 1lu);
  StreamStats stats = comparator.stream_stats().at(stats_key);
  EXPECT_EQ(stats.stream_started_time, captured_time);
  ExpectEmpty(stats.psnr);
  ExpectEmpty(stats.ssim);
  ExpectSizeAndAllElementsAre(stats.transport_time_ms, /*size=*/1,
                              /*value=*/20.0);
  ExpectEmpty(stats.total_delay_incl_transport_ms);
  ExpectEmpty(stats.time_between_rendered_frames_ms);
  ExpectEmpty(stats.encode_frame_rate);
  ExpectSizeAndAllElementsAre(stats.encode_time_ms, /*size=*/1, /*value=*/10.0);
  ExpectSizeAndAllElementsAre(stats.decode_time_ms, /*size=*/1, /*value=*/10.0);
  ExpectEmpty(stats.receive_to_render_time_ms);
  ExpectEmpty(stats.skipped_between_rendered);
  ExpectSizeAndAllElementsAre(stats.freeze_time_ms, /*size=*/1, /*value=*/0);
  ExpectEmpty(stats.time_between_freezes_ms);
  EXPECT_GE(GetFirstOrDie(stats.resolution_of_decoded_frame), 200 * 100.0);
  ExpectSizeAndAllElementsAre(stats.target_encode_bitrate, /*size=*/1,
                              /*value=*/2000.0);
  EXPECT_THAT(stats.spatial_layers_qp, SizeIs(1));
  ExpectSizeAndAllElementsAre(stats.spatial_layers_qp[0], /*size=*/2,
                              /*value=*/5.0);
  ExpectSizeAndAllElementsAre(stats.rendered_frame_qp, /*size=*/1,
                              /*value=*/10.0);
  ExpectSizeAndAllElementsAre(stats.recv_key_frame_size_bytes, /*size=*/1,
                              /*value=*/500.0);
  ExpectEmpty(stats.recv_delta_frame_size_bytes);
  EXPECT_EQ(stats.total_encoded_images_payload, 1000);
  EXPECT_EQ(stats.num_send_key_frames, 1);
  EXPECT_EQ(stats.num_recv_key_frames, 1);
  EXPECT_THAT(stats.dropped_by_phase, Eq(std::map<FrameDropPhase, int64_t>{
                                          {FrameDropPhase::kBeforeEncoder, 0},
                                          {FrameDropPhase::kByEncoder, 0},
                                          {FrameDropPhase::kTransport, 0},
                                          {FrameDropPhase::kByDecoder, 0},
                                          {FrameDropPhase::kAfterDecoder, 0}}));
  EXPECT_EQ(stats.encoders,
            std::vector<StreamCodecInfo>{*frame_stats.used_encoder});
  EXPECT_EQ(stats.decoders,
            std::vector<StreamCodecInfo>{*frame_stats.used_decoder});
}

TEST(DefaultVideoQualityAnalyzerFramesComparatorTest,
     DecoderFailureOnInFlightKeyFrameAccountedInStats) {
  DefaultVideoQualityAnalyzerCpuMeasurer cpu_measurer;
  DefaultVideoQualityAnalyzerFramesComparator comparator(
      Clock::GetRealTimeClock(), cpu_measurer,
      DefaultVideoQualityAnalyzerOptions());

  Timestamp captured_time = Clock::GetRealTimeClock()->CurrentTime();
  uint16_t frame_id = 1;
  size_t stream = 0;
  size_t sender = 0;
  size_t receiver = 1;
  InternalStatsKey stats_key(stream, sender, receiver);

  // Frame captured
  FrameStats frame_stats(/*frame_id=*/1, captured_time);
  // Frame pre encoded
  frame_stats.pre_encode_time = captured_time + TimeDelta::Millis(10);
  // Frame encoded
  frame_stats.encoded_time = captured_time + TimeDelta::Millis(20);
  frame_stats.used_encoder =
      Vp8CodecForOneFrame(frame_id, frame_stats.encoded_time);
  frame_stats.encoded_frame_type = VideoFrameType::kVideoFrameKey;
  frame_stats.encoded_image_size = DataSize::Bytes(1000);
  frame_stats.target_encode_bitrate = 2000;
  frame_stats.spatial_layers_qp = {
      {0, StatsCounter(
              /*samples=*/{{5, Timestamp::Seconds(1)},
                           {5, Timestamp::Seconds(2)}})}};
  // Frame pre decoded
  frame_stats.pre_decoded_frame_type = VideoFrameType::kVideoFrameKey;
  frame_stats.pre_decoded_image_size = DataSize::Bytes(500);
  frame_stats.received_time = captured_time + TimeDelta::Millis(30);
  frame_stats.decode_start_time = captured_time + TimeDelta::Millis(40);
  // Frame decoded
  frame_stats.decoder_failed = true;
  frame_stats.used_decoder =
      Vp8CodecForOneFrame(frame_id, frame_stats.decode_end_time);
  frame_stats.decoded_frame_qp = 10;

  comparator.Start(/*max_threads_count=*/1);
  comparator.EnsureStatsForStream(stream, sender, /*peers_count=*/2,
                                  captured_time, captured_time);
  comparator.AddComparison(stats_key,
                           /*captured=*/std::nullopt,
                           /*rendered=*/std::nullopt,
                           FrameComparisonType::kFrameInFlight, frame_stats);
  comparator.Stop(/*last_rendered_frame_times=*/{});

  EXPECT_EQ(comparator.stream_stats().size(), 1lu);
  StreamStats stats = comparator.stream_stats().at(stats_key);
  EXPECT_EQ(stats.stream_started_time, captured_time);
  ExpectEmpty(stats.psnr);
  ExpectEmpty(stats.ssim);
  ExpectSizeAndAllElementsAre(stats.transport_time_ms, /*size=*/1,
                              /*value=*/20.0);
  ExpectEmpty(stats.total_delay_incl_transport_ms);
  ExpectEmpty(stats.time_between_rendered_frames_ms);
  ExpectEmpty(stats.encode_frame_rate);
  ExpectSizeAndAllElementsAre(stats.encode_time_ms, /*size=*/1, /*value=*/10.0);
  ExpectEmpty(stats.decode_time_ms);
  ExpectEmpty(stats.receive_to_render_time_ms);
  ExpectEmpty(stats.skipped_between_rendered);
  ExpectSizeAndAllElementsAre(stats.freeze_time_ms, /*size=*/1, /*value=*/0);
  ExpectEmpty(stats.time_between_freezes_ms);
  ExpectEmpty(stats.resolution_of_decoded_frame);
  ExpectSizeAndAllElementsAre(stats.target_encode_bitrate, /*size=*/1,
                              /*value=*/2000.0);
  EXPECT_THAT(stats.spatial_layers_qp, SizeIs(1));
  ExpectSizeAndAllElementsAre(stats.spatial_layers_qp[0], /*size=*/2,
                              /*value=*/5.0);
  ExpectEmpty(stats.rendered_frame_qp);
  ExpectSizeAndAllElementsAre(stats.recv_key_frame_size_bytes, /*size=*/1,
                              /*value=*/500.0);
  ExpectEmpty(stats.recv_delta_frame_size_bytes);
  EXPECT_EQ(stats.total_encoded_images_payload, 1000);
  EXPECT_EQ(stats.num_send_key_frames, 1);
  EXPECT_EQ(stats.num_recv_key_frames, 1);
  // All frame in flight are not considered as dropped.
  EXPECT_THAT(stats.dropped_by_phase, Eq(std::map<FrameDropPhase, int64_t>{
                                          {FrameDropPhase::kBeforeEncoder, 0},
                                          {FrameDropPhase::kByEncoder, 0},
                                          {FrameDropPhase::kTransport, 0},
                                          {FrameDropPhase::kByDecoder, 0},
                                          {FrameDropPhase::kAfterDecoder, 0}}));
  EXPECT_EQ(stats.encoders,
            std::vector<StreamCodecInfo>{*frame_stats.used_encoder});
  EXPECT_EQ(stats.decoders,
            std::vector<StreamCodecInfo>{*frame_stats.used_decoder});
}
// Frame in flight end

// Dropped frame start
TEST(DefaultVideoQualityAnalyzerFramesComparatorTest,
     CapturedOnlyDroppedFrameAccountedInStats) {
  DefaultVideoQualityAnalyzerCpuMeasurer cpu_measurer;
  DefaultVideoQualityAnalyzerFramesComparator comparator(
      Clock::GetRealTimeClock(), cpu_measurer,
      DefaultVideoQualityAnalyzerOptions());

  Timestamp captured_time = Clock::GetRealTimeClock()->CurrentTime();
  size_t stream = 0;
  size_t sender = 0;
  size_t receiver = 1;
  InternalStatsKey stats_key(stream, sender, receiver);

  // Frame captured
  FrameStats frame_stats(/*frame_id=*/1, captured_time);

  comparator.Start(/*max_threads_count=*/1);
  comparator.EnsureStatsForStream(stream, sender, /*peers_count=*/2,
                                  captured_time, captured_time);
  comparator.AddComparison(stats_key,
                           /*captured=*/std::nullopt,
                           /*rendered=*/std::nullopt,
                           FrameComparisonType::kDroppedFrame, frame_stats);
  comparator.Stop(/*last_rendered_frame_times=*/{});

  EXPECT_EQ(comparator.stream_stats().size(), 1lu);
  StreamStats stats = comparator.stream_stats().at(stats_key);
  EXPECT_EQ(stats.stream_started_time, captured_time);
  ExpectEmpty(stats.psnr);
  ExpectEmpty(stats.ssim);
  ExpectEmpty(stats.transport_time_ms);
  ExpectEmpty(stats.total_delay_incl_transport_ms);
  ExpectEmpty(stats.time_between_rendered_frames_ms);
  ExpectEmpty(stats.encode_frame_rate);
  ExpectEmpty(stats.encode_time_ms);
  ExpectEmpty(stats.decode_time_ms);
  ExpectEmpty(stats.receive_to_render_time_ms);
  ExpectEmpty(stats.skipped_between_rendered);
  ExpectSizeAndAllElementsAre(stats.freeze_time_ms, /*size=*/1, /*value=*/0);
  ExpectEmpty(stats.time_between_freezes_ms);
  ExpectEmpty(stats.resolution_of_decoded_frame);
  ExpectEmpty(stats.target_encode_bitrate);
  EXPECT_THAT(stats.spatial_layers_qp, IsEmpty());
  ExpectEmpty(stats.rendered_frame_qp);
  ExpectEmpty(stats.recv_key_frame_size_bytes);
  ExpectEmpty(stats.recv_delta_frame_size_bytes);
  EXPECT_EQ(stats.total_encoded_images_payload, 0);
  EXPECT_EQ(stats.num_send_key_frames, 0);
  EXPECT_EQ(stats.num_recv_key_frames, 0);
  EXPECT_THAT(stats.dropped_by_phase, Eq(std::map<FrameDropPhase, int64_t>{
                                          {FrameDropPhase::kBeforeEncoder, 1},
                                          {FrameDropPhase::kByEncoder, 0},
                                          {FrameDropPhase::kTransport, 0},
                                          {FrameDropPhase::kByDecoder, 0},
                                          {FrameDropPhase::kAfterDecoder, 0}}));
  EXPECT_THAT(stats.encoders, IsEmpty());
  EXPECT_THAT(stats.decoders, IsEmpty());
}

TEST(DefaultVideoQualityAnalyzerFramesComparatorTest,
     PreEncodedDroppedFrameAccountedInStats) {
  DefaultVideoQualityAnalyzerCpuMeasurer cpu_measurer;
  DefaultVideoQualityAnalyzerFramesComparator comparator(
      Clock::GetRealTimeClock(), cpu_measurer,
      DefaultVideoQualityAnalyzerOptions());

  Timestamp captured_time = Clock::GetRealTimeClock()->CurrentTime();
  size_t stream = 0;
  size_t sender = 0;
  size_t receiver = 1;
  InternalStatsKey stats_key(stream, sender, receiver);

  // Frame captured
  FrameStats frame_stats(/*frame_id=*/1, captured_time);
  // Frame pre encoded
  frame_stats.pre_encode_time = captured_time + TimeDelta::Millis(10);

  comparator.Start(/*max_threads_count=*/1);
  comparator.EnsureStatsForStream(stream, sender, /*peers_count=*/2,
                                  captured_time, captured_time);
  comparator.AddComparison(stats_key,
                           /*captured=*/std::nullopt,
                           /*rendered=*/std::nullopt,
                           FrameComparisonType::kDroppedFrame, frame_stats);
  comparator.Stop(/*last_rendered_frame_times=*/{});

  EXPECT_EQ(comparator.stream_stats().size(), 1lu);
  StreamStats stats = comparator.stream_stats().at(stats_key);
  EXPECT_EQ(stats.stream_started_time, captured_time);
  ExpectEmpty(stats.psnr);
  ExpectEmpty(stats.ssim);
  ExpectEmpty(stats.transport_time_ms);
  ExpectEmpty(stats.total_delay_incl_transport_ms);
  ExpectEmpty(stats.time_between_rendered_frames_ms);
  ExpectEmpty(stats.encode_frame_rate);
  ExpectEmpty(stats.encode_time_ms);
  ExpectEmpty(stats.decode_time_ms);
  ExpectEmpty(stats.receive_to_render_time_ms);
  ExpectEmpty(stats.skipped_between_rendered);
  ExpectSizeAndAllElementsAre(stats.freeze_time_ms, /*size=*/1, /*value=*/0);
  ExpectEmpty(stats.time_between_freezes_ms);
  ExpectEmpty(stats.resolution_of_decoded_frame);
  ExpectEmpty(stats.target_encode_bitrate);
  EXPECT_THAT(stats.spatial_layers_qp, IsEmpty());
  ExpectEmpty(stats.rendered_frame_qp);
  ExpectEmpty(stats.recv_key_frame_size_bytes);
  ExpectEmpty(stats.recv_delta_frame_size_bytes);
  EXPECT_EQ(stats.total_encoded_images_payload, 0);
  EXPECT_EQ(stats.num_send_key_frames, 0);
  EXPECT_EQ(stats.num_recv_key_frames, 0);
  EXPECT_THAT(stats.dropped_by_phase, Eq(std::map<FrameDropPhase, int64_t>{
                                          {FrameDropPhase::kBeforeEncoder, 0},
                                          {FrameDropPhase::kByEncoder, 1},
                                          {FrameDropPhase::kTransport, 0},
                                          {FrameDropPhase::kByDecoder, 0},
                                          {FrameDropPhase::kAfterDecoder, 0}}));
  EXPECT_THAT(stats.encoders, IsEmpty());
  EXPECT_THAT(stats.decoders, IsEmpty());
}

TEST(DefaultVideoQualityAnalyzerFramesComparatorTest,
     EncodedDroppedKeyFrameAccountedInStats) {
  DefaultVideoQualityAnalyzerCpuMeasurer cpu_measurer;
  DefaultVideoQualityAnalyzerFramesComparator comparator(
      Clock::GetRealTimeClock(), cpu_measurer,
      DefaultVideoQualityAnalyzerOptions());

  Timestamp captured_time = Clock::GetRealTimeClock()->CurrentTime();
  uint16_t frame_id = 1;
  size_t stream = 0;
  size_t sender = 0;
  size_t receiver = 1;
  InternalStatsKey stats_key(stream, sender, receiver);

  // Frame captured
  FrameStats frame_stats(/*frame_id=*/1, captured_time);
  // Frame pre encoded
  frame_stats.pre_encode_time = captured_time + TimeDelta::Millis(10);
  // Frame encoded
  frame_stats.encoded_time = captured_time + TimeDelta::Millis(20);
  frame_stats.used_encoder =
      Vp8CodecForOneFrame(frame_id, frame_stats.encoded_time);
  frame_stats.encoded_frame_type = VideoFrameType::kVideoFrameKey;
  frame_stats.encoded_image_size = DataSize::Bytes(1000);
  frame_stats.target_encode_bitrate = 2000;
  frame_stats.spatial_layers_qp = {
      {0, StatsCounter(
              /*samples=*/{{5, Timestamp::Seconds(1)},
                           {5, Timestamp::Seconds(2)}})}};

  comparator.Start(/*max_threads_count=*/1);
  comparator.EnsureStatsForStream(stream, sender, /*peers_count=*/2,
                                  captured_time, captured_time);
  comparator.AddComparison(stats_key,
                           /*captured=*/std::nullopt,
                           /*rendered=*/std::nullopt,
                           FrameComparisonType::kDroppedFrame, frame_stats);
  comparator.Stop(/*last_rendered_frame_times=*/{});

  EXPECT_EQ(comparator.stream_stats().size(), 1lu);
  StreamStats stats = comparator.stream_stats().at(stats_key);
  EXPECT_EQ(stats.stream_started_time, captured_time);
  ExpectEmpty(stats.psnr);
  ExpectEmpty(stats.ssim);
  ExpectEmpty(stats.transport_time_ms);
  ExpectEmpty(stats.total_delay_incl_transport_ms);
  ExpectEmpty(stats.time_between_rendered_frames_ms);
  ExpectEmpty(stats.encode_frame_rate);
  ExpectSizeAndAllElementsAre(stats.encode_time_ms, /*size=*/1, /*value=*/10.0);
  ExpectEmpty(stats.decode_time_ms);
  ExpectEmpty(stats.receive_to_render_time_ms);
  ExpectEmpty(stats.skipped_between_rendered);
  ExpectSizeAndAllElementsAre(stats.freeze_time_ms, /*size=*/1, /*value=*/0);
  ExpectEmpty(stats.time_between_freezes_ms);
  ExpectEmpty(stats.resolution_of_decoded_frame);
  ExpectSizeAndAllElementsAre(stats.target_encode_bitrate, /*size=*/1,
                              /*value=*/2000.0);
  EXPECT_THAT(stats.spatial_layers_qp, SizeIs(1));
  ExpectSizeAndAllElementsAre(stats.spatial_layers_qp[0], /*size=*/2,
                              /*value=*/5.0);
  ExpectEmpty(stats.rendered_frame_qp);
  ExpectEmpty(stats.recv_key_frame_size_bytes);
  ExpectEmpty(stats.recv_delta_frame_size_bytes);
  EXPECT_EQ(stats.total_encoded_images_payload, 1000);
  EXPECT_EQ(stats.num_send_key_frames, 1);
  EXPECT_EQ(stats.num_recv_key_frames, 0);
  EXPECT_THAT(stats.dropped_by_phase, Eq(std::map<FrameDropPhase, int64_t>{
                                          {FrameDropPhase::kBeforeEncoder, 0},
                                          {FrameDropPhase::kByEncoder, 0},
                                          {FrameDropPhase::kTransport, 1},
                                          {FrameDropPhase::kByDecoder, 0},
                                          {FrameDropPhase::kAfterDecoder, 0}}));
  EXPECT_EQ(stats.encoders,
            std::vector<StreamCodecInfo>{*frame_stats.used_encoder});
  EXPECT_THAT(stats.decoders, IsEmpty());
}

TEST(DefaultVideoQualityAnalyzerFramesComparatorTest,
     EncodedDroppedDeltaFrameAccountedInStats) {
  DefaultVideoQualityAnalyzerCpuMeasurer cpu_measurer;
  DefaultVideoQualityAnalyzerFramesComparator comparator(
      Clock::GetRealTimeClock(), cpu_measurer,
      DefaultVideoQualityAnalyzerOptions());

  Timestamp captured_time = Clock::GetRealTimeClock()->CurrentTime();
  uint16_t frame_id = 1;
  size_t stream = 0;
  size_t sender = 0;
  size_t receiver = 1;
  InternalStatsKey stats_key(stream, sender, receiver);

  // Frame captured
  FrameStats frame_stats(/*frame_id=*/1, captured_time);
  // Frame pre encoded
  frame_stats.pre_encode_time = captured_time + TimeDelta::Millis(10);
  // Frame encoded
  frame_stats.encoded_time = captured_time + TimeDelta::Millis(20);
  frame_stats.used_encoder =
      Vp8CodecForOneFrame(frame_id, frame_stats.encoded_time);
  frame_stats.encoded_frame_type = VideoFrameType::kVideoFrameDelta;
  frame_stats.encoded_image_size = DataSize::Bytes(1000);
  frame_stats.target_encode_bitrate = 2000;
  frame_stats.spatial_layers_qp = {
      {0, StatsCounter(
              /*samples=*/{{5, Timestamp::Seconds(1)},
                           {5, Timestamp::Seconds(2)}})}};

  comparator.Start(/*max_threads_count=*/1);
  comparator.EnsureStatsForStream(stream, sender, /*peers_count=*/2,
                                  captured_time, captured_time);
  comparator.AddComparison(stats_key,
                           /*captured=*/std::nullopt,
                           /*rendered=*/std::nullopt,
                           FrameComparisonType::kDroppedFrame, frame_stats);
  comparator.Stop(/*last_rendered_frame_times=*/{});

  EXPECT_EQ(comparator.stream_stats().size(), 1lu);
  StreamStats stats = comparator.stream_stats().at(stats_key);
  EXPECT_EQ(stats.stream_started_time, captured_time);
  ExpectEmpty(stats.psnr);
  ExpectEmpty(stats.ssim);
  ExpectEmpty(stats.transport_time_ms);
  ExpectEmpty(stats.total_delay_incl_transport_ms);
  ExpectEmpty(stats.time_between_rendered_frames_ms);
  ExpectEmpty(stats.encode_frame_rate);
  ExpectSizeAndAllElementsAre(stats.encode_time_ms, /*size=*/1, /*value=*/10.0);
  ExpectEmpty(stats.decode_time_ms);
  ExpectEmpty(stats.receive_to_render_time_ms);
  ExpectEmpty(stats.skipped_between_rendered);
  ExpectSizeAndAllElementsAre(stats.freeze_time_ms, /*size=*/1, /*value=*/0);
  ExpectEmpty(stats.time_between_freezes_ms);
  ExpectEmpty(stats.resolution_of_decoded_frame);
  ExpectSizeAndAllElementsAre(stats.target_encode_bitrate, /*size=*/1,
                              /*value=*/2000.0);
  EXPECT_THAT(stats.spatial_layers_qp, SizeIs(1));
  ExpectSizeAndAllElementsAre(stats.spatial_layers_qp[0], /*size=*/2,
                              /*value=*/5.0);
  ExpectEmpty(stats.rendered_frame_qp);
  ExpectEmpty(stats.recv_key_frame_size_bytes);
  ExpectEmpty(stats.recv_delta_frame_size_bytes);
  EXPECT_EQ(stats.total_encoded_images_payload, 1000);
  EXPECT_EQ(stats.num_send_key_frames, 0);
  EXPECT_EQ(stats.num_recv_key_frames, 0);
  EXPECT_THAT(stats.dropped_by_phase, Eq(std::map<FrameDropPhase, int64_t>{
                                          {FrameDropPhase::kBeforeEncoder, 0},
                                          {FrameDropPhase::kByEncoder, 0},
                                          {FrameDropPhase::kTransport, 1},
                                          {FrameDropPhase::kByDecoder, 0},
                                          {FrameDropPhase::kAfterDecoder, 0}}));
  EXPECT_EQ(stats.encoders,
            std::vector<StreamCodecInfo>{*frame_stats.used_encoder});
  EXPECT_THAT(stats.decoders, IsEmpty());
}

TEST(DefaultVideoQualityAnalyzerFramesComparatorTest,
     PreDecodedDroppedKeyFrameAccountedInStats) {
  DefaultVideoQualityAnalyzerCpuMeasurer cpu_measurer;
  DefaultVideoQualityAnalyzerFramesComparator comparator(
      Clock::GetRealTimeClock(), cpu_measurer,
      DefaultVideoQualityAnalyzerOptions());

  Timestamp captured_time = Clock::GetRealTimeClock()->CurrentTime();
  uint16_t frame_id = 1;
  size_t stream = 0;
  size_t sender = 0;
  size_t receiver = 1;
  InternalStatsKey stats_key(stream, sender, receiver);

  // Frame captured
  FrameStats frame_stats(/*frame_id=*/1, captured_time);
  // Frame pre encoded
  frame_stats.pre_encode_time = captured_time + TimeDelta::Millis(10);
  // Frame encoded
  frame_stats.encoded_time = captured_time + TimeDelta::Millis(20);
  frame_stats.used_encoder =
      Vp8CodecForOneFrame(frame_id, frame_stats.encoded_time);
  frame_stats.encoded_frame_type = VideoFrameType::kVideoFrameKey;
  frame_stats.encoded_image_size = DataSize::Bytes(1000);
  frame_stats.target_encode_bitrate = 2000;
  // Frame pre decoded
  frame_stats.pre_decoded_frame_type = VideoFrameType::kVideoFrameKey;
  frame_stats.pre_decoded_image_size = DataSize::Bytes(500);
  frame_stats.received_time = captured_time + TimeDelta::Millis(30);
  frame_stats.decode_start_time = captured_time + TimeDelta::Millis(40);

  comparator.Start(/*max_threads_count=*/1);
  comparator.EnsureStatsForStream(stream, sender, /*peers_count=*/2,
                                  captured_time, captured_time);
  comparator.AddComparison(stats_key,
                           /*captured=*/std::nullopt,
                           /*rendered=*/std::nullopt,
                           FrameComparisonType::kDroppedFrame, frame_stats);
  comparator.Stop(/*last_rendered_frame_times=*/{});

  EXPECT_EQ(comparator.stream_stats().size(), 1lu);
  StreamStats stats = comparator.stream_stats().at(stats_key);
  EXPECT_EQ(stats.stream_started_time, captured_time);
  ExpectEmpty(stats.psnr);
  ExpectEmpty(stats.ssim);
  ExpectEmpty(stats.transport_time_ms);
  ExpectEmpty(stats.total_delay_incl_transport_ms);
  ExpectEmpty(stats.time_between_rendered_frames_ms);
  ExpectEmpty(stats.encode_frame_rate);
  ExpectSizeAndAllElementsAre(stats.encode_time_ms, /*size=*/1, /*value=*/10.0);
  ExpectEmpty(stats.decode_time_ms);
  ExpectEmpty(stats.receive_to_render_time_ms);
  ExpectEmpty(stats.skipped_between_rendered);
  ExpectSizeAndAllElementsAre(stats.freeze_time_ms, /*size=*/1, /*value=*/0);
  ExpectEmpty(stats.time_between_freezes_ms);
  ExpectEmpty(stats.resolution_of_decoded_frame);
  ExpectSizeAndAllElementsAre(stats.target_encode_bitrate, /*size=*/1,
                              /*value=*/2000.0);
  ExpectEmpty(stats.recv_key_frame_size_bytes);
  ExpectEmpty(stats.recv_delta_frame_size_bytes);
  EXPECT_EQ(stats.total_encoded_images_payload, 1000);
  EXPECT_EQ(stats.num_send_key_frames, 1);
  EXPECT_EQ(stats.num_recv_key_frames, 0);
  EXPECT_THAT(stats.dropped_by_phase, Eq(std::map<FrameDropPhase, int64_t>{
                                          {FrameDropPhase::kBeforeEncoder, 0},
                                          {FrameDropPhase::kByEncoder, 0},
                                          {FrameDropPhase::kTransport, 0},
                                          {FrameDropPhase::kByDecoder, 1},
                                          {FrameDropPhase::kAfterDecoder, 0}}));
  EXPECT_EQ(stats.encoders,
            std::vector<StreamCodecInfo>{*frame_stats.used_encoder});
  EXPECT_THAT(stats.decoders, IsEmpty());
  ExpectEmpty(stats.rendered_frame_qp);
}

TEST(DefaultVideoQualityAnalyzerFramesComparatorTest,
     DecodedDroppedKeyFrameAccountedInStats) {
  // We don't really drop frames after decoder, so it's a bit unclear what is
  // correct way to account such frames in stats, so this test just fixes some
  // current way.
  DefaultVideoQualityAnalyzerCpuMeasurer cpu_measurer;
  DefaultVideoQualityAnalyzerFramesComparator comparator(
      Clock::GetRealTimeClock(), cpu_measurer,
      DefaultVideoQualityAnalyzerOptions());

  Timestamp captured_time = Clock::GetRealTimeClock()->CurrentTime();
  uint16_t frame_id = 1;
  size_t stream = 0;
  size_t sender = 0;
  size_t receiver = 1;
  InternalStatsKey stats_key(stream, sender, receiver);

  // Frame captured
  FrameStats frame_stats(/*frame_id=*/1, captured_time);
  // Frame pre encoded
  frame_stats.pre_encode_time = captured_time + TimeDelta::Millis(10);
  // Frame encoded
  frame_stats.encoded_time = captured_time + TimeDelta::Millis(20);
  frame_stats.used_encoder =
      Vp8CodecForOneFrame(frame_id, frame_stats.encoded_time);
  frame_stats.encoded_frame_type = VideoFrameType::kVideoFrameKey;
  frame_stats.encoded_image_size = DataSize::Bytes(1000);
  frame_stats.target_encode_bitrate = 2000;
  frame_stats.spatial_layers_qp = {
      {0, StatsCounter(
              /*samples=*/{{5, Timestamp::Seconds(1)},
                           {5, Timestamp::Seconds(2)}})}};
  // Frame pre decoded
  frame_stats.pre_decoded_frame_type = VideoFrameType::kVideoFrameKey;
  frame_stats.pre_decoded_image_size = DataSize::Bytes(500);
  frame_stats.received_time = captured_time + TimeDelta::Millis(30);
  frame_stats.decode_start_time = captured_time + TimeDelta::Millis(40);
  // Frame decoded
  frame_stats.decode_end_time = captured_time + TimeDelta::Millis(50);
  frame_stats.used_decoder =
      Vp8CodecForOneFrame(frame_id, frame_stats.decode_end_time);
  frame_stats.decoded_frame_width = 200;
  frame_stats.decoded_frame_height = 100;
  frame_stats.decoded_frame_qp = 10;

  comparator.Start(/*max_threads_count=*/1);
  comparator.EnsureStatsForStream(stream, sender, /*peers_count=*/2,
                                  captured_time, captured_time);
  comparator.AddComparison(stats_key,
                           /*captured=*/std::nullopt,
                           /*rendered=*/std::nullopt,
                           FrameComparisonType::kDroppedFrame, frame_stats);
  comparator.Stop(/*last_rendered_frame_times=*/{});

  EXPECT_EQ(comparator.stream_stats().size(), 1lu);
  StreamStats stats = comparator.stream_stats().at(stats_key);
  EXPECT_EQ(stats.stream_started_time, captured_time);
  ExpectEmpty(stats.psnr);
  ExpectEmpty(stats.ssim);
  ExpectEmpty(stats.transport_time_ms);
  ExpectEmpty(stats.total_delay_incl_transport_ms);
  ExpectEmpty(stats.time_between_rendered_frames_ms);
  ExpectEmpty(stats.encode_frame_rate);
  ExpectSizeAndAllElementsAre(stats.encode_time_ms, /*size=*/1, /*value=*/10.0);
  ExpectEmpty(stats.decode_time_ms);
  ExpectEmpty(stats.receive_to_render_time_ms);
  ExpectEmpty(stats.skipped_between_rendered);
  ExpectSizeAndAllElementsAre(stats.freeze_time_ms, /*size=*/1, /*value=*/0);
  ExpectEmpty(stats.time_between_freezes_ms);
  ExpectEmpty(stats.resolution_of_decoded_frame);
  ExpectSizeAndAllElementsAre(stats.target_encode_bitrate, /*size=*/1,
                              /*value=*/2000.0);
  EXPECT_THAT(stats.spatial_layers_qp, SizeIs(1));
  ExpectSizeAndAllElementsAre(stats.spatial_layers_qp[0], /*size=*/2,
                              /*value=*/5.0);
  ExpectEmpty(stats.rendered_frame_qp);
  ExpectEmpty(stats.recv_key_frame_size_bytes);
  ExpectEmpty(stats.recv_delta_frame_size_bytes);
  EXPECT_EQ(stats.total_encoded_images_payload, 1000);
  EXPECT_EQ(stats.num_send_key_frames, 1);
  EXPECT_EQ(stats.num_recv_key_frames, 0);
  EXPECT_THAT(stats.dropped_by_phase, Eq(std::map<FrameDropPhase, int64_t>{
                                          {FrameDropPhase::kBeforeEncoder, 0},
                                          {FrameDropPhase::kByEncoder, 0},
                                          {FrameDropPhase::kTransport, 0},
                                          {FrameDropPhase::kByDecoder, 0},
                                          {FrameDropPhase::kAfterDecoder, 1}}));
  EXPECT_EQ(stats.encoders,
            std::vector<StreamCodecInfo>{*frame_stats.used_encoder});
  EXPECT_EQ(stats.decoders,
            std::vector<StreamCodecInfo>{*frame_stats.used_decoder});
}

TEST(DefaultVideoQualityAnalyzerFramesComparatorTest,
     DecoderFailedDroppedKeyFrameAccountedInStats) {
  DefaultVideoQualityAnalyzerCpuMeasurer cpu_measurer;
  DefaultVideoQualityAnalyzerFramesComparator comparator(
      Clock::GetRealTimeClock(), cpu_measurer,
      DefaultVideoQualityAnalyzerOptions());

  Timestamp captured_time = Clock::GetRealTimeClock()->CurrentTime();
  uint16_t frame_id = 1;
  size_t stream = 0;
  size_t sender = 0;
  size_t receiver = 1;
  InternalStatsKey stats_key(stream, sender, receiver);

  // Frame captured
  FrameStats frame_stats(/*frame_id=*/1, captured_time);
  // Frame pre encoded
  frame_stats.pre_encode_time = captured_time + TimeDelta::Millis(10);
  // Frame encoded
  frame_stats.encoded_time = captured_time + TimeDelta::Millis(20);
  frame_stats.used_encoder =
      Vp8CodecForOneFrame(frame_id, frame_stats.encoded_time);
  frame_stats.encoded_frame_type = VideoFrameType::kVideoFrameKey;
  frame_stats.encoded_image_size = DataSize::Bytes(1000);
  frame_stats.target_encode_bitrate = 2000;
  frame_stats.spatial_layers_qp = {
      {0, StatsCounter(
              /*samples=*/{{5, Timestamp::Seconds(1)},
                           {5, Timestamp::Seconds(2)}})}};
  // Frame pre decoded
  frame_stats.pre_decoded_frame_type = VideoFrameType::kVideoFrameKey;
  frame_stats.pre_decoded_image_size = DataSize::Bytes(500);
  frame_stats.received_time = captured_time + TimeDelta::Millis(30);
  frame_stats.decode_start_time = captured_time + TimeDelta::Millis(40);
  // Frame decoded
  frame_stats.decoder_failed = true;
  frame_stats.used_decoder =
      Vp8CodecForOneFrame(frame_id, frame_stats.decode_end_time);
  frame_stats.decoded_frame_qp = 10;

  comparator.Start(/*max_threads_count=*/1);
  comparator.EnsureStatsForStream(stream, sender, /*peers_count=*/2,
                                  captured_time, captured_time);
  comparator.AddComparison(stats_key,
                           /*captured=*/std::nullopt,
                           /*rendered=*/std::nullopt,
                           FrameComparisonType::kDroppedFrame, frame_stats);
  comparator.Stop(/*last_rendered_frame_times=*/{});

  EXPECT_EQ(comparator.stream_stats().size(), 1lu);
  StreamStats stats = comparator.stream_stats().at(stats_key);
  EXPECT_EQ(stats.stream_started_time, captured_time);
  ExpectEmpty(stats.psnr);
  ExpectEmpty(stats.ssim);
  ExpectSizeAndAllElementsAre(stats.transport_time_ms, /*size=*/1,
                              /*value=*/20.0);
  ExpectEmpty(stats.total_delay_incl_transport_ms);
  ExpectEmpty(stats.time_between_rendered_frames_ms);
  ExpectEmpty(stats.encode_frame_rate);
  ExpectSizeAndAllElementsAre(stats.encode_time_ms, /*size=*/1, /*value=*/10.0);
  ExpectEmpty(stats.decode_time_ms);
  ExpectEmpty(stats.receive_to_render_time_ms);
  ExpectEmpty(stats.skipped_between_rendered);
  ExpectSizeAndAllElementsAre(stats.freeze_time_ms, /*size=*/1, /*value=*/0);
  ExpectEmpty(stats.time_between_freezes_ms);
  ExpectEmpty(stats.resolution_of_decoded_frame);
  ExpectSizeAndAllElementsAre(stats.target_encode_bitrate, /*size=*/1,
                              /*value=*/2000.0);
  EXPECT_THAT(stats.spatial_layers_qp, SizeIs(1));
  ExpectSizeAndAllElementsAre(stats.spatial_layers_qp[0], /*size=*/2,
                              /*value=*/5.0);
  ExpectEmpty(stats.rendered_frame_qp);
  ExpectSizeAndAllElementsAre(stats.recv_key_frame_size_bytes, /*size=*/1,
                              /*value=*/500.0);
  ExpectEmpty(stats.recv_delta_frame_size_bytes);
  EXPECT_EQ(stats.total_encoded_images_payload, 1000);
  EXPECT_EQ(stats.num_send_key_frames, 1);
  EXPECT_EQ(stats.num_recv_key_frames, 1);
  EXPECT_THAT(stats.dropped_by_phase, Eq(std::map<FrameDropPhase, int64_t>{
                                          {FrameDropPhase::kBeforeEncoder, 0},
                                          {FrameDropPhase::kByEncoder, 0},
                                          {FrameDropPhase::kTransport, 0},
                                          {FrameDropPhase::kByDecoder, 1},
                                          {FrameDropPhase::kAfterDecoder, 0}}));
  EXPECT_EQ(stats.encoders,
            std::vector<StreamCodecInfo>{*frame_stats.used_encoder});
  EXPECT_EQ(stats.decoders,
            std::vector<StreamCodecInfo>{*frame_stats.used_decoder});
}
// Dropped frame end

// Regular frame start
TEST(DefaultVideoQualityAnalyzerFramesComparatorTest,
     RenderedKeyFrameAccountedInStats) {
  DefaultVideoQualityAnalyzerCpuMeasurer cpu_measurer;
  DefaultVideoQualityAnalyzerFramesComparator comparator(
      Clock::GetRealTimeClock(), cpu_measurer,
      DefaultVideoQualityAnalyzerOptions());

  Timestamp captured_time = Clock::GetRealTimeClock()->CurrentTime();
  uint16_t frame_id = 1;
  size_t stream = 0;
  size_t sender = 0;
  size_t receiver = 1;
  InternalStatsKey stats_key(stream, sender, receiver);

  // Frame captured
  VideoFrame frame =
      CreateFrame(frame_id, /*width=*/320, /*height=*/180, captured_time);
  FrameStats frame_stats(/*frame_id=*/1, captured_time);
  // Frame pre encoded
  frame_stats.pre_encode_time = captured_time + TimeDelta::Millis(10);
  // Frame encoded
  frame_stats.encoded_time = captured_time + TimeDelta::Millis(20);
  frame_stats.used_encoder =
      Vp8CodecForOneFrame(frame_id, frame_stats.encoded_time);
  frame_stats.encoded_frame_type = VideoFrameType::kVideoFrameKey;
  frame_stats.encoded_image_size = DataSize::Bytes(1000);
  frame_stats.target_encode_bitrate = 2000;
  frame_stats.spatial_layers_qp = {
      {0, StatsCounter(
              /*samples=*/{{5, Timestamp::Seconds(1)},
                           {5, Timestamp::Seconds(2)}})}};
  // Frame pre decoded
  frame_stats.pre_decoded_frame_type = VideoFrameType::kVideoFrameKey;
  frame_stats.pre_decoded_image_size = DataSize::Bytes(500);
  frame_stats.received_time = captured_time + TimeDelta::Millis(30);
  frame_stats.decode_start_time = captured_time + TimeDelta::Millis(40);
  // Frame decoded
  frame_stats.decode_end_time = captured_time + TimeDelta::Millis(50);
  frame_stats.used_decoder =
      Vp8CodecForOneFrame(frame_id, frame_stats.decode_end_time);
  frame_stats.decoded_frame_width = 200;
  frame_stats.decoded_frame_height = 100;
  frame_stats.decoded_frame_qp = 10;
  // Frame rendered
  frame_stats.rendered_time = captured_time + TimeDelta::Millis(60);

  comparator.Start(/*max_threads_count=*/1);
  comparator.EnsureStatsForStream(stream, sender, /*peers_count=*/2,
                                  captured_time, captured_time);
  comparator.AddComparison(stats_key,
                           /*captured=*/frame,
                           /*rendered=*/frame, FrameComparisonType::kRegular,
                           frame_stats);
  comparator.Stop(/*last_rendered_frame_times=*/{});

  EXPECT_EQ(comparator.stream_stats().size(), 1lu);
  StreamStats stats = comparator.stream_stats().at(stats_key);
  EXPECT_EQ(stats.stream_started_time, captured_time);
  EXPECT_GE(GetFirstOrDie(stats.psnr), 20);
  EXPECT_GE(GetFirstOrDie(stats.ssim), 0.5);
  ExpectSizeAndAllElementsAre(stats.transport_time_ms, /*size=*/1,
                              /*value=*/20.0);
  EXPECT_GE(GetFirstOrDie(stats.total_delay_incl_transport_ms), 60.0);
  ExpectEmpty(stats.time_between_rendered_frames_ms);
  ExpectEmpty(stats.encode_frame_rate);
  ExpectSizeAndAllElementsAre(stats.encode_time_ms, /*size=*/1, /*value=*/10.0);
  EXPECT_GE(GetFirstOrDie(stats.decode_time_ms), 10.0);
  EXPECT_GE(GetFirstOrDie(stats.receive_to_render_time_ms), 30.0);
  ExpectEmpty(stats.skipped_between_rendered);
  ExpectSizeAndAllElementsAre(stats.freeze_time_ms, /*size=*/1, /*value=*/0);
  ExpectEmpty(stats.time_between_freezes_ms);
  EXPECT_GE(GetFirstOrDie(stats.resolution_of_decoded_frame), 200 * 100.0);
  ExpectSizeAndAllElementsAre(stats.target_encode_bitrate, /*size=*/1,
                              /*value=*/2000.0);
  EXPECT_THAT(stats.spatial_layers_qp, SizeIs(1));
  ExpectSizeAndAllElementsAre(stats.spatial_layers_qp[0], /*size=*/2,
                              /*value=*/5.0);
  ExpectSizeAndAllElementsAre(stats.rendered_frame_qp, /*size=*/1,
                              /*value=*/10.0);
  ExpectSizeAndAllElementsAre(stats.recv_key_frame_size_bytes, /*size=*/1,
                              /*value=*/500.0);
  ExpectEmpty(stats.recv_delta_frame_size_bytes);
  EXPECT_EQ(stats.total_encoded_images_payload, 1000);
  EXPECT_EQ(stats.num_send_key_frames, 1);
  EXPECT_EQ(stats.num_recv_key_frames, 1);
  EXPECT_THAT(stats.dropped_by_phase, Eq(std::map<FrameDropPhase, int64_t>{
                                          {FrameDropPhase::kBeforeEncoder, 0},
                                          {FrameDropPhase::kByEncoder, 0},
                                          {FrameDropPhase::kTransport, 0},
                                          {FrameDropPhase::kByDecoder, 0},
                                          {FrameDropPhase::kAfterDecoder, 0}}));
  EXPECT_EQ(stats.encoders,
            std::vector<StreamCodecInfo>{*frame_stats.used_encoder});
  EXPECT_EQ(stats.decoders,
            std::vector<StreamCodecInfo>{*frame_stats.used_decoder});
}

TEST(DefaultVideoQualityAnalyzerFramesComparatorTest, AllStatsHaveMetadataSet) {
  DefaultVideoQualityAnalyzerCpuMeasurer cpu_measurer;
  DefaultVideoQualityAnalyzerFramesComparator comparator(
      Clock::GetRealTimeClock(), cpu_measurer,
      DefaultVideoQualityAnalyzerOptions());

  Timestamp captured_time = Clock::GetRealTimeClock()->CurrentTime();
  uint16_t frame_id = 1;
  size_t stream = 0;
  size_t sender = 0;
  size_t receiver = 1;
  InternalStatsKey stats_key(stream, sender, receiver);

  // Frame captured
  VideoFrame frame =
      CreateFrame(frame_id, /*width=*/320, /*height=*/180, captured_time);
  FrameStats frame_stats(/*frame_id=*/1, captured_time);
  // Frame pre encoded
  frame_stats.pre_encode_time = captured_time + TimeDelta::Millis(10);
  // Frame encoded
  frame_stats.encoded_time = captured_time + TimeDelta::Millis(20);
  frame_stats.used_encoder =
      Vp8CodecForOneFrame(frame_id, frame_stats.encoded_time);
  frame_stats.encoded_frame_type = VideoFrameType::kVideoFrameKey;
  frame_stats.encoded_image_size = DataSize::Bytes(1000);
  frame_stats.target_encode_bitrate = 2000;
  frame_stats.spatial_layers_qp = {
      {0, StatsCounter(
              /*samples=*/{{5, Timestamp::Seconds(1)},
                           {5, Timestamp::Seconds(2)}})}};
  // Frame pre decoded
  frame_stats.pre_decoded_frame_type = VideoFrameType::kVideoFrameKey;
  frame_stats.pre_decoded_image_size = DataSize::Bytes(500);
  frame_stats.received_time = captured_time + TimeDelta::Millis(30);
  frame_stats.decode_start_time = captured_time + TimeDelta::Millis(40);
  // Frame decoded
  frame_stats.decode_end_time = captured_time + TimeDelta::Millis(50);
  frame_stats.used_decoder =
      Vp8CodecForOneFrame(frame_id, frame_stats.decode_end_time);
  // Frame rendered
  frame_stats.rendered_time = captured_time + TimeDelta::Millis(60);
  frame_stats.decoded_frame_width = 200;
  frame_stats.decoded_frame_height = 100;
  frame_stats.decoded_frame_qp = 10;

  comparator.Start(/*max_threads_count=*/1);
  comparator.EnsureStatsForStream(stream, sender, /*peers_count=*/2,
                                  captured_time, captured_time);
  comparator.AddComparison(stats_key,
                           /*captured=*/frame,
                           /*rendered=*/frame, FrameComparisonType::kRegular,
                           frame_stats);
  comparator.Stop(/*last_rendered_frame_times=*/{});

  EXPECT_EQ(comparator.stream_stats().size(), 1lu);
  StreamStats stats = comparator.stream_stats().at(stats_key);
  AssertFirstMetadataHasField(stats.psnr, "frame_id", "1");
  AssertFirstMetadataHasField(stats.ssim, "frame_id", "1");
  AssertFirstMetadataHasField(stats.transport_time_ms, "frame_id", "1");
  AssertFirstMetadataHasField(stats.total_delay_incl_transport_ms, "frame_id",
                              "1");
  AssertFirstMetadataHasField(stats.encode_time_ms, "frame_id", "1");
  AssertFirstMetadataHasField(stats.decode_time_ms, "frame_id", "1");
  AssertFirstMetadataHasField(stats.receive_to_render_time_ms, "frame_id", "1");
  AssertFirstMetadataHasField(stats.resolution_of_decoded_frame, "frame_id",
                              "1");
  AssertFirstMetadataHasField(stats.target_encode_bitrate, "frame_id", "1");
  AssertFirstMetadataHasField(stats.spatial_layers_qp[0], "frame_id", "1");
  AssertFirstMetadataHasField(stats.recv_key_frame_size_bytes, "frame_id", "1");
  AssertFirstMetadataHasField(stats.rendered_frame_qp, "frame_id", "1");

  ExpectEmpty(stats.recv_delta_frame_size_bytes);
}
// Regular frame end

TEST(DefaultVideoQualityAnalyzerFramesComparatorTest,
     FreezeStatsPresentedWithMetadataAfterAddFrameWithSkippedAndDelay) {
  DefaultVideoQualityAnalyzerCpuMeasurer cpu_measurer;
  DefaultVideoQualityAnalyzerFramesComparator comparator(
      Clock::GetRealTimeClock(), cpu_measurer, AnalyzerOptionsForTest());

  Timestamp stream_start_time = Clock::GetRealTimeClock()->CurrentTime();
  size_t stream = 0;
  size_t sender = 0;
  size_t receiver = 1;
  size_t peers_count = 2;
  InternalStatsKey stats_key(stream, sender, receiver);

  comparator.Start(/*max_threads_count=*/1);
  comparator.EnsureStatsForStream(stream, sender, peers_count,
                                  stream_start_time, stream_start_time);

  // Add 5 frames which were rendered with 30 fps (~30ms between frames)
  // Frame ids are in [1..5] and last frame is with 120ms offset from first.
  std::optional<Timestamp> prev_frame_rendered_time = std::nullopt;
  for (int i = 0; i < 5; ++i) {
    FrameStats frame_stats = FrameStatsWith10msDeltaBetweenPhasesAnd10x10Frame(
        /*frame_id=*/i + 1, stream_start_time + TimeDelta::Millis(30 * i));
    frame_stats.prev_frame_rendered_time = prev_frame_rendered_time;
    frame_stats.time_between_rendered_frames =
        prev_frame_rendered_time.has_value()
            ? std::optional<TimeDelta>(frame_stats.rendered_time -
                                       *prev_frame_rendered_time)
            : std::nullopt;
    prev_frame_rendered_time = frame_stats.rendered_time;

    comparator.AddComparison(stats_key,
                             /*captured=*/std::nullopt,
                             /*rendered=*/std::nullopt,
                             FrameComparisonType::kRegular, frame_stats);
  }

  // Next frame was rendered with 4 frames skipped and delay 300ms after last
  // frame.
  FrameStats freeze_frame_stats =
      FrameStatsWith10msDeltaBetweenPhasesAnd10x10Frame(
          /*frame_id=*/10, stream_start_time + TimeDelta::Millis(120 + 300));
  freeze_frame_stats.prev_frame_rendered_time = prev_frame_rendered_time;
  freeze_frame_stats.time_between_rendered_frames =
      freeze_frame_stats.rendered_time - *prev_frame_rendered_time;

  comparator.AddComparison(stats_key,
                           /*skipped_between_rendered=*/4,
                           /*captured=*/std::nullopt,
                           /*rendered=*/std::nullopt,
                           FrameComparisonType::kRegular, freeze_frame_stats);
  comparator.Stop(/*last_rendered_frame_times=*/{});

  StreamStats stats = comparator.stream_stats().at(stats_key);
  ASSERT_THAT(GetFirstOrDie(stats.skipped_between_rendered), Eq(4));
  AssertFirstMetadataHasField(stats.skipped_between_rendered, "frame_id", "10");
  ASSERT_THAT(GetFirstOrDie(stats.freeze_time_ms), Eq(300));
  AssertFirstMetadataHasField(stats.freeze_time_ms, "frame_id", "10");
  // 180ms is time from the stream start to the rendered time of the last frame
  // among first 5 frames which were received before freeze.
  ASSERT_THAT(GetFirstOrDie(stats.time_between_freezes_ms), Eq(180));
  AssertFirstMetadataHasField(stats.time_between_freezes_ms, "frame_id", "10");
}
// Stats validation tests end.

}  // namespace
}  // namespace webrtc
