/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This test doesn't actually verify the output since it's just printed
// to stdout by void functions, but it's still useful as it executes the code.

#include <stdio.h>
#include <fstream>
#include <string>

#include "rtc_tools/frame_analyzer/video_quality_analysis.h"
#include "test/gtest.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace test {

// Setup a log file to write the output to instead of stdout because we don't
// want those numbers to be picked up as perf numbers.
class VideoQualityAnalysisTest : public ::testing::Test {
 protected:
  void SetUp() {
    std::string log_filename = TempFilename(webrtc::test::OutputPath(),
                                            "VideoQualityAnalysisTest.log");
    logfile_ = fopen(log_filename.c_str(), "w");
    ASSERT_TRUE(logfile_ != NULL);

    stats_filename_ref_ = TempFilename(OutputPath(), "stats-1.txt");
    stats_filename_ = TempFilename(OutputPath(), "stats-2.txt");
  }
  void TearDown() { ASSERT_EQ(0, fclose(logfile_)); }
  FILE* logfile_;
  std::string stats_filename_ref_;
  std::string stats_filename_;
};

TEST_F(VideoQualityAnalysisTest, MatchExtractedY4mFrame) {
  std::string video_file =
         webrtc::test::ResourcePath("reference_less_video_test_file", "y4m");

  std::string extracted_frame_from_video_file =
         webrtc::test::ResourcePath("video_quality_analysis_frame", "txt");

  int frame_height = 720, frame_width = 1280;
  int frame_number = 2;
  int size = GetI420FrameSize(frame_width, frame_height);
  uint8_t* result_frame = new uint8_t[size];
  uint8_t* expected_frame = new uint8_t[size];

  FILE* input_file = fopen(extracted_frame_from_video_file.c_str(), "rb");
  fread(expected_frame, 1, size, input_file);

  ExtractFrameFromY4mFile(video_file.c_str(),
                          frame_width, frame_height,
                          frame_number, result_frame);

  EXPECT_EQ(*expected_frame, *result_frame);
  fclose(input_file);
  delete[] result_frame;
  delete[] expected_frame;
}

TEST_F(VideoQualityAnalysisTest, PrintAnalysisResultsEmpty) {
  ResultsContainer result;
  PrintAnalysisResults(logfile_, "Empty", &result);
}

TEST_F(VideoQualityAnalysisTest, PrintAnalysisResultsOneFrame) {
  ResultsContainer result;
  result.frames.push_back(AnalysisResult(0, 35.0, 0.9));
  PrintAnalysisResults(logfile_, "OneFrame", &result);
}

TEST_F(VideoQualityAnalysisTest, PrintAnalysisResultsThreeFrames) {
  ResultsContainer result;
  result.frames.push_back(AnalysisResult(0, 35.0, 0.9));
  result.frames.push_back(AnalysisResult(1, 34.0, 0.8));
  result.frames.push_back(AnalysisResult(2, 33.0, 0.7));
  PrintAnalysisResults(logfile_, "ThreeFrames", &result);
}

TEST_F(VideoQualityAnalysisTest, PrintMaxRepeatedAndSkippedFramesInvalidFile) {
  remove(stats_filename_.c_str());
  PrintMaxRepeatedAndSkippedFrames(logfile_, "NonExistingStatsFile",
                                   stats_filename_ref_, stats_filename_);
}

TEST_F(VideoQualityAnalysisTest,
       PrintMaxRepeatedAndSkippedFramesEmptyStatsFile) {
  std::ofstream stats_file;
  stats_file.open(stats_filename_ref_.c_str());
  stats_file.close();
  stats_file.open(stats_filename_.c_str());
  stats_file.close();
  PrintMaxRepeatedAndSkippedFrames(logfile_, "EmptyStatsFile",
                                   stats_filename_ref_, stats_filename_);
}

TEST_F(VideoQualityAnalysisTest, PrintMaxRepeatedAndSkippedFramesNormalFile) {
  std::ofstream stats_file;

  stats_file.open(stats_filename_ref_.c_str());
  stats_file << "frame_0001 0100\n";
  stats_file << "frame_0002 0101\n";
  stats_file << "frame_0003 0102\n";
  stats_file << "frame_0004 0103\n";
  stats_file << "frame_0005 0106\n";
  stats_file << "frame_0006 0107\n";
  stats_file << "frame_0007 0108\n";
  stats_file.close();

  stats_file.open(stats_filename_.c_str());
  stats_file << "frame_0001 0100\n";
  stats_file << "frame_0002 0101\n";
  stats_file << "frame_0003 0101\n";
  stats_file << "frame_0004 0106\n";
  stats_file.close();

  PrintMaxRepeatedAndSkippedFrames(logfile_, "NormalStatsFile",
                                   stats_filename_ref_, stats_filename_);
}

namespace {
void VerifyLogOutput(const std::string& log_filename,
                     const std::vector<std::string>& expected_out) {
  std::ifstream logf(log_filename);
  std::string line;

  std::size_t i;
  for (i = 0; i < expected_out.size() && getline(logf, line); ++i) {
    ASSERT_EQ(expected_out.at(i), line);
  }
  ASSERT_TRUE(i == expected_out.size()) << "Not enough input data";
}
}  // unnamed namespace

TEST_F(VideoQualityAnalysisTest,
       PrintMaxRepeatedAndSkippedFramesSkippedFrames) {
  std::ofstream stats_file;

  std::string log_filename =
      TempFilename(webrtc::test::OutputPath(), "log.log");
  FILE* logfile = fopen(log_filename.c_str(), "w");
  ASSERT_TRUE(logfile != NULL);
  stats_file.open(stats_filename_ref_.c_str());
  stats_file << "frame_0001 0100\n";
  stats_file << "frame_0002 0101\n";
  stats_file << "frame_0002 0101\n";
  stats_file << "frame_0003 0103\n";
  stats_file << "frame_0004 0103\n";
  stats_file << "frame_0005 0106\n";
  stats_file << "frame_0006 0106\n";
  stats_file << "frame_0007 0108\n";
  stats_file << "frame_0008 0110\n";
  stats_file << "frame_0009 0112\n";
  stats_file.close();

  stats_file.open(stats_filename_.c_str());
  stats_file << "frame_0001 0101\n";
  stats_file << "frame_0002 0101\n";
  stats_file << "frame_0003 0101\n";
  stats_file << "frame_0004 0108\n";
  stats_file << "frame_0005 0108\n";
  stats_file << "frame_0006 0112\n";
  stats_file.close();

  PrintMaxRepeatedAndSkippedFrames(logfile, "NormalStatsFile",
                                   stats_filename_ref_, stats_filename_);
  ASSERT_EQ(0, fclose(logfile));

  std::vector<std::string> expected_out = {
      "RESULT Max_repeated: NormalStatsFile= 2",
      "RESULT Max_skipped: NormalStatsFile= 2",
      "RESULT Total_skipped: NormalStatsFile= 3",
      "RESULT Decode_errors_reference: NormalStatsFile= 0",
      "RESULT Decode_errors_test: NormalStatsFile= 0"};
  VerifyLogOutput(log_filename, expected_out);
}

TEST_F(VideoQualityAnalysisTest,
       PrintMaxRepeatedAndSkippedFramesDecodeErrorInTest) {
  std::ofstream stats_file;

  std::string log_filename =
      TempFilename(webrtc::test::OutputPath(), "log.log");
  FILE* logfile = fopen(log_filename.c_str(), "w");
  ASSERT_TRUE(logfile != NULL);
  stats_file.open(stats_filename_ref_.c_str());
  stats_file << "frame_0001 0100\n";
  stats_file << "frame_0002 0100\n";
  stats_file << "frame_0002 0101\n";
  stats_file << "frame_0003 0103\n";
  stats_file << "frame_0004 0103\n";
  stats_file << "frame_0005 0106\n";
  stats_file << "frame_0006 0107\n";
  stats_file << "frame_0007 0107\n";
  stats_file << "frame_0008 0110\n";
  stats_file << "frame_0009 0112\n";
  stats_file.close();

  stats_file.open(stats_filename_.c_str());
  stats_file << "frame_0001 0101\n";
  stats_file << "frame_0002 Barcode error\n";
  stats_file << "frame_0003 Barcode error\n";
  stats_file << "frame_0004 Barcode error\n";
  stats_file << "frame_0005 0107\n";
  stats_file << "frame_0006 0110\n";
  stats_file.close();

  PrintMaxRepeatedAndSkippedFrames(logfile, "NormalStatsFile",
                                   stats_filename_ref_, stats_filename_);
  ASSERT_EQ(0, fclose(logfile));

  std::vector<std::string> expected_out = {
      "RESULT Max_repeated: NormalStatsFile= 1",
      "RESULT Max_skipped: NormalStatsFile= 0",
      "RESULT Total_skipped: NormalStatsFile= 0",
      "RESULT Decode_errors_reference: NormalStatsFile= 0",
      "RESULT Decode_errors_test: NormalStatsFile= 3"};
  VerifyLogOutput(log_filename, expected_out);
}

TEST_F(VideoQualityAnalysisTest, CalculateFrameClustersOneValue) {
  std::ofstream stats_file;

  stats_file.open(stats_filename_.c_str());
  stats_file << "frame_0001 0101\n";
  stats_file.close();

  FILE* stats_filef = fopen(stats_filename_.c_str(), "r");
  ASSERT_TRUE(stats_filef != NULL);

  auto clusters = CalculateFrameClusters(stats_filef, nullptr);
  ASSERT_EQ(0, fclose(stats_filef));
  decltype(clusters) expected = {std::make_pair(101, 1)};
  ASSERT_EQ(expected, clusters);
}

TEST_F(VideoQualityAnalysisTest, CalculateFrameClustersOneOneTwo) {
  std::ofstream stats_file;

  stats_file.open(stats_filename_.c_str());
  stats_file << "frame_0001 0101\n";
  stats_file << "frame_0002 0101\n";
  stats_file << "frame_0003 0102\n";
  stats_file.close();

  FILE* stats_filef = fopen(stats_filename_.c_str(), "r");
  ASSERT_TRUE(stats_filef != NULL);

  auto clusters = CalculateFrameClusters(stats_filef, nullptr);
  ASSERT_EQ(0, fclose(stats_filef));
  decltype(clusters) expected = {std::make_pair(101, 2),
                                 std::make_pair(102, 1)};
  ASSERT_EQ(expected, clusters);
}

TEST_F(VideoQualityAnalysisTest, CalculateFrameClustersOneOneErrErrThree) {
  std::ofstream stats_file;

  stats_file.open(stats_filename_.c_str());
  stats_file << "frame_0001 0101\n";
  stats_file << "frame_0002 0101\n";
  stats_file << "frame_0003 Barcode error\n";
  stats_file << "frame_0004 Barcode error\n";
  stats_file << "frame_0005 0103\n";
  stats_file.close();

  FILE* stats_filef = fopen(stats_filename_.c_str(), "r");
  ASSERT_TRUE(stats_filef != NULL);

  auto clusters = CalculateFrameClusters(stats_filef, nullptr);
  ASSERT_EQ(0, fclose(stats_filef));
  decltype(clusters) expected = {std::make_pair(101, 2),
                                 std::make_pair(DECODE_ERROR, 2),
                                 std::make_pair(103, 1)};
  ASSERT_EQ(expected, clusters);
}

TEST_F(VideoQualityAnalysisTest, CalculateFrameClustersErrErr) {
  std::ofstream stats_file;

  stats_file.open(stats_filename_.c_str());
  stats_file << "frame_0001 Barcode error\n";
  stats_file << "frame_0002 Barcode error\n";
  stats_file.close();

  FILE* stats_filef = fopen(stats_filename_.c_str(), "r");
  ASSERT_TRUE(stats_filef != NULL);

  auto clusters = CalculateFrameClusters(stats_filef, nullptr);
  ASSERT_EQ(0, fclose(stats_filef));
  decltype(clusters) expected = {std::make_pair(DECODE_ERROR, 2)};
  ASSERT_EQ(expected, clusters);
}

TEST_F(VideoQualityAnalysisTest, CalculateFrameClustersOneOneErrErrOneOne) {
  std::ofstream stats_file;

  stats_file.open(stats_filename_.c_str());
  stats_file << "frame_0001 0101\n";
  stats_file << "frame_0002 0101\n";
  stats_file << "frame_0003 Barcode error\n";
  stats_file << "frame_0004 Barcode error\n";
  stats_file << "frame_0005 0101\n";
  stats_file << "frame_0006 0101\n";
  stats_file.close();

  FILE* stats_filef = fopen(stats_filename_.c_str(), "r");
  ASSERT_TRUE(stats_filef != NULL);

  auto clusters = CalculateFrameClusters(stats_filef, nullptr);
  ASSERT_EQ(0, fclose(stats_filef));
  decltype(clusters) expected = {std::make_pair(101, 6)};
  ASSERT_EQ(expected, clusters);
}

TEST_F(VideoQualityAnalysisTest, CalculateFrameClustersEmpty) {
  std::ofstream stats_file;

  stats_file.open(stats_filename_.c_str());
  stats_file.close();

  FILE* stats_filef = fopen(stats_filename_.c_str(), "r");
  ASSERT_TRUE(stats_filef != NULL);

  auto clusters = CalculateFrameClusters(stats_filef, nullptr);
  ASSERT_EQ(0, fclose(stats_filef));
  decltype(clusters) expected;
  ASSERT_EQ(expected, clusters);
}
}  // namespace test
}  // namespace webrtc
