//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EGLContextSharingTest.cpp:
//   Tests relating to shared Contexts.

#include <gtest/gtest.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "test_utils/ANGLETest.h"
#include "test_utils/angle_test_configs.h"

using namespace angle;

namespace
{

EGLBoolean SafeDestroyContext(EGLDisplay display, EGLContext &context)
{
    EGLBoolean result = EGL_TRUE;
    if (context != EGL_NO_CONTEXT)
    {
        result  = eglDestroyContext(display, context);
        context = EGL_NO_CONTEXT;
    }
    return result;
}

class EGLContextSharingTest : public ANGLETest
{
  public:
    EGLContextSharingTest() : mContexts{EGL_NO_CONTEXT, EGL_NO_CONTEXT}, mTexture(0) {}

    void TearDown() override
    {
        glDeleteTextures(1, &mTexture);

        EGLDisplay display = getEGLWindow()->getDisplay();

        if (display != EGL_NO_DISPLAY)
        {
            for (auto &context : mContexts)
            {
                SafeDestroyContext(display, context);
            }
        }

        ANGLETest::TearDown();
    }

    EGLContext mContexts[2];
    GLuint mTexture;
};

// Tests that creating resources works after freeing the share context.
TEST_P(EGLContextSharingTest, BindTextureAfterShareContextFree)
{
    EGLDisplay display = getEGLWindow()->getDisplay();
    EGLConfig config   = getEGLWindow()->getConfig();
    EGLSurface surface = getEGLWindow()->getSurface();

    const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION,
                                     getEGLWindow()->getClientMajorVersion(), EGL_NONE};

    mContexts[0] = eglCreateContext(display, config, nullptr, contextAttribs);
    ASSERT_EGL_SUCCESS();
    ASSERT_TRUE(mContexts[0] != EGL_NO_CONTEXT);
    mContexts[1] = eglCreateContext(display, config, mContexts[1], contextAttribs);
    ASSERT_EGL_SUCCESS();
    ASSERT_TRUE(mContexts[1] != EGL_NO_CONTEXT);

    ASSERT_EGL_TRUE(SafeDestroyContext(display, mContexts[0]));
    ASSERT_EGL_TRUE(eglMakeCurrent(display, surface, surface, mContexts[1]));
    ASSERT_EGL_SUCCESS();

    glGenTextures(1, &mTexture);
    glBindTexture(GL_TEXTURE_2D, mTexture);
    ASSERT_GL_NO_ERROR();
}

}  // anonymous namespace

ANGLE_INSTANTIATE_TEST(EGLContextSharingTest, ES2_D3D9(), ES2_D3D11(), ES2_OPENGL());
