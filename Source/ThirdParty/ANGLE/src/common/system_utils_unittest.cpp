//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// system_utils_unittest.cpp: Unit tests for ANGLE's system utility functions

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "common/system_utils.h"
#include "util/test_utils.h"

#include <vector>

using namespace angle;

namespace
{
// Test getting the executable path
TEST(SystemUtils, ExecutablePath)
{
    // TODO: fuchsia support. http://anglebug.com/3161
#if !defined(ANGLE_PLATFORM_FUCHSIA)
    std::string executablePath = GetExecutablePath();
    EXPECT_NE("", executablePath);
#endif
}

// Test getting the executable directory
TEST(SystemUtils, ExecutableDir)
{
    // TODO: fuchsia support. http://anglebug.com/3161
#if !defined(ANGLE_PLATFORM_FUCHSIA)
    std::string executableDir = GetExecutableDirectory();
    EXPECT_NE("", executableDir);

    std::string executablePath = GetExecutablePath();
    EXPECT_LT(executableDir.size(), executablePath.size());
    EXPECT_EQ(0, strncmp(executableDir.c_str(), executablePath.c_str(), executableDir.size()));
#endif
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

// Test CPU time measurement with a small operation
// (pretty much the measurement itself)
TEST(SystemUtils, CpuTimeSmallOp)
{
    double cpuTimeStart = GetCurrentProcessCpuTime();
    double cpuTimeEnd   = GetCurrentProcessCpuTime();
    EXPECT_GE(cpuTimeEnd, cpuTimeStart);
}

// Test CPU time measurement with a sleepy operation
TEST(SystemUtils, CpuTimeSleepy)
{
    double wallTimeStart = GetCurrentSystemTime();
    double cpuTimeStart  = GetCurrentProcessCpuTime();
    angle::Sleep(1);
    double cpuTimeEnd  = GetCurrentProcessCpuTime();
    double wallTimeEnd = GetCurrentSystemTime();
    EXPECT_GE(cpuTimeEnd, cpuTimeStart);
    EXPECT_GE(wallTimeEnd - wallTimeStart, cpuTimeEnd - cpuTimeStart);
}

// Test CPU time measurement with a heavy operation
TEST(SystemUtils, CpuTimeHeavyOp)
{
    constexpr size_t bufferSize = 1048576;
    std::vector<uint8_t> buffer(bufferSize, 1);
    double cpuTimeStart = GetCurrentProcessCpuTime();
    memset(buffer.data(), 0, bufferSize);
    double cpuTimeEnd = GetCurrentProcessCpuTime();
    EXPECT_GE(cpuTimeEnd, cpuTimeStart);
}

#if defined(ANGLE_PLATFORM_POSIX)
TEST(SystemUtils, ConcatenatePathSimple)
{
    std::string path1    = "/this/is/path1";
    std::string path2    = "this/is/path2";
    std::string expected = "/this/is/path1/this/is/path2";
    EXPECT_EQ(ConcatenatePath(path1, path2), expected);
}

TEST(SystemUtils, ConcatenatePath1Empty)
{
    std::string path1    = "";
    std::string path2    = "this/is/path2";
    std::string expected = "this/is/path2";
    EXPECT_EQ(ConcatenatePath(path1, path2), expected);
}

TEST(SystemUtils, ConcatenatePath2Empty)
{
    std::string path1    = "/this/is/path1";
    std::string path2    = "";
    std::string expected = "/this/is/path1";
    EXPECT_EQ(ConcatenatePath(path1, path2), expected);
}

TEST(SystemUtils, ConcatenatePath2FullPath)
{
    std::string path1    = "/this/is/path1";
    std::string path2    = "/this/is/path2";
    std::string expected = "/this/is/path2";
    EXPECT_EQ(ConcatenatePath(path1, path2), expected);
}

TEST(SystemUtils, ConcatenatePathRedundantSeparators)
{
    std::string path1    = "/this/is/path1/";
    std::string path2    = "this/is/path2";
    std::string expected = "/this/is/path1/this/is/path2";
    EXPECT_EQ(ConcatenatePath(path1, path2), expected);
}

TEST(SystemUtils, IsFullPath)
{
    std::string path1 = "/this/is/path1/";
    std::string path2 = "this/is/path2";
    EXPECT_TRUE(IsFullPath(path1));
    EXPECT_FALSE(IsFullPath(path2));
}
#elif defined(ANGLE_PLATFORM_WINDOWS)
TEST(SystemUtils, ConcatenatePathSimple)
{
    std::string path1    = "C:\\this\\is\\path1";
    std::string path2    = "this\\is\\path2";
    std::string expected = "C:\\this\\is\\path1\\this\\is\\path2";
    EXPECT_EQ(ConcatenatePath(path1, path2), expected);
}

TEST(SystemUtils, ConcatenatePath1Empty)
{
    std::string path1    = "";
    std::string path2    = "this\\is\\path2";
    std::string expected = "this\\is\\path2";
    EXPECT_EQ(ConcatenatePath(path1, path2), expected);
}

TEST(SystemUtils, ConcatenatePath2Empty)
{
    std::string path1    = "C:\\this\\is\\path1";
    std::string path2    = "";
    std::string expected = "C:\\this\\is\\path1";
    EXPECT_EQ(ConcatenatePath(path1, path2), expected);
}

TEST(SystemUtils, ConcatenatePath2FullPath)
{
    std::string path1    = "C:\\this\\is\\path1";
    std::string path2    = "C:\\this\\is\\path2";
    std::string expected = "C:\\this\\is\\path2";
    EXPECT_EQ(ConcatenatePath(path1, path2), expected);
}

TEST(SystemUtils, ConcatenatePathRedundantSeparators)
{
    std::string path1    = "C:\\this\\is\\path1\\";
    std::string path2    = "this\\is\\path2";
    std::string expected = "C:\\this\\is\\path1\\this\\is\\path2";
    EXPECT_EQ(ConcatenatePath(path1, path2), expected);
}

TEST(SystemUtils, ConcatenatePathRedundantSeparators2)
{
    std::string path1    = "C:\\this\\is\\path1\\";
    std::string path2    = "\\this\\is\\path2";
    std::string expected = "C:\\this\\is\\path1\\this\\is\\path2";
    EXPECT_EQ(ConcatenatePath(path1, path2), expected);
}

TEST(SystemUtils, ConcatenatePathRedundantSeparators3)
{
    std::string path1    = "C:\\this\\is\\path1";
    std::string path2    = "\\this\\is\\path2";
    std::string expected = "C:\\this\\is\\path1\\this\\is\\path2";
    EXPECT_EQ(ConcatenatePath(path1, path2), expected);
}

TEST(SystemUtils, IsFullPath)
{
    std::string path1 = "C:\\this\\is\\path1\\";
    std::string path2 = "this\\is\\path2";
    EXPECT_TRUE(IsFullPath(path1));
    EXPECT_FALSE(IsFullPath(path2));
}
#endif

}  // anonymous namespace
