//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// OESTextureCubeMapTest:
//   Tests for GL_OES_texture_cube_map in OpenGL ES 1.1.

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

class OESTextureCubeMapTest : public ANGLETest<>
{
  protected:
    OESTextureCubeMapTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void setupCommonTestState()
    {
        glDisable(GL_TEXTURE_2D);
        glEnable(GL_TEXTURE_CUBE_MAP_OES);

        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, mQuadPositions.data());
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);

        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    }

    void teardownCommonTestState()
    {
        glDisable(GL_TEXTURE_CUBE_MAP_OES);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisableClientState(GL_VERTEX_ARRAY);
    }

    std::array<float, 12> mQuadPositions = {
        -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, -1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f,
    };
};

// Test pure GL_TEXTURE_CUBE_MAP_OES base level sampling on different texture units.
TEST_P(OESTextureCubeMapTest, CubeMapTextureUnitsRender)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_cube_map"));

    GLint maxTexUnits = 1;
    glGetIntegerv(GL_MAX_TEXTURE_UNITS, &maxTexUnits);
    // Cap at 3 units because unit 3 is not supported for cubemaps by the current GLES1
    // ubershader implementation.
    maxTexUnits = std::min(maxTexUnits, 3);

    for (GLint unit = 0; unit < maxTexUnits; ++unit)
    {
        glActiveTexture(GL_TEXTURE0 + unit);
        glClientActiveTexture(GL_TEXTURE0 + unit);

        setupCommonTestState();
        GLTexture texture;
        glBindTexture(GL_TEXTURE_CUBE_MAP_OES, texture);

        // Define 6 distinct colors for the 6 faces
        std::vector<GLColor> colors = {
            GLColor::red,      // +X
            GLColor::green,    // -X
            GLColor::blue,     // +Y
            GLColor::yellow,   // -Y
            GLColor::cyan,     // +Z
            GLColor::magenta,  // -Z
        };

        for (int i = 0; i < 6; ++i)
        {
            GLenum face = GL_TEXTURE_CUBE_MAP_POSITIVE_X_OES + i;
            std::vector<GLColor> faceData(4 * 4, colors[i]);
            glTexImage2D(face, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, faceData.data());
        }

        glTexParameteri(GL_TEXTURE_CUBE_MAP_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        EXPECT_GL_NO_ERROR();
        glViewport(0, 0, 4, 4);

        std::vector<std::array<float, 3>> faceDirections = {
            {1.0f, 0.0f, 0.0f},   // +X
            {-1.0f, 0.0f, 0.0f},  // -X
            {0.0f, 1.0f, 0.0f},   // +Y
            {0.0f, -1.0f, 0.0f},  // -Y
            {0.0f, 0.0f, 1.0f},   // +Z
            {0.0f, 0.0f, -1.0f},  // -Z
        };

        for (size_t i = 0; i < 6; ++i)
        {
            std::array<float, 12> texCoords;
            for (int v = 0; v < 4; ++v)
            {
                texCoords[v * 3 + 0] = faceDirections[i][0];
                texCoords[v * 3 + 1] = faceDirections[i][1];
                texCoords[v * 3 + 2] = faceDirections[i][2];
            }
            glTexCoordPointer(3, GL_FLOAT, 0, texCoords.data());
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            GLColor actualColor = ReadColor(0, 0);
            EXPECT_EQ(colors[i], actualColor) << "Mismatch on unit " << unit << " face " << i;
        }

        teardownCommonTestState();
    }

    glActiveTexture(GL_TEXTURE0);
    glClientActiveTexture(GL_TEXTURE0);
}

// Test GL_TEXTURE_CUBE_MAP_OES mipmap sampling.
TEST_P(OESTextureCubeMapTest, CubeMapMipmapRender)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_cube_map"));

    setupCommonTestState();
    GLTexture texture;
    glBindTexture(GL_TEXTURE_CUBE_MAP_OES, texture);

    // Level 0: 16x16, Red
    // Level 1: 8x8, Green
    // Level 2: 4x4, Blue
    // Level 3: 2x2, Yellow
    // Level 4: 1x1, Cyan

    std::vector<GLColor> level0Data(16 * 16, GLColor::red);
    std::vector<GLColor> level1Data(8 * 8, GLColor::green);
    std::vector<GLColor> level2Data(4 * 4, GLColor::blue);
    std::vector<GLColor> level3Data(2 * 2, GLColor::yellow);
    std::vector<GLColor> level4Data(1 * 1, GLColor::cyan);

    for (int i = 0; i < 6; ++i)
    {
        GLenum face = GL_TEXTURE_CUBE_MAP_POSITIVE_X_OES + i;

        glTexImage2D(face, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, level0Data.data());
        glTexImage2D(face, 1, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, level1Data.data());
        glTexImage2D(face, 2, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, level2Data.data());
        glTexImage2D(face, 3, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, level3Data.data());
        glTexImage2D(face, 4, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, level4Data.data());
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP_OES, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP_OES, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    EXPECT_GL_NO_ERROR();

    // Use +X face for testing
    std::vector<std::array<float, 3>> faceCoords = {
        {1.0f, -1.0f, -1.0f},  // Bottom Left
        {1.0f, 1.0f, -1.0f},   // Bottom Right
        {1.0f, -1.0f, 1.0f},   // Top Left
        {1.0f, 1.0f, 1.0f},    // Top Right
    };

    std::array<float, 12> texCoords;
    for (int v = 0; v < 4; ++v)
    {
        texCoords[v * 3 + 0] = faceCoords[v][0];
        texCoords[v * 3 + 1] = faceCoords[v][1];
        texCoords[v * 3 + 2] = faceCoords[v][2];
    }
    glTexCoordPointer(3, GL_FLOAT, 0, texCoords.data());

    // Test Level 0 (16x16 viewport)
    glViewport(0, 0, 16, 16);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    GLColor colorLevel0 = ReadColor(8, 8);
    EXPECT_EQ(GLColor::red, colorLevel0) << "Expected Red from Level 0";

    // Test Level 1 (8x8 viewport)
    glViewport(0, 0, 8, 8);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    GLColor colorLevel1 = ReadColor(4, 4);
    EXPECT_EQ(GLColor::green, colorLevel1) << "Expected Green from Level 1";

    // Test Level 2 (4x4 viewport)
    glViewport(0, 0, 4, 4);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    GLColor colorLevel2 = ReadColor(2, 2);
    EXPECT_EQ(GLColor::blue, colorLevel2) << "Expected Blue from Level 2";

    teardownCommonTestState();
}

ANGLE_INSTANTIATE_TEST_ES1(OESTextureCubeMapTest);
