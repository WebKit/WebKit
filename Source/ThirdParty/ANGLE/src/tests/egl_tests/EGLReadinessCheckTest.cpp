//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// EGLReadinessCheckTest.cpp:
//      Tests used to check environment in which other tests are run.

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_libc_calls
#endif

#include <gtest/gtest.h>

#include "gpu_info_util/SystemInfo.h"
#include "test_utils/ANGLETest.h"
#include "test_utils/angle_test_instantiate.h"

using namespace angle;

class EGLReadinessCheckTest : public ANGLETest<>
{};

// Checks the tests are running against ANGLE
TEST_P(EGLReadinessCheckTest, IsRunningOnANGLE)
{
    const char *extensionString =
        static_cast<const char *>(eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS));
    ASSERT_NE(strstr(extensionString, "EGL_ANGLE_platform_angle"), nullptr);
}

// Checks that getting function pointer works
TEST_P(EGLReadinessCheckTest, HasGetPlatformDisplay)
{
    ASSERT_NE(eglGetPlatformDisplay, nullptr);
}

// Checks that calling GetProcAddress for a non-existent function fails.
TEST_P(EGLReadinessCheckTest, GetProcAddressNegativeTest)
{
    auto check = eglGetProcAddress("WigglyWombats");
    EXPECT_EQ(nullptr, check);
}

// Checks that calling GetProcAddress for a null pointer function fails.
TEST_P(EGLReadinessCheckTest, GetProcAddressNullInput)
{
    auto check = eglGetProcAddress(nullptr);
    EXPECT_EQ(nullptr, check);
}

// Tests that our allowlist function generally maps to our support function.
// We can add specific exceptions here if needed.
// Disabled because it was creating a large number of configs. This could even result
// in a BSOD on Windows.
TEST_P(EGLReadinessCheckTest, DISABLED_AllowlistMatchesSupport)
{
    // Has issues with Vulkan support detection on Android.
    ANGLE_SKIP_TEST_IF(IsAndroid());

    // Cannot make any useful checks if SystemInfo is not supported.
    SystemInfo systemInfo;
    ANGLE_SKIP_TEST_IF(!GetSystemInfo(&systemInfo));

    auto check = [&systemInfo](const PlatformParameters &params) {
        EXPECT_EQ(IsConfigAllowlisted(systemInfo, params), IsConfigSupported(params)) << params;
    };

    check(ES1_OPENGL());
    check(ES2_OPENGL());
    check(ES3_OPENGL());
    check(ES31_OPENGL());

    check(ES1_OPENGLES());
    check(ES2_OPENGLES());
    check(ES3_OPENGLES());
    check(ES31_OPENGLES());

    check(ES1_D3D9());
    check(ES2_D3D9());

    check(ES1_D3D11());
    check(ES2_D3D11());
    check(ES3_D3D11());
    check(ES31_D3D11());

    check(ES1_VULKAN());
    check(ES2_VULKAN());
    check(ES3_VULKAN());

    check(ES1_VULKAN_NULL());
    check(ES2_VULKAN_NULL());
    check(ES3_VULKAN_NULL());

    check(ES1_NULL());
    check(ES2_NULL());
    check(ES3_NULL());
    check(ES31_NULL());
}

ANGLE_INSTANTIATE_TEST(EGLReadinessCheckTest, WithNoFixture(PlatformParameters()));
