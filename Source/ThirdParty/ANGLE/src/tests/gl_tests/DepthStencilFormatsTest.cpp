//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

#include "common/mathutil.h"
#include "platform/WorkaroundsD3D.h"

using namespace angle;

class DepthStencilFormatsTestBase : public ANGLETest
{
  protected:
    DepthStencilFormatsTestBase()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    bool checkTexImageFormatSupport(GLenum format, GLenum type)
    {
        EXPECT_GL_NO_ERROR();

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, format, 1, 1, 0, format, type, nullptr);
        glDeleteTextures(1, &tex);

        return (glGetError() == GL_NO_ERROR);
    }

    bool checkTexStorageFormatSupport(GLenum internalFormat)
    {
        EXPECT_GL_NO_ERROR();

        GLuint tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexStorage2DEXT(GL_TEXTURE_2D, 1, internalFormat, 1, 1);
        glDeleteTextures(1, &tex);

        return (glGetError() == GL_NO_ERROR);
    }

    bool checkRenderbufferFormatSupport(GLenum internalFormat)
    {
        EXPECT_GL_NO_ERROR();

        GLuint rb = 0;
        glGenRenderbuffers(1, &rb);
        glBindRenderbuffer(GL_RENDERBUFFER, rb);
        glRenderbufferStorage(GL_RENDERBUFFER, internalFormat, 1, 1);
        glDeleteRenderbuffers(1, &rb);

        return (glGetError() == GL_NO_ERROR);
    }

    virtual void SetUp()
    {
        ANGLETest::SetUp();

        constexpr char kVS[] = R"(precision highp float;
attribute vec4 position;
varying vec2 texcoord;

void main()
{
    gl_Position = position;
    texcoord = (position.xy * 0.5) + 0.5;
})";

        constexpr char kFS[] = R"(precision highp float;
uniform sampler2D tex;
varying vec2 texcoord;

void main()
{
    gl_FragColor = texture2D(tex, texcoord);
})";

        mProgram = CompileProgram(kVS, kFS);
        if (mProgram == 0)
        {
            FAIL() << "shader compilation failed.";
        }

        mTextureUniformLocation = glGetUniformLocation(mProgram, "tex");
        EXPECT_NE(-1, mTextureUniformLocation);

        glGenTextures(1, &mTexture);
        ASSERT_GL_NO_ERROR();
    }

    virtual void TearDown()
    {
        glDeleteProgram(mProgram);
        glDeleteTextures(1, &mTexture);

        ANGLETest::TearDown();
    }

    GLuint mProgram;
    GLuint mTexture;
    GLint mTextureUniformLocation;
};

class DepthStencilFormatsTest : public DepthStencilFormatsTestBase
{};

class DepthStencilFormatsTestES3 : public DepthStencilFormatsTestBase
{};

TEST_P(DepthStencilFormatsTest, DepthTexture)
{
    bool shouldHaveTextureSupport = IsGLExtensionEnabled("GL_ANGLE_depth_texture");
    EXPECT_EQ(shouldHaveTextureSupport,
              checkTexImageFormatSupport(GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT));
    EXPECT_EQ(shouldHaveTextureSupport,
              checkTexImageFormatSupport(GL_DEPTH_COMPONENT, GL_UNSIGNED_INT));

    if (IsGLExtensionEnabled("GL_EXT_texture_storage"))
    {
        EXPECT_EQ(shouldHaveTextureSupport, checkTexStorageFormatSupport(GL_DEPTH_COMPONENT16));
        EXPECT_EQ(shouldHaveTextureSupport, checkTexStorageFormatSupport(GL_DEPTH_COMPONENT32_OES));
    }
}

TEST_P(DepthStencilFormatsTest, PackedDepthStencil)
{
    // Expected to fail in D3D9 if GL_OES_packed_depth_stencil is not present.
    // Expected to fail in D3D11 if GL_OES_packed_depth_stencil or GL_ANGLE_depth_texture is not
    // present.

    bool shouldHaveRenderbufferSupport = IsGLExtensionEnabled("GL_OES_packed_depth_stencil");
    EXPECT_EQ(shouldHaveRenderbufferSupport,
              checkRenderbufferFormatSupport(GL_DEPTH24_STENCIL8_OES));

    bool shouldHaveTextureSupport = IsGLExtensionEnabled("GL_OES_packed_depth_stencil") &&
                                    IsGLExtensionEnabled("GL_ANGLE_depth_texture");
    EXPECT_EQ(shouldHaveTextureSupport,
              checkTexImageFormatSupport(GL_DEPTH_STENCIL_OES, GL_UNSIGNED_INT_24_8_OES));

    if (IsGLExtensionEnabled("GL_EXT_texture_storage"))
    {
        EXPECT_EQ(shouldHaveTextureSupport, checkTexStorageFormatSupport(GL_DEPTH24_STENCIL8_OES));
    }
}

TEST_P(DepthStencilFormatsTestES3, DrawWithDepthStencil)
{
    GLushort data[16];
    for (unsigned int i = 0; i < 16; i++)
    {
        data[i] = std::numeric_limits<GLushort>::max();
    }
    glBindTexture(GL_TEXTURE_2D, mTexture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT16, 4, 4);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glUseProgram(mProgram);
    glUniform1i(mTextureUniformLocation, 0);

    glClear(GL_COLOR_BUFFER_BIT);
    drawQuad(mProgram, "position", 0.5f);

    ASSERT_GL_NO_ERROR();

    GLubyte pixel[4];
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &pixel);

    // Only require the red and alpha channels have the correct values, the depth texture extensions
    // leave the green and blue channels undefined
    ASSERT_NEAR(255, pixel[0], 2.0);
    ASSERT_EQ(255, pixel[3]);
}

// This test reproduces a driver bug on Intel windows platforms on driver version
// from 4815 to 4901.
// When rendering with Stencil buffer enabled and depth buffer disabled, large
// viewport will lead to memory leak and driver crash. And the pixel result
// is a random value.
TEST_P(DepthStencilFormatsTestES3, DrawWithLargeViewport)
{
    ANGLE_SKIP_TEST_IF(IsIntel() && (IsOSX() || IsWindows()));

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());

    glEnable(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_TEST);

    // The iteration is to reproduce memory leak when rendering several times.
    for (int i = 0; i < 10; ++i)
    {
        // Create offscreen fbo and its color attachment and depth stencil attachment.
        GLTexture framebufferColorTexture;
        glBindTexture(GL_TEXTURE_2D, framebufferColorTexture);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, getWindowWidth(), getWindowHeight());
        ASSERT_GL_NO_ERROR();

        GLTexture framebufferStencilTexture;
        glBindTexture(GL_TEXTURE_2D, framebufferStencilTexture);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH24_STENCIL8, getWindowWidth(), getWindowHeight());
        ASSERT_GL_NO_ERROR();

        GLFramebuffer fb;
        glBindFramebuffer(GL_FRAMEBUFFER, fb);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               framebufferColorTexture, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                               framebufferStencilTexture, 0);

        EXPECT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
        ASSERT_GL_NO_ERROR();

        GLint kStencilRef = 4;
        glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
        glStencilFunc(GL_ALWAYS, kStencilRef, 0xFF);

        float viewport[2];
        glGetFloatv(GL_MAX_VIEWPORT_DIMS, viewport);

        glViewport(0, 0, static_cast<GLsizei>(viewport[0]), static_cast<GLsizei>(viewport[1]));
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb);

        drawQuad(program.get(), essl1_shaders::PositionAttrib(), 0.0f);
        ASSERT_GL_NO_ERROR();

        glBindFramebuffer(GL_READ_FRAMEBUFFER, fb);
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
        ASSERT_GL_NO_ERROR();
    }
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST(DepthStencilFormatsTest,
                       ES2_D3D9(),
                       ES2_D3D11(),
                       ES2_OPENGL(),
                       ES2_OPENGLES(),
                       ES2_VULKAN());
ANGLE_INSTANTIATE_TEST(DepthStencilFormatsTestES3, ES3_D3D11(), ES3_OPENGL(), ES3_OPENGLES());

class TinyDepthStencilWorkaroundTest : public ANGLETest
{
  public:
    TinyDepthStencilWorkaroundTest()
    {
        setWindowWidth(512);
        setWindowHeight(512);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    // Override the workarounds to enable "tiny" depth/stencil textures.
    void overrideWorkaroundsD3D(WorkaroundsD3D *workarounds) override
    {
        workarounds->emulateTinyStencilTextures = true;
    }
};

// Tests that the tiny depth stencil textures workaround does not "stick" depth textures.
// http://anglebug.com/1664
TEST_P(TinyDepthStencilWorkaroundTest, DepthTexturesStick)
{
    constexpr char kDrawVS[] =
        "#version 100\n"
        "attribute vec3 vertex;\n"
        "void main () {\n"
        "  gl_Position = vec4(vertex.x, vertex.y, vertex.z * 2.0 - 1.0, 1);\n"
        "}\n";

    ANGLE_GL_PROGRAM(drawProgram, kDrawVS, essl1_shaders::fs::Red());

    constexpr char kBlitVS[] =
        "#version 100\n"
        "attribute vec2 vertex;\n"
        "varying vec2 position;\n"
        "void main () {\n"
        "  position = vertex * .5 + .5;\n"
        "  gl_Position = vec4(vertex, 0, 1);\n"
        "}\n";

    constexpr char kBlitFS[] =
        "#version 100\n"
        "precision mediump float;\n"
        "uniform sampler2D texture;\n"
        "varying vec2 position;\n"
        "void main () {\n"
        "  gl_FragColor = vec4 (texture2D (texture, position).rrr, 1.);\n"
        "}\n";

    ANGLE_GL_PROGRAM(blitProgram, kBlitVS, kBlitFS);

    GLint blitTextureLocation = glGetUniformLocation(blitProgram.get(), "texture");
    ASSERT_NE(-1, blitTextureLocation);

    GLTexture colorTex;
    glBindTexture(GL_TEXTURE_2D, colorTex.get());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, getWindowWidth(), getWindowHeight(), 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLTexture depthTex;
    glBindTexture(GL_TEXTURE_2D, depthTex.get());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    ASSERT_EQ(getWindowWidth(), getWindowHeight());
    int levels = gl::log2(getWindowWidth());
    for (int mipLevel = 0; mipLevel <= levels; ++mipLevel)
    {
        int size = getWindowWidth() >> mipLevel;
        glTexImage2D(GL_TEXTURE_2D, mipLevel, GL_DEPTH_STENCIL, size, size, 0, GL_DEPTH_STENCIL,
                     GL_UNSIGNED_INT_24_8_OES, nullptr);
    }

    glBindTexture(GL_TEXTURE_2D, 0);

    ASSERT_GL_NO_ERROR();

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.get());

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex.get(), 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex.get(), 0);

    ASSERT_GL_NO_ERROR();

    glDepthRangef(0.0f, 1.0f);
    glViewport(0, 0, getWindowWidth(), getWindowHeight());
    glClearColor(0, 0, 0, 1);

    // Draw loop.
    for (unsigned int frame = 0; frame < 3; ++frame)
    {
        // draw into FBO
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.get());
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        glEnable(GL_DEPTH_TEST);

        float depth = ((frame % 2 == 0) ? 0.0f : 1.0f);
        drawQuad(drawProgram.get(), "vertex", depth);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // blit FBO
        glDisable(GL_DEPTH_TEST);

        glUseProgram(blitProgram.get());
        glUniform1i(blitTextureLocation, 0);
        glBindTexture(GL_TEXTURE_2D, depthTex.get());

        drawQuad(blitProgram.get(), "vertex", 0.5f);

        Vector4 depthVec(depth, depth, depth, 1);
        GLColor depthColor(depthVec);

        EXPECT_PIXEL_COLOR_NEAR(0, 0, depthColor, 1);
        ASSERT_GL_NO_ERROR();
    }
}

ANGLE_INSTANTIATE_TEST(TinyDepthStencilWorkaroundTest, ES3_D3D11());
