//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Framebuffer tests:
//   Various tests related for Frambuffers.
//

#include "test_utils/ANGLETest.h"

using namespace angle;

class FramebufferFormatsTest : public ANGLETest
{
  protected:
    FramebufferFormatsTest() : mFramebuffer(0), mTexture(0), mRenderbuffer(0), mProgram(0)
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void checkBitCount(GLuint fbo, GLenum channel, GLint minBits)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        GLint bits = 0;
        glGetIntegerv(channel, &bits);

        if (minBits == 0)
        {
            EXPECT_EQ(minBits, bits);
        }
        else
        {
            EXPECT_GE(bits, minBits);
        }
    }

    void testBitCounts(GLuint fbo,
                       GLint minRedBits,
                       GLint minGreenBits,
                       GLint minBlueBits,
                       GLint minAlphaBits,
                       GLint minDepthBits,
                       GLint minStencilBits)
    {
        checkBitCount(fbo, GL_RED_BITS, minRedBits);
        checkBitCount(fbo, GL_GREEN_BITS, minGreenBits);
        checkBitCount(fbo, GL_BLUE_BITS, minBlueBits);
        checkBitCount(fbo, GL_ALPHA_BITS, minAlphaBits);
        checkBitCount(fbo, GL_DEPTH_BITS, minDepthBits);
        checkBitCount(fbo, GL_STENCIL_BITS, minStencilBits);
    }

    void testTextureFormat(GLenum internalFormat,
                           GLint minRedBits,
                           GLint minGreenBits,
                           GLint minBlueBits,
                           GLint minAlphaBits)
    {
        glGenTextures(1, &mTexture);
        glBindTexture(GL_TEXTURE_2D, mTexture);
        glTexStorage2DEXT(GL_TEXTURE_2D, 1, internalFormat, 1, 1);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTexture, 0);

        testBitCounts(mFramebuffer, minRedBits, minGreenBits, minBlueBits, minAlphaBits, 0, 0);
    }

    void testRenderbufferMultisampleFormat(int minESVersion,
                                           GLenum attachmentType,
                                           GLenum internalFormat)
    {
        // TODO(geofflang): Figure out why this is broken on Intel OpenGL
        if (IsIntel() && getPlatformRenderer() == EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE)
        {
            std::cout << "Test skipped on Intel OpenGL." << std::endl;
            return;
        }

        int clientVersion = getClientMajorVersion();
        if (clientVersion < minESVersion)
        {
            return;
        }

        // Check that multisample is supported with at least two samples (minimum required is 1)
        bool supports2Samples = false;

        if (clientVersion == 2)
        {
            if (extensionEnabled("ANGLE_framebuffer_multisample"))
            {
                int maxSamples;
                glGetIntegerv(GL_MAX_SAMPLES_ANGLE, &maxSamples);
                supports2Samples = maxSamples >= 2;
            }
        }
        else
        {
            assert(clientVersion >= 3);
            int maxSamples;
            glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
            supports2Samples = maxSamples >= 2;
        }

        if (!supports2Samples)
        {
            return;
        }

        glGenRenderbuffers(1, &mRenderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, mRenderbuffer);

        EXPECT_GL_NO_ERROR();
        glRenderbufferStorageMultisampleANGLE(GL_RENDERBUFFER, 2, internalFormat, 128, 128);
        EXPECT_GL_NO_ERROR();
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachmentType, GL_RENDERBUFFER, mRenderbuffer);
        EXPECT_GL_NO_ERROR();
    }

    void testZeroHeightRenderbuffer()
    {
        glGenRenderbuffers(1, &mRenderbuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, mRenderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 1, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                  mRenderbuffer);
        EXPECT_GL_NO_ERROR();
    }

    void SetUp() override
    {
        ANGLETest::SetUp();

        glGenFramebuffers(1, &mFramebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    }

    void TearDown() override
    {
        ANGLETest::TearDown();

        if (mTexture != 0)
        {
            glDeleteTextures(1, &mTexture);
            mTexture = 0;
        }

        if (mRenderbuffer != 0)
        {
            glDeleteRenderbuffers(1, &mRenderbuffer);
            mRenderbuffer = 0;
        }

        if (mFramebuffer != 0)
        {
            glDeleteFramebuffers(1, &mFramebuffer);
            mFramebuffer = 0;
        }

        if (mProgram != 0)
        {
            glDeleteProgram(mProgram);
            mProgram = 0;
        }
    }

    GLuint mFramebuffer;
    GLuint mTexture;
    GLuint mRenderbuffer;
    GLuint mProgram;
};

TEST_P(FramebufferFormatsTest, RGBA4)
{
    testTextureFormat(GL_RGBA4, 4, 4, 4, 4);
}

TEST_P(FramebufferFormatsTest, RGB565)
{
    testTextureFormat(GL_RGB565, 5, 6, 5, 0);
}

TEST_P(FramebufferFormatsTest, RGB8)
{
    if (getClientMajorVersion() < 3 && !extensionEnabled("GL_OES_rgb8_rgba8"))
    {
        std::cout << "Test skipped due to missing ES3 or GL_OES_rgb8_rgba8." << std::endl;
        return;
    }

    testTextureFormat(GL_RGB8_OES, 8, 8, 8, 0);
}

TEST_P(FramebufferFormatsTest, BGRA8)
{
    if (!extensionEnabled("GL_EXT_texture_format_BGRA8888"))
    {
        std::cout << "Test skipped due to missing GL_EXT_texture_format_BGRA8888." << std::endl;
        return;
    }

    testTextureFormat(GL_BGRA8_EXT, 8, 8, 8, 8);
}

TEST_P(FramebufferFormatsTest, RGBA8)
{
    if (getClientMajorVersion() < 3 && !extensionEnabled("GL_OES_rgb8_rgba8"))
    {
        std::cout << "Test skipped due to missing ES3 or GL_OES_rgb8_rgba8." << std::endl;
        return;
    }

    testTextureFormat(GL_RGBA8_OES, 8, 8, 8, 8);
}

TEST_P(FramebufferFormatsTest, RenderbufferMultisample_DEPTH16)
{
    testRenderbufferMultisampleFormat(2, GL_DEPTH_ATTACHMENT, GL_DEPTH_COMPONENT16);
}

TEST_P(FramebufferFormatsTest, RenderbufferMultisample_DEPTH24)
{
    testRenderbufferMultisampleFormat(3, GL_DEPTH_ATTACHMENT, GL_DEPTH_COMPONENT24);
}

TEST_P(FramebufferFormatsTest, RenderbufferMultisample_DEPTH32F)
{
    if (getClientMajorVersion() < 3)
    {
        std::cout << "Test skipped due to missing ES3." << std::endl;
        return;
    }

    testRenderbufferMultisampleFormat(3, GL_DEPTH_ATTACHMENT, GL_DEPTH_COMPONENT32F);
}

TEST_P(FramebufferFormatsTest, RenderbufferMultisample_DEPTH24_STENCIL8)
{
    testRenderbufferMultisampleFormat(3, GL_DEPTH_STENCIL_ATTACHMENT, GL_DEPTH24_STENCIL8);
}

TEST_P(FramebufferFormatsTest, RenderbufferMultisample_DEPTH32F_STENCIL8)
{
    if (getClientMajorVersion() < 3)
    {
        std::cout << "Test skipped due to missing ES3." << std::endl;
        return;
    }

    testRenderbufferMultisampleFormat(3, GL_DEPTH_STENCIL_ATTACHMENT, GL_DEPTH32F_STENCIL8);
}

TEST_P(FramebufferFormatsTest, RenderbufferMultisample_STENCIL_INDEX8)
{
    // TODO(geofflang): Figure out how to support GLSTENCIL_INDEX8 on desktop GL
    if (GetParam().getRenderer() == EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE)
    {
        std::cout << "Test skipped on Desktop OpenGL." << std::endl;
        return;
    }

    testRenderbufferMultisampleFormat(2, GL_STENCIL_ATTACHMENT, GL_STENCIL_INDEX8);
}

// Test that binding an incomplete cube map is rejected by ANGLE.
TEST_P(FramebufferFormatsTest, IncompleteCubeMap)
{
    // First make a complete CubeMap.
    glGenTextures(1, &mTexture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, mTexture);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X,
                           mTexture, 0);

    // Verify the framebuffer is complete.
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // Make the CubeMap cube-incomplete.
    glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 nullptr);

    // Verify the framebuffer is incomplete.
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT,
                     glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // Verify drawing with the incomplete framebuffer produces a GL error
    const std::string &vs = "attribute vec4 position; void main() { gl_Position = position; }";
    const std::string &ps = "void main() { gl_FragColor = vec4(1, 0, 0, 1); }";
    mProgram = CompileProgram(vs, ps);
    ASSERT_NE(0u, mProgram);
    drawQuad(mProgram, "position", 0.5f);
    ASSERT_GL_ERROR(GL_INVALID_FRAMEBUFFER_OPERATION);
}

// Test that a renderbuffer with zero height but nonzero width is handled without crashes/asserts.
TEST_P(FramebufferFormatsTest, ZeroHeightRenderbuffer)
{
    if (getClientMajorVersion() < 3)
    {
        std::cout << "Test skipped due to missing ES3" << std::endl;
        return;
    }

    testZeroHeightRenderbuffer();
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST(FramebufferFormatsTest,
                       ES2_D3D9(),
                       ES2_D3D11(),
                       ES3_D3D11(),
                       ES2_OPENGL(),
                       ES3_OPENGL(),
                       ES2_OPENGLES(),
                       ES3_OPENGLES());

class FramebufferInvalidateTest : public ANGLETest
{
  protected:
    FramebufferInvalidateTest() : mFramebuffer(0), mRenderbuffer(0) {}

    void SetUp() override
    {
        ANGLETest::SetUp();

        glGenFramebuffers(1, &mFramebuffer);
        glGenRenderbuffers(1, &mRenderbuffer);
    }

    void TearDown() override
    {
        glDeleteFramebuffers(1, &mFramebuffer);
        glDeleteRenderbuffers(1, &mRenderbuffer);
        ANGLETest::TearDown();
    }

    GLuint mFramebuffer;
    GLuint mRenderbuffer;
};

// Covers invalidating an incomplete framebuffer. This should be a no-op, but should not error.
TEST_P(FramebufferInvalidateTest, Incomplete)
{
    glBindFramebuffer(GL_FRAMEBUFFER, mFramebuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, mRenderbuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, mRenderbuffer);
    EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT,
                     glCheckFramebufferStatus(GL_FRAMEBUFFER));

    std::vector<GLenum> attachments;
    attachments.push_back(GL_COLOR_ATTACHMENT0);

    glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, attachments.data());
    EXPECT_GL_NO_ERROR();
}

ANGLE_INSTANTIATE_TEST(FramebufferInvalidateTest, ES3_D3D11(), ES3_OPENGL(), ES3_OPENGLES());
