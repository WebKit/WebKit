//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include <gtest/gtest.h>
#include <vector>

#include "test_utils/ANGLETest.h"

using namespace angle;

class EGLDisplayTest : public ANGLETest<>
{};

// Tests that an EGLDisplay can be re-initialized.
TEST_P(EGLDisplayTest, InitalizeTerminateInitalize)
{
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EXPECT_EGL_TRUE(eglInitialize(display, nullptr, nullptr) != EGL_FALSE);
    EXPECT_EGL_TRUE(eglTerminate(display) != EGL_FALSE);
    EXPECT_EGL_TRUE(eglInitialize(display, nullptr, nullptr) != EGL_FALSE);
}

ANGLE_INSTANTIATE_TEST(EGLDisplayTest,
                       WithNoFixture(ES2_D3D9()),
                       WithNoFixture(ES2_D3D11()),
                       WithNoFixture(ES2_OPENGL()),
                       WithNoFixture(ES2_VULKAN()),
                       WithNoFixture(ES3_D3D11()),
                       WithNoFixture(ES3_OPENGL()),
                       WithNoFixture(ES3_VULKAN()));
