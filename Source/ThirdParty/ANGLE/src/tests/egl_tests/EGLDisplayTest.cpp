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
{
  protected:
    EGLConfig chooseConfig(EGLDisplay display)
    {
        const EGLint attribs[] = {EGL_RED_SIZE,
                                  8,
                                  EGL_GREEN_SIZE,
                                  8,
                                  EGL_BLUE_SIZE,
                                  8,
                                  EGL_ALPHA_SIZE,
                                  8,
                                  EGL_RENDERABLE_TYPE,
                                  EGL_OPENGL_ES2_BIT,
                                  EGL_SURFACE_TYPE,
                                  EGL_PBUFFER_BIT,
                                  EGL_NONE};
        EGLConfig config       = EGL_NO_CONFIG_KHR;
        EGLint count           = 0;
        EXPECT_EGL_TRUE(eglChooseConfig(display, attribs, &config, 1, &count));
        EXPECT_EGL_TRUE(count > 0);
        return config;
    }

    EGLContext createContext(EGLDisplay display, EGLConfig config)
    {
        const EGLint attribs[] = {EGL_CONTEXT_MAJOR_VERSION, 2, EGL_NONE};
        EGLContext context     = eglCreateContext(display, config, nullptr, attribs);
        EXPECT_NE(context, EGL_NO_CONTEXT);
        return context;
    }

    EGLSurface createSurface(EGLDisplay display, EGLConfig config)
    {
        const EGLint attribs[] = {EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE};
        EGLSurface surface     = eglCreatePbufferSurface(display, config, attribs);
        EXPECT_NE(surface, EGL_NO_SURFACE);
        return surface;
    }
};

// Tests that an EGLDisplay can be re-initialized.
TEST_P(EGLDisplayTest, InitializeTerminateInitialize)
{
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EXPECT_EGL_TRUE(eglInitialize(display, nullptr, nullptr) != EGL_FALSE);
    EXPECT_EGL_TRUE(eglTerminate(display) != EGL_FALSE);
    EXPECT_EGL_TRUE(eglInitialize(display, nullptr, nullptr) != EGL_FALSE);
}

// Tests current Context leaking when call eglTerminate() while it is current.
TEST_P(EGLDisplayTest, ContextLeakAfterTerminate)
{
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EXPECT_EGL_TRUE(eglInitialize(display, nullptr, nullptr));

    EGLConfig config   = chooseConfig(display);
    EGLContext context = createContext(display, config);
    EGLSurface surface = createSurface(display, config);

    // Make "context" current.
    EXPECT_EGL_TRUE(eglMakeCurrent(display, surface, surface, context));

    // Terminate display while "context" is current.
    EXPECT_EGL_TRUE(eglTerminate(display));

    // Unmake "context" from current and allow Display to actually terminate.
    (void)eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    // Get EGLDisplay again.
    display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    // Check if Display was actually terminated.
    EGLint val;
    EXPECT_EGL_FALSE(eglQueryContext(display, context, EGL_CONTEXT_CLIENT_TYPE, &val));
    EXPECT_EQ(eglGetError(), EGL_NOT_INITIALIZED);
}

ANGLE_INSTANTIATE_TEST(EGLDisplayTest,
                       WithNoFixture(ES2_D3D9()),
                       WithNoFixture(ES2_D3D11()),
                       WithNoFixture(ES2_OPENGL()),
                       WithNoFixture(ES2_VULKAN()),
                       WithNoFixture(ES3_D3D11()),
                       WithNoFixture(ES3_OPENGL()),
                       WithNoFixture(ES3_VULKAN()));
