//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

#include "common/gl_enum_utils.h"

#include <limits>

using namespace angle;

class DrawBuffersTest : public ANGLETest<>
{
  protected:
    DrawBuffersTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setConfigDepthBits(24);
    }

    void testTearDown() override
    {
        glDeleteFramebuffers(1, &mFBO);
        glDeleteFramebuffers(1, &mReadFramebuffer);
        glDeleteTextures(4, mTextures);
    }

    // We must call a different DrawBuffers method depending on extension support. Use this
    // method instead of calling on directly.
    void setDrawBuffers(GLsizei n, const GLenum *drawBufs)
    {
        if (IsGLExtensionEnabled("GL_EXT_draw_buffers"))
        {
            glDrawBuffersEXT(n, drawBufs);
        }
        else
        {
            ASSERT_GE(getClientMajorVersion(), 3);
            glDrawBuffers(n, drawBufs);
        }
    }

    // Use this method to filter if we can support these tests.
    bool setupTest()
    {
        if (getClientMajorVersion() < 3 && (!EnsureGLExtensionEnabled("GL_EXT_draw_buffers") ||
                                            !EnsureGLExtensionEnabled("GL_ANGLE_framebuffer_blit")))
        {
            return false;
        }

        // This test seems to fail on an nVidia machine when the window is hidden
        setWindowVisible(getOSWindow(), shouldShowWindow());

        glGenFramebuffers(1, &mFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mFBO);

        glGenTextures(4, mTextures);

        for (size_t texIndex = 0; texIndex < ArraySize(mTextures); texIndex++)
        {
            glBindTexture(GL_TEXTURE_2D, mTextures[texIndex]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, getWindowWidth(), getWindowHeight(), 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, nullptr);
        }

        glGetIntegerv(GL_MAX_DRAW_BUFFERS, &mMaxDrawBuffers);

        glGenFramebuffers(1, &mReadFramebuffer);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, mReadFramebuffer);

        return true;
    }

    void setupMRTProgramESSL3(bool bufferEnabled[8], GLuint *programOut)
    {
        std::stringstream strstr;

        strstr << "#version 300 es\n"
                  "precision highp float;\n";

        for (unsigned int index = 0; index < 8; index++)
        {
            if (bufferEnabled[index])
            {
                strstr << "layout(location = " << index
                       << ") "
                          "out vec4 value"
                       << index << ";\n";
            }
        }

        strstr << "void main()\n"
                  "{\n";

        for (unsigned int index = 0; index < 8; index++)
        {
            if (bufferEnabled[index])
            {
                unsigned int r = (index + 1) & 1;
                unsigned int g = (index + 1) & 2;
                unsigned int b = (index + 1) & 4;

                strstr << "    value" << index << " = vec4(" << r << ".0, " << g << ".0, " << b
                       << ".0, 1.0);\n";
            }
        }

        strstr << "}\n";

        *programOut = CompileProgram(essl3_shaders::vs::Simple(), strstr.str().c_str());
        if (*programOut == 0)
        {
            FAIL() << "shader compilation failed.";
        }
    }

    void setupMRTProgramESSL1(bool bufferEnabled[8], GLuint *programOut)
    {
        std::stringstream strstr;

        strstr << "#extension GL_EXT_draw_buffers : enable\n"
                  "precision highp float;\n"
                  "void main()\n"
                  "{\n";

        for (unsigned int index = 0; index < 8; index++)
        {
            if (bufferEnabled[index])
            {
                unsigned int r = (index + 1) & 1;
                unsigned int g = (index + 1) & 2;
                unsigned int b = (index + 1) & 4;

                strstr << "    gl_FragData[" << index << "] = vec4(" << r << ".0, " << g << ".0, "
                       << b << ".0, 1.0);\n";
            }
        }

        strstr << "}\n";

        *programOut = CompileProgram(essl1_shaders::vs::Simple(), strstr.str().c_str());
        if (*programOut == 0)
        {
            FAIL() << "shader compilation failed.";
        }
    }

    void setupMRTProgram(bool bufferEnabled[8], GLuint *programOut)
    {
        if (getClientMajorVersion() == 3)
        {
            setupMRTProgramESSL3(bufferEnabled, programOut);
        }
        else
        {
            ASSERT_EQ(getClientMajorVersion(), 2);
            setupMRTProgramESSL1(bufferEnabled, programOut);
        }
    }

    const char *positionAttrib()
    {
        if (getClientMajorVersion() == 3)
        {
            return essl3_shaders::PositionAttrib();
        }
        else
        {
            return essl1_shaders::PositionAttrib();
        }
    }

    static GLColor getColorForIndex(unsigned int index)
    {
        GLubyte r = (((index + 1) & 1) > 0) ? 255 : 0;
        GLubyte g = (((index + 1) & 2) > 0) ? 255 : 0;
        GLubyte b = (((index + 1) & 4) > 0) ? 255 : 0;
        return GLColor(r, g, b, 255u);
    }

    void verifyAttachment2DColor(unsigned int index,
                                 GLuint textureName,
                                 GLenum target,
                                 GLint level,
                                 GLColor color)
    {
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target, textureName,
                               level);
        EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, color)
            << "index " << index;
    }

    void verifyAttachment2DUnwritten(unsigned int index, GLuint texture, GLenum target, GLint level)
    {
        verifyAttachment2DColor(index, texture, target, level, GLColor::transparentBlack);
    }

    void verifyAttachment2D(unsigned int index, GLuint texture, GLenum target, GLint level)
    {
        verifyAttachment2DColor(index, texture, target, level, getColorForIndex(index));
    }

    void verifyAttachment3DOES(unsigned int index, GLuint texture, GLint level, GLint layer)
    {
        ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_3D"));

        glFramebufferTexture3DOES(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_3D, texture,
                                  level, layer);
        EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, getColorForIndex(index));
    }

    void verifyAttachmentLayer(unsigned int index, GLuint texture, GLint level, GLint layer)
    {
        glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture, level, layer);
        EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, getColorForIndex(index));
    }

    GLuint mFBO             = 0;
    GLuint mReadFramebuffer = 0;
    GLuint mTextures[4]     = {};
    GLint mMaxDrawBuffers   = 0;
};

class DrawBuffersWebGL2Test : public DrawBuffersTest
{
  public:
    DrawBuffersWebGL2Test()
    {
        setWebGLCompatibilityEnabled(true);
        setRobustResourceInit(true);
    }
};

// Verify that GL_MAX_DRAW_BUFFERS returns the expected values for D3D11
TEST_P(DrawBuffersTest, VerifyD3DLimits)
{
    EGLPlatformParameters platform = GetParam().eglParameters;

    ANGLE_SKIP_TEST_IF(platform.renderer != EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE);

    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &mMaxDrawBuffers);

    if (platform.majorVersion == 9 && platform.minorVersion == 3)
    {
        // D3D11 Feature Level 9_3 supports 4 draw buffers
        ASSERT_EQ(mMaxDrawBuffers, 4);
    }
    else
    {
        // D3D11 Feature Level 10_0+ supports 8 draw buffers
        ASSERT_EQ(mMaxDrawBuffers, 8);
    }
}

TEST_P(DrawBuffersTest, Gaps)
{
    ANGLE_SKIP_TEST_IF(!setupTest());

    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, mTextures[0], 0);

    bool flags[8] = {false, true};

    GLuint program;
    setupMRTProgram(flags, &program);

    const GLenum bufs[] = {GL_NONE, GL_COLOR_ATTACHMENT1};
    setDrawBuffers(2, bufs);
    drawQuad(program, positionAttrib(), 0.5);

    verifyAttachment2D(1, mTextures[0], GL_TEXTURE_2D, 0);

    glDeleteProgram(program);
}

// Test that blend works with gaps
TEST_P(DrawBuffersTest, BlendWithGaps)
{
    ANGLE_SKIP_TEST_IF(!setupTest());

    // http://anglebug.com/42263715
    ANGLE_SKIP_TEST_IF(IsMac() && IsIntel() && IsDesktopOpenGL());

    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, mTextures[0], 0);

    ASSERT_GL_NO_ERROR();

    bool flags[8] = {false, true};

    GLuint program;
    setupMRTProgram(flags, &program);

    const GLenum bufs[] = {GL_NONE, GL_COLOR_ATTACHMENT1};
    setDrawBuffers(2, bufs);

    // Draws green into attachment 1
    drawQuad(program, positionAttrib(), 0.5);
    verifyAttachment2D(1, mTextures[0], GL_TEXTURE_2D, 0);
    ASSERT_GL_NO_ERROR();

    // Clear with red
    glClearColor(1.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    verifyAttachment2DColor(1, mTextures[0], GL_TEXTURE_2D, 0, GLColor(255u, 0, 0, 255u));

    // Draw green into attachment 1 again but with blending, expecting yellow
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    drawQuad(program, positionAttrib(), 0.5);
    verifyAttachment2DColor(1, mTextures[0], GL_TEXTURE_2D, 0, GLColor(255u, 255u, 0, 255u));
    ASSERT_GL_NO_ERROR();

    glDeleteProgram(program);
}

// Test that clear works with gaps
TEST_P(DrawBuffersTest, ClearWithGaps)
{
    ANGLE_SKIP_TEST_IF(!setupTest());

    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &mMaxDrawBuffers);
    ASSERT_GE(mMaxDrawBuffers, 4);

    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0], 0);

    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, mTextures[1], 0);

    const GLenum bufs[] = {GL_COLOR_ATTACHMENT0, GL_NONE, GL_NONE, GL_COLOR_ATTACHMENT3};

    bool flags[8] = {true, false, false, true};
    GLuint program;
    setupMRTProgram(flags, &program);

    setDrawBuffers(4, bufs);

    glClearColor(1.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // A bogus draw to make sure clears are done with a render pass in the Vulkan backend.
    glEnable(GL_BLEND);
    glBlendFunc(GL_ZERO, GL_ONE);
    drawQuad(program, positionAttrib(), 0.5);
    EXPECT_GL_NO_ERROR();

    verifyAttachment2DColor(0, mTextures[0], GL_TEXTURE_2D, 0, GLColor::yellow);
    verifyAttachment2DColor(3, mTextures[1], GL_TEXTURE_2D, 0, GLColor::yellow);

    EXPECT_GL_NO_ERROR();
}

// Test that mid-render pass clear works with gaps
TEST_P(DrawBuffersTest, MidRenderPassClearWithGaps)
{
    ANGLE_SKIP_TEST_IF(!setupTest());

    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &mMaxDrawBuffers);
    ASSERT_GE(mMaxDrawBuffers, 4);

    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0], 0);

    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, mTextures[1], 0);

    const GLenum bufs[] = {GL_COLOR_ATTACHMENT0, GL_NONE, GL_NONE, GL_COLOR_ATTACHMENT3};

    bool flags[8] = {true, false, false, true};
    GLuint program;
    setupMRTProgram(flags, &program);

    setDrawBuffers(4, bufs);

    drawQuad(program, positionAttrib(), 0.5);

    glClearColor(1.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // A bogus draw to make sure clears are done with a render pass in the Vulkan backend.
    glEnable(GL_BLEND);
    glBlendFunc(GL_ZERO, GL_ONE);
    drawQuad(program, positionAttrib(), 0.5);
    EXPECT_GL_NO_ERROR();

    verifyAttachment2DColor(0, mTextures[0], GL_TEXTURE_2D, 0, GLColor::yellow);
    verifyAttachment2DColor(3, mTextures[1], GL_TEXTURE_2D, 0, GLColor::yellow);

    EXPECT_GL_NO_ERROR();
}

// Test that mid-render pass clear works with gaps.  Uses RGB format.
TEST_P(DrawBuffersTest, MidRenderPassClearWithGapsRGB)
{
    ANGLE_SKIP_TEST_IF(!setupTest());

    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &mMaxDrawBuffers);
    ASSERT_GE(mMaxDrawBuffers, 4);

    GLTexture textures[2];

    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, getWindowWidth(), getWindowHeight(), 0, GL_RGB,
                 GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[0], 0);

    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, getWindowWidth(), getWindowHeight(), 0, GL_RGB,
                 GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, textures[1], 0);

    const GLenum bufs[] = {GL_COLOR_ATTACHMENT0, GL_NONE, GL_NONE, GL_COLOR_ATTACHMENT3};

    bool flags[8] = {true, false, false, true};
    GLuint program;
    setupMRTProgram(flags, &program);

    setDrawBuffers(4, bufs);

    drawQuad(program, positionAttrib(), 0.5);

    glClearColor(1.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // A bogus draw to make sure clears are done with a render pass in the Vulkan backend.
    glEnable(GL_BLEND);
    glBlendFunc(GL_ZERO, GL_ONE);
    drawQuad(program, positionAttrib(), 0.5);
    EXPECT_GL_NO_ERROR();

    verifyAttachment2DColor(0, textures[0], GL_TEXTURE_2D, 0, GLColor::yellow);
    verifyAttachment2DColor(3, textures[1], GL_TEXTURE_2D, 0, GLColor::yellow);

    EXPECT_GL_NO_ERROR();
}

// Test that a masked draw and a mid-render pass clear works with gaps.
TEST_P(DrawBuffersTest, MaskedDrawMidRPClearWithGaps)
{
    ANGLE_SKIP_TEST_IF(!setupTest());

    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &mMaxDrawBuffers);
    ASSERT_GE(mMaxDrawBuffers, 4);

    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0], 0);

    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, mTextures[1], 0);

    // Mask out attachment 3, so we only draw to attachment 1.
    GLenum bufs[] = {GL_COLOR_ATTACHMENT0, GL_NONE, GL_NONE, GL_NONE};
    bool flags[8] = {true, false, false, false};
    GLuint program;
    setupMRTProgram(flags, &program);

    setDrawBuffers(4, bufs);

    drawQuad(program, positionAttrib(), 0.5);

    // Re-enable attachment 3, so we clear both attachment 1 and 3.
    bufs[3] = GL_COLOR_ATTACHMENT3;
    setDrawBuffers(4, bufs);
    flags[3] = true;
    setupMRTProgram(flags, &program);

    glClearColor(1.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // A bogus draw to make sure clears are done with a render pass in the Vulkan backend.
    glEnable(GL_BLEND);
    glBlendFunc(GL_ZERO, GL_ONE);
    drawQuad(program, positionAttrib(), 0.5);
    EXPECT_GL_NO_ERROR();

    verifyAttachment2DColor(0, mTextures[0], GL_TEXTURE_2D, 0, GLColor::yellow);
    verifyAttachment2DColor(3, mTextures[1], GL_TEXTURE_2D, 0, GLColor::yellow);

    EXPECT_GL_NO_ERROR();
}

// Test that a masked draw and a mid-render pass clear works with gaps.  Uses RGB format.
TEST_P(DrawBuffersTest, MaskedDrawMidRPClearWithGapsRGB)
{
    ANGLE_SKIP_TEST_IF(!setupTest());

    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &mMaxDrawBuffers);
    ASSERT_GE(mMaxDrawBuffers, 4);

    GLTexture textures[2];

    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, getWindowWidth(), getWindowHeight(), 0, GL_RGB,
                 GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[0], 0);

    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, getWindowWidth(), getWindowHeight(), 0, GL_RGB,
                 GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, textures[1], 0);

    // Mask out attachment 3, so we only draw to attachment 1.
    GLenum bufs[] = {GL_COLOR_ATTACHMENT0, GL_NONE, GL_NONE, GL_NONE};
    bool flags[8] = {true, false, false, false};
    GLuint program;
    setupMRTProgram(flags, &program);

    setDrawBuffers(4, bufs);

    drawQuad(program, positionAttrib(), 0.5);

    // Re-enable attachment 3, so we clear both attachment 1 and 3.
    bufs[3] = GL_COLOR_ATTACHMENT3;
    setDrawBuffers(4, bufs);
    flags[3] = true;
    setupMRTProgram(flags, &program);

    glClearColor(1.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // A bogus draw to make sure clears are done with a render pass in the Vulkan backend.
    glEnable(GL_BLEND);
    glBlendFunc(GL_ZERO, GL_ONE);
    drawQuad(program, positionAttrib(), 0.5);
    EXPECT_GL_NO_ERROR();

    verifyAttachment2DColor(0, textures[0], GL_TEXTURE_2D, 0, GLColor::yellow);
    verifyAttachment2DColor(3, textures[1], GL_TEXTURE_2D, 0, GLColor::yellow);

    EXPECT_GL_NO_ERROR();
}

TEST_P(DrawBuffersTest, FirstAndLast)
{
    ANGLE_SKIP_TEST_IF(!setupTest());

    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0], 0);

    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_2D, mTextures[1], 0);

    bool flags[8] = {true, false, false, true};

    GLuint program;
    setupMRTProgram(flags, &program);

    const GLenum bufs[] = {GL_COLOR_ATTACHMENT0, GL_NONE, GL_NONE, GL_COLOR_ATTACHMENT3};

    setDrawBuffers(4, bufs);
    drawQuad(program, positionAttrib(), 0.5);

    verifyAttachment2D(0, mTextures[0], GL_TEXTURE_2D, 0);
    verifyAttachment2D(3, mTextures[1], GL_TEXTURE_2D, 0);

    EXPECT_GL_NO_ERROR();

    glDeleteProgram(program);
}

TEST_P(DrawBuffersTest, FirstHalfNULL)
{
    ANGLE_SKIP_TEST_IF(!setupTest());

    bool flags[8]  = {false};
    GLenum bufs[8] = {GL_NONE};

    ASSERT_GT(mMaxDrawBuffers, 0);
    ASSERT_LE(mMaxDrawBuffers, 8);
    GLuint halfMaxDrawBuffers = static_cast<GLuint>(mMaxDrawBuffers) / 2;

    for (GLuint texIndex = 0; texIndex < halfMaxDrawBuffers; texIndex++)
    {
        glBindTexture(GL_TEXTURE_2D, mTextures[texIndex]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + halfMaxDrawBuffers + texIndex,
                               GL_TEXTURE_2D, mTextures[texIndex], 0);
        flags[texIndex + halfMaxDrawBuffers] = true;
        bufs[texIndex + halfMaxDrawBuffers]  = GL_COLOR_ATTACHMENT0 + halfMaxDrawBuffers + texIndex;
    }

    GLuint program;
    setupMRTProgram(flags, &program);

    setDrawBuffers(mMaxDrawBuffers, bufs);
    drawQuad(program, positionAttrib(), 0.5);

    for (GLuint texIndex = 0; texIndex < halfMaxDrawBuffers; texIndex++)
    {
        verifyAttachment2D(texIndex + halfMaxDrawBuffers, mTextures[texIndex], GL_TEXTURE_2D, 0);
    }

    EXPECT_GL_NO_ERROR();

    glDeleteProgram(program);
}

// Test draw buffers query on the default framebuffer
TEST_P(DrawBuffersTest, DefaultFramebufferDrawBufferQuery)
{
    ANGLE_SKIP_TEST_IF(!setupTest());

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    EGLWindow *window  = getEGLWindow();
    EGLDisplay display = window->getDisplay();
    EGLSurface surface = window->getSurface();
    GLint drawbuffer = 0;

    if (EGL_NO_SURFACE != surface)
    {
        // Check that the draw buffer state is GL_BACK
        glGetIntegerv(GL_DRAW_BUFFER0, &drawbuffer);
        EXPECT_GL_NO_ERROR();
        EXPECT_EQ(GL_BACK, drawbuffer);
    }

    // Test that non-zero draw buffers can be queried on the default framebuffer
    glGetIntegerv(GL_DRAW_BUFFER1, &drawbuffer);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(GL_NONE, drawbuffer);

    // Make our context current with no surfaces
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, window->getContext());
    // Check that the draw buffer state is GL_NONE
    glGetIntegerv(GL_DRAW_BUFFER0, &drawbuffer);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(GL_NONE, drawbuffer);

    if (EGL_NO_SURFACE != surface)
    {
        // Make our context current with a surface again
        eglMakeCurrent(display, surface, surface, window->getContext());
        // Test that the draw buffer state is GL_BACK once again
        glGetIntegerv(GL_DRAW_BUFFER0, &drawbuffer);
        EXPECT_GL_NO_ERROR();
        EXPECT_EQ(GL_BACK, drawbuffer);
    }
}

// Test that drawing with all color buffers disabled works.
TEST_P(DrawBuffersTest, None)
{
    ANGLE_SKIP_TEST_IF(!setupTest());

    bool flags[8]  = {false};
    GLenum bufs[8] = {GL_NONE};
    GLTexture textures[8];

    ASSERT_GT(mMaxDrawBuffers, 0);
    ASSERT_LE(mMaxDrawBuffers, 8);
    for (GLint texIndex = 0; texIndex < mMaxDrawBuffers; ++texIndex)
    {
        glBindTexture(GL_TEXTURE_2D, textures[texIndex]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, getWindowWidth(), getWindowHeight(), 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, nullptr);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + texIndex, GL_TEXTURE_2D,
                               textures[texIndex], 0);
        flags[texIndex] = true;
        bufs[texIndex]  = GL_COLOR_ATTACHMENT0 + texIndex;
    }

    GLuint program;
    setupMRTProgram(flags, &program);

    setDrawBuffers(mMaxDrawBuffers, bufs);
    glClearColor(0.5, 0.5, 0.5, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);

    for (GLint texIndex = 0; texIndex < mMaxDrawBuffers; ++texIndex)
    {
        bufs[texIndex] = GL_NONE;
    }

    setDrawBuffers(mMaxDrawBuffers, bufs);
    drawQuad(program, positionAttrib(), 0.5);

    ASSERT_GL_NO_ERROR();

    for (GLint texIndex = 0; texIndex < mMaxDrawBuffers; ++texIndex)
    {
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               textures[texIndex], 0);
        EXPECT_PIXEL_NEAR(getWindowWidth() / 2, getWindowHeight() / 2, 127, 127, 127, 255, 1);
    }

    glDeleteProgram(program);
}

// Test that drawing with a color buffer disabled and a depth buffer enabled works.
TEST_P(DrawBuffersTest, NoneWithDepth)
{
    ANGLE_SKIP_TEST_IF(!setupTest());

    bool flags[8]  = {true, false, false, false, false, false, false, false};
    GLenum bufs[8] = {
        GL_COLOR_ATTACHMENT0, GL_NONE, GL_NONE, GL_NONE, GL_NONE, GL_NONE, GL_NONE, GL_NONE};

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, getWindowWidth(), getWindowHeight(), 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    GLRenderbuffer rb;
    glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, getWindowWidth(),
                          getWindowHeight());
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rb);

    EXPECT_GL_NO_ERROR();
    EXPECT_GL_FRAMEBUFFER_COMPLETE(GL_DRAW_FRAMEBUFFER);

    GLuint program;
    setupMRTProgram(flags, &program);

    ASSERT_GT(mMaxDrawBuffers, 0);
    ASSERT_LE(mMaxDrawBuffers, 8);
    setDrawBuffers(mMaxDrawBuffers, bufs);

    glClearColor(0.5, 0.5, 0.5, 1.0);
    glClearDepthf(0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    EXPECT_PIXEL_NEAR(getWindowWidth() / 2, getWindowHeight() / 2, 127, 127, 127, 255, 1);

    // Color buffer must remain untouched, depth buffer must be set to 1.0
    bufs[0] = GL_NONE;
    setDrawBuffers(mMaxDrawBuffers, bufs);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);
    drawQuad(program, positionAttrib(), 1.0);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_NEAR(getWindowWidth() / 2, getWindowHeight() / 2, 127, 127, 127, 255, 1);

    // Draw with the color buffer and depth test enabled.
    // Depth test must fail and the color buffer must remain unchanged.
    bufs[0] = GL_COLOR_ATTACHMENT0;
    setDrawBuffers(mMaxDrawBuffers, bufs);
    glDepthFunc(GL_LESS);
    drawQuad(program, positionAttrib(), 1.0);
    EXPECT_PIXEL_NEAR(getWindowWidth() / 2, getWindowHeight() / 2, 127, 127, 127, 255, 1);

    // Draw with another Z value.
    // Depth test must pass and the color buffer must be updated.
    drawQuad(program, positionAttrib(), 0.0);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::red);

    glDeleteProgram(program);
}

// Test that drawing with a color buffer disabled and a stencil buffer enabled works.
TEST_P(DrawBuffersTest, NoneWithStencil)
{
    ANGLE_SKIP_TEST_IF(!setupTest());

    bool flags[8]  = {true, false, false, false, false, false, false, false};
    GLenum bufs[8] = {
        GL_COLOR_ATTACHMENT0, GL_NONE, GL_NONE, GL_NONE, GL_NONE, GL_NONE, GL_NONE, GL_NONE};

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, getWindowWidth(), getWindowHeight(), 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    GLRenderbuffer rb;
    glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, getWindowWidth(), getWindowHeight());
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb);

    EXPECT_GL_NO_ERROR();
    EXPECT_GL_FRAMEBUFFER_COMPLETE(GL_DRAW_FRAMEBUFFER);

    GLuint program;
    setupMRTProgram(flags, &program);

    ASSERT_GT(mMaxDrawBuffers, 0);
    ASSERT_LE(mMaxDrawBuffers, 8);
    setDrawBuffers(mMaxDrawBuffers, bufs);

    glClearColor(0.5, 0.5, 0.5, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    EXPECT_PIXEL_NEAR(getWindowWidth() / 2, getWindowHeight() / 2, 127, 127, 127, 255, 1);

    // Color buffer must remain untouched, stencil test must pass and stencil buffer must be
    // incremented to 1.
    bufs[0] = GL_NONE;
    setDrawBuffers(mMaxDrawBuffers, bufs);
    glEnable(GL_STENCIL_TEST);
    glStencilOp(GL_KEEP, GL_INCR, GL_INCR);
    drawQuad(program, positionAttrib(), 1.0);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_NEAR(getWindowWidth() / 2, getWindowHeight() / 2, 127, 127, 127, 255, 1);

    // Draw with the color buffer enabled and stencil test expecting 0.
    // Stencil test must fail, and both the color and the stencil buffers must remain unchanged.
    bufs[0] = GL_COLOR_ATTACHMENT0;
    setDrawBuffers(mMaxDrawBuffers, bufs);
    glStencilFunc(GL_EQUAL, 0, 255);
    drawQuad(program, positionAttrib(), 1.0);
    EXPECT_PIXEL_NEAR(getWindowWidth() / 2, getWindowHeight() / 2, 127, 127, 127, 255, 1);

    // Draw with stencil ref value matching the stored stencil buffer value.
    // Stencil test must pass and the color buffer must be updated.
    glStencilFunc(GL_EQUAL, 1, 255);
    drawQuad(program, positionAttrib(), 1.0);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, GLColor::red);

    glDeleteProgram(program);
}

// Test that draws to every buffer and verifies that every buffer was drawn to.
TEST_P(DrawBuffersTest, AllRGBA8)
{
    ANGLE_SKIP_TEST_IF(!setupTest());

    bool flags[8]  = {false};
    GLenum bufs[8] = {GL_NONE};
    GLTexture textures[8];

    ASSERT_GT(mMaxDrawBuffers, 0);
    ASSERT_LE(mMaxDrawBuffers, 8);
    for (GLint texIndex = 0; texIndex < mMaxDrawBuffers; ++texIndex)
    {
        glBindTexture(GL_TEXTURE_2D, textures[texIndex]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, getWindowWidth(), getWindowHeight(), 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + texIndex, GL_TEXTURE_2D,
                               textures[texIndex], 0);
        flags[texIndex] = true;
        bufs[texIndex]  = GL_COLOR_ATTACHMENT0 + texIndex;
    }

    GLuint program;
    setupMRTProgram(flags, &program);

    setDrawBuffers(mMaxDrawBuffers, bufs);
    drawQuad(program, positionAttrib(), 0.5);

    for (GLint texIndex = 0; texIndex < mMaxDrawBuffers; ++texIndex)
    {
        verifyAttachment2D(texIndex, textures[texIndex], GL_TEXTURE_2D, 0);
    }

    EXPECT_GL_NO_ERROR();

    glDeleteProgram(program);
}
// Same as above but adds a state change from a program with different masks after a clear.
TEST_P(DrawBuffersWebGL2Test, TwoProgramsWithDifferentOutputsAndClear)
{
    // TODO(http://anglebug.com/42261569): Broken on the GL back-end.
    ANGLE_SKIP_TEST_IF(IsOpenGL());

    ANGLE_SKIP_TEST_IF(!setupTest());

    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &mMaxDrawBuffers);
    ASSERT_GE(mMaxDrawBuffers, 4);

    bool flags[8]      = {false};
    GLenum someBufs[4] = {GL_NONE};
    GLenum allBufs[4]  = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2,
                          GL_COLOR_ATTACHMENT3};

    constexpr GLuint kMaxBuffers     = 4;
    constexpr GLuint kHalfMaxBuffers = 2;

    // Enable all draw buffers.
    for (GLuint texIndex = 0; texIndex < kMaxBuffers; texIndex++)
    {
        glBindTexture(GL_TEXTURE_2D, mTextures[texIndex]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + texIndex, GL_TEXTURE_2D,
                               mTextures[texIndex], 0);
        someBufs[texIndex] =
            texIndex >= kHalfMaxBuffers ? GL_COLOR_ATTACHMENT0 + texIndex : GL_NONE;

        // Mask out the first two buffers.
        flags[texIndex] = texIndex >= kHalfMaxBuffers;
    }

    GLuint program;
    setupMRTProgram(flags, &program);

    // Now set up a second simple program that draws to FragColor. Should be broadcast.
    ANGLE_GL_PROGRAM(simpleProgram, essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());

    // Draw with simple program.
    drawQuad(simpleProgram, essl1_shaders::PositionAttrib(), 0.5f, 1.0f, true);
    ASSERT_GL_NO_ERROR();

    // Clear draw buffers.
    setDrawBuffers(kMaxBuffers, someBufs);
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ASSERT_GL_NO_ERROR();

    // Verify first is drawn red, second is untouched, and last two are cleared green.
    verifyAttachment2DColor(0, mTextures[0], GL_TEXTURE_2D, 0, GLColor::red);
    verifyAttachment2DColor(1, mTextures[1], GL_TEXTURE_2D, 0, GLColor::transparentBlack);
    verifyAttachment2DColor(2, mTextures[2], GL_TEXTURE_2D, 0, GLColor::green);
    verifyAttachment2DColor(3, mTextures[3], GL_TEXTURE_2D, 0, GLColor::green);

    // Draw with MRT program.
    setDrawBuffers(kMaxBuffers, someBufs);
    drawQuad(program, positionAttrib(), 0.5, 1.0f, true);
    ASSERT_GL_NO_ERROR();

    // Only the last two attachments should be updated.
    verifyAttachment2DColor(0, mTextures[0], GL_TEXTURE_2D, 0, GLColor::red);
    verifyAttachment2DColor(1, mTextures[1], GL_TEXTURE_2D, 0, GLColor::transparentBlack);
    verifyAttachment2D(2, mTextures[2], GL_TEXTURE_2D, 0);
    verifyAttachment2D(3, mTextures[3], GL_TEXTURE_2D, 0);

    // Active draw buffers with no fragment output is not allowed.
    setDrawBuffers(kMaxBuffers, allBufs);
    drawQuad(program, positionAttrib(), 0.5, 1.0f, true);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);
    // Exception: when RASTERIZER_DISCARD is enabled.
    glEnable(GL_RASTERIZER_DISCARD);
    drawQuad(program, positionAttrib(), 0.5, 1.0f, true);
    ASSERT_GL_NO_ERROR();
    glDisable(GL_RASTERIZER_DISCARD);
    // Exception: when all 4 channels of color mask are set to false.
    glColorMask(false, false, false, false);
    drawQuad(program, positionAttrib(), 0.5, 1.0f, true);
    ASSERT_GL_NO_ERROR();
    glColorMask(false, true, false, false);
    drawQuad(program, positionAttrib(), 0.5, 1.0f, true);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);
    glColorMask(true, true, true, true);
    drawQuad(program, positionAttrib(), 0.5, 1.0f, true);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);

    // Clear again. All attachments should be cleared.
    glClear(GL_COLOR_BUFFER_BIT);
    verifyAttachment2DColor(0, mTextures[0], GL_TEXTURE_2D, 0, GLColor::green);
    verifyAttachment2DColor(1, mTextures[1], GL_TEXTURE_2D, 0, GLColor::green);
    verifyAttachment2DColor(2, mTextures[2], GL_TEXTURE_2D, 0, GLColor::green);
    verifyAttachment2DColor(3, mTextures[3], GL_TEXTURE_2D, 0, GLColor::green);

    glDeleteProgram(program);
}

// Test clear with gaps in draw buffers, originally show up as
// webgl_conformance_vulkan_passthrough_tests conformance/extensions/webgl-draw-buffers.html
// failure. This is added for ease of debugging.
TEST_P(DrawBuffersWebGL2Test, Clear)
{
    ANGLE_SKIP_TEST_IF(!setupTest());

    constexpr GLint kMaxBuffers = 4;

    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &mMaxDrawBuffers);
    ASSERT_GE(mMaxDrawBuffers, kMaxBuffers);

    GLenum drawBufs[kMaxBuffers] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1,
                                    GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3};

    // Enable all draw buffers.
    for (GLuint texIndex = 0; texIndex < kMaxBuffers; texIndex++)
    {
        glBindTexture(GL_TEXTURE_2D, mTextures[texIndex]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + texIndex, GL_TEXTURE_2D,
                               mTextures[texIndex], 0);
    }

    // Clear with all draw buffers.
    setDrawBuffers(kMaxBuffers, drawBufs);
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Clear with first half none draw buffers.
    drawBufs[0] = GL_NONE;
    drawBufs[1] = GL_NONE;
    setDrawBuffers(kMaxBuffers, drawBufs);
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ASSERT_GL_NO_ERROR();

    // Verify first is drawn red, second is untouched, and last two are cleared green.
    verifyAttachment2DColor(0, mTextures[0], GL_TEXTURE_2D, 0, GLColor::red);
    verifyAttachment2DColor(1, mTextures[1], GL_TEXTURE_2D, 0, GLColor::red);
    verifyAttachment2DColor(2, mTextures[2], GL_TEXTURE_2D, 0, GLColor::green);
    verifyAttachment2DColor(3, mTextures[3], GL_TEXTURE_2D, 0, GLColor::green);
}

TEST_P(DrawBuffersTest, UnwrittenOutputVariablesShouldNotCrash)
{
    ANGLE_SKIP_TEST_IF(!setupTest());

    // Bind two render targets but use a shader which writes only to the first one.
    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0], 0);

    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, mTextures[1], 0);

    bool flags[8] = {true, false};

    GLuint program;
    setupMRTProgram(flags, &program);

    const GLenum bufs[] = {
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1,
        GL_NONE,
        GL_NONE,
    };

    setDrawBuffers(4, bufs);

    // This call should not crash when we dynamically generate the HLSL code.
    drawQuad(program, positionAttrib(), 0.5);

    verifyAttachment2D(0, mTextures[0], GL_TEXTURE_2D, 0);

    EXPECT_GL_NO_ERROR();

    glDeleteProgram(program);
}

TEST_P(DrawBuffersTest, BroadcastGLFragColor)
{
    // Broadcast is not supported on GLES 3.0.
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() >= 3);
    ANGLE_SKIP_TEST_IF(!setupTest());

    // Bind two render targets. gl_FragColor should be broadcast to both.
    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0], 0);

    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, mTextures[1], 0);

    const GLenum bufs[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};

    constexpr char kFS[] =
        "#extension GL_EXT_draw_buffers : enable\n"
        "precision highp float;\n"
        "uniform float u_zero;\n"
        "void main()\n"
        "{\n"
        "    gl_FragColor = vec4(1, 0, 0, 1);\n"
        "    if (u_zero < 1.0)\n"
        "    {\n"
        "        return;\n"
        "    }\n"
        "}\n";

    GLuint program = CompileProgram(essl1_shaders::vs::Simple(), kFS);
    if (program == 0)
    {
        FAIL() << "shader compilation failed.";
    }

    setDrawBuffers(2, bufs);
    drawQuad(program, essl1_shaders::PositionAttrib(), 0.5);

    verifyAttachment2D(0, mTextures[0], GL_TEXTURE_2D, 0);
    verifyAttachment2D(0, mTextures[1], GL_TEXTURE_2D, 0);

    EXPECT_GL_NO_ERROR();

    glDeleteProgram(program);
}

// Test that binding multiple layers of a 3D texture works correctly.
// This is the same as DrawBuffersTestES3.3DTextures but is used for GL_OES_texture_3D extension
// on GLES 2.0 instead.
TEST_P(DrawBuffersTest, 3DTexturesOES)
{
    ANGLE_SKIP_TEST_IF(!setupTest());
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_3D"));

    GLTexture texture;
    glBindTexture(GL_TEXTURE_3D, texture);
    glTexImage3DOES(GL_TEXTURE_3D, 0, GL_RGBA, getWindowWidth(), getWindowHeight(),
                    getWindowWidth(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glFramebufferTexture3DOES(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_3D, texture, 0, 0);
    glFramebufferTexture3DOES(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_3D, texture, 0, 1);
    glFramebufferTexture3DOES(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_3D, texture, 0, 2);
    glFramebufferTexture3DOES(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, GL_TEXTURE_3D, texture, 0, 3);

    bool flags[8] = {true, true, true, true, false};

    GLuint program;
    setupMRTProgram(flags, &program);

    const GLenum bufs[] = {
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1,
        GL_COLOR_ATTACHMENT2,
        GL_COLOR_ATTACHMENT3,
    };

    setDrawBuffers(4, bufs);
    drawQuad(program, positionAttrib(), 0.5);

    verifyAttachment3DOES(0, texture, 0, 0);
    verifyAttachment3DOES(1, texture, 0, 1);
    verifyAttachment3DOES(2, texture, 0, 2);
    verifyAttachment3DOES(3, texture, 0, 3);

    EXPECT_GL_NO_ERROR();

    glDeleteProgram(program);
}

class DrawBuffersTestES3 : public DrawBuffersTest
{};

// Test that binding multiple layers of a 3D texture works correctly
TEST_P(DrawBuffersTestES3, 3DTextures)
{
    ANGLE_SKIP_TEST_IF(!setupTest());

    GLTexture texture;
    glBindTexture(GL_TEXTURE_3D, texture);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, getWindowWidth(), getWindowHeight(), getWindowWidth(),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture, 0, 0);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, texture, 0, 1);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, texture, 0, 2);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, texture, 0, 3);

    bool flags[8] = {true, true, true, true, false};

    GLuint program;
    setupMRTProgram(flags, &program);

    const GLenum bufs[] = {
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1,
        GL_COLOR_ATTACHMENT2,
        GL_COLOR_ATTACHMENT3,
    };

    glDrawBuffers(4, bufs);
    drawQuad(program, positionAttrib(), 0.5);

    verifyAttachmentLayer(0, texture, 0, 0);
    verifyAttachmentLayer(1, texture, 0, 1);
    verifyAttachmentLayer(2, texture, 0, 2);
    verifyAttachmentLayer(3, texture, 0, 3);

    EXPECT_GL_NO_ERROR();

    glDeleteProgram(program);
}

// Test that binding multiple layers of a 2D array texture works correctly
TEST_P(DrawBuffersTestES3, 2DArrayTextures)
{
    ANGLE_SKIP_TEST_IF(!setupTest());

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, getWindowWidth(), getWindowHeight(),
                 getWindowWidth(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture, 0, 0);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, texture, 0, 1);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, texture, 0, 2);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, texture, 0, 3);

    bool flags[8] = {true, true, true, true, false};

    GLuint program;
    setupMRTProgram(flags, &program);

    const GLenum bufs[] = {
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1,
        GL_COLOR_ATTACHMENT2,
        GL_COLOR_ATTACHMENT3,
    };

    glDrawBuffers(4, bufs);
    drawQuad(program, positionAttrib(), 0.5);

    verifyAttachmentLayer(0, texture, 0, 0);
    verifyAttachmentLayer(1, texture, 0, 1);
    verifyAttachmentLayer(2, texture, 0, 2);
    verifyAttachmentLayer(3, texture, 0, 3);

    EXPECT_GL_NO_ERROR();

    glDeleteProgram(program);
}

// Test that binding multiple faces of a CubeMap texture works correctly
TEST_P(DrawBuffersTestES3, CubeMapTextures)
{
    ANGLE_SKIP_TEST_IF(!setupTest());

    GLTexture texture;
    glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
    glTexStorage2D(GL_TEXTURE_CUBE_MAP, 1, GL_RGBA8, getWindowWidth(), getWindowHeight());
    EXPECT_GL_NO_ERROR();

    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture, 0, 3);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, texture, 0, 2);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, texture, 0, 1);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, texture, 0, 0);
    EXPECT_GL_NO_ERROR();

    bool flags[8] = {true, true, true, true, false};

    GLuint program;
    setupMRTProgram(flags, &program);

    const GLenum bufs[] = {
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1,
        GL_COLOR_ATTACHMENT2,
        GL_COLOR_ATTACHMENT3,
    };

    glDrawBuffers(4, bufs);
    drawQuad(program, positionAttrib(), 0.5);

    verifyAttachmentLayer(0, texture, 0, 3);
    verifyAttachmentLayer(1, texture, 0, 2);
    verifyAttachmentLayer(2, texture, 0, 1);
    verifyAttachmentLayer(3, texture, 0, 0);

    EXPECT_GL_NO_ERROR();

    glDeleteProgram(program);
}

// Test that binding multiple layers of a CubeMap array texture works correctly
TEST_P(DrawBuffersTestES3, CubeMapArrayTextures)
{
    ANGLE_SKIP_TEST_IF(!setupTest());
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_cube_map_array"));

    GLTexture texture;
    glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, texture);
    glTexStorage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 1, GL_RGBA8, getWindowWidth(), getWindowHeight(),
                   static_cast<GLint>(kCubeFaces.size()));
    EXPECT_GL_NO_ERROR();

    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture, 0, 3);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, texture, 0, 2);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, texture, 0, 1);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, texture, 0, 0);
    EXPECT_GL_NO_ERROR();

    bool flags[8] = {true, true, true, true, false};

    GLuint program;
    setupMRTProgram(flags, &program);

    const GLenum bufs[] = {
        GL_COLOR_ATTACHMENT0,
        GL_COLOR_ATTACHMENT1,
        GL_COLOR_ATTACHMENT2,
        GL_COLOR_ATTACHMENT3,
    };

    glDrawBuffers(4, bufs);
    drawQuad(program, positionAttrib(), 0.5);

    verifyAttachmentLayer(0, texture, 0, 3);
    verifyAttachmentLayer(1, texture, 0, 2);
    verifyAttachmentLayer(2, texture, 0, 1);
    verifyAttachmentLayer(3, texture, 0, 0);

    EXPECT_GL_NO_ERROR();

    glDeleteProgram(program);
}

// Test that blend works when draw buffers and framebuffers change.
TEST_P(DrawBuffersTestES3, BlendWithDrawBufferAndFramebufferChanges)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_draw_buffers_indexed"));

    // http://anglebug.com/42263715
    ANGLE_SKIP_TEST_IF(IsMac() && IsIntel() && IsDesktopOpenGL());

    // Create two framebuffers, one with 3 attachments (fbo3), one with 4 (fbo4).  The test issues
    // draw calls on fbo3 with different attachments enabled, then switches to fbo4 (without
    // dirtying blend state) and draws to other attachments.  It ensures that blend state is
    // appropriately set on framebuffer change.

    GLenum bufs[4] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2,
                      GL_COLOR_ATTACHMENT3};

    GLFramebuffer fbo[2];
    GLTexture tex[7];
    constexpr GLfloat kClearValue[] = {1, 1, 1, 1};

    glBindFramebuffer(GL_FRAMEBUFFER, fbo[0]);

    for (uint32_t texIndex = 0; texIndex < 7; ++texIndex)
    {
        size_t colorAttachmentIndex = texIndex >= 3 ? texIndex - 3 : texIndex;
        if (texIndex == 3)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, fbo[1]);
        }

        glBindTexture(GL_TEXTURE_2D, tex[texIndex]);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 1, 1);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + colorAttachmentIndex,
                               GL_TEXTURE_2D, tex[texIndex], 0);
        EXPECT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

        glDrawBuffers(4, bufs);
        glClearBufferfv(GL_COLOR, colorAttachmentIndex, kClearValue);
    }
    ASSERT_GL_NO_ERROR();

    glEnableiOES(GL_BLEND, 0);
    glEnableiOES(GL_BLEND, 1);
    glEnableiOES(GL_BLEND, 2);
    glEnableiOES(GL_BLEND, 3);

    glBlendEquationiOES(0, GL_FUNC_REVERSE_SUBTRACT);
    glBlendEquationiOES(1, GL_MIN);
    glBlendEquationiOES(2, GL_FUNC_REVERSE_SUBTRACT);
    glBlendEquationiOES(3, GL_FUNC_REVERSE_SUBTRACT);

    glBlendFunciOES(0, GL_ONE, GL_ONE);
    glBlendFunciOES(1, GL_DST_ALPHA, GL_DST_ALPHA);
    glBlendFunciOES(2, GL_SRC_ALPHA, GL_SRC_ALPHA);
    glBlendFunciOES(3, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA);

    bufs[0] = GL_NONE;
    bufs[2] = GL_NONE;
    glDrawBuffers(4, bufs);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo[0]);

    bufs[2] = GL_COLOR_ATTACHMENT2;
    glDrawBuffers(3, bufs);

    constexpr char kFS[] = R"(#version 300 es
precision highp float;

uniform vec4 value0;
uniform vec4 value1;
uniform vec4 value2;
uniform vec4 value3;

layout(location = 0) out vec4 color0;
layout(location = 1) out vec4 color1;
layout(location = 2) out vec4 color2;
layout(location = 3) out vec4 color3;

void main()
{
    color0 = value0;
    color1 = value1;
    color2 = value2;
    color3 = value3;
}
)";

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Simple(), kFS);
    glUseProgram(program);

    GLint uniforms[4];
    for (uint32_t attachmentIndex = 0; attachmentIndex < 4; ++attachmentIndex)
    {
        char uniformName[20];
        snprintf(uniformName, sizeof uniformName, "value%u", attachmentIndex);
        uniforms[attachmentIndex] = glGetUniformLocation(program, uniformName);
        ASSERT_NE(uniforms[attachmentIndex], -1);
    }

    // Currently, fbo3 is bound.  The attachment states are:
    //
    //     0: DISABLED Color: (1, 1, 1, 1), Blend: reverse subtract, ONE/ONE
    //     1:          Color: (1, 1, 1, 1), Blend: min, DST_ALPHA/DST_ALPHA
    //     2:          Color: (1, 1, 1, 1), Blend: reverse subtract, SRC_ALPHA/SRC_ALPHA
    //
    // Draw:
    //
    //     0: Color: don't care
    //     1: Color: (0.75, 0.5, 0.25, 0.5)  ->  Result after blend is: (0.75, 0.5, 0.25, 0.5)
    //     2: Color: (0.25, 0.5, 0.75, 0.5)  ->  Result after blend is: (0.375, 0.25, 0.125, 0.25)

    // Draws green into attachment 1
    glUniform4f(uniforms[1], 0.75, 0.5, 0.25, 0.5);
    glUniform4f(uniforms[2], 0.25, 0.5, 0.75, 0.5);
    drawQuad(program, positionAttrib(), 0.5);
    ASSERT_GL_NO_ERROR();

    bufs[0] = GL_COLOR_ATTACHMENT0;
    bufs[1] = GL_NONE;
    glDrawBuffers(3, bufs);

    // Currently, fbo3 is bound.  The attachment states are:
    //
    //     0:          Color: (1, 1, 1, 1), Blend: reverse subtract, ONE/ONE
    //     1: DISABLED Color: (0.75, 0.5, 0.25, 0.5), Blend: min, DST_ALPHA/DST_ALPHA
    //     2:          Color: (0.375, 0.25, 0.125, 0.25), Blend: reverse subtract,
    //     SRC_ALPHA/SRC_ALPHA
    //
    // Draw:
    //
    //     0: Color: (0.5, 0.25, 0.75, 0.25) ->  Result after blend is: (0.5, 0.75, 0.25, 0.75)
    //     1: Color: don't care
    //     2: Color: (0.125, 0, 0, 1)  ->  Result after blend is: (0.25, 0.25, 0.125, 0)

    // Clear with red
    glUniform4f(uniforms[0], 0.5, 0.25, 0.75, 0.25);
    glUniform4f(uniforms[2], 0.125, 0, 0, 1);
    drawQuad(program, positionAttrib(), 0.5);
    ASSERT_GL_NO_ERROR();

    glBindFramebuffer(GL_FRAMEBUFFER, fbo[1]);

    // Currently, fbo4 is bound.  The attachment states are:
    //
    //     0: DISABLED Color: (1, 1, 1, 1), Blend: reverse subtract, ONE/ONE
    //     1:          Color: (1, 1, 1, 1), Blend: min, DST_ALPHA/DST_ALPHA
    //     2: DISABLED Color: (1, 1, 1, 1), Blend: reverse subtract, SRC_ALPHA/SRC_ALPHA
    //     3:          Color: (1, 1, 1, 1), Blend: reverse subtract, ONE_MINUS_SRC_ALPHA/SRC_ALPHA
    //
    // Draw:
    //
    //     0: Color: don't care
    //     1: Color: (0.125, 0.5, 0.625, 0.25)  ->  Result after blend is: (0.125, 0.5, 0.625, 0.25)
    //     2: Color: don't care
    //     3: Color: (0.75, 0.25, 0.5, 0.75)  ->  Result after blend is:
    //                                                               (0.5625, 0.6875, 0.625, 0.5625)

    glUniform4f(uniforms[1], 0.125, 0.5, 0.625, 0.25);
    glUniform4f(uniforms[3], 0.75, 0.25, 0.5, 0.75);
    drawQuad(program, positionAttrib(), 0.5);
    ASSERT_GL_NO_ERROR();

    // Verify results
    glBindFramebuffer(GL_FRAMEBUFFER, fbo[0]);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    EXPECT_PIXEL_NEAR(0, 0, 127, 191, 63, 191, 1);
    glReadBuffer(GL_COLOR_ATTACHMENT1);
    EXPECT_PIXEL_NEAR(0, 0, 191, 127, 63, 127, 1);
    glReadBuffer(GL_COLOR_ATTACHMENT2);
    EXPECT_PIXEL_NEAR(0, 0, 63, 63, 31, 0, 1);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo[1]);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    EXPECT_PIXEL_NEAR(0, 0, 255, 255, 255, 255, 1);
    glReadBuffer(GL_COLOR_ATTACHMENT1);
    EXPECT_PIXEL_NEAR(0, 0, 31, 127, 159, 63, 1);
    glReadBuffer(GL_COLOR_ATTACHMENT2);
    EXPECT_PIXEL_NEAR(0, 0, 255, 255, 255, 255, 1);
    glReadBuffer(GL_COLOR_ATTACHMENT3);
    EXPECT_PIXEL_NEAR(0, 0, 143, 175, 159, 143, 1);
}

// Test that a disabled color attachment incompatible with a fragment output
// is correctly ignored and does not affect other attachments.
TEST_P(DrawBuffersTestES3, DrawWithDisabledIncompatibleAttachment)
{
    ANGLE_SKIP_TEST_IF(!setupTest());

    ASSERT_GE(mMaxDrawBuffers, 4);
    for (GLuint texIndex = 0; texIndex < 4; texIndex++)
    {
        glBindTexture(GL_TEXTURE_2D, mTextures[texIndex]);
        if (texIndex == 1)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8UI, getWindowWidth(), getWindowHeight(), 0,
                         GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, nullptr);
        }
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + texIndex, GL_TEXTURE_2D,
                               mTextures[texIndex], 0);
    }
    ASSERT_GL_NO_ERROR();
    EXPECT_GL_FRAMEBUFFER_COMPLETE(GL_DRAW_FRAMEBUFFER);

    const GLenum bufs[] = {GL_COLOR_ATTACHMENT0, GL_NONE, GL_COLOR_ATTACHMENT2,
                           GL_COLOR_ATTACHMENT3};
    setDrawBuffers(4, bufs);

    bool flags[8] = {true, true, true, true};
    GLuint program;
    setupMRTProgram(flags, &program);

    drawQuad(program, positionAttrib(), 0.5);
    EXPECT_GL_NO_ERROR();

    verifyAttachment2D(0, mTextures[0], GL_TEXTURE_2D, 0);
    verifyAttachment2D(2, mTextures[2], GL_TEXTURE_2D, 0);
    verifyAttachment2D(3, mTextures[3], GL_TEXTURE_2D, 0);

    glDeleteProgram(program);
}

// Vulkan backend is setting per buffer color mask to false for draw buffers that set to GL_NONE.
// These set of tests are to test draw buffer change followed by draw/clear/blit and followed by
// draw buffer change are behaving correctly.
class ColorMaskForDrawBuffersTest : public DrawBuffersTest
{
  protected:
    void setupColorMaskForDrawBuffersTest()
    {
        glBindTexture(GL_TEXTURE_2D, mTextures[0]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0],
                               0);
        glBindTexture(GL_TEXTURE_2D, mTextures[1]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, mTextures[1],
                               0);
        glBindTexture(GL_TEXTURE_2D, mTextures[2]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, mTextures[2],
                               0);

        constexpr char kFS_ESSL3[] =
            "#version 300 es\n"
            "precision highp float;\n"
            "uniform mediump vec4 u_color0;\n"
            "uniform mediump vec4 u_color1;\n"
            "uniform mediump vec4 u_color2;\n"
            "layout(location = 0) out vec4 out_color0;\n"
            "layout(location = 1) out vec4 out_color1;\n"
            "layout(location = 2) out vec4 out_color2;\n"
            "void main()\n"
            "{\n"
            "    out_color0 = u_color0;\n"
            "    out_color1 = u_color1;\n"
            "    out_color2 = u_color2;\n"
            "}\n";
        program = CompileProgram(essl3_shaders::vs::Simple(), kFS_ESSL3);
        glUseProgram(program);

        positionLocation = glGetAttribLocation(program, positionAttrib());
        ASSERT_NE(-1, positionLocation);
        color0UniformLocation = glGetUniformLocation(program, "u_color0");
        ASSERT_NE(color0UniformLocation, -1);
        color1UniformLocation = glGetUniformLocation(program, "u_color1");
        ASSERT_NE(color1UniformLocation, -1);
        color2UniformLocation = glGetUniformLocation(program, "u_color2");
        ASSERT_NE(color2UniformLocation, -1);

        glUniform4fv(color0UniformLocation, 1, GLColor::red.toNormalizedVector().data());
        glUniform4fv(color1UniformLocation, 1, GLColor::green.toNormalizedVector().data());
        glUniform4fv(color2UniformLocation, 1, GLColor::yellow.toNormalizedVector().data());

        // Draw into the buffers so that buffer0 is red, buffer1 is green and buffer2 is yellow
        resetDrawBuffers();
        drawQuad(program, positionAttrib(), 0.5);
        EXPECT_GL_NO_ERROR();

        for (int i = 0; i < 4; i++)
        {
            drawBuffers[i] = GL_NONE;
        }
    }

    void resetDrawBuffers()
    {
        drawBuffers[0] = GL_COLOR_ATTACHMENT0;
        drawBuffers[1] = GL_COLOR_ATTACHMENT1;
        drawBuffers[2] = GL_COLOR_ATTACHMENT2;
        drawBuffers[3] = GL_NONE;
        setDrawBuffers(4, drawBuffers);
    }

    GLenum drawBuffers[4];
    GLuint program;
    GLint positionLocation;
    GLint color0UniformLocation;
    GLint color1UniformLocation;
    GLint color2UniformLocation;
};

// Test draw buffer state change followed draw call
TEST_P(ColorMaskForDrawBuffersTest, DrawQuad)
{
    ANGLE_SKIP_TEST_IF(!setupTest());
    setupColorMaskForDrawBuffersTest();

    // Draw blue into attachment0. Buffer0 should be blue and buffer1 should remain green
    drawBuffers[0] = GL_COLOR_ATTACHMENT0;
    setDrawBuffers(4, drawBuffers);
    glUniform4fv(color0UniformLocation, 1, GLColor::blue.toNormalizedVector().data());
    glUniform4fv(color1UniformLocation, 1, GLColor::cyan.toNormalizedVector().data());
    glViewport(0, 0, getWindowWidth() / 2, getWindowHeight() / 2);
    drawQuad(program, positionAttrib(), 0.5);

    resetDrawBuffers();
    glUniform4fv(color0UniformLocation, 1, GLColor::magenta.toNormalizedVector().data());
    glUniform4fv(color1UniformLocation, 1, GLColor::white.toNormalizedVector().data());
    glViewport(getWindowWidth() / 2, 0, getWindowWidth() / 2, getWindowHeight() / 2);
    drawQuad(program, positionAttrib(), 0.5);
    EXPECT_GL_NO_ERROR();

    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0],
                           0);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 4, getWindowHeight() / 4, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() * 3 / 4, getWindowHeight() / 4, GLColor::magenta);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 4, getWindowHeight() * 3 / 4, GLColor::red);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[1],
                           0);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 4, getWindowHeight() / 4, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() * 3 / 4, getWindowHeight() / 4, GLColor::white);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 4, getWindowHeight() * 3 / 4, GLColor::green);
    EXPECT_GL_NO_ERROR();
}

// Test draw buffer state change followed clear
TEST_P(ColorMaskForDrawBuffersTest, Clear)
{
    ANGLE_SKIP_TEST_IF(!setupTest());
    setupColorMaskForDrawBuffersTest();

    // Clear attachment1. Buffer0 should retain red and buffer1 should be blue
    drawBuffers[1] = GL_COLOR_ATTACHMENT1;
    setDrawBuffers(4, drawBuffers);
    angle::Vector4 clearColor = GLColor::blue.toNormalizedVector();
    glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    glClear(GL_COLOR_BUFFER_BIT);
    verifyAttachment2DColor(0, mTextures[0], GL_TEXTURE_2D, 0, GLColor::red);
    verifyAttachment2DColor(1, mTextures[1], GL_TEXTURE_2D, 0, GLColor::blue);
    EXPECT_GL_NO_ERROR();
}

// Test draw buffer state change followed scissored clear
TEST_P(ColorMaskForDrawBuffersTest, ScissoredClear)
{
    ANGLE_SKIP_TEST_IF(!setupTest());
    setupColorMaskForDrawBuffersTest();

    // Clear attachment1. Buffer0 should retain red and buffer1 should be blue
    drawBuffers[1] = GL_COLOR_ATTACHMENT1;
    setDrawBuffers(4, drawBuffers);
    angle::Vector4 clearColor = GLColor::blue.toNormalizedVector();
    glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    glScissor(0, 0, getWindowWidth() / 2, getWindowHeight() / 2);
    glEnable(GL_SCISSOR_TEST);
    glClear(GL_COLOR_BUFFER_BIT);

    resetDrawBuffers();
    clearColor = GLColor::magenta.toNormalizedVector();
    glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    glScissor(getWindowWidth() / 2, 0, getWindowWidth() / 2, getWindowHeight() / 2);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_GL_NO_ERROR();

    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0],
                           0);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 4, getWindowHeight() / 4, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() * 3 / 4, getWindowHeight() / 4, GLColor::magenta);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 4, getWindowHeight() * 3 / 4, GLColor::red);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[1],
                           0);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 4, getWindowHeight() / 4, GLColor::blue);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() * 3 / 4, getWindowHeight() / 4, GLColor::magenta);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 4, getWindowHeight() * 3 / 4, GLColor::green);
    EXPECT_GL_NO_ERROR();
}

// Test draw buffer state change followed FBO blit
TEST_P(ColorMaskForDrawBuffersTest, Blit)
{
    ANGLE_SKIP_TEST_IF(!setupTest());
    setupColorMaskForDrawBuffersTest();

    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[2],
                           0);

    // BLIT mTexture[2] to attachment0. Buffer0 should remain red and buffer1 should be yellow
    drawBuffers[0] = GL_COLOR_ATTACHMENT0;
    setDrawBuffers(4, drawBuffers);
    glBlitFramebuffer(0, 0, getWindowWidth(), getWindowHeight(), 0, 0, getWindowWidth(),
                      getWindowHeight(), GL_COLOR_BUFFER_BIT, GL_NEAREST);
    verifyAttachment2DColor(0, mTextures[0], GL_TEXTURE_2D, 0, GLColor::yellow);
    verifyAttachment2DColor(1, mTextures[1], GL_TEXTURE_2D, 0, GLColor::green);
    EXPECT_GL_NO_ERROR();
}

// Test that enabling/disabling FBO draw buffers affects color mask
TEST_P(ColorMaskForDrawBuffersTest, StateChangeAffectsColorMask)
{
    ANGLE_SKIP_TEST_IF(!setupTest());
    setupColorMaskForDrawBuffersTest();

    // Setup draw
    glUseProgram(program);
    std::array<Vector3, 6> quadVertices = GetQuadVertices();
    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, quadVertices.data());
    glEnableVertexAttribArray(positionLocation);

    // Draw with some attachments disabled.  Attachments are initially, red, green and yellow.
    drawBuffers[0] = GL_COLOR_ATTACHMENT0;
    drawBuffers[1] = GL_NONE;
    drawBuffers[2] = GL_NONE;
    setDrawBuffers(4, drawBuffers);
    glUniform4fv(color0UniformLocation, 1, GLColor::blue.toNormalizedVector().data());
    glUniform4fv(color1UniformLocation, 1, GLColor::cyan.toNormalizedVector().data());
    // Attachment 0 is now blue
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_NO_ERROR();

    // Draw with the second attachment enabled
    drawBuffers[0] = GL_COLOR_ATTACHMENT0;
    drawBuffers[1] = GL_COLOR_ATTACHMENT1;
    setDrawBuffers(4, drawBuffers);
    glUniform4fv(color0UniformLocation, 1, GLColor::magenta.toNormalizedVector().data());
    glUniform4fv(color1UniformLocation, 1, GLColor::white.toNormalizedVector().data());
    // Attachment 0 is now magenta, and attachment 1 is white.  Attachment 2 is still yellow.
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_NO_ERROR();

    // Check second attachment was updated by the second draw
    glBindFramebuffer(GL_READ_FRAMEBUFFER, mFBO);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::magenta);
    glReadBuffer(GL_COLOR_ATTACHMENT1);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::white);
    glReadBuffer(GL_COLOR_ATTACHMENT2);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::yellow);
    EXPECT_GL_NO_ERROR();
}

// Test that enabling/disabling FBO draw buffers affects blend state appropriately as well as the
// color mask.
TEST_P(ColorMaskForDrawBuffersTest, StateChangeAffectsBlendState)
{
    ANGLE_SKIP_TEST_IF(!setupTest());
    setupColorMaskForDrawBuffersTest();

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    // Setup draw
    glUseProgram(program);
    std::array<Vector3, 6> quadVertices = GetQuadVertices();
    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, quadVertices.data());
    glEnableVertexAttribArray(positionLocation);

    // Draw with some attachments disabled.  Attachments are initially, red, green and yellow.
    drawBuffers[0] = GL_COLOR_ATTACHMENT0;
    drawBuffers[1] = GL_NONE;
    drawBuffers[2] = GL_NONE;
    setDrawBuffers(4, drawBuffers);
    glUniform4fv(color0UniformLocation, 1, GLColor::blue.toNormalizedVector().data());
    glUniform4fv(color1UniformLocation, 1, GLColor::cyan.toNormalizedVector().data());
    // Attachment 0 is now magenta.
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_NO_ERROR();

    // Draw with the second attachment enabled
    drawBuffers[0] = GL_COLOR_ATTACHMENT0;
    drawBuffers[1] = GL_COLOR_ATTACHMENT1;
    setDrawBuffers(4, drawBuffers);
    glUniform4fv(color0UniformLocation, 1, GLColor::green.toNormalizedVector().data());
    glUniform4fv(color1UniformLocation, 1, GLColor::blue.toNormalizedVector().data());
    // Attachment 0 is now white, attachment 1 is cyan and attachment 2 is still yellow.
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_GL_NO_ERROR();

    // Check second attachment was updated by the second draw
    glBindFramebuffer(GL_READ_FRAMEBUFFER, mFBO);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::white);
    glReadBuffer(GL_COLOR_ATTACHMENT1);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::cyan);
    glReadBuffer(GL_COLOR_ATTACHMENT2);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::yellow);
    EXPECT_GL_NO_ERROR();
}

// Test parameters: <platform, color format, depth/stencil format (0 if none),
// useTexture, sample count (0 = single-sampled)>.
using DrawBuffersAnyFormatVariationsTestParams =
    std::tuple<angle::PlatformParameters, GLenum, GLenum, bool, GLint>;

std::string DrawBuffersAnyFormatVariationsTestPrint(
    const ::testing::TestParamInfo<DrawBuffersAnyFormatVariationsTestParams> &paramsInfo)
{
    const DrawBuffersAnyFormatVariationsTestParams &params = paramsInfo.param;
    std::ostringstream out;
    out << std::get<0>(params) << "__"
        << gl::GLenumToString(gl::GLESEnum::AllEnums, std::get<1>(params)) << "_"
        << (std::get<2>(params) == 0
                ? "NoDepthStencil"
                : gl::GLenumToString(gl::GLESEnum::AllEnums, std::get<2>(params)))
        << "_" << (std::get<3>(params) ? "Texture" : "Renderbuffer");
    if (std::get<4>(params) > 0)
    {
        out << "_MSAA" << std::get<4>(params);
    }
    return out.str();
}

constexpr GLenum kDrawBuffersAnyFormatColorFormats[] = {
    // Normalized / float-castable
    GL_R8,
    GL_RG8,
    GL_RGB8,
    GL_RGB565,
    GL_RGB5_A1,
    GL_RGBA4,
    GL_RGB10_A2,
    GL_RGBA8,
    GL_SRGB8_ALPHA8,
    // Signed / unsigned integer
    GL_R8I,
    GL_R8UI,
    GL_R16I,
    GL_R16UI,
    GL_R32I,
    GL_R32UI,
    GL_RG8I,
    GL_RG8UI,
    GL_RG16I,
    GL_RG16UI,
    GL_RG32I,
    GL_RG32UI,
    GL_RGBA8I,
    GL_RGBA8UI,
    GL_RGBA16I,
    GL_RGBA16UI,
    GL_RGBA32I,
    GL_RGBA32UI,
    GL_RGB10_A2UI,
    // Float, require EXT_color_buffer_half_float / EXT_color_buffer_float.
    GL_R16F,
    GL_RG16F,
    GL_RGBA16F,
    GL_RGB16F,  // Renderable only via EXT_color_buffer_half_float;
    GL_R32F,
    GL_RG32F,
    GL_RGBA32F,
    GL_R11F_G11F_B10F,
    // Shared exponent, requires GL_QCOM_render_shared_exponent to render.
    GL_RGB9_E5,
    // Unsigned normalized 16-bit, requires GL_EXT_texture_norm16 to render.
    GL_R16_EXT,
    GL_RG16_EXT,
    GL_RGBA16_EXT,
    // Signed normalized 8-bit, requires GL_EXT_render_snorm to render.
    GL_R8_SNORM,
    GL_RG8_SNORM,
    GL_RGBA8_SNORM,
    // Signed normalized 16-bit, requires GL_EXT_render_snorm + GL_EXT_texture_norm16 to render.
    GL_R16_SNORM_EXT,
    GL_RG16_SNORM_EXT,
    GL_RGBA16_SNORM_EXT,
    // Requires GL_EXT_texture_format_BGRA8888.
    GL_BGRA8_EXT,
    // Requires GL_ANGLE_rgbx_internal_format.
    GL_RGBX8_ANGLE,
};

constexpr GLenum kDrawBuffersAnyFormatDepthStencilFormats[] = {
    0,  // No depth/stencil attachment
    GL_DEPTH_COMPONENT16,
    GL_DEPTH_COMPONENT24,
    GL_DEPTH_COMPONENT32F,
    GL_STENCIL_INDEX8,
    GL_DEPTH24_STENCIL8,
    GL_DEPTH32F_STENCIL8,
};

constexpr GLenum kDrawBuffersAnyFormatMSAAColorFormats[] = {
    GL_R8,
    GL_RG8,
    GL_RGB8,
    GL_RGB565,
    GL_RGB5_A1,
    GL_RGBA4,
    GL_RGB10_A2,
    GL_RGBA8,
    GL_SRGB8_ALPHA8,
    // Half/float color.
    GL_R16F,
    GL_RG16F,
    GL_RGBA16F,
    // RGB half-float is renderable only via EXT_color_buffer_half_float.
    GL_RGB16F,
    GL_R32F,
    GL_RG32F,
    GL_RGBA32F,
    GL_R11F_G11F_B10F,
    // Shared exponent.
    GL_RGB9_E5,
    // Unsigned normalized 16-bit.
    GL_R16_EXT,
    GL_RG16_EXT,
    GL_RGBA16_EXT,
    // Signed normalized 8-bit.
    GL_R8_SNORM,
    GL_RG8_SNORM,
    GL_RGBA8_SNORM,
    // Signed normalized 16-bit.
    GL_R16_SNORM_EXT,
    GL_RG16_SNORM_EXT,
    GL_RGBA16_SNORM_EXT,
    // BGRA8, requires GL_EXT_texture_format_BGRA8888.
    GL_BGRA8_EXT,
    // RGBX8, requires GL_ANGLE_rgbx_internal_format.
    GL_RGBX8_ANGLE,
};

constexpr GLint kDrawBuffersAnyFormatMaxTestedSampleCount = 4;

struct DrawBuffersAnyFormatExpectationPerSampleResult
{
    GLint sampleCount;                  // 0 = single-sampled
    GLint firstFailingDrawBufferCount;  // Count where FRAMEBUFFER_UNSUPPORTED starts.
};

struct DrawBuffersAnyFormatExpectation
{
    const char *deviceNameSubstring;
    GLenum colorFormat;
    DrawBuffersAnyFormatExpectationPerSampleResult sampleCounts[3];
};

// Per-known device results for framebuffer-completeness for the
// (color format, sample count) keys listed.
// If the device does not appear in the table at all, it is unknown
// device. Any framebuffer status at any buffer count is tolerated.
// For known devices:
// For this (color format, sampleCount), the FBO is expected to
// become FRAMEBUFFER_UNSUPPORTED at firstFailingDrawBufferCount
// and remain unsupported for larger counts.
//
// firstFailingDrawBufferCount:
//   0 -- FBO is expected FRAMEBUFFER_UNSUPPORTED at every count
//   9 -- FBO is expected FRAMEBUFFER_COMPLETE at every count
constexpr DrawBuffersAnyFormatExpectation kDrawBuffersAnyFormatExpectedFramebufferIncomplete[] = {
#if ANGLE_PLATFORM_MACOS
    // Force expectations by having one result: all framebuffers are complete.
    {"Apple", GL_RGBA32F, {{0, 9}, {2, 9}, {4, 9}}},
#elif ANGLE_PLATFORM_IOS_FAMILY_SIMULATOR
    // Simulator uses a 32-byte color tile budget but its emulated
    // pixelBytes don't match a real Apple GPU.  Many "small" formats
    // come out oversized.
    {"Apple iOS simulator GPU", GL_R11F_G11F_B10F, {{0, 5}, {2, 5}, {4, 5}}},
    {"Apple iOS simulator GPU", GL_RG32F, {{0, 5}, {2, 0}, {4, 0}}},
    {"Apple iOS simulator GPU", GL_RG32I, {{0, 5}, {2, 0}, {4, 0}}},
    {"Apple iOS simulator GPU", GL_RG32UI, {{0, 5}, {2, 0}, {4, 0}}},
    {"Apple iOS simulator GPU", GL_RGB10_A2, {{0, 5}, {2, 5}, {4, 5}}},
    {"Apple iOS simulator GPU", GL_RGB10_A2UI, {{0, 9}, {2, 0}, {4, 0}}},
    {"Apple iOS simulator GPU", GL_RGB565, {{0, 9}, {2, 5}, {4, 5}}},
    {"Apple iOS simulator GPU", GL_RGB5_A1, {{0, 9}, {2, 5}, {4, 5}}},
    {"Apple iOS simulator GPU", GL_RGB8, {{0, 9}, {2, 5}, {4, 5}}},
    {"Apple iOS simulator GPU", GL_RGB16F, {{0, 5}, {2, 5}, {4, 5}}},
    {"Apple iOS simulator GPU", GL_RGBA16F, {{0, 5}, {2, 5}, {4, 5}}},
    {"Apple iOS simulator GPU", GL_RGBA16I, {{0, 5}, {2, 0}, {4, 0}}},
    {"Apple iOS simulator GPU", GL_RGBA16UI, {{0, 5}, {2, 0}, {4, 0}}},
    {"Apple iOS simulator GPU", GL_RGBA32F, {{0, 3}, {2, 0}, {4, 0}}},
    {"Apple iOS simulator GPU", GL_RGBA32I, {{0, 3}, {2, 0}, {4, 0}}},
    {"Apple iOS simulator GPU", GL_RGBA32UI, {{0, 3}, {2, 0}, {4, 0}}},
    {"Apple iOS simulator GPU", GL_RGBA4, {{0, 9}, {2, 5}, {4, 5}}},
    {"Apple iOS simulator GPU", GL_RGBA8, {{0, 9}, {2, 5}, {4, 5}}},
    {"Apple iOS simulator GPU", GL_RGBA8_SNORM, {{0, 9}, {2, 5}, {4, 5}}},
    {"Apple iOS simulator GPU", GL_RGBA16_EXT, {{0, 5}, {2, 5}, {4, 5}}},
    {"Apple iOS simulator GPU", GL_RGBA16_SNORM_EXT, {{0, 5}, {2, 5}, {4, 5}}},
    {"Apple iOS simulator GPU", GL_SRGB8_ALPHA8, {{0, 9}, {2, 5}, {4, 5}}},
    {"Apple iOS simulator GPU", GL_BGRA8_EXT, {{0, 9}, {2, 5}, {4, 5}}},
#elif ANGLE_PLATFORM_IOS_FAMILY
    // Apple A12*, Apple5, 64 bytes of implicit image block size.
    {"Apple A12", GL_RGBA32F, {{0, 5}, {2, 5}, {4, 5}}},
    {"Apple A12", GL_RGBA32I, {{0, 5}, {2, 0}, {4, 0}}},
    {"Apple A12", GL_RGBA32UI, {{0, 5}, {2, 0}, {4, 0}}},

    // Apple A13*, Apple6, 64 bytes of implicit image block size.
    {"Apple A13", GL_RGBA32F, {{0, 5}, {2, 5}, {4, 5}}},
    {"Apple A13", GL_RGBA32I, {{0, 5}, {2, 0}, {4, 0}}},
    {"Apple A13", GL_RGBA32UI, {{0, 5}, {2, 0}, {4, 0}}},

    // Force expect success, Apple7+, 128 bytes of implicit image block size.
    {"Apple A14", GL_RGBA32F, {{0, 9}, {2, 9}, {4, 9}}},
    {"Apple A15", GL_RGBA32F, {{0, 9}, {2, 9}, {4, 9}}},
    {"Apple A16", GL_RGBA32F, {{0, 9}, {2, 9}, {4, 9}}},
    {"Apple A17", GL_RGBA32F, {{0, 9}, {2, 9}, {4, 9}}},
    {"Apple A18", GL_RGBA32F, {{0, 9}, {2, 9}, {4, 9}}},
    {"Apple A19", GL_RGBA32F, {{0, 9}, {2, 9}, {4, 9}}},
    {"Apple M", GL_RGBA32F, {{0, 9}, {2, 9}, {4, 9}}},
#endif
};

class DrawBuffersAnyFormatTestES3 : public ANGLETest<DrawBuffersAnyFormatVariationsTestParams>
{
  protected:
    DrawBuffersAnyFormatTestES3()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setConfigDepthBits(24);
        setConfigStencilBits(8);
    }

    GLenum colorFormat() const { return std::get<1>(GetParam()); }
    GLenum depthStencilFormat() const { return std::get<2>(GetParam()); }
    bool useTexture() const { return std::get<3>(GetParam()); }
    GLint samples() const { return std::get<4>(GetParam()); }

    bool isSignedInt() const;
    bool isUnsignedInt() const;
    bool isFloatColorFormat() const;
    bool isSnorm8ColorFormat() const;
    bool isSnorm16ColorFormat() const;

    static GLenum DepthStencilAttachmentFor(GLenum format);
    static int NumColorChannels(GLenum internalformat);

    void setUpFramebufferAttachments(GLFramebuffer &fb, GLint numAttachments);

    void clearColorAttachment(GLint i);
    testing::AssertionResult verifyColorAttachment(GLint i);

    void clearAll(GLint numAttachments);

    // Verifies each color attachment was cleared to its per-attachment value.
    testing::AssertionResult verifyAttachments(GLint numAttachments);

    bool deviceSupportsParameters() const;

    testing::AssertionResult verifyKnownDeviceExpectedFramebufferStatus(GLenum status,
                                                                        GLint numAttachments) const;
};

bool DrawBuffersAnyFormatTestES3::isSignedInt() const
{
    switch (colorFormat())
    {
        case GL_R8I:
        case GL_R16I:
        case GL_R32I:
        case GL_RG8I:
        case GL_RG16I:
        case GL_RG32I:
        case GL_RGBA8I:
        case GL_RGBA16I:
        case GL_RGBA32I:
            return true;
        default:
            return false;
    }
}

bool DrawBuffersAnyFormatTestES3::isUnsignedInt() const
{
    switch (colorFormat())
    {
        case GL_R8UI:
        case GL_R16UI:
        case GL_R32UI:
        case GL_RG8UI:
        case GL_RG16UI:
        case GL_RG32UI:
        case GL_RGBA8UI:
        case GL_RGBA16UI:
        case GL_RGBA32UI:
        case GL_RGB10_A2UI:
            return true;
        default:
            return false;
    }
}

GLenum DrawBuffersAnyFormatTestES3::DepthStencilAttachmentFor(GLenum format)
{
    switch (format)
    {
        case GL_DEPTH_COMPONENT16:
        case GL_DEPTH_COMPONENT24:
        case GL_DEPTH_COMPONENT32F:
            return GL_DEPTH_ATTACHMENT;
        case GL_STENCIL_INDEX8:
            return GL_STENCIL_ATTACHMENT;
        case GL_DEPTH24_STENCIL8:
        case GL_DEPTH32F_STENCIL8:
            return GL_DEPTH_STENCIL_ATTACHMENT;
        default:
            return 0;
    }
}

bool DrawBuffersAnyFormatTestES3::isFloatColorFormat() const
{
    switch (colorFormat())
    {
        case GL_R16F:
        case GL_RG16F:
        case GL_RGBA16F:
        case GL_RGB16F:
        case GL_R32F:
        case GL_RG32F:
        case GL_RGBA32F:
        case GL_R11F_G11F_B10F:
        case GL_RGB9_E5:
            return true;
        default:
            return false;
    }
}

bool DrawBuffersAnyFormatTestES3::isSnorm8ColorFormat() const
{
    switch (colorFormat())
    {
        case GL_R8_SNORM:
        case GL_RG8_SNORM:
        case GL_RGBA8_SNORM:
            return true;
        default:
            return false;
    }
}

bool DrawBuffersAnyFormatTestES3::isSnorm16ColorFormat() const
{
    switch (colorFormat())
    {
        case GL_R16_SNORM_EXT:
        case GL_RG16_SNORM_EXT:
        case GL_RGBA16_SNORM_EXT:
            return true;
        default:
            return false;
    }
}

int DrawBuffersAnyFormatTestES3::NumColorChannels(GLenum internalformat)
{
    switch (internalformat)
    {
        case GL_R8:
        case GL_R8I:
        case GL_R8UI:
        case GL_R16I:
        case GL_R16UI:
        case GL_R32I:
        case GL_R32UI:
        case GL_R16F:
        case GL_R32F:
        case GL_R16_EXT:
        case GL_R8_SNORM:
        case GL_R16_SNORM_EXT:
            return 1;
        case GL_RG8:
        case GL_RG8I:
        case GL_RG8UI:
        case GL_RG16I:
        case GL_RG16UI:
        case GL_RG32I:
        case GL_RG32UI:
        case GL_RG16F:
        case GL_RG32F:
        case GL_RG16_EXT:
        case GL_RG8_SNORM:
        case GL_RG16_SNORM_EXT:
            return 2;
        case GL_RGB8:
        case GL_RGB565:
        case GL_RGB16F:
        case GL_R11F_G11F_B10F:
        case GL_RGB9_E5:
        case GL_RGBX8_ANGLE:
            return 3;
        default:
            return 4;
    }
}

void DrawBuffersAnyFormatTestES3::setUpFramebufferAttachments(GLFramebuffer &fb,
                                                              GLint numAttachments)
{
    constexpr GLsizei kSize = 16;
    const GLenum cFormat    = colorFormat();
    const GLenum dsFormat   = depthStencilFormat();
    const GLenum dsAttach   = DepthStencilAttachmentFor(dsFormat);
    const GLint msaa        = samples();

    glBindFramebuffer(GL_FRAMEBUFFER, fb);

    if (useTexture())
    {
        // Multisampled textures require ES 3.1 or an extension.  The MSAA
        // suite uses renderbuffers only; this branch is for samples == 0.
        ASSERT_EQ(msaa, 0);
        std::vector<GLTexture> colorTexs(numAttachments);
        for (GLint i = 0; i < numAttachments; ++i)
        {
            glBindTexture(GL_TEXTURE_2D, colorTexs[i]);
            glTexStorage2D(GL_TEXTURE_2D, 1, cFormat, kSize, kSize);
            ASSERT_GL_NO_ERROR() << i;
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D,
                                   colorTexs[i], 0);
            ASSERT_GL_NO_ERROR() << i;
        }
        GLTexture dsTex;
        if (dsAttach != 0)
        {
            glBindTexture(GL_TEXTURE_2D, dsTex);
            glTexStorage2D(GL_TEXTURE_2D, 1, dsFormat, kSize, kSize);
            glFramebufferTexture2D(GL_FRAMEBUFFER, dsAttach, GL_TEXTURE_2D, dsTex, 0);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    else
    {
        std::vector<GLRenderbuffer> colorRbs(numAttachments);
        for (GLint i = 0; i < numAttachments; ++i)
        {
            glBindRenderbuffer(GL_RENDERBUFFER, colorRbs[i]);
            if (msaa > 0)
            {
                glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaa, cFormat, kSize, kSize);
            }
            else
            {
                glRenderbufferStorage(GL_RENDERBUFFER, cFormat, kSize, kSize);
            }
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_RENDERBUFFER,
                                      colorRbs[i]);
        }
        GLRenderbuffer dsRb;
        if (dsAttach != 0)
        {
            glBindRenderbuffer(GL_RENDERBUFFER, dsRb);
            if (msaa > 0)
            {
                glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaa, dsFormat, kSize, kSize);
            }
            else
            {
                glRenderbufferStorage(GL_RENDERBUFFER, dsFormat, kSize, kSize);
            }
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, dsAttach, GL_RENDERBUFFER, dsRb);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

void DrawBuffersAnyFormatTestES3::clearColorAttachment(GLint i)
{
    // Per-attachment R/G/B is the bit decomposition of (i+1); A is always
    // "max".  Using only 0 / max (=255 / 1.0 / 1) values means the cleared
    // values round-trip through any normalized format.
    const bool R = ((i + 1) & 1) != 0;
    const bool G = ((i + 1) & 2) != 0;
    const bool B = ((i + 1) & 4) != 0;
    if (isUnsignedInt())
    {
        const GLuint v[4] = {R ? 1u : 0u, G ? 1u : 0u, B ? 1u : 0u, 1u};
        glClearBufferuiv(GL_COLOR, i, v);
    }
    else if (isSignedInt())
    {
        const GLint v[4] = {R ? 1 : 0, G ? 1 : 0, B ? 1 : 0, 1};
        glClearBufferiv(GL_COLOR, i, v);
    }
    else
    {
        const GLfloat v[4] = {R ? 1.0f : 0.0f, G ? 1.0f : 0.0f, B ? 1.0f : 0.0f, 1.0f};
        glClearBufferfv(GL_COLOR, i, v);
    }
}

testing::AssertionResult DrawBuffersAnyFormatTestES3::verifyColorAttachment(GLint i)
{
    GLFramebuffer resolveFb;
    GLRenderbuffer resolveRb;
    GLint sourceFb = 0;

    if (samples() > 0)
    {
        constexpr GLsizei kSize = 16;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &sourceFb);

        glBindRenderbuffer(GL_RENDERBUFFER, resolveRb);
        glRenderbufferStorage(GL_RENDERBUFFER, colorFormat(), kSize, kSize);

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFb);
        glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                  resolveRb);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, sourceFb);
        glReadBuffer(GL_COLOR_ATTACHMENT0 + i);
        glBlitFramebuffer(0, 0, kSize, kSize, 0, 0, kSize, kSize, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, sourceFb);
        const GLenum blitErr = glGetError();
        if (blitErr != GL_NO_ERROR)
        {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, sourceFb);
            return testing::AssertionFailure()
                   << "MSAA blit-resolve attachment " << i << ": "
                   << gl::GLenumToString(gl::GLESEnum::AllEnums, blitErr);
        }

        glBindFramebuffer(GL_READ_FRAMEBUFFER, resolveFb);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
    }
    else
    {
        glReadBuffer(GL_COLOR_ATTACHMENT0 + i);
    }

    const int numC = NumColorChannels(colorFormat());

    // Mask the expected components by the format's channel count.  Channels
    // not present in the format read back as 0 (RGB) or "1" (alpha) per the
    // ES 3.0 spec.
    const bool R = numC >= 1 && ((i + 1) & 1) != 0;
    const bool G = numC >= 2 && ((i + 1) & 2) != 0;
    const bool B = numC >= 3 && ((i + 1) & 4) != 0;

    testing::AssertionResult result = testing::AssertionSuccess();

    if (isUnsignedInt())
    {
        GLuint pixel[4]          = {0u, 0u, 0u, 0u};
        const GLuint expected[4] = {R ? 1u : 0u, G ? 1u : 0u, B ? 1u : 0u, 1u};
        glReadPixels(0, 0, 1, 1, GL_RGBA_INTEGER, GL_UNSIGNED_INT, pixel);
        if (memcmp(pixel, expected, sizeof(pixel)) != 0)
        {
            result = testing::AssertionFailure()
                     << "color attachment " << i << ": got (" << pixel[0] << ", " << pixel[1]
                     << ", " << pixel[2] << ", " << pixel[3] << "), expected (" << expected[0]
                     << ", " << expected[1] << ", " << expected[2] << ", " << expected[3] << ")";
        }
    }
    else if (isSignedInt())
    {
        GLint pixel[4]          = {0, 0, 0, 0};
        const GLint expected[4] = {R ? 1 : 0, G ? 1 : 0, B ? 1 : 0, 1};
        glReadPixels(0, 0, 1, 1, GL_RGBA_INTEGER, GL_INT, pixel);
        if (memcmp(pixel, expected, sizeof(pixel)) != 0)
        {
            result = testing::AssertionFailure()
                     << "color attachment " << i << ": got (" << pixel[0] << ", " << pixel[1]
                     << ", " << pixel[2] << ", " << pixel[3] << "), expected (" << expected[0]
                     << ", " << expected[1] << ", " << expected[2] << ", " << expected[3] << ")";
        }
    }
    else if (isSnorm8ColorFormat())
    {
        // GL_EXT_render_snorm adds GL_RGBA + GL_BYTE to ReadPixels.  SNORM
        // 1.0 maps to 127.
        GLbyte pixel[4]          = {0, 0, 0, 0};
        const GLbyte expected[4] = {static_cast<GLbyte>(R ? 127 : 0),
                                    static_cast<GLbyte>(G ? 127 : 0),
                                    static_cast<GLbyte>(B ? 127 : 0), 127};
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_BYTE, pixel);
        if (std::memcmp(pixel, expected, sizeof(pixel)) != 0)
        {
            result = testing::AssertionFailure()
                     << "color attachment " << i << ": got (" << +pixel[0] << ", " << +pixel[1]
                     << ", " << +pixel[2] << ", " << +pixel[3] << "), expected (" << +expected[0]
                     << ", " << +expected[1] << ", " << +expected[2] << ", " << +expected[3] << ")";
        }
    }
    else if (isSnorm16ColorFormat())
    {
        // GL_EXT_render_snorm adds GL_RGBA + GL_SHORT to ReadPixels.  SNORM
        // 1.0 maps to 32767.
        GLshort pixel[4]          = {0, 0, 0, 0};
        const GLshort expected[4] = {static_cast<GLshort>(R ? 32767 : 0),
                                     static_cast<GLshort>(G ? 32767 : 0),
                                     static_cast<GLshort>(B ? 32767 : 0), 32767};
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_SHORT, pixel);
        if (std::memcmp(pixel, expected, sizeof(pixel)) != 0)
        {
            result = testing::AssertionFailure()
                     << "color attachment " << i << ": got (" << pixel[0] << ", " << pixel[1]
                     << ", " << pixel[2] << ", " << pixel[3] << "), expected (" << expected[0]
                     << ", " << expected[1] << ", " << expected[2] << ", " << expected[3] << ")";
        }
    }
    else if (isFloatColorFormat())
    {
        const GLColor32F expected(R ? 1.0f : 0.0f, G ? 1.0f : 0.0f, B ? 1.0f : 0.0f, 1.0f);
        const GLColor32F actual = angle::ReadColor32F(0, 0);
        if (actual != expected)
        {
            result = testing::AssertionFailure()
                     << "color attachment " << i << ": got " << actual << ", expected " << expected;
        }
    }
    else
    {
        const GLColor expected(static_cast<GLubyte>(R ? 255 : 0), static_cast<GLubyte>(G ? 255 : 0),
                               static_cast<GLubyte>(B ? 255 : 0), 255);
        const GLColor actual = angle::ReadColor(0, 0);
        if (actual != expected)
        {
            result = testing::AssertionFailure()
                     << "color attachment " << i << ": got " << actual << ", expected " << expected;
        }
    }

    if (samples() > 0)
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, sourceFb);
    }
    return result;
}

void DrawBuffersAnyFormatTestES3::clearAll(GLint numAttachments)
{
    const GLenum dsAttach = DepthStencilAttachmentFor(depthStencilFormat());

    for (GLint i = 0; i < numAttachments; ++i)
    {
        clearColorAttachment(i);
    }

    GLbitfield clearMask = 0;
    if (dsAttach == GL_DEPTH_ATTACHMENT || dsAttach == GL_DEPTH_STENCIL_ATTACHMENT)
    {
        glClearDepthf(1.0f);
        clearMask |= GL_DEPTH_BUFFER_BIT;
    }
    if (dsAttach == GL_STENCIL_ATTACHMENT || dsAttach == GL_DEPTH_STENCIL_ATTACHMENT)
    {
        glClearStencil(0x55);
        clearMask |= GL_STENCIL_BUFFER_BIT;
    }
    if (clearMask != 0)
    {
        glClear(clearMask);
    }
}

testing::AssertionResult DrawBuffersAnyFormatTestES3::verifyAttachments(GLint numAttachments)
{
    for (GLint i = 0; i < numAttachments; ++i)
    {
        testing::AssertionResult r = verifyColorAttachment(i);
        if (!r)
        {
            return r;
        }
    }
    return testing::AssertionSuccess();
}

bool DrawBuffersAnyFormatTestES3::deviceSupportsParameters() const
{
    switch (colorFormat())
    {
        case GL_R16F:
        case GL_RG16F:
        case GL_RGBA16F:
            if (!EnsureGLExtensionEnabled("GL_EXT_color_buffer_half_float") &&
                !EnsureGLExtensionEnabled("GL_EXT_color_buffer_float"))
            {
                return false;
            }
            break;
        case GL_RGB16F:
            if (!EnsureGLExtensionEnabled("GL_EXT_color_buffer_half_float"))
            {
                return false;
            }
            break;
        case GL_R32F:
        case GL_RG32F:
        case GL_RGBA32F:
        case GL_R11F_G11F_B10F:
            if (!EnsureGLExtensionEnabled("GL_EXT_color_buffer_float"))
            {
                return false;
            }
            break;
        case GL_RGB9_E5:
            if (!EnsureGLExtensionEnabled("GL_QCOM_render_shared_exponent"))
            {
                return false;
            }
            break;
        case GL_R16_EXT:
        case GL_RG16_EXT:
        case GL_RGBA16_EXT:
            if (!EnsureGLExtensionEnabled("GL_EXT_texture_norm16"))
            {
                return false;
            }
            break;
        case GL_R8_SNORM:
        case GL_RG8_SNORM:
        case GL_RGBA8_SNORM:
            if (!EnsureGLExtensionEnabled("GL_EXT_render_snorm"))
            {
                return false;
            }
            break;
        case GL_R16_SNORM_EXT:
        case GL_RG16_SNORM_EXT:
        case GL_RGBA16_SNORM_EXT:
            if (!EnsureGLExtensionEnabled("GL_EXT_render_snorm") ||
                !EnsureGLExtensionEnabled("GL_EXT_texture_norm16"))
            {
                return false;
            }
            break;
        case GL_BGRA8_EXT:
            if (!EnsureGLExtensionEnabled("GL_EXT_texture_format_BGRA8888"))
            {
                return false;
            }
            break;
        case GL_RGBX8_ANGLE:
            if (!EnsureGLExtensionEnabled("GL_ANGLE_rgbx_internal_format"))
            {
                return false;
            }
            break;
        default:
            break;
    }

    // Stencil-only attachments as textures require GL_OES_texture_stencil8.
    // Renderbuffer GL_STENCIL_INDEX8 is required by ES 3.0 with no extension.
    if (useTexture() && depthStencilFormat() == GL_STENCIL_INDEX8)
    {
        if (!EnsureGLExtensionEnabled("GL_OES_texture_stencil8"))
        {
            return false;
        }
    }

    if (samples() > 0)
    {
        if (isSignedInt() || isUnsignedInt())
        {
            UNREACHABLE();
            return false;
        }
        GLint maxSamplesForColor = 0;
        glGetInternalformativ(GL_RENDERBUFFER, colorFormat(), GL_SAMPLES, 1, &maxSamplesForColor);
        if (samples() > maxSamplesForColor)
        {
            return false;
        }
        if (depthStencilFormat() != 0)
        {
            GLint maxSamplesForDS = 0;
            glGetInternalformativ(GL_RENDERBUFFER, depthStencilFormat(), GL_SAMPLES, 1,
                                  &maxSamplesForDS);
            if (samples() > maxSamplesForDS)
            {
                return false;
            }
        }
    }

    return true;
}

testing::AssertionResult DrawBuffersAnyFormatTestES3::verifyKnownDeviceExpectedFramebufferStatus(
    GLenum status,
    GLint numAttachments) const
{
    // Known Devices have entry in the table.
    // For given (format, samples, numAttachments, status), verify status:
    //   * A matching entry covers the (format, samples, numAttachments)  ->
    //     Expect status incomplete.
    //   * status Complete   -> success (expectations default to success).
    //   * status Incomplete -> failure (missing expectation).

    const std::string deviceName(reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    bool hasExpectation = false;
    // If we have expectation, the default is that all framebuffers resolve to working.
    GLint expectedFirstFailingBufferCount = std::numeric_limits<GLint>::max();
    for (const DrawBuffersAnyFormatExpectation &e :
         kDrawBuffersAnyFormatExpectedFramebufferIncomplete)
    {
        if (deviceName.find(e.deviceNameSubstring) == std::string::npos)
        {
            continue;
        }
        // NOTE: If device has any entry in the table, we expect the table fully
        // define the status behavior.
        hasExpectation = true;
        if (e.colorFormat != colorFormat())
        {
            continue;
        }
        for (const DrawBuffersAnyFormatExpectationPerSampleResult &r : e.sampleCounts)
        {
            if (r.sampleCount == samples())
            {
                expectedFirstFailingBufferCount = r.firstFailingDrawBufferCount;
                break;
            }
        }
    }
    if (hasExpectation)
    {
        GLenum expectedStatus = static_cast<GLenum>(numAttachments < expectedFirstFailingBufferCount
                                                        ? GL_FRAMEBUFFER_COMPLETE
                                                        : GL_FRAMEBUFFER_UNSUPPORTED);
        if (status == expectedStatus)
        {
            return testing::AssertionSuccess();
        }
        return testing::AssertionFailure()
               << "device: " << deviceName << " n=" << numAttachments << " samples=" << samples()
               << " format=" << gl::GLenumToString(gl::GLESEnum::AllEnums, colorFormat())
               << " expected status: " << gl::GLenumToString(gl::GLESEnum::AllEnums, expectedStatus)
               << " got status: " << gl::GLenumToString(gl::GLESEnum::AllEnums, status);
    }
    // Device not in the table: any status is tolerated.  Toggle ((false)) ->
    // ((true)) to log a candidate row to add to the expectations table.
    if (status != GL_FRAMEBUFFER_COMPLETE && ((false)))
    {
        printf("DrawBuffersAnyFormat incomplete: {\"%s\", %s, {..., {%d, %d}, ...}},\n",
               deviceName.c_str(), gl::GLenumToString(gl::GLESEnum::AllEnums, colorFormat()),
               samples(), numAttachments);
    }
    return testing::AssertionSuccess();
}

// Fill FBO with maximum amount of attachments for a specific format.
// Test clear codepath with the FBO.
TEST_P(DrawBuffersAnyFormatTestES3, ClearIterativeAttachmentCount)
{
    ANGLE_SKIP_TEST_IF(!deviceSupportsParameters());

    GLint maxDrawBuffers = 0;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
    ASSERT_GE(maxDrawBuffers, 4);

    for (GLint n = maxDrawBuffers; n >= 1; --n)
    {
        SCOPED_TRACE(testing::Message() << "numAttachments=" << n);

        GLFramebuffer fb;
        setUpFramebufferAttachments(fb, n);
        glBindFramebuffer(GL_FRAMEBUFFER, fb);
        ASSERT_GL_NO_ERROR();

        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        EXPECT_TRUE(verifyKnownDeviceExpectedFramebufferStatus(status, n));
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            continue;
        }

        std::vector<GLenum> drawBuffers(n);
        for (GLint i = 0; i < n; ++i)
        {
            drawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;
        }
        glDrawBuffers(n, drawBuffers.data());

        clearAll(n);
        EXPECT_GL_NO_ERROR();
        EXPECT_TRUE(verifyAttachments(n));
        break;
    }
}

// Fill FBO with maximum amount of attachments for a specific format.
// Test clear-after-draw codepath with the FBO.
TEST_P(DrawBuffersAnyFormatTestES3, DrawThenClearIterativeAttachmentCount)
{
    ANGLE_SKIP_TEST_IF(!deviceSupportsParameters());

    GLint maxDrawBuffers = 0;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
    ASSERT_GE(maxDrawBuffers, 4);

    for (GLint n = maxDrawBuffers; n >= 1; --n)
    {
        SCOPED_TRACE(testing::Message() << "numAttachments=" << n);

        GLFramebuffer fb;
        setUpFramebufferAttachments(fb, n);
        glBindFramebuffer(GL_FRAMEBUFFER, fb);
        ASSERT_GL_NO_ERROR();

        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        EXPECT_TRUE(verifyKnownDeviceExpectedFramebufferStatus(status, n));
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            continue;
        }

        std::vector<GLenum> drawBuffers(n);
        for (GLint i = 0; i < n; ++i)
        {
            drawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;
        }
        glDrawBuffers(n, drawBuffers.data());

        const GLenum dsAttach = DepthStencilAttachmentFor(depthStencilFormat());
        const bool hasDepth =
            dsAttach == GL_DEPTH_ATTACHMENT || dsAttach == GL_DEPTH_STENCIL_ATTACHMENT;
        const bool hasStencil =
            dsAttach == GL_STENCIL_ATTACHMENT || dsAttach == GL_DEPTH_STENCIL_ATTACHMENT;
        if (hasDepth)
        {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_ALWAYS);
        }
        else
        {
            glDisable(GL_DEPTH_TEST);
        }
        if (hasStencil)
        {
            glEnable(GL_STENCIL_TEST);
            glStencilFunc(GL_ALWAYS, 0x3C, 0xFF);
            glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
            glStencilMask(0xFF);
        }
        else
        {
            glDisable(GL_STENCIL_TEST);
        }

        const char *outType = isUnsignedInt() ? "uvec4" : (isSignedInt() ? "ivec4" : "vec4");
        const char *outValue =
            isUnsignedInt() ? "uvec4(0u, 0u, 1u, 1u)"
                            : (isSignedInt() ? "ivec4(0, 0, 1, 1)" : "vec4(0.0, 0.0, 1.0, 1.0)");

        std::stringstream fs;
        fs << "#version 300 es\nprecision highp float;\n";
        for (GLint i = 0; i < n; ++i)
        {
            fs << "layout(location = " << i << ") out " << outType << " value" << i << ";\n";
        }
        fs << "void main(){\n";
        for (GLint i = 0; i < n; ++i)
        {
            fs << "value" << i << " = " << outValue << ";\n";
        }
        fs << "}\n";

        ANGLE_GL_PROGRAM(drawMRT, essl3_shaders::vs::Simple(), fs.str().c_str());
        drawQuad(drawMRT, essl3_shaders::PositionAttrib(), 0.0f);

        clearAll(n);
        EXPECT_GL_NO_ERROR();
        EXPECT_TRUE(verifyAttachments(n));
        break;
    }
}

// Setup a big framebuffer and if it returns incomplete, expect clear to
// fail too.
TEST_P(DrawBuffersAnyFormatTestES3, IncompleteClearIsInvalidFramebufferOperation)
{
    ANGLE_SKIP_TEST_IF(!deviceSupportsParameters());

    GLint maxDrawBuffers = 0;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
    ASSERT_GE(maxDrawBuffers, 4);

    for (GLint n = 1; n <= maxDrawBuffers; ++n)
    {
        SCOPED_TRACE(testing::Message() << "numAttachments=" << n);

        GLFramebuffer fb;
        setUpFramebufferAttachments(fb, n);
        glBindFramebuffer(GL_FRAMEBUFFER, fb);
        ASSERT_GL_NO_ERROR();

        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        EXPECT_TRUE(verifyKnownDeviceExpectedFramebufferStatus(status, n));
        if (status == GL_FRAMEBUFFER_COMPLETE)
        {
            continue;
        }

        glClear(GL_COLOR_BUFFER_BIT);
        EXPECT_GL_ERROR(GL_INVALID_FRAMEBUFFER_OPERATION);
        glClear(GL_STENCIL_BUFFER_BIT);
        EXPECT_GL_ERROR(GL_INVALID_FRAMEBUFFER_OPERATION);
    }
}

// Sanity test: detects implementations that advertise a per-format max
// sample count exceeding what the MSAA instantiation of
// DrawBuffersAnyFormatTestES3 actually exercises.  If this fires, raise
// kDrawBuffersAnyFormatMaxTestedSampleCount and add the new value to the
// MSAA instantiation's sample-count list.
TEST_P(DrawBuffersTestES3, DrawBuffersAnyFormatMSAASampleLimitIsEnough)
{
    EnsureGLExtensionEnabled("GL_EXT_color_buffer_half_float");
    EnsureGLExtensionEnabled("GL_EXT_color_buffer_float");
    EnsureGLExtensionEnabled("GL_QCOM_render_shared_exponent");
    EnsureGLExtensionEnabled("GL_EXT_texture_norm16");
    EnsureGLExtensionEnabled("GL_EXT_render_snorm");
    EnsureGLExtensionEnabled("GL_EXT_texture_format_BGRA8888");
    EnsureGLExtensionEnabled("GL_ANGLE_rgbx_internal_format");

    auto checkFormat = [&](GLenum fmt) {
        GLint numCounts = 0;
        glGetInternalformativ(GL_RENDERBUFFER, fmt, GL_NUM_SAMPLE_COUNTS, 1, &numCounts);
        if (glGetError() != GL_NO_ERROR)
        {
            return;
        }
        if (numCounts == 0)
        {
            return;
        }
        GLint maxSamples = 0;
        glGetInternalformativ(GL_RENDERBUFFER, fmt, GL_SAMPLES, 1, &maxSamples);
        EXPECT_GL_NO_ERROR();
        EXPECT_LE(maxSamples, kDrawBuffersAnyFormatMaxTestedSampleCount)
            << "format: " << gl::GLenumToString(gl::GLESEnum::AllEnums, fmt);
    };

    for (GLenum fmt : kDrawBuffersAnyFormatMSAAColorFormats)
    {
        checkFormat(fmt);
    }
    for (GLenum fmt : kDrawBuffersAnyFormatDepthStencilFormats)
    {
        if (fmt == 0)
        {
            continue;  // sentinel for "no depth/stencil attachment"
        }
        checkFormat(fmt);
    }
}

ANGLE_INSTANTIATE_TEST_ES2_AND_ES3_AND(DrawBuffersTest,
                                       ES2_METAL().enable(Feature::LimitMaxDrawBuffersForTesting),
                                       ES3_METAL().enable(Feature::LimitMaxDrawBuffersForTesting),
                                       ES2_VULKAN()
                                           .disable(Feature::SupportsTransformFeedbackExtension)
                                           .disable(Feature::EmulateTransformFeedback));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(DrawBuffersWebGL2Test);
ANGLE_INSTANTIATE_TEST_ES3(DrawBuffersWebGL2Test);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(DrawBuffersTestES3);
ANGLE_INSTANTIATE_TEST_ES3(DrawBuffersTestES3);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ColorMaskForDrawBuffersTest);
ANGLE_INSTANTIATE_TEST_ES3(ColorMaskForDrawBuffersTest);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(DrawBuffersAnyFormatTestES3);

const angle::PlatformParameters kDrawBuffersAnyFormatPlatforms[] = {
    ANGLE_ALL_TEST_PLATFORMS_ES3,
    ES3_VULKAN().enable(Feature::ForceFallbackFormat),
    ES3_VULKAN().enable(Feature::PreferDrawClearOverVkCmdClearAttachments),
};

INSTANTIATE_TEST_SUITE_P(
    NonMSAA,
    DrawBuffersAnyFormatTestES3,
    testing::Combine(
        testing::ValuesIn(::angle::FilterTestParams(kDrawBuffersAnyFormatPlatforms,
                                                    ArraySize(kDrawBuffersAnyFormatPlatforms))),
        testing::ValuesIn(kDrawBuffersAnyFormatColorFormats),
        testing::ValuesIn(kDrawBuffersAnyFormatDepthStencilFormats),
        testing::Bool(),
        testing::Values<GLint>(0)),
    DrawBuffersAnyFormatVariationsTestPrint);

INSTANTIATE_TEST_SUITE_P(
    MSAA,
    DrawBuffersAnyFormatTestES3,
    testing::Combine(
        testing::ValuesIn(::angle::FilterTestParams(kDrawBuffersAnyFormatPlatforms,
                                                    ArraySize(kDrawBuffersAnyFormatPlatforms))),
        testing::ValuesIn(kDrawBuffersAnyFormatMSAAColorFormats),
        testing::ValuesIn(kDrawBuffersAnyFormatDepthStencilFormats),
        testing::Values(false),  // renderbuffers only; ES 3.0 has no MSAA textures
        testing::Values<GLint>(2, kDrawBuffersAnyFormatMaxTestedSampleCount)),
    DrawBuffersAnyFormatVariationsTestPrint);
