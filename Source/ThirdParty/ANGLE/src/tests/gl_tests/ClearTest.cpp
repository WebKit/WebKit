//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"

#include "platform/FeaturesVk.h"
#include "test_utils/gl_raii.h"
#include "util/random_utils.h"
#include "util/shader_utils.h"

using namespace angle;

namespace
{

Vector4 RandomVec4(int seed, float minValue, float maxValue)
{
    RNG rng(seed);
    srand(seed);
    return Vector4(
        rng.randomFloatBetween(minValue, maxValue), rng.randomFloatBetween(minValue, maxValue),
        rng.randomFloatBetween(minValue, maxValue), rng.randomFloatBetween(minValue, maxValue));
}

GLColor Vec4ToColor(const Vector4 &vec)
{
    GLColor color;
    color.R = static_cast<uint8_t>(vec.x() * 255.0f);
    color.G = static_cast<uint8_t>(vec.y() * 255.0f);
    color.B = static_cast<uint8_t>(vec.z() * 255.0f);
    color.A = static_cast<uint8_t>(vec.w() * 255.0f);
    return color;
}

class ClearTestBase : public ANGLETest
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

class ClearTestRGB : public ANGLETest
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

class MaskedScissoredClearTestBase
    : public ANGLETestWithParam<MaskedScissoredClearVariationsTestParams>
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

    void MaskedScissoredColorDepthStencilClear(
        const MaskedScissoredClearVariationsTestParams &params);

    bool mHasDepth   = true;
    bool mHasStencil = true;
};

class MaskedScissoredClearTest : public MaskedScissoredClearTestBase
{};

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

    // Override a feature to force emulation of stencil-only and depth-only formats with a packed
    // depth/stencil format
    void overrideFeaturesVk(FeaturesVk *featuresVk) override
    {
        featuresVk->overrideFeatures({"force_fallback_format"}, true);
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
    glClearColor(0.25f, 0.5f, 0.5f, 0.5f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_NEAR(0, 0, 64, 128, 128, 255, 1.0);
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
    ANGLE_SKIP_TEST_IF(IsD3D9() || IsD3D11() || (IsOzone() && IsOpenGLES()));
    ANGLE_SKIP_TEST_IF(IsOSX() && (IsNVIDIA() || IsIntel()) && IsDesktopOpenGL());
    ANGLE_SKIP_TEST_IF(IsAndroid() && IsAdreno() && IsOpenGLES());

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

// Test that clearing all buffers through glClearColor followed by a clear of a specific buffer
// clears to the correct values.
TEST_P(ClearTestES3, ClearMultipleAttachmentsFollowedBySpecificOne)
{
    // http://anglebug.com/4092
    ANGLE_SKIP_TEST_IF(isSwiftshader());
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

// Test that clearing multiple attachments of different nature (float, int and uint) in the
// presence of a color mask works correctly.  In the Vulkan backend, this exercises clearWithDraw
// and the relevant internal shaders.
TEST_P(ClearTestES3, MaskedClearHeterogeneousAttachments)
{
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

// This tests a bug where in a masked clear when calling "ClearBuffer", we would
// mistakenly clear every channel (including the masked-out ones)
TEST_P(ClearTestES3, MaskedClearBufferBug)
{
    // TODO(syoussefi): Qualcomm driver crashes in the presence of VK_ATTACHMENT_UNUSED.
    // http://anglebug.com/3423
    ANGLE_SKIP_TEST_IF(IsVulkan() && IsAndroid());

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
            int seed              = cellX + cellY * numRowsCols;
            const Vector4 color   = RandomVec4(seed, fmtValueMin, fmtValueMax);
            GLColor expectedColor = Vec4ToColor(color);

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

void MaskedScissoredClearTestBase::MaskedScissoredColorDepthStencilClear(
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

    // clearDepth && !maskDepth fails on Intel Ubuntu 19.04 Mesa 19.0.2 GL. http://anglebug.com/3614
    ANGLE_SKIP_TEST_IF(IsLinux() && IsIntel() && IsDesktopOpenGL() && clearDepth && !maskDepth);

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
    GLColor expectedCenterColorRGB =
        !clearColor ? color1RGB : maskColor ? color2MaskedRGB : color2RGB;
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
    MaskedScissoredColorDepthStencilClear(GetParam());
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

    MaskedScissoredColorDepthStencilClear(GetParam());
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

#ifdef Bool
// X11 craziness.
#    undef Bool
#endif

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(ClearTest);
ANGLE_INSTANTIATE_TEST_ES3(ClearTestES3);
ANGLE_INSTANTIATE_TEST_COMBINE_4(MaskedScissoredClearTest,
                                 MaskedScissoredClearVariationsTestPrint,
                                 testing::Range(0, 3),
                                 testing::Range(0, 3),
                                 testing::Range(0, 3),
                                 testing::Bool(),
                                 ES2_D3D9(),
                                 ES2_D3D11(),
                                 ES3_D3D11(),
                                 ES2_OPENGL(),
                                 ES3_OPENGL(),
                                 ES2_OPENGLES(),
                                 ES3_OPENGLES(),
                                 ES2_VULKAN(),
                                 ES3_VULKAN());
ANGLE_INSTANTIATE_TEST_COMBINE_4(VulkanClearTest,
                                 MaskedScissoredClearVariationsTestPrint,
                                 testing::Range(0, 3),
                                 testing::Range(0, 3),
                                 testing::Range(0, 3),
                                 testing::Bool(),
                                 ES2_VULKAN(),
                                 ES3_VULKAN());

// Not all ANGLE backends support RGB backbuffers
ANGLE_INSTANTIATE_TEST(ClearTestRGB, ES2_D3D11(), ES3_D3D11(), ES2_VULKAN(), ES3_VULKAN());

}  // anonymous namespace
