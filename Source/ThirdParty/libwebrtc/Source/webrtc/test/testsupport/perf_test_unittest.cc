/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/testsupport/perf_test.h"

#include <algorithm>
#include <limits>
#include <string>

#include "test/gtest.h"
#include "test/testsupport/rtc_expect_death.h"

#if WEBRTC_ENABLE_PROTOBUF
#include "third_party/catapult/tracing/tracing/value/histogram.h"
namespace proto = catapult::tracing::tracing::proto;
#endif

namespace webrtc {
namespace test {

class PerfTest : public ::testing::Test {
 protected:
  void TearDown() override { ClearPerfResults(); }
};

#if defined(WEBRTC_IOS)
#define MAYBE_TestPrintResult DISABLED_TestPrintResult
#else
#define MAYBE_TestPrintResult TestPrintResult
#endif
TEST_F(PerfTest, MAYBE_TestPrintResult) {
  ::testing::internal::CaptureStdout();
  std::string expected;

  expected += "RESULT measurementmodifier: trace= 42 units\n";
  PrintResult("measurement", "modifier", "trace", 42, "units", false);

  expected += "*RESULT foobar: baz_v= 1423730 widgets\n";
  PrintResult("foo", "bar", "baz_v", 1423730, "widgets", true);

  expected += "RESULT foobar: baz_me= {1,2} lemurs\n";
  PrintResultMeanAndError("foo", "bar", "baz_me", 1, 2, "lemurs", false);

  const double kListOfScalars[] = {1, 2, 3};
  expected += "RESULT foobar: baz_vl= [1,2,3] units\n";
  PrintResultList("foo", "bar", "baz_vl", kListOfScalars, "units", false);

  EXPECT_EQ(expected, ::testing::internal::GetCapturedStdout());
}

TEST_F(PerfTest, TestClearPerfResults) {
  PrintResult("measurement", "modifier", "trace", 42, "units", false);
  ClearPerfResults();
  EXPECT_EQ("", GetPerfResults());
}

#if WEBRTC_ENABLE_PROTOBUF

TEST_F(PerfTest, TestGetPerfResultsHistograms) {
  PrintResult("measurement", "_modifier", "story_1", 42, "ms", false);
  PrintResult("foo", "bar", "story_1", 7, "sigma", true);
  // Note: the error will be ignored, not supported by histograms.
  PrintResultMeanAndError("foo", "bar", "story_1", 1, 2000, "sigma", false);
  const double kListOfScalars[] = {1, 2, 3};
  PrintResultList("foo", "bar", "story_1", kListOfScalars, "sigma", false);

  proto::HistogramSet histogram_set;
  EXPECT_TRUE(histogram_set.ParseFromString(GetPerfResults()))
      << "Expected valid histogram set";

  ASSERT_EQ(histogram_set.histograms_size(), 2)
      << "Should be two histograms: foobar and measurement_modifier";
  const proto::Histogram& hist1 = histogram_set.histograms(0);
  const proto::Histogram& hist2 = histogram_set.histograms(1);

  EXPECT_EQ(hist1.name(), "foobar");

  // Spot check some things in here (there's a more thorough test on the
  // histogram writer itself).
  EXPECT_EQ(hist1.unit().unit(), proto::SIGMA);
  EXPECT_EQ(hist1.sample_values_size(), 5);
  EXPECT_EQ(hist1.sample_values(0), 7);
  EXPECT_EQ(hist1.sample_values(1), 1);
  EXPECT_EQ(hist1.sample_values(2), 1);
  EXPECT_EQ(hist1.sample_values(3), 2);
  EXPECT_EQ(hist1.sample_values(4), 3);

  EXPECT_EQ(hist1.diagnostics().diagnostic_map().count("stories"), 1u);
  const proto::Diagnostic& stories =
      hist1.diagnostics().diagnostic_map().at("stories");
  ASSERT_EQ(stories.generic_set().values_size(), 1);
  EXPECT_EQ(stories.generic_set().values(0), "\"story_1\"");

  EXPECT_EQ(hist2.name(), "measurement_modifier");
  EXPECT_EQ(hist2.unit().unit(), proto::MS_BEST_FIT_FORMAT);
}

#endif  // WEBRTC_ENABLE_PROTOBUF

#if GTEST_HAS_DEATH_TEST
using PerfDeathTest = PerfTest;

TEST_F(PerfDeathTest, TestFiniteResultError) {
  const double kNan = std::numeric_limits<double>::quiet_NaN();
  const double kInf = std::numeric_limits<double>::infinity();

  RTC_EXPECT_DEATH(PrintResult("a", "b", "c", kNan, "d", false), "finit");
  RTC_EXPECT_DEATH(PrintResult("a", "b", "c", kInf, "d", false), "finit");

  RTC_EXPECT_DEATH(PrintResultMeanAndError("a", "b", "c", kNan, 1, "d", false),
                   "");
  RTC_EXPECT_DEATH(PrintResultMeanAndError("a", "b", "c", 1, kInf, "d", false),
                   "");

  const double kNanList[] = {kNan, kNan};
  RTC_EXPECT_DEATH(PrintResultList("a", "b", "c", kNanList, "d", false), "");
  const double kInfList[] = {0, kInf};
  RTC_EXPECT_DEATH(PrintResultList("a", "b", "c", kInfList, "d", false), "");
}
#endif

}  // namespace test
}  // namespace webrtc
