//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// QCOMTextureLodBiasTest.cpp:
//   Tests for the GL_QCOM_texture_lod_bias extension.

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

class QCOMTextureLodBiasTest : public ANGLETest<>
{
  protected:
    QCOMTextureLodBiasTest()
    {
        setWindowWidth(32);
        setWindowHeight(32);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void testSetUp() override
    {
        mProgram = CompileProgram(essl1_shaders::vs::Texture2D(), essl1_shaders::fs::Texture2D());
        if (mProgram == 0)
        {
            FAIL() << "shader compilation failed";
        }
    }

    void testTearDown() override
    {
        if (mProgram != 0)
        {
            glDeleteProgram(mProgram);
        }
    }

    void createMipmappedTexture()
    {
        // Level 0: Red
        std::vector<GLColor> redColor(4 * 4, GLColor::red);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     redColor.data());

        // Level 1: Green
        std::vector<GLColor> greenColor(2 * 2, GLColor::green);
        glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     greenColor.data());

        // Level 2: Blue
        std::vector<GLColor> blueColor(1 * 1, GLColor::blue);
        glTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     blueColor.data());
    }

    GLuint mProgram = 0;
};

// Test that GL_TEXTURE_LOD_BIAS_QCOM can be set and queried.
TEST_P(QCOMTextureLodBiasTest, Query)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_QCOM_texture_lod_bias"));

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);

    // Default value should be 0.0
    GLfloat bias = -1.0f;
    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS_QCOM, &bias);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(0.0f, bias);

    // Set a positive bias
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS_QCOM, 1.5f);
    EXPECT_GL_NO_ERROR();

    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS_QCOM, &bias);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(1.5f, bias);

    // Set a negative bias
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS_QCOM, -1.5f);
    EXPECT_GL_NO_ERROR();

    glGetTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS_QCOM, &bias);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(-1.5f, bias);
}

// Test that GL_TEXTURE_LOD_BIAS_QCOM can be set and queried on sampler objects.
TEST_P(QCOMTextureLodBiasTest, SamplerQuery)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_QCOM_texture_lod_bias"));
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);

    GLSampler sampler;

    // Default value should be 0.0
    GLfloat bias = -1.0f;
    glGetSamplerParameterfv(sampler, GL_TEXTURE_LOD_BIAS_QCOM, &bias);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(0.0f, bias);

    glSamplerParameterf(sampler, GL_TEXTURE_LOD_BIAS_QCOM, 1.5f);
    EXPECT_GL_NO_ERROR();

    glGetSamplerParameterfv(sampler, GL_TEXTURE_LOD_BIAS_QCOM, &bias);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(1.5f, bias);
}

// Test that GL_TEXTURE_LOD_BIAS_QCOM affects rendering.
TEST_P(QCOMTextureLodBiasTest, Render)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_QCOM_texture_lod_bias"));

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);

    createMipmappedTexture();

    // Set filtering to LinearMipmapNearest to pick a single mip level
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Draw quad that covers the whole viewport (4x4)
    // It should sample Level 0 (Red) initially
    glUseProgram(mProgram);
    glViewport(0, 0, 4, 4);
    drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    // Add +1.0 bias. It should sample Level 1 (Green)
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS_QCOM, 1.0f);
    drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    // Add +2.0 bias. It should sample Level 2 (Blue)
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS_QCOM, 2.0f);
    drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);
}

// Test that GL_TEXTURE_LOD_BIAS_QCOM on sampler objects affects rendering.
TEST_P(QCOMTextureLodBiasTest, SamplerRender)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_QCOM_texture_lod_bias"));
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);

    createMipmappedTexture();

    GLSampler sampler;
    glBindSampler(0, sampler);

    // Set filtering on sampler
    glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Draw quad that covers the whole viewport (4x4)
    // It should sample Level 0 (Red) initially
    glUseProgram(mProgram);
    glViewport(0, 0, 4, 4);
    drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    // Add +1.0 bias on sampler. It should sample Level 1 (Green)
    glSamplerParameterf(sampler, GL_TEXTURE_LOD_BIAS_QCOM, 1.0f);
    drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    // Add +2.0 bias on sampler. It should sample Level 2 (Blue)
    glSamplerParameterf(sampler, GL_TEXTURE_LOD_BIAS_QCOM, 2.0f);
    drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);

    glBindSampler(0, 0);
}

// Test that GL_TEXTURE_LOD_BIAS_QCOM is applied BEFORE TEXTURE_MAX_LOD clamping (Issue 3 in spec).
TEST_P(QCOMTextureLodBiasTest, Clamping)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_QCOM_texture_lod_bias"));
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);

    createMipmappedTexture();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Set TEXTURE_MAX_LOD to 1.0 (Limit to Level 1)
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_LOD, 1.0f);

    // Set bias to 2.0. Base LOD is 0. 0 + 2 = 2.
    // If applied before clamping: clamp(2, max=1) = 1 (Green).
    // If applied after clamping: clamp(0, max=1) + 2 = 2 (Blue).
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS_QCOM, 2.0f);

    glUseProgram(mProgram);
    glViewport(0, 0, 4, 4);
    drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.5f);

    // Expect Green (Level 1)
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// According to GL_QCOM_texture_lod_bias spec, the lodBias parameter should be clamped between the
// positive and negative values of the implementation defined constant MAX_TEXTURE_LOD_BIAS_EXT.
TEST_P(QCOMTextureLodBiasTest, BiasClamping)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_QCOM_texture_lod_bias"));

    GLfloat maxBias = 0.0f;
    glGetFloatv(GL_MAX_TEXTURE_LOD_BIAS, &maxBias);
    if (glGetError() != GL_NO_ERROR)
    {
        GTEST_SKIP() << "Test skipped: GL_MAX_TEXTURE_LOD_BIAS is not supported";
    }

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);

    createMipmappedTexture();

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Set bias to a value larger than maxBias.
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS_QCOM, maxBias + 1.0f);
    EXPECT_GL_NO_ERROR();

    glUseProgram(mProgram);
    glViewport(0, 0, 4, 4);
    drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.5f);

    // Should still use the max available level (Level 2 - Blue)
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);

    // Set negative bias larger than -maxBias
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS_QCOM, -(maxBias + 1.0f));
    EXPECT_GL_NO_ERROR();

    drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.5f);

    // Should use the min available level (Level 0 - Red)
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
}

// Test that GL_TEXTURE_LOD_BIAS_QCOM affects rendering when using textureLod in shader.
TEST_P(QCOMTextureLodBiasTest, RenderWithTextureLod)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_QCOM_texture_lod_bias"));
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);

    createMipmappedTexture();

    // Set filtering to LinearMipmapNearest to pick a single mip level
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    ANGLE_GL_PROGRAM(program, essl3_shaders::vs::Texture2DLod(), essl3_shaders::fs::Texture2DLod());
    glUseProgram(program);

    GLint texLocation = glGetUniformLocation(program, essl3_shaders::Texture2DUniform());
    GLint lodLocation = glGetUniformLocation(program, essl3_shaders::LodUniform());

    // Expect texture unit 0
    glUniform1i(texLocation, 0);

    glViewport(0, 0, 4, 4);

    // Set explicit LOD to 1.0. Base LOD is 0. Total LOD = 1.0.
    // It should sample Level 1 (Green).
    glUniform1f(lodLocation, 1.0f);
    drawQuad(program, essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    // Add +1.0 bias. Total LOD = 1.0 + 1.0 = 2.0.
    // It should sample Level 2 (Blue).
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS_QCOM, 1.0f);
    drawQuad(program, essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);

    // Set explicit LOD to 2.0, and negative bias -1.0. Total LOD = 2.0 - 1.0 = 1.0.
    // It should sample Level 1 (Green).
    glUniform1f(lodLocation, 2.0f);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS_QCOM, -1.0f);
    drawQuad(program, essl3_shaders::PositionAttrib(), 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(QCOMTextureLodBiasTest);
