/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/pc/e2e/analyzer/video/analyzing_video_sink.h"

#include <stdio.h>

#include <optional>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "api/scoped_refptr.h"
#include "api/test/create_frame_generator.h"
#include "api/test/frame_generator_interface.h"
#include "api/test/pclf/media_configuration.h"
#include "api/units/time_delta.h"
#include "api/units/timestamp.h"
#include "api/video/i420_buffer.h"
#include "api/video/video_frame.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_base/time_utils.h"
#include "system_wrappers/include/clock.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/pc/e2e/analyzer/video/example_video_quality_analyzer.h"
#include "test/testsupport/file_utils.h"
#include "test/testsupport/frame_reader.h"
#include "test/time_controller/simulated_time_controller.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::Test;

// Remove files and directories in a directory non-recursively.
void CleanDir(absl::string_view dir, size_t expected_output_files_count) {
  std::optional<std::vector<std::string>> dir_content =
      test::ReadDirectory(dir);
  if (expected_output_files_count == 0) {
    ASSERT_TRUE(!dir_content.has_value() || dir_content->empty())
        << "Empty directory is expected";
  } else {
    ASSERT_TRUE(dir_content.has_value()) << "Test directory is empty!";
    EXPECT_EQ(dir_content->size(), expected_output_files_count);
    for (const auto& entry : *dir_content) {
      if (test::DirExists(entry)) {
        EXPECT_TRUE(test::RemoveDir(entry))
            << "Failed to remove sub directory: " << entry;
      } else if (test::FileExists(entry)) {
        EXPECT_TRUE(test::RemoveFile(entry))
            << "Failed to remove file: " << entry;
      } else {
        FAIL() << "Can't remove unknown file type: " << entry;
      }
    }
  }
  EXPECT_TRUE(test::RemoveDir(dir)) << "Failed to remove directory: " << dir;
}

VideoFrame CreateFrame(test::FrameGeneratorInterface& frame_generator) {
  test::FrameGeneratorInterface::VideoFrameData frame_data =
      frame_generator.NextFrame();
  return VideoFrame::Builder()
      .set_video_frame_buffer(frame_data.buffer)
      .set_update_rect(frame_data.update_rect)
      .build();
}

std::unique_ptr<test::FrameGeneratorInterface> CreateFrameGenerator(
    size_t width,
    size_t height) {
  return test::CreateSquareFrameGenerator(width, height,
                                          /*type=*/std::nullopt,
                                          /*num_squares=*/std::nullopt);
}

void AssertFrameIdsAre(const std::string& filename,
                       std::vector<std::string> expected_ids) {
  FILE* file = fopen(filename.c_str(), "r");
  ASSERT_TRUE(file != nullptr) << "Failed to open frame ids file: " << filename;
  std::vector<std::string> actual_ids;
  char buffer[8];
  while (fgets(buffer, sizeof buffer, file) != nullptr) {
    std::string current_id(buffer);
    EXPECT_GE(current_id.size(), 2lu)
        << "Found invalid frame id: [" << current_id << "]";
    if (current_id.size() < 2) {
      continue;
    }
    // Trim "\n" at the end.
    actual_ids.push_back(current_id.substr(0, current_id.size() - 1));
  }
  fclose(file);
  EXPECT_THAT(actual_ids, ElementsAreArray(expected_ids));
}

class AnalyzingVideoSinkTest : public Test {
 protected:
  ~AnalyzingVideoSinkTest() override = default;

  void SetUp() override {
    // Create an empty temporary directory for this test.
    test_directory_ = test::JoinFilename(
        test::OutputPath(),
        "TestDir_AnalyzingVideoSinkTest_" +
            std::string(
                testing::UnitTest::GetInstance()->current_test_info()->name()));
    test::CreateDir(test_directory_);
  }

  void TearDown() override {
    CleanDir(test_directory_, expected_output_files_count_);
  }

  void ExpectOutputFilesCount(size_t count) {
    expected_output_files_count_ = count;
  }

  std::string test_directory_;
  size_t expected_output_files_count_ = 0;
};

TEST_F(AnalyzingVideoSinkTest, VideoFramesAreDumpedCorrectly) {
  VideoSubscription subscription;
  subscription.SubscribeToPeer(
      "alice", VideoResolution(/*width=*/640, /*height=*/360, /*fps=*/30));
  VideoConfig video_config("alice_video", /*width=*/1280, /*height=*/720,
                           /*fps=*/30);
  video_config.output_dump_options = VideoDumpOptions(test_directory_);

  ExampleVideoQualityAnalyzer analyzer;
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      CreateFrameGenerator(/*width=*/1280, /*height=*/720);
  VideoFrame frame = CreateFrame(*frame_generator);
  frame.set_id(analyzer.OnFrameCaptured("alice", "alice_video", frame));

  {
    // `helper` and `sink` has to be destroyed so all frames will be written
    // to the disk.
    AnalyzingVideoSinksHelper helper;
    helper.AddConfig("alice", video_config);
    AnalyzingVideoSink sink("bob", Clock::GetRealTimeClock(), analyzer, helper,
                            subscription, /*report_infra_stats=*/false);
    sink.OnFrame(frame);
  }

  EXPECT_THAT(analyzer.frames_rendered(), Eq(static_cast<uint64_t>(1)));

  auto frame_reader = test::CreateY4mFrameReader(
      test::JoinFilename(test_directory_, "alice_video_bob_640x360_30.y4m"));
  EXPECT_THAT(frame_reader->num_frames(), Eq(1));
  rtc::scoped_refptr<I420Buffer> actual_frame = frame_reader->PullFrame();
  rtc::scoped_refptr<I420BufferInterface> expected_frame =
      frame.video_frame_buffer()->ToI420();
  double psnr = I420PSNR(*expected_frame, *actual_frame);
  double ssim = I420SSIM(*expected_frame, *actual_frame);
  // Actual should be downscaled version of expected.
  EXPECT_GT(ssim, 0.98);
  EXPECT_GT(psnr, 38);

  ExpectOutputFilesCount(1);
}

TEST_F(AnalyzingVideoSinkTest,
       FallbackOnConfigResolutionIfNoSubscriptionProvided) {
  VideoSubscription subscription;
  VideoConfig video_config("alice_video", /*width=*/320, /*height=*/240,
                           /*fps=*/30);
  video_config.output_dump_options = VideoDumpOptions(test_directory_);

  ExampleVideoQualityAnalyzer analyzer;
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      CreateFrameGenerator(/*width=*/320, /*height=*/240);
  VideoFrame frame = CreateFrame(*frame_generator);
  frame.set_id(analyzer.OnFrameCaptured("alice", "alice_video", frame));

  {
    // `helper` and `sink` has to be destroyed so all frames will be written
    // to the disk.
    AnalyzingVideoSinksHelper helper;
    helper.AddConfig("alice", video_config);
    AnalyzingVideoSink sink("bob", Clock::GetRealTimeClock(), analyzer, helper,
                            subscription, /*report_infra_stats=*/false);
    sink.OnFrame(frame);
  }

  EXPECT_THAT(analyzer.frames_rendered(), Eq(static_cast<uint64_t>(1)));

  auto frame_reader = test::CreateY4mFrameReader(
      test::JoinFilename(test_directory_, "alice_video_bob_320x240_30.y4m"));
  EXPECT_THAT(frame_reader->num_frames(), Eq(1));
  rtc::scoped_refptr<I420Buffer> actual_frame = frame_reader->PullFrame();
  rtc::scoped_refptr<I420BufferInterface> expected_frame =
      frame.video_frame_buffer()->ToI420();
  double psnr = I420PSNR(*expected_frame, *actual_frame);
  double ssim = I420SSIM(*expected_frame, *actual_frame);
  // Frames should be equal.
  EXPECT_DOUBLE_EQ(ssim, 1.00);
  EXPECT_DOUBLE_EQ(psnr, 48);

  ExpectOutputFilesCount(1);
}

TEST_F(AnalyzingVideoSinkTest,
       FallbackOnConfigResolutionIfNoSubscriptionIsNotResolved) {
  VideoSubscription subscription;
  subscription.SubscribeToAllPeers(
      VideoResolution(VideoResolution::Spec::kMaxFromSender));
  VideoConfig video_config("alice_video", /*width=*/320, /*height=*/240,
                           /*fps=*/30);
  video_config.output_dump_options = VideoDumpOptions(test_directory_);

  ExampleVideoQualityAnalyzer analyzer;
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      CreateFrameGenerator(/*width=*/320, /*height=*/240);
  VideoFrame frame = CreateFrame(*frame_generator);
  frame.set_id(analyzer.OnFrameCaptured("alice", "alice_video", frame));

  {
    // `helper` and `sink` has to be destroyed so all frames will be written
    // to the disk.
    AnalyzingVideoSinksHelper helper;
    helper.AddConfig("alice", video_config);
    AnalyzingVideoSink sink("bob", Clock::GetRealTimeClock(), analyzer, helper,
                            subscription, /*report_infra_stats=*/false);
    sink.OnFrame(frame);
  }

  EXPECT_THAT(analyzer.frames_rendered(), Eq(static_cast<uint64_t>(1)));

  auto frame_reader = test::CreateY4mFrameReader(
      test::JoinFilename(test_directory_, "alice_video_bob_320x240_30.y4m"));
  EXPECT_THAT(frame_reader->num_frames(), Eq(1));
  rtc::scoped_refptr<I420Buffer> actual_frame = frame_reader->PullFrame();
  rtc::scoped_refptr<I420BufferInterface> expected_frame =
      frame.video_frame_buffer()->ToI420();
  double psnr = I420PSNR(*expected_frame, *actual_frame);
  double ssim = I420SSIM(*expected_frame, *actual_frame);
  // Frames should be equal.
  EXPECT_DOUBLE_EQ(ssim, 1.00);
  EXPECT_DOUBLE_EQ(psnr, 48);

  ExpectOutputFilesCount(1);
}

TEST_F(AnalyzingVideoSinkTest,
       VideoFramesAreDumpedCorrectlyWhenSubscriptionChanged) {
  VideoSubscription subscription_before;
  subscription_before.SubscribeToPeer(
      "alice", VideoResolution(/*width=*/1280, /*height=*/720, /*fps=*/30));
  VideoSubscription subscription_after;
  subscription_after.SubscribeToPeer(
      "alice", VideoResolution(/*width=*/640, /*height=*/360, /*fps=*/30));
  VideoConfig video_config("alice_video", /*width=*/1280, /*height=*/720,
                           /*fps=*/30);
  video_config.output_dump_options = VideoDumpOptions(test_directory_);

  ExampleVideoQualityAnalyzer analyzer;
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      CreateFrameGenerator(/*width=*/1280, /*height=*/720);
  VideoFrame frame_before = CreateFrame(*frame_generator);
  frame_before.set_id(
      analyzer.OnFrameCaptured("alice", "alice_video", frame_before));
  VideoFrame frame_after = CreateFrame(*frame_generator);
  frame_after.set_id(
      analyzer.OnFrameCaptured("alice", "alice_video", frame_after));

  {
    // `helper` and `sink` has to be destroyed so all frames will be written
    // to the disk.
    AnalyzingVideoSinksHelper helper;
    helper.AddConfig("alice", video_config);
    AnalyzingVideoSink sink("bob", Clock::GetRealTimeClock(), analyzer, helper,
                            subscription_before, /*report_infra_stats=*/false);
    sink.OnFrame(frame_before);

    sink.UpdateSubscription(subscription_after);
    sink.OnFrame(frame_after);
  }

  EXPECT_THAT(analyzer.frames_rendered(), Eq(static_cast<uint64_t>(2)));

  {
    auto frame_reader = test::CreateY4mFrameReader(
        test::JoinFilename(test_directory_, "alice_video_bob_1280x720_30.y4m"));
    EXPECT_THAT(frame_reader->num_frames(), Eq(1));
    rtc::scoped_refptr<I420Buffer> actual_frame = frame_reader->PullFrame();
    rtc::scoped_refptr<I420BufferInterface> expected_frame =
        frame_before.video_frame_buffer()->ToI420();
    double psnr = I420PSNR(*expected_frame, *actual_frame);
    double ssim = I420SSIM(*expected_frame, *actual_frame);
    // Frames should be equal.
    EXPECT_DOUBLE_EQ(ssim, 1.00);
    EXPECT_DOUBLE_EQ(psnr, 48);
  }
  {
    auto frame_reader = test::CreateY4mFrameReader(
        test::JoinFilename(test_directory_, "alice_video_bob_640x360_30.y4m"));
    EXPECT_THAT(frame_reader->num_frames(), Eq(1));
    rtc::scoped_refptr<I420Buffer> actual_frame = frame_reader->PullFrame();
    rtc::scoped_refptr<I420BufferInterface> expected_frame =
        frame_after.video_frame_buffer()->ToI420();
    double psnr = I420PSNR(*expected_frame, *actual_frame);
    double ssim = I420SSIM(*expected_frame, *actual_frame);
    // Actual should be downscaled version of expected.
    EXPECT_GT(ssim, 0.98);
    EXPECT_GT(psnr, 38);
  }

  ExpectOutputFilesCount(2);
}

TEST_F(AnalyzingVideoSinkTest,
       VideoFramesAreDumpedCorrectlyWhenSubscriptionChangedOnTheSameOne) {
  VideoSubscription subscription_before;
  subscription_before.SubscribeToPeer(
      "alice", VideoResolution(/*width=*/640, /*height=*/360, /*fps=*/30));
  VideoSubscription subscription_after;
  subscription_after.SubscribeToPeer(
      "alice", VideoResolution(/*width=*/640, /*height=*/360, /*fps=*/30));
  VideoConfig video_config("alice_video", /*width=*/640, /*height=*/360,
                           /*fps=*/30);
  video_config.output_dump_options = VideoDumpOptions(test_directory_);

  ExampleVideoQualityAnalyzer analyzer;
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      CreateFrameGenerator(/*width=*/640, /*height=*/360);
  VideoFrame frame_before = CreateFrame(*frame_generator);
  frame_before.set_id(
      analyzer.OnFrameCaptured("alice", "alice_video", frame_before));
  VideoFrame frame_after = CreateFrame(*frame_generator);
  frame_after.set_id(
      analyzer.OnFrameCaptured("alice", "alice_video", frame_after));

  {
    // `helper` and `sink` has to be destroyed so all frames will be written
    // to the disk.
    AnalyzingVideoSinksHelper helper;
    helper.AddConfig("alice", video_config);
    AnalyzingVideoSink sink("bob", Clock::GetRealTimeClock(), analyzer, helper,
                            subscription_before, /*report_infra_stats=*/false);
    sink.OnFrame(frame_before);

    sink.UpdateSubscription(subscription_after);
    sink.OnFrame(frame_after);
  }

  EXPECT_THAT(analyzer.frames_rendered(), Eq(static_cast<uint64_t>(2)));

  {
    auto frame_reader = test::CreateY4mFrameReader(
        test::JoinFilename(test_directory_, "alice_video_bob_640x360_30.y4m"));
    EXPECT_THAT(frame_reader->num_frames(), Eq(2));
    // Read the first frame.
    rtc::scoped_refptr<I420Buffer> actual_frame = frame_reader->PullFrame();
    rtc::scoped_refptr<I420BufferInterface> expected_frame =
        frame_before.video_frame_buffer()->ToI420();
    // Frames should be equal.
    EXPECT_DOUBLE_EQ(I420SSIM(*expected_frame, *actual_frame), 1.00);
    EXPECT_DOUBLE_EQ(I420PSNR(*expected_frame, *actual_frame), 48);
    // Read the second frame.
    actual_frame = frame_reader->PullFrame();
    expected_frame = frame_after.video_frame_buffer()->ToI420();
    // Frames should be equal.
    EXPECT_DOUBLE_EQ(I420SSIM(*expected_frame, *actual_frame), 1.00);
    EXPECT_DOUBLE_EQ(I420PSNR(*expected_frame, *actual_frame), 48);
  }

  ExpectOutputFilesCount(1);
}

TEST_F(AnalyzingVideoSinkTest, SmallDiviationsInAspectRationAreAllowed) {
  VideoSubscription subscription;
  subscription.SubscribeToPeer(
      "alice", VideoResolution(/*width=*/480, /*height=*/270, /*fps=*/30));
  VideoConfig video_config("alice_video", /*width=*/480, /*height=*/270,
                           /*fps=*/30);
  video_config.output_dump_options = VideoDumpOptions(test_directory_);

  ExampleVideoQualityAnalyzer analyzer;
  // Generator produces downscaled frames with a bit different aspect ration.
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      CreateFrameGenerator(/*width=*/240, /*height=*/136);
  VideoFrame frame = CreateFrame(*frame_generator);
  frame.set_id(analyzer.OnFrameCaptured("alice", "alice_video", frame));

  {
    // `helper` and `sink` has to be destroyed so all frames will be written
    // to the disk.
    AnalyzingVideoSinksHelper helper;
    helper.AddConfig("alice", video_config);
    AnalyzingVideoSink sink("bob", Clock::GetRealTimeClock(), analyzer, helper,
                            subscription, /*report_infra_stats=*/false);
    sink.OnFrame(frame);
  }

  EXPECT_THAT(analyzer.frames_rendered(), Eq(static_cast<uint64_t>(1)));

  {
    auto frame_reader = test::CreateY4mFrameReader(
        test::JoinFilename(test_directory_, "alice_video_bob_480x270_30.y4m"));
    EXPECT_THAT(frame_reader->num_frames(), Eq(1));
    // Read the first frame.
    rtc::scoped_refptr<I420Buffer> actual_frame = frame_reader->PullFrame();
    rtc::scoped_refptr<I420BufferInterface> expected_frame =
        frame.video_frame_buffer()->ToI420();
    // Actual frame is upscaled version of the expected. But because rendered
    // resolution is equal to the actual frame size we need to upscale expected
    // during comparison and then they have to be the same.
    EXPECT_DOUBLE_EQ(I420SSIM(*actual_frame, *expected_frame), 1);
    EXPECT_DOUBLE_EQ(I420PSNR(*actual_frame, *expected_frame), 48);
  }

  ExpectOutputFilesCount(1);
}

TEST_F(AnalyzingVideoSinkTest, VideoFramesIdsAreDumpedWhenRequested) {
  VideoSubscription subscription;
  subscription.SubscribeToPeer(
      "alice", VideoResolution(/*width=*/320, /*height=*/240, /*fps=*/30));
  VideoConfig video_config("alice_video", /*width=*/320, /*height=*/240,
                           /*fps=*/30);
  video_config.output_dump_options =
      VideoDumpOptions(test_directory_, /*export_frame_ids=*/true);

  ExampleVideoQualityAnalyzer analyzer;
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      CreateFrameGenerator(/*width=*/320, /*height=*/240);

  std::vector<std::string> expected_frame_ids;
  {
    // `helper` and `sink` has to be destroyed so all frames will be written
    // to the disk.
    AnalyzingVideoSinksHelper helper;
    helper.AddConfig("alice", video_config);
    AnalyzingVideoSink sink("bob", Clock::GetRealTimeClock(), analyzer, helper,
                            subscription, /*report_infra_stats=*/false);
    for (int i = 0; i < 10; ++i) {
      VideoFrame frame = CreateFrame(*frame_generator);
      frame.set_id(analyzer.OnFrameCaptured("alice", "alice_video", frame));
      expected_frame_ids.push_back(std::to_string(frame.id()));
      sink.OnFrame(frame);
    }
  }

  EXPECT_THAT(analyzer.frames_rendered(), Eq(static_cast<uint64_t>(10)));

  AssertFrameIdsAre(
      test::JoinFilename(test_directory_,
                         "alice_video_bob_320x240_30.frame_ids.txt"),
      expected_frame_ids);

  ExpectOutputFilesCount(2);
}

TEST_F(AnalyzingVideoSinkTest,
       VideoFramesAndIdsAreDumpedWithFixedFpsWhenRequested) {
  GlobalSimulatedTimeController simulated_time(Timestamp::Seconds(100000));

  VideoSubscription subscription;
  subscription.SubscribeToPeer(
      "alice", VideoResolution(/*width=*/320, /*height=*/240, /*fps=*/10));
  VideoConfig video_config("alice_video", /*width=*/320, /*height=*/240,
                           /*fps=*/10);
  video_config.output_dump_options =
      VideoDumpOptions(test_directory_, /*export_frame_ids=*/true);
  video_config.output_dump_use_fixed_framerate = true;

  ExampleVideoQualityAnalyzer analyzer;
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      CreateFrameGenerator(/*width=*/320, /*height=*/240);

  VideoFrame frame1 = CreateFrame(*frame_generator);
  frame1.set_id(analyzer.OnFrameCaptured("alice", "alice_video", frame1));
  VideoFrame frame2 = CreateFrame(*frame_generator);
  frame2.set_id(analyzer.OnFrameCaptured("alice", "alice_video", frame2));

  {
    // `helper` and `sink` has to be destroyed so all frames will be written
    // to the disk.
    AnalyzingVideoSinksHelper helper;
    helper.AddConfig("alice", video_config);
    AnalyzingVideoSink sink("bob", simulated_time.GetClock(), analyzer, helper,
                            subscription, /*report_infra_stats=*/false);
    sink.OnFrame(frame1);
    // Advance almost 1 second, so the first frame has to be repeated 9 time
    // more.
    simulated_time.AdvanceTime(TimeDelta::Millis(990));
    sink.OnFrame(frame2);
    simulated_time.AdvanceTime(TimeDelta::Millis(100));
  }

  EXPECT_THAT(analyzer.frames_rendered(), Eq(static_cast<uint64_t>(2)));

  auto frame_reader = test::CreateY4mFrameReader(
      test::JoinFilename(test_directory_, "alice_video_bob_320x240_10.y4m"));
  EXPECT_THAT(frame_reader->num_frames(), Eq(11));
  for (int i = 0; i < 10; ++i) {
    rtc::scoped_refptr<I420Buffer> actual_frame = frame_reader->PullFrame();
    rtc::scoped_refptr<I420BufferInterface> expected_frame =
        frame1.video_frame_buffer()->ToI420();
    double psnr = I420PSNR(*expected_frame, *actual_frame);
    double ssim = I420SSIM(*expected_frame, *actual_frame);
    // Frames should be equal.
    EXPECT_DOUBLE_EQ(ssim, 1.00);
    EXPECT_DOUBLE_EQ(psnr, 48);
  }
  rtc::scoped_refptr<I420Buffer> actual_frame = frame_reader->PullFrame();
  rtc::scoped_refptr<I420BufferInterface> expected_frame =
      frame2.video_frame_buffer()->ToI420();
  double psnr = I420PSNR(*expected_frame, *actual_frame);
  double ssim = I420SSIM(*expected_frame, *actual_frame);
  // Frames should be equal.
  EXPECT_DOUBLE_EQ(ssim, 1.00);
  EXPECT_DOUBLE_EQ(psnr, 48);

  AssertFrameIdsAre(
      test::JoinFilename(test_directory_,
                         "alice_video_bob_320x240_10.frame_ids.txt"),
      {std::to_string(frame1.id()), std::to_string(frame1.id()),
       std::to_string(frame1.id()), std::to_string(frame1.id()),
       std::to_string(frame1.id()), std::to_string(frame1.id()),
       std::to_string(frame1.id()), std::to_string(frame1.id()),
       std::to_string(frame1.id()), std::to_string(frame1.id()),
       std::to_string(frame2.id())});

  ExpectOutputFilesCount(2);
}

TEST_F(AnalyzingVideoSinkTest, InfraMetricsCollectedWhenRequested) {
  VideoSubscription subscription;
  subscription.SubscribeToPeer(
      "alice", VideoResolution(/*width=*/1280, /*height=*/720, /*fps=*/30));
  VideoConfig video_config("alice_video", /*width=*/640, /*height=*/360,
                           /*fps=*/30);

  ExampleVideoQualityAnalyzer analyzer;
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      CreateFrameGenerator(/*width=*/640, /*height=*/360);
  VideoFrame frame = CreateFrame(*frame_generator);
  frame.set_id(analyzer.OnFrameCaptured("alice", "alice_video", frame));

  AnalyzingVideoSinksHelper helper;
  helper.AddConfig("alice", video_config);
  AnalyzingVideoSink sink("bob", Clock::GetRealTimeClock(), analyzer, helper,
                          subscription, /*report_infra_stats=*/true);
  sink.OnFrame(frame);

  AnalyzingVideoSink::Stats stats = sink.stats();
  EXPECT_THAT(stats.scaling_tims_ms.NumSamples(), Eq(1));
  EXPECT_THAT(stats.scaling_tims_ms.GetAverage(), Ge(0));
  EXPECT_THAT(stats.analyzing_sink_processing_time_ms.NumSamples(), Eq(1));
  EXPECT_THAT(stats.analyzing_sink_processing_time_ms.GetAverage(),
              Ge(stats.scaling_tims_ms.GetAverage()));

  ExpectOutputFilesCount(0);
}

TEST_F(AnalyzingVideoSinkTest, InfraMetricsNotCollectedWhenNotRequested) {
  VideoSubscription subscription;
  subscription.SubscribeToPeer(
      "alice", VideoResolution(/*width=*/1280, /*height=*/720, /*fps=*/30));
  VideoConfig video_config("alice_video", /*width=*/640, /*height=*/360,
                           /*fps=*/30);

  ExampleVideoQualityAnalyzer analyzer;
  std::unique_ptr<test::FrameGeneratorInterface> frame_generator =
      CreateFrameGenerator(/*width=*/640, /*height=*/360);
  VideoFrame frame = CreateFrame(*frame_generator);
  frame.set_id(analyzer.OnFrameCaptured("alice", "alice_video", frame));

  AnalyzingVideoSinksHelper helper;
  helper.AddConfig("alice", video_config);
  AnalyzingVideoSink sink("bob", Clock::GetRealTimeClock(), analyzer, helper,
                          subscription, /*report_infra_stats=*/false);
  sink.OnFrame(frame);

  AnalyzingVideoSink::Stats stats = sink.stats();
  EXPECT_THAT(stats.scaling_tims_ms.NumSamples(), Eq(0));
  EXPECT_THAT(stats.analyzing_sink_processing_time_ms.NumSamples(), Eq(0));

  ExpectOutputFilesCount(0);
}

}  // namespace
}  // namespace webrtc_pc_e2e
}  // namespace webrtc
