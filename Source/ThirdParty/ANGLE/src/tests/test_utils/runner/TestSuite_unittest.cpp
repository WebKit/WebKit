//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// TestSuite_unittest.cpp: Unit tests for ANGLE's test harness.
//

#include <gtest/gtest.h>

#include "../angle_test_instantiate.h"
#include "TestSuite.h"
#include "common/debug.h"
#include "common/system_utils.h"
#include "util/test_utils.h"
#include "util/test_utils_unittest_helper.h"

#include <rapidjson/document.h>

using namespace angle;

namespace js = rapidjson;

namespace
{
constexpr char kTestHelperExecutable[] = "test_utils_unittest_helper";

class TestSuiteTest : public testing::Test
{
  protected:
    void TearDown() override
    {
        if (!mTempFileName.empty())
        {
            angle::DeleteFile(mTempFileName.c_str());
        }
    }

    std::string mTempFileName;
};

// Tests the ANGLE standalone testing harness. Runs four tests with different ending conditions.
// Verifies that Pass, Fail, Crash and Timeout are all handled correctly.
TEST_F(TestSuiteTest, RunMockTests)
{
    std::string executablePath = GetExecutableDirectory();
    EXPECT_NE(executablePath, "");
    executablePath += std::string("/") + kTestHelperExecutable + GetExecutableExtension();

    constexpr uint32_t kMaxTempDirLen = 100;
    char tempFileName[kMaxTempDirLen * 2];
    ASSERT_TRUE(GetTempDir(tempFileName, kMaxTempDirLen));

    std::stringstream tempFNameStream;
    tempFNameStream << tempFileName << "/test_temp_" << rand() << ".json";
    mTempFileName = tempFNameStream.str();

    std::string resultsFileName = "--results-file=" + mTempFileName;

    std::vector<const char *> args = {executablePath.c_str(),
                                      kRunTestSuite,
                                      "--gtest_filter=MockTestSuiteTest.DISABLED_*",
                                      "--gtest_also_run_disabled_tests",
                                      "--bot-mode",
                                      "--test-timeout=10",
                                      resultsFileName.c_str()};

    ProcessHandle process(args, true, true);
    EXPECT_TRUE(process->started());
    EXPECT_TRUE(process->finish());
    EXPECT_TRUE(process->finished());
    EXPECT_EQ(process->getStderr(), "");

    TestResults actual;
    ASSERT_TRUE(GetTestResultsFromFile(mTempFileName.c_str(), &actual));
    EXPECT_TRUE(DeleteFile(mTempFileName.c_str()));
    mTempFileName.clear();

    std::map<TestIdentifier, TestResult> expectedResults = {
        {{"MockTestSuiteTest", "DISABLED_Pass"}, {TestResultType::Pass, 0.0}},
        {{"MockTestSuiteTest", "DISABLED_Fail"}, {TestResultType::Fail, 0.0}},
        {{"MockTestSuiteTest", "DISABLED_Timeout"}, {TestResultType::Timeout, 0.0}},
        // {{"MockTestSuiteTest", "DISABLED_Crash"}, {TestResultType::Crash, 0.0}},
    };

    EXPECT_EQ(expectedResults, actual.results);
}

// Normal passing test.
TEST(MockTestSuiteTest, DISABLED_Pass)
{
    EXPECT_TRUE(true);
}

// Normal failing test.
TEST(MockTestSuiteTest, DISABLED_Fail)
{
    EXPECT_TRUE(false);
}

// Trigger a test timeout.
TEST(MockTestSuiteTest, DISABLED_Timeout)
{
    angle::Sleep(30000);
}

// Trigger a test crash.
// TEST(MockTestSuiteTest, DISABLED_Crash)
// {
//     ANGLE_CRASH();
// }
}  // namespace
