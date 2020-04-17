//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// system_utils_unittest.cpp: Unit tests for ANGLE's system utility functions

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "common/system_utils.h"

using namespace angle;

namespace
{
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
}  // anonymous namespace
