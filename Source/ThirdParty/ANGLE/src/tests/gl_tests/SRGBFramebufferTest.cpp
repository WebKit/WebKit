//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SRGBFramebufferTest.cpp: Tests of sRGB framebuffer functionality.

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

namespace angle
{

class SRGBFramebufferTest : public ANGLETest
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

    GLColor linearColor(64, 127, 191, 255);
    GLColor srgbColor(13, 54, 133, 255);

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture.get());
    glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB_ALPHA_EXT, 1, 1, 0, GL_SRGB_ALPHA_EXT, GL_UNSIGNED_BYTE,
                 nullptr);

    GLFramebuffer framebuffer;
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.get());
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.get(), 0);

    glUseProgram(mProgram);
    glUniform4fv(mColorLocation, 1, srgbColor.toNormalizedVector().data());

    glEnable(GL_FRAMEBUFFER_SRGB_EXT);
    drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_NEAR(0, 0, linearColor, 1.0);

    glDisable(GL_FRAMEBUFFER_SRGB_EXT);
    drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_NEAR(0, 0, srgbColor, 1.0);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(SRGBFramebufferTest);

}  // namespace angle
