//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ANGLETestCL:
//   Implementation of ANGLE CL testing fixture.
//

#include "ANGLETestCL.h"

#include <algorithm>
#include <cstdlib>
#include "common/PackedEnums.h"
#include "common/platform.h"
#include "gpu_info_util/SystemInfo.h"
#include "test_expectations/GPUTestConfig.h"
#include "util/EGLWindow.h"
#include "util/OSWindow.h"
#include "util/random_utils.h"
#include "util/test_utils.h"

#if defined(ANGLE_PLATFORM_WINDOWS)
#    include <VersionHelpers.h>
#endif  // defined(ANGLE_PLATFORM_WINDOWS)

#if defined(ANGLE_HAS_RAPIDJSON)
#    include "test_utils/runner/TestSuite.h"
#endif  // defined(ANGLE_HAS_RAPIDJSON)

using namespace angle;

template <>
ANGLETestCL<PlatformParameters>::ANGLETestCL(const PlatformParameters &params)
    : mSetUpCalled(false), mIsSetUp(false), mTearDownCalled(false)
{
    // Override the default platform methods with the ANGLE test methods pointer.
    mCurrentParams                               = params;
    mCurrentParams.eglParameters.platformMethods = &gDefaultPlatformMethods;

    if (mCurrentParams.getRenderer() == EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE)
    {
#if defined(ANGLE_ENABLE_VULKAN_VALIDATION_LAYERS)
        mCurrentParams.eglParameters.debugLayersEnabled = EGL_TRUE;
#else
        mCurrentParams.eglParameters.debugLayersEnabled = EGL_FALSE;
#endif
    }

    if (gEnableRenderDocCapture)
    {
        mRenderDoc.attach();
    }
}

template <>
void ANGLETestCL<angle::PlatformParameters>::SetUp()
{
    mSetUpCalled = true;

    // Delay test startup to allow a debugger to attach.
    if (GetTestStartDelaySeconds())
    {
        angle::Sleep(GetTestStartDelaySeconds() * 1000);
    }

    const testing::TestInfo *testInfo = testing::UnitTest::GetInstance()->current_test_info();

    // Check the skip list.

    angle::GPUTestConfig::API api = GetTestConfigAPIFromRenderer(
        mCurrentParams.driver, mCurrentParams.getRenderer(), mCurrentParams.getDeviceType());
    GPUTestConfig testConfig = GPUTestConfig(api, 0);

    std::stringstream fullTestNameStr;
    fullTestNameStr << testInfo->test_suite_name() << "." << testInfo->name();
    std::string fullTestName = fullTestNameStr.str();

    // TODO(b/279980674): TestSuite depends on rapidjson which we don't have in aosp builds,
    // for now disable both TestSuite and expectations.
#if defined(ANGLE_HAS_RAPIDJSON)
    TestSuite *testSuite = TestSuite::GetInstance();
    int32_t testExpectation =
        testSuite->getTestExpectationWithConfigAndUpdateTimeout(testConfig, fullTestName);

    if (testExpectation == GPUTestExpectationsParser::kGpuTestSkip)
    {
        GTEST_SKIP() << "Test skipped on this config";
    }
#endif

    if (IsWindows())
    {
        WriteDebugMessage("Entering %s\n", fullTestName.c_str());
    }

    if (gEnableANGLEPerTestCaptureLabel)
    {
        std::string testName = std::string{testInfo->name()};
        std::replace(testName.begin(), testName.end(), '/', '_');
        SetEnvironmentVar("ANGLE_CAPTURE_LABEL",
                          (std::string{testInfo->test_suite_name()} + "_" + testName).c_str());
    }

    mIsSetUp = true;

    testSetUp();
}

template <>
void ANGLETestCL<angle::PlatformParameters>::TearDown()
{
    if (mIsSetUp)
    {
        testTearDown();
    }

    mTearDownCalled = true;

    if (IsWindows())
    {
        const testing::TestInfo *info = testing::UnitTest::GetInstance()->current_test_info();
        WriteDebugMessage("Exiting %s.%s\n", info->test_suite_name(), info->name());
    }
}
