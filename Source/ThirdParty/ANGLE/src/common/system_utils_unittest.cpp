//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// system_utils_unittest.cpp: Unit tests for ANGLE's system utility functions

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "common/system_utils.h"
#include "common/system_utils_unittest_helper.h"

using namespace angle;

namespace
{
#if defined(ANGLE_PLATFORM_WINDOWS)
constexpr char kRunAppHelperExecutable[] = "angle_unittests_helper.exe";
#else
constexpr char kRunAppHelperExecutable[] = "angle_unittests_helper";
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

// Test getting the executable path
TEST(SystemUtils, ExecutablePath)
{
#if defined(ANGLE_PLATFORM_FUCHSIA)
    // TODO: fuchsia support. http://anglebug.com/3161
    return;
#endif

    std::string executablePath = GetExecutablePath();
    EXPECT_NE("", executablePath);
}

// Test getting the executable directory
TEST(SystemUtils, ExecutableDir)
{
#if defined(ANGLE_PLATFORM_FUCHSIA)
    // TODO: fuchsia support. http://anglebug.com/3161
    return;
#endif

    std::string executableDir = GetExecutableDirectory();
    EXPECT_NE("", executableDir);

    std::string executablePath = GetExecutablePath();
    EXPECT_LT(executableDir.size(), executablePath.size());
    EXPECT_EQ(0, strncmp(executableDir.c_str(), executablePath.c_str(), executableDir.size()));
}

// Test setting environment variables
TEST(SystemUtils, Environment)
{
    constexpr char kEnvVarName[]  = "UNITTEST_ENV_VARIABLE";
    constexpr char kEnvVarValue[] = "The quick brown fox jumps over the lazy dog";

    bool setEnvDone = SetEnvironmentVar(kEnvVarName, kEnvVarValue);
    EXPECT_TRUE(setEnvDone);

    std::string readback = GetEnvironmentVar(kEnvVarName);
    EXPECT_EQ(kEnvVarValue, readback);

    bool unsetEnvDone = UnsetEnvironmentVar(kEnvVarName);
    EXPECT_TRUE(unsetEnvDone);

    readback = GetEnvironmentVar(kEnvVarName);
    EXPECT_EQ("", readback);
}

// Test running an external application and receiving its output
TEST(SystemUtils, RunApp)
{
#if defined(ANGLE_PLATFORM_ANDROID)
    // TODO: android support. http://anglebug.com/3125
    return;
#endif

#if defined(ANGLE_PLATFORM_FUCHSIA)
    // TODO: fuchsia support. http://anglebug.com/3161
    return;
#endif

    std::string executablePath = GetExecutableDirectory();
    EXPECT_NE(executablePath, "");
    executablePath += "/";
    executablePath += kRunAppHelperExecutable;

    std::vector<const char *> args = {executablePath.c_str(), kRunAppTestArg1, kRunAppTestArg2,
                                      nullptr};

    std::string stdoutOutput;
    std::string stderrOutput;
    int exitCode = EXIT_FAILURE;

    // Test that the application can be executed.
    bool ranApp = RunApp(args, &stdoutOutput, &stderrOutput, &exitCode);
    EXPECT_TRUE(ranApp);
    EXPECT_EQ(kRunAppTestStdout, NormalizeNewLines(stdoutOutput));
    EXPECT_EQ(kRunAppTestStderr, NormalizeNewLines(stderrOutput));
    EXPECT_EQ(EXIT_SUCCESS, exitCode);

    // Test that environment variables reach the cild.
    bool setEnvDone = SetEnvironmentVar(kRunAppTestEnvVarName, kRunAppTestEnvVarValue);
    EXPECT_TRUE(setEnvDone);

    ranApp = RunApp(args, &stdoutOutput, &stderrOutput, &exitCode);
    EXPECT_TRUE(ranApp);
    EXPECT_EQ("", stdoutOutput);
    EXPECT_EQ(kRunAppTestEnvVarValue, NormalizeNewLines(stderrOutput));
    EXPECT_EQ(EXIT_SUCCESS, exitCode);
}

}  // anonymous namespace
