//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

class DrawBuffersTest : public ANGLETest
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

    void TearDown() override
    {
        glDeleteFramebuffers(1, &mFBO);
        glDeleteFramebuffers(1, &mReadFramebuffer);
        glDeleteTextures(4, mTextures);

        ANGLETest::TearDown();
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
        setWindowVisible(true);

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

    // TODO(ynovikov): Investigate the failure (http://anglebug.com/1535)
    ANGLE_SKIP_TEST_IF(IsWindows() && IsAMD() && IsDesktopOpenGL());

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

TEST_P(DrawBuffersTest, FirstAndLast)
{
    ANGLE_SKIP_TEST_IF(!setupTest());

    // TODO(ynovikov): Investigate the failure (https://anglebug.com/1533)
    ANGLE_SKIP_TEST_IF(IsWindows() && IsAMD() && IsDesktopOpenGL());

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

    // TODO(ynovikov): Investigate the failure (https://anglebug.com/1533)
    ANGLE_SKIP_TEST_IF(IsWindows() && IsAMD() && IsDesktopOpenGL());

    bool flags[8]  = {false};
    GLenum bufs[8] = {GL_NONE};

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

// Test that non-zero draw buffers can be queried on the default framebuffer
TEST_P(DrawBuffersTest, DefaultFramebufferDrawBufferQuery)
{
    ANGLE_SKIP_TEST_IF(!setupTest());

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    GLint drawbuffer = 0;
    glGetIntegerv(GL_DRAW_BUFFER1, &drawbuffer);
    EXPECT_GL_NO_ERROR();

    EXPECT_EQ(GL_NONE, drawbuffer);
}

// Same as above but adds a state change from a program with different masks after a clear.
TEST_P(DrawBuffersWebGL2Test, TwoProgramsWithDifferentOutputsAndClear)
{
    // TODO(http://anglebug.com/2872): Broken on the GL back-end.
    ANGLE_SKIP_TEST_IF(IsOpenGL());

    // TODO(ynovikov): Investigate the failure (https://anglebug.com/1533)
    ANGLE_SKIP_TEST_IF(IsWindows() && IsAMD() && IsDesktopOpenGL());

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

    // Clear again. All attachments should be cleared.
    glClear(GL_COLOR_BUFFER_BIT);
    verifyAttachment2DColor(0, mTextures[0], GL_TEXTURE_2D, 0, GLColor::green);
    verifyAttachment2DColor(1, mTextures[1], GL_TEXTURE_2D, 0, GLColor::green);
    verifyAttachment2DColor(2, mTextures[2], GL_TEXTURE_2D, 0, GLColor::green);
    verifyAttachment2DColor(3, mTextures[3], GL_TEXTURE_2D, 0, GLColor::green);

    glDeleteProgram(program);
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

class DrawBuffersTestES3 : public DrawBuffersTest
{};

// Test that binding multiple layers of a 3D texture works correctly
TEST_P(DrawBuffersTestES3, 3DTextures)
{
    ANGLE_SKIP_TEST_IF(!setupTest());

    GLTexture texture;
    glBindTexture(GL_TEXTURE_3D, texture.get());
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, getWindowWidth(), getWindowHeight(), getWindowWidth(),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture.get(), 0, 0);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, texture.get(), 0, 1);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, texture.get(), 0, 2);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, texture.get(), 0, 3);

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

    verifyAttachmentLayer(0, texture.get(), 0, 0);
    verifyAttachmentLayer(1, texture.get(), 0, 1);
    verifyAttachmentLayer(2, texture.get(), 0, 2);
    verifyAttachmentLayer(3, texture.get(), 0, 3);

    EXPECT_GL_NO_ERROR();

    glDeleteProgram(program);
}

// Test that binding multiple layers of a 2D array texture works correctly
TEST_P(DrawBuffersTestES3, 2DArrayTextures)
{
    ANGLE_SKIP_TEST_IF(!setupTest());

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture.get());
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, getWindowWidth(), getWindowHeight(),
                 getWindowWidth(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture.get(), 0, 0);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, texture.get(), 0, 1);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, texture.get(), 0, 2);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, texture.get(), 0, 3);

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

    verifyAttachmentLayer(0, texture.get(), 0, 0);
    verifyAttachmentLayer(1, texture.get(), 0, 1);
    verifyAttachmentLayer(2, texture.get(), 0, 2);
    verifyAttachmentLayer(3, texture.get(), 0, 3);

    EXPECT_GL_NO_ERROR();

    glDeleteProgram(program);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST(DrawBuffersTest,
                       ES2_D3D11(),
                       ES3_D3D11(),
                       ES2_OPENGL(),
                       ES3_OPENGL(),
                       ES2_OPENGLES(),
                       ES3_OPENGLES(),
                       ES2_VULKAN());

ANGLE_INSTANTIATE_TEST(DrawBuffersWebGL2Test, ES3_D3D11(), ES3_OPENGL());

ANGLE_INSTANTIATE_TEST(DrawBuffersTestES3, ES3_D3D11(), ES3_OPENGL(), ES3_OPENGLES());
