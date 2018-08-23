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

#include <string>

#include "test/gtest.h"

namespace {

const char* kJsonExpected = R"({
  "format_version":"1.0",
  "charts":{
    "foobar":{
      "baz_v":{
        "type":"scalar",
        "value":7,
        "units":"widgets"
      },
      "baz_me":{
        "type":"list_of_scalar_values",
        "values":[1],
        "std":2,
        "units":"lemurs"
      },
      "baz_vl":{
        "type":"list_of_scalar_values",
        "values":[1,2,3],
        "units":"units"
      }
    },
    "measurementmodifier":{
      "trace":{
        "type":"scalar",
        "value":42,
        "units":"units"
      }
    }
  }
})";

std::string RemoveSpaces(std::string s) {
  s.erase(std::remove(s.begin(), s.end(), ' '), s.end());
  s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
  return s;
}

}  // namespace

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
  testing::internal::CaptureStdout();
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

  EXPECT_EQ(expected, testing::internal::GetCapturedStdout());
}

TEST_F(PerfTest, TestGetPerfResultsJSON) {
  PrintResult("measurement", "modifier", "trace", 42, "units", false);
  PrintResult("foo", "bar", "baz_v", 7, "widgets", true);
  PrintResultMeanAndError("foo", "bar", "baz_me", 1, 2, "lemurs", false);
  const double kListOfScalars[] = {1, 2, 3};
  PrintResultList("foo", "bar", "baz_vl", kListOfScalars, "units", false);

  EXPECT_EQ(RemoveSpaces(kJsonExpected), GetPerfResultsJSON());
}

TEST_F(PerfTest, TestClearPerfResults) {
  PrintResult("measurement", "modifier", "trace", 42, "units", false);
  ClearPerfResults();
  EXPECT_EQ(R"({"format_version":"1.0","charts":{}})", GetPerfResultsJSON());
}

}  // namespace test
}  // namespace webrtc
