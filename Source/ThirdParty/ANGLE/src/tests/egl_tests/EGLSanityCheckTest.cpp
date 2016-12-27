//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// EGLSanityCheckTest.cpp:
//      tests used to check setup in which tests are run.

#include <gtest/gtest.h>

#include "test_utils/ANGLETest.h"

// Checks the tests are running against ANGLE
TEST(EGLSanityCheckTest, IsRunningOnANGLE)
{
    const char *extensionString =
        static_cast<const char *>(eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS));
    ASSERT_NE(strstr(extensionString, "EGL_ANGLE_platform_angle"), nullptr);
}

// Checks that getting function pointer works
TEST(EGLSanityCheckTest, HasGetPlatformDisplayEXT)
{
    PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT =
        reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
            eglGetProcAddress("eglGetPlatformDisplayEXT"));

    ASSERT_NE(eglGetPlatformDisplayEXT, nullptr);
}
