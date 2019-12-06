//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ExplicitContextTest.cpp: Tests of the EGL_ANGLE_explicit_context extension

#include "test_utils/ANGLETest.h"

#include "test_utils/gl_raii.h"
#include "util/EGLWindow.h"

namespace angle
{

class ExplicitContextTest : public ANGLETest
{
  protected:
    ExplicitContextTest()
    {
        setWindowWidth(256);
        setWindowHeight(256);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }
};

// Test to ensure that the basic functionality of the extension works.
TEST_P(ExplicitContextTest, BasicTest)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_explicit_context") ||
                       !IsGLExtensionEnabled("GL_ANGLE_explicit_context_gles1"));

    EGLContext context = getEGLWindow()->getContext();

    // Clear to green
    glClearColorContextANGLE(context, 0.0f, 1.0f, 0.0f, 1.0f);
    glClearContextANGLE(context, GL_COLOR_BUFFER_BIT);

    // Check for green
    EXPECT_PIXEL_EQ(0, 0, 0, 255, 0, 255);
    ASSERT_GL_NO_ERROR();
}

// Test to ensure that extension works with eglGetProcAddress
TEST_P(ExplicitContextTest, GetProcAddress)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_explicit_context") ||
                       !IsGLExtensionEnabled("GL_ANGLE_explicit_context_gles1"));

    EGLContext context = getEGLWindow()->getContext();

    // Clear to green
    glClearColorContextANGLE(context, 1.0f, 0, 0, 1.0f);
    glClearContextANGLE(context, GL_COLOR_BUFFER_BIT);

    // Check for green
    EXPECT_PIXEL_EQ(0, 0, 255, 0, 0, 255);
    EXPECT_GL_NO_ERROR();
}

// Test to ensure that a passed context of null results in a no-op
TEST_P(ExplicitContextTest, NullContext)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_explicit_context") ||
                       !IsGLExtensionEnabled("GL_ANGLE_explicit_context_gles1"));

    EGLContext context = getEGLWindow()->getContext();

    // Set clear color to red
    glClearColorContextANGLE(context, 1.0f, 0, 0, 1.0f);
    // Clear to red
    glClearContextANGLE(context, GL_COLOR_BUFFER_BIT);

    // Set clear color to green
    glClearColorContextANGLE(context, 0.0f, 1.0f, 0.0f, 1.0f);
    // Call clear with null context, which should perform a silent no-op
    glClearContextANGLE(nullptr, GL_COLOR_BUFFER_BIT);

    // Check that the color is red
    EXPECT_PIXEL_EQ(0, 0, 255, 0, 0, 255);
    EXPECT_GL_NO_ERROR();
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST_ES2(ExplicitContextTest);

}  // namespace angle
