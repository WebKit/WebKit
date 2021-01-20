/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/perf_test_histogram_writer.h"

#include <memory>
#include <string>

#include "test/gtest.h"
#include "third_party/catapult/tracing/tracing/value/histogram.h"

namespace webrtc {
namespace test {

namespace proto = catapult::tracing::tracing::proto;

TEST(PerfHistogramWriterUnittest, TestSimpleHistogram) {
  std::unique_ptr<PerfTestResultWriter> writer =
      std::unique_ptr<PerfTestResultWriter>(CreateHistogramWriter());

  writer->LogResult("-", "-", 0, "ms", false, ImproveDirection::kNone);

  proto::HistogramSet histogram_set;
  EXPECT_TRUE(histogram_set.ParseFromString(writer->Serialize()))
      << "Expected valid histogram set";

  ASSERT_EQ(histogram_set.histograms_size(), 1);
}

TEST(PerfHistogramWriterUnittest, WritesSamplesAndUserStory) {
  std::unique_ptr<PerfTestResultWriter> writer =
      std::unique_ptr<PerfTestResultWriter>(CreateHistogramWriter());

  writer->LogResult("measurement", "user_story", 15e7, "Hz", false,
                    ImproveDirection::kBiggerIsBetter);

  proto::HistogramSet histogram_set;
  histogram_set.ParseFromString(writer->Serialize());
  const proto::Histogram& hist1 = histogram_set.histograms(0);

  EXPECT_EQ(hist1.name(), "measurement");

  EXPECT_EQ(hist1.unit().unit(), proto::HERTZ);
  EXPECT_EQ(hist1.unit().improvement_direction(), proto::BIGGER_IS_BETTER);

  EXPECT_EQ(hist1.sample_values_size(), 1);
  EXPECT_EQ(hist1.sample_values(0), 15e7);

  EXPECT_EQ(hist1.diagnostics().diagnostic_map().count("stories"), 1u);
  const proto::Diagnostic& stories =
      hist1.diagnostics().diagnostic_map().at("stories");
  ASSERT_EQ(stories.generic_set().values_size(), 1);
  EXPECT_EQ(stories.generic_set().values(0), "\"user_story\"");
}

TEST(PerfHistogramWriterUnittest, WritesOneHistogramPerMeasurementAndStory) {
  std::unique_ptr<PerfTestResultWriter> writer =
      std::unique_ptr<PerfTestResultWriter>(CreateHistogramWriter());

  writer->LogResult("measurement", "story1", 1, "ms", false,
                    ImproveDirection::kNone);
  writer->LogResult("measurement", "story1", 2, "ms", false,
                    ImproveDirection::kNone);
  writer->LogResult("measurement", "story2", 2, "ms", false,
                    ImproveDirection::kNone);

  proto::HistogramSet histogram_set;
  histogram_set.ParseFromString(writer->Serialize());
  ASSERT_EQ(histogram_set.histograms_size(), 2);

  const proto::Histogram& hist1 = histogram_set.histograms(0);
  const proto::Histogram& hist2 = histogram_set.histograms(1);

  EXPECT_EQ(hist1.name(), "measurement");
  EXPECT_EQ(hist2.name(), "measurement");

  const proto::Diagnostic& stories1 =
      hist1.diagnostics().diagnostic_map().at("stories");
  EXPECT_EQ(stories1.generic_set().values(0), "\"story1\"");
  EXPECT_EQ(hist1.sample_values_size(), 2);

  const proto::Diagnostic& stories2 =
      hist2.diagnostics().diagnostic_map().at("stories");
  EXPECT_EQ(stories2.generic_set().values(0), "\"story2\"");
  EXPECT_EQ(hist2.sample_values_size(), 1);
}

TEST(PerfHistogramWriterUnittest, IgnoresError) {
  std::unique_ptr<PerfTestResultWriter> writer =
      std::unique_ptr<PerfTestResultWriter>(CreateHistogramWriter());

  writer->LogResultMeanAndError("-", "-", 17, 12345, "ms", false,
                                ImproveDirection::kNone);

  proto::HistogramSet histogram_set;
  histogram_set.ParseFromString(writer->Serialize());
  const proto::Histogram& hist1 = histogram_set.histograms(0);

  EXPECT_EQ(hist1.running().mean(), 17);
  EXPECT_EQ(hist1.running().variance(), 0) << "The error should be ignored.";
}

TEST(PerfHistogramWriterUnittest, WritesDecibelIntoMeasurementName) {
  std::unique_ptr<PerfTestResultWriter> writer =
      std::unique_ptr<PerfTestResultWriter>(CreateHistogramWriter());

  writer->LogResult("measurement", "-", 0, "dB", false,
                    ImproveDirection::kNone);

  proto::HistogramSet histogram_set;
  histogram_set.ParseFromString(writer->Serialize());
  const proto::Histogram& hist1 = histogram_set.histograms(0);

  EXPECT_EQ(hist1.unit().unit(), proto::UNITLESS)
      << "dB should map to unitless";
  EXPECT_EQ(hist1.name(), "measurement_dB") << "measurement should be renamed";
}

TEST(PerfHistogramWriterUnittest, WritesFpsIntoMeasurementName) {
  std::unique_ptr<PerfTestResultWriter> writer =
      std::unique_ptr<PerfTestResultWriter>(CreateHistogramWriter());

  writer->LogResult("measurement", "-", 0, "fps", false,
                    ImproveDirection::kNone);

  proto::HistogramSet histogram_set;
  histogram_set.ParseFromString(writer->Serialize());
  const proto::Histogram& hist1 = histogram_set.histograms(0);

  EXPECT_EQ(hist1.unit().unit(), proto::HERTZ) << "fps should map to hertz";
  EXPECT_EQ(hist1.name(), "measurement_fps") << "measurement should be renamed";
}

TEST(PerfHistogramWriterUnittest, WritesPercentIntoMeasurementName) {
  std::unique_ptr<PerfTestResultWriter> writer =
      std::unique_ptr<PerfTestResultWriter>(CreateHistogramWriter());

  writer->LogResult("measurement", "-", 0, "%", false, ImproveDirection::kNone);

  proto::HistogramSet histogram_set;
  histogram_set.ParseFromString(writer->Serialize());
  const proto::Histogram& hist1 = histogram_set.histograms(0);

  EXPECT_EQ(hist1.unit().unit(), proto::UNITLESS)
      << "percent should map to hertz";
  EXPECT_EQ(hist1.name(), "measurement_%") << "measurement should be renamed";
}

TEST(PerfHistogramWriterUnittest, BitsPerSecondIsConvertedToBytes) {
  std::unique_ptr<PerfTestResultWriter> writer =
      std::unique_ptr<PerfTestResultWriter>(CreateHistogramWriter());

  writer->LogResult("-", "-", 1024, "bps", false, ImproveDirection::kNone);

  proto::HistogramSet histogram_set;
  histogram_set.ParseFromString(writer->Serialize());
  const proto::Histogram& hist1 = histogram_set.histograms(0);

  EXPECT_EQ(hist1.sample_values(0), 128) << "1024 bits = 128 bytes";
}

TEST(PerfHistogramWriterUnittest, ParsesDirection) {
  std::unique_ptr<PerfTestResultWriter> writer =
      std::unique_ptr<PerfTestResultWriter>(CreateHistogramWriter());

  writer->LogResult("measurement1", "-", 0, "bps", false,
                    ImproveDirection::kBiggerIsBetter);
  writer->LogResult("measurement2", "-", 0, "frames", false,
                    ImproveDirection::kSmallerIsBetter);
  writer->LogResult("measurement3", "-", 0, "sigma", false,
                    ImproveDirection::kNone);

  proto::HistogramSet histogram_set;
  histogram_set.ParseFromString(writer->Serialize());
  const proto::Histogram& hist1 = histogram_set.histograms(0);
  const proto::Histogram& hist2 = histogram_set.histograms(1);
  const proto::Histogram& hist3 = histogram_set.histograms(2);

  EXPECT_EQ(hist1.unit().unit(), proto::BYTES_PER_SECOND);
  EXPECT_EQ(hist1.unit().improvement_direction(), proto::BIGGER_IS_BETTER);

  EXPECT_EQ(hist2.unit().unit(), proto::COUNT);
  EXPECT_EQ(hist2.unit().improvement_direction(), proto::SMALLER_IS_BETTER);

  EXPECT_EQ(hist3.unit().unit(), proto::SIGMA);
  EXPECT_EQ(hist3.unit().improvement_direction(), proto::NOT_SPECIFIED);
}

}  // namespace test
}  // namespace webrtc
