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

}  // namespace

// Setup a log file to write the output to instead of stdout because we don't
// want those numbers to be picked up as perf numbers.
class VideoQualityAnalysisTest : public ::testing::Test {
 protected:
  void SetUp() {
    std::string log_filename = TempFilename(webrtc::test::OutputPath(),
                                            "VideoQualityAnalysisTest.log");
    logfile_ = fopen(log_filename.c_str(), "w");
    ASSERT_TRUE(logfile_ != NULL);
  }
  void TearDown() { ASSERT_EQ(0, fclose(logfile_)); }
  FILE* logfile_;
};

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

TEST_F(VideoQualityAnalysisTest,
       PrintMaxRepeatedAndSkippedFramesSkippedFrames) {
  ResultsContainer result;

  std::string log_filename =
      TempFilename(webrtc::test::OutputPath(), "log.log");
  FILE* logfile = fopen(log_filename.c_str(), "w");
  ASSERT_TRUE(logfile != NULL);

  result.max_repeated_frames = 2;
  result.max_skipped_frames = 2;
  result.total_skipped_frames = 3;
  result.decode_errors_ref = 0;
  result.decode_errors_test = 0;

  PrintAnalysisResults(logfile, "NormalStatsFile", &result);
  ASSERT_EQ(0, fclose(logfile));

  std::vector<std::string> expected_out = {
      "RESULT Max_repeated: NormalStatsFile= 2 ",
      "RESULT Max_skipped: NormalStatsFile= 2 ",
      "RESULT Total_skipped: NormalStatsFile= 3 ",
      "RESULT Decode_errors_reference: NormalStatsFile= 0 ",
      "RESULT Decode_errors_test: NormalStatsFile= 0 "};
  VerifyLogOutput(log_filename, expected_out);
}

TEST_F(VideoQualityAnalysisTest,
       PrintMaxRepeatedAndSkippedFramesDecodeErrorInTest) {
  ResultsContainer result;

  std::string log_filename =
      TempFilename(webrtc::test::OutputPath(), "log.log");
  FILE* logfile = fopen(log_filename.c_str(), "w");
  ASSERT_TRUE(logfile != NULL);

  result.max_repeated_frames = 1;
  result.max_skipped_frames = 0;
  result.total_skipped_frames = 0;
  result.decode_errors_ref = 0;
  result.decode_errors_test = 3;
  PrintAnalysisResults(logfile, "NormalStatsFile", &result);
  ASSERT_EQ(0, fclose(logfile));

  std::vector<std::string> expected_out = {
      "RESULT Max_repeated: NormalStatsFile= 1 ",
      "RESULT Max_skipped: NormalStatsFile= 0 ",
      "RESULT Total_skipped: NormalStatsFile= 0 ",
      "RESULT Decode_errors_reference: NormalStatsFile= 0 ",
      "RESULT Decode_errors_test: NormalStatsFile= 3 "};
  VerifyLogOutput(log_filename, expected_out);
}

TEST_F(VideoQualityAnalysisTest, CalculateFrameClustersOneValue) {
  const std::vector<Cluster> result = CalculateFrameClusters({1});
  EXPECT_EQ(1u, result.size());
  EXPECT_EQ(1u, result[0].index);
  EXPECT_EQ(1, result[0].number_of_repeated_frames);
}

TEST_F(VideoQualityAnalysisTest, GetMaxRepeatedFramesOneValue) {
  EXPECT_EQ(1, GetMaxRepeatedFrames(CalculateFrameClusters({1})));
}

TEST_F(VideoQualityAnalysisTest, GetMaxSkippedFramesOneValue) {
  EXPECT_EQ(0, GetMaxSkippedFrames(CalculateFrameClusters({1})));
}

TEST_F(VideoQualityAnalysisTest, GetTotalNumberOfSkippedFramesOneValue) {
  EXPECT_EQ(0, GetTotalNumberOfSkippedFrames(CalculateFrameClusters({1})));
}

TEST_F(VideoQualityAnalysisTest, CalculateFrameClustersOneOneTwo) {
  const std::vector<Cluster> result = CalculateFrameClusters({1, 1, 2});
  EXPECT_EQ(2u, result.size());
  EXPECT_EQ(1u, result[0].index);
  EXPECT_EQ(2, result[0].number_of_repeated_frames);
  EXPECT_EQ(2u, result[1].index);
  EXPECT_EQ(1, result[1].number_of_repeated_frames);
}

TEST_F(VideoQualityAnalysisTest, GetMaxRepeatedFramesOneOneTwo) {
  EXPECT_EQ(2, GetMaxRepeatedFrames(CalculateFrameClusters({1, 1, 2})));
}

TEST_F(VideoQualityAnalysisTest, GetMaxSkippedFramesOneOneTwo) {
  EXPECT_EQ(0, GetMaxSkippedFrames(CalculateFrameClusters({1, 1, 2})));
}

TEST_F(VideoQualityAnalysisTest, GetTotalNumberOfSkippedFramesOneOneTwo) {
  EXPECT_EQ(0,
            GetTotalNumberOfSkippedFrames(CalculateFrameClusters({1, 1, 2})));
}

TEST_F(VideoQualityAnalysisTest, CalculateFrameClustersEmpty) {
  EXPECT_TRUE(CalculateFrameClusters({}).empty());
}

TEST_F(VideoQualityAnalysisTest, GetMaxRepeatedFramesEmpty) {
  EXPECT_EQ(0, GetMaxRepeatedFrames({}));
}

TEST_F(VideoQualityAnalysisTest, GetMaxSkippedFramesEmpty) {
  EXPECT_EQ(0, GetMaxSkippedFrames({}));
}

TEST_F(VideoQualityAnalysisTest, GetTotalNumberOfSkippedFramesEmpty) {
  EXPECT_EQ(0, GetTotalNumberOfSkippedFrames({}));
}

}  // namespace test
}  // namespace webrtc
