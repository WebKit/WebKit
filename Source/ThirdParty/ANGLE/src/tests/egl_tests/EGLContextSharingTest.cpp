//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EGLContextSharingTest.cpp:
//   Tests relating to shared Contexts.

#include <gtest/gtest.h>

#include "test_utils/ANGLETest.h"
#include "test_utils/angle_test_configs.h"
#include "util/EGLWindow.h"

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

        // Set default test state to not give an error on shutdown.
        getEGLWindow()->makeCurrent();

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

// Tests the creation of contexts using EGL_ANGLE_display_texture_share_group
TEST_P(EGLContextSharingTest, DisplayShareGroupContextCreation)
{
    EGLDisplay display = getEGLWindow()->getDisplay();
    EGLConfig config   = getEGLWindow()->getConfig();

    const EGLint inShareGroupContextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2, EGL_DISPLAY_TEXTURE_SHARE_GROUP_ANGLE, EGL_TRUE, EGL_NONE};

    // Test creating two contexts in the global share group
    mContexts[0] = eglCreateContext(display, config, nullptr, inShareGroupContextAttribs);
    mContexts[1] = eglCreateContext(display, config, mContexts[1], inShareGroupContextAttribs);

    if (!IsEGLDisplayExtensionEnabled(display, "EGL_ANGLE_display_texture_share_group"))
    {
        // Make sure an error is generated and early-exit
        ASSERT_EGL_ERROR(EGL_BAD_ATTRIBUTE);
        ASSERT_EQ(EGL_NO_CONTEXT, mContexts[0]);
        return;
    }

    ASSERT_EGL_SUCCESS();

    ASSERT_NE(EGL_NO_CONTEXT, mContexts[0]);
    ASSERT_NE(EGL_NO_CONTEXT, mContexts[1]);
    eglDestroyContext(display, mContexts[0]);

    // Try creating a context that is not in the global share group but tries to share with a
    // context that is
    const EGLint notInShareGroupContextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2, EGL_DISPLAY_TEXTURE_SHARE_GROUP_ANGLE, EGL_FALSE, EGL_NONE};
    mContexts[1] = eglCreateContext(display, config, mContexts[1], notInShareGroupContextAttribs);
    ASSERT_EGL_ERROR(EGL_BAD_ATTRIBUTE);
    ASSERT_TRUE(mContexts[1] == EGL_NO_CONTEXT);
}

// Tests the sharing of textures using EGL_ANGLE_display_texture_share_group
TEST_P(EGLContextSharingTest, DisplayShareGroupObjectSharing)
{
    EGLDisplay display = getEGLWindow()->getDisplay();
    if (!IsEGLDisplayExtensionEnabled(display, "EGL_ANGLE_display_texture_share_group"))
    {
        std::cout << "Test skipped because EGL_ANGLE_display_texture_share_group is not present."
                  << std::endl;
        return;
    }

    EGLConfig config   = getEGLWindow()->getConfig();
    EGLSurface surface = getEGLWindow()->getSurface();

    const EGLint inShareGroupContextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2, EGL_DISPLAY_TEXTURE_SHARE_GROUP_ANGLE, EGL_TRUE, EGL_NONE};

    // Create two contexts in the global share group but not in the same context share group
    mContexts[0] = eglCreateContext(display, config, nullptr, inShareGroupContextAttribs);
    mContexts[1] = eglCreateContext(display, config, nullptr, inShareGroupContextAttribs);

    ASSERT_EGL_SUCCESS();

    ASSERT_NE(EGL_NO_CONTEXT, mContexts[0]);
    ASSERT_NE(EGL_NO_CONTEXT, mContexts[1]);

    ASSERT_EGL_TRUE(eglMakeCurrent(display, surface, surface, mContexts[0]));
    ASSERT_EGL_SUCCESS();

    // Create a texture and buffer in ctx 0
    GLuint textureFromCtx0 = 0;
    glGenTextures(1, &textureFromCtx0);
    glBindTexture(GL_TEXTURE_2D, textureFromCtx0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
    ASSERT_GL_TRUE(glIsTexture(textureFromCtx0));

    GLuint bufferFromCtx0 = 0;
    glGenBuffers(1, &bufferFromCtx0);
    glBindBuffer(GL_ARRAY_BUFFER, bufferFromCtx0);
    glBufferData(GL_ARRAY_BUFFER, 1, nullptr, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    ASSERT_GL_TRUE(glIsBuffer(bufferFromCtx0));

    ASSERT_GL_NO_ERROR();

    // Switch to context 1 and verify that the texture is accessible but the buffer is not
    ASSERT_EGL_TRUE(eglMakeCurrent(display, surface, surface, mContexts[1]));
    ASSERT_EGL_SUCCESS();

    ASSERT_GL_TRUE(glIsTexture(textureFromCtx0));

    ASSERT_GL_FALSE(glIsBuffer(bufferFromCtx0));
    glDeleteBuffers(1, &bufferFromCtx0);
    ASSERT_GL_NO_ERROR();

    // Call readpixels on the texture to verify that the backend has proper support
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureFromCtx0, 0);

    GLubyte pixel[4];
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    ASSERT_GL_NO_ERROR();

    glDeleteFramebuffers(1, &fbo);

    glDeleteTextures(1, &textureFromCtx0);
    ASSERT_GL_NO_ERROR();
    ASSERT_GL_FALSE(glIsTexture(textureFromCtx0));

    // Switch back to context 0 and delete the buffer
    ASSERT_EGL_TRUE(eglMakeCurrent(display, surface, surface, mContexts[0]));
    ASSERT_EGL_SUCCESS();

    ASSERT_GL_TRUE(glIsBuffer(bufferFromCtx0));
    glDeleteBuffers(1, &bufferFromCtx0);
    ASSERT_GL_NO_ERROR();
}

// Tests that shared textures using EGL_ANGLE_display_texture_share_group are released when the last
// context is destroyed
TEST_P(EGLContextSharingTest, DisplayShareGroupReleasedWithLastContext)
{
    EGLDisplay display = getEGLWindow()->getDisplay();
    if (!IsEGLDisplayExtensionEnabled(display, "EGL_ANGLE_display_texture_share_group"))
    {
        std::cout << "Test skipped because EGL_ANGLE_display_texture_share_group is not present."
                  << std::endl;
        return;
    }

    EGLConfig config   = getEGLWindow()->getConfig();
    EGLSurface surface = getEGLWindow()->getSurface();

    const EGLint inShareGroupContextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2, EGL_DISPLAY_TEXTURE_SHARE_GROUP_ANGLE, EGL_TRUE, EGL_NONE};

    // Create two contexts in the global share group but not in the same context share group
    mContexts[0] = eglCreateContext(display, config, nullptr, inShareGroupContextAttribs);
    mContexts[1] = eglCreateContext(display, config, nullptr, inShareGroupContextAttribs);

    // Create a texture and buffer in ctx 0
    ASSERT_EGL_TRUE(eglMakeCurrent(display, surface, surface, mContexts[0]));
    GLuint textureFromCtx0 = 0;
    glGenTextures(1, &textureFromCtx0);
    glBindTexture(GL_TEXTURE_2D, textureFromCtx0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
    ASSERT_GL_TRUE(glIsTexture(textureFromCtx0));

    // Switch to context 1 and verify that the texture is accessible
    ASSERT_EGL_TRUE(eglMakeCurrent(display, surface, surface, mContexts[1]));
    ASSERT_GL_TRUE(glIsTexture(textureFromCtx0));

    // Destroy both contexts, the texture should be cleaned up automatically
    ASSERT_EGL_TRUE(eglDestroyContext(display, mContexts[0]));
    ASSERT_EGL_TRUE(eglDestroyContext(display, mContexts[1]));

    // Create a new context and verify it cannot access the texture previously created
    mContexts[0] = eglCreateContext(display, config, nullptr, inShareGroupContextAttribs);

    ASSERT_GL_FALSE(glIsTexture(textureFromCtx0));
}

}  // anonymous namespace

ANGLE_INSTANTIATE_TEST(EGLContextSharingTest,
                       ES2_D3D9(),
                       ES2_D3D11(),
                       ES3_D3D11(),
                       ES2_OPENGL(),
                       ES3_OPENGL(),
                       ES2_VULKAN());
