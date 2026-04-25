//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SRGBFramebufferTest.cpp: Tests of sRGB framebuffer functionality.

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

namespace
{
constexpr angle::GLColor linearColor(64, 127, 191, 255);
constexpr angle::GLColor srgbColor(13, 54, 133, 255);
}  // namespace

namespace angle
{

class SRGBFramebufferTest : public ANGLETest<>
{
  protected:
    SRGBFramebufferTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void testSetUp() override
    {
        mProgram = CompileProgram(essl1_shaders::vs::Simple(), essl1_shaders::fs::UniformColor());
        ASSERT_NE(0u, mProgram);

        mColorLocation = glGetUniformLocation(mProgram, essl1_shaders::ColorUniform());
        ASSERT_NE(-1, mColorLocation);
    }

    void testTearDown() override { glDeleteProgram(mProgram); }

    GLuint mProgram      = 0;
    GLint mColorLocation = -1;
};

class SRGBFramebufferTestES3 : public SRGBFramebufferTest
{};

// Test basic validation of GL_EXT_sRGB_write_control
TEST_P(SRGBFramebufferTest, Validation)
{
    GLenum expectedError =
        IsGLExtensionEnabled("GL_EXT_sRGB_write_control") ? GL_NO_ERROR : GL_INVALID_ENUM;

    GLboolean value = GL_FALSE;
    glEnable(GL_FRAMEBUFFER_SRGB_EXT);
    EXPECT_GL_ERROR(expectedError);

    glGetBooleanv(GL_FRAMEBUFFER_SRGB_EXT, &value);
    EXPECT_GL_ERROR(expectedError);
    if (expectedError == GL_NO_ERROR)
    {
        EXPECT_GL_TRUE(value);
    }

    glDisable(GL_FRAMEBUFFER_SRGB_EXT);
    EXPECT_GL_ERROR(expectedError);

    glGetBooleanv(GL_FRAMEBUFFER_SRGB_EXT, &value);
    EXPECT_GL_ERROR(expectedError);
    if (expectedError == GL_NO_ERROR)
    {
        EXPECT_GL_FALSE(value);
    }
}

// Test basic functionality of GL_EXT_sRGB_write_control
TEST_P(SRGBFramebufferTest, BasicUsage)
{
    if (!IsGLExtensionEnabled("GL_EXT_sRGB_write_control") ||
        (!IsGLExtensionEnabled("GL_EXT_sRGB") && getClientMajorVersion() < 3))
    {
        std::cout
            << "Test skipped because GL_EXT_sRGB_write_control and GL_EXT_sRGB are not available."
            << std::endl;
        return;
    }

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB_ALPHA_EXT, 1, 1, 0, GL_SRGB_ALPHA_EXT, GL_UNSIGNED_BYTE,
                 nullptr);

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    glUseProgram(mProgram);
    glUniform4fv(mColorLocation, 1, srgbColor.toNormalizedVector().data());

    glEnable(GL_FRAMEBUFFER_SRGB_EXT);
    drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_NEAR(0, 0, linearColor, 1.0);

    glDisable(GL_FRAMEBUFFER_SRGB_EXT);
    drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_NEAR(0, 0, srgbColor, 1.0);
}

// Test that GL_EXT_sRGB_write_control state applies to all framebuffers if multiple are used
// 1. disable srgb
// 2. draw to both framebuffers
// 3. enable srgb
// 4. draw to both framebuffers
TEST_P(SRGBFramebufferTest, MultipleFramebuffers)
{
    if (!IsGLExtensionEnabled("GL_EXT_sRGB_write_control") ||
        (!IsGLExtensionEnabled("GL_EXT_sRGB") && getClientMajorVersion() < 3))
    {
        std::cout
            << "Test skipped because GL_EXT_sRGB_write_control and GL_EXT_sRGB are not available."
            << std::endl;
        return;
    }

    // NVIDIA failures on older drivers
    // http://anglebug.com/42264177
    ANGLE_SKIP_TEST_IF(IsNVIDIA() && IsOpenGLES());

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB_ALPHA_EXT, 1, 1, 0, GL_SRGB_ALPHA_EXT, GL_UNSIGNED_BYTE,
                 nullptr);

    GLFramebuffer framebuffer1;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer1);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    glUseProgram(mProgram);
    glUniform4fv(mColorLocation, 1, srgbColor.toNormalizedVector().data());

    glDisable(GL_FRAMEBUFFER_SRGB_EXT);
    drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_NEAR(0, 0, srgbColor, 1.0);

    GLFramebuffer framebuffer2;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer2);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_NEAR(0, 0, srgbColor, 1.0);

    glEnable(GL_FRAMEBUFFER_SRGB_EXT);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer1);
    drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_NEAR(0, 0, linearColor, 1.0);

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer2);
    drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_NEAR(0, 0, linearColor, 1.0);
}

// Test that we behave correctly when we toggle FRAMEBUFFER_SRGB_EXT on a framebuffer that has an
// attachment in linear colorspace
TEST_P(SRGBFramebufferTest, NegativeAlreadyLinear)
{
    if (!IsGLExtensionEnabled("GL_EXT_sRGB_write_control") ||
        (!IsGLExtensionEnabled("GL_EXT_sRGB") && getClientMajorVersion() < 3))
    {
        std::cout
            << "Test skipped because GL_EXT_sRGB_write_control and GL_EXT_sRGB are not available."
            << std::endl;
        return;
    }

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    glUseProgram(mProgram);
    glUniform4fv(mColorLocation, 1, linearColor.toNormalizedVector().data());

    glEnable(GL_FRAMEBUFFER_SRGB_EXT);
    drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_NEAR(0, 0, linearColor, 1.0);

    glDisable(GL_FRAMEBUFFER_SRGB_EXT);
    drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_NEAR(0, 0, linearColor, 1.0);
}

// Test that lifetimes of internal resources are tracked correctly by deleting a texture and then
// attempting to use it. This is expected to produce a non-fatal error.
TEST_P(SRGBFramebufferTest, NegativeLifetimeTracking)
{
    if (!IsGLExtensionEnabled("GL_EXT_sRGB_write_control") ||
        (!IsGLExtensionEnabled("GL_EXT_sRGB") && getClientMajorVersion() < 3))
    {
        std::cout
            << "Test skipped because GL_EXT_sRGB_write_control and GL_EXT_sRGB are not available."
            << std::endl;
        return;
    }

    // NVIDIA failures
    // http://anglebug.com/42264177
    ANGLE_SKIP_TEST_IF(IsNVIDIA() && IsOpenGLES());

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB_ALPHA_EXT, 1, 1, 0, GL_SRGB_ALPHA_EXT, GL_UNSIGNED_BYTE,
                 nullptr);

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    glUseProgram(mProgram);
    glUniform4fv(mColorLocation, 1, srgbColor.toNormalizedVector().data());

    glDisable(GL_FRAMEBUFFER_SRGB_EXT);
    drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_NEAR(0, 0, srgbColor, 1.0);

    // Delete the texture
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    texture.reset();

    drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_GL_ERROR(GL_INVALID_FRAMEBUFFER_OPERATION);

    GLColor throwaway_color;
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &throwaway_color);
    EXPECT_GL_ERROR(GL_INVALID_FRAMEBUFFER_OPERATION);
}

// Test that glBlitFramebuffer correctly converts colorspaces
TEST_P(SRGBFramebufferTestES3, BlitFramebuffer)
{
    // http://anglebug.com/42264326
    ANGLE_SKIP_TEST_IF(!IsVulkan());

    if (!IsGLExtensionEnabled("GL_EXT_sRGB_write_control") ||
        (!IsGLExtensionEnabled("GL_EXT_sRGB") && getClientMajorVersion() < 3))
    {
        std::cout
            << "Test skipped because GL_EXT_sRGB_write_control and GL_EXT_sRGB are not available."
            << std::endl;
        return;
    }

    GLTexture dstTexture;
    glBindTexture(GL_TEXTURE_2D, dstTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB_ALPHA_EXT, 1, 1, 0, GL_SRGB_ALPHA_EXT, GL_UNSIGNED_BYTE,
                 nullptr);
    GLFramebuffer dstFramebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, dstFramebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dstTexture, 0);

    GLTexture srcTexture;
    glBindTexture(GL_TEXTURE_2D, srcTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB_ALPHA_EXT, 1, 1, 0, GL_SRGB_ALPHA_EXT, GL_UNSIGNED_BYTE,
                 nullptr);

    GLFramebuffer srcFramebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, srcFramebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, srcTexture, 0);

    glUseProgram(mProgram);
    glUniform4fv(mColorLocation, 1, srgbColor.toNormalizedVector().data());

    // Draw onto the framebuffer normally
    glEnable(GL_FRAMEBUFFER_SRGB_EXT);
    drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_NEAR(0, 0, linearColor, 1.0);

    // Blit the framebuffer normally
    glEnable(GL_FRAMEBUFFER_SRGB_EXT);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFramebuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFramebuffer);
    glBlitFramebuffer(0, 0, 1, 1, 0, 0, 1, 1, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, dstFramebuffer);
    EXPECT_PIXEL_COLOR_NEAR(0, 0, linearColor, 1.0);

    // Blit the framebuffer with forced linear colorspace
    glDisable(GL_FRAMEBUFFER_SRGB_EXT);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFramebuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFramebuffer);
    glBlitFramebuffer(0, 0, 1, 1, 0, 0, 1, 1, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, dstFramebuffer);
    EXPECT_PIXEL_COLOR_NEAR(0, 0, srgbColor, 1.0);
}

// This test reproduces an issue in the Vulkan backend found in the Chromium CI that
// was caused by enabling the VK_KHR_image_format_list extension on SwiftShader
// which exposed GL_EXT_sRGB_write_control.
TEST_P(SRGBFramebufferTest, DrawToSmallFBOClearLargeFBO)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_sRGB_write_control") ||
                       (!IsGLExtensionEnabled("GL_EXT_sRGB") && getClientMajorVersion() < 3));

    // Disabling GL_FRAMEBUFFER_SRGB_EXT caused the issue
    glDisable(GL_FRAMEBUFFER_SRGB_EXT);

    // The issue involved framebuffers of two different sizes.
    // The smaller needed to be drawn to, while the larger one could be just cleared
    // to reproduce the issue. These are the smallest tested sizes that generated
    // the validation error.
    constexpr GLsizei kDimensionsSmall[] = {1, 1};
    constexpr GLsizei kDimensionsLarge[] = {2, 2};
    {
        GLTexture texture;
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8, kDimensionsSmall[0], kDimensionsSmall[1]);
        glBindTexture(GL_TEXTURE_2D, 0);

        GLFramebuffer framebuffer;
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

        unsigned char vertexData[] = {0};
        GLBuffer vertexBuffer;
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, sizeof(char), vertexData, GL_STATIC_DRAW);

        unsigned int indexData[] = {0};
        GLBuffer indexBuffer;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(int), indexData, GL_STATIC_DRAW);

        glUseProgram(mProgram);

        glDrawElements(GL_POINTS, 1, GL_UNSIGNED_INT, nullptr);

        EXPECT_GL_NO_ERROR();
    }
    {
        GLTexture texture;
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexStorage2DEXT(GL_TEXTURE_2D, 1, GL_RGBA8, kDimensionsLarge[0], kDimensionsLarge[1]);
        glBindTexture(GL_TEXTURE_2D, 0);

        GLFramebuffer framebuffer;
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

        // Vulkan validation happened to fail here with:
        // "Cannot execute a render pass with renderArea not within the bound of the framebuffer"
        glClear(GL_COLOR_BUFFER_BIT);

        EXPECT_GL_NO_ERROR();
    }
}

class SRGBFramebufferDefaultLinearTest : public ANGLETest<>
{
  protected:
    SRGBFramebufferDefaultLinearTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void testBasic(bool isSrgb, bool isES3)
    {
        // Default framebuffer attachment queries require OpenGL ES 3.0
        if (isES3)
        {
            GLint encoding;
            glGetFramebufferAttachmentParameteriv(
                GL_FRAMEBUFFER, GL_BACK, GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, &encoding);
            ASSERT_GL_NO_ERROR();
            EXPECT_GLENUM_EQ(encoding, isSrgb ? GL_SRGB : GL_LINEAR);
        }

        glClearColor(0.5, 0.5, 0.5, 0.5);
        glClear(GL_COLOR_BUFFER_BIT);
        ASSERT_GL_NO_ERROR();

        if (isSrgb)
        {
            EXPECT_PIXEL_NEAR(0, 0, 188, 188, 188, 127, 1);
        }
        else
        {
            EXPECT_PIXEL_NEAR(0, 0, 127, 127, 127, 127, 1);
        }

        ANGLE_GL_PROGRAM(program, essl1_shaders::vs::Simple(), essl1_shaders::fs::UniformColor());
        glUseProgram(program);
        const GLint colorUniformLocation =
            glGetUniformLocation(program, angle::essl1_shaders::ColorUniform());
        ASSERT_NE(colorUniformLocation, -1);
        glUniform4f(colorUniformLocation, 0.25, 0.25, 0.25, 0.25);
        drawQuad(program, essl1_shaders::PositionAttrib(), 0.0f);
        ASSERT_GL_NO_ERROR();

        if (isSrgb)
        {
            EXPECT_PIXEL_NEAR(0, 0, 137, 137, 137, 64, 1);
        }
        else
        {
            EXPECT_PIXEL_NEAR(0, 0, 64, 64, 64, 64, 1);
        }

        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
        glUniform4f(colorUniformLocation, 0.5, 0.5, 0.5, 0.5);
        drawQuad(program, essl1_shaders::PositionAttrib(), 0.0f);
        ASSERT_GL_NO_ERROR();

        if (isSrgb)
        {
            EXPECT_PIXEL_NEAR(0, 0, 225, 225, 225, 191, 1);
        }
        else
        {
            EXPECT_PIXEL_NEAR(0, 0, 191, 191, 191, 191, 1);
        }
    }

    void testBlit(bool isFromFboToSurface, bool isFboSrgb, bool isSurfaceSrgb, bool isES3)
    {
        PFNGLBLITFRAMEBUFFERPROC blitFramebuffer = isES3 ? glBlitFramebuffer : glBlitFramebufferNV;

        GLRenderbuffer rb;
        glBindRenderbuffer(GL_RENDERBUFFER, rb);
        glRenderbufferStorage(GL_RENDERBUFFER, isFboSrgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, 128, 128);
        ASSERT_GL_NO_ERROR();

        GLFramebuffer fb;
        glBindFramebuffer(GL_FRAMEBUFFER, fb);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rb);
        ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);
        ASSERT_GL_NO_ERROR();

        // Clear source to 0.5
        glBindFramebuffer(GL_FRAMEBUFFER, isFromFboToSurface ? fb : 0);
        glClearColor(0.25, 0.5, 0.75, 0.5);
        glClear(GL_COLOR_BUFFER_BIT);
        ASSERT_GL_NO_ERROR();

        // Clear destination to 0.0
        glBindFramebuffer(GL_FRAMEBUFFER, isFromFboToSurface ? 0 : fb);
        glClearColor(0.0, 0.0, 0.0, 0.0);
        glClear(GL_COLOR_BUFFER_BIT);
        ASSERT_GL_NO_ERROR();

        glBindFramebuffer(GL_READ_FRAMEBUFFER, isFromFboToSurface ? fb : 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, isFromFboToSurface ? 0 : fb);
        blitFramebuffer(0, 0, 128, 128, 0, 0, 128, 128, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        ASSERT_GL_NO_ERROR();

        glBindFramebuffer(GL_READ_FRAMEBUFFER, isFromFboToSurface ? 0 : fb);
        if ((isFromFboToSurface && isSurfaceSrgb) || (!isFromFboToSurface && isFboSrgb))
        {
            EXPECT_PIXEL_NEAR(0, 0, 137, 188, 225, 127, 1);
        }
        else
        {
            EXPECT_PIXEL_NEAR(0, 0, 64, 127, 191, 127, 1);
        }

        // Test linear filtering

        if (!isFromFboToSurface)
        {
            // Prepare the default framebuffer content
            std::vector<uint8_t> data(128 * 128 * 4);
            for (size_t i = 0; i < data.size(); ++i)
            {
                data[i] = i & 4 ? 255 : 0;
            }
            GLTexture tempTex;
            glBindTexture(GL_TEXTURE_2D, tempTex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                         data.data());
            ASSERT_GL_NO_ERROR();

            GLFramebuffer tempFb;
            glBindFramebuffer(GL_FRAMEBUFFER, tempFb);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tempTex, 0);
            ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);
            ASSERT_GL_NO_ERROR();

            glBindFramebuffer(GL_READ_FRAMEBUFFER, tempFb);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            blitFramebuffer(0, 0, 128, 128, 0, 0, 128, 128, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            ASSERT_GL_NO_ERROR();
        }

        const int fboDim = isFromFboToSurface ? 256 : 64;
        std::vector<uint8_t> data(fboDim * fboDim * 4);
        if (isFromFboToSurface)
        {
            // Prepare texture content
            for (size_t i = 0; i < data.size(); ++i)
            {
                data[i] = i & 4 ? 255 : 0;
            }
        }

        GLTexture tex;
        glBindTexture(GL_TEXTURE_2D, tex);
        if (isES3)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, isFboSrgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, fboDim, fboDim,
                         0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
        }
        else
        {
            glTexImage2D(GL_TEXTURE_2D, 0, isFboSrgb ? GL_SRGB_ALPHA_EXT : GL_RGBA, fboDim, fboDim,
                         0, isFboSrgb ? GL_SRGB_ALPHA_EXT : GL_RGBA, GL_UNSIGNED_BYTE, data.data());
        }
        ASSERT_GL_NO_ERROR();

        glBindFramebuffer(GL_FRAMEBUFFER, fb);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        ASSERT_GL_NO_ERROR();
        ASSERT_GL_FRAMEBUFFER_COMPLETE(GL_FRAMEBUFFER);

        // Clear destination to 0.0
        glBindFramebuffer(GL_FRAMEBUFFER, isFromFboToSurface ? 0 : fb);
        glClearColor(0.0, 0.0, 0.0, 0.0);
        glClear(GL_COLOR_BUFFER_BIT);
        ASSERT_GL_NO_ERROR();

        glBindFramebuffer(GL_READ_FRAMEBUFFER, isFromFboToSurface ? fb : 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, isFromFboToSurface ? 0 : fb);
        if (isFromFboToSurface)
        {
            blitFramebuffer(0, 0, 256, 256, 0, 0, 128, 128, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        }
        else
        {
            blitFramebuffer(0, 0, 128, 128, 0, 0, 64, 64, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        }
        ASSERT_GL_NO_ERROR();

        glBindFramebuffer(GL_READ_FRAMEBUFFER, isFromFboToSurface ? 0 : fb);
        if ((isFromFboToSurface && isSurfaceSrgb) || (!isFromFboToSurface && isFboSrgb))
        {
            EXPECT_PIXEL_NEAR(0, 0, 188, 188, 188, 127, 1);
        }
        else
        {
            EXPECT_PIXEL_NEAR(0, 0, 127, 127, 127, 127, 1);
        }
    }

    void testBlitFromLinearSurfaceToLinearFbo(bool isES3) { testBlit(false, false, false, isES3); }
    void testBlitFromLinearFboToLinearSurface(bool isES3) { testBlit(true, false, false, isES3); }
    void testBlitFromLinearSurfaceToSrgbFbo(bool isES3) { testBlit(false, true, false, isES3); }
    void testBlitFromSrgbFboToLinearSurface(bool isES3) { testBlit(true, true, false, isES3); }

    void testBlitFromSrgbSurfaceToLinearFbo(bool isES3) { testBlit(false, false, true, isES3); }
    void testBlitFromLinearFboToSrgbSurface(bool isES3) { testBlit(true, false, true, isES3); }
    void testBlitFromSrgbSurfaceToSrgbFbo(bool isES3) { testBlit(false, true, true, isES3); }
    void testBlitFromSrgbFboToSrgbSurface(bool isES3) { testBlit(true, true, true, isES3); }
};

// Test that basic operations are performed with linear encoding.
TEST_P(SRGBFramebufferDefaultLinearTest, ClearAndDrawAndBlend)
{
    testBasic(false, getClientMajorVersion() >= 3);
}

// Test blits from the linearly-encoded default framebuffer to a linearly-encoded FBO.
TEST_P(SRGBFramebufferDefaultLinearTest, BlitToLinearFbo)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 &&
                       (!IsGLExtensionEnabled("GL_OES_rgb8_rgba8") ||
                        !IsGLExtensionEnabled("GL_NV_framebuffer_blit")));

    testBlitFromLinearSurfaceToLinearFbo(getClientMajorVersion() >= 3);
}

// Test blits from a linearly-encoded FBO to the linearly-encoded default framebuffer.
TEST_P(SRGBFramebufferDefaultLinearTest, BlitFromLinearFbo)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 &&
                       (!IsGLExtensionEnabled("GL_OES_rgb8_rgba8") ||
                        !IsGLExtensionEnabled("GL_NV_framebuffer_blit")));

    testBlitFromLinearFboToLinearSurface(getClientMajorVersion() >= 3);
}

// Test blits from the linearly-encoded default framebuffer to an sRGB-encoded FBO.
TEST_P(SRGBFramebufferDefaultLinearTest, BlitToSrgbFbo)
{
    ANGLE_SKIP_TEST_IF(
        getClientMajorVersion() < 3 &&
        (!IsGLExtensionEnabled("GL_EXT_sRGB") || !IsGLExtensionEnabled("GL_NV_framebuffer_blit")));

    testBlitFromLinearSurfaceToSrgbFbo(getClientMajorVersion() >= 3);
}

// Test blits from an sRGB-encoded FBO to the linearly-encoded default framebuffer.
TEST_P(SRGBFramebufferDefaultLinearTest, BlitFromSrgbFbo)
{
    ANGLE_SKIP_TEST_IF(
        getClientMajorVersion() < 3 &&
        (!IsGLExtensionEnabled("GL_EXT_sRGB") || !IsGLExtensionEnabled("GL_NV_framebuffer_blit")));

    testBlitFromSrgbFboToLinearSurface(getClientMajorVersion() >= 3);
}

class SRGBFramebufferDefaultSrgbTest : public SRGBFramebufferDefaultLinearTest
{
  protected:
    SRGBFramebufferDefaultSrgbTest() : SRGBFramebufferDefaultLinearTest()
    {
        setConfigColorSpace(EGL_GL_COLORSPACE_SRGB);
    }
};

// Test that basic operations are performed with sRGB encoding.
TEST_P(SRGBFramebufferDefaultSrgbTest, ClearAndDrawAndBlend)
{
    testBasic(true, getClientMajorVersion() >= 3);
}

// Test blits from the sRGB-encoded default framebuffer to a linearly-encoded FBO.
TEST_P(SRGBFramebufferDefaultSrgbTest, BlitToLinearFbo)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 &&
                       (!IsGLExtensionEnabled("GL_OES_rgb8_rgba8") ||
                        !IsGLExtensionEnabled("GL_NV_framebuffer_blit")));

    testBlitFromSrgbSurfaceToLinearFbo(getClientMajorVersion() >= 3);
}

// Test blits from a linearly-encoded FBO to the sRGB-encoded default framebuffer.
TEST_P(SRGBFramebufferDefaultSrgbTest, BlitFromLinearFbo)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3 &&
                       (!IsGLExtensionEnabled("GL_OES_rgb8_rgba8") ||
                        !IsGLExtensionEnabled("GL_NV_framebuffer_blit")));

    testBlitFromLinearFboToSrgbSurface(getClientMajorVersion() >= 3);
}

// Test blits from the sRGB-encoded default framebuffer to an sRGB-encoded FBO.
TEST_P(SRGBFramebufferDefaultSrgbTest, BlitToSrgbFbo)
{
    ANGLE_SKIP_TEST_IF(
        getClientMajorVersion() < 3 &&
        (!IsGLExtensionEnabled("GL_EXT_sRGB") || !IsGLExtensionEnabled("GL_NV_framebuffer_blit")));

    testBlitFromSrgbSurfaceToSrgbFbo(getClientMajorVersion() >= 3);
}

// Test blits from an sRGB-encoded FBO to the sRGB-encoded default framebuffer.
TEST_P(SRGBFramebufferDefaultSrgbTest, BlitFromSrgbFbo)
{
    ANGLE_SKIP_TEST_IF(
        getClientMajorVersion() < 3 &&
        (!IsGLExtensionEnabled("GL_EXT_sRGB") || !IsGLExtensionEnabled("GL_NV_framebuffer_blit")));

    testBlitFromSrgbFboToSrgbSurface(getClientMajorVersion() >= 3);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(SRGBFramebufferTest);
ANGLE_INSTANTIATE_TEST_ES3(SRGBFramebufferTestES3);

ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(SRGBFramebufferDefaultLinearTest);
ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(SRGBFramebufferDefaultSrgbTest);

}  // namespace angle
