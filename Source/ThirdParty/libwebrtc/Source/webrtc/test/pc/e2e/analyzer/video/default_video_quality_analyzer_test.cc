/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <map>
#include <memory>
#include <vector>

#include "api/rtp_packet_info.h"
#include "api/rtp_packet_infos.h"
#include "api/test/create_frame_generator.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_tools/frame_analyzer/video_geometry_aligner.h"
#include "system_wrappers/include/sleep.h"
#include "test/gtest.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

using StatsSample = ::webrtc::SamplesStatsCounter::StatsSample;

constexpr int kAnalyzerMaxThreadsCount = 1;
constexpr int kMaxFramesInFlightPerStream = 10;
constexpr int kFrameWidth = 320;
constexpr int kFrameHeight = 240;
constexpr double kMaxSsim = 1;
constexpr char kStreamLabel[] = "video-stream";
constexpr char kSenderPeerName[] = "alice";
constexpr char kReceiverPeerName[] = "bob";

DefaultVideoQualityAnalyzerOptions AnalyzerOptionsForTest() {
  DefaultVideoQualityAnalyzerOptions options;
  options.heavy_metrics_computation_enabled = false;
  options.adjust_cropping_before_comparing_frames = false;
  options.max_frames_in_flight_per_stream_count = kMaxFramesInFlightPerStream;
  return options;
}

VideoFrame NextFrame(test::FrameGeneratorInterface* frame_generator,
                     int64_t timestamp_us) {
  test::FrameGeneratorInterface::VideoFrameData frame_data =
      frame_generator->NextFrame();
  return VideoFrame::Builder()
      .set_video_frame_buffer(frame_data.buffer)
      .set_update_rect(frame_data.update_rect)
      .set_timestamp_us(timestamp_us)
      .build();
}

EncodedImage FakeEncode(const VideoFrame& frame) {
  EncodedImage image;
  std::vector<RtpPacketInfo> packet_infos;
  packet_infos.push_back(RtpPacketInfo(
      /*ssrc=*/1,
      /*csrcs=*/{},
      /*rtp_timestamp=*/frame.timestamp(),
      /*audio_level=*/absl::nullopt,
      /*absolute_capture_time=*/absl::nullopt,
      /*receive_time=*/Timestamp::Micros(frame.timestamp_us() + 10000)));
  image.SetPacketInfos(RtpPacketInfos(packet_infos));
  return image;
}

VideoFrame DeepCopy(const VideoFrame& frame) {
  VideoFrame copy = frame;
  copy.set_video_frame_buffer(
      I420Buffer::Copy(*frame.video_frame_buffer()->ToI420()));
  return copy;
}

std::vector<StatsSample> GetSortedSamples(const SamplesStatsCounter& counter) {
  rtc::ArrayView<const StatsSample> view = counter.GetTimedSamples();
  std::vector<StatsSample> out(view.begin(), view.end());
  std::sort(out.begin(), out.end(),
            [](const StatsSample& a, const StatsSample& b) {
              return a.time < b.time;
            });
  return out;
}

std::string ToString(const std::vector<StatsSample>& values) {
  rtc::StringBuilder out;
  for (const auto& v : values) {
    out << "{ time_ms=" << v.time.ms() << "; value=" << v.value << "}, ";
  }
  return out.str();
}

void FakeCPULoad() {
  std::vector<int> temp(1000000);
  for (size_t i = 0; i < temp.size(); ++i) {
    temp[i] = rand();
  }
  std::sort(temp.begin(), temp.end());
  ASSERT_TRUE(std::is_sorted(temp.begin(), temp.end()));
}

TEST(DefaultVideoQualityAnalyzerTest,
     MemoryOverloadedAndThenAllFramesReceived) {
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      test::CreateSquareFrameGenerator(kFrameWidth, kFrameHeight,
                                       /*type=*/absl::nullopt,
                                       /*num_squares=*/absl::nullopt);

  DefaultVideoQualityAnalyzer analyzer(Clock::GetRealTimeClock(),
                                       AnalyzerOptionsForTest());
  analyzer.Start("test_case",
                 std::vector<std::string>{kSenderPeerName, kReceiverPeerName},
                 kAnalyzerMaxThreadsCount);

  std::map<uint16_t, VideoFrame> captured_frames;
  std::vector<uint16_t> frames_order;
  for (int i = 0; i < kMaxFramesInFlightPerStream * 2; ++i) {
    VideoFrame frame = NextFrame(frame_generator.get(), i);
    frame.set_id(
        analyzer.OnFrameCaptured(kSenderPeerName, kStreamLabel, frame));
    frames_order.push_back(frame.id());
    captured_frames.insert({frame.id(), frame});
    analyzer.OnFramePreEncode(kSenderPeerName, frame);
    analyzer.OnFrameEncoded(kSenderPeerName, frame.id(), FakeEncode(frame),
                            VideoQualityAnalyzerInterface::EncoderStats());
  }

  for (const uint16_t& frame_id : frames_order) {
    VideoFrame received_frame = DeepCopy(captured_frames.at(frame_id));
    analyzer.OnFramePreDecode(kReceiverPeerName, received_frame.id(),
                              FakeEncode(received_frame));
    analyzer.OnFrameDecoded(kReceiverPeerName, received_frame,
                            VideoQualityAnalyzerInterface::DecoderStats());
    analyzer.OnFrameRendered(kReceiverPeerName, received_frame);
  }

  // Give analyzer some time to process frames on async thread. The computations
  // have to be fast (heavy metrics are disabled!), so if doesn't fit 100ms it
  // means we have an issue!
  SleepMs(100);
  analyzer.Stop();

  AnalyzerStats stats = analyzer.GetAnalyzerStats();
  EXPECT_EQ(stats.memory_overloaded_comparisons_done,
            kMaxFramesInFlightPerStream);
  EXPECT_EQ(stats.comparisons_done, kMaxFramesInFlightPerStream * 2);
  FrameCounters frame_counters = analyzer.GetGlobalCounters();
  EXPECT_EQ(frame_counters.captured, kMaxFramesInFlightPerStream * 2);
  EXPECT_EQ(frame_counters.rendered, kMaxFramesInFlightPerStream * 2);
  EXPECT_EQ(frame_counters.dropped, 0);
}

TEST(DefaultVideoQualityAnalyzerTest,
     FillMaxMemoryReceiveAllMemoryOverloadedAndThenAllFramesReceived) {
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      test::CreateSquareFrameGenerator(kFrameWidth, kFrameHeight,
                                       /*type=*/absl::nullopt,
                                       /*num_squares=*/absl::nullopt);

  DefaultVideoQualityAnalyzer analyzer(Clock::GetRealTimeClock(),
                                       AnalyzerOptionsForTest());
  analyzer.Start("test_case",
                 std::vector<std::string>{kSenderPeerName, kReceiverPeerName},
                 kAnalyzerMaxThreadsCount);

  std::map<uint16_t, VideoFrame> captured_frames;
  std::vector<uint16_t> frames_order;
  // Feel analyzer's memory up to limit
  for (int i = 0; i < kMaxFramesInFlightPerStream; ++i) {
    VideoFrame frame = NextFrame(frame_generator.get(), i);
    frame.set_id(
        analyzer.OnFrameCaptured(kSenderPeerName, kStreamLabel, frame));
    frames_order.push_back(frame.id());
    captured_frames.insert({frame.id(), frame});
    analyzer.OnFramePreEncode(kSenderPeerName, frame);
    analyzer.OnFrameEncoded(kSenderPeerName, frame.id(), FakeEncode(frame),
                            VideoQualityAnalyzerInterface::EncoderStats());
  }

  // Receive all frames.
  for (const uint16_t& frame_id : frames_order) {
    VideoFrame received_frame = DeepCopy(captured_frames.at(frame_id));
    analyzer.OnFramePreDecode(kReceiverPeerName, received_frame.id(),
                              FakeEncode(received_frame));
    analyzer.OnFrameDecoded(kReceiverPeerName, received_frame,
                            VideoQualityAnalyzerInterface::DecoderStats());
    analyzer.OnFrameRendered(kReceiverPeerName, received_frame);
  }
  frames_order.clear();

  // Give analyzer some time to process frames on async thread. The computations
  // have to be fast (heavy metrics are disabled!), so if doesn't fit 100ms it
  // means we have an issue!
  SleepMs(100);

  // Overload analyzer's memory up to limit
  for (int i = 0; i < 2 * kMaxFramesInFlightPerStream; ++i) {
    VideoFrame frame = NextFrame(frame_generator.get(), i);
    frame.set_id(
        analyzer.OnFrameCaptured(kSenderPeerName, kStreamLabel, frame));
    frames_order.push_back(frame.id());
    captured_frames.insert({frame.id(), frame});
    analyzer.OnFramePreEncode(kSenderPeerName, frame);
    analyzer.OnFrameEncoded(kSenderPeerName, frame.id(), FakeEncode(frame),
                            VideoQualityAnalyzerInterface::EncoderStats());
  }

  // Receive all frames.
  for (const uint16_t& frame_id : frames_order) {
    VideoFrame received_frame = DeepCopy(captured_frames.at(frame_id));
    analyzer.OnFramePreDecode(kReceiverPeerName, received_frame.id(),
                              FakeEncode(received_frame));
    analyzer.OnFrameDecoded(kReceiverPeerName, received_frame,
                            VideoQualityAnalyzerInterface::DecoderStats());
    analyzer.OnFrameRendered(kReceiverPeerName, received_frame);
  }

  // Give analyzer some time to process frames on async thread. The computations
  // have to be fast (heavy metrics are disabled!), so if doesn't fit 100ms it
  // means we have an issue!
  SleepMs(100);
  analyzer.Stop();

  AnalyzerStats stats = analyzer.GetAnalyzerStats();
  EXPECT_EQ(stats.memory_overloaded_comparisons_done,
            kMaxFramesInFlightPerStream);
  EXPECT_EQ(stats.comparisons_done, kMaxFramesInFlightPerStream * 3);
  FrameCounters frame_counters = analyzer.GetGlobalCounters();
  EXPECT_EQ(frame_counters.captured, kMaxFramesInFlightPerStream * 3);
  EXPECT_EQ(frame_counters.rendered, kMaxFramesInFlightPerStream * 3);
  EXPECT_EQ(frame_counters.dropped, 0);
}

TEST(DefaultVideoQualityAnalyzerTest,
     MemoryOverloadedHalfDroppedAndThenHalfFramesReceived) {
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      test::CreateSquareFrameGenerator(kFrameWidth, kFrameHeight,
                                       /*type=*/absl::nullopt,
                                       /*num_squares=*/absl::nullopt);

  DefaultVideoQualityAnalyzer analyzer(Clock::GetRealTimeClock(),
                                       AnalyzerOptionsForTest());
  analyzer.Start("test_case",
                 std::vector<std::string>{kSenderPeerName, kReceiverPeerName},
                 kAnalyzerMaxThreadsCount);

  std::map<uint16_t, VideoFrame> captured_frames;
  std::vector<uint16_t> frames_order;
  for (int i = 0; i < kMaxFramesInFlightPerStream * 2; ++i) {
    VideoFrame frame = NextFrame(frame_generator.get(), i);
    frame.set_id(
        analyzer.OnFrameCaptured(kSenderPeerName, kStreamLabel, frame));
    frames_order.push_back(frame.id());
    captured_frames.insert({frame.id(), frame});
    analyzer.OnFramePreEncode(kSenderPeerName, frame);
    analyzer.OnFrameEncoded(kSenderPeerName, frame.id(), FakeEncode(frame),
                            VideoQualityAnalyzerInterface::EncoderStats());
  }

  for (size_t i = kMaxFramesInFlightPerStream; i < frames_order.size(); ++i) {
    uint16_t frame_id = frames_order.at(i);
    VideoFrame received_frame = DeepCopy(captured_frames.at(frame_id));
    analyzer.OnFramePreDecode(kReceiverPeerName, received_frame.id(),
                              FakeEncode(received_frame));
    analyzer.OnFrameDecoded(kReceiverPeerName, received_frame,
                            VideoQualityAnalyzerInterface::DecoderStats());
    analyzer.OnFrameRendered(kReceiverPeerName, received_frame);
  }

  // Give analyzer some time to process frames on async thread. The computations
  // have to be fast (heavy metrics are disabled!), so if doesn't fit 100ms it
  // means we have an issue!
  SleepMs(100);
  analyzer.Stop();

  AnalyzerStats stats = analyzer.GetAnalyzerStats();
  EXPECT_EQ(stats.memory_overloaded_comparisons_done, 0);
  EXPECT_EQ(stats.comparisons_done, kMaxFramesInFlightPerStream * 2);
  FrameCounters frame_counters = analyzer.GetGlobalCounters();
  EXPECT_EQ(frame_counters.captured, kMaxFramesInFlightPerStream * 2);
  EXPECT_EQ(frame_counters.rendered, kMaxFramesInFlightPerStream);
  EXPECT_EQ(frame_counters.dropped, kMaxFramesInFlightPerStream);
}

TEST(DefaultVideoQualityAnalyzerTest, NormalScenario) {
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      test::CreateSquareFrameGenerator(kFrameWidth, kFrameHeight,
                                       /*type=*/absl::nullopt,
                                       /*num_squares=*/absl::nullopt);

  DefaultVideoQualityAnalyzer analyzer(Clock::GetRealTimeClock(),
                                       AnalyzerOptionsForTest());
  analyzer.Start("test_case",
                 std::vector<std::string>{kSenderPeerName, kReceiverPeerName},
                 kAnalyzerMaxThreadsCount);

  std::map<uint16_t, VideoFrame> captured_frames;
  std::vector<uint16_t> frames_order;
  for (int i = 0; i < kMaxFramesInFlightPerStream; ++i) {
    VideoFrame frame = NextFrame(frame_generator.get(), i);
    frame.set_id(
        analyzer.OnFrameCaptured(kSenderPeerName, kStreamLabel, frame));
    frames_order.push_back(frame.id());
    captured_frames.insert({frame.id(), frame});
    analyzer.OnFramePreEncode(kSenderPeerName, frame);
    analyzer.OnFrameEncoded(kSenderPeerName, frame.id(), FakeEncode(frame),
                            VideoQualityAnalyzerInterface::EncoderStats());
  }

  for (size_t i = 1; i < frames_order.size(); i += 2) {
    uint16_t frame_id = frames_order.at(i);
    VideoFrame received_frame = DeepCopy(captured_frames.at(frame_id));
    analyzer.OnFramePreDecode(kReceiverPeerName, received_frame.id(),
                              FakeEncode(received_frame));
    analyzer.OnFrameDecoded(kReceiverPeerName, received_frame,
                            VideoQualityAnalyzerInterface::DecoderStats());
    analyzer.OnFrameRendered(kReceiverPeerName, received_frame);
  }

  // Give analyzer some time to process frames on async thread. The computations
  // have to be fast (heavy metrics are disabled!), so if doesn't fit 100ms it
  // means we have an issue!
  SleepMs(100);
  analyzer.Stop();

  AnalyzerStats stats = analyzer.GetAnalyzerStats();
  EXPECT_EQ(stats.memory_overloaded_comparisons_done, 0);
  EXPECT_EQ(stats.comparisons_done, kMaxFramesInFlightPerStream);

  std::vector<StatsSample> frames_in_flight_sizes =
      GetSortedSamples(stats.frames_in_flight_left_count);
  EXPECT_EQ(frames_in_flight_sizes.back().value, 0)
      << ToString(frames_in_flight_sizes);

  FrameCounters frame_counters = analyzer.GetGlobalCounters();
  EXPECT_EQ(frame_counters.captured, kMaxFramesInFlightPerStream);
  EXPECT_EQ(frame_counters.received, kMaxFramesInFlightPerStream / 2);
  EXPECT_EQ(frame_counters.decoded, kMaxFramesInFlightPerStream / 2);
  EXPECT_EQ(frame_counters.rendered, kMaxFramesInFlightPerStream / 2);
  EXPECT_EQ(frame_counters.dropped, kMaxFramesInFlightPerStream / 2);
}

TEST(DefaultVideoQualityAnalyzerTest, OneFrameReceivedTwice) {
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      test::CreateSquareFrameGenerator(kFrameWidth, kFrameHeight,
                                       /*type=*/absl::nullopt,
                                       /*num_squares=*/absl::nullopt);

  DefaultVideoQualityAnalyzer analyzer(Clock::GetRealTimeClock(),
                                       AnalyzerOptionsForTest());
  analyzer.Start("test_case",
                 std::vector<std::string>{kSenderPeerName, kReceiverPeerName},
                 kAnalyzerMaxThreadsCount);

  VideoFrame captured_frame = NextFrame(frame_generator.get(), 0);
  captured_frame.set_id(
      analyzer.OnFrameCaptured(kSenderPeerName, kStreamLabel, captured_frame));
  analyzer.OnFramePreEncode(kSenderPeerName, captured_frame);
  analyzer.OnFrameEncoded(kSenderPeerName, captured_frame.id(),
                          FakeEncode(captured_frame),
                          VideoQualityAnalyzerInterface::EncoderStats());

  VideoFrame received_frame = DeepCopy(captured_frame);
  analyzer.OnFramePreDecode(kReceiverPeerName, received_frame.id(),
                            FakeEncode(received_frame));
  analyzer.OnFrameDecoded(kReceiverPeerName, received_frame,
                          VideoQualityAnalyzerInterface::DecoderStats());
  analyzer.OnFrameRendered(kReceiverPeerName, received_frame);

  received_frame = DeepCopy(captured_frame);
  analyzer.OnFramePreDecode(kReceiverPeerName, received_frame.id(),
                            FakeEncode(received_frame));
  analyzer.OnFrameDecoded(kReceiverPeerName, received_frame,
                          VideoQualityAnalyzerInterface::DecoderStats());
  analyzer.OnFrameRendered(kReceiverPeerName, received_frame);

  // Give analyzer some time to process frames on async thread. The computations
  // have to be fast (heavy metrics are disabled!), so if doesn't fit 100ms it
  // means we have an issue!
  SleepMs(100);
  analyzer.Stop();

  AnalyzerStats stats = analyzer.GetAnalyzerStats();
  EXPECT_EQ(stats.memory_overloaded_comparisons_done, 0);
  EXPECT_EQ(stats.comparisons_done, 1);

  FrameCounters frame_counters = analyzer.GetGlobalCounters();
  EXPECT_EQ(frame_counters.captured, 1);
  EXPECT_EQ(frame_counters.received, 1);
  EXPECT_EQ(frame_counters.decoded, 1);
  EXPECT_EQ(frame_counters.rendered, 1);
  EXPECT_EQ(frame_counters.dropped, 0);
}

TEST(DefaultVideoQualityAnalyzerTest, NormalScenario2Receivers) {
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      test::CreateSquareFrameGenerator(kFrameWidth, kFrameHeight,
                                       /*type=*/absl::nullopt,
                                       /*num_squares=*/absl::nullopt);

  constexpr char kAlice[] = "alice";
  constexpr char kBob[] = "bob";
  constexpr char kCharlie[] = "charlie";

  DefaultVideoQualityAnalyzer analyzer(Clock::GetRealTimeClock(),
                                       AnalyzerOptionsForTest());
  analyzer.Start("test_case", std::vector<std::string>{kAlice, kBob, kCharlie},
                 kAnalyzerMaxThreadsCount);

  std::map<uint16_t, VideoFrame> captured_frames;
  std::vector<uint16_t> frames_order;
  for (int i = 0; i < kMaxFramesInFlightPerStream; ++i) {
    VideoFrame frame = NextFrame(frame_generator.get(), i);
    frame.set_id(analyzer.OnFrameCaptured(kAlice, kStreamLabel, frame));
    frames_order.push_back(frame.id());
    captured_frames.insert({frame.id(), frame});
    analyzer.OnFramePreEncode(kAlice, frame);
    SleepMs(20);
    analyzer.OnFrameEncoded(kAlice, frame.id(), FakeEncode(frame),
                            VideoQualityAnalyzerInterface::EncoderStats());
  }

  SleepMs(50);

  for (size_t i = 1; i < frames_order.size(); i += 2) {
    uint16_t frame_id = frames_order.at(i);
    VideoFrame received_frame = DeepCopy(captured_frames.at(frame_id));
    analyzer.OnFramePreDecode(kBob, received_frame.id(),
                              FakeEncode(received_frame));
    SleepMs(30);
    analyzer.OnFrameDecoded(kBob, received_frame,
                            VideoQualityAnalyzerInterface::DecoderStats());
    SleepMs(10);
    analyzer.OnFrameRendered(kBob, received_frame);
  }

  for (size_t i = 1; i < frames_order.size(); i += 2) {
    uint16_t frame_id = frames_order.at(i);
    VideoFrame received_frame = DeepCopy(captured_frames.at(frame_id));
    analyzer.OnFramePreDecode(kCharlie, received_frame.id(),
                              FakeEncode(received_frame));
    SleepMs(40);
    analyzer.OnFrameDecoded(kCharlie, received_frame,
                            VideoQualityAnalyzerInterface::DecoderStats());
    SleepMs(5);
    analyzer.OnFrameRendered(kCharlie, received_frame);
  }

  // Give analyzer some time to process frames on async thread. The computations
  // have to be fast (heavy metrics are disabled!), so if doesn't fit 100ms it
  // means we have an issue!
  SleepMs(100);
  analyzer.Stop();

  AnalyzerStats analyzer_stats = analyzer.GetAnalyzerStats();
  EXPECT_EQ(analyzer_stats.memory_overloaded_comparisons_done, 0);
  EXPECT_EQ(analyzer_stats.comparisons_done, kMaxFramesInFlightPerStream * 2);

  FrameCounters frame_counters = analyzer.GetGlobalCounters();
  EXPECT_EQ(frame_counters.captured, kMaxFramesInFlightPerStream);
  EXPECT_EQ(frame_counters.received, kMaxFramesInFlightPerStream);
  EXPECT_EQ(frame_counters.decoded, kMaxFramesInFlightPerStream);
  EXPECT_EQ(frame_counters.rendered, kMaxFramesInFlightPerStream);
  EXPECT_EQ(frame_counters.dropped, kMaxFramesInFlightPerStream);
  EXPECT_EQ(analyzer.GetKnownVideoStreams().size(), 2lu);
  for (auto stream_key : analyzer.GetKnownVideoStreams()) {
    FrameCounters stream_conters =
        analyzer.GetPerStreamCounters().at(stream_key);
    // On some devices the pipeline can be too slow, so we actually can't
    // force real constraints here. Lets just check, that at least 1
    // frame passed whole pipeline.
    EXPECT_GE(stream_conters.captured, 10);
    EXPECT_GE(stream_conters.pre_encoded, 10);
    EXPECT_GE(stream_conters.encoded, 10);
    EXPECT_GE(stream_conters.received, 5);
    EXPECT_GE(stream_conters.decoded, 5);
    EXPECT_GE(stream_conters.rendered, 5);
    EXPECT_GE(stream_conters.dropped, 5);
  }

  std::map<StatsKey, StreamStats> stats = analyzer.GetStats();
  const StatsKey kAliceBobStats(kStreamLabel, kAlice, kBob);
  const StatsKey kAliceCharlieStats(kStreamLabel, kAlice, kCharlie);
  EXPECT_EQ(stats.size(), 2lu);
  {
    auto it = stats.find(kAliceBobStats);
    EXPECT_FALSE(it == stats.end());
    ASSERT_FALSE(it->second.encode_time_ms.IsEmpty());
    EXPECT_GE(it->second.encode_time_ms.GetMin(), 20);
    ASSERT_FALSE(it->second.decode_time_ms.IsEmpty());
    EXPECT_GE(it->second.decode_time_ms.GetMin(), 30);
    ASSERT_FALSE(it->second.resolution_of_rendered_frame.IsEmpty());
    EXPECT_GE(it->second.resolution_of_rendered_frame.GetMin(),
              kFrameWidth * kFrameHeight - 1);
    EXPECT_LE(it->second.resolution_of_rendered_frame.GetMax(),
              kFrameWidth * kFrameHeight + 1);
  }
  {
    auto it = stats.find(kAliceCharlieStats);
    EXPECT_FALSE(it == stats.end());
    ASSERT_FALSE(it->second.encode_time_ms.IsEmpty());
    EXPECT_GE(it->second.encode_time_ms.GetMin(), 20);
    ASSERT_FALSE(it->second.decode_time_ms.IsEmpty());
    EXPECT_GE(it->second.decode_time_ms.GetMin(), 30);
    ASSERT_FALSE(it->second.resolution_of_rendered_frame.IsEmpty());
    EXPECT_GE(it->second.resolution_of_rendered_frame.GetMin(),
              kFrameWidth * kFrameHeight - 1);
    EXPECT_LE(it->second.resolution_of_rendered_frame.GetMax(),
              kFrameWidth * kFrameHeight + 1);
  }
}

TEST(DefaultVideoQualityAnalyzerTest, OneFrameReceivedTwiceWith2Receivers) {
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      test::CreateSquareFrameGenerator(kFrameWidth, kFrameHeight,
                                       /*type=*/absl::nullopt,
                                       /*num_squares=*/absl::nullopt);

  constexpr char kAlice[] = "alice";
  constexpr char kBob[] = "bob";
  constexpr char kCharlie[] = "charlie";

  DefaultVideoQualityAnalyzer analyzer(Clock::GetRealTimeClock(),
                                       AnalyzerOptionsForTest());
  analyzer.Start("test_case", std::vector<std::string>{kAlice, kBob, kCharlie},
                 kAnalyzerMaxThreadsCount);

  VideoFrame captured_frame = NextFrame(frame_generator.get(), 0);
  captured_frame.set_id(
      analyzer.OnFrameCaptured(kAlice, kStreamLabel, captured_frame));
  analyzer.OnFramePreEncode(kAlice, captured_frame);
  analyzer.OnFrameEncoded(kAlice, captured_frame.id(),
                          FakeEncode(captured_frame),
                          VideoQualityAnalyzerInterface::EncoderStats());

  VideoFrame received_frame = DeepCopy(captured_frame);
  analyzer.OnFramePreDecode(kBob, received_frame.id(),
                            FakeEncode(received_frame));
  analyzer.OnFrameDecoded(kBob, received_frame,
                          VideoQualityAnalyzerInterface::DecoderStats());
  analyzer.OnFrameRendered(kBob, received_frame);

  received_frame = DeepCopy(captured_frame);
  analyzer.OnFramePreDecode(kBob, received_frame.id(),
                            FakeEncode(received_frame));
  analyzer.OnFrameDecoded(kBob, received_frame,
                          VideoQualityAnalyzerInterface::DecoderStats());
  analyzer.OnFrameRendered(kBob, received_frame);

  // Give analyzer some time to process frames on async thread. The computations
  // have to be fast (heavy metrics are disabled!), so if doesn't fit 100ms it
  // means we have an issue!
  SleepMs(100);
  analyzer.Stop();

  AnalyzerStats stats = analyzer.GetAnalyzerStats();
  EXPECT_EQ(stats.memory_overloaded_comparisons_done, 0);
  EXPECT_EQ(stats.comparisons_done, 1);

  FrameCounters frame_counters = analyzer.GetGlobalCounters();
  EXPECT_EQ(frame_counters.captured, 1);
  EXPECT_EQ(frame_counters.received, 1);
  EXPECT_EQ(frame_counters.decoded, 1);
  EXPECT_EQ(frame_counters.rendered, 1);
  EXPECT_EQ(frame_counters.dropped, 0);
}

TEST(DefaultVideoQualityAnalyzerTest, HeavyQualityMetricsFromEqualFrames) {
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      test::CreateSquareFrameGenerator(kFrameWidth, kFrameHeight,
                                       /*type=*/absl::nullopt,
                                       /*num_squares=*/absl::nullopt);

  DefaultVideoQualityAnalyzerOptions analyzer_options;
  analyzer_options.heavy_metrics_computation_enabled = true;
  analyzer_options.adjust_cropping_before_comparing_frames = false;
  analyzer_options.max_frames_in_flight_per_stream_count =
      kMaxFramesInFlightPerStream;
  DefaultVideoQualityAnalyzer analyzer(Clock::GetRealTimeClock(),
                                       analyzer_options);
  analyzer.Start("test_case",
                 std::vector<std::string>{kSenderPeerName, kReceiverPeerName},
                 kAnalyzerMaxThreadsCount);

  for (int i = 0; i < kMaxFramesInFlightPerStream; ++i) {
    VideoFrame frame = NextFrame(frame_generator.get(), i);
    frame.set_id(
        analyzer.OnFrameCaptured(kSenderPeerName, kStreamLabel, frame));
    analyzer.OnFramePreEncode(kSenderPeerName, frame);
    analyzer.OnFrameEncoded(kSenderPeerName, frame.id(), FakeEncode(frame),
                            VideoQualityAnalyzerInterface::EncoderStats());

    VideoFrame received_frame = DeepCopy(frame);
    analyzer.OnFramePreDecode(kReceiverPeerName, received_frame.id(),
                              FakeEncode(received_frame));
    analyzer.OnFrameDecoded(kReceiverPeerName, received_frame,
                            VideoQualityAnalyzerInterface::DecoderStats());
    analyzer.OnFrameRendered(kReceiverPeerName, received_frame);
  }

  // Give analyzer some time to process frames on async thread. Heavy metrics
  // computation is turned on, so giving some extra time to be sure that
  // computatio have ended.
  SleepMs(500);
  analyzer.Stop();

  AnalyzerStats stats = analyzer.GetAnalyzerStats();
  EXPECT_EQ(stats.memory_overloaded_comparisons_done, 0);
  EXPECT_EQ(stats.comparisons_done, kMaxFramesInFlightPerStream);

  std::vector<StatsSample> frames_in_flight_sizes =
      GetSortedSamples(stats.frames_in_flight_left_count);
  EXPECT_EQ(frames_in_flight_sizes.back().value, 0)
      << ToString(frames_in_flight_sizes);

  std::map<StatsKey, StreamStats> stream_stats = analyzer.GetStats();
  const StatsKey kAliceBobStats(kStreamLabel, kSenderPeerName,
                                kReceiverPeerName);
  EXPECT_EQ(stream_stats.size(), 1lu);

  auto it = stream_stats.find(kAliceBobStats);
  EXPECT_GE(it->second.psnr.GetMin(), kPerfectPSNR);
  EXPECT_GE(it->second.ssim.GetMin(), kMaxSsim);
}

TEST(DefaultVideoQualityAnalyzerTest,
     HeavyQualityMetricsFromShiftedFramesWithAdjustment) {
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      test::CreateSquareFrameGenerator(kFrameWidth, kFrameHeight,
                                       /*type=*/absl::nullopt,
                                       /*num_squares=*/absl::nullopt);

  DefaultVideoQualityAnalyzerOptions analyzer_options;
  analyzer_options.heavy_metrics_computation_enabled = true;
  analyzer_options.adjust_cropping_before_comparing_frames = true;
  analyzer_options.max_frames_in_flight_per_stream_count =
      kMaxFramesInFlightPerStream;
  DefaultVideoQualityAnalyzer analyzer(Clock::GetRealTimeClock(),
                                       analyzer_options);
  analyzer.Start("test_case",
                 std::vector<std::string>{kSenderPeerName, kReceiverPeerName},
                 kAnalyzerMaxThreadsCount);

  for (int i = 0; i < kMaxFramesInFlightPerStream; ++i) {
    VideoFrame frame = NextFrame(frame_generator.get(), i);
    frame.set_id(
        analyzer.OnFrameCaptured(kSenderPeerName, kStreamLabel, frame));
    analyzer.OnFramePreEncode(kSenderPeerName, frame);
    analyzer.OnFrameEncoded(kSenderPeerName, frame.id(), FakeEncode(frame),
                            VideoQualityAnalyzerInterface::EncoderStats());

    VideoFrame received_frame = frame;
    // Shift frame by a few pixels.
    test::CropRegion crop_region{0, 1, 3, 0};
    rtc::scoped_refptr<VideoFrameBuffer> cropped_buffer =
        CropAndZoom(crop_region, received_frame.video_frame_buffer()->ToI420());
    received_frame.set_video_frame_buffer(cropped_buffer);

    analyzer.OnFramePreDecode(kReceiverPeerName, received_frame.id(),
                              FakeEncode(received_frame));
    analyzer.OnFrameDecoded(kReceiverPeerName, received_frame,
                            VideoQualityAnalyzerInterface::DecoderStats());
    analyzer.OnFrameRendered(kReceiverPeerName, received_frame);
  }

  // Give analyzer some time to process frames on async thread. Heavy metrics
  // computation is turned on, so giving some extra time to be sure that
  // computatio have ended.
  SleepMs(500);
  analyzer.Stop();

  AnalyzerStats stats = analyzer.GetAnalyzerStats();
  EXPECT_EQ(stats.memory_overloaded_comparisons_done, 0);
  EXPECT_EQ(stats.comparisons_done, kMaxFramesInFlightPerStream);

  std::vector<StatsSample> frames_in_flight_sizes =
      GetSortedSamples(stats.frames_in_flight_left_count);
  EXPECT_EQ(frames_in_flight_sizes.back().value, 0)
      << ToString(frames_in_flight_sizes);

  std::map<StatsKey, StreamStats> stream_stats = analyzer.GetStats();
  const StatsKey kAliceBobStats(kStreamLabel, kSenderPeerName,
                                kReceiverPeerName);
  EXPECT_EQ(stream_stats.size(), 1lu);

  auto it = stream_stats.find(kAliceBobStats);
  EXPECT_GE(it->second.psnr.GetMin(), kPerfectPSNR);
  EXPECT_GE(it->second.ssim.GetMin(), kMaxSsim);
}

TEST(DefaultVideoQualityAnalyzerTest, CpuUsage) {
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      test::CreateSquareFrameGenerator(kFrameWidth, kFrameHeight,
                                       /*type=*/absl::nullopt,
                                       /*num_squares=*/absl::nullopt);

  DefaultVideoQualityAnalyzer analyzer(Clock::GetRealTimeClock(),
                                       AnalyzerOptionsForTest());
  analyzer.Start("test_case",
                 std::vector<std::string>{kSenderPeerName, kReceiverPeerName},
                 kAnalyzerMaxThreadsCount);

  std::map<uint16_t, VideoFrame> captured_frames;
  std::vector<uint16_t> frames_order;
  for (int i = 0; i < kMaxFramesInFlightPerStream; ++i) {
    VideoFrame frame = NextFrame(frame_generator.get(), i);
    frame.set_id(
        analyzer.OnFrameCaptured(kSenderPeerName, kStreamLabel, frame));
    frames_order.push_back(frame.id());
    captured_frames.insert({frame.id(), frame});
    analyzer.OnFramePreEncode(kSenderPeerName, frame);
    analyzer.OnFrameEncoded(kSenderPeerName, frame.id(), FakeEncode(frame),
                            VideoQualityAnalyzerInterface::EncoderStats());
  }

  // Windows CPU clock has low accuracy. We need to fake some additional load to
  // be sure that the clock ticks (https://crbug.com/webrtc/12249).
  FakeCPULoad();

  for (size_t i = 1; i < frames_order.size(); i += 2) {
    uint16_t frame_id = frames_order.at(i);
    VideoFrame received_frame = DeepCopy(captured_frames.at(frame_id));
    analyzer.OnFramePreDecode(kReceiverPeerName, received_frame.id(),
                              FakeEncode(received_frame));
    analyzer.OnFrameDecoded(kReceiverPeerName, received_frame,
                            VideoQualityAnalyzerInterface::DecoderStats());
    analyzer.OnFrameRendered(kReceiverPeerName, received_frame);
  }

  // Give analyzer some time to process frames on async thread. The computations
  // have to be fast (heavy metrics are disabled!), so if doesn't fit 100ms it
  // means we have an issue!
  SleepMs(100);
  analyzer.Stop();

  double cpu_usage = analyzer.GetCpuUsagePercent();
  ASSERT_GT(cpu_usage, 0);

  SleepMs(100);
  analyzer.Stop();

  EXPECT_EQ(analyzer.GetCpuUsagePercent(), cpu_usage);
}

TEST(DefaultVideoQualityAnalyzerTest, RuntimeParticipantsAdding) {
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      test::CreateSquareFrameGenerator(kFrameWidth, kFrameHeight,
                                       /*type=*/absl::nullopt,
                                       /*num_squares=*/absl::nullopt);

  constexpr char kAlice[] = "alice";
  constexpr char kBob[] = "bob";
  constexpr char kCharlie[] = "charlie";
  constexpr char kKatie[] = "katie";

  constexpr int kFramesCount = 9;
  constexpr int kOneThirdFrames = kFramesCount / 3;
  constexpr int kTwoThirdFrames = 2 * kOneThirdFrames;

  DefaultVideoQualityAnalyzer analyzer(Clock::GetRealTimeClock(),
                                       AnalyzerOptionsForTest());
  analyzer.Start("test_case", {}, kAnalyzerMaxThreadsCount);

  std::map<uint16_t, VideoFrame> captured_frames;
  std::vector<uint16_t> frames_order;
  analyzer.RegisterParticipantInCall(kAlice);
  analyzer.RegisterParticipantInCall(kBob);

  // Alice is sending frames.
  for (int i = 0; i < kFramesCount; ++i) {
    VideoFrame frame = NextFrame(frame_generator.get(), i);
    frame.set_id(analyzer.OnFrameCaptured(kAlice, kStreamLabel, frame));
    frames_order.push_back(frame.id());
    captured_frames.insert({frame.id(), frame});
    analyzer.OnFramePreEncode(kAlice, frame);
    analyzer.OnFrameEncoded(kAlice, frame.id(), FakeEncode(frame),
                            VideoQualityAnalyzerInterface::EncoderStats());
  }

  // Bob receives one third of the sent frames.
  for (int i = 0; i < kOneThirdFrames; ++i) {
    uint16_t frame_id = frames_order.at(i);
    VideoFrame received_frame = DeepCopy(captured_frames.at(frame_id));
    analyzer.OnFramePreDecode(kBob, received_frame.id(),
                              FakeEncode(received_frame));
    analyzer.OnFrameDecoded(kBob, received_frame,
                            VideoQualityAnalyzerInterface::DecoderStats());
    analyzer.OnFrameRendered(kBob, received_frame);
  }

  analyzer.RegisterParticipantInCall(kCharlie);
  analyzer.RegisterParticipantInCall(kKatie);

  // New participants were dynamically added. Bob and Charlie receive second
  // third of the sent frames. Katie drops the frames.
  for (int i = kOneThirdFrames; i < kTwoThirdFrames; ++i) {
    uint16_t frame_id = frames_order.at(i);
    VideoFrame bob_received_frame = DeepCopy(captured_frames.at(frame_id));
    analyzer.OnFramePreDecode(kBob, bob_received_frame.id(),
                              FakeEncode(bob_received_frame));
    analyzer.OnFrameDecoded(kBob, bob_received_frame,
                            VideoQualityAnalyzerInterface::DecoderStats());
    analyzer.OnFrameRendered(kBob, bob_received_frame);

    VideoFrame charlie_received_frame = DeepCopy(captured_frames.at(frame_id));
    analyzer.OnFramePreDecode(kCharlie, charlie_received_frame.id(),
                              FakeEncode(charlie_received_frame));
    analyzer.OnFrameDecoded(kCharlie, charlie_received_frame,
                            VideoQualityAnalyzerInterface::DecoderStats());
    analyzer.OnFrameRendered(kCharlie, charlie_received_frame);
  }

  // Bob, Charlie and Katie receive the rest of the sent frames.
  for (int i = kTwoThirdFrames; i < kFramesCount; ++i) {
    uint16_t frame_id = frames_order.at(i);
    VideoFrame bob_received_frame = DeepCopy(captured_frames.at(frame_id));
    analyzer.OnFramePreDecode(kBob, bob_received_frame.id(),
                              FakeEncode(bob_received_frame));
    analyzer.OnFrameDecoded(kBob, bob_received_frame,
                            VideoQualityAnalyzerInterface::DecoderStats());
    analyzer.OnFrameRendered(kBob, bob_received_frame);

    VideoFrame charlie_received_frame = DeepCopy(captured_frames.at(frame_id));
    analyzer.OnFramePreDecode(kCharlie, charlie_received_frame.id(),
                              FakeEncode(charlie_received_frame));
    analyzer.OnFrameDecoded(kCharlie, charlie_received_frame,
                            VideoQualityAnalyzerInterface::DecoderStats());
    analyzer.OnFrameRendered(kCharlie, charlie_received_frame);

    VideoFrame katie_received_frame = DeepCopy(captured_frames.at(frame_id));
    analyzer.OnFramePreDecode(kKatie, katie_received_frame.id(),
                              FakeEncode(katie_received_frame));
    analyzer.OnFrameDecoded(kKatie, katie_received_frame,
                            VideoQualityAnalyzerInterface::DecoderStats());
    analyzer.OnFrameRendered(kKatie, katie_received_frame);
  }

  // Give analyzer some time to process frames on async thread. The computations
  // have to be fast (heavy metrics are disabled!), so if doesn't fit 100ms it
  // means we have an issue!
  SleepMs(100);
  analyzer.Stop();

  AnalyzerStats stats = analyzer.GetAnalyzerStats();
  EXPECT_EQ(stats.memory_overloaded_comparisons_done, 0);
  EXPECT_EQ(stats.comparisons_done, kFramesCount + 2 * kTwoThirdFrames);

  std::vector<StatsSample> frames_in_flight_sizes =
      GetSortedSamples(stats.frames_in_flight_left_count);
  EXPECT_EQ(frames_in_flight_sizes.back().value, 0)
      << ToString(frames_in_flight_sizes);

  FrameCounters frame_counters = analyzer.GetGlobalCounters();
  EXPECT_EQ(frame_counters.captured, kFramesCount);
  EXPECT_EQ(frame_counters.received, 2 * kFramesCount);
  EXPECT_EQ(frame_counters.decoded, 2 * kFramesCount);
  EXPECT_EQ(frame_counters.rendered, 2 * kFramesCount);
  EXPECT_EQ(frame_counters.dropped, kOneThirdFrames);

  EXPECT_EQ(analyzer.GetKnownVideoStreams().size(), 3lu);
  const StatsKey kAliceBobStats(kStreamLabel, kAlice, kBob);
  const StatsKey kAliceCharlieStats(kStreamLabel, kAlice, kCharlie);
  const StatsKey kAliceKatieStats(kStreamLabel, kAlice, kKatie);
  {
    FrameCounters stream_conters =
        analyzer.GetPerStreamCounters().at(kAliceBobStats);
    EXPECT_EQ(stream_conters.captured, kFramesCount);
    EXPECT_EQ(stream_conters.pre_encoded, kFramesCount);
    EXPECT_EQ(stream_conters.encoded, kFramesCount);
    EXPECT_EQ(stream_conters.received, kFramesCount);
    EXPECT_EQ(stream_conters.decoded, kFramesCount);
    EXPECT_EQ(stream_conters.rendered, kFramesCount);
  }
  {
    FrameCounters stream_conters =
        analyzer.GetPerStreamCounters().at(kAliceCharlieStats);
    EXPECT_EQ(stream_conters.captured, kTwoThirdFrames);
    EXPECT_EQ(stream_conters.pre_encoded, kTwoThirdFrames);
    EXPECT_EQ(stream_conters.encoded, kTwoThirdFrames);
    EXPECT_EQ(stream_conters.received, kTwoThirdFrames);
    EXPECT_EQ(stream_conters.decoded, kTwoThirdFrames);
    EXPECT_EQ(stream_conters.rendered, kTwoThirdFrames);
  }
  {
    FrameCounters stream_conters =
        analyzer.GetPerStreamCounters().at(kAliceKatieStats);
    EXPECT_EQ(stream_conters.captured, kTwoThirdFrames);
    EXPECT_EQ(stream_conters.pre_encoded, kTwoThirdFrames);
    EXPECT_EQ(stream_conters.encoded, kTwoThirdFrames);
    EXPECT_EQ(stream_conters.received, kOneThirdFrames);
    EXPECT_EQ(stream_conters.decoded, kOneThirdFrames);
    EXPECT_EQ(stream_conters.rendered, kOneThirdFrames);
  }
}

}  // namespace
}  // namespace webrtc_pc_e2e
}  // namespace webrtc
