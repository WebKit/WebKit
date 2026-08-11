//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// EXTTextureLodBiasTest.cpp:
//   Tests for the GL_EXT_texture_lod_bias extension (GLES 1.1).

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

class EXTTextureLodBiasTest : public ANGLETest<>
{
  protected:
    EXTTextureLodBiasTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void setupMipmapTexture()
    {
        std::vector<GLColor> redColor(4 * 4, GLColor::red);
        std::vector<GLColor> greenColor(2 * 2, GLColor::green);
        std::vector<GLColor> blueColor(1 * 1, GLColor::blue);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     redColor.data());
        glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     greenColor.data());
        glTexImage2D(GL_TEXTURE_2D, 2, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     blueColor.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    std::array<float, 12> mQuadPositions = {
        -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, -1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f,
    };

    std::array<float, 8> mQuadTexCoords = {
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
    };

    void setupQuadVertexArrays(int numTextureUnits = 1)
    {
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, mQuadPositions.data());

        for (int i = 0; i < numTextureUnits; ++i)
        {
            glClientActiveTexture(GL_TEXTURE0 + i);
            glEnableClientState(GL_TEXTURE_COORD_ARRAY);
            glTexCoordPointer(2, GL_FLOAT, 0, mQuadTexCoords.data());
        }
        glClientActiveTexture(GL_TEXTURE0);
    }
};

// Test that GL_TEXTURE_LOD_BIAS_EXT can be set and queried.
TEST_P(EXTTextureLodBiasTest, Query)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_lod_bias"));

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);

    // Default value should be 0.0
    GLfloat bias = -1.0f;
    glGetTexEnvfv(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, &bias);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(0.0f, bias);

    // Set a positive bias
    glTexEnvf(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, 1.5f);
    EXPECT_GL_NO_ERROR();

    glGetTexEnvfv(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, &bias);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(1.5f, bias);

    // Set a negative bias
    glTexEnvf(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, -1.5f);
    EXPECT_GL_NO_ERROR();

    glGetTexEnvfv(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, &bias);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(-1.5f, bias);

    // Query MAX_TEXTURE_LOD_BIAS_EXT
    GLfloat maxLodBias = -1.0f;
    glGetFloatv(GL_MAX_TEXTURE_LOD_BIAS_EXT, &maxLodBias);
    EXPECT_GL_NO_ERROR();

    // Query MAX_TEXTURE_LOD_BIAS_EXT as int
    GLint maxLodBiasI = -1;
    glGetIntegerv(GL_MAX_TEXTURE_LOD_BIAS_EXT, &maxLodBiasI);
    EXPECT_GL_NO_ERROR();
}

// Test that GL_TEXTURE_LOD_BIAS_EXT can be set and queried using integer and fixed point types.
TEST_P(EXTTextureLodBiasTest, QueryIntegerAndFixed)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_lod_bias"));

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);

    // Test integer functions
    GLint biasI = -1;
    glGetTexEnviv(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, &biasI);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(0, biasI);

    glTexEnvi(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, 2);
    EXPECT_GL_NO_ERROR();

    glGetTexEnviv(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, &biasI);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(2, biasI);

    // Test fixed point functions
    // 1.0 in fixed point is 0x10000
    GLfixed biasX = -1;
    glTexEnvx(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, 0x10000);  // 1.0
    EXPECT_GL_NO_ERROR();

    glGetTexEnvxv(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, &biasX);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(0x10000, biasX);

    // Query as int after setting as fixed
    glGetTexEnviv(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, &biasI);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(1, biasI);
}

// Test that GL_TEXTURE_LOD_BIAS_EXT affects rendering in GLES 1.
TEST_P(EXTTextureLodBiasTest, Render)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_lod_bias"));

    glEnable(GL_TEXTURE_2D);
    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);

    setupMipmapTexture();

    setupQuadVertexArrays();

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);  // Modulate with white (no-op)

    glViewport(0, 0, 4, 4);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    // Add +1.0 bias. It should sample Level 1 (Green)
    glTexEnvf(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, 1.0f);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);

    // Add +2.0 bias. It should sample Level 2 (Blue)
    glTexEnvf(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, 2.0f);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);
}

// Test that GL_TEXTURE_LOD_BIAS_EXT affects rendering for point sprites in GLES 1.
TEST_P(EXTTextureLodBiasTest, PointSpriteRender)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_lod_bias"));
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_point_sprite"));

    glEnable(GL_TEXTURE_2D);
    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);

    setupMipmapTexture();

    // Enable point sprites
    glEnable(GL_POINT_SPRITE_OES);
    glTexEnvi(GL_POINT_SPRITE_OES, GL_COORD_REPLACE_OES, GL_TRUE);

    // Set up vertex for a single point at the center
    std::vector<float> positions = {0.0f, 0.0f, 0.0f};

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, positions.data());

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glViewport(0, 0, 4, 4);
    glPointSize(4.0f);

    glDrawArrays(GL_POINTS, 0, 1);
    EXPECT_GL_NO_ERROR();

    // Check the center pixel
    EXPECT_PIXEL_COLOR_EQ(2, 2, GLColor::red);

    // Add +1.0 bias. It should sample Level 1 (Green)
    glTexEnvf(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, 1.0f);
    glDrawArrays(GL_POINTS, 0, 1);
    EXPECT_PIXEL_COLOR_EQ(2, 2, GLColor::green);

    // Add +2.0 bias. It should sample Level 2 (Blue)
    glTexEnvf(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, 2.0f);
    glDrawArrays(GL_POINTS, 0, 1);
    EXPECT_PIXEL_COLOR_EQ(2, 2, GLColor::blue);

    // Clean up
    glDisable(GL_POINT_SPRITE_OES);
}

// Test that GL_TEXTURE_LOD_BIAS_EXT works correctly on non-zero texture units.
TEST_P(EXTTextureLodBiasTest, MultiTextureUnitRender)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_lod_bias"));

    // We need at least 2 texture units
    GLint maxTexUnits = 0;
    glGetIntegerv(GL_MAX_TEXTURE_UNITS, &maxTexUnits);
    ANGLE_SKIP_TEST_IF(maxTexUnits < 2);

    // Shared Texture for Unit 0 and Unit 1:
    // Level 0 = Red, Level 1 = Green, Level 2 = Blue
    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    setupMipmapTexture();

    // Unit 0: Mode = REPLACE
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

    // Unit 1: Mode = ADD
    glActiveTexture(GL_TEXTURE1);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_ADD);

    setupQuadVertexArrays(2);

    glViewport(0, 0, 4, 4);

    // Case 1: No bias on either unit (0.0, 0.0). Both sample Red. Result = Red + Red = Red
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    // Case 2: Bias +1.0 on Unit 1. Unit 0 (Red) + Unit 1 (Green) = Yellow
    glActiveTexture(GL_TEXTURE1);
    glTexEnvf(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, 1.0f);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::yellow);

    // Case 3: Bias +2.0 on Unit 0, and keep +1.0 on Unit 1.
    // Unit 0 (Blue) + Unit 1 (Green) = Cyan
    glActiveTexture(GL_TEXTURE0);
    glTexEnvf(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, 2.0f);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::cyan);
}

// Test that extreme values for GL_TEXTURE_LOD_BIAS_EXT do not crash and are bounded by mip limits.
TEST_P(EXTTextureLodBiasTest, LargeBiasClamping)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_lod_bias"));

    glEnable(GL_TEXTURE_2D);
    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);

    setupMipmapTexture();
    setupQuadVertexArrays();
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glViewport(0, 0, 4, 4);

    // Set an extreme positive bias. It should max out at Level 2 (Blue).
    glTexEnvf(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, 100.0f);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::blue);

    // Set an extreme negative bias. It should min out at Level 0 (Red).
    glTexEnvf(GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, -100.0f);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
}

ANGLE_INSTANTIATE_TEST_ES1(EXTTextureLodBiasTest);
