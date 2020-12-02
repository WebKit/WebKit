//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// test_utils_unittest.cpp: Unit tests for ANGLE's test utility functions

#include "gtest/gtest.h"

#include "common/system_utils.h"
#include "util/Timer.h"
#include "util/test_utils.h"
#include "util/test_utils_unittest_helper.h"

using namespace angle;

namespace
{
#if defined(ANGLE_PLATFORM_WINDOWS)
constexpr char kRunAppHelperExecutable[] = "test_utils_unittest_helper.exe";
#else
constexpr char kRunAppHelperExecutable[] = "test_utils_unittest_helper";
#endif

// Transforms various line endings into C/Unix line endings:
//
// - A\nB -> A\nB
// - A\rB -> A\nB
// - A\r\nB -> A\nB
std::string NormalizeNewLines(const std::string &str)
{
    std::string result;

    for (size_t i = 0; i < str.size(); ++i)
    {
        if (str[i] == '\r')
        {
            if (i + 1 < str.size() && str[i + 1] == '\n')
            {
                ++i;
            }
            result += '\n';
        }
        else
        {
            result += str[i];
        }
    }

    return result;
}

// Tests that Sleep() actually waits some time.
TEST(TestUtils, Sleep)
{
    Timer timer;
    timer.start();
    angle::Sleep(500);
    timer.stop();

    // Use a slightly fuzzy range
    EXPECT_GT(timer.getElapsedTime(), 0.48);
}

constexpr uint32_t kMaxPath = 1000;

// Temporary file creation is not supported on Android right now.
#if defined(ANGLE_PLATFORM_ANDROID)
#    define MAYBE_CreateAndDeleteTemporaryFile DISABLED_CreateAndDeleteTemporaryFile
#    define MAYBE_CreateAndDeleteFileInTempDir DISABLED_CreateAndDeleteFileInTempDir
#else
#    define MAYBE_CreateAndDeleteTemporaryFile CreateAndDeleteTemporaryFile
#    define MAYBE_CreateAndDeleteFileInTempDir CreateAndDeleteFileInTempDir
#endif  // defined(ANGLE_PLATFORM_ANDROID)

// Test creating and deleting temporary file.
TEST(TestUtils, MAYBE_CreateAndDeleteTemporaryFile)
{
    char path[kMaxPath] = {};
    ASSERT_TRUE(CreateTemporaryFile(path, kMaxPath));
    ASSERT_TRUE(strlen(path) > 0);

    const char kOutputString[] = "test output";

    FILE *fp = fopen(path, "wt");
    ASSERT_NE(fp, nullptr);
    int retval = fputs(kOutputString, fp);
    fclose(fp);

    EXPECT_GE(retval, 0);

    // Test ReadEntireFileToString
    char actualString[kMaxPath];
    EXPECT_TRUE(ReadEntireFileToString(path, actualString, kMaxPath));
    EXPECT_EQ(strcmp(actualString, kOutputString), 0);

    // Delete the temporary file.
    EXPECT_TRUE(angle::DeleteFile(path));
}

// Tests creating and deleting a file in the system temp dir.
TEST(TestUtils, MAYBE_CreateAndDeleteFileInTempDir)
{
    char tempDir[kMaxPath];
    ASSERT_TRUE(GetTempDir(tempDir, kMaxPath));

    char path[kMaxPath] = {};
    ASSERT_TRUE(CreateTemporaryFileInDir(tempDir, path, kMaxPath));
    ASSERT_TRUE(strlen(path) > 0);

    const char kOutputString[] = "test output";

    FILE *fp = fopen(path, "wt");
    ASSERT_NE(fp, nullptr);
    int retval = fputs(kOutputString, fp);
    fclose(fp);

    EXPECT_GE(retval, 0);

    // Test ReadEntireFileToString
    char actualString[kMaxPath];
    EXPECT_TRUE(ReadEntireFileToString(path, actualString, kMaxPath));
    EXPECT_EQ(strcmp(actualString, kOutputString), 0);

    // Delete the temporary file.
    EXPECT_TRUE(angle::DeleteFile(path));
}

// TODO: android support. http://anglebug.com/3125
#if defined(ANGLE_PLATFORM_ANDROID)
#    define MAYBE_RunApp DISABLED_RunApp
#    define MAYBE_RunAppAsync DISABLED_RunAppAsync
// TODO: fuchsia support. http://anglebug.com/3161
#elif defined(ANGLE_PLATFORM_FUCHSIA)
#    define MAYBE_RunApp DISABLED_RunApp
#    define MAYBE_RunAppAsync DISABLED_RunAppAsync
#else
#    define MAYBE_RunApp RunApp
#    define MAYBE_RunAppAsync RunAppAsync
#endif  // defined(ANGLE_PLATFORM_ANDROID)

// Test running an external application and receiving its output
TEST(TestUtils, MAYBE_RunApp)
{
    std::string executablePath = GetExecutableDirectory();
    EXPECT_NE(executablePath, "");
    executablePath += "/";
    executablePath += kRunAppHelperExecutable;

    std::vector<const char *> args = {executablePath.c_str(), kRunAppTestArg1, kRunAppTestArg2};

    // Test that the application can be executed.
    {
        ProcessHandle process(args, true, true);
        EXPECT_TRUE(process->started());
        EXPECT_TRUE(process->finish());
        EXPECT_TRUE(process->finished());

        EXPECT_GT(process->getElapsedTimeSeconds(), 0.0);
        EXPECT_EQ(kRunAppTestStdout, NormalizeNewLines(process->getStdout()));
        EXPECT_EQ(kRunAppTestStderr, NormalizeNewLines(process->getStderr()));
        EXPECT_EQ(EXIT_SUCCESS, process->getExitCode());
    }

    // Test that environment variables reach the child.
    {
        bool setEnvDone = SetEnvironmentVar(kRunAppTestEnvVarName, kRunAppTestEnvVarValue);
        EXPECT_TRUE(setEnvDone);

        ProcessHandle process(LaunchProcess(args, true, true));
        EXPECT_TRUE(process->started());
        EXPECT_TRUE(process->finish());

        EXPECT_GT(process->getElapsedTimeSeconds(), 0.0);
        EXPECT_EQ("", process->getStdout());
        EXPECT_EQ(kRunAppTestEnvVarValue, NormalizeNewLines(process->getStderr()));
        EXPECT_EQ(EXIT_SUCCESS, process->getExitCode());

        // Unset environment var.
        SetEnvironmentVar(kRunAppTestEnvVarName, "");
    }
}

// Test running an external application and receiving its output asynchronously.
TEST(TestUtils, MAYBE_RunAppAsync)
{
    std::string executablePath = GetExecutableDirectory();
    EXPECT_NE(executablePath, "");
    executablePath += "/";
    executablePath += kRunAppHelperExecutable;

    std::vector<const char *> args = {executablePath.c_str(), kRunAppTestArg1, kRunAppTestArg2};

    // Test that the application can be executed.
    {
        ProcessHandle process(args, true, true);
        EXPECT_TRUE(process->started());

        constexpr double kTimeout = 3.0;

        Timer timer;
        timer.start();
        while (!process->finished() && timer.getElapsedTime() < kTimeout)
        {
            angle::Sleep(1);
        }

        EXPECT_TRUE(process->finished());
        EXPECT_GT(process->getElapsedTimeSeconds(), 0.0);
        EXPECT_EQ(kRunAppTestStdout, NormalizeNewLines(process->getStdout()));
        EXPECT_EQ(kRunAppTestStderr, NormalizeNewLines(process->getStderr()));
        EXPECT_EQ(EXIT_SUCCESS, process->getExitCode());
    }
}

// Verify that NumberOfProcessors returns something reasonable.
TEST(TestUtils, NumberOfProcessors)
{
    int numProcs = angle::NumberOfProcessors();
    EXPECT_GT(numProcs, 0);
    EXPECT_LT(numProcs, 1000);
}
}  // namespace
