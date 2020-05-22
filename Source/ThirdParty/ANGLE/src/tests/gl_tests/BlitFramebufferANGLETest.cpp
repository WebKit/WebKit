//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

class BlitFramebufferANGLETest : public ANGLETest
{
  protected:
    BlitFramebufferANGLETest()
    {
        setWindowWidth(32);
        setWindowHeight(32);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setConfigDepthBits(24);
        setConfigStencilBits(8);

        mCheckerProgram = 0;
        mBlueProgram    = 0;
        mRedProgram     = 0;

        mOriginalFBO = 0;

        mUserFBO                = 0;
        mUserColorBuffer        = 0;
        mUserDepthStencilBuffer = 0;

        mSmallFBO                = 0;
        mSmallColorBuffer        = 0;
        mSmallDepthStencilBuffer = 0;

        mColorOnlyFBO         = 0;
        mColorOnlyColorBuffer = 0;

        mDiffFormatFBO         = 0;
        mDiffFormatColorBuffer = 0;

        mDiffSizeFBO         = 0;
        mDiffSizeColorBuffer = 0;

        mMRTFBO          = 0;
        mMRTColorBuffer0 = 0;
        mMRTColorBuffer1 = 0;

        mRGBAColorbuffer              = 0;
        mRGBAFBO                      = 0;
        mRGBAMultisampledRenderbuffer = 0;
        mRGBAMultisampledFBO          = 0;

        mBGRAColorbuffer              = 0;
        mBGRAFBO                      = 0;
        mBGRAMultisampledRenderbuffer = 0;
        mBGRAMultisampledFBO          = 0;
    }

    void testSetUp() override
    {
        mCheckerProgram =
            CompileProgram(essl1_shaders::vs::Passthrough(), essl1_shaders::fs::Checkered());
        mBlueProgram = CompileProgram(essl1_shaders::vs::Simple(), essl1_shaders::fs::Blue());
        mRedProgram  = CompileProgram(essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());
        if (mCheckerProgram == 0 || mBlueProgram == 0 || mRedProgram == 0)
        {
            FAIL() << "shader compilation failed.";
        }

        EXPECT_GL_NO_ERROR();

        GLint originalFBO;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &originalFBO);
        if (originalFBO >= 0)
        {
            mOriginalFBO = (GLuint)originalFBO;
        }

        GLenum format = GL_BGRA_EXT;

        glGenFramebuffers(1, &mUserFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, mUserFBO);
        glGenTextures(1, &mUserColorBuffer);
        glGenRenderbuffers(1, &mUserDepthStencilBuffer);
        glBindTexture(GL_TEXTURE_2D, mUserColorBuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               mUserColorBuffer, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, format, getWindowWidth(), getWindowHeight(), 0, format,
                     GL_UNSIGNED_BYTE, nullptr);
        glBindRenderbuffer(GL_RENDERBUFFER, mUserDepthStencilBuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, getWindowWidth(),
                              getWindowHeight());
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                  mUserDepthStencilBuffer);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                  mUserDepthStencilBuffer);

        ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
        ASSERT_GL_NO_ERROR();

        glGenFramebuffers(1, &mSmallFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, mSmallFBO);
        glGenTextures(1, &mSmallColorBuffer);
        glGenRenderbuffers(1, &mSmallDepthStencilBuffer);
        glBindTexture(GL_TEXTURE_2D, mSmallColorBuffer);
        glTexImage2D(GL_TEXTURE_2D, 0, format, getWindowWidth() / 2, getWindowHeight() / 2, 0,
                     format, GL_UNSIGNED_BYTE, nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               mSmallColorBuffer, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, mSmallDepthStencilBuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, getWindowWidth() / 2,
                              getWindowHeight() / 2);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                  mSmallDepthStencilBuffer);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                  mSmallDepthStencilBuffer);

        ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
        ASSERT_GL_NO_ERROR();

        glGenFramebuffers(1, &mColorOnlyFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, mColorOnlyFBO);
        glGenTextures(1, &mColorOnlyColorBuffer);
        glBindTexture(GL_TEXTURE_2D, mColorOnlyColorBuffer);
        glTexImage2D(GL_TEXTURE_2D, 0, format, getWindowWidth(), getWindowHeight(), 0, format,
                     GL_UNSIGNED_BYTE, nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               mColorOnlyColorBuffer, 0);

        ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
        ASSERT_GL_NO_ERROR();

        glGenFramebuffers(1, &mDiffFormatFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, mDiffFormatFBO);
        glGenTextures(1, &mDiffFormatColorBuffer);
        glBindTexture(GL_TEXTURE_2D, mDiffFormatColorBuffer);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, getWindowWidth(), getWindowHeight(), 0, GL_RGB,
                     GL_UNSIGNED_SHORT_5_6_5, nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               mDiffFormatColorBuffer, 0);

        ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
        ASSERT_GL_NO_ERROR();

        glGenFramebuffers(1, &mDiffSizeFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, mDiffSizeFBO);
        glGenTextures(1, &mDiffSizeColorBuffer);
        glBindTexture(GL_TEXTURE_2D, mDiffSizeColorBuffer);
        glTexImage2D(GL_TEXTURE_2D, 0, format, getWindowWidth() * 2, getWindowHeight() * 2, 0,
                     format, GL_UNSIGNED_BYTE, nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               mDiffSizeColorBuffer, 0);

        ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
        ASSERT_GL_NO_ERROR();

        if (IsGLExtensionEnabled("GL_EXT_draw_buffers"))
        {
            glGenFramebuffers(1, &mMRTFBO);
            glBindFramebuffer(GL_FRAMEBUFFER, mMRTFBO);
            glGenTextures(1, &mMRTColorBuffer0);
            glGenTextures(1, &mMRTColorBuffer1);
            glBindTexture(GL_TEXTURE_2D, mMRTColorBuffer0);
            glTexImage2D(GL_TEXTURE_2D, 0, format, getWindowWidth(), getWindowHeight(), 0, format,
                         GL_UNSIGNED_BYTE, nullptr);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D,
                                   mMRTColorBuffer0, 0);
            glBindTexture(GL_TEXTURE_2D, mMRTColorBuffer1);
            glTexImage2D(GL_TEXTURE_2D, 0, format, getWindowWidth(), getWindowHeight(), 0, format,
                         GL_UNSIGNED_BYTE, nullptr);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1_EXT, GL_TEXTURE_2D,
                                   mMRTColorBuffer1, 0);

            ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
            ASSERT_GL_NO_ERROR();
        }

        if (IsGLExtensionEnabled("GL_ANGLE_framebuffer_multisample") &&
            IsGLExtensionEnabled("GL_OES_rgb8_rgba8"))
        {
            // RGBA single-sampled framebuffer
            glGenTextures(1, &mRGBAColorbuffer);
            glBindTexture(GL_TEXTURE_2D, mRGBAColorbuffer);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, getWindowWidth(), getWindowHeight(), 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, nullptr);

            glGenFramebuffers(1, &mRGBAFBO);
            glBindFramebuffer(GL_FRAMEBUFFER, mRGBAFBO);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   mRGBAColorbuffer, 0);

            ASSERT_GL_NO_ERROR();
            ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

            // RGBA multisampled framebuffer
            glGenRenderbuffers(1, &mRGBAMultisampledRenderbuffer);
            glBindRenderbuffer(GL_RENDERBUFFER, mRGBAMultisampledRenderbuffer);
            glRenderbufferStorageMultisampleANGLE(GL_RENDERBUFFER, 1, GL_RGBA8, getWindowWidth(),
                                                  getWindowHeight());

            glGenFramebuffers(1, &mRGBAMultisampledFBO);
            glBindFramebuffer(GL_FRAMEBUFFER, mRGBAMultisampledFBO);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                      mRGBAMultisampledRenderbuffer);

            ASSERT_GL_NO_ERROR();
            ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

            if (IsGLExtensionEnabled("GL_EXT_texture_format_BGRA8888"))
            {
                // BGRA single-sampled framebuffer
                glGenTextures(1, &mBGRAColorbuffer);
                glBindTexture(GL_TEXTURE_2D, mBGRAColorbuffer);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA_EXT, getWindowWidth(), getWindowHeight(), 0,
                             GL_BGRA_EXT, GL_UNSIGNED_BYTE, nullptr);

                glGenFramebuffers(1, &mBGRAFBO);
                glBindFramebuffer(GL_FRAMEBUFFER, mBGRAFBO);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                       mBGRAColorbuffer, 0);

                ASSERT_GL_NO_ERROR();
                ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

                // BGRA multisampled framebuffer
                glGenRenderbuffers(1, &mBGRAMultisampledRenderbuffer);
                glBindRenderbuffer(GL_RENDERBUFFER, mBGRAMultisampledRenderbuffer);
                glRenderbufferStorageMultisampleANGLE(GL_RENDERBUFFER, 1, GL_BGRA8_EXT,
                                                      getWindowWidth(), getWindowHeight());

                glGenFramebuffers(1, &mBGRAMultisampledFBO);
                glBindFramebuffer(GL_FRAMEBUFFER, mBGRAMultisampledFBO);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                          mBGRAMultisampledRenderbuffer);

                ASSERT_GL_NO_ERROR();
                ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
            }
        }

        glBindFramebuffer(GL_FRAMEBUFFER, mOriginalFBO);
    }

    void testTearDown() override
    {
        glDeleteProgram(mCheckerProgram);
        glDeleteProgram(mBlueProgram);
        glDeleteProgram(mRedProgram);

        glDeleteFramebuffers(1, &mUserFBO);
        glDeleteTextures(1, &mUserColorBuffer);
        glDeleteRenderbuffers(1, &mUserDepthStencilBuffer);

        glDeleteFramebuffers(1, &mSmallFBO);
        glDeleteTextures(1, &mSmallColorBuffer);
        glDeleteRenderbuffers(1, &mSmallDepthStencilBuffer);

        glDeleteFramebuffers(1, &mColorOnlyFBO);
        glDeleteTextures(1, &mSmallDepthStencilBuffer);

        glDeleteFramebuffers(1, &mDiffFormatFBO);
        glDeleteTextures(1, &mDiffFormatColorBuffer);

        glDeleteFramebuffers(1, &mDiffSizeFBO);
        glDeleteTextures(1, &mDiffSizeColorBuffer);

        if (IsGLExtensionEnabled("GL_EXT_draw_buffers"))
        {
            glDeleteFramebuffers(1, &mMRTFBO);
            glDeleteTextures(1, &mMRTColorBuffer0);
            glDeleteTextures(1, &mMRTColorBuffer1);
        }

        if (mRGBAColorbuffer != 0)
        {
            glDeleteTextures(1, &mRGBAColorbuffer);
        }

        if (mRGBAFBO != 0)
        {
            glDeleteFramebuffers(1, &mRGBAFBO);
        }

        if (mRGBAMultisampledRenderbuffer != 0)
        {
            glDeleteRenderbuffers(1, &mRGBAMultisampledRenderbuffer);
        }

        if (mRGBAMultisampledFBO != 0)
        {
            glDeleteFramebuffers(1, &mRGBAMultisampledFBO);
        }

        if (mBGRAColorbuffer != 0)
        {
            glDeleteTextures(1, &mBGRAColorbuffer);
        }

        if (mBGRAFBO != 0)
        {
            glDeleteFramebuffers(1, &mBGRAFBO);
        }

        if (mBGRAMultisampledRenderbuffer != 0)
        {
            glDeleteRenderbuffers(1, &mBGRAMultisampledRenderbuffer);
        }

        if (mBGRAMultisampledFBO != 0)
        {
            glDeleteFramebuffers(1, &mBGRAMultisampledFBO);
        }
    }

    void multisampleTestHelper(GLuint readFramebuffer, GLuint drawFramebuffer)
    {
        glClearColor(0.0, 1.0, 0.0, 1.0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, readFramebuffer);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        EXPECT_GL_NO_ERROR();

        glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebuffer);
        glBlitFramebufferANGLE(0, 0, getWindowWidth(), getWindowHeight(), 0, 0, getWindowWidth(),
                               getWindowHeight(), GL_COLOR_BUFFER_BIT, GL_NEAREST);
        EXPECT_GL_NO_ERROR();

        glBindFramebuffer(GL_READ_FRAMEBUFFER, drawFramebuffer);
        EXPECT_PIXEL_EQ(getWindowWidth() / 4, getWindowHeight() / 4, 0, 255, 0, 255);
        EXPECT_PIXEL_EQ(3 * getWindowWidth() / 4, getWindowHeight() / 4, 0, 255, 0, 255);
        EXPECT_PIXEL_EQ(3 * getWindowWidth() / 4, 3 * getWindowHeight() / 4, 0, 255, 0, 255);
        EXPECT_PIXEL_EQ(getWindowWidth() / 4, 3 * getWindowHeight() / 4, 0, 255, 0, 255);
    }

    bool checkExtension(const std::string &extension)
    {
        if (!IsGLExtensionEnabled(extension))
        {
            std::cout << "Test skipped because " << extension << " not supported." << std::endl;
            return false;
        }

        return true;
    }

    GLuint mCheckerProgram;
    GLuint mBlueProgram;
    GLuint mRedProgram;

    GLuint mOriginalFBO;

    GLuint mUserFBO;
    GLuint mUserColorBuffer;
    GLuint mUserDepthStencilBuffer;

    GLuint mSmallFBO;
    GLuint mSmallColorBuffer;
    GLuint mSmallDepthStencilBuffer;

    GLuint mColorOnlyFBO;
    GLuint mColorOnlyColorBuffer;

    GLuint mDiffFormatFBO;
    GLuint mDiffFormatColorBuffer;

    GLuint mDiffSizeFBO;
    GLuint mDiffSizeColorBuffer;

    GLuint mMRTFBO;
    GLuint mMRTColorBuffer0;
    GLuint mMRTColorBuffer1;

    GLuint mRGBAColorbuffer;
    GLuint mRGBAFBO;
    GLuint mRGBAMultisampledRenderbuffer;
    GLuint mRGBAMultisampledFBO;

    GLuint mBGRAColorbuffer;
    GLuint mBGRAFBO;
    GLuint mBGRAMultisampledRenderbuffer;
    GLuint mBGRAMultisampledFBO;
};

// Draw to user-created framebuffer, blit whole-buffer color to original framebuffer.
TEST_P(BlitFramebufferANGLETest, BlitColorToDefault)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_framebuffer_blit"));

    glBindFramebuffer(GL_FRAMEBUFFER, mUserFBO);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    drawQuad(mCheckerProgram, essl1_shaders::PositionAttrib(), 0.8f);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, mUserFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, mOriginalFBO);

    glBlitFramebufferANGLE(0, 0, getWindowWidth(), getWindowHeight(), 0, 0, getWindowWidth(),
                           getWindowHeight(), GL_COLOR_BUFFER_BIT, GL_NEAREST);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_FRAMEBUFFER, mOriginalFBO);

    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 4, getWindowHeight() / 4, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(3 * getWindowWidth() / 4, getWindowHeight() / 4, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(3 * getWindowWidth() / 4, 3 * getWindowHeight() / 4, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 4, 3 * getWindowHeight() / 4, GLColor::green);
}

// Draw to system framebuffer, blit whole-buffer color to user-created framebuffer.
TEST_P(BlitFramebufferANGLETest, ReverseColorBlit)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_framebuffer_blit"));

    // TODO(jmadill): Fix this. http://anglebug.com/2743
    ANGLE_SKIP_TEST_IF(IsVulkan() && IsAndroid());

    glBindFramebuffer(GL_FRAMEBUFFER, mOriginalFBO);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    drawQuad(mCheckerProgram, essl1_shaders::PositionAttrib(), 0.8f);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, mOriginalFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, mUserFBO);

    glBlitFramebufferANGLE(0, 0, getWindowWidth(), getWindowHeight(), 0, 0, getWindowWidth(),
                           getWindowHeight(), GL_COLOR_BUFFER_BIT, GL_NEAREST);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_FRAMEBUFFER, mUserFBO);

    EXPECT_PIXEL_EQ(getWindowWidth() / 4, getWindowHeight() / 4, 255, 0, 0, 255);
    EXPECT_PIXEL_EQ(3 * getWindowWidth() / 4, getWindowHeight() / 4, 0, 255, 0, 255);
    EXPECT_PIXEL_EQ(3 * getWindowWidth() / 4, 3 * getWindowHeight() / 4, 255, 0, 0, 255);
    EXPECT_PIXEL_EQ(getWindowWidth() / 4, 3 * getWindowHeight() / 4, 0, 255, 0, 255);
}

// blit from user-created FBO to system framebuffer, with the scissor test enabled.
TEST_P(BlitFramebufferANGLETest, ScissoredBlit)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_framebuffer_blit"));

    glBindFramebuffer(GL_FRAMEBUFFER, mUserFBO);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    drawQuad(mCheckerProgram, essl1_shaders::PositionAttrib(), 0.8f);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, mOriginalFBO);
    glBindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, mUserFBO);

    glClearColor(1.0, 1.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glScissor(getWindowWidth() / 2, 0, getWindowWidth() / 2, getWindowHeight());
    glEnable(GL_SCISSOR_TEST);

    glBlitFramebufferANGLE(0, 0, getWindowWidth(), getWindowHeight(), 0, 0, getWindowWidth(),
                           getWindowHeight(), GL_COLOR_BUFFER_BIT, GL_NEAREST);

    EXPECT_GL_NO_ERROR();

    glDisable(GL_SCISSOR_TEST);

    glBindFramebuffer(GL_FRAMEBUFFER, mOriginalFBO);

    EXPECT_PIXEL_EQ(getWindowWidth() / 4, getWindowHeight() / 4, 255, 255, 255, 255);
    EXPECT_PIXEL_EQ(3 * getWindowWidth() / 4, getWindowHeight() / 4, 0, 255, 0, 255);
    EXPECT_PIXEL_EQ(3 * getWindowWidth() / 4, 3 * getWindowHeight() / 4, 255, 0, 0, 255);
    EXPECT_PIXEL_EQ(getWindowWidth() / 4, 3 * getWindowHeight() / 4, 255, 255, 255, 255);
}

// blit from system FBO to user-created framebuffer, with the scissor test enabled.
TEST_P(BlitFramebufferANGLETest, ReverseScissoredBlit)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_framebuffer_blit"));

    glBindFramebuffer(GL_FRAMEBUFFER, mOriginalFBO);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    drawQuad(mCheckerProgram, essl1_shaders::PositionAttrib(), 0.8f);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, mUserFBO);
    glBindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, mOriginalFBO);

    glClearColor(1.0, 1.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glScissor(getWindowWidth() / 2, 0, getWindowWidth() / 2, getWindowHeight());
    glEnable(GL_SCISSOR_TEST);

    glBlitFramebufferANGLE(0, 0, getWindowWidth(), getWindowHeight(), 0, 0, getWindowWidth(),
                           getWindowHeight(), GL_COLOR_BUFFER_BIT, GL_NEAREST);

    EXPECT_GL_NO_ERROR();

    glDisable(GL_SCISSOR_TEST);

    glBindFramebuffer(GL_FRAMEBUFFER, mUserFBO);

    EXPECT_PIXEL_EQ(getWindowWidth() / 4, getWindowHeight() / 4, 255, 255, 255, 255);
    EXPECT_PIXEL_EQ(3 * getWindowWidth() / 4, getWindowHeight() / 4, 0, 255, 0, 255);
    EXPECT_PIXEL_EQ(3 * getWindowWidth() / 4, 3 * getWindowHeight() / 4, 255, 0, 0, 255);
    EXPECT_PIXEL_EQ(getWindowWidth() / 4, 3 * getWindowHeight() / 4, 255, 255, 255, 255);
}

// blit from user-created FBO to system framebuffer, using region larger than buffer.
TEST_P(BlitFramebufferANGLETest, OversizedBlit)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_framebuffer_blit"));

    glBindFramebuffer(GL_FRAMEBUFFER, mUserFBO);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    drawQuad(mCheckerProgram, essl1_shaders::PositionAttrib(), 0.8f);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, mOriginalFBO);
    glBindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, mUserFBO);

    glClearColor(1.0, 1.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glBlitFramebufferANGLE(0, 0, getWindowWidth() * 2, getWindowHeight() * 2, 0, 0,
                           getWindowWidth() * 2, getWindowHeight() * 2, GL_COLOR_BUFFER_BIT,
                           GL_NEAREST);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_FRAMEBUFFER, mOriginalFBO);

    EXPECT_PIXEL_EQ(getWindowWidth() / 4, getWindowHeight() / 4, 255, 0, 0, 255);
    EXPECT_PIXEL_EQ(3 * getWindowWidth() / 4, getWindowHeight() / 4, 0, 255, 0, 255);
    EXPECT_PIXEL_EQ(3 * getWindowWidth() / 4, 3 * getWindowHeight() / 4, 255, 0, 0, 255);
    EXPECT_PIXEL_EQ(getWindowWidth() / 4, 3 * getWindowHeight() / 4, 0, 255, 0, 255);
}

// blit from system FBO to user-created framebuffer, using region larger than buffer.
TEST_P(BlitFramebufferANGLETest, ReverseOversizedBlit)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_framebuffer_blit"));

    glBindFramebuffer(GL_FRAMEBUFFER, mOriginalFBO);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    drawQuad(mCheckerProgram, essl1_shaders::PositionAttrib(), 0.8f);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, mUserFBO);
    glBindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, mOriginalFBO);

    glClearColor(1.0, 1.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glBlitFramebufferANGLE(0, 0, getWindowWidth() * 2, getWindowHeight() * 2, 0, 0,
                           getWindowWidth() * 2, getWindowHeight() * 2, GL_COLOR_BUFFER_BIT,
                           GL_NEAREST);
    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_FRAMEBUFFER, mUserFBO);

    EXPECT_PIXEL_EQ(getWindowWidth() / 4, getWindowHeight() / 4, 255, 0, 0, 255);
    EXPECT_PIXEL_EQ(3 * getWindowWidth() / 4, getWindowHeight() / 4, 0, 255, 0, 255);
    EXPECT_PIXEL_EQ(3 * getWindowWidth() / 4, 3 * getWindowHeight() / 4, 255, 0, 0, 255);
    EXPECT_PIXEL_EQ(getWindowWidth() / 4, 3 * getWindowHeight() / 4, 0, 255, 0, 255);
}

// blit from user-created FBO to system framebuffer, with depth buffer.
TEST_P(BlitFramebufferANGLETest, BlitWithDepthUserToDefault)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_framebuffer_blit"));

    glBindFramebuffer(GL_FRAMEBUFFER, mUserFBO);

    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);

    EXPECT_GL_NO_ERROR();

    // Clear the first half of the screen
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, getWindowWidth(), getWindowHeight() / 2);

    glClearDepthf(0.1f);
    glClearColor(1.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Scissor the second half of the screen
    glScissor(0, getWindowHeight() / 2, getWindowWidth(), getWindowHeight() / 2);

    glClearDepthf(0.9f);
    glClearColor(0.0, 1.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDisable(GL_SCISSOR_TEST);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, mOriginalFBO);
    glBindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, mUserFBO);

    glBlitFramebufferANGLE(0, 0, getWindowWidth(), getWindowHeight(), 0, 0, getWindowWidth(),
                           getWindowHeight(), GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT,
                           GL_NEAREST);
    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_FRAMEBUFFER, mOriginalFBO);

    // if blit is happening correctly, this quad will draw only on the bottom half since it will
    // be behind on the first half and in front on the second half.
    drawQuad(mBlueProgram, essl1_shaders::PositionAttrib(), 0.5f);

    glDisable(GL_DEPTH_TEST);

    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 4, getWindowHeight() / 4, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(3 * getWindowWidth() / 4, getWindowHeight() / 4, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(3 * getWindowWidth() / 4, 3 * getWindowHeight() / 4, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 4, 3 * getWindowHeight() / 4, GLColor::blue);
}

// blit from system FBO to user-created framebuffer, with depth buffer.
TEST_P(BlitFramebufferANGLETest, BlitWithDepthDefaultToUser)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_framebuffer_blit"));

    glBindFramebuffer(GL_FRAMEBUFFER, mOriginalFBO);

    glDepthMask(GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glEnable(GL_DEPTH_TEST);

    EXPECT_GL_NO_ERROR();

    // Clear the first half of the screen
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, 0, getWindowWidth(), getWindowHeight() / 2);

    glClearDepthf(0.1f);
    glClearColor(1.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Scissor the second half of the screen
    glScissor(0, getWindowHeight() / 2, getWindowWidth(), getWindowHeight() / 2);

    glClearDepthf(0.9f);
    glClearColor(0.0, 1.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glDisable(GL_SCISSOR_TEST);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, mUserFBO);
    glBindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, mOriginalFBO);

    glBlitFramebufferANGLE(0, 0, getWindowWidth(), getWindowHeight(), 0, 0, getWindowWidth(),
                           getWindowHeight(), GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT,
                           GL_NEAREST);
    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_FRAMEBUFFER, mUserFBO);

    // if blit is happening correctly, this quad will draw only on the bottom half since it will be
    // behind on the first half and in front on the second half.
    drawQuad(mBlueProgram, essl1_shaders::PositionAttrib(), 0.5f);

    glDisable(GL_DEPTH_TEST);

    EXPECT_PIXEL_EQ(getWindowWidth() / 4, getWindowHeight() / 4, 255, 0, 0, 255);
    EXPECT_PIXEL_EQ(3 * getWindowWidth() / 4, getWindowHeight() / 4, 255, 0, 0, 255);
    EXPECT_PIXEL_EQ(3 * getWindowWidth() / 4, 3 * getWindowHeight() / 4, 0, 0, 255, 255);
    EXPECT_PIXEL_EQ(getWindowWidth() / 4, 3 * getWindowHeight() / 4, 0, 0, 255, 255);
}

// blit from one region of the system fbo to another-- this should fail.
TEST_P(BlitFramebufferANGLETest, BlitSameBufferOriginal)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_framebuffer_blit"));

    glBindFramebuffer(GL_FRAMEBUFFER, mOriginalFBO);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    drawQuad(mCheckerProgram, essl1_shaders::PositionAttrib(), 0.3f);

    EXPECT_GL_NO_ERROR();

    glBlitFramebufferANGLE(0, 0, getWindowWidth() / 2, getWindowHeight(), getWindowWidth() / 2, 0,
                           getWindowWidth(), getWindowHeight(), GL_COLOR_BUFFER_BIT, GL_NEAREST);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// blit from one region of the system fbo to another.
TEST_P(BlitFramebufferANGLETest, BlitSameBufferUser)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_framebuffer_blit"));

    glBindFramebuffer(GL_FRAMEBUFFER, mUserFBO);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    drawQuad(mCheckerProgram, essl1_shaders::PositionAttrib(), 0.3f);

    EXPECT_GL_NO_ERROR();

    glBlitFramebufferANGLE(0, 0, getWindowWidth() / 2, getWindowHeight(), getWindowWidth() / 2, 0,
                           getWindowWidth(), getWindowHeight(), GL_COLOR_BUFFER_BIT, GL_NEAREST);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

TEST_P(BlitFramebufferANGLETest, BlitPartialColor)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_framebuffer_blit"));

    glBindFramebuffer(GL_FRAMEBUFFER, mUserFBO);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    drawQuad(mCheckerProgram, essl1_shaders::PositionAttrib(), 0.5f);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, mOriginalFBO);
    glBindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, mUserFBO);

    glClearColor(1.0, 1.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glBlitFramebufferANGLE(0, 0, getWindowWidth() / 2, getWindowHeight() / 2, 0,
                           getWindowHeight() / 2, getWindowWidth() / 2, getWindowHeight(),
                           GL_COLOR_BUFFER_BIT, GL_NEAREST);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_FRAMEBUFFER, mOriginalFBO);

    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 4, getWindowHeight() / 4, GLColor::white);
    EXPECT_PIXEL_COLOR_EQ(3 * getWindowWidth() / 4, getWindowHeight() / 4, GLColor::white);
    EXPECT_PIXEL_COLOR_EQ(3 * getWindowWidth() / 4, 3 * getWindowHeight() / 4, GLColor::white);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 4, 3 * getWindowHeight() / 4, GLColor::red);
}

TEST_P(BlitFramebufferANGLETest, BlitDifferentSizes)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_framebuffer_blit"));

    glBindFramebuffer(GL_FRAMEBUFFER, mUserFBO);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    drawQuad(mCheckerProgram, essl1_shaders::PositionAttrib(), 0.5f);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, mSmallFBO);
    glBindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, mUserFBO);

    glClearColor(1.0, 1.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    glBlitFramebufferANGLE(0, 0, getWindowWidth(), getWindowHeight(), 0, 0, getWindowWidth(),
                           getWindowHeight(), GL_COLOR_BUFFER_BIT, GL_NEAREST);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_FRAMEBUFFER, mSmallFBO);

    EXPECT_PIXEL_EQ(getWindowWidth() / 4, getWindowHeight() / 4, 255, 0, 0, 255);

    EXPECT_GL_NO_ERROR();
}

TEST_P(BlitFramebufferANGLETest, BlitWithMissingAttachments)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_framebuffer_blit"));

    glBindFramebuffer(GL_FRAMEBUFFER, mColorOnlyFBO);

    glClear(GL_COLOR_BUFFER_BIT);
    drawQuad(mCheckerProgram, essl1_shaders::PositionAttrib(), 0.3f);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, mOriginalFBO);
    glBindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, mColorOnlyFBO);

    glClearColor(1.0, 1.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // generate INVALID_OPERATION if the read FBO has no depth attachment
    glBlitFramebufferANGLE(0, 0, getWindowWidth(), getWindowHeight(), 0, 0, getWindowWidth(),
                           getWindowHeight(), GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT,
                           GL_NEAREST);

    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // generate INVALID_OPERATION if the read FBO has no stencil attachment
    glBlitFramebufferANGLE(0, 0, getWindowWidth(), getWindowHeight(), 0, 0, getWindowWidth(),
                           getWindowHeight(), GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT,
                           GL_NEAREST);

    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // generate INVALID_OPERATION if we read from a missing color attachment
    glReadBuffer(GL_COLOR_ATTACHMENT1);
    glBlitFramebufferANGLE(0, 0, getWindowWidth(), getWindowHeight(), 0, 0, getWindowWidth(),
                           getWindowHeight(), GL_COLOR_BUFFER_BIT, GL_NEAREST);

    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

TEST_P(BlitFramebufferANGLETest, BlitStencil)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_framebuffer_blit"));

    // http://anglebug.com/2205
    ANGLE_SKIP_TEST_IF(IsIntel() && IsD3D9());

    glBindFramebuffer(GL_FRAMEBUFFER, mUserFBO);

    glClearColor(0.0, 1.0, 0.0, 1.0);
    glClearStencil(0x0);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Scissor half the screen so we fill the stencil only halfway
    glScissor(0, 0, getWindowWidth(), getWindowHeight() / 2);
    glEnable(GL_SCISSOR_TEST);

    // fill the stencil buffer with 0x1
    glStencilFunc(GL_ALWAYS, 0x1, 0xFF);
    glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
    glEnable(GL_STENCIL_TEST);
    drawQuad(mRedProgram, essl1_shaders::PositionAttrib(), 0.3f);

    glDisable(GL_SCISSOR_TEST);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, mOriginalFBO);
    glBindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, mUserFBO);

    // These clears are not useful in theory because we're copying over them, but its
    // helpful in debugging if we see white in any result.
    glClearColor(1.0, 1.0, 1.0, 1.0);
    glClearStencil(0x0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // depth blit request should be silently ignored, because the read FBO has no depth attachment
    glBlitFramebufferANGLE(0, 0, getWindowWidth(), getWindowHeight(), 0, 0, getWindowWidth(),
                           getWindowHeight(), GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT,
                           GL_NEAREST);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_FRAMEBUFFER, mOriginalFBO);

    EXPECT_PIXEL_EQ(getWindowWidth() / 4, getWindowHeight() / 4, 255, 0, 0, 255);
    EXPECT_PIXEL_EQ(3 * getWindowWidth() / 4, getWindowHeight() / 4, 255, 0, 0, 255);
    EXPECT_PIXEL_EQ(3 * getWindowWidth() / 4, 3 * getWindowHeight() / 4, 0, 255, 0, 255);
    EXPECT_PIXEL_EQ(getWindowWidth() / 4, 3 * getWindowHeight() / 4, 0, 255, 0, 255);

    glStencilFunc(GL_EQUAL, 0x1, 0xFF);  // only pass if stencil buffer at pixel reads 0x1

    drawQuad(mBlueProgram, essl1_shaders::PositionAttrib(),
             0.8f);  // blue quad will draw if stencil buffer was copied

    glDisable(GL_STENCIL_TEST);

    EXPECT_PIXEL_EQ(getWindowWidth() / 4, getWindowHeight() / 4, 0, 0, 255, 255);
    EXPECT_PIXEL_EQ(3 * getWindowWidth() / 4, getWindowHeight() / 4, 0, 0, 255, 255);
    EXPECT_PIXEL_EQ(3 * getWindowWidth() / 4, 3 * getWindowHeight() / 4, 0, 255, 0, 255);
    EXPECT_PIXEL_EQ(getWindowWidth() / 4, 3 * getWindowHeight() / 4, 0, 255, 0, 255);
}

// make sure that attempting to blit a partial depth buffer issues an error
TEST_P(BlitFramebufferANGLETest, BlitPartialDepthStencil)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_framebuffer_blit"));

    glBindFramebuffer(GL_FRAMEBUFFER, mUserFBO);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    drawQuad(mCheckerProgram, essl1_shaders::PositionAttrib(), 0.5f);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, mOriginalFBO);
    glBindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, mUserFBO);

    glBlitFramebufferANGLE(0, 0, getWindowWidth() / 2, getWindowHeight() / 2, 0, 0,
                           getWindowWidth() / 2, getWindowHeight() / 2, GL_DEPTH_BUFFER_BIT,
                           GL_NEAREST);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Test blit with MRT framebuffers
TEST_P(BlitFramebufferANGLETest, BlitMRT)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_framebuffer_blit"));

    if (!IsGLExtensionEnabled("GL_EXT_draw_buffers"))
    {
        return;
    }

    GLenum drawBuffers[] = {GL_COLOR_ATTACHMENT0_EXT, GL_COLOR_ATTACHMENT1_EXT};

    glBindFramebuffer(GL_FRAMEBUFFER, mMRTFBO);
    glDrawBuffersEXT(2, drawBuffers);

    glBindFramebuffer(GL_FRAMEBUFFER, mColorOnlyFBO);

    glClear(GL_COLOR_BUFFER_BIT);

    drawQuad(mCheckerProgram, essl1_shaders::PositionAttrib(), 0.8f);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, mColorOnlyFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, mMRTFBO);

    glBlitFramebufferANGLE(0, 0, getWindowWidth(), getWindowHeight(), 0, 0, getWindowWidth(),
                           getWindowHeight(), GL_COLOR_BUFFER_BIT, GL_NEAREST);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_FRAMEBUFFER, mMRTFBO);

    EXPECT_PIXEL_EQ(getWindowWidth() / 4, getWindowHeight() / 4, 255, 0, 0, 255);
    EXPECT_PIXEL_EQ(3 * getWindowWidth() / 4, getWindowHeight() / 4, 0, 255, 0, 255);
    EXPECT_PIXEL_EQ(3 * getWindowWidth() / 4, 3 * getWindowHeight() / 4, 255, 0, 0, 255);
    EXPECT_PIXEL_EQ(getWindowWidth() / 4, 3 * getWindowHeight() / 4, 0, 255, 0, 255);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1_EXT, GL_TEXTURE_2D, 0, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mMRTColorBuffer0,
                           0);

    EXPECT_PIXEL_EQ(getWindowWidth() / 4, getWindowHeight() / 4, 255, 0, 0, 255);
    EXPECT_PIXEL_EQ(3 * getWindowWidth() / 4, getWindowHeight() / 4, 0, 255, 0, 255);
    EXPECT_PIXEL_EQ(3 * getWindowWidth() / 4, 3 * getWindowHeight() / 4, 255, 0, 0, 255);
    EXPECT_PIXEL_EQ(getWindowWidth() / 4, 3 * getWindowHeight() / 4, 0, 255, 0, 255);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mMRTColorBuffer0,
                           0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1_EXT, GL_TEXTURE_2D,
                           mMRTColorBuffer1, 0);
}

// Test multisampled framebuffer blits if supported
TEST_P(BlitFramebufferANGLETest, MultisampledRGBAToRGBA)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_framebuffer_blit"));

    if (!checkExtension("GL_ANGLE_framebuffer_multisample"))
        return;

    if (!checkExtension("GL_OES_rgb8_rgba8"))
        return;

    multisampleTestHelper(mRGBAMultisampledFBO, mRGBAFBO);
}

TEST_P(BlitFramebufferANGLETest, MultisampledRGBAToBGRA)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_framebuffer_blit"));

    if (!checkExtension("GL_ANGLE_framebuffer_multisample"))
        return;

    if (!checkExtension("GL_OES_rgb8_rgba8"))
        return;

    if (!checkExtension("GL_EXT_texture_format_BGRA8888"))
        return;

    multisampleTestHelper(mRGBAMultisampledFBO, mBGRAFBO);
}

TEST_P(BlitFramebufferANGLETest, MultisampledBGRAToRGBA)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_framebuffer_blit"));

    if (!checkExtension("GL_ANGLE_framebuffer_multisample"))
        return;

    if (!checkExtension("GL_OES_rgb8_rgba8"))
        return;

    if (!checkExtension("GL_EXT_texture_format_BGRA8888"))
        return;

    multisampleTestHelper(mBGRAMultisampledFBO, mRGBAFBO);
}

TEST_P(BlitFramebufferANGLETest, MultisampledBGRAToBGRA)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_framebuffer_blit"));

    if (!checkExtension("GL_ANGLE_framebuffer_multisample"))
        return;

    if (!checkExtension("GL_OES_rgb8_rgba8"))
        return;

    if (!checkExtension("GL_EXT_texture_format_BGRA8888"))
        return;

    multisampleTestHelper(mBGRAMultisampledFBO, mBGRAFBO);
}

// Make sure that attempts to stretch in a blit call issue an error
TEST_P(BlitFramebufferANGLETest, ErrorStretching)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_framebuffer_blit"));

    glBindFramebuffer(GL_FRAMEBUFFER, mUserFBO);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    drawQuad(mCheckerProgram, essl1_shaders::PositionAttrib(), 0.5f);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, mOriginalFBO);
    glBindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, mUserFBO);

    glBlitFramebufferANGLE(0, 0, getWindowWidth() / 2, getWindowHeight() / 2, 0, 0,
                           getWindowWidth(), getWindowHeight(), GL_COLOR_BUFFER_BIT, GL_NEAREST);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Make sure that attempts to flip in a blit call issue an error
TEST_P(BlitFramebufferANGLETest, ErrorFlipping)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_framebuffer_blit"));

    glBindFramebuffer(GL_FRAMEBUFFER, mUserFBO);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    drawQuad(mCheckerProgram, essl1_shaders::PositionAttrib(), 0.5f);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, mOriginalFBO);
    glBindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, mUserFBO);

    glBlitFramebufferANGLE(0, 0, getWindowWidth() / 2, getWindowHeight() / 2, getWindowWidth() / 2,
                           getWindowHeight() / 2, 0, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

TEST_P(BlitFramebufferANGLETest, Errors)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_framebuffer_blit"));

    glBindFramebuffer(GL_FRAMEBUFFER, mUserFBO);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    drawQuad(mCheckerProgram, essl1_shaders::PositionAttrib(), 0.5f);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, mOriginalFBO);
    glBindFramebuffer(GL_READ_FRAMEBUFFER_ANGLE, mUserFBO);

    glBlitFramebufferANGLE(0, 0, getWindowWidth(), getWindowHeight(), 0, 0, getWindowWidth(),
                           getWindowHeight(), GL_COLOR_BUFFER_BIT, GL_LINEAR);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    glBlitFramebufferANGLE(0, 0, getWindowWidth(), getWindowHeight(), 0, 0, getWindowWidth(),
                           getWindowHeight(), GL_COLOR_BUFFER_BIT | 234, GL_NEAREST);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER_ANGLE, mDiffFormatFBO);

    glBlitFramebufferANGLE(0, 0, getWindowWidth(), getWindowHeight(), 0, 0, getWindowWidth(),
                           getWindowHeight(), GL_COLOR_BUFFER_BIT, GL_NEAREST);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// TODO(geofflang): Fix the dependence on glBlitFramebufferANGLE without checks and assuming the
// default framebuffer is BGRA to enable the GL and GLES backends. (http://anglebug.com/1289)

class BlitFramebufferTest : public ANGLETest
{
  protected:
    BlitFramebufferTest()
    {
        setWindowWidth(256);
        setWindowHeight(256);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setConfigDepthBits(24);
        setConfigStencilBits(8);
    }

    void initColorFBO(GLFramebuffer *fbo,
                      GLRenderbuffer *rbo,
                      GLenum rboFormat,
                      GLsizei width,
                      GLsizei height)
    {
        glBindRenderbuffer(GL_RENDERBUFFER, *rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, rboFormat, width, height);

        glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, *rbo);
    }

    void initColorFBOWithCheckerPattern(GLFramebuffer *fbo,
                                        GLRenderbuffer *rbo,
                                        GLenum rboFormat,
                                        GLsizei width,
                                        GLsizei height)
    {
        initColorFBO(fbo, rbo, rboFormat, width, height);

        ANGLE_GL_PROGRAM(checkerProgram, essl1_shaders::vs::Passthrough(),
                         essl1_shaders::fs::Checkered());
        glViewport(0, 0, width, height);
        glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
        drawQuad(checkerProgram.get(), essl1_shaders::PositionAttrib(), 0.5f);
    }
};

// Tests resolving a multisample depth buffer.
TEST_P(BlitFramebufferTest, MultisampleDepth)
{
    // TODO(oetuaho@nvidia.com): http://crbug.com/837717
    ANGLE_SKIP_TEST_IF(IsOpenGL() && IsOSX());

    GLRenderbuffer renderbuf;
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuf.get());
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 2, GL_DEPTH_COMPONENT24, 256, 256);

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.get());
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                              renderbuf.get());

    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    glClearDepthf(0.5f);
    glClear(GL_DEPTH_BUFFER_BIT);

    GLRenderbuffer destRenderbuf;
    glBindRenderbuffer(GL_RENDERBUFFER, destRenderbuf.get());
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 256, 256);

    GLFramebuffer resolved;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolved.get());
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                              destRenderbuf.get());

    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer.get());
    glBlitFramebuffer(0, 0, 256, 256, 0, 0, 256, 256, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, resolved.get());

    GLTexture colorbuf;
    glBindTexture(GL_TEXTURE_2D, colorbuf.get());
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 256, 256);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorbuf.get(), 0);

    ASSERT_GL_NO_ERROR();

    // Clear to green
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    // Make sure resulting depth is near 0.5f.
    ANGLE_GL_PROGRAM(drawRed, essl3_shaders::vs::Simple(), essl3_shaders::fs::Red());
    glEnable(GL_DEPTH_TEST);
    glDepthMask(false);
    glDepthFunc(GL_LESS);
    drawQuad(drawRed.get(), essl3_shaders::PositionAttrib(), -0.01f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(255, 0, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(0, 255, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(255, 255, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(127, 127, GLColor::red);

    ANGLE_GL_PROGRAM(drawBlue, essl3_shaders::vs::Simple(), essl3_shaders::fs::Blue());
    glEnable(GL_DEPTH_TEST);
    glDepthMask(false);
    glDepthFunc(GL_GREATER);
    drawQuad(drawBlue.get(), essl3_shaders::PositionAttrib(), 0.01f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(255, 0, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(0, 255, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(255, 255, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(127, 127, GLColor::blue);

    ASSERT_GL_NO_ERROR();
}

// Tests clearing a multisampled depth buffer.
TEST_P(BlitFramebufferTest, MultisampleDepthClear)
{
    // clearDepth && !maskDepth fails on Intel Ubuntu 19.04 Mesa 19.0.2 GL. http://anglebug.com/3614
    ANGLE_SKIP_TEST_IF(IsLinux() && IsIntel() && IsDesktopOpenGL());

    // http://anglebug.com/4092
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsOpenGLES());

    GLRenderbuffer depthMS;
    glBindRenderbuffer(GL_RENDERBUFFER, depthMS.get());
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 2, GL_DEPTH_COMPONENT24, 256, 256);

    GLRenderbuffer colorMS;
    glBindRenderbuffer(GL_RENDERBUFFER, colorMS.get());
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 2, GL_RGBA8, 256, 256);

    GLRenderbuffer colorResolved;
    glBindRenderbuffer(GL_RENDERBUFFER, colorResolved.get());
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 256, 256);

    GLFramebuffer framebufferMS;
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferMS.get());
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthMS.get());
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorMS.get());

    // Clear depth buffer to 0.5 and color to green.
    glClearDepthf(0.5f);
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    glFlush();

    // Draw red into the multisampled color buffer.
    ANGLE_GL_PROGRAM(drawRed, essl3_shaders::vs::Simple(), essl3_shaders::fs::Red());
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_EQUAL);
    drawQuad(drawRed.get(), essl3_shaders::PositionAttrib(), 0.0f);

    // Resolve the color buffer to make sure the above draw worked correctly, which in turn implies
    // that the multisampled depth clear worked.
    GLFramebuffer framebufferResolved;
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferResolved.get());
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                              colorResolved.get());
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebufferMS.get());
    glBlitFramebuffer(0, 0, 256, 256, 0, 0, 256, 256, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, framebufferResolved.get());

    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(255, 0, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(0, 255, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(255, 255, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(127, 127, GLColor::red);

    ASSERT_GL_NO_ERROR();
}

// Test resolving a multisampled stencil buffer.
TEST_P(BlitFramebufferTest, MultisampleStencil)
{
    // Incorrect rendering results seen on AMD Windows OpenGL. http://anglebug.com/2486
    ANGLE_SKIP_TEST_IF(IsAMD() && IsOpenGL() && IsWindows());

    GLRenderbuffer renderbuf;
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuf.get());
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 2, GL_STENCIL_INDEX8, 256, 256);

    ANGLE_GL_PROGRAM(drawRed, essl3_shaders::vs::Simple(), essl3_shaders::fs::Red());

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.get());
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              renderbuf.get());

    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // fill the stencil buffer with 0x1
    glStencilFunc(GL_ALWAYS, 0x1, 0xFF);
    glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
    glEnable(GL_STENCIL_TEST);
    drawQuad(drawRed.get(), essl3_shaders::PositionAttrib(), 0.5f);

    GLTexture destColorbuf;
    glBindTexture(GL_TEXTURE_2D, destColorbuf.get());
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 256, 256);

    GLRenderbuffer destRenderbuf;
    glBindRenderbuffer(GL_RENDERBUFFER, destRenderbuf.get());
    glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, 256, 256);

    GLFramebuffer resolved;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolved.get());
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           destColorbuf.get(), 0);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              destRenderbuf.get());

    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer.get());
    glBlitFramebuffer(0, 0, 256, 256, 0, 0, 256, 256, GL_STENCIL_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, resolved.get());

    ASSERT_GL_NO_ERROR();

    // Clear to green
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    // Draw red if the stencil is 0x1, which should be true after the resolve.
    glStencilFunc(GL_EQUAL, 0x1, 0xFF);
    drawQuad(drawRed.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    ASSERT_GL_NO_ERROR();
}

// Test resolving a multisampled stencil buffer with scissor.
TEST_P(BlitFramebufferTest, ScissoredMultisampleStencil)
{
    // Incorrect rendering results seen on AMD Windows OpenGL. http://anglebug.com/2486
    ANGLE_SKIP_TEST_IF(IsAMD() && IsOpenGL() && IsWindows());

    // Fails verifying that the middle pixel is red. http://anglebug.com/3496
    ANGLE_SKIP_TEST_IF((IsIntel() || IsAMD()) && IsOSX());

    constexpr GLuint kSize = 256;

    // Create the resolve framebuffer.
    GLTexture destColorbuf;
    glBindTexture(GL_TEXTURE_2D, destColorbuf.get());
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, kSize, kSize);

    GLRenderbuffer destRenderbuf;
    glBindRenderbuffer(GL_RENDERBUFFER, destRenderbuf.get());
    glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, kSize, kSize);

    GLFramebuffer resolved;
    glBindFramebuffer(GL_FRAMEBUFFER, resolved.get());
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           destColorbuf.get(), 0);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              destRenderbuf.get());

    // Clear the resolved buffer with gray and 0x10 stencil.
    GLColor gray(127, 127, 127, 255);
    glClearStencil(0x10);
    glClearColor(0.499f, 0.499f, 0.499f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    EXPECT_PIXEL_COLOR_EQ(0, 0, gray);
    EXPECT_PIXEL_COLOR_EQ(kSize / 2, kSize / 2, gray);

    // Create the multisampled framebuffer.
    GLRenderbuffer renderbuf;
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuf.get());
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 2, GL_STENCIL_INDEX8, kSize, kSize);

    ANGLE_GL_PROGRAM(drawRed, essl3_shaders::vs::Simple(), essl3_shaders::fs::Red());
    ANGLE_GL_PROGRAM(drawGreen, essl3_shaders::vs::Simple(), essl3_shaders::fs::Green());
    ANGLE_GL_PROGRAM(drawBlue, essl3_shaders::vs::Simple(), essl3_shaders::fs::Blue());

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.get());
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              renderbuf.get());

    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // Fill the stencil buffer with 0x1.
    glClearStencil(0x1);
    glClear(GL_STENCIL_BUFFER_BIT);

    // Fill a smaller region of the buffer with 0x2.
    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_STENCIL_TEST);
    glScissor(kSize / 4, kSize / 4, kSize / 2, kSize / 2);
    glStencilFunc(GL_ALWAYS, 0x2, 0xFF);
    glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
    drawQuad(drawRed.get(), essl3_shaders::PositionAttrib(), 0.5f);

    // Blit into the resolved framebuffer (with scissor still enabled).
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolved.get());
    glBlitFramebuffer(0, 0, kSize, kSize, 0, 0, kSize, kSize, GL_STENCIL_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, resolved.get());

    ASSERT_GL_NO_ERROR();

    // Draw blue if the stencil is 0x1, which should never be true.
    glDisable(GL_SCISSOR_TEST);
    glStencilMask(0);
    glStencilFunc(GL_EQUAL, 0x1, 0xFF);
    drawQuad(drawBlue.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, gray);
    EXPECT_PIXEL_COLOR_EQ(kSize - 1, 0, gray);
    EXPECT_PIXEL_COLOR_EQ(0, kSize - 1, gray);
    EXPECT_PIXEL_COLOR_EQ(kSize - 1, kSize - 1, gray);
    EXPECT_PIXEL_COLOR_EQ(kSize / 2, kSize / 2, gray);

    // Draw red if the stencil is 0x2, which should be true in the middle after the blit/resolve.
    glStencilFunc(GL_EQUAL, 0x2, 0xFF);
    drawQuad(drawRed.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, gray);
    EXPECT_PIXEL_COLOR_EQ(kSize - 1, 0, gray);
    EXPECT_PIXEL_COLOR_EQ(0, kSize - 1, gray);
    EXPECT_PIXEL_COLOR_EQ(kSize - 1, kSize - 1, gray);
    EXPECT_PIXEL_COLOR_EQ(kSize / 2, kSize / 2, GLColor::red);

    // Draw green if the stencil is 0x10, which should be left untouched in the outer regions.
    glStencilFunc(GL_EQUAL, 0x10, 0xFF);
    drawQuad(drawGreen.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(kSize - 1, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(0, kSize - 1, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(kSize - 1, kSize - 1, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(kSize / 2, kSize / 2, GLColor::red);

    ASSERT_GL_NO_ERROR();
}

// Blit an SRGB framebuffer and scale it.
TEST_P(BlitFramebufferTest, BlitSRGBToRGBAndScale)
{
    constexpr const GLsizei kWidth  = 256;
    constexpr const GLsizei kHeight = 256;

    GLRenderbuffer sourceRBO, targetRBO;
    GLFramebuffer sourceFBO, targetFBO;
    initColorFBOWithCheckerPattern(&sourceFBO, &sourceRBO, GL_SRGB8_ALPHA8, kWidth * 2,
                                   kHeight * 2);
    initColorFBO(&targetFBO, &targetRBO, GL_RGBA8, kWidth, kHeight);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, sourceFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, targetFBO);

    glViewport(0, 0, kWidth, kHeight);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Scale down without flipping.
    glBlitFramebuffer(0, 0, kWidth * 2, kHeight * 2, 0, 0, kWidth, kHeight, GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_FRAMEBUFFER, targetFBO);

    EXPECT_PIXEL_COLOR_EQ(kWidth / 4, kHeight / 4, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(3 * kWidth / 4, kHeight / 4, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(3 * kWidth / 4, 3 * kHeight / 4, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(kWidth / 4, 3 * kHeight / 4, GLColor::green);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, sourceFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, targetFBO);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Scale down and flip in the X direction.
    glBlitFramebuffer(0, 0, kWidth * 2, kHeight * 2, kWidth, 0, 0, kHeight, GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_FRAMEBUFFER, targetFBO);

    EXPECT_PIXEL_COLOR_EQ(kWidth / 4, kHeight / 4, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(3 * kWidth / 4, kHeight / 4, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(3 * kWidth / 4, 3 * kHeight / 4, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(kWidth / 4, 3 * kHeight / 4, GLColor::red);
}

// Blit stencil, with scissor and scale it.
TEST_P(BlitFramebufferTest, BlitStencilScissoredScaled)
{
    constexpr GLint kSize = 256;

    // Create the destination framebuffer.
    GLTexture destColorbuf;
    glBindTexture(GL_TEXTURE_2D, destColorbuf.get());
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, kSize, kSize);

    GLRenderbuffer destRenderbuf;
    glBindRenderbuffer(GL_RENDERBUFFER, destRenderbuf.get());
    glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, kSize, kSize);

    GLFramebuffer destFBO;
    glBindFramebuffer(GL_FRAMEBUFFER, destFBO.get());
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           destColorbuf.get(), 0);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              destRenderbuf.get());

    // Clear the destination buffer with gray and 0x10 stencil.
    GLColor gray(127, 127, 127, 255);
    glClearStencil(0x10);
    glClearColor(0.499f, 0.499f, 0.499f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    EXPECT_PIXEL_COLOR_EQ(0, 0, gray);
    EXPECT_PIXEL_COLOR_EQ(kSize / 2, kSize / 2, gray);

    // Create the source framebuffer.
    GLRenderbuffer renderbuf;
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuf.get());
    glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, kSize, kSize);

    ANGLE_GL_PROGRAM(drawRed, essl3_shaders::vs::Simple(), essl3_shaders::fs::Red());
    ANGLE_GL_PROGRAM(drawGreen, essl3_shaders::vs::Simple(), essl3_shaders::fs::Green());
    ANGLE_GL_PROGRAM(drawBlue, essl3_shaders::vs::Simple(), essl3_shaders::fs::Blue());

    GLFramebuffer sourceFBO;
    glBindFramebuffer(GL_FRAMEBUFFER, sourceFBO.get());
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              renderbuf.get());

    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // Fill the stencil buffer with 0x1.
    glClearStencil(0x1);
    glClear(GL_STENCIL_BUFFER_BIT);

    // Fill a smaller region of the buffer with 0x2.
    glEnable(GL_SCISSOR_TEST);
    glEnable(GL_STENCIL_TEST);
    glScissor(kSize / 4, kSize / 4, kSize / 2, kSize / 2);
    glStencilFunc(GL_ALWAYS, 0x2, 0xFF);
    glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
    drawQuad(drawRed.get(), essl3_shaders::PositionAttrib(), 0.5f);

    // Blit and scale down into the destination framebuffer (with scissor still enabled).
    //
    // Source looks like this:
    //
    //     +----|----|----|----+
    //     |                   |
    //     |       0x1         |
    //     -    +---------+    -
    //     |    |         |    |
    //     |    |         |    |
    //     -    |   0x2   |    -
    //     |    |         |    |
    //     |    |         |    |
    //     -    +---------+    -
    //     |                   |
    //     |                   |
    //     +----|----|----|----+
    //
    // We want the destination to look like this:
    //
    //     +----|----|----|----+
    //     |                   |
    //     |       0x10        |
    //     -    +---------+    -
    //     |    |  0x1    |    |
    //     |    |  +------+    |
    //     -    |  |      |    -
    //     |    |  | 0x2  |    |
    //     |    |  |      |    |
    //     -    +--+------+    -
    //     |                   |
    //     |                   |
    //     +----|----|----|----+
    //
    // The corresponding blit would be: (0, 0, 3/4, 3/4) -> (1/4, 1/4, 3/4, 3/4).  For testing, we
    // would like to avoid having the destination area and scissor to match.  Using destination
    // area as (0, 0, 1, 1), and keeping the same scaling, the source area should be
    // (-3/8, -3/8, 9/8, 9/8).
    //
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destFBO.get());
    constexpr GLint kBlitSrc[2] = {-3 * kSize / 8, 9 * kSize / 8};
    glBlitFramebuffer(kBlitSrc[0], kBlitSrc[0], kBlitSrc[1], kBlitSrc[1], 0, 0, kSize, kSize,
                      GL_STENCIL_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, destFBO.get());

    ASSERT_GL_NO_ERROR();

    // Draw blue if the stencil is 0x1, which should be true only in the top and left of the inner
    // square.
    glDisable(GL_SCISSOR_TEST);
    glStencilMask(0);
    glStencilFunc(GL_EQUAL, 0x1, 0xFF);
    drawQuad(drawBlue.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, gray);
    EXPECT_PIXEL_COLOR_EQ(kSize - 1, 0, gray);
    EXPECT_PIXEL_COLOR_EQ(0, kSize - 1, gray);
    EXPECT_PIXEL_COLOR_EQ(kSize - 1, kSize - 1, gray);

    EXPECT_PIXEL_COLOR_EQ(kSize / 4, kSize / 4, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(kSize / 4, 3 * kSize / 4 - 1, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(3 * kSize / 4 - 1, kSize / 4, GLColor::blue);

    EXPECT_PIXEL_COLOR_EQ(kSize / 2, kSize / 2, gray);
    EXPECT_PIXEL_COLOR_EQ(3 * kSize / 4 - 1, 3 * kSize / 4 - 1, gray);

    // Draw red if the stencil is 0x2, which should be true in the bottom/right of the middle
    // square after the blit.
    glStencilFunc(GL_EQUAL, 0x2, 0xFF);
    drawQuad(drawRed.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, gray);
    EXPECT_PIXEL_COLOR_EQ(kSize - 1, 0, gray);
    EXPECT_PIXEL_COLOR_EQ(0, kSize - 1, gray);
    EXPECT_PIXEL_COLOR_EQ(kSize - 1, kSize - 1, gray);

    EXPECT_PIXEL_COLOR_EQ(kSize / 4, kSize / 4, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(kSize / 4, 3 * kSize / 4 - 1, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(3 * kSize / 4 - 1, kSize / 4, GLColor::blue);

    EXPECT_PIXEL_COLOR_EQ(kSize / 2, kSize / 2, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(3 * kSize / 4 - 1, 3 * kSize / 4 - 1, GLColor::red);

    // Draw green if the stencil is 0x10, which should be left untouched in the outer regions.
    glStencilFunc(GL_EQUAL, 0x10, 0xFF);
    drawQuad(drawGreen.get(), essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(kSize - 1, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(0, kSize - 1, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(kSize - 1, kSize - 1, GLColor::green);

    EXPECT_PIXEL_COLOR_EQ(kSize / 4, kSize / 4, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(kSize / 4, 3 * kSize / 4 - 1, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(3 * kSize / 4 - 1, kSize / 4, GLColor::blue);

    EXPECT_PIXEL_COLOR_EQ(kSize / 2, kSize / 2, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(3 * kSize / 4 - 1, 3 * kSize / 4 - 1, GLColor::red);

    ASSERT_GL_NO_ERROR();
}

// Blit a subregion of an SRGB framebuffer to an RGB framebuffer.
TEST_P(BlitFramebufferTest, PartialBlitSRGBToRGB)
{
    constexpr const GLsizei kWidth  = 256;
    constexpr const GLsizei kHeight = 256;

    GLRenderbuffer sourceRBO, targetRBO;
    GLFramebuffer sourceFBO, targetFBO;
    initColorFBOWithCheckerPattern(&sourceFBO, &sourceRBO, GL_SRGB8_ALPHA8, kWidth * 2,
                                   kHeight * 2);
    initColorFBO(&targetFBO, &targetRBO, GL_RGBA8, kWidth, kHeight);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, sourceFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, targetFBO);

    glViewport(0, 0, kWidth, kHeight);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Blit a part of the source FBO without flipping.
    glBlitFramebuffer(kWidth, kHeight, kWidth * 2, kHeight * 2, 0, 0, kWidth, kHeight,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_FRAMEBUFFER, targetFBO);

    EXPECT_PIXEL_COLOR_EQ(kWidth / 4, kHeight / 4, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(3 * kWidth / 4, kHeight / 4, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(3 * kWidth / 4, 3 * kHeight / 4, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(kWidth / 4, 3 * kHeight / 4, GLColor::red);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, sourceFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, targetFBO);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Blit a part of the source FBO and flip in the X direction.
    glBlitFramebuffer(kWidth * 2, 0, kWidth, kHeight, kWidth, 0, 0, kHeight, GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_FRAMEBUFFER, targetFBO);

    EXPECT_PIXEL_COLOR_EQ(kWidth / 4, kHeight / 4, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(3 * kWidth / 4, kHeight / 4, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(3 * kWidth / 4, 3 * kHeight / 4, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(kWidth / 4, 3 * kHeight / 4, GLColor::green);
}

// Blit an SRGB framebuffer with an oversized source area (parts outside the source area should be
// clipped out).
TEST_P(BlitFramebufferTest, BlitSRGBToRGBOversizedSourceArea)
{
    constexpr const GLsizei kWidth  = 256;
    constexpr const GLsizei kHeight = 256;

    GLRenderbuffer sourceRBO, targetRBO;
    GLFramebuffer sourceFBO, targetFBO;
    initColorFBOWithCheckerPattern(&sourceFBO, &sourceRBO, GL_SRGB8_ALPHA8, kWidth, kHeight);
    initColorFBO(&targetFBO, &targetRBO, GL_RGBA8, kWidth, kHeight);

    EXPECT_GL_NO_ERROR();

    glViewport(0, 0, kWidth, kHeight);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, sourceFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, targetFBO);

    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Blit so that the source area gets placed at the center of the target FBO.
    // The width of the source area is 1/4 of the width of the target FBO.
    glBlitFramebuffer(-3 * kWidth / 2, -3 * kHeight / 2, 5 * kWidth / 2, 5 * kHeight / 2, 0, 0,
                      kWidth, kHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_FRAMEBUFFER, targetFBO);

    // Source FBO colors can be found in the middle of the target FBO.
    EXPECT_PIXEL_COLOR_EQ(7 * kWidth / 16, 7 * kHeight / 16, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(9 * kWidth / 16, 7 * kHeight / 16, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(9 * kWidth / 16, 9 * kHeight / 16, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(7 * kWidth / 16, 9 * kHeight / 16, GLColor::green);

    // Clear color should remain around the edges of the target FBO (WebGL 2.0 spec explicitly
    // requires this and ANGLE is expected to follow that).
    EXPECT_PIXEL_COLOR_EQ(kWidth / 4, kHeight / 4, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(3 * kWidth / 4, kHeight / 4, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(3 * kWidth / 4, 3 * kHeight / 4, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(kWidth / 4, 3 * kHeight / 4, GLColor::blue);
}

// Blit an SRGB framebuffer with an oversized dest area (even though the result is clipped, it
// should be scaled as if the whole dest area was used).
TEST_P(BlitFramebufferTest, BlitSRGBToRGBOversizedDestArea)
{
    constexpr const GLsizei kWidth  = 256;
    constexpr const GLsizei kHeight = 256;

    GLRenderbuffer sourceRBO, targetRBO;
    GLFramebuffer sourceFBO, targetFBO;
    initColorFBOWithCheckerPattern(&sourceFBO, &sourceRBO, GL_SRGB8_ALPHA8, kWidth, kHeight);
    initColorFBO(&targetFBO, &targetRBO, GL_RGBA8, kWidth, kHeight);

    EXPECT_GL_NO_ERROR();

    glViewport(0, 0, kWidth, kHeight);

    glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Dest is oversized but centered the same as source
    glBindFramebuffer(GL_READ_FRAMEBUFFER, sourceFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, targetFBO);

    glBlitFramebuffer(0, 0, kWidth, kHeight, -kWidth / 2, -kHeight / 2, 3 * kWidth / 2,
                      3 * kHeight / 2, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_FRAMEBUFFER, targetFBO);

    EXPECT_PIXEL_COLOR_EQ(1, 1, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(kWidth / 4, kHeight / 4, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(kWidth / 2 - 1, kHeight / 2 - 1, GLColor::red);

    EXPECT_PIXEL_COLOR_EQ(1, kWidth - 1, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(kWidth / 4, 3 * kHeight / 4, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(kWidth / 2 - 1, kHeight / 2 + 1, GLColor::green);

    EXPECT_PIXEL_COLOR_EQ(kWidth - 1, 1, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(3 * kWidth / 4, kHeight / 4, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(kWidth / 2 + 1, kHeight / 2 - 1, GLColor::green);

    EXPECT_PIXEL_COLOR_EQ(kWidth - 1, kHeight - 1, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(3 * kWidth / 4, 3 * kHeight / 4, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(kWidth / 2 + 1, kHeight / 2 + 1, GLColor::red);

    // Dest is oversized in the negative direction
    glBindFramebuffer(GL_READ_FRAMEBUFFER, sourceFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, targetFBO);

    glBlitFramebuffer(0, 0, kWidth, kHeight, -kWidth / 2, -kHeight / 2, kWidth, kHeight,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_FRAMEBUFFER, targetFBO);

    EXPECT_PIXEL_COLOR_EQ(1, 1, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(kWidth / 2 - 1, kHeight / 2 - 1, GLColor::red);

    EXPECT_PIXEL_COLOR_EQ(1, kWidth - 1, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(kWidth / 4, 3 * kHeight / 4, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(kWidth / 2 - 1, kHeight / 2 + 1, GLColor::red);

    EXPECT_PIXEL_COLOR_EQ(kWidth - 1, 1, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(3 * kWidth / 4, kHeight / 4, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(kWidth / 2 + 1, kHeight / 2 - 1, GLColor::red);

    EXPECT_PIXEL_COLOR_EQ(kWidth - 1, kHeight - 1, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(3 * kWidth / 4, 3 * kHeight / 4, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(kWidth / 2 + 1, kHeight / 2 + 1, GLColor::red);

    // Dest is oversized in the positive direction
    glBindFramebuffer(GL_READ_FRAMEBUFFER, sourceFBO);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, targetFBO);

    glBlitFramebuffer(0, 0, kWidth, kHeight, 0, 0, 3 * kWidth / 2, 3 * kHeight / 2,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_FRAMEBUFFER, targetFBO);

    EXPECT_PIXEL_COLOR_EQ(1, 1, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(kWidth / 4, kHeight / 4, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(kWidth / 2 - 1, kHeight / 2 - 1, GLColor::red);

    EXPECT_PIXEL_COLOR_EQ(1, kWidth - 1, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(kWidth / 4, 3 * kHeight / 4, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(kWidth / 2 - 1, kHeight / 2 + 1, GLColor::red);

    EXPECT_PIXEL_COLOR_EQ(kWidth - 1, 1, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(3 * kWidth / 4, kHeight / 4, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(kWidth / 2 + 1, kHeight / 2 - 1, GLColor::red);

    EXPECT_PIXEL_COLOR_EQ(kWidth - 1, kHeight - 1, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(kWidth / 2 + 1, kHeight / 2 + 1, GLColor::red);
}

// Test blitFramebuffer size overflow checks. WebGL 2.0 spec section 5.41. We do validation for
// overflows also in non-WebGL mode to avoid triggering driver bugs.
TEST_P(BlitFramebufferTest, BlitFramebufferSizeOverflow)
{
    GLTexture textures[2];
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexStorage2D(GL_TEXTURE_2D, 3, GL_RGBA8, 4, 4);
    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glTexStorage2D(GL_TEXTURE_2D, 3, GL_RGBA8, 4, 4);

    GLFramebuffer framebuffers[2];
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffers[0]);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffers[1]);

    ASSERT_GL_NO_ERROR();

    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[0],
                           0);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[1],
                           0);
    ASSERT_GL_NO_ERROR();

    // srcX
    glBlitFramebuffer(-1, 0, std::numeric_limits<GLint>::max(), 4, 0, 0, 4, 4, GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
    glBlitFramebuffer(std::numeric_limits<GLint>::max(), 0, -1, 4, 0, 0, 4, 4, GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // srcY
    glBlitFramebuffer(0, -1, 4, std::numeric_limits<GLint>::max(), 0, 0, 4, 4, GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
    glBlitFramebuffer(0, std::numeric_limits<GLint>::max(), 4, -1, 0, 0, 4, 4, GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // dstX
    glBlitFramebuffer(0, 0, 4, 4, -1, 0, std::numeric_limits<GLint>::max(), 4, GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
    glBlitFramebuffer(0, 0, 4, 4, std::numeric_limits<GLint>::max(), 0, -1, 4, GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // dstY
    glBlitFramebuffer(0, 0, 4, 4, 0, -1, 4, std::numeric_limits<GLint>::max(), GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
    glBlitFramebuffer(0, 0, 4, 4, 0, std::numeric_limits<GLint>::max(), 4, -1, GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Test blitFramebuffer size overflow checks. WebGL 2.0 spec section 5.41. Similar to above test,
// but this test more accurately duplicates the behavior of the WebGL test
// conformance2/rendering/blitframebuffer-size-overflow.html, which covers a few more edge cases.
TEST_P(BlitFramebufferTest, BlitFramebufferSizeOverflow2)
{
    GLTexture textures[2];
    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexStorage2D(GL_TEXTURE_2D, 3, GL_RGBA8, 4, 4);
    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glTexStorage2D(GL_TEXTURE_2D, 3, GL_RGBA8, 4, 4);

    GLFramebuffer framebuffers[2];
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffers[0]);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffers[1]);

    ASSERT_GL_NO_ERROR();

    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[0],
                           0);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[1],
                           0);
    ASSERT_GL_NO_ERROR();

    GLint width  = 8;
    GLint height = 8;

    GLTexture tex0;
    glBindTexture(GL_TEXTURE_2D, tex0);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

    GLFramebuffer fb0;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb0);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex0, 0);

    GLTexture tex1;
    glBindTexture(GL_TEXTURE_2D, tex1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

    GLFramebuffer fb1;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb1);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex1, 0);

    GLint max = std::numeric_limits<GLint>::max();
    // Using max 32-bit integer as blitFramebuffer parameter should succeed.
    glBlitFramebuffer(0, 0, max, max, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBlitFramebuffer(0, 0, width, height, 0, 0, max, max, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBlitFramebuffer(0, 0, max, max, 0, 0, max, max, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    EXPECT_GL_NO_ERROR();

    // Using blitFramebuffer parameters where calculated width/height matches max 32-bit integer
    // should succeed
    glBlitFramebuffer(-1, -1, max - 1, max - 1, 0, 0, width, height, GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
    glBlitFramebuffer(0, 0, width, height, -1, -1, max - 1, max - 1, GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
    glBlitFramebuffer(-1, -1, max - 1, max - 1, -1, -1, max - 1, max - 1, GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
    EXPECT_GL_NO_ERROR();

    // Using source width/height greater than max 32-bit integer should fail.
    glBlitFramebuffer(-1, -1, max, max, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // Using source width/height greater than max 32-bit integer should fail.
    glBlitFramebuffer(max, max, -1, -1, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // Using destination width/height greater than max 32-bit integer should fail.
    glBlitFramebuffer(0, 0, width, height, -1, -1, max, max, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // Using destination width/height greater than max 32-bit integer should fail.
    glBlitFramebuffer(0, 0, width, height, max, max, -1, -1, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // Using both source and destination width/height greater than max 32-bit integer should fail.
    glBlitFramebuffer(-1, -1, max, max, -1, -1, max, max, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    // Using minimum and maximum integers for all boundaries should fail.
    glBlitFramebuffer(-max - 1, -max - 1, max, max, -max - 1, -max - 1, max, max,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST(BlitFramebufferANGLETest,
                       ES2_D3D9(),
                       ES2_D3D11(),
                       ES2_D3D11_PRESENT_PATH_FAST(),
                       ES2_OPENGL(),
                       ES3_OPENGL(),
                       ES2_VULKAN(),
                       ES3_VULKAN());

ANGLE_INSTANTIATE_TEST_ES3(BlitFramebufferTest);
