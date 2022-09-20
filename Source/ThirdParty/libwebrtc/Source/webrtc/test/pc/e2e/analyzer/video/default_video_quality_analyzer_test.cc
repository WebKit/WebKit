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
  options.compute_psnr = false;
  options.compute_ssim = false;
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

void PassFramesThroughAnalyzer(DefaultVideoQualityAnalyzer& analyzer,
                               absl::string_view sender,
                               absl::string_view stream_label,
                               std::vector<absl::string_view> receivers,
                               int frames_count,
                               test::FrameGeneratorInterface& frame_generator) {
  for (int i = 0; i < frames_count; ++i) {
    VideoFrame frame = NextFrame(&frame_generator, /*timestamp_us=*/1);
    uint16_t frame_id =
        analyzer.OnFrameCaptured(sender, std::string(stream_label), frame);
    frame.set_id(frame_id);
    analyzer.OnFramePreEncode(sender, frame);
    analyzer.OnFrameEncoded(sender, frame.id(), FakeEncode(frame),
                            VideoQualityAnalyzerInterface::EncoderStats());
    for (absl::string_view receiver : receivers) {
      VideoFrame received_frame = DeepCopy(frame);
      analyzer.OnFramePreDecode(receiver, received_frame.id(),
                                FakeEncode(received_frame));
      analyzer.OnFrameDecoded(receiver, received_frame,
                              VideoQualityAnalyzerInterface::DecoderStats());
      analyzer.OnFrameRendered(receiver, received_frame);
    }
  }
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

  VideoStreamsInfo streams_info = analyzer.GetKnownStreams();
  EXPECT_EQ(streams_info.GetStreams(), std::set<std::string>{kStreamLabel});
  EXPECT_EQ(streams_info.GetStreams(kAlice),
            std::set<std::string>{kStreamLabel});
  EXPECT_EQ(streams_info.GetSender(kStreamLabel), kAlice);
  EXPECT_EQ(streams_info.GetReceivers(kStreamLabel),
            (std::set<std::string>{kBob, kCharlie}));

  EXPECT_EQ(streams_info.GetStatsKeys().size(), 2lu);
  for (auto stream_key : streams_info.GetStatsKeys()) {
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
  const StatsKey kAliceBobStats(kStreamLabel, kBob);
  const StatsKey kAliceCharlieStats(kStreamLabel, kCharlie);
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
  analyzer_options.compute_psnr = true;
  analyzer_options.compute_ssim = true;
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
  const StatsKey kAliceBobStats(kStreamLabel, kReceiverPeerName);
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
  analyzer_options.compute_psnr = true;
  analyzer_options.compute_ssim = true;
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
  const StatsKey kAliceBobStats(kStreamLabel, kReceiverPeerName);
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

  const StatsKey kAliceBobStats(kStreamLabel, kBob);
  const StatsKey kAliceCharlieStats(kStreamLabel, kCharlie);
  const StatsKey kAliceKatieStats(kStreamLabel, kKatie);
  EXPECT_EQ(analyzer.GetKnownStreams().GetStatsKeys(),
            (std::set<StatsKey>{kAliceBobStats, kAliceCharlieStats,
                                kAliceKatieStats}));
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

<<<<<<< HEAD
TEST(DefaultVideoQualityAnalyzerTest,
     SimulcastFrameWasFullyReceivedByAllPeersBeforeEncodeFinish) {
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      test::CreateSquareFrameGenerator(kFrameWidth, kFrameHeight,
                                       /*type=*/absl::nullopt,
                                       /*num_squares=*/absl::nullopt);

  DefaultVideoQualityAnalyzer analyzer(Clock::GetRealTimeClock(),
                                       AnalyzerOptionsForTest());
  constexpr char kAlice[] = "alice";
  constexpr char kBob[] = "bob";
  constexpr char kCharlie[] = "charlie";
  analyzer.Start("test_case", std::vector<std::string>{kAlice, kBob, kCharlie},
                 kAnalyzerMaxThreadsCount);

  VideoFrame frame = NextFrame(frame_generator.get(), 1);

  frame.set_id(analyzer.OnFrameCaptured(kAlice, kStreamLabel, frame));
  analyzer.OnFramePreEncode(kAlice, frame);
  // Encode 1st simulcast layer
  analyzer.OnFrameEncoded(kAlice, frame.id(), FakeEncode(frame),
                          VideoQualityAnalyzerInterface::EncoderStats());

  // Receive by Bob
  VideoFrame received_frame = DeepCopy(frame);
  analyzer.OnFramePreDecode(kBob, received_frame.id(),
                            FakeEncode(received_frame));
  analyzer.OnFrameDecoded(kBob, received_frame,
                          VideoQualityAnalyzerInterface::DecoderStats());
  analyzer.OnFrameRendered(kBob, received_frame);
  // Receive by Charlie
  received_frame = DeepCopy(frame);
  analyzer.OnFramePreDecode(kCharlie, received_frame.id(),
                            FakeEncode(received_frame));
  analyzer.OnFrameDecoded(kCharlie, received_frame,
                          VideoQualityAnalyzerInterface::DecoderStats());
  analyzer.OnFrameRendered(kCharlie, received_frame);

  // Encode 2nd simulcast layer
  analyzer.OnFrameEncoded(kAlice, frame.id(), FakeEncode(frame),
                          VideoQualityAnalyzerInterface::EncoderStats());

  // Give analyzer some time to process frames on async thread. The computations
  // have to be fast (heavy metrics are disabled!), so if doesn't fit 100ms it
  // means we have an issue!
  SleepMs(100);
  analyzer.Stop();

  AnalyzerStats stats = analyzer.GetAnalyzerStats();
  EXPECT_EQ(stats.comparisons_done, 2);

  std::vector<StatsSample> frames_in_flight_sizes =
      GetSortedSamples(stats.frames_in_flight_left_count);
  EXPECT_EQ(frames_in_flight_sizes.back().value, 0)
      << ToString(frames_in_flight_sizes);

  FrameCounters frame_counters = analyzer.GetGlobalCounters();
  EXPECT_EQ(frame_counters.captured, 1);
  EXPECT_EQ(frame_counters.rendered, 2);
}

TEST(DefaultVideoQualityAnalyzerTest,
     FrameCanBeReceivedBySenderAfterItWasReceivedByReceiver) {
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      test::CreateSquareFrameGenerator(kFrameWidth, kFrameHeight,
                                       /*type=*/absl::nullopt,
                                       /*num_squares=*/absl::nullopt);

  DefaultVideoQualityAnalyzerOptions options = AnalyzerOptionsForTest();
  options.enable_receive_own_stream = true;
  DefaultVideoQualityAnalyzer analyzer(Clock::GetRealTimeClock(), options);
  analyzer.Start("test_case",
                 std::vector<std::string>{kSenderPeerName, kReceiverPeerName},
                 kAnalyzerMaxThreadsCount);

  std::vector<VideoFrame> frames;
  for (int i = 0; i < 3; ++i) {
    VideoFrame frame = NextFrame(frame_generator.get(), 1);
    frame.set_id(
        analyzer.OnFrameCaptured(kSenderPeerName, kStreamLabel, frame));
    frames.push_back(frame);
    analyzer.OnFramePreEncode(kSenderPeerName, frame);
    analyzer.OnFrameEncoded(kSenderPeerName, frame.id(), FakeEncode(frame),
                            VideoQualityAnalyzerInterface::EncoderStats());
  }

  // Receive by 2nd peer.
  for (VideoFrame& frame : frames) {
    VideoFrame received_frame = DeepCopy(frame);
    analyzer.OnFramePreDecode(kReceiverPeerName, received_frame.id(),
                              FakeEncode(received_frame));
    analyzer.OnFrameDecoded(kReceiverPeerName, received_frame,
                            VideoQualityAnalyzerInterface::DecoderStats());
    analyzer.OnFrameRendered(kReceiverPeerName, received_frame);
  }

  // Check that we still have that frame in flight.
  AnalyzerStats analyzer_stats = analyzer.GetAnalyzerStats();
  std::vector<StatsSample> frames_in_flight_sizes =
      GetSortedSamples(analyzer_stats.frames_in_flight_left_count);
  EXPECT_EQ(frames_in_flight_sizes.back().value, 3)
      << "Expected that frame is still in flight, "
      << "because it wasn't received by sender"
      << ToString(frames_in_flight_sizes);

  // Receive by sender
  for (VideoFrame& frame : frames) {
    VideoFrame received_frame = DeepCopy(frame);
    analyzer.OnFramePreDecode(kSenderPeerName, received_frame.id(),
                              FakeEncode(received_frame));
    analyzer.OnFrameDecoded(kSenderPeerName, received_frame,
                            VideoQualityAnalyzerInterface::DecoderStats());
    analyzer.OnFrameRendered(kSenderPeerName, received_frame);
  }

  // Give analyzer some time to process frames on async thread. The computations
  // have to be fast (heavy metrics are disabled!), so if doesn't fit 100ms it
  // means we have an issue!
  SleepMs(100);
  analyzer.Stop();

  analyzer_stats = analyzer.GetAnalyzerStats();
  EXPECT_EQ(analyzer_stats.comparisons_done, 6);

  frames_in_flight_sizes =
      GetSortedSamples(analyzer_stats.frames_in_flight_left_count);
  EXPECT_EQ(frames_in_flight_sizes.back().value, 0)
      << ToString(frames_in_flight_sizes);

  FrameCounters frame_counters = analyzer.GetGlobalCounters();
  EXPECT_EQ(frame_counters.captured, 3);
  EXPECT_EQ(frame_counters.rendered, 6);

  EXPECT_EQ(analyzer.GetStats().size(), 2lu);
  {
    FrameCounters stream_conters = analyzer.GetPerStreamCounters().at(
        StatsKey(kStreamLabel, kReceiverPeerName));
    EXPECT_EQ(stream_conters.captured, 3);
    EXPECT_EQ(stream_conters.pre_encoded, 3);
    EXPECT_EQ(stream_conters.encoded, 3);
    EXPECT_EQ(stream_conters.received, 3);
    EXPECT_EQ(stream_conters.decoded, 3);
    EXPECT_EQ(stream_conters.rendered, 3);
  }
  {
    FrameCounters stream_conters = analyzer.GetPerStreamCounters().at(
        StatsKey(kStreamLabel, kSenderPeerName));
    EXPECT_EQ(stream_conters.captured, 3);
    EXPECT_EQ(stream_conters.pre_encoded, 3);
    EXPECT_EQ(stream_conters.encoded, 3);
    EXPECT_EQ(stream_conters.received, 3);
    EXPECT_EQ(stream_conters.decoded, 3);
    EXPECT_EQ(stream_conters.rendered, 3);
  }
}

TEST(DefaultVideoQualityAnalyzerTest,
     FrameCanBeReceivedByReceiverAfterItWasReceivedBySender) {
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      test::CreateSquareFrameGenerator(kFrameWidth, kFrameHeight,
                                       /*type=*/absl::nullopt,
                                       /*num_squares=*/absl::nullopt);

  DefaultVideoQualityAnalyzerOptions options = AnalyzerOptionsForTest();
  options.enable_receive_own_stream = true;
  DefaultVideoQualityAnalyzer analyzer(Clock::GetRealTimeClock(), options);
  analyzer.Start("test_case",
                 std::vector<std::string>{kSenderPeerName, kReceiverPeerName},
                 kAnalyzerMaxThreadsCount);

  std::vector<VideoFrame> frames;
  for (int i = 0; i < 3; ++i) {
    VideoFrame frame = NextFrame(frame_generator.get(), 1);
    frame.set_id(
        analyzer.OnFrameCaptured(kSenderPeerName, kStreamLabel, frame));
    frames.push_back(frame);
    analyzer.OnFramePreEncode(kSenderPeerName, frame);
    analyzer.OnFrameEncoded(kSenderPeerName, frame.id(), FakeEncode(frame),
                            VideoQualityAnalyzerInterface::EncoderStats());
  }

  // Receive by sender
  for (VideoFrame& frame : frames) {
    VideoFrame received_frame = DeepCopy(frame);
    analyzer.OnFramePreDecode(kSenderPeerName, received_frame.id(),
                              FakeEncode(received_frame));
    analyzer.OnFrameDecoded(kSenderPeerName, received_frame,
                            VideoQualityAnalyzerInterface::DecoderStats());
    analyzer.OnFrameRendered(kSenderPeerName, received_frame);
  }

  // Check that we still have that frame in flight.
  AnalyzerStats analyzer_stats = analyzer.GetAnalyzerStats();
  std::vector<StatsSample> frames_in_flight_sizes =
      GetSortedSamples(analyzer_stats.frames_in_flight_left_count);
  EXPECT_EQ(frames_in_flight_sizes.back().value, 3)
      << "Expected that frame is still in flight, "
      << "because it wasn't received by sender"
      << ToString(frames_in_flight_sizes);

  // Receive by 2nd peer.
  for (VideoFrame& frame : frames) {
    VideoFrame received_frame = DeepCopy(frame);
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

  analyzer_stats = analyzer.GetAnalyzerStats();
  EXPECT_EQ(analyzer_stats.comparisons_done, 6);

  frames_in_flight_sizes =
      GetSortedSamples(analyzer_stats.frames_in_flight_left_count);
  EXPECT_EQ(frames_in_flight_sizes.back().value, 0)
      << ToString(frames_in_flight_sizes);

  FrameCounters frame_counters = analyzer.GetGlobalCounters();
  EXPECT_EQ(frame_counters.captured, 3);
  EXPECT_EQ(frame_counters.rendered, 6);

  EXPECT_EQ(analyzer.GetStats().size(), 2lu);
  {
    FrameCounters stream_conters = analyzer.GetPerStreamCounters().at(
        StatsKey(kStreamLabel, kReceiverPeerName));
    EXPECT_EQ(stream_conters.captured, 3);
    EXPECT_EQ(stream_conters.pre_encoded, 3);
    EXPECT_EQ(stream_conters.encoded, 3);
    EXPECT_EQ(stream_conters.received, 3);
    EXPECT_EQ(stream_conters.decoded, 3);
    EXPECT_EQ(stream_conters.rendered, 3);
  }
  {
    FrameCounters stream_conters = analyzer.GetPerStreamCounters().at(
        StatsKey(kStreamLabel, kSenderPeerName));
    EXPECT_EQ(stream_conters.captured, 3);
    EXPECT_EQ(stream_conters.pre_encoded, 3);
    EXPECT_EQ(stream_conters.encoded, 3);
    EXPECT_EQ(stream_conters.received, 3);
    EXPECT_EQ(stream_conters.decoded, 3);
    EXPECT_EQ(stream_conters.rendered, 3);
  }
}

TEST(DefaultVideoQualityAnalyzerTest, CodecTrackedCorrectly) {
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      test::CreateSquareFrameGenerator(kFrameWidth, kFrameHeight,
                                       /*type=*/absl::nullopt,
                                       /*num_squares=*/absl::nullopt);

  DefaultVideoQualityAnalyzer analyzer(Clock::GetRealTimeClock(),
                                       AnalyzerOptionsForTest());
  analyzer.Start("test_case",
                 std::vector<std::string>{kSenderPeerName, kReceiverPeerName},
                 kAnalyzerMaxThreadsCount);

  VideoQualityAnalyzerInterface::EncoderStats encoder_stats;
  std::vector<std::string> codec_names = {"codec_1", "codec_2"};
  std::vector<VideoFrame> frames;
  // Send 3 frame for each codec.
  for (size_t i = 0; i < codec_names.size(); ++i) {
    for (size_t j = 0; j < 3; ++j) {
      VideoFrame frame = NextFrame(frame_generator.get(), 3 * i + j);
      frame.set_id(
          analyzer.OnFrameCaptured(kSenderPeerName, kStreamLabel, frame));
      analyzer.OnFramePreEncode(kSenderPeerName, frame);
      encoder_stats.encoder_name = codec_names[i];
      analyzer.OnFrameEncoded(kSenderPeerName, frame.id(), FakeEncode(frame),
                              encoder_stats);
      frames.push_back(std::move(frame));
    }
  }

  // Receive 3 frame for each codec.
  VideoQualityAnalyzerInterface::DecoderStats decoder_stats;
  for (size_t i = 0; i < codec_names.size(); ++i) {
    for (size_t j = 0; j < 3; ++j) {
      VideoFrame received_frame = DeepCopy(frames[3 * i + j]);
      analyzer.OnFramePreDecode(kReceiverPeerName, received_frame.id(),
                                FakeEncode(received_frame));
      decoder_stats.decoder_name = codec_names[i];
      analyzer.OnFrameDecoded(kReceiverPeerName, received_frame, decoder_stats);
      analyzer.OnFrameRendered(kReceiverPeerName, received_frame);
    }
  }

  // Give analyzer some time to process frames on async thread. The computations
  // have to be fast (heavy metrics are disabled!), so if doesn't fit 100ms it
  // means we have an issue!
  SleepMs(100);
  analyzer.Stop();

  std::map<StatsKey, StreamStats> stats = analyzer.GetStats();
  ASSERT_EQ(stats.size(), 1lu);
  const StreamStats& stream_stats =
      stats.at(StatsKey(kStreamLabel, kReceiverPeerName));
  ASSERT_EQ(stream_stats.encoders.size(), 2lu);
  EXPECT_EQ(stream_stats.encoders[0].codec_name, codec_names[0]);
  EXPECT_EQ(stream_stats.encoders[0].first_frame_id, frames[0].id());
  EXPECT_EQ(stream_stats.encoders[0].last_frame_id, frames[2].id());
  EXPECT_EQ(stream_stats.encoders[1].codec_name, codec_names[1]);
  EXPECT_EQ(stream_stats.encoders[1].first_frame_id, frames[3].id());
  EXPECT_EQ(stream_stats.encoders[1].last_frame_id, frames[5].id());

  ASSERT_EQ(stream_stats.decoders.size(), 2lu);
  EXPECT_EQ(stream_stats.decoders[0].codec_name, codec_names[0]);
  EXPECT_EQ(stream_stats.decoders[0].first_frame_id, frames[0].id());
  EXPECT_EQ(stream_stats.decoders[0].last_frame_id, frames[2].id());
  EXPECT_EQ(stream_stats.decoders[1].codec_name, codec_names[1]);
  EXPECT_EQ(stream_stats.decoders[1].first_frame_id, frames[3].id());
  EXPECT_EQ(stream_stats.decoders[1].last_frame_id, frames[5].id());
}

TEST(DefaultVideoQualityAnalyzerTest,
     FramesInFlightAreCorrectlySentToTheComparatorAfterStop) {
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      test::CreateSquareFrameGenerator(kFrameWidth, kFrameHeight,
                                       /*type=*/absl::nullopt,
                                       /*num_squares=*/absl::nullopt);

  DefaultVideoQualityAnalyzerOptions options = AnalyzerOptionsForTest();
  DefaultVideoQualityAnalyzer analyzer(Clock::GetRealTimeClock(), options);
  analyzer.Start("test_case",
                 std::vector<std::string>{kSenderPeerName, kReceiverPeerName},
                 kAnalyzerMaxThreadsCount);

  // There are 7 different timings inside frame stats: captured, pre_encode,
  // encoded, received, decode_start, decode_end, rendered. captured is always
  // set and received is set together with decode_start. So we create 6
  // different frames, where for each frame next timings will be set
  //   * 1st - all of them set
  //   * 2nd - captured, pre_encode, encoded, received, decode_start, decode_end
  //   * 3rd - captured, pre_encode, encoded, received, decode_start
  //   * 4th - captured, pre_encode, encoded
  //   * 5th - captured, pre_encode
  //   * 6th - captured
  std::vector<VideoFrame> frames;
  // Sender side actions
  for (int i = 0; i < 6; ++i) {
    VideoFrame frame = NextFrame(frame_generator.get(), 1);
    frame.set_id(
        analyzer.OnFrameCaptured(kSenderPeerName, kStreamLabel, frame));
    frames.push_back(frame);
  }
  for (int i = 0; i < 5; ++i) {
    analyzer.OnFramePreEncode(kSenderPeerName, frames[i]);
  }
  for (int i = 0; i < 4; ++i) {
    analyzer.OnFrameEncoded(kSenderPeerName, frames[i].id(),
                            FakeEncode(frames[i]),
                            VideoQualityAnalyzerInterface::EncoderStats());
  }

  // Receiver side actions
  for (int i = 0; i < 3; ++i) {
    analyzer.OnFramePreDecode(kReceiverPeerName, frames[i].id(),
                              FakeEncode(frames[i]));
  }
  for (int i = 0; i < 2; ++i) {
    analyzer.OnFrameDecoded(kReceiverPeerName, DeepCopy(frames[i]),
                            VideoQualityAnalyzerInterface::DecoderStats());
  }
  for (int i = 0; i < 1; ++i) {
    analyzer.OnFrameRendered(kReceiverPeerName, DeepCopy(frames[i]));
  }

  // Give analyzer some time to process frames on async thread. The computations
  // have to be fast (heavy metrics are disabled!), so if doesn't fit 100ms it
  // means we have an issue!
  SleepMs(100);
  analyzer.Stop();

  AnalyzerStats analyzer_stats = analyzer.GetAnalyzerStats();
  EXPECT_EQ(analyzer_stats.comparisons_done, 6);

  // The last frames in flight size has to reflect the amount of frame in flight
  // before all of them were sent to the comparison when Stop() was invoked.
  std::vector<StatsSample> frames_in_flight_sizes =
      GetSortedSamples(analyzer_stats.frames_in_flight_left_count);
  EXPECT_EQ(frames_in_flight_sizes.back().value, 5)
      << ToString(frames_in_flight_sizes);

  FrameCounters frame_counters = analyzer.GetGlobalCounters();
  EXPECT_EQ(frame_counters.captured, 6);
  EXPECT_EQ(frame_counters.pre_encoded, 5);
  EXPECT_EQ(frame_counters.encoded, 4);
  EXPECT_EQ(frame_counters.received, 3);
  EXPECT_EQ(frame_counters.decoded, 2);
  EXPECT_EQ(frame_counters.rendered, 1);

  EXPECT_EQ(analyzer.GetStats().size(), 1lu);
  {
    FrameCounters stream_conters = analyzer.GetPerStreamCounters().at(
        StatsKey(kStreamLabel, kReceiverPeerName));
    EXPECT_EQ(stream_conters.captured, 6);
    EXPECT_EQ(stream_conters.pre_encoded, 5);
    EXPECT_EQ(stream_conters.encoded, 4);
    EXPECT_EQ(stream_conters.received, 3);
    EXPECT_EQ(stream_conters.decoded, 2);
    EXPECT_EQ(stream_conters.rendered, 1);
  }
}

TEST(
    DefaultVideoQualityAnalyzerTest,
    FramesInFlightAreCorrectlySentToTheComparatorAfterStopForSenderAndReceiver) {
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      test::CreateSquareFrameGenerator(kFrameWidth, kFrameHeight,
                                       /*type=*/absl::nullopt,
                                       /*num_squares=*/absl::nullopt);

  DefaultVideoQualityAnalyzerOptions options = AnalyzerOptionsForTest();
  options.enable_receive_own_stream = true;
  DefaultVideoQualityAnalyzer analyzer(Clock::GetRealTimeClock(), options);
  analyzer.Start("test_case",
                 std::vector<std::string>{kSenderPeerName, kReceiverPeerName},
                 kAnalyzerMaxThreadsCount);

  // There are 7 different timings inside frame stats: captured, pre_encode,
  // encoded, received, decode_start, decode_end, rendered. captured is always
  // set and received is set together with decode_start. So we create 6
  // different frames, where for each frame next timings will be set
  //   * 1st - all of them set
  //   * 2nd - captured, pre_encode, encoded, received, decode_start, decode_end
  //   * 3rd - captured, pre_encode, encoded, received, decode_start
  //   * 4th - captured, pre_encode, encoded
  //   * 5th - captured, pre_encode
  //   * 6th - captured
  std::vector<VideoFrame> frames;
  // Sender side actions
  for (int i = 0; i < 6; ++i) {
    VideoFrame frame = NextFrame(frame_generator.get(), 1);
    frame.set_id(
        analyzer.OnFrameCaptured(kSenderPeerName, kStreamLabel, frame));
    frames.push_back(frame);
  }
  for (int i = 0; i < 5; ++i) {
    analyzer.OnFramePreEncode(kSenderPeerName, frames[i]);
  }
  for (int i = 0; i < 4; ++i) {
    analyzer.OnFrameEncoded(kSenderPeerName, frames[i].id(),
                            FakeEncode(frames[i]),
                            VideoQualityAnalyzerInterface::EncoderStats());
  }

  // Receiver side actions
  for (int i = 0; i < 3; ++i) {
    analyzer.OnFramePreDecode(kSenderPeerName, frames[i].id(),
                              FakeEncode(frames[i]));
    analyzer.OnFramePreDecode(kReceiverPeerName, frames[i].id(),
                              FakeEncode(frames[i]));
  }
  for (int i = 0; i < 2; ++i) {
    analyzer.OnFrameDecoded(kSenderPeerName, DeepCopy(frames[i]),
                            VideoQualityAnalyzerInterface::DecoderStats());
    analyzer.OnFrameDecoded(kReceiverPeerName, DeepCopy(frames[i]),
                            VideoQualityAnalyzerInterface::DecoderStats());
  }
  for (int i = 0; i < 1; ++i) {
    analyzer.OnFrameRendered(kSenderPeerName, DeepCopy(frames[i]));
    analyzer.OnFrameRendered(kReceiverPeerName, DeepCopy(frames[i]));
  }

  // Give analyzer some time to process frames on async thread. The computations
  // have to be fast (heavy metrics are disabled!), so if doesn't fit 100ms it
  // means we have an issue!
  SleepMs(100);
  analyzer.Stop();

  AnalyzerStats analyzer_stats = analyzer.GetAnalyzerStats();
  EXPECT_EQ(analyzer_stats.comparisons_done, 12);

  // The last frames in flight size has to reflect the amount of frame in flight
  // before all of them were sent to the comparison when Stop() was invoked.
  std::vector<StatsSample> frames_in_flight_sizes =
      GetSortedSamples(analyzer_stats.frames_in_flight_left_count);
  EXPECT_EQ(frames_in_flight_sizes.back().value, 5)
      << ToString(frames_in_flight_sizes);

  FrameCounters frame_counters = analyzer.GetGlobalCounters();
  EXPECT_EQ(frame_counters.captured, 6);
  EXPECT_EQ(frame_counters.pre_encoded, 5);
  EXPECT_EQ(frame_counters.encoded, 4);
  EXPECT_EQ(frame_counters.received, 6);
  EXPECT_EQ(frame_counters.decoded, 4);
  EXPECT_EQ(frame_counters.rendered, 2);

  EXPECT_EQ(analyzer.GetStats().size(), 2lu);
  {
    FrameCounters stream_conters = analyzer.GetPerStreamCounters().at(
        StatsKey(kStreamLabel, kReceiverPeerName));
    EXPECT_EQ(stream_conters.captured, 6);
    EXPECT_EQ(stream_conters.pre_encoded, 5);
    EXPECT_EQ(stream_conters.encoded, 4);
    EXPECT_EQ(stream_conters.received, 3);
    EXPECT_EQ(stream_conters.decoded, 2);
    EXPECT_EQ(stream_conters.rendered, 1);
  }
  {
    FrameCounters stream_conters = analyzer.GetPerStreamCounters().at(
        StatsKey(kStreamLabel, kSenderPeerName));
    EXPECT_EQ(stream_conters.captured, 6);
    EXPECT_EQ(stream_conters.pre_encoded, 5);
    EXPECT_EQ(stream_conters.encoded, 4);
    EXPECT_EQ(stream_conters.received, 3);
    EXPECT_EQ(stream_conters.decoded, 2);
    EXPECT_EQ(stream_conters.rendered, 1);
  }
}

TEST(DefaultVideoQualityAnalyzerTest, GetStreamFrames) {
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      test::CreateSquareFrameGenerator(kFrameWidth, kFrameHeight,
                                       /*type=*/absl::nullopt,
                                       /*num_squares=*/absl::nullopt);

  DefaultVideoQualityAnalyzerOptions options = AnalyzerOptionsForTest();
  DefaultVideoQualityAnalyzer analyzer(Clock::GetRealTimeClock(), options);
  analyzer.Start("test_case", std::vector<std::string>{"alice", "bob"},
                 kAnalyzerMaxThreadsCount);

  // The order in which peers captured frames and passed them to analyzer.
  std::vector<std::string> frame_capturers_sequence{
      "alice", "alice", "bob",   "bob",   "bob",
      "bob",   "bob",   "alice", "alice", "alice",
  };

  std::map<std::string, std::vector<uint16_t>> stream_to_frame_ids;
  stream_to_frame_ids.emplace("alice_video", std::vector<uint16_t>{});
  stream_to_frame_ids.emplace("bob_video", std::vector<uint16_t>{});

  std::vector<VideoFrame> frames;
  for (const std::string& sender : frame_capturers_sequence) {
    VideoFrame frame = NextFrame(frame_generator.get(), /*timestamp_us=*/1);
    uint16_t frame_id =
        analyzer.OnFrameCaptured(sender, sender + "_video", frame);
    frame.set_id(frame_id);
    stream_to_frame_ids.find(sender + "_video")->second.push_back(frame_id);
    frames.push_back(frame);
    analyzer.OnFramePreEncode(sender, frame);
    analyzer.OnFrameEncoded(sender, frame.id(), FakeEncode(frame),
                            VideoQualityAnalyzerInterface::EncoderStats());
  }
  // We don't need to receive frames for stats to be gathered correctly.

  // Give analyzer some time to process frames on async thread. The computations
  // have to be fast (heavy metrics are disabled!), so if doesn't fit 100ms it
  // means we have an issue!
  SleepMs(100);
  analyzer.Stop();

  EXPECT_EQ(analyzer.GetStreamFrames(), stream_to_frame_ids);
}

TEST(DefaultVideoQualityAnalyzerTest, ReceiverReceivedFramesWhenSenderRemoved) {
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      test::CreateSquareFrameGenerator(kFrameWidth, kFrameHeight,
                                       /*type=*/absl::nullopt,
                                       /*num_squares=*/absl::nullopt);

  DefaultVideoQualityAnalyzerOptions options = AnalyzerOptionsForTest();
  DefaultVideoQualityAnalyzer analyzer(Clock::GetRealTimeClock(), options);
  analyzer.Start("test_case", std::vector<std::string>{"alice", "bob"},
                 kAnalyzerMaxThreadsCount);

  VideoFrame frame = NextFrame(frame_generator.get(), /*timestamp_us=*/1);
  uint16_t frame_id = analyzer.OnFrameCaptured("alice", "alice_video", frame);
  frame.set_id(frame_id);
  analyzer.OnFramePreEncode("alice", frame);
  analyzer.OnFrameEncoded("alice", frame.id(), FakeEncode(frame),
                          VideoQualityAnalyzerInterface::EncoderStats());

  analyzer.UnregisterParticipantInCall("alice");

  analyzer.OnFramePreDecode("bob", frame.id(), FakeEncode(frame));
  analyzer.OnFrameDecoded("bob", DeepCopy(frame),
                          VideoQualityAnalyzerInterface::DecoderStats());
  analyzer.OnFrameRendered("bob", DeepCopy(frame));

  // Give analyzer some time to process frames on async thread. The computations
  // have to be fast (heavy metrics are disabled!), so if doesn't fit 100ms it
  // means we have an issue!
  SleepMs(100);
  analyzer.Stop();

  FrameCounters stream_conters =
      analyzer.GetPerStreamCounters().at(StatsKey("alice_video", "bob"));
  EXPECT_EQ(stream_conters.captured, 1);
  EXPECT_EQ(stream_conters.pre_encoded, 1);
  EXPECT_EQ(stream_conters.encoded, 1);
  EXPECT_EQ(stream_conters.received, 1);
  EXPECT_EQ(stream_conters.decoded, 1);
  EXPECT_EQ(stream_conters.rendered, 1);
}

TEST(DefaultVideoQualityAnalyzerTest,
     ReceiverReceivedFramesWhenSenderRemovedWithSelfview) {
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      test::CreateSquareFrameGenerator(kFrameWidth, kFrameHeight,
                                       /*type=*/absl::nullopt,
                                       /*num_squares=*/absl::nullopt);

  DefaultVideoQualityAnalyzerOptions options = AnalyzerOptionsForTest();
  options.enable_receive_own_stream = true;
  DefaultVideoQualityAnalyzer analyzer(Clock::GetRealTimeClock(), options);
  analyzer.Start("test_case", std::vector<std::string>{"alice", "bob"},
                 kAnalyzerMaxThreadsCount);

  VideoFrame frame = NextFrame(frame_generator.get(), /*timestamp_us=*/1);
  uint16_t frame_id = analyzer.OnFrameCaptured("alice", "alice_video", frame);
  frame.set_id(frame_id);
  analyzer.OnFramePreEncode("alice", frame);
  analyzer.OnFrameEncoded("alice", frame.id(), FakeEncode(frame),
                          VideoQualityAnalyzerInterface::EncoderStats());

  analyzer.UnregisterParticipantInCall("alice");

  analyzer.OnFramePreDecode("bob", frame.id(), FakeEncode(frame));
  analyzer.OnFrameDecoded("bob", DeepCopy(frame),
                          VideoQualityAnalyzerInterface::DecoderStats());
  analyzer.OnFrameRendered("bob", DeepCopy(frame));

  // Give analyzer some time to process frames on async thread. The computations
  // have to be fast (heavy metrics are disabled!), so if doesn't fit 100ms it
  // means we have an issue!
  SleepMs(100);
  analyzer.Stop();

  FrameCounters stream_conters =
      analyzer.GetPerStreamCounters().at(StatsKey("alice_video", "bob"));
  EXPECT_EQ(stream_conters.captured, 1);
  EXPECT_EQ(stream_conters.pre_encoded, 1);
  EXPECT_EQ(stream_conters.encoded, 1);
  EXPECT_EQ(stream_conters.received, 1);
  EXPECT_EQ(stream_conters.decoded, 1);
  EXPECT_EQ(stream_conters.rendered, 1);
}

TEST(DefaultVideoQualityAnalyzerTest,
     SenderReceivedFramesWhenReceiverRemovedWithSelfview) {
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      test::CreateSquareFrameGenerator(kFrameWidth, kFrameHeight,
                                       /*type=*/absl::nullopt,
                                       /*num_squares=*/absl::nullopt);

  DefaultVideoQualityAnalyzerOptions options = AnalyzerOptionsForTest();
  options.enable_receive_own_stream = true;
  DefaultVideoQualityAnalyzer analyzer(Clock::GetRealTimeClock(), options);
  analyzer.Start("test_case", std::vector<std::string>{"alice", "bob"},
                 kAnalyzerMaxThreadsCount);

  VideoFrame frame = NextFrame(frame_generator.get(), /*timestamp_us=*/1);
  uint16_t frame_id = analyzer.OnFrameCaptured("alice", "alice_video", frame);
  frame.set_id(frame_id);
  analyzer.OnFramePreEncode("alice", frame);
  analyzer.OnFrameEncoded("alice", frame.id(), FakeEncode(frame),
                          VideoQualityAnalyzerInterface::EncoderStats());

  analyzer.UnregisterParticipantInCall("bob");

  analyzer.OnFramePreDecode("alice", frame.id(), FakeEncode(frame));
  analyzer.OnFrameDecoded("alice", DeepCopy(frame),
                          VideoQualityAnalyzerInterface::DecoderStats());
  analyzer.OnFrameRendered("alice", DeepCopy(frame));

  // Give analyzer some time to process frames on async thread. The computations
  // have to be fast (heavy metrics are disabled!), so if doesn't fit 100ms it
  // means we have an issue!
  SleepMs(100);
  analyzer.Stop();

  FrameCounters stream_conters =
      analyzer.GetPerStreamCounters().at(StatsKey("alice_video", "alice"));
  EXPECT_EQ(stream_conters.captured, 1);
  EXPECT_EQ(stream_conters.pre_encoded, 1);
  EXPECT_EQ(stream_conters.encoded, 1);
  EXPECT_EQ(stream_conters.received, 1);
  EXPECT_EQ(stream_conters.decoded, 1);
  EXPECT_EQ(stream_conters.rendered, 1);
}

TEST(DefaultVideoQualityAnalyzerTest,
     SenderAndReceiverReceivedFramesWhenReceiverRemovedWithSelfview) {
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      test::CreateSquareFrameGenerator(kFrameWidth, kFrameHeight,
                                       /*type=*/absl::nullopt,
                                       /*num_squares=*/absl::nullopt);

  DefaultVideoQualityAnalyzerOptions options = AnalyzerOptionsForTest();
  options.enable_receive_own_stream = true;
  DefaultVideoQualityAnalyzer analyzer(Clock::GetRealTimeClock(), options);
  analyzer.Start("test_case", std::vector<std::string>{"alice", "bob"},
                 kAnalyzerMaxThreadsCount);

  VideoFrame frame = NextFrame(frame_generator.get(), /*timestamp_us=*/1);
  uint16_t frame_id = analyzer.OnFrameCaptured("alice", "alice_video", frame);
  frame.set_id(frame_id);
  analyzer.OnFramePreEncode("alice", frame);
  analyzer.OnFrameEncoded("alice", frame.id(), FakeEncode(frame),
                          VideoQualityAnalyzerInterface::EncoderStats());

  analyzer.OnFramePreDecode("bob", frame.id(), FakeEncode(frame));
  analyzer.OnFrameDecoded("bob", DeepCopy(frame),
                          VideoQualityAnalyzerInterface::DecoderStats());
  analyzer.OnFrameRendered("bob", DeepCopy(frame));

  analyzer.UnregisterParticipantInCall("bob");

  analyzer.OnFramePreDecode("alice", frame.id(), FakeEncode(frame));
  analyzer.OnFrameDecoded("alice", DeepCopy(frame),
                          VideoQualityAnalyzerInterface::DecoderStats());
  analyzer.OnFrameRendered("alice", DeepCopy(frame));

  // Give analyzer some time to process frames on async thread. The computations
  // have to be fast (heavy metrics are disabled!), so if doesn't fit 100ms it
  // means we have an issue!
  SleepMs(100);
  analyzer.Stop();

  FrameCounters alice_alice_stream_conters =
      analyzer.GetPerStreamCounters().at(StatsKey("alice_video", "alice"));
  EXPECT_EQ(alice_alice_stream_conters.captured, 1);
  EXPECT_EQ(alice_alice_stream_conters.pre_encoded, 1);
  EXPECT_EQ(alice_alice_stream_conters.encoded, 1);
  EXPECT_EQ(alice_alice_stream_conters.received, 1);
  EXPECT_EQ(alice_alice_stream_conters.decoded, 1);
  EXPECT_EQ(alice_alice_stream_conters.rendered, 1);

  FrameCounters alice_bob_stream_conters =
      analyzer.GetPerStreamCounters().at(StatsKey("alice_video", "bob"));
  EXPECT_EQ(alice_bob_stream_conters.captured, 1);
  EXPECT_EQ(alice_bob_stream_conters.pre_encoded, 1);
  EXPECT_EQ(alice_bob_stream_conters.encoded, 1);
  EXPECT_EQ(alice_bob_stream_conters.received, 1);
  EXPECT_EQ(alice_bob_stream_conters.decoded, 1);
  EXPECT_EQ(alice_bob_stream_conters.rendered, 1);
}

TEST(DefaultVideoQualityAnalyzerTest, ReceiverRemovedBeforeCapturing2ndFrame) {
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      test::CreateSquareFrameGenerator(kFrameWidth, kFrameHeight,
                                       /*type=*/absl::nullopt,
                                       /*num_squares=*/absl::nullopt);

  DefaultVideoQualityAnalyzerOptions options = AnalyzerOptionsForTest();
  DefaultVideoQualityAnalyzer analyzer(Clock::GetRealTimeClock(), options);
  analyzer.Start("test_case", std::vector<std::string>{"alice", "bob"},
                 kAnalyzerMaxThreadsCount);

  PassFramesThroughAnalyzer(analyzer, "alice", "alice_video", {"bob"},
                            /*frames_count=*/1, *frame_generator);
  analyzer.UnregisterParticipantInCall("bob");
  PassFramesThroughAnalyzer(analyzer, "alice", "alice_video", {},
                            /*frames_count=*/1, *frame_generator);

  // Give analyzer some time to process frames on async thread. The computations
  // have to be fast (heavy metrics are disabled!), so if doesn't fit 100ms it
  // means we have an issue!
  SleepMs(100);
  analyzer.Stop();

  FrameCounters global_stream_conters = analyzer.GetGlobalCounters();
  EXPECT_EQ(global_stream_conters.captured, 2);
  EXPECT_EQ(global_stream_conters.pre_encoded, 2);
  EXPECT_EQ(global_stream_conters.encoded, 2);
  EXPECT_EQ(global_stream_conters.received, 1);
  EXPECT_EQ(global_stream_conters.decoded, 1);
  EXPECT_EQ(global_stream_conters.rendered, 1);
  FrameCounters stream_conters =
      analyzer.GetPerStreamCounters().at(StatsKey("alice_video", "bob"));
  EXPECT_EQ(stream_conters.captured, 2);
  EXPECT_EQ(stream_conters.pre_encoded, 2);
  EXPECT_EQ(stream_conters.encoded, 2);
  EXPECT_EQ(stream_conters.received, 1);
  EXPECT_EQ(stream_conters.decoded, 1);
  EXPECT_EQ(stream_conters.rendered, 1);
}

TEST(DefaultVideoQualityAnalyzerTest, ReceiverRemovedBeforePreEncoded) {
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      test::CreateSquareFrameGenerator(kFrameWidth, kFrameHeight,
                                       /*type=*/absl::nullopt,
                                       /*num_squares=*/absl::nullopt);

  DefaultVideoQualityAnalyzerOptions options = AnalyzerOptionsForTest();
  DefaultVideoQualityAnalyzer analyzer(Clock::GetRealTimeClock(), options);
  analyzer.Start("test_case", std::vector<std::string>{"alice", "bob"},
                 kAnalyzerMaxThreadsCount);

  VideoFrame frame = NextFrame(frame_generator.get(), /*timestamp_us=*/1);
  uint16_t frame_id = analyzer.OnFrameCaptured("alice", "alice_video", frame);
  frame.set_id(frame_id);
  analyzer.UnregisterParticipantInCall("bob");
  analyzer.OnFramePreEncode("alice", frame);
  analyzer.OnFrameEncoded("alice", frame.id(), FakeEncode(frame),
                          VideoQualityAnalyzerInterface::EncoderStats());

  // Give analyzer some time to process frames on async thread. The computations
  // have to be fast (heavy metrics are disabled!), so if doesn't fit 100ms it
  // means we have an issue!
  SleepMs(100);
  analyzer.Stop();

  FrameCounters global_stream_conters = analyzer.GetGlobalCounters();
  EXPECT_EQ(global_stream_conters.captured, 1);
  EXPECT_EQ(global_stream_conters.pre_encoded, 1);
  EXPECT_EQ(global_stream_conters.encoded, 1);
  EXPECT_EQ(global_stream_conters.received, 0);
  EXPECT_EQ(global_stream_conters.decoded, 0);
  EXPECT_EQ(global_stream_conters.rendered, 0);
  FrameCounters stream_conters =
      analyzer.GetPerStreamCounters().at(StatsKey("alice_video", "bob"));
  EXPECT_EQ(stream_conters.captured, 1);
  EXPECT_EQ(stream_conters.pre_encoded, 1);
  EXPECT_EQ(stream_conters.encoded, 1);
  EXPECT_EQ(stream_conters.received, 0);
  EXPECT_EQ(stream_conters.decoded, 0);
  EXPECT_EQ(stream_conters.rendered, 0);
}

TEST(DefaultVideoQualityAnalyzerTest, ReceiverRemovedBeforeEncoded) {
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      test::CreateSquareFrameGenerator(kFrameWidth, kFrameHeight,
                                       /*type=*/absl::nullopt,
                                       /*num_squares=*/absl::nullopt);

  DefaultVideoQualityAnalyzerOptions options = AnalyzerOptionsForTest();
  DefaultVideoQualityAnalyzer analyzer(Clock::GetRealTimeClock(), options);
  analyzer.Start("test_case", std::vector<std::string>{"alice", "bob"},
                 kAnalyzerMaxThreadsCount);

  VideoFrame frame = NextFrame(frame_generator.get(), /*timestamp_us=*/1);
  uint16_t frame_id = analyzer.OnFrameCaptured("alice", "alice_video", frame);
  frame.set_id(frame_id);
  analyzer.OnFramePreEncode("alice", frame);
  analyzer.UnregisterParticipantInCall("bob");
  analyzer.OnFrameEncoded("alice", frame.id(), FakeEncode(frame),
                          VideoQualityAnalyzerInterface::EncoderStats());

  // Give analyzer some time to process frames on async thread. The computations
  // have to be fast (heavy metrics are disabled!), so if doesn't fit 100ms it
  // means we have an issue!
  SleepMs(100);
  analyzer.Stop();

  FrameCounters global_stream_conters = analyzer.GetGlobalCounters();
  EXPECT_EQ(global_stream_conters.captured, 1);
  EXPECT_EQ(global_stream_conters.pre_encoded, 1);
  EXPECT_EQ(global_stream_conters.encoded, 1);
  EXPECT_EQ(global_stream_conters.received, 0);
  EXPECT_EQ(global_stream_conters.decoded, 0);
  EXPECT_EQ(global_stream_conters.rendered, 0);
  FrameCounters stream_conters =
      analyzer.GetPerStreamCounters().at(StatsKey("alice_video", "bob"));
  EXPECT_EQ(stream_conters.captured, 1);
  EXPECT_EQ(stream_conters.pre_encoded, 1);
  EXPECT_EQ(stream_conters.encoded, 1);
  EXPECT_EQ(stream_conters.received, 0);
  EXPECT_EQ(stream_conters.decoded, 0);
  EXPECT_EQ(stream_conters.rendered, 0);
}

TEST(DefaultVideoQualityAnalyzerTest,
     ReceiverRemovedBetweenSimulcastLayersEncoded) {
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      test::CreateSquareFrameGenerator(kFrameWidth, kFrameHeight,
                                       /*type=*/absl::nullopt,
                                       /*num_squares=*/absl::nullopt);

  DefaultVideoQualityAnalyzerOptions options = AnalyzerOptionsForTest();
  DefaultVideoQualityAnalyzer analyzer(Clock::GetRealTimeClock(), options);
  analyzer.Start("test_case", std::vector<std::string>{"alice", "bob"},
                 kAnalyzerMaxThreadsCount);

  VideoFrame frame = NextFrame(frame_generator.get(), /*timestamp_us=*/1);
  uint16_t frame_id = analyzer.OnFrameCaptured("alice", "alice_video", frame);
  frame.set_id(frame_id);
  analyzer.OnFramePreEncode("alice", frame);
  // 1st simulcast layer encoded
  analyzer.OnFrameEncoded("alice", frame.id(), FakeEncode(frame),
                          VideoQualityAnalyzerInterface::EncoderStats());
  analyzer.UnregisterParticipantInCall("bob");
  // 2nd simulcast layer encoded
  analyzer.OnFrameEncoded("alice", frame.id(), FakeEncode(frame),
                          VideoQualityAnalyzerInterface::EncoderStats());

  // Give analyzer some time to process frames on async thread. The computations
  // have to be fast (heavy metrics are disabled!), so if doesn't fit 100ms it
  // means we have an issue!
  SleepMs(100);
  analyzer.Stop();

  FrameCounters global_stream_conters = analyzer.GetGlobalCounters();
  EXPECT_EQ(global_stream_conters.captured, 1);
  EXPECT_EQ(global_stream_conters.pre_encoded, 1);
  EXPECT_EQ(global_stream_conters.encoded, 1);
  EXPECT_EQ(global_stream_conters.received, 0);
  EXPECT_EQ(global_stream_conters.decoded, 0);
  EXPECT_EQ(global_stream_conters.rendered, 0);
  FrameCounters stream_conters =
      analyzer.GetPerStreamCounters().at(StatsKey("alice_video", "bob"));
  EXPECT_EQ(stream_conters.captured, 1);
  EXPECT_EQ(stream_conters.pre_encoded, 1);
  EXPECT_EQ(stream_conters.encoded, 1);
  EXPECT_EQ(stream_conters.received, 0);
  EXPECT_EQ(stream_conters.decoded, 0);
  EXPECT_EQ(stream_conters.rendered, 0);
}

TEST(DefaultVideoQualityAnalyzerTest, UnregisterOneAndRegisterAnother) {
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      test::CreateSquareFrameGenerator(kFrameWidth, kFrameHeight,
                                       /*type=*/absl::nullopt,
                                       /*num_squares=*/absl::nullopt);

  DefaultVideoQualityAnalyzerOptions options = AnalyzerOptionsForTest();
  DefaultVideoQualityAnalyzer analyzer(Clock::GetRealTimeClock(), options);
  analyzer.Start("test_case",
                 std::vector<std::string>{"alice", "bob", "charlie"},
                 kAnalyzerMaxThreadsCount);

  PassFramesThroughAnalyzer(analyzer, "alice", "alice_video",
                            {"bob", "charlie"},
                            /*frames_count=*/2, *frame_generator);
  analyzer.UnregisterParticipantInCall("bob");
  analyzer.RegisterParticipantInCall("david");
  PassFramesThroughAnalyzer(analyzer, "alice", "alice_video",
                            {"charlie", "david"},
                            /*frames_count=*/4, *frame_generator);

  // Give analyzer some time to process frames on async thread. The computations
  // have to be fast (heavy metrics are disabled!), so if doesn't fit 100ms it
  // means we have an issue!
  SleepMs(100);
  analyzer.Stop();

  FrameCounters global_stream_conters = analyzer.GetGlobalCounters();
  EXPECT_EQ(global_stream_conters.captured, 6);
  EXPECT_EQ(global_stream_conters.pre_encoded, 6);
  EXPECT_EQ(global_stream_conters.encoded, 6);
  EXPECT_EQ(global_stream_conters.received, 12);
  EXPECT_EQ(global_stream_conters.decoded, 12);
  EXPECT_EQ(global_stream_conters.rendered, 12);
  FrameCounters alice_bob_stream_conters =
      analyzer.GetPerStreamCounters().at(StatsKey("alice_video", "bob"));
  EXPECT_EQ(alice_bob_stream_conters.captured, 6);
  EXPECT_EQ(alice_bob_stream_conters.pre_encoded, 6);
  EXPECT_EQ(alice_bob_stream_conters.encoded, 6);
  EXPECT_EQ(alice_bob_stream_conters.received, 2);
  EXPECT_EQ(alice_bob_stream_conters.decoded, 2);
  EXPECT_EQ(alice_bob_stream_conters.rendered, 2);
  FrameCounters alice_charlie_stream_conters =
      analyzer.GetPerStreamCounters().at(StatsKey("alice_video", "charlie"));
  EXPECT_EQ(alice_charlie_stream_conters.captured, 6);
  EXPECT_EQ(alice_charlie_stream_conters.pre_encoded, 6);
  EXPECT_EQ(alice_charlie_stream_conters.encoded, 6);
  EXPECT_EQ(alice_charlie_stream_conters.received, 6);
  EXPECT_EQ(alice_charlie_stream_conters.decoded, 6);
  EXPECT_EQ(alice_charlie_stream_conters.rendered, 6);
  FrameCounters alice_david_stream_conters =
      analyzer.GetPerStreamCounters().at(StatsKey("alice_video", "david"));
  EXPECT_EQ(alice_david_stream_conters.captured, 6);
  EXPECT_EQ(alice_david_stream_conters.pre_encoded, 6);
  EXPECT_EQ(alice_david_stream_conters.encoded, 6);
  EXPECT_EQ(alice_david_stream_conters.received, 4);
  EXPECT_EQ(alice_david_stream_conters.decoded, 4);
  EXPECT_EQ(alice_david_stream_conters.rendered, 4);
}

TEST(DefaultVideoQualityAnalyzerTest,
     UnregisterOneAndRegisterAnotherRegisterBack) {
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      test::CreateSquareFrameGenerator(kFrameWidth, kFrameHeight,
                                       /*type=*/absl::nullopt,
                                       /*num_squares=*/absl::nullopt);

  DefaultVideoQualityAnalyzerOptions options = AnalyzerOptionsForTest();
  DefaultVideoQualityAnalyzer analyzer(Clock::GetRealTimeClock(), options);
  analyzer.Start("test_case",
                 std::vector<std::string>{"alice", "bob", "charlie"},
                 kAnalyzerMaxThreadsCount);

  PassFramesThroughAnalyzer(analyzer, "alice", "alice_video",
                            {"bob", "charlie"},
                            /*frames_count=*/2, *frame_generator);
  analyzer.UnregisterParticipantInCall("bob");
  PassFramesThroughAnalyzer(analyzer, "alice", "alice_video", {"charlie"},
                            /*frames_count=*/4, *frame_generator);
  analyzer.RegisterParticipantInCall("bob");
  PassFramesThroughAnalyzer(analyzer, "alice", "alice_video",
                            {"bob", "charlie"},
                            /*frames_count=*/6, *frame_generator);

  // Give analyzer some time to process frames on async thread. The computations
  // have to be fast (heavy metrics are disabled!), so if doesn't fit 100ms it
  // means we have an issue!
  SleepMs(100);
  analyzer.Stop();

  FrameCounters global_stream_conters = analyzer.GetGlobalCounters();
  EXPECT_EQ(global_stream_conters.captured, 12);
  EXPECT_EQ(global_stream_conters.pre_encoded, 12);
  EXPECT_EQ(global_stream_conters.encoded, 12);
  EXPECT_EQ(global_stream_conters.received, 20);
  EXPECT_EQ(global_stream_conters.decoded, 20);
  EXPECT_EQ(global_stream_conters.rendered, 20);
  FrameCounters alice_bob_stream_conters =
      analyzer.GetPerStreamCounters().at(StatsKey("alice_video", "bob"));
  EXPECT_EQ(alice_bob_stream_conters.captured, 12);
  EXPECT_EQ(alice_bob_stream_conters.pre_encoded, 12);
  EXPECT_EQ(alice_bob_stream_conters.encoded, 12);
  EXPECT_EQ(alice_bob_stream_conters.received, 8);
  EXPECT_EQ(alice_bob_stream_conters.decoded, 8);
  EXPECT_EQ(alice_bob_stream_conters.rendered, 8);
  FrameCounters alice_charlie_stream_conters =
      analyzer.GetPerStreamCounters().at(StatsKey("alice_video", "charlie"));
  EXPECT_EQ(alice_charlie_stream_conters.captured, 12);
  EXPECT_EQ(alice_charlie_stream_conters.pre_encoded, 12);
  EXPECT_EQ(alice_charlie_stream_conters.encoded, 12);
  EXPECT_EQ(alice_charlie_stream_conters.received, 12);
  EXPECT_EQ(alice_charlie_stream_conters.decoded, 12);
  EXPECT_EQ(alice_charlie_stream_conters.rendered, 12);
}

=======
>>>>>>> parent of 8e32ad0e8387 (revert libwebrtc changes to help bump)
}  // namespace
}  // namespace webrtc_pc_e2e
}  // namespace webrtc
