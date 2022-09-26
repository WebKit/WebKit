//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"

#include "platform/FeaturesVk_autogen.h"
#include "test_utils/gl_raii.h"
#include "util/random_utils.h"
#include "util/shader_utils.h"
#include "util/test_utils.h"

using namespace angle;

namespace
{
class ClearTestBase : public ANGLETest<>
{
  protected:
    ClearTestBase()
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

    void testSetUp() override
    {
        mFBOs.resize(2, 0);
        glGenFramebuffers(2, mFBOs.data());

        ASSERT_GL_NO_ERROR();
    }

    void testTearDown() override
    {
        if (!mFBOs.empty())
        {
            glDeleteFramebuffers(static_cast<GLsizei>(mFBOs.size()), mFBOs.data());
        }

        if (!mTextures.empty())
        {
            glDeleteTextures(static_cast<GLsizei>(mTextures.size()), mTextures.data());
        }
    }

    std::vector<GLuint> mFBOs;
    std::vector<GLuint> mTextures;
};

class ClearTest : public ClearTestBase
{};

class ClearTestES3 : public ClearTestBase
{
  protected:
    void verifyDepth(float depthValue, uint32_t size)
    {
        // Use a small shader to verify depth.
        ANGLE_GL_PROGRAM(depthTestProgram, essl1_shaders::vs::Passthrough(),
                         essl1_shaders::fs::Blue());
        ANGLE_GL_PROGRAM(depthTestProgramFail, essl1_shaders::vs::Passthrough(),
                         essl1_shaders::fs::Red());
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        drawQuad(depthTestProgram, essl1_shaders::PositionAttrib(), depthValue * 2 - 1 - 0.01f);
        drawQuad(depthTestProgramFail, essl1_shaders::PositionAttrib(), depthValue * 2 - 1 + 0.01f);
        glDisable(GL_DEPTH_TEST);
        ASSERT_GL_NO_ERROR();

        EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor::blue, 1);
        EXPECT_PIXEL_COLOR_NEAR(size - 1, 0, GLColor::blue, 1);
        EXPECT_PIXEL_COLOR_NEAR(0, size - 1, GLColor::blue, 1);
        EXPECT_PIXEL_COLOR_NEAR(size - 1, size - 1, GLColor::blue, 1);
    }

    void verifyStencil(uint32_t stencilValue, uint32_t size)
    {
        // Use another small shader to verify stencil.
        ANGLE_GL_PROGRAM(stencilTestProgram, essl1_shaders::vs::Passthrough(),
                         essl1_shaders::fs::Green());
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_EQUAL, stencilValue, 0xFF);
        drawQuad(stencilTestProgram, essl1_shaders::PositionAttrib(), 0.0f);
        glDisable(GL_STENCIL_TEST);
        ASSERT_GL_NO_ERROR();

        EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor::green, 1);
        EXPECT_PIXEL_COLOR_NEAR(size - 1, 0, GLColor::green, 1);
        EXPECT_PIXEL_COLOR_NEAR(0, size - 1, GLColor::green, 1);
        EXPECT_PIXEL_COLOR_NEAR(size - 1, size - 1, GLColor::green, 1);
    }
};

class ClearTestRGB : public ANGLETest<>
{
  protected:
    ClearTestRGB()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
    }
};

class ClearTestRGB_ES3 : public ClearTestRGB
{};

// Each int parameter can have three values: don't clear, clear, or masked clear.  The bool
// parameter controls scissor.
using MaskedScissoredClearVariationsTestParams =
    std::tuple<angle::PlatformParameters, int, int, int, bool>;

void ParseMaskedScissoredClearVariationsTestParams(
    const MaskedScissoredClearVariationsTestParams &params,
    bool *clearColor,
    bool *clearDepth,
    bool *clearStencil,
    bool *maskColor,
    bool *maskDepth,
    bool *maskStencil,
    bool *scissor)
{
    int colorClearInfo   = std::get<1>(params);
    int depthClearInfo   = std::get<2>(params);
    int stencilClearInfo = std::get<3>(params);

    *clearColor   = colorClearInfo > 0;
    *clearDepth   = depthClearInfo > 0;
    *clearStencil = stencilClearInfo > 0;

    *maskColor   = colorClearInfo > 1;
    *maskDepth   = depthClearInfo > 1;
    *maskStencil = stencilClearInfo > 1;

    *scissor = std::get<4>(params);
}

std::string MaskedScissoredClearVariationsTestPrint(
    const ::testing::TestParamInfo<MaskedScissoredClearVariationsTestParams> &paramsInfo)
{
    const MaskedScissoredClearVariationsTestParams &params = paramsInfo.param;
    std::ostringstream out;

    out << std::get<0>(params);

    bool clearColor, clearDepth, clearStencil;
    bool maskColor, maskDepth, maskStencil;
    bool scissor;

    ParseMaskedScissoredClearVariationsTestParams(params, &clearColor, &clearDepth, &clearStencil,
                                                  &maskColor, &maskDepth, &maskStencil, &scissor);

    if (scissor || clearColor || clearDepth || clearStencil || maskColor || maskDepth ||
        maskStencil)
    {
        out << "_";
    }

    if (scissor)
    {
        out << "_scissored";
    }

    if (clearColor || clearDepth || clearStencil)
    {
        out << "_clear_";
        if (clearColor)
        {
            out << "c";
        }
        if (clearDepth)
        {
            out << "d";
        }
        if (clearStencil)
        {
            out << "s";
        }
    }

    if (maskColor || maskDepth || maskStencil)
    {
        out << "_mask_";
        if (maskColor)
        {
            out << "c";
        }
        if (maskDepth)
        {
            out << "d";
        }
        if (maskStencil)
        {
            out << "s";
        }
    }

    return out.str();
}

class MaskedScissoredClearTestBase : public ANGLETest<MaskedScissoredClearVariationsTestParams>
{
  protected:
    MaskedScissoredClearTestBase()
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

    void maskedScissoredColorDepthStencilClear(
        const MaskedScissoredClearVariationsTestParams &params);

    bool mHasDepth   = true;
    bool mHasStencil = true;
};

class MaskedScissoredClearTest : public MaskedScissoredClearTestBase
{};

// Overrides a feature to force emulation of stencil-only and depth-only formats with a packed
// depth/stencil format
class VulkanClearTest : public MaskedScissoredClearTestBase
{
  protected:
    void testSetUp() override
    {
        glBindTexture(GL_TEXTURE_2D, mColorTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, getWindowWidth(), getWindowHeight(), 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, nullptr);

        // Setup Color/Stencil FBO with a stencil format that's emulated with packed depth/stencil.
        glBindFramebuffer(GL_FRAMEBUFFER, mColorStencilFBO);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mColorTexture,
                               0);
        glBindRenderbuffer(GL_RENDERBUFFER, mStencilTexture);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, getWindowWidth(),
                              getWindowHeight());
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                  mStencilTexture);

        ASSERT_GL_NO_ERROR();

        // Note: GL_DEPTH_COMPONENT24 is not allowed in GLES2.
        if (getClientMajorVersion() >= 3)
        {
            // Setup Color/Depth FBO with a depth format that's emulated with packed depth/stencil.
            glBindFramebuffer(GL_FRAMEBUFFER, mColorDepthFBO);

            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   mColorTexture, 0);
            glBindRenderbuffer(GL_RENDERBUFFER, mDepthTexture);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, getWindowWidth(),
                                  getWindowHeight());
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                      mDepthTexture);
        }

        ASSERT_GL_NO_ERROR();
    }

    void bindColorStencilFBO()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, mColorStencilFBO);
        mHasDepth = false;
    }

    void bindColorDepthFBO()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, mColorDepthFBO);
        mHasStencil = false;
    }

  private:
    GLFramebuffer mColorStencilFBO;
    GLFramebuffer mColorDepthFBO;
    GLTexture mColorTexture;
    GLRenderbuffer mDepthTexture;
    GLRenderbuffer mStencilTexture;
};

// Test clearing the default framebuffer
TEST_P(ClearTest, DefaultFramebuffer)
{
    glClearColor(0.25f, 0.5f, 0.5f, 0.5f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_NEAR(0, 0, 64, 128, 128, 128, 1.0);
}

// Test clearing the default framebuffer with scissor and mask
// This forces down path that uses draw to do clear
TEST_P(ClearTest, EmptyScissor)
{
    // These configs have bug that fails this test.
    // These configs are unmaintained so skipping.
    ANGLE_SKIP_TEST_IF(IsIntel() && IsD3D9());
    ANGLE_SKIP_TEST_IF(IsOSX());
    glClearColor(0.25f, 0.5f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST);
    glScissor(-10, 0, 5, 5);
    glClearColor(0.5f, 0.25f, 0.75f, 0.5f);
    glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_TRUE);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    EXPECT_PIXEL_NEAR(0, 0, 64, 128, 128, 255, 1.0);
}

// Test clearing the RGB default framebuffer and verify that the alpha channel is not cleared
TEST_P(ClearTestRGB, DefaultFramebufferRGB)
{
    // Some GPUs don't support RGB format default framebuffer,
    // so skip if the back buffer has alpha bits.
    EGLWindow *window          = getEGLWindow();
    EGLDisplay display         = window->getDisplay();
    EGLConfig config           = window->getConfig();
    EGLint backbufferAlphaBits = 0;
    eglGetConfigAttrib(display, config, EGL_ALPHA_SIZE, &backbufferAlphaBits);
    ANGLE_SKIP_TEST_IF(backbufferAlphaBits != 0);

    glClearColor(0.25f, 0.5f, 0.5f, 0.5f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_NEAR(0, 0, 64, 128, 128, 255, 1.0);
}

// Invalidate the RGB default framebuffer and verify that the alpha channel is not cleared, and
// stays set after drawing.
TEST_P(ClearTestRGB_ES3, InvalidateDefaultFramebufferRGB)
{
    ANGLE_GL_PROGRAM(blueProgram, essl1_shaders::vs::Simple(), essl1_shaders::fs::Blue());

    // Some GPUs don't support RGB format default framebuffer,
    // so skip if the back buffer has alpha bits.
    EGLWindow *window          = getEGLWindow();
    EGLDisplay display         = window->getDisplay();
    EGLConfig config           = window->getConfig();
    EGLint backbufferAlphaBits = 0;
    eglGetConfigAttrib(display, config, EGL_ALPHA_SIZE, &backbufferAlphaBits);
    ANGLE_SKIP_TEST_IF(backbufferAlphaBits != 0);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    // Verify that even though Alpha is cleared to 0.0 for this RGB FBO, it should be read back as
    // 1.0, since the glReadPixels() is issued with GL_RGBA.
    // OpenGL ES 3.2 spec:
    // 16.1.3 Obtaining Pixels from the Framebuffer
    // If G, B, or A values are not present in the internal format, they are taken to be zero,
    // zero, and one respectively.
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::black);

    const GLenum discards[] = {GL_COLOR};
    glInvalidateFramebuffer(GL_FRAMEBUFFER, 1, discards);

    // Don't explicitly clear, but draw blue (make sure alpha is not cleared)
    drawQuad(blueProgram, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);
}

// Draw with a shader that outputs alpha=0.5. Readback and ensure that alpha=1.
TEST_P(ClearTestRGB_ES3, ShaderOutputsAlphaVerifyReadingAlphaIsOne)
{
    ANGLE_GL_PROGRAM(blueProgram, essl1_shaders::vs::Simple(), essl1_shaders::fs::UniformColor());
    glUseProgram(blueProgram);

    // Some GPUs don't support RGB format default framebuffer,
    // so skip if the back buffer has alpha bits.
    EGLWindow *window          = getEGLWindow();
    EGLDisplay display         = window->getDisplay();
    EGLConfig config           = window->getConfig();
    EGLint backbufferAlphaBits = 0;
    eglGetConfigAttrib(display, config, EGL_ALPHA_SIZE, &backbufferAlphaBits);
    ANGLE_SKIP_TEST_IF(backbufferAlphaBits != 0);

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::black);

    GLint colorUniformLocation =
        glGetUniformLocation(blueProgram, angle::essl1_shaders::ColorUniform());
    ASSERT_NE(colorUniformLocation, -1);
    glUniform4f(colorUniformLocation, 0.0f, 0.0f, 1.0f, 0.5f);
    ASSERT_GL_NO_ERROR();

    // Don't explicitly clear, but draw blue (make sure alpha is not cleared)
    drawQuad(blueProgram, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);
}

// Test clearing a RGBA8 Framebuffer
TEST_P(ClearTest, RGBA8Framebuffer)
{
    glBindFramebuffer(GL_FRAMEBUFFER, mFBOs[0]);

    GLTexture texture;

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, getWindowWidth(), getWindowHeight(), 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    glClearColor(0.5f, 0.5f, 0.5f, 0.5f);
    glClear(GL_COLOR_BUFFER_BIT);

    EXPECT_PIXEL_NEAR(0, 0, 128, 128, 128, 128, 1.0);
}

// Test to validate that we can go from an RGBA framebuffer attachment, to an RGB one and still
// have a correct behavior after.
TEST_P(ClearTest, ChangeFramebufferAttachmentFromRGBAtoRGB)
{
    // http://anglebug.com/2689
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsAdreno() && IsOpenGLES());

    // http://anglebug.com/5165
    ANGLE_SKIP_TEST_IF(IsOSX() && IsDesktopOpenGL() && IsIntel());

    ANGLE_GL_PROGRAM(program, angle::essl1_shaders::vs::Simple(),
                     angle::essl1_shaders::fs::UniformColor());
    setupQuadVertexBuffer(0.5f, 1.0f);
    glUseProgram(program);
    GLint positionLocation = glGetAttribLocation(program, angle::essl1_shaders::PositionAttrib());
    ASSERT_NE(positionLocation, -1);
    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(positionLocation);

    GLint colorUniformLocation =
        glGetUniformLocation(program, angle::essl1_shaders::ColorUniform());
    ASSERT_NE(colorUniformLocation, -1);

    glUniform4f(colorUniformLocation, 1.0f, 1.0f, 1.0f, 0.5f);

    glBindFramebuffer(GL_FRAMEBUFFER, mFBOs[0]);

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, getWindowWidth(), getWindowHeight(), 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    // Initially clear to black.
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Clear with masked color.
    glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_TRUE);
    glClearColor(0.5f, 0.5f, 0.5f, 0.75f);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();

    // So far so good, we have an RGBA framebuffer that we've cleared to 0.5 everywhere.
    EXPECT_PIXEL_NEAR(0, 0, 128, 0, 128, 192, 1.0);

    // In the Vulkan backend, RGB textures are emulated with an RGBA texture format
    // underneath and we keep a special mask to know that we shouldn't touch the alpha
    // channel when we have that emulated texture. This test exists to validate that
    // this mask gets updated correctly when the framebuffer attachment changes.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, getWindowWidth(), getWindowHeight(), 0, GL_RGB,
                 GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    ASSERT_GL_NO_ERROR();

    glDrawArrays(GL_TRIANGLES, 0, 6);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_RECT_EQ(0, 0, getWindowWidth(), getWindowHeight(), GLColor::magenta);
}

// Test clearing a RGB8 Framebuffer with a color mask.
TEST_P(ClearTest, RGB8WithMaskFramebuffer)
{
    glBindFramebuffer(GL_FRAMEBUFFER, mFBOs[0]);

    GLTexture texture;

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, getWindowWidth(), getWindowHeight(), 0, GL_RGB,
                 GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    glClearColor(0.2f, 0.4f, 0.6f, 0.8f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Since there's no alpha, we expect to get 255 back instead of the clear value (204).
    EXPECT_PIXEL_NEAR(0, 0, 51, 102, 153, 255, 1.0);

    glColorMask(GL_TRUE, GL_TRUE, GL_FALSE, GL_TRUE);
    glClearColor(0.1f, 0.3f, 0.5f, 0.7f);
    glClear(GL_COLOR_BUFFER_BIT);

    // The blue channel was masked so its value should be unchanged.
    EXPECT_PIXEL_NEAR(0, 0, 26, 77, 153, 255, 1.0);

    // Restore default.
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

TEST_P(ClearTest, ClearIssue)
{
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glClearColor(0.0, 1.0, 0.0, 1.0);
    glClearDepthf(0.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_FRAMEBUFFER, mFBOs[0]);

    GLRenderbuffer rbo;
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB565, 16, 16);

    EXPECT_GL_NO_ERROR();

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo);

    EXPECT_GL_NO_ERROR();

    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClearDepthf(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    EXPECT_GL_NO_ERROR();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    ANGLE_GL_PROGRAM(blueProgram, essl1_shaders::vs::Simple(), essl1_shaders::fs::Blue());
    drawQuad(blueProgram, essl1_shaders::PositionAttrib(), 0.5f);

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Regression test for a bug where "glClearDepthf"'s argument was not clamped
// In GLES 2 they where declared as GLclampf and the behaviour is the same in GLES 3.2
TEST_P(ClearTest, ClearIsClamped)
{
    glClearDepthf(5.0f);

    GLfloat clear_depth;
    glGetFloatv(GL_DEPTH_CLEAR_VALUE, &clear_depth);
    EXPECT_EQ(1.0f, clear_depth);
}

// Regression test for a bug where "glDepthRangef"'s arguments were not clamped
// In GLES 2 they where declared as GLclampf and the behaviour is the same in GLES 3.2
TEST_P(ClearTest, DepthRangefIsClamped)
{
    glDepthRangef(1.1f, -4.0f);

    GLfloat depth_range[2];
    glGetFloatv(GL_DEPTH_RANGE, depth_range);
    EXPECT_EQ(1.0f, depth_range[0]);
    EXPECT_EQ(0.0f, depth_range[1]);
}

// Test scissored clears on Depth16
TEST_P(ClearTest, Depth16Scissored)
{
    GLRenderbuffer renderbuffer;
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    constexpr int kRenderbufferSize = 64;
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, kRenderbufferSize,
                          kRenderbufferSize);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, renderbuffer);

    glClearDepthf(0.0f);
    glClear(GL_DEPTH_BUFFER_BIT);

    glEnable(GL_SCISSOR_TEST);
    constexpr int kNumSteps = 13;
    for (int ndx = 1; ndx < kNumSteps; ndx++)
    {
        float perc = static_cast<float>(ndx) / static_cast<float>(kNumSteps);
        glScissor(0, 0, static_cast<int>(kRenderbufferSize * perc),
                  static_cast<int>(kRenderbufferSize * perc));
        glClearDepthf(perc);
        glClear(GL_DEPTH_BUFFER_BIT);
    }
}

// Test scissored clears on Stencil8
TEST_P(ClearTest, Stencil8Scissored)
{
    GLRenderbuffer renderbuffer;
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    constexpr int kRenderbufferSize = 64;
    glRenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8, kRenderbufferSize, kRenderbufferSize);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderbuffer);

    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);

    glEnable(GL_SCISSOR_TEST);
    constexpr int kNumSteps = 13;
    for (int ndx = 1; ndx < kNumSteps; ndx++)
    {
        float perc = static_cast<float>(ndx) / static_cast<float>(kNumSteps);
        glScissor(0, 0, static_cast<int>(kRenderbufferSize * perc),
                  static_cast<int>(kRenderbufferSize * perc));
        glClearStencil(static_cast<int>(perc * 255.0f));
        glClear(GL_STENCIL_BUFFER_BIT);
    }
}

// Covers a bug in the Vulkan back-end where starting a new command buffer in
// the masked clear would not trigger descriptor sets to be re-bound.
TEST_P(ClearTest, MaskedClearThenDrawWithUniform)
{
    // Initialize a program with a uniform.
    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), essl1_shaders::fs::UniformColor());
    glUseProgram(program);

    GLint uniLoc = glGetUniformLocation(program, essl1_shaders::ColorUniform());
    ASSERT_NE(-1, uniLoc);
    glUniform4f(uniLoc, 0.0f, 1.0f, 0.0f, 1.0f);

    // Initialize position attribute.
    GLint posLoc = glGetAttribLocation(program, essl1_shaders::PositionAttrib());
    ASSERT_NE(-1, posLoc);
    setupQuadVertexBuffer(0.5f, 1.0f);
    glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(posLoc);

    // Initialize a simple FBO.
    constexpr GLsizei kSize = 2;
    GLTexture clearTexture;
    glBindTexture(GL_TEXTURE_2D, clearTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, clearTexture, 0);

    glViewport(0, 0, kSize, kSize);

    // Clear and draw to flush out dirty bits.
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Flush to trigger a new serial.
    glFlush();

    // Enable color mask and draw again to trigger the bug.
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    glClearColor(1.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Clear with a mask to verify that masked clear is done properly
// (can't use inline or RenderOp clear when some color channels are masked)
TEST_P(ClearTestES3, ClearPlusMaskDrawAndClear)
{
    // Initialize a program with a uniform.
    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), essl1_shaders::fs::UniformColor());
    glUseProgram(program);

    GLint uniLoc = glGetUniformLocation(program, essl1_shaders::ColorUniform());
    ASSERT_NE(-1, uniLoc);
    glUniform4f(uniLoc, 0.0f, 1.0f, 0.0f, 1.0f);

    // Initialize position attribute.
    GLint posLoc = glGetAttribLocation(program, essl1_shaders::PositionAttrib());
    ASSERT_NE(-1, posLoc);
    setupQuadVertexBuffer(0.5f, 1.0f);
    glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(posLoc);

    // Initialize a simple FBO.
    constexpr GLsizei kSize = 2;
    GLTexture clearTexture;
    glBindTexture(GL_TEXTURE_2D, clearTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, clearTexture, 0);

    GLRenderbuffer depthStencil;
    glBindRenderbuffer(GL_RENDERBUFFER, depthStencil);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, kSize, kSize);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              depthStencil);
    ASSERT_GL_NO_ERROR();

    glViewport(0, 0, kSize, kSize);

    // Clear and draw to flush out dirty bits.
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepthf(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Draw green rectangle
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Enable color mask and draw again to trigger the bug.
    glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_TRUE);
    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Draw purple-ish rectangle, green should be masked off
    glUniform4f(uniLoc, 1.0f, 0.25f, 1.0f, 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::white);
}

// Test that clearing all buffers through glClearColor followed by a clear of a specific buffer
// clears to the correct values.
TEST_P(ClearTestES3, ClearMultipleAttachmentsFollowedBySpecificOne)
{
    constexpr uint32_t kSize            = 16;
    constexpr uint32_t kAttachmentCount = 4;
    std::vector<unsigned char> pixelData(kSize * kSize * 4, 255);

    glBindFramebuffer(GL_FRAMEBUFFER, mFBOs[0]);

    GLTexture textures[kAttachmentCount];
    GLenum drawBuffers[kAttachmentCount];
    GLColor clearValues[kAttachmentCount];

    for (uint32_t i = 0; i < kAttachmentCount; ++i)
    {
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     pixelData.data());
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, textures[i],
                               0);
        drawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;

        clearValues[i].R = static_cast<GLubyte>(1 + i * 20);
        clearValues[i].G = static_cast<GLubyte>(7 + i * 20);
        clearValues[i].B = static_cast<GLubyte>(12 + i * 20);
        clearValues[i].A = static_cast<GLubyte>(16 + i * 20);
    }

    glDrawBuffers(kAttachmentCount, drawBuffers);

    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::white);

    // Clear all targets.
    angle::Vector4 clearColor = clearValues[0].toNormalizedVector();
    glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();

    // Clear odd targets individually.
    for (uint32_t i = 1; i < kAttachmentCount; i += 2)
    {
        clearColor = clearValues[i].toNormalizedVector();
        glClearBufferfv(GL_COLOR, i, clearColor.data());
    }

    // Even attachments should be cleared to color 0, while odd attachments are cleared to their
    // respective color.
    for (uint32_t i = 0; i < kAttachmentCount; ++i)
    {
        glReadBuffer(GL_COLOR_ATTACHMENT0 + i);
        ASSERT_GL_NO_ERROR();

        uint32_t clearIndex   = i % 2 == 0 ? 0 : i;
        const GLColor &expect = clearValues[clearIndex];

        EXPECT_PIXEL_COLOR_EQ(0, 0, expect);
        EXPECT_PIXEL_COLOR_EQ(0, kSize - 1, expect);
        EXPECT_PIXEL_COLOR_EQ(kSize - 1, 0, expect);
        EXPECT_PIXEL_COLOR_EQ(kSize - 1, kSize - 1, expect);
    }
}

// Test that clearing each render target individually works.  In the Vulkan backend, this should be
// done in a single render pass.
TEST_P(ClearTestES3, ClearMultipleAttachmentsIndividually)
{
    // http://anglebug.com/4855
    ANGLE_SKIP_TEST_IF(IsWindows() && IsIntel() && IsVulkan());

    constexpr uint32_t kSize             = 16;
    constexpr uint32_t kAttachmentCount  = 2;
    constexpr float kDepthClearValue     = 0.125f;
    constexpr int32_t kStencilClearValue = 0x67;
    std::vector<unsigned char> pixelData(kSize * kSize * 4, 255);

    glBindFramebuffer(GL_FRAMEBUFFER, mFBOs[0]);

    GLTexture textures[kAttachmentCount];
    GLRenderbuffer depthStencil;
    GLenum drawBuffers[kAttachmentCount];
    GLColor clearValues[kAttachmentCount];

    for (uint32_t i = 0; i < kAttachmentCount; ++i)
    {
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     pixelData.data());
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, textures[i],
                               0);
        drawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;

        clearValues[i].R = static_cast<GLubyte>(1 + i * 20);
        clearValues[i].G = static_cast<GLubyte>(7 + i * 20);
        clearValues[i].B = static_cast<GLubyte>(12 + i * 20);
        clearValues[i].A = static_cast<GLubyte>(16 + i * 20);
    }

    glBindRenderbuffer(GL_RENDERBUFFER, depthStencil);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, kSize, kSize);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              depthStencil);

    glDrawBuffers(kAttachmentCount, drawBuffers);

    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::white);

    for (uint32_t i = 0; i < kAttachmentCount; ++i)
    {
        glClearBufferfv(GL_COLOR, i, clearValues[i].toNormalizedVector().data());
    }

    glClearBufferfv(GL_DEPTH, 0, &kDepthClearValue);
    glClearBufferiv(GL_STENCIL, 0, &kStencilClearValue);
    ASSERT_GL_NO_ERROR();

    for (uint32_t i = 0; i < kAttachmentCount; ++i)
    {
        glReadBuffer(GL_COLOR_ATTACHMENT0 + i);
        ASSERT_GL_NO_ERROR();

        const GLColor &expect = clearValues[i];

        EXPECT_PIXEL_COLOR_EQ(0, 0, expect);
        EXPECT_PIXEL_COLOR_EQ(0, kSize - 1, expect);
        EXPECT_PIXEL_COLOR_EQ(kSize - 1, 0, expect);
        EXPECT_PIXEL_COLOR_EQ(kSize - 1, kSize - 1, expect);
    }

    glReadBuffer(GL_COLOR_ATTACHMENT0);
    for (uint32_t i = 1; i < kAttachmentCount; ++i)
        drawBuffers[i] = GL_NONE;
    glDrawBuffers(kAttachmentCount, drawBuffers);

    verifyDepth(kDepthClearValue, kSize);
    verifyStencil(kStencilClearValue, kSize);
}

// Test that clearing multiple attachments in the presence of a color mask, scissor or both
// correctly clears all the attachments.
TEST_P(ClearTestES3, MaskedScissoredClearMultipleAttachments)
{
    constexpr uint32_t kSize            = 16;
    constexpr uint32_t kAttachmentCount = 2;
    std::vector<unsigned char> pixelData(kSize * kSize * 4, 255);

    glBindFramebuffer(GL_FRAMEBUFFER, mFBOs[0]);

    GLTexture textures[kAttachmentCount];
    GLenum drawBuffers[kAttachmentCount];

    for (uint32_t i = 0; i < kAttachmentCount; ++i)
    {
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     pixelData.data());
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, textures[i],
                               0);
        drawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;
    }

    glDrawBuffers(kAttachmentCount, drawBuffers);

    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::white);

    // Masked clear
    GLColor clearColorMasked(31, 63, 255, 191);
    angle::Vector4 clearColor = GLColor(31, 63, 127, 191).toNormalizedVector();

    glColorMask(GL_TRUE, GL_TRUE, GL_FALSE, GL_TRUE);
    glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();

    // All attachments should be cleared, with the blue channel untouched
    for (uint32_t i = 0; i < kAttachmentCount; ++i)
    {
        glReadBuffer(GL_COLOR_ATTACHMENT0 + i);
        ASSERT_GL_NO_ERROR();

        EXPECT_PIXEL_COLOR_EQ(0, 0, clearColorMasked);
        EXPECT_PIXEL_COLOR_EQ(0, kSize - 1, clearColorMasked);
        EXPECT_PIXEL_COLOR_EQ(kSize - 1, 0, clearColorMasked);
        EXPECT_PIXEL_COLOR_EQ(kSize - 1, kSize - 1, clearColorMasked);
        EXPECT_PIXEL_COLOR_EQ(kSize / 2, kSize / 2, clearColorMasked);
    }

    // Masked scissored clear
    GLColor clearColorMaskedScissored(63, 127, 255, 31);
    clearColor = GLColor(63, 127, 191, 31).toNormalizedVector();

    glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    glEnable(GL_SCISSOR_TEST);
    glScissor(kSize / 6, kSize / 6, kSize / 3, kSize / 3);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();

    // The corners should keep the previous value while the center is cleared, except its blue
    // channel.
    for (uint32_t i = 0; i < kAttachmentCount; ++i)
    {
        glReadBuffer(GL_COLOR_ATTACHMENT0 + i);
        ASSERT_GL_NO_ERROR();

        EXPECT_PIXEL_COLOR_EQ(0, 0, clearColorMasked);
        EXPECT_PIXEL_COLOR_EQ(0, kSize - 1, clearColorMasked);
        EXPECT_PIXEL_COLOR_EQ(kSize - 1, 0, clearColorMasked);
        EXPECT_PIXEL_COLOR_EQ(kSize - 1, kSize - 1, clearColorMasked);
        EXPECT_PIXEL_COLOR_EQ(kSize / 3, 2 * kSize / 3, clearColorMasked);
        EXPECT_PIXEL_COLOR_EQ(2 * kSize / 3, kSize / 3, clearColorMasked);
        EXPECT_PIXEL_COLOR_EQ(2 * kSize / 3, 2 * kSize / 3, clearColorMasked);

        EXPECT_PIXEL_COLOR_EQ(kSize / 3, kSize / 3, clearColorMaskedScissored);
    }

    // Scissored clear
    GLColor clearColorScissored(127, 191, 31, 63);
    clearColor = GLColor(127, 191, 31, 63).toNormalizedVector();

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();

    // The corners should keep the old value while all channels of the center are cleared.
    for (uint32_t i = 0; i < kAttachmentCount; ++i)
    {
        glReadBuffer(GL_COLOR_ATTACHMENT0 + i);
        ASSERT_GL_NO_ERROR();

        EXPECT_PIXEL_COLOR_EQ(0, 0, clearColorMasked);
        EXPECT_PIXEL_COLOR_EQ(0, kSize - 1, clearColorMasked);
        EXPECT_PIXEL_COLOR_EQ(kSize - 1, 0, clearColorMasked);
        EXPECT_PIXEL_COLOR_EQ(kSize - 1, kSize - 1, clearColorMasked);
        EXPECT_PIXEL_COLOR_EQ(kSize / 3, 2 * kSize / 3, clearColorMasked);
        EXPECT_PIXEL_COLOR_EQ(2 * kSize / 3, kSize / 3, clearColorMasked);
        EXPECT_PIXEL_COLOR_EQ(2 * kSize / 3, 2 * kSize / 3, clearColorMasked);

        EXPECT_PIXEL_COLOR_EQ(kSize / 3, kSize / 3, clearColorScissored);
    }
}

// Test clearing multiple attachments in the presence of an indexed color mask.
TEST_P(ClearTestES3, MaskedIndexedClearMultipleAttachments)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_draw_buffers_indexed"));

    constexpr uint32_t kSize            = 16;
    constexpr uint32_t kAttachmentCount = 4;
    std::vector<unsigned char> pixelData(kSize * kSize * 4, 255);

    glBindFramebuffer(GL_FRAMEBUFFER, mFBOs[0]);

    GLTexture textures[kAttachmentCount];
    GLenum drawBuffers[kAttachmentCount];

    for (uint32_t i = 0; i < kAttachmentCount; ++i)
    {
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     pixelData.data());
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, textures[i],
                               0);
        drawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;
    }

    glDrawBuffers(kAttachmentCount, drawBuffers);

    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::white);

    // Masked clear
    GLColor clearColorMasked(31, 63, 255, 191);
    angle::Vector4 clearColor = GLColor(31, 63, 127, 191).toNormalizedVector();

    // Block blue channel for all attachements
    glColorMask(GL_TRUE, GL_TRUE, GL_FALSE, GL_TRUE);

    // Unblock blue channel for attachments 0 and 1
    glColorMaskiOES(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glColorMaskiOES(1, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();

    // All attachments should be cleared, with the blue channel untouched for all attachments but 1.
    for (uint32_t i = 0; i < kAttachmentCount; ++i)
    {
        glReadBuffer(GL_COLOR_ATTACHMENT0 + i);
        ASSERT_GL_NO_ERROR();

        const GLColor attachmentColor = (i > 1) ? clearColorMasked : clearColor;
        EXPECT_PIXEL_COLOR_EQ(0, 0, attachmentColor);
        EXPECT_PIXEL_COLOR_EQ(0, kSize - 1, attachmentColor);
        EXPECT_PIXEL_COLOR_EQ(kSize - 1, 0, attachmentColor);
        EXPECT_PIXEL_COLOR_EQ(kSize - 1, kSize - 1, attachmentColor);
        EXPECT_PIXEL_COLOR_EQ(kSize / 2, kSize / 2, attachmentColor);
    }
}

// Test that clearing multiple attachments of different nature (float, int and uint) in the
// presence of a color mask works correctly.  In the Vulkan backend, this exercises clearWithDraw
// and the relevant internal shaders.
TEST_P(ClearTestES3, MaskedClearHeterogeneousAttachments)
{
    // http://anglebug.com/4855
    ANGLE_SKIP_TEST_IF(IsWindows() && IsIntel() && IsVulkan());

    // TODO(anglebug.com/5491)
    ANGLE_SKIP_TEST_IF(IsIOS() && IsOpenGLES());

    constexpr uint32_t kSize                              = 16;
    constexpr uint32_t kAttachmentCount                   = 3;
    constexpr float kDepthClearValue                      = 0.256f;
    constexpr int32_t kStencilClearValue                  = 0x1D;
    constexpr GLenum kAttachmentFormats[kAttachmentCount] = {
        GL_RGBA8,
        GL_RGBA8I,
        GL_RGBA8UI,
    };
    constexpr GLenum kDataFormats[kAttachmentCount] = {
        GL_RGBA,
        GL_RGBA_INTEGER,
        GL_RGBA_INTEGER,
    };
    constexpr GLenum kDataTypes[kAttachmentCount] = {
        GL_UNSIGNED_BYTE,
        GL_BYTE,
        GL_UNSIGNED_BYTE,
    };

    std::vector<unsigned char> pixelData(kSize * kSize * 4, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, mFBOs[0]);

    GLTexture textures[kAttachmentCount];
    GLRenderbuffer depthStencil;
    GLenum drawBuffers[kAttachmentCount];

    for (uint32_t i = 0; i < kAttachmentCount; ++i)
    {
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, kAttachmentFormats[i], kSize, kSize, 0, kDataFormats[i],
                     kDataTypes[i], pixelData.data());
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, textures[i],
                               0);
        drawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;
    }

    glBindRenderbuffer(GL_RENDERBUFFER, depthStencil);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, kSize, kSize);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              depthStencil);

    glDrawBuffers(kAttachmentCount, drawBuffers);

    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_EQ(0, 0, 0, 0, 0, 0);

    // Mask out red for all clears
    glColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_TRUE);

    glClearBufferfv(GL_DEPTH, 0, &kDepthClearValue);
    glClearBufferiv(GL_STENCIL, 0, &kStencilClearValue);

    GLColor clearValuef = {25, 50, 75, 100};
    glClearBufferfv(GL_COLOR, 0, clearValuef.toNormalizedVector().data());

    int clearValuei[4] = {10, -20, 30, -40};
    glClearBufferiv(GL_COLOR, 1, clearValuei);

    uint32_t clearValueui[4] = {50, 60, 70, 80};
    glClearBufferuiv(GL_COLOR, 2, clearValueui);

    ASSERT_GL_NO_ERROR();

    {
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        ASSERT_GL_NO_ERROR();

        GLColor expect = clearValuef;
        expect.R       = 0;

        EXPECT_PIXEL_COLOR_EQ(0, 0, expect);
        EXPECT_PIXEL_COLOR_EQ(0, kSize - 1, expect);
        EXPECT_PIXEL_COLOR_EQ(kSize - 1, 0, expect);
        EXPECT_PIXEL_COLOR_EQ(kSize - 1, kSize - 1, expect);
    }

    {
        glReadBuffer(GL_COLOR_ATTACHMENT1);
        ASSERT_GL_NO_ERROR();

        EXPECT_PIXEL_8I(0, 0, 0, clearValuei[1], clearValuei[2], clearValuei[3]);
        EXPECT_PIXEL_8I(0, kSize - 1, 0, clearValuei[1], clearValuei[2], clearValuei[3]);
        EXPECT_PIXEL_8I(kSize - 1, 0, 0, clearValuei[1], clearValuei[2], clearValuei[3]);
        EXPECT_PIXEL_8I(kSize - 1, kSize - 1, 0, clearValuei[1], clearValuei[2], clearValuei[3]);
    }

    {
        glReadBuffer(GL_COLOR_ATTACHMENT2);
        ASSERT_GL_NO_ERROR();

        EXPECT_PIXEL_8UI(0, 0, 0, clearValueui[1], clearValueui[2], clearValueui[3]);
        EXPECT_PIXEL_8UI(0, kSize - 1, 0, clearValueui[1], clearValueui[2], clearValueui[3]);
        EXPECT_PIXEL_8UI(kSize - 1, 0, 0, clearValueui[1], clearValueui[2], clearValueui[3]);
        EXPECT_PIXEL_8UI(kSize - 1, kSize - 1, 0, clearValueui[1], clearValueui[2],
                         clearValueui[3]);
    }

    glReadBuffer(GL_COLOR_ATTACHMENT0);
    for (uint32_t i = 1; i < kAttachmentCount; ++i)
        drawBuffers[i] = GL_NONE;
    glDrawBuffers(kAttachmentCount, drawBuffers);

    verifyDepth(kDepthClearValue, kSize);
    verifyStencil(kStencilClearValue, kSize);
}

// Test that clearing multiple attachments of different nature (float, int and uint) in the
// presence of a scissor test works correctly.  In the Vulkan backend, this exercises clearWithDraw
// and the relevant internal shaders.
TEST_P(ClearTestES3, ScissoredClearHeterogeneousAttachments)
{
    // http://anglebug.com/4855
    ANGLE_SKIP_TEST_IF(IsWindows() && IsIntel() && IsVulkan());

    // http://anglebug.com/5116
    ANGLE_SKIP_TEST_IF(IsWindows() && (IsOpenGL() || IsD3D11()) && IsAMD());

    // http://anglebug.com/5237
    ANGLE_SKIP_TEST_IF(IsWindows7() && IsD3D11() && IsNVIDIA());

    // TODO(anglebug.com/5491)
    ANGLE_SKIP_TEST_IF(IsIOS() && IsOpenGLES());

    constexpr uint32_t kSize                              = 16;
    constexpr uint32_t kHalfSize                          = kSize / 2;
    constexpr uint32_t kAttachmentCount                   = 3;
    constexpr float kDepthClearValue                      = 0.256f;
    constexpr int32_t kStencilClearValue                  = 0x1D;
    constexpr GLenum kAttachmentFormats[kAttachmentCount] = {
        GL_RGBA8,
        GL_RGBA8I,
        GL_RGBA8UI,
    };
    constexpr GLenum kDataFormats[kAttachmentCount] = {
        GL_RGBA,
        GL_RGBA_INTEGER,
        GL_RGBA_INTEGER,
    };
    constexpr GLenum kDataTypes[kAttachmentCount] = {
        GL_UNSIGNED_BYTE,
        GL_BYTE,
        GL_UNSIGNED_BYTE,
    };

    std::vector<unsigned char> pixelData(kSize * kSize * 4, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, mFBOs[0]);

    GLTexture textures[kAttachmentCount];
    GLRenderbuffer depthStencil;
    GLenum drawBuffers[kAttachmentCount];

    for (uint32_t i = 0; i < kAttachmentCount; ++i)
    {
        glBindTexture(GL_TEXTURE_2D, textures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, kAttachmentFormats[i], kSize, kSize, 0, kDataFormats[i],
                     kDataTypes[i], pixelData.data());
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, textures[i],
                               0);
        drawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;
    }

    glBindRenderbuffer(GL_RENDERBUFFER, depthStencil);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, kSize, kSize);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              depthStencil);

    glDrawBuffers(kAttachmentCount, drawBuffers);

    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_EQ(0, 0, 0, 0, 0, 0);

    // Enable scissor test
    glScissor(0, 0, kHalfSize, kHalfSize);
    glEnable(GL_SCISSOR_TEST);

    GLColor clearValuef         = {25, 50, 75, 100};
    angle::Vector4 clearValuefv = clearValuef.toNormalizedVector();

    glClearColor(clearValuefv.x(), clearValuefv.y(), clearValuefv.z(), clearValuefv.w());
    glClearDepthf(kDepthClearValue);

    // clear stencil.
    glClearBufferiv(GL_STENCIL, 0, &kStencilClearValue);

    // clear float color attachment & depth together
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // clear integer attachment.
    int clearValuei[4] = {10, -20, 30, -40};
    glClearBufferiv(GL_COLOR, 1, clearValuei);

    // clear unsigned integer attachment
    uint32_t clearValueui[4] = {50, 60, 70, 80};
    glClearBufferuiv(GL_COLOR, 2, clearValueui);

    ASSERT_GL_NO_ERROR();

    {
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        ASSERT_GL_NO_ERROR();

        GLColor expect = clearValuef;

        EXPECT_PIXEL_COLOR_EQ(0, 0, expect);
        EXPECT_PIXEL_COLOR_EQ(0, kHalfSize - 1, expect);
        EXPECT_PIXEL_COLOR_EQ(kHalfSize - 1, 0, expect);
        EXPECT_PIXEL_COLOR_EQ(kHalfSize - 1, kHalfSize - 1, expect);
        EXPECT_PIXEL_EQ(kHalfSize + 1, kHalfSize + 1, 0, 0, 0, 0);
    }

    {
        glReadBuffer(GL_COLOR_ATTACHMENT1);
        ASSERT_GL_NO_ERROR();

        EXPECT_PIXEL_8I(0, 0, clearValuei[0], clearValuei[1], clearValuei[2], clearValuei[3]);
        EXPECT_PIXEL_8I(0, kHalfSize - 1, clearValuei[0], clearValuei[1], clearValuei[2],
                        clearValuei[3]);
        EXPECT_PIXEL_8I(kHalfSize - 1, 0, clearValuei[0], clearValuei[1], clearValuei[2],
                        clearValuei[3]);
        EXPECT_PIXEL_8I(kHalfSize - 1, kHalfSize - 1, clearValuei[0], clearValuei[1],
                        clearValuei[2], clearValuei[3]);
        EXPECT_PIXEL_8I(kHalfSize + 1, kHalfSize + 1, 0, 0, 0, 0);
    }

    {
        glReadBuffer(GL_COLOR_ATTACHMENT2);
        ASSERT_GL_NO_ERROR();

        EXPECT_PIXEL_8UI(0, 0, clearValueui[0], clearValueui[1], clearValueui[2], clearValueui[3]);
        EXPECT_PIXEL_8UI(0, kHalfSize - 1, clearValueui[0], clearValueui[1], clearValueui[2],
                         clearValueui[3]);
        EXPECT_PIXEL_8UI(kHalfSize - 1, 0, clearValueui[0], clearValueui[1], clearValueui[2],
                         clearValueui[3]);
        EXPECT_PIXEL_8UI(kHalfSize - 1, kHalfSize - 1, clearValueui[0], clearValueui[1],
                         clearValueui[2], clearValueui[3]);
        EXPECT_PIXEL_8UI(kHalfSize + 1, kHalfSize + 1, 0, 0, 0, 0);
    }

    glReadBuffer(GL_COLOR_ATTACHMENT0);
    for (uint32_t i = 1; i < kAttachmentCount; ++i)
        drawBuffers[i] = GL_NONE;
    glDrawBuffers(kAttachmentCount, drawBuffers);

    verifyDepth(kDepthClearValue, kHalfSize);
    verifyStencil(kStencilClearValue, kHalfSize);
}

// This tests a bug where in a masked clear when calling "ClearBuffer", we would
// mistakenly clear every channel (including the masked-out ones)
TEST_P(ClearTestES3, MaskedClearBufferBug)
{
    unsigned char pixelData[] = {255, 255, 255, 255};

    glBindFramebuffer(GL_FRAMEBUFFER, mFBOs[0]);

    GLTexture textures[2];

    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelData);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[0], 0);

    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixelData);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, textures[1], 0);

    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_EQ(0, 0, 255, 255, 255, 255);

    float clearValue[]   = {0, 0.5f, 0.5f, 1.0f};
    GLenum drawBuffers[] = {GL_NONE, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, drawBuffers);
    glColorMask(GL_TRUE, GL_TRUE, GL_FALSE, GL_TRUE);
    glClearBufferfv(GL_COLOR, 1, clearValue);

    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_EQ(0, 0, 255, 255, 255, 255);

    glReadBuffer(GL_COLOR_ATTACHMENT1);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_NEAR(0, 0, 0, 127, 255, 255, 1);
}

TEST_P(ClearTestES3, BadFBOSerialBug)
{
    // First make a simple framebuffer, and clear it to green
    glBindFramebuffer(GL_FRAMEBUFFER, mFBOs[0]);

    GLTexture textures[2];

    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, getWindowWidth(), getWindowHeight());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[0], 0);

    GLenum drawBuffers[] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, drawBuffers);

    float clearValues1[] = {0.0f, 1.0f, 0.0f, 1.0f};
    glClearBufferfv(GL_COLOR, 0, clearValues1);

    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    // Next make a second framebuffer, and draw it to red
    // (Triggers bad applied render target serial)
    GLFramebuffer fbo2;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo2);
    ASSERT_GL_NO_ERROR();

    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, getWindowWidth(), getWindowHeight());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[1], 0);

    glDrawBuffers(1, drawBuffers);

    ANGLE_GL_PROGRAM(blueProgram, essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());
    drawQuad(blueProgram, essl1_shaders::PositionAttrib(), 0.5f);

    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    // Check that the first framebuffer is still green.
    glBindFramebuffer(GL_FRAMEBUFFER, mFBOs[0]);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Test that SRGB framebuffers clear to the linearized clear color
TEST_P(ClearTestES3, SRGBClear)
{
    // First make a simple framebuffer, and clear it
    glBindFramebuffer(GL_FRAMEBUFFER, mFBOs[0]);

    GLTexture texture;

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_SRGB8_ALPHA8, getWindowWidth(), getWindowHeight());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    glClearColor(0.5f, 0.5f, 0.5f, 0.5f);
    glClear(GL_COLOR_BUFFER_BIT);

    EXPECT_PIXEL_NEAR(0, 0, 188, 188, 188, 128, 1.0);
}

// Test that framebuffers with mixed SRGB/Linear attachments clear to the correct color for each
// attachment
TEST_P(ClearTestES3, MixedSRGBClear)
{
    glBindFramebuffer(GL_FRAMEBUFFER, mFBOs[0]);

    GLTexture textures[2];

    glBindTexture(GL_TEXTURE_2D, textures[0]);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_SRGB8_ALPHA8, getWindowWidth(), getWindowHeight());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[0], 0);

    glBindTexture(GL_TEXTURE_2D, textures[1]);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, getWindowWidth(), getWindowHeight());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, textures[1], 0);

    GLenum drawBuffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, drawBuffers);

    // Clear both textures
    glClearColor(0.5f, 0.5f, 0.5f, 0.5f);
    glClear(GL_COLOR_BUFFER_BIT);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, 0, 0);

    // Check value of texture0
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[0], 0);
    EXPECT_PIXEL_NEAR(0, 0, 188, 188, 188, 128, 1.0);

    // Check value of texture1
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[1], 0);
    EXPECT_PIXEL_NEAR(0, 0, 128, 128, 128, 128, 1.0);
}

// This test covers a D3D11 bug where calling ClearRenderTargetView sometimes wouldn't sync
// before a draw call. The test draws small quads to a larger FBO (the default back buffer).
// Before each blit to the back buffer it clears the quad to a certain color using
// ClearBufferfv to give a solid color. The sync problem goes away if we insert a call to
// flush or finish after ClearBufferfv or each draw.
TEST_P(ClearTestES3, RepeatedClear)
{
    // Fails on 431.02 driver. http://anglebug.com/3748
    ANGLE_SKIP_TEST_IF(IsWindows() && IsNVIDIA() && IsVulkan());
    ANGLE_SKIP_TEST_IF(IsARM64() && IsWindows() && IsD3D());

    constexpr char kVS[] =
        "#version 300 es\n"
        "in highp vec2 position;\n"
        "out highp vec2 v_coord;\n"
        "void main(void)\n"
        "{\n"
        "    gl_Position = vec4(position, 0, 1);\n"
        "    vec2 texCoord = (position * 0.5) + 0.5;\n"
        "    v_coord = texCoord;\n"
        "}\n";

    constexpr char kFS[] =
        "#version 300 es\n"
        "in highp vec2 v_coord;\n"
        "out highp vec4 color;\n"
        "uniform sampler2D tex;\n"
        "void main()\n"
        "{\n"
        "    color = texture(tex, v_coord);\n"
        "}\n";

    ANGLE_GL_PROGRAM(program, kVS, kFS);

    mTextures.resize(1, 0);
    glGenTextures(1, mTextures.data());

    GLenum format           = GL_RGBA8;
    const int numRowsCols   = 3;
    const int cellSize      = 32;
    const int fboSize       = cellSize;
    const int backFBOSize   = cellSize * numRowsCols;
    const float fmtValueMin = 0.0f;
    const float fmtValueMax = 1.0f;

    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexStorage2D(GL_TEXTURE_2D, 1, format, fboSize, fboSize);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ASSERT_GL_NO_ERROR();

    glBindFramebuffer(GL_FRAMEBUFFER, mFBOs[0]);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTextures[0], 0);
    ASSERT_GL_NO_ERROR();

    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));

    // larger fbo bound -- clear to transparent black
    glUseProgram(program);
    GLint uniLoc = glGetUniformLocation(program, "tex");
    ASSERT_NE(-1, uniLoc);
    glUniform1i(uniLoc, 0);
    glBindTexture(GL_TEXTURE_2D, mTextures[0]);

    GLint positionLocation = glGetAttribLocation(program, "position");
    ASSERT_NE(-1, positionLocation);

    glUseProgram(program);

    for (int cellY = 0; cellY < numRowsCols; cellY++)
    {
        for (int cellX = 0; cellX < numRowsCols; cellX++)
        {
            int seed            = cellX + cellY * numRowsCols;
            const Vector4 color = RandomVec4(seed, fmtValueMin, fmtValueMax);

            glBindFramebuffer(GL_FRAMEBUFFER, mFBOs[0]);
            glClearBufferfv(GL_COLOR, 0, color.data());

            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            // Method 1: Set viewport and draw full-viewport quad
            glViewport(cellX * cellSize, cellY * cellSize, cellSize, cellSize);
            drawQuad(program, "position", 0.5f);

            // Uncommenting the glFinish call seems to make the test pass.
            // glFinish();
        }
    }

    std::vector<GLColor> pixelData(backFBOSize * backFBOSize);
    glReadPixels(0, 0, backFBOSize, backFBOSize, GL_RGBA, GL_UNSIGNED_BYTE, pixelData.data());

    for (int cellY = 0; cellY < numRowsCols; cellY++)
    {
        for (int cellX = 0; cellX < numRowsCols; cellX++)
        {
            int seed            = cellX + cellY * numRowsCols;
            const Vector4 color = RandomVec4(seed, fmtValueMin, fmtValueMax);
            GLColor expectedColor(color);

            int testN = cellX * cellSize + cellY * backFBOSize * cellSize + backFBOSize + 1;
            GLColor actualColor = pixelData[testN];
            EXPECT_NEAR(expectedColor.R, actualColor.R, 1);
            EXPECT_NEAR(expectedColor.G, actualColor.G, 1);
            EXPECT_NEAR(expectedColor.B, actualColor.B, 1);
            EXPECT_NEAR(expectedColor.A, actualColor.A, 1);
        }
    }

    ASSERT_GL_NO_ERROR();
}

// Test that clearing RGB8 attachments work when verified through sampling.
TEST_P(ClearTestES3, ClearRGB8)
{
    GLFramebuffer fb;
    glBindFramebuffer(GL_FRAMEBUFFER, fb);

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGB8, 1, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    // Clear the texture through framebuffer.
    const GLubyte kClearColor[] = {63, 127, 191, 55};
    glClearColor(kClearColor[0] / 255.0f, kClearColor[1] / 255.0f, kClearColor[2] / 255.0f,
                 kClearColor[3] / 255.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Sample from it and verify clear is done.
    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Texture2DLod(), essl3_shaders::fs::Texture2DLod());
    glUseProgram(program);
    GLint textureLocation = glGetUniformLocation(program, essl3_shaders::Texture2DUniform());
    ASSERT_NE(-1, textureLocation);
    GLint lodLocation = glGetUniformLocation(program, essl3_shaders::LodUniform());
    ASSERT_NE(-1, lodLocation);

    glUniform1i(textureLocation, 0);
    glUniform1f(lodLocation, 0);

    drawQuad(program, essl3_shaders::PositionAttrib(), 0.5f);

    EXPECT_PIXEL_NEAR(0, 0, kClearColor[0], kClearColor[1], kClearColor[2], 255, 1);
    ASSERT_GL_NO_ERROR();
}

// Test that clearing RGB8 attachments from a 2D texture array does not cause
// VUID-VkImageMemoryBarrier-oldLayout-01197
TEST_P(ClearTestES3, TextureArrayRGB8)
{
    GLFramebuffer fb;
    glBindFramebuffer(GL_FRAMEBUFFER, fb);

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGB8, 1, 1, 2);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0, 0);
    glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, tex, 0, 1);

    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    GLenum bufs[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, &bufs[0]);

    glClearColor(1.0, 0.0, 1.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb);

    glReadBuffer(GL_COLOR_ATTACHMENT0);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::magenta);

    glReadBuffer(GL_COLOR_ATTACHMENT1);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::magenta);

    EXPECT_GL_NO_ERROR();
}

void MaskedScissoredClearTestBase::maskedScissoredColorDepthStencilClear(
    const MaskedScissoredClearVariationsTestParams &params)
{
    // Flaky on Android Nexus 5x and Pixel 2, possible Qualcomm driver bug.
    // TODO(jmadill): Re-enable when possible. http://anglebug.com/2548
    ANGLE_SKIP_TEST_IF(IsOpenGLES() && IsAndroid());

    const int w      = getWindowWidth();
    const int h      = getWindowHeight();
    const int wthird = w / 3;
    const int hthird = h / 3;

    constexpr float kPreClearDepth     = 0.9f;
    constexpr float kClearDepth        = 0.5f;
    constexpr uint8_t kPreClearStencil = 0xFF;
    constexpr uint8_t kClearStencil    = 0x16;
    constexpr uint8_t kStencilMask     = 0x59;
    constexpr uint8_t kMaskedClearStencil =
        (kPreClearStencil & ~kStencilMask) | (kClearStencil & kStencilMask);

    bool clearColor, clearDepth, clearStencil;
    bool maskColor, maskDepth, maskStencil;
    bool scissor;

    ParseMaskedScissoredClearVariationsTestParams(params, &clearColor, &clearDepth, &clearStencil,
                                                  &maskColor, &maskDepth, &maskStencil, &scissor);

    // Clear to a random color, 0.9 depth and 0x00 stencil
    Vector4 color1(0.1f, 0.2f, 0.3f, 0.4f);
    GLColor color1RGB(color1);

    glClearColor(color1[0], color1[1], color1[2], color1[3]);
    glClearDepthf(kPreClearDepth);
    glClearStencil(kPreClearStencil);

    if (!clearColor)
    {
        // If not asked to clear color, clear it anyway, but individually.  The clear value is
        // still used to verify that the depth/stencil clear happened correctly.  This allows
        // testing for depth/stencil-only clear implementations.
        glClear(GL_COLOR_BUFFER_BIT);
    }

    glClear((clearColor ? GL_COLOR_BUFFER_BIT : 0) | (clearDepth ? GL_DEPTH_BUFFER_BIT : 0) |
            (clearStencil ? GL_STENCIL_BUFFER_BIT : 0));
    ASSERT_GL_NO_ERROR();

    // Verify color was cleared correctly.
    EXPECT_PIXEL_COLOR_NEAR(0, 0, color1RGB, 1);

    if (scissor)
    {
        glEnable(GL_SCISSOR_TEST);
        glScissor(wthird / 2, hthird / 2, wthird, hthird);
    }

    // Use color and stencil masks to clear to a second color, 0.5 depth and 0x59 stencil.
    Vector4 color2(0.2f, 0.4f, 0.6f, 0.8f);
    GLColor color2RGB(color2);
    glClearColor(color2[0], color2[1], color2[2], color2[3]);
    glClearDepthf(kClearDepth);
    glClearStencil(kClearStencil);
    if (maskColor)
    {
        glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_FALSE);
    }
    if (maskDepth)
    {
        glDepthMask(GL_FALSE);
    }
    if (maskStencil)
    {
        glStencilMask(kStencilMask);
    }
    glClear((clearColor ? GL_COLOR_BUFFER_BIT : 0) | (clearDepth ? GL_DEPTH_BUFFER_BIT : 0) |
            (clearStencil ? GL_STENCIL_BUFFER_BIT : 0));
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glStencilMask(0xFF);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    ASSERT_GL_NO_ERROR();

    GLColor color2MaskedRGB(color2RGB[0], color1RGB[1], color2RGB[2], color1RGB[3]);

    // If not clearing color, the original color should be left both in the center and corners.  If
    // using a scissor, the corners should be left to the original color, while the center is
    // possibly changed.  If using a mask, the center (and corners if not scissored), changes to
    // the masked results.
    GLColor expectedCenterColorRGB = !clearColor ? color1RGB
                                     : maskColor ? color2MaskedRGB
                                                 : color2RGB;
    GLColor expectedCornerColorRGB = scissor ? color1RGB : expectedCenterColorRGB;

    // Verify second clear color mask worked as expected.
    EXPECT_PIXEL_COLOR_NEAR(wthird, hthird, expectedCenterColorRGB, 1);

    EXPECT_PIXEL_COLOR_NEAR(0, 0, expectedCornerColorRGB, 1);
    EXPECT_PIXEL_COLOR_NEAR(w - 1, 0, expectedCornerColorRGB, 1);
    EXPECT_PIXEL_COLOR_NEAR(0, h - 1, expectedCornerColorRGB, 1);
    EXPECT_PIXEL_COLOR_NEAR(w - 1, h - 1, expectedCornerColorRGB, 1);
    EXPECT_PIXEL_COLOR_NEAR(wthird, 2 * hthird, expectedCornerColorRGB, 1);
    EXPECT_PIXEL_COLOR_NEAR(2 * wthird, hthird, expectedCornerColorRGB, 1);
    EXPECT_PIXEL_COLOR_NEAR(2 * wthird, 2 * hthird, expectedCornerColorRGB, 1);

    // If there is depth, but depth is not asked to be cleared, the depth buffer contains garbage,
    // so no particular behavior can be expected.
    if (clearDepth || !mHasDepth)
    {
        // We use a small shader to verify depth.
        ANGLE_GL_PROGRAM(depthTestProgram, essl1_shaders::vs::Passthrough(),
                         essl1_shaders::fs::Blue());
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(maskDepth ? GL_GREATER : GL_EQUAL);
        // - If depth is cleared, but it's masked, kPreClearDepth should be in the depth buffer.
        // - If depth is cleared, but it's not masked, kClearDepth should be in the depth buffer.
        // - If depth is not cleared, the if above ensures there is no depth buffer at all,
        //   which means depth test will always pass.
        drawQuad(depthTestProgram, essl1_shaders::PositionAttrib(), maskDepth ? 1.0f : 0.0f);
        glDisable(GL_DEPTH_TEST);
        ASSERT_GL_NO_ERROR();

        // Either way, we expect blue to be written to the center.
        expectedCenterColorRGB = GLColor::blue;
        // If there is no depth, depth test always passes so the whole image must be blue.  Same if
        // depth write is masked.
        expectedCornerColorRGB =
            mHasDepth && scissor && !maskDepth ? expectedCornerColorRGB : GLColor::blue;

        EXPECT_PIXEL_COLOR_NEAR(wthird, hthird, expectedCenterColorRGB, 1);

        EXPECT_PIXEL_COLOR_NEAR(0, 0, expectedCornerColorRGB, 1);
        EXPECT_PIXEL_COLOR_NEAR(w - 1, 0, expectedCornerColorRGB, 1);
        EXPECT_PIXEL_COLOR_NEAR(0, h - 1, expectedCornerColorRGB, 1);
        EXPECT_PIXEL_COLOR_NEAR(w - 1, h - 1, expectedCornerColorRGB, 1);
        EXPECT_PIXEL_COLOR_NEAR(wthird, 2 * hthird, expectedCornerColorRGB, 1);
        EXPECT_PIXEL_COLOR_NEAR(2 * wthird, hthird, expectedCornerColorRGB, 1);
        EXPECT_PIXEL_COLOR_NEAR(2 * wthird, 2 * hthird, expectedCornerColorRGB, 1);
    }

    // If there is stencil, but it's not asked to be cleared, there is similarly no expectation.
    if (clearStencil || !mHasStencil)
    {
        // And another small shader to verify stencil.
        ANGLE_GL_PROGRAM(stencilTestProgram, essl1_shaders::vs::Passthrough(),
                         essl1_shaders::fs::Green());
        glEnable(GL_STENCIL_TEST);
        // - If stencil is cleared, but it's masked, kMaskedClearStencil should be in the stencil
        //   buffer.
        // - If stencil is cleared, but it's not masked, kClearStencil should be in the stencil
        //   buffer.
        // - If stencil is not cleared, the if above ensures there is no stencil buffer at all,
        //   which means stencil test will always pass.
        glStencilFunc(GL_EQUAL, maskStencil ? kMaskedClearStencil : kClearStencil, 0xFF);
        drawQuad(stencilTestProgram, essl1_shaders::PositionAttrib(), 0.0f);
        glDisable(GL_STENCIL_TEST);
        ASSERT_GL_NO_ERROR();

        // Either way, we expect green to be written to the center.
        expectedCenterColorRGB = GLColor::green;
        // If there is no stencil, stencil test always passes so the whole image must be green.
        expectedCornerColorRGB = mHasStencil && scissor ? expectedCornerColorRGB : GLColor::green;

        EXPECT_PIXEL_COLOR_NEAR(wthird, hthird, expectedCenterColorRGB, 1);

        EXPECT_PIXEL_COLOR_NEAR(0, 0, expectedCornerColorRGB, 1);
        EXPECT_PIXEL_COLOR_NEAR(w - 1, 0, expectedCornerColorRGB, 1);
        EXPECT_PIXEL_COLOR_NEAR(0, h - 1, expectedCornerColorRGB, 1);
        EXPECT_PIXEL_COLOR_NEAR(w - 1, h - 1, expectedCornerColorRGB, 1);
        EXPECT_PIXEL_COLOR_NEAR(wthird, 2 * hthird, expectedCornerColorRGB, 1);
        EXPECT_PIXEL_COLOR_NEAR(2 * wthird, hthird, expectedCornerColorRGB, 1);
        EXPECT_PIXEL_COLOR_NEAR(2 * wthird, 2 * hthird, expectedCornerColorRGB, 1);
    }
}

// Tests combinations of color, depth, stencil clears with or without masks or scissor.
TEST_P(MaskedScissoredClearTest, Test)
{
    maskedScissoredColorDepthStencilClear(GetParam());
}

// Tests combinations of color, depth, stencil clears with or without masks or scissor.
//
// This uses depth/stencil attachments that are single-channel, but are emulated with a format
// that has both channels.
TEST_P(VulkanClearTest, Test)
{
    bool clearColor, clearDepth, clearStencil;
    bool maskColor, maskDepth, maskStencil;
    bool scissor;

    ParseMaskedScissoredClearVariationsTestParams(GetParam(), &clearColor, &clearDepth,
                                                  &clearStencil, &maskColor, &maskDepth,
                                                  &maskStencil, &scissor);

    // We only care about clearing depth xor stencil.
    if (clearDepth == clearStencil)
    {
        return;
    }

    if (clearDepth)
    {
        // Creating a depth-only renderbuffer is an ES3 feature.
        ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);
        bindColorDepthFBO();
    }
    else
    {
        bindColorStencilFBO();
    }

    maskedScissoredColorDepthStencilClear(GetParam());
}

// Tests that clearing a non existing attachment works.
TEST_P(ClearTest, ClearColorThenClearNonExistingDepthStencil)
{
    constexpr GLsizei kSize = 16;

    GLTexture color;
    glBindTexture(GL_TEXTURE_2D, color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
    ASSERT_GL_NO_ERROR();
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    // Clear color.
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Clear depth/stencil.
    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();

    // Read back color.
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
}

// Tests that clearing a non existing attachment works.
TEST_P(ClearTestES3, ClearDepthStencilThenClearNonExistingColor)
{
    constexpr GLsizei kSize = 16;

    GLRenderbuffer depth;
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, kSize, kSize);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depth);
    ASSERT_GL_NO_ERROR();
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    // Clear depth/stencil.
    glClearDepthf(1.0f);
    glClearStencil(0xAA);
    glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();

    // Clear color.
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_GL_NO_ERROR();
}

// Test that just clearing a nonexistent drawbuffer of the default framebuffer doesn't cause an
// assert.
TEST_P(ClearTestES3, ClearBuffer1OnDefaultFramebufferNoAssert)
{
    std::vector<GLuint> testUint(4);
    glClearBufferuiv(GL_COLOR, 1, testUint.data());
    std::vector<GLint> testInt(4);
    glClearBufferiv(GL_COLOR, 1, testInt.data());
    std::vector<GLfloat> testFloat(4);
    glClearBufferfv(GL_COLOR, 1, testFloat.data());
    EXPECT_GL_NO_ERROR();
}

// Clears many small concentric rectangles using scissor regions.
TEST_P(ClearTest, InceptionScissorClears)
{
    angle::RNG rng;

    constexpr GLuint kSize = 16;

    // Create a square user FBO so we have more control over the dimensions.
    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLRenderbuffer rbo;
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, kSize, kSize);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo);
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    glViewport(0, 0, kSize, kSize);

    // Draw small concentric squares using scissor.
    std::vector<GLColor> expectedColors;
    for (GLuint index = 0; index < (kSize - 1) / 2; index++)
    {
        // Do the first clear without the scissor.
        if (index > 0)
        {
            glEnable(GL_SCISSOR_TEST);
            glScissor(index, index, kSize - (index * 2), kSize - (index * 2));
        }

        GLColor color = RandomColor(&rng);
        expectedColors.push_back(color);
        Vector4 floatColor = color.toNormalizedVector();
        glClearColor(floatColor[0], floatColor[1], floatColor[2], floatColor[3]);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    ASSERT_GL_NO_ERROR();

    std::vector<GLColor> actualColors(expectedColors.size());
    glReadPixels(0, kSize / 2, actualColors.size(), 1, GL_RGBA, GL_UNSIGNED_BYTE,
                 actualColors.data());

    EXPECT_EQ(expectedColors, actualColors);
}

// Clears many small concentric rectangles using scissor regions.
TEST_P(ClearTest, DrawThenInceptionScissorClears)
{
    angle::RNG rng;

    constexpr GLuint kSize = 16;

    // Create a square user FBO so we have more control over the dimensions.
    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    GLRenderbuffer rbo;
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, kSize, kSize);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo);
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    glViewport(0, 0, kSize, kSize);

    ANGLE_GL_PROGRAM(redProgram, essl1_shaders::vs::Simple(), essl1_shaders::fs::Red());
    drawQuad(redProgram, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    // Draw small concentric squares using scissor.
    std::vector<GLColor> expectedColors;
    for (GLuint index = 0; index < (kSize - 1) / 2; index++)
    {
        // Do the first clear without the scissor.
        if (index > 0)
        {
            glEnable(GL_SCISSOR_TEST);
            glScissor(index, index, kSize - (index * 2), kSize - (index * 2));
        }

        GLColor color = RandomColor(&rng);
        expectedColors.push_back(color);
        Vector4 floatColor = color.toNormalizedVector();
        glClearColor(floatColor[0], floatColor[1], floatColor[2], floatColor[3]);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    ASSERT_GL_NO_ERROR();

    std::vector<GLColor> actualColors(expectedColors.size());
    glReadPixels(0, kSize / 2, actualColors.size(), 1, GL_RGBA, GL_UNSIGNED_BYTE,
                 actualColors.data());

    EXPECT_EQ(expectedColors, actualColors);
}

// Test that clearBuffer with disabled non-zero drawbuffer or disabled read source doesn't cause an
// assert.
TEST_P(ClearTestES3, ClearDisabledNonZeroAttachmentNoAssert)
{
    // http://anglebug.com/4612
    ANGLE_SKIP_TEST_IF(IsOSX() && IsDesktopOpenGL());

    GLFramebuffer fb;
    glBindFramebuffer(GL_FRAMEBUFFER, fb);

    GLRenderbuffer rb;
    glBindRenderbuffer(GL_RENDERBUFFER, rb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, 16, 16);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, rb);
    glDrawBuffers(0, nullptr);
    glReadBuffer(GL_NONE);

    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    float clearColorf[4] = {0.5, 0.5, 0.5, 0.5};
    glClearBufferfv(GL_COLOR, 1, clearColorf);

    GLuint clearColorui[4] = {255, 255, 255, 255};
    glClearBufferuiv(GL_COLOR, 1, clearColorui);

    GLint clearColori[4] = {-127, -127, -127, -127};
    glClearBufferiv(GL_COLOR, 1, clearColori);

    EXPECT_GL_NO_ERROR();
}

// Test that having a framebuffer with maximum number of attachments and clearing color, depth and
// stencil works.
TEST_P(ClearTestES3, ClearMaxAttachments)
{
    // http://anglebug.com/4612
    ANGLE_SKIP_TEST_IF(IsOSX() && IsDesktopOpenGL());
    // http://anglebug.com/5397
    ANGLE_SKIP_TEST_IF(IsAMD() && IsD3D11());

    constexpr GLsizei kSize = 16;

    GLint maxDrawBuffers = 0;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
    ASSERT_GE(maxDrawBuffers, 4);

    // Setup framebuffer.
    GLFramebuffer fb;
    glBindFramebuffer(GL_FRAMEBUFFER, fb);

    std::vector<GLRenderbuffer> color(maxDrawBuffers);
    std::vector<GLenum> drawBuffers(maxDrawBuffers);

    for (GLint colorIndex = 0; colorIndex < maxDrawBuffers; ++colorIndex)
    {
        glBindRenderbuffer(GL_RENDERBUFFER, color[colorIndex]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, kSize, kSize);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + colorIndex,
                                  GL_RENDERBUFFER, color[colorIndex]);

        drawBuffers[colorIndex] = GL_COLOR_ATTACHMENT0 + colorIndex;
    }

    GLRenderbuffer depthStencil;
    glBindRenderbuffer(GL_RENDERBUFFER, depthStencil);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, kSize, kSize);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              depthStencil);

    EXPECT_GL_NO_ERROR();
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    glDrawBuffers(maxDrawBuffers, drawBuffers.data());

    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClearDepthf(1.0f);
    glClearStencil(0x55);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    EXPECT_GL_NO_ERROR();

    // Verify that every color attachment is cleared correctly.
    for (GLint colorIndex = 0; colorIndex < maxDrawBuffers; ++colorIndex)
    {
        glReadBuffer(GL_COLOR_ATTACHMENT0 + colorIndex);

        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
        EXPECT_PIXEL_COLOR_EQ(0, kSize - 1, GLColor::red);
        EXPECT_PIXEL_COLOR_EQ(kSize - 1, 0, GLColor::red);
        EXPECT_PIXEL_COLOR_EQ(kSize - 1, kSize - 1, GLColor::red);
    }

    // Verify that depth and stencil attachments are cleared correctly.
    GLFramebuffer fbVerify;
    glBindFramebuffer(GL_FRAMEBUFFER, fbVerify);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color[0]);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              depthStencil);

    // If depth is not cleared to 1, rendering would fail.
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // If stencil is not cleared to 0x55, rendering would fail.
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_EQUAL, 0x55, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glStencilMask(0xFF);

    // Draw green.
    ANGLE_GL_PROGRAM(drawGreen, essl1_shaders::vs::Simple(), essl1_shaders::fs::Green());
    drawQuad(drawGreen, essl1_shaders::PositionAttrib(), 0.95f);

    // Verify that green was drawn.
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(0, kSize - 1, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(kSize - 1, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(kSize - 1, kSize - 1, GLColor::green);
}

// Test that having a framebuffer with maximum number of attachments and clearing color, depth and
// stencil after a draw call works.
TEST_P(ClearTestES3, ClearMaxAttachmentsAfterDraw)
{
    // http://anglebug.com/4612
    ANGLE_SKIP_TEST_IF(IsOSX() && IsDesktopOpenGL());

    constexpr GLsizei kSize = 16;

    GLint maxDrawBuffers = 0;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
    ASSERT_GE(maxDrawBuffers, 4);

    // Setup framebuffer.
    GLFramebuffer fb;
    glBindFramebuffer(GL_FRAMEBUFFER, fb);

    std::vector<GLRenderbuffer> color(maxDrawBuffers);
    std::vector<GLenum> drawBuffers(maxDrawBuffers);

    for (GLint colorIndex = 0; colorIndex < maxDrawBuffers; ++colorIndex)
    {
        glBindRenderbuffer(GL_RENDERBUFFER, color[colorIndex]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, kSize, kSize);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + colorIndex,
                                  GL_RENDERBUFFER, color[colorIndex]);

        drawBuffers[colorIndex] = GL_COLOR_ATTACHMENT0 + colorIndex;
    }

    GLRenderbuffer depthStencil;
    glBindRenderbuffer(GL_RENDERBUFFER, depthStencil);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, kSize, kSize);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              depthStencil);

    EXPECT_GL_NO_ERROR();
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    glDrawBuffers(maxDrawBuffers, drawBuffers.data());

    // Issue a draw call to render blue, depth=0 and stencil 0x3C to the attachments.
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_ALWAYS);

    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 0x3C, 0xFF);
    glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
    glStencilMask(0xFF);

    // Generate shader for this framebuffer.
    std::stringstream strstr;
    strstr << "#version 300 es\n"
              "precision highp float;\n";
    for (GLint colorIndex = 0; colorIndex < maxDrawBuffers; ++colorIndex)
    {
        strstr << "layout(location = " << colorIndex << ") out vec4 value" << colorIndex << ";\n";
    }
    strstr << "void main()\n"
              "{\n";
    for (GLint colorIndex = 0; colorIndex < maxDrawBuffers; ++colorIndex)
    {
        strstr << "value" << colorIndex << " = vec4(0.0f, 0.0f, 1.0f, 1.0f);\n";
    }
    strstr << "}\n";

    ANGLE_GL_PROGRAM(drawMRT, essl3_shaders::vs::Simple(), strstr.str().c_str());
    drawQuad(drawMRT, essl3_shaders::PositionAttrib(), 0.0f);
    ASSERT_GL_NO_ERROR();

    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClearDepthf(1.0f);
    glClearStencil(0x55);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    EXPECT_GL_NO_ERROR();

    // Verify that every color attachment is cleared correctly.
    for (GLint colorIndex = 0; colorIndex < maxDrawBuffers; ++colorIndex)
    {
        glReadBuffer(GL_COLOR_ATTACHMENT0 + colorIndex);

        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red) << colorIndex;
        EXPECT_PIXEL_COLOR_EQ(0, kSize - 1, GLColor::red) << colorIndex;
        EXPECT_PIXEL_COLOR_EQ(kSize - 1, 0, GLColor::red) << colorIndex;
        EXPECT_PIXEL_COLOR_EQ(kSize - 1, kSize - 1, GLColor::red) << colorIndex;
    }

    // Verify that depth and stencil attachments are cleared correctly.
    GLFramebuffer fbVerify;
    glBindFramebuffer(GL_FRAMEBUFFER, fbVerify);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color[0]);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              depthStencil);

    // If depth is not cleared to 1, rendering would fail.
    glDepthFunc(GL_LESS);

    // If stencil is not cleared to 0x55, rendering would fail.
    glStencilFunc(GL_EQUAL, 0x55, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    // Draw green.
    ANGLE_GL_PROGRAM(drawGreen, essl1_shaders::vs::Simple(), essl1_shaders::fs::Green());
    drawQuad(drawGreen, essl1_shaders::PositionAttrib(), 0.95f);

    // Verify that green was drawn.
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(0, kSize - 1, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(kSize - 1, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(kSize - 1, kSize - 1, GLColor::green);
}

// Test that mixed masked clear works after clear.
TEST_P(ClearTestES3, ClearThenMixedMaskedClear)
{
    constexpr GLsizei kSize = 16;

    // Setup framebuffer.
    GLRenderbuffer color;
    glBindRenderbuffer(GL_RENDERBUFFER, color);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, kSize, kSize);

    GLRenderbuffer depthStencil;
    glBindRenderbuffer(GL_RENDERBUFFER, depthStencil);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, kSize, kSize);

    GLFramebuffer fb;
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              depthStencil);
    EXPECT_GL_NO_ERROR();
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    // Clear color and depth/stencil
    glClearColor(0.1f, 1.0f, 0.0f, 0.7f);
    glClearDepthf(0.0f);
    glClearStencil(0x55);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Clear again, but with color and stencil masked
    glClearColor(1.0f, 0.2f, 0.6f, 1.0f);
    glClearDepthf(1.0f);
    glClearStencil(0x3C);
    glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_TRUE);
    glStencilMask(0xF0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // Issue a draw call to verify color, depth and stencil.

    // If depth is not cleared to 1, rendering would fail.
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);

    // If stencil is not cleared to 0x35, rendering would fail.
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_EQUAL, 0x35, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glStencilMask(0xFF);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    ANGLE_GL_PROGRAM(drawColor, essl1_shaders::vs::Simple(), essl1_shaders::fs::UniformColor());
    glUseProgram(drawColor);
    GLint colorUniformLocation =
        glGetUniformLocation(drawColor, angle::essl1_shaders::ColorUniform());
    ASSERT_NE(colorUniformLocation, -1);

    // Blend half-transparent blue into the color buffer.
    glUniform4f(colorUniformLocation, 0.0f, 0.0f, 1.0f, 0.5f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    drawQuad(drawColor, essl1_shaders::PositionAttrib(), 0.95f);
    ASSERT_GL_NO_ERROR();

    // Verify that the color buffer is now gray
    const GLColor kExpected(127, 127, 127, 191);
    EXPECT_PIXEL_COLOR_NEAR(0, 0, kExpected, 1);
    EXPECT_PIXEL_COLOR_NEAR(0, kSize - 1, kExpected, 1);
    EXPECT_PIXEL_COLOR_NEAR(kSize - 1, 0, kExpected, 1);
    EXPECT_PIXEL_COLOR_NEAR(kSize - 1, kSize - 1, kExpected, 1);
}

// Test that clearing stencil after a draw call works.
TEST_P(ClearTestES3, ClearStencilAfterDraw)
{
    // http://anglebug.com/4612
    ANGLE_SKIP_TEST_IF(IsOSX() && IsDesktopOpenGL());

    constexpr GLsizei kSize = 16;

    GLint maxDrawBuffers = 0;
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
    ASSERT_GE(maxDrawBuffers, 4);

    // Setup framebuffer.
    GLFramebuffer fb;
    glBindFramebuffer(GL_FRAMEBUFFER, fb);

    std::vector<GLRenderbuffer> color(maxDrawBuffers);
    std::vector<GLenum> drawBuffers(maxDrawBuffers);

    for (GLint colorIndex = 0; colorIndex < maxDrawBuffers; ++colorIndex)
    {
        glBindRenderbuffer(GL_RENDERBUFFER, color[colorIndex]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, kSize, kSize);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + colorIndex,
                                  GL_RENDERBUFFER, color[colorIndex]);

        drawBuffers[colorIndex] = GL_COLOR_ATTACHMENT0 + colorIndex;
    }

    GLRenderbuffer depthStencil;
    glBindRenderbuffer(GL_RENDERBUFFER, depthStencil);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, kSize, kSize);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              depthStencil);

    EXPECT_GL_NO_ERROR();
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    glDrawBuffers(maxDrawBuffers, drawBuffers.data());

    // Issue a draw call to render blue and stencil 0x3C to the attachments.
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 0x3C, 0xFF);
    glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
    glStencilMask(0xFF);

    // Generate shader for this framebuffer.
    std::stringstream strstr;
    strstr << "#version 300 es\n"
              "precision highp float;\n";
    for (GLint colorIndex = 0; colorIndex < maxDrawBuffers; ++colorIndex)
    {
        strstr << "layout(location = " << colorIndex << ") out vec4 value" << colorIndex << ";\n";
    }
    strstr << "void main()\n"
              "{\n";
    for (GLint colorIndex = 0; colorIndex < maxDrawBuffers; ++colorIndex)
    {
        strstr << "value" << colorIndex << " = vec4(0.0f, 0.0f, 1.0f, 1.0f);\n";
    }
    strstr << "}\n";

    ANGLE_GL_PROGRAM(drawMRT, essl3_shaders::vs::Simple(), strstr.str().c_str());
    drawQuad(drawMRT, essl3_shaders::PositionAttrib(), 0.0f);
    ASSERT_GL_NO_ERROR();

    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClearStencil(0x55);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    EXPECT_GL_NO_ERROR();

    // Verify that every color attachment is cleared correctly.
    for (GLint colorIndex = 0; colorIndex < maxDrawBuffers; ++colorIndex)
    {
        glReadBuffer(GL_COLOR_ATTACHMENT0 + colorIndex);

        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
        EXPECT_PIXEL_COLOR_EQ(0, kSize - 1, GLColor::red);
        EXPECT_PIXEL_COLOR_EQ(kSize - 1, 0, GLColor::red);
        EXPECT_PIXEL_COLOR_EQ(kSize - 1, kSize - 1, GLColor::red);
    }

    // Verify that depth and stencil attachments are cleared correctly.
    GLFramebuffer fbVerify;
    glBindFramebuffer(GL_FRAMEBUFFER, fbVerify);

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color[0]);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                              depthStencil);

    // If stencil is not cleared to 0x55, rendering would fail.
    glStencilFunc(GL_EQUAL, 0x55, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);

    // Draw green.
    ANGLE_GL_PROGRAM(drawGreen, essl1_shaders::vs::Simple(), essl1_shaders::fs::Green());
    drawQuad(drawGreen, essl1_shaders::PositionAttrib(), 0.95f);

    // Verify that green was drawn.
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(0, kSize - 1, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(kSize - 1, 0, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(kSize - 1, kSize - 1, GLColor::green);
}

// Test that mid-render pass clearing of mixed used and unused color attachments works.
TEST_P(ClearTestES3, MixedRenderPassClearMixedUsedUnusedAttachments)
{
    // http://anglebug.com/4612
    ANGLE_SKIP_TEST_IF(IsOSX() && IsDesktopOpenGL());

    constexpr GLsizei kSize = 16;

    // Setup framebuffer.
    GLFramebuffer fb;
    glBindFramebuffer(GL_FRAMEBUFFER, fb);

    GLRenderbuffer color[2];

    for (GLint colorIndex = 0; colorIndex < 2; ++colorIndex)
    {
        glBindRenderbuffer(GL_RENDERBUFFER, color[colorIndex]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, kSize, kSize);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + colorIndex,
                                  GL_RENDERBUFFER, color[colorIndex]);
    }
    EXPECT_GL_NO_ERROR();
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    // Disable color attachment 0.
    GLenum drawBuffers[] = {GL_NONE, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, drawBuffers);

    // Draw into color attachment 1
    constexpr char kFS[] = R"(#version 300 es
precision highp float;
layout(location = 0) out vec4 color0;
layout(location = 1) out vec4 color1;
void main()
{
    color0 = vec4(0, 0, 1, 1);
    color1 = vec4(1, 0, 0, 1);
})";

    ANGLE_GL_PROGRAM(drawMRT, essl3_shaders::vs::Simple(), kFS);
    drawQuad(drawMRT, essl3_shaders::PositionAttrib(), 0.0f);
    ASSERT_GL_NO_ERROR();

    // Color attachment 0 is now uninitialized, while color attachment 1 is red.
    // Re-enable color attachment 0 and clear both attachments to green.
    drawBuffers[0] = GL_COLOR_ATTACHMENT0;
    glDrawBuffers(2, drawBuffers);

    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_GL_NO_ERROR();

    // Verify that both color attachments are now green.
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    glReadBuffer(GL_COLOR_ATTACHMENT1);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
    EXPECT_GL_NO_ERROR();
}

// Test that draw without state change after masked clear works
TEST_P(ClearTestES3, DrawClearThenDrawWithoutStateChange)
{
    swapBuffers();
    constexpr GLsizei kSize = 16;

    // Setup framebuffer.
    GLRenderbuffer color;
    glBindRenderbuffer(GL_RENDERBUFFER, color);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, kSize, kSize);

    GLFramebuffer fb;
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
    EXPECT_GL_NO_ERROR();
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    // Clear color initially.
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Mask color.
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);

    ANGLE_GL_PROGRAM(drawColor, essl1_shaders::vs::Simple(), essl1_shaders::fs::UniformColor());
    glUseProgram(drawColor);
    GLint colorUniformLocation =
        glGetUniformLocation(drawColor, angle::essl1_shaders::ColorUniform());
    ASSERT_NE(colorUniformLocation, -1);

    // Initialize position attribute.
    GLint posLoc = glGetAttribLocation(drawColor, essl1_shaders::PositionAttrib());
    ASSERT_NE(-1, posLoc);
    setupQuadVertexBuffer(0.5f, 1.0f);
    glVertexAttribPointer(posLoc, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(posLoc);

    // Draw red.
    glViewport(0, 0, kSize, kSize);
    glClearColor(0.0f, 1.0f, 0.0f, 0.0f);
    glUniform4f(colorUniformLocation, 1.0f, 0.0f, 0.0f, 0.5f);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Clear to green without any state change.
    glClear(GL_COLOR_BUFFER_BIT);

    // Draw red again without any state change.
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Verify that the color buffer is now red
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(0, kSize - 1, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(kSize - 1, 0, GLColor::red);
    EXPECT_PIXEL_COLOR_EQ(kSize - 1, kSize - 1, GLColor::red);
}

// Test that clear stencil value is correctly masked to 8 bits.
TEST_P(ClearTest, ClearStencilMask)
{
    GLint stencilBits = 0;
    glGetIntegerv(GL_STENCIL_BITS, &stencilBits);
    EXPECT_EQ(stencilBits, 8);

    ANGLE_GL_PROGRAM(drawColor, essl1_shaders::vs::Simple(), essl1_shaders::fs::Green());
    glUseProgram(drawColor);

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::black);

    // Clear stencil value must be masked to 0x42
    glClearStencil(0x142);
    glClear(GL_STENCIL_BUFFER_BIT);

    // Check that the stencil test works as expected
    glEnable(GL_STENCIL_TEST);

    // Negative case
    glStencilFunc(GL_NOTEQUAL, 0x42, 0xFF);
    drawQuad(drawColor, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::black);

    // Positive case
    glStencilFunc(GL_EQUAL, 0x42, 0xFF);
    drawQuad(drawColor, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    ASSERT_GL_NO_ERROR();
}

// Test that glClearBufferiv correctly masks the clear stencil value.
TEST_P(ClearTestES3, ClearBufferivStencilMask)
{
    GLint stencilBits = 0;
    glGetIntegerv(GL_STENCIL_BITS, &stencilBits);
    EXPECT_EQ(stencilBits, 8);

    ANGLE_GL_PROGRAM(drawColor, essl1_shaders::vs::Simple(), essl1_shaders::fs::Green());
    glUseProgram(drawColor);

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::black);

    // Clear stencil value must be masked to 0x42
    const GLint kStencilClearValue = 0x142;
    glClearBufferiv(GL_STENCIL, 0, &kStencilClearValue);

    // Check that the stencil test works as expected
    glEnable(GL_STENCIL_TEST);

    // Negative case
    glStencilFunc(GL_NOTEQUAL, 0x42, 0xFF);
    drawQuad(drawColor, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::black);

    // Positive case
    glStencilFunc(GL_EQUAL, 0x42, 0xFF);
    drawQuad(drawColor, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    ASSERT_GL_NO_ERROR();
}

// Test that glClearBufferfi correctly masks the clear stencil value.
TEST_P(ClearTestES3, ClearBufferfiStencilMask)
{
    GLint stencilBits = 0;
    glGetIntegerv(GL_STENCIL_BITS, &stencilBits);
    EXPECT_EQ(stencilBits, 8);

    ANGLE_GL_PROGRAM(drawColor, essl1_shaders::vs::Simple(), essl1_shaders::fs::Green());
    glUseProgram(drawColor);

    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::black);

    // Clear stencil value must be masked to 0x42
    glClearBufferfi(GL_DEPTH_STENCIL, 0, 0.5f, 0x142);

    // Check that the stencil test works as expected
    glEnable(GL_STENCIL_TEST);

    // Negative case
    glStencilFunc(GL_NOTEQUAL, 0x42, 0xFF);
    drawQuad(drawColor, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::black);

    // Positive case
    glStencilFunc(GL_EQUAL, 0x42, 0xFF);
    drawQuad(drawColor, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    ASSERT_GL_NO_ERROR();
}

// Test that glClearBufferfi works when stencil attachment is not present.
TEST_P(ClearTestES3, ClearBufferfiNoStencilAttachment)
{
    constexpr GLsizei kSize = 16;

    GLRenderbuffer color;
    glBindRenderbuffer(GL_RENDERBUFFER, color);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, kSize, kSize);

    GLRenderbuffer depth;
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, kSize, kSize);

    GLFramebuffer fbo;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    EXPECT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);
    EXPECT_GL_NO_ERROR();

    // Clear depth/stencil with glClearBufferfi.  Note that the stencil attachment doesn't exist.
    glClearBufferfi(GL_DEPTH_STENCIL, 0, 0.5f, 0x55);
    EXPECT_GL_NO_ERROR();

    // Verify depth is cleared correctly.
    verifyDepth(0.5f, kSize);
}

// Test that scissored clear followed by non-scissored draw works.  Ensures that when scissor size
// is expanded, the clear operation remains limited to the scissor region.  Written to catch
// potential future bugs if loadOp=CLEAR is used in the Vulkan backend for a small render pass and
// then the render area is mistakenly enlarged.
TEST_P(ClearTest, ScissoredClearThenNonScissoredDraw)
{
    constexpr GLsizei kSize = 16;
    const std::vector<GLColor> kInitialData(kSize * kSize, GLColor::red);

    // Setup framebuffer.  Initialize color with red.
    GLTexture color;
    glBindTexture(GL_TEXTURE_2D, color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 kInitialData.data());

    GLFramebuffer fb;
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
    EXPECT_GL_NO_ERROR();
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    // Issue a scissored clear to green.
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glScissor(kSize / 2, 0, kSize / 2, kSize);
    glEnable(GL_SCISSOR_TEST);
    glClear(GL_COLOR_BUFFER_BIT);

    // Expand the scissor and blend blue into the framebuffer.
    glScissor(0, 0, kSize, kSize);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);

    ANGLE_GL_PROGRAM(drawBlue, essl1_shaders::vs::Simple(), essl1_shaders::fs::Blue());
    drawQuad(drawBlue, essl1_shaders::PositionAttrib(), 0.5f);
    ASSERT_GL_NO_ERROR();

    // Verify that the left half is magenta, and the right half is cyan.
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::magenta);
    EXPECT_PIXEL_COLOR_EQ(kSize / 2 - 1, 0, GLColor::magenta);
    EXPECT_PIXEL_COLOR_EQ(0, kSize - 1, GLColor::magenta);
    EXPECT_PIXEL_COLOR_EQ(kSize / 2 - 1, kSize - 1, GLColor::magenta);

    EXPECT_PIXEL_COLOR_EQ(kSize / 2, 0, GLColor::cyan);
    EXPECT_PIXEL_COLOR_EQ(kSize / 2, kSize - 1, GLColor::cyan);
    EXPECT_PIXEL_COLOR_EQ(kSize - 1, 0, GLColor::cyan);
    EXPECT_PIXEL_COLOR_EQ(kSize - 1, kSize - 1, GLColor::cyan);
}

// Test that clear followed by a scissored masked clear works.
TEST_P(ClearTest, ClearThenScissoredMaskedClear)
{
    constexpr GLsizei kSize = 16;

    // Setup framebuffer
    GLTexture color;
    glBindTexture(GL_TEXTURE_2D, color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kSize, kSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    GLFramebuffer fb;
    glBindFramebuffer(GL_FRAMEBUFFER, fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
    EXPECT_GL_NO_ERROR();
    ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

    // Clear to red.
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Mask red and clear to green with a scissor
    glColorMask(GL_FALSE, GL_TRUE, GL_TRUE, GL_TRUE);
    glScissor(0, 0, kSize / 2, kSize);
    glEnable(GL_SCISSOR_TEST);
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Verify that the left half is yellow, and the right half is red.
    EXPECT_PIXEL_RECT_EQ(0, 0, kSize / 2, kSize, GLColor::yellow);
    EXPECT_PIXEL_RECT_EQ(kSize / 2, 0, kSize / 2, kSize, GLColor::red);
}

// Test that a scissored stencil clear followed by a full clear works.
TEST_P(ClearTestES3, StencilScissoredClearThenFullClear)
{
    constexpr GLsizei kSize = 128;

    GLint stencilBits = 0;
    glGetIntegerv(GL_STENCIL_BITS, &stencilBits);
    EXPECT_EQ(stencilBits, 8);

    // Clear stencil value must be masked to 0x42
    glClearBufferfi(GL_DEPTH_STENCIL, 0, 0.5f, 0x142);

    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    // Shrink the render area.
    glScissor(kSize / 2, 0, kSize / 2, kSize);
    glEnable(GL_SCISSOR_TEST);

    // Clear stencil.
    glClearBufferfi(GL_DEPTH_STENCIL, 0, 0.5f, 0x64);

    // Grow the render area.
    glScissor(0, 0, kSize, kSize);
    glEnable(GL_SCISSOR_TEST);

    // Check that the stencil test works as expected
    glEnable(GL_STENCIL_TEST);

    // Scissored region is green, outside is red (clear color)
    glStencilFunc(GL_EQUAL, 0x64, 0xFF);
    ANGLE_GL_PROGRAM(drawGreen, essl3_shaders::vs::Simple(), essl3_shaders::fs::Green());
    glUseProgram(drawGreen);
    drawQuad(drawGreen, essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_RECT_EQ(0, 0, kSize / 2, kSize, GLColor::red);
    EXPECT_PIXEL_RECT_EQ(kSize / 2, 0, kSize / 2, kSize, GLColor::green);

    // Outside scissored region is blue.
    glStencilFunc(GL_EQUAL, 0x42, 0xFF);
    ANGLE_GL_PROGRAM(drawBlue, essl3_shaders::vs::Simple(), essl3_shaders::fs::Blue());
    glUseProgram(drawBlue);
    drawQuad(drawBlue, essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_RECT_EQ(0, 0, kSize / 2, kSize, GLColor::blue);
    EXPECT_PIXEL_RECT_EQ(kSize / 2, 0, kSize / 2, kSize, GLColor::green);

    ASSERT_GL_NO_ERROR();
}

// This is a test that must be verified visually.
//
// Tests that clear of the default framebuffer applies to the window.
TEST_P(ClearTest, DISABLED_ClearReachesWindow)
{
    ANGLE_GL_PROGRAM(blueProgram, essl1_shaders::vs::Simple(), essl1_shaders::fs::Blue());

    // Draw blue.
    drawQuad(blueProgram, essl1_shaders::PositionAttrib(), 0.5f);
    swapBuffers();

    // Use glClear to clear to red.  Regression test for the Vulkan backend where this clear
    // remained "deferred" and didn't make it to the window on swap.
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    swapBuffers();

    // Wait for visual verification.
    angle::Sleep(2000);
}

// Test that clearing slices of a 3D texture and reading them back works.
TEST_P(ClearTestES3, ClearAndReadPixels3DTexture)
{
    constexpr uint32_t kWidth  = 128;
    constexpr uint32_t kHeight = 128;
    constexpr uint32_t kDepth  = 7;

    GLTexture texture;
    glBindTexture(GL_TEXTURE_3D, texture);
    glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA8, kWidth, kHeight, kDepth);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, 0);

    std::array<GLColor, kDepth> clearColors = {
        GLColor::red,  GLColor::green,   GLColor::blue,  GLColor::yellow,
        GLColor::cyan, GLColor::magenta, GLColor::white,
    };

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);

    for (uint32_t z = 0; z < kDepth; ++z)
    {
        glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture, 0, z);
        glClearBufferfv(GL_COLOR, 0, clearColors[z].toNormalizedVector().data());
    }

    for (uint32_t z = 0; z < kDepth; ++z)
    {
        glFramebufferTextureLayer(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture, 0, z);
        EXPECT_PIXEL_COLOR_EQ(0, 0, clearColors[z]);
    }
}

// Test that clearing stencil with zero first byte in mask doesn't crash.
TEST_P(ClearTestES3, ClearStencilZeroFirstByteMask)
{
    glStencilMask(0xe7d6a900);
    glClear(GL_STENCIL_BUFFER_BIT);
}

// Test that mid render pass clear after draw sets the render pass size correctly.
TEST_P(ClearTestES3, ScissoredDrawThenFullClear)
{
    const int w = getWindowWidth();
    const int h = getWindowHeight();

    // Use viewport to imply scissor on the draw call
    glViewport(w / 4, h / 4, w / 2, h / 2);

    ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Passthrough(), essl1_shaders::fs::Blue());
    drawQuad(program, essl1_shaders::PositionAttrib(), 0);

    // Mid-render-pass clear without scissor or viewport change, which covers the whole framebuffer.
    glClearColor(1, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    EXPECT_PIXEL_RECT_EQ(0, 0, w, h, GLColor::yellow);
}

// Test that mid render pass clear after masked clear sets the render pass size correctly.
TEST_P(ClearTestES3, MaskedScissoredClearThenFullClear)
{
    const int w = getWindowWidth();
    const int h = getWindowHeight();

    // Use viewport to imply a small scissor on (non-existing) draw calls.  This is important to
    // make sure render area that's derived from scissor+viewport for draw calls doesn't
    // accidentally fix render area derived from scissor for clear calls.
    glViewport(w / 2, h / 2, 1, 1);

    glEnable(GL_SCISSOR_TEST);
    glScissor(w / 4, h / 4, w / 2, h / 2);
    glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_FALSE);
    glClearColor(0.13, 0.38, 0.87, 0.65);
    glClear(GL_COLOR_BUFFER_BIT);

    // Mid-render-pass clear without scissor, which covers the whole framebuffer.
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(1, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    EXPECT_PIXEL_RECT_EQ(0, 0, w, h, GLColor::yellow);
}

// Test that mid render pass masked clear after masked clear sets the render pass size correctly.
TEST_P(ClearTestES3, MaskedScissoredClearThenFullMaskedClear)
{
    const int w = getWindowWidth();
    const int h = getWindowHeight();

    // Make sure the framebuffer is initialized.
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::black);

    // Use viewport to imply a small scissor on (non-existing) draw calls  This is important to
    // make sure render area that's derived from scissor+viewport for draw calls doesn't
    // accidentally fix render area derived from scissor for clear calls.
    glViewport(w / 2, h / 2, 1, 1);

    glEnable(GL_SCISSOR_TEST);
    glScissor(w / 4, h / 4, w / 2, h / 2);
    glColorMask(GL_TRUE, GL_FALSE, GL_TRUE, GL_FALSE);
    glClearColor(1, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    // Mid-render-pass clear without scissor, which covers the whole framebuffer.
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE);
    glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);

    EXPECT_PIXEL_RECT_EQ(0, 0, w / 4, h, GLColor::green);
    EXPECT_PIXEL_RECT_EQ(w / 4, 0, w / 2, h / 4, GLColor::green);
    EXPECT_PIXEL_RECT_EQ(w / 4, 3 * h / 4, w / 2, h / 4, GLColor::green);
    EXPECT_PIXEL_RECT_EQ(3 * w / 4, 0, w / 4, h, GLColor::green);

    EXPECT_PIXEL_RECT_EQ(w / 4, h / 4, w / 2, h / 2, GLColor::yellow);
}

#ifdef Bool
// X11 craziness.
#    undef Bool
#endif

ANGLE_INSTANTIATE_TEST_ES2_AND_ES3_AND(
    ClearTest,
    ES3_VULKAN().enable(Feature::PreferDrawClearOverVkCmdClearAttachments));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ClearTestES3);
ANGLE_INSTANTIATE_TEST_ES3_AND(
    ClearTestES3,
    ES3_VULKAN().enable(Feature::PreferDrawClearOverVkCmdClearAttachments));

ANGLE_INSTANTIATE_TEST_COMBINE_4(MaskedScissoredClearTest,
                                 MaskedScissoredClearVariationsTestPrint,
                                 testing::Range(0, 3),
                                 testing::Range(0, 3),
                                 testing::Range(0, 3),
                                 testing::Bool(),
                                 ANGLE_ALL_TEST_PLATFORMS_ES2,
                                 ANGLE_ALL_TEST_PLATFORMS_ES3,
                                 ES3_VULKAN()
                                     .disable(Feature::SupportsExtendedDynamicState)
                                     .disable(Feature::SupportsExtendedDynamicState2)
                                     .disable(Feature::SupportsLogicOpDynamicState),
                                 ES3_VULKAN()
                                     .disable(Feature::SupportsExtendedDynamicState2)
                                     .disable(Feature::SupportsLogicOpDynamicState));

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(VulkanClearTest);
ANGLE_INSTANTIATE_TEST_COMBINE_4(VulkanClearTest,
                                 MaskedScissoredClearVariationsTestPrint,
                                 testing::Range(0, 3),
                                 testing::Range(0, 3),
                                 testing::Range(0, 3),
                                 testing::Bool(),
                                 ES2_VULKAN().enable(Feature::ForceFallbackFormat),
                                 ES2_VULKAN_SWIFTSHADER().enable(Feature::ForceFallbackFormat),
                                 ES3_VULKAN().enable(Feature::ForceFallbackFormat),
                                 ES3_VULKAN_SWIFTSHADER().enable(Feature::ForceFallbackFormat));

// Not all ANGLE backends support RGB backbuffers
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ClearTestRGB);
ANGLE_INSTANTIATE_TEST(ClearTestRGB,
                       ES2_D3D11(),
                       ES3_D3D11(),
                       ES2_VULKAN(),
                       ES3_VULKAN(),
                       ES2_METAL(),
                       ES3_METAL());

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ClearTestRGB_ES3);
ANGLE_INSTANTIATE_TEST(ClearTestRGB_ES3, ES3_D3D11(), ES3_VULKAN(), ES3_METAL());

}  // anonymous namespace
