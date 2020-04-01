/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <map>
#include <memory>
#include <vector>

#include "api/rtp_packet_info.h"
#include "api/rtp_packet_infos.h"
#include "api/test/create_frame_generator.h"
#include "api/video/encoded_image.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "system_wrappers/include/sleep.h"
#include "test/gtest.h"
#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

constexpr int kAnalyzerMaxThreadsCount = 1;
constexpr int kMaxFramesInFlightPerStream = 10;
constexpr int kFrameWidth = 320;
constexpr int kFrameHeight = 240;
constexpr char kStreamLabel[] = "video-stream";

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
  packet_infos.push_back(
      RtpPacketInfo(/*ssrc=*/1,
                    /*csrcs=*/{},
                    /*rtp_timestamp=*/frame.timestamp(),
                    /*audio_level=*/absl::nullopt,
                    /*absolute_capture_time=*/absl::nullopt,
                    /*receive_time_ms=*/frame.timestamp_us() + 10));
  image.SetPacketInfos(RtpPacketInfos(packet_infos));
  return image;
}

VideoFrame DeepCopy(const VideoFrame& frame) {
  VideoFrame copy = frame;
  copy.set_video_frame_buffer(
      I420Buffer::Copy(*frame.video_frame_buffer()->ToI420()));
  return copy;
}

TEST(DefaultVideoQualityAnalyzerTest,
     MemoryOverloadedAndThenAllFramesReceived) {
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      test::CreateSquareFrameGenerator(kFrameWidth, kFrameHeight,
                                       /*type=*/absl::nullopt,
                                       /*num_squares=*/absl::nullopt);

  DefaultVideoQualityAnalyzer analyzer(
      /*heavy_metrics_computation_enabled=*/false, kMaxFramesInFlightPerStream);
  analyzer.Start("test_case", kAnalyzerMaxThreadsCount);

  std::map<uint16_t, VideoFrame> captured_frames;
  std::vector<uint16_t> frames_order;
  for (int i = 0; i < kMaxFramesInFlightPerStream * 2; ++i) {
    VideoFrame frame = NextFrame(frame_generator.get(), i);
    frame.set_id(analyzer.OnFrameCaptured(kStreamLabel, frame));
    frames_order.push_back(frame.id());
    captured_frames.insert({frame.id(), frame});
    analyzer.OnFramePreEncode(frame);
    analyzer.OnFrameEncoded(frame.id(), FakeEncode(frame));
  }

  for (const uint16_t& frame_id : frames_order) {
    VideoFrame received_frame = DeepCopy(captured_frames.at(frame_id));
    analyzer.OnFramePreDecode(received_frame.id(), FakeEncode(received_frame));
    analyzer.OnFrameDecoded(received_frame, /*decode_time_ms=*/absl::nullopt,
                            /*qp=*/absl::nullopt);
    analyzer.OnFrameRendered(received_frame);
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
     MemoryOverloadedHalfDroppedAndThenHalfFramesReceived) {
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      test::CreateSquareFrameGenerator(kFrameWidth, kFrameHeight,
                                       /*type=*/absl::nullopt,
                                       /*num_squares=*/absl::nullopt);

  DefaultVideoQualityAnalyzer analyzer(
      /*heavy_metrics_computation_enabled=*/false, kMaxFramesInFlightPerStream);
  analyzer.Start("test_case", kAnalyzerMaxThreadsCount);

  std::map<uint16_t, VideoFrame> captured_frames;
  std::vector<uint16_t> frames_order;
  for (int i = 0; i < kMaxFramesInFlightPerStream * 2; ++i) {
    VideoFrame frame = NextFrame(frame_generator.get(), i);
    frame.set_id(analyzer.OnFrameCaptured(kStreamLabel, frame));
    frames_order.push_back(frame.id());
    captured_frames.insert({frame.id(), frame});
    analyzer.OnFramePreEncode(frame);
    analyzer.OnFrameEncoded(frame.id(), FakeEncode(frame));
  }

  for (size_t i = kMaxFramesInFlightPerStream; i < frames_order.size(); ++i) {
    uint16_t frame_id = frames_order.at(i);
    VideoFrame received_frame = DeepCopy(captured_frames.at(frame_id));
    analyzer.OnFramePreDecode(received_frame.id(), FakeEncode(received_frame));
    analyzer.OnFrameDecoded(received_frame, /*decode_time_ms=*/absl::nullopt,
                            /*qp=*/absl::nullopt);
    analyzer.OnFrameRendered(received_frame);
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

  DefaultVideoQualityAnalyzer analyzer(
      /*heavy_metrics_computation_enabled=*/false, kMaxFramesInFlightPerStream);
  analyzer.Start("test_case", kAnalyzerMaxThreadsCount);

  std::map<uint16_t, VideoFrame> captured_frames;
  std::vector<uint16_t> frames_order;
  for (int i = 0; i < kMaxFramesInFlightPerStream; ++i) {
    VideoFrame frame = NextFrame(frame_generator.get(), i);
    frame.set_id(analyzer.OnFrameCaptured(kStreamLabel, frame));
    frames_order.push_back(frame.id());
    captured_frames.insert({frame.id(), frame});
    analyzer.OnFramePreEncode(frame);
    analyzer.OnFrameEncoded(frame.id(), FakeEncode(frame));
  }

  for (size_t i = 1; i < frames_order.size(); i += 2) {
    uint16_t frame_id = frames_order.at(i);
    VideoFrame received_frame = DeepCopy(captured_frames.at(frame_id));
    analyzer.OnFramePreDecode(received_frame.id(), FakeEncode(received_frame));
    analyzer.OnFrameDecoded(received_frame, /*decode_time_ms=*/absl::nullopt,
                            /*qp=*/absl::nullopt);
    analyzer.OnFrameRendered(received_frame);
  }

  // Give analyzer some time to process frames on async thread. The computations
  // have to be fast (heavy metrics are disabled!), so if doesn't fit 100ms it
  // means we have an issue!
  SleepMs(100);
  analyzer.Stop();

  AnalyzerStats stats = analyzer.GetAnalyzerStats();
  EXPECT_EQ(stats.memory_overloaded_comparisons_done, 0);
  EXPECT_EQ(stats.comparisons_done, kMaxFramesInFlightPerStream);

  FrameCounters frame_counters = analyzer.GetGlobalCounters();
  EXPECT_EQ(frame_counters.captured, kMaxFramesInFlightPerStream);
  EXPECT_EQ(frame_counters.received, kMaxFramesInFlightPerStream / 2);
  EXPECT_EQ(frame_counters.decoded, kMaxFramesInFlightPerStream / 2);
  EXPECT_EQ(frame_counters.rendered, kMaxFramesInFlightPerStream / 2);
  EXPECT_EQ(frame_counters.dropped, kMaxFramesInFlightPerStream / 2);
}

}  // namespace
}  // namespace webrtc_pc_e2e
}  // namespace webrtc
