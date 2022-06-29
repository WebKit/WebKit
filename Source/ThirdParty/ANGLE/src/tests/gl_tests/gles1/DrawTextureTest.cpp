//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DrawTextureTest.cpp: Tests basic usage of glDrawTex*.

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

#include <memory>
#include <vector>

using namespace angle;

class DrawTextureTest : public ANGLETest<>
{
  protected:
    DrawTextureTest()
    {
        setWindowWidth(32);
        setWindowHeight(32);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setConfigDepthBits(24);
    }

    void testSetUp() override
    {
        mTexture.reset(new GLTexture());
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, mTexture->get());
    }

    void testTearDown() override { mTexture.reset(); }

    std::unique_ptr<GLTexture> mTexture;
};

// Negative test for invalid width/height values.
TEST_P(DrawTextureTest, NegativeValue)
{
    glDrawTexiOES(0, 0, 0, 0, 0);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
    glDrawTexiOES(0, 0, 0, -1, 0);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
    glDrawTexiOES(0, 0, 0, 0, -1);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
    glDrawTexiOES(0, 0, 0, -1, -1);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Basic draw.
TEST_P(DrawTextureTest, Basic)
{
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &GLColor::green);
    glDrawTexiOES(0, 0, 0, 1, 1);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Tests that odd viewport dimensions are handled correctly.
// If the viewport dimension is even, then the incorrect way
// of getting the center screen coordinate by dividing by 2 and
// converting to integer will work in that case, but not if
// the viewport dimension is odd.
TEST_P(DrawTextureTest, CorrectNdcForOddViewportDimensions)
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // clang-format off
    std::array<GLColor, 2> textureData = {
        GLColor::green, GLColor::green
    };
    // clang-format on

    glViewport(0, 0, 3, 3);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, textureData.data());

    GLint cropRect[] = {0, 0, 2, 1};
    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, cropRect);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    EXPECT_GL_NO_ERROR();

    GLint x = 1;
    GLint y = 1;
    glDrawTexiOES(x, y, 0, 2, 1);
    EXPECT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(x, y, GLColor::green);
    EXPECT_PIXEL_COLOR_EQ(x + 1, y, GLColor::green);

    EXPECT_PIXEL_COLOR_EQ(x, y + 1, GLColor::black);
    EXPECT_PIXEL_COLOR_EQ(x + 1, y + 1, GLColor::black);
    EXPECT_PIXEL_COLOR_EQ(x + 2, y, GLColor::black);
    EXPECT_PIXEL_COLOR_EQ(x + 3, y, GLColor::black);
}

// Tests that vertex attributes enabled with fewer than 6 verts do not cause a crash.
TEST_P(DrawTextureTest, VertexAttributesNoCrash)
{
    glEnableClientState(GL_COLOR_ARRAY);
    glColorPointer(4, GL_FLOAT, 0, &GLColor::white);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &GLColor::green);
    EXPECT_GL_NO_ERROR();

    glDrawTexiOES(0, 0, 0, 1, 1);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Tests that the color array, if enabled, is not used as the vertex color.
TEST_P(DrawTextureTest, ColorArrayNotUsed)
{
    glEnableClientState(GL_COLOR_ARRAY);

    // This color is set to black on purpose to ensure that the color in the upcoming vertex array
    // is not used in the texture draw. If it is used, then the texture we want to read will be
    // modulated with the color in the vertex array instead of GL_CURRENT_COLOR (which at the moment
    // is white (1.0, 1.0, 1.0, 1.0).
    glColorPointer(4, GL_FLOAT, 0, &GLColor::black);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &GLColor::green);
    EXPECT_GL_NO_ERROR();

    glDrawTexiOES(0, 0, 0, 1, 1);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

// Tests that values of differenty types are properly normalized with glColorPointer
TEST_P(DrawTextureTest, ColorArrayDifferentTypes)
{
    constexpr GLubyte kTextureColorData[] = {0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF,
                                             0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF};
    constexpr GLfloat kVertexPtrData[]    = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f};
    constexpr GLfloat kTexCoordPtrData[]  = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
    constexpr GLubyte kGLubyteData[]      = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                             0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    constexpr GLfloat kGLfloatData[]      = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0,
                                             1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    constexpr GLfixed kGLfixedData[]      = {0x10000, 0x10000, 0x10000, 0x10000, 0x10000, 0x10000,
                                             0x10000, 0x10000, 0x10000, 0x10000, 0x10000, 0x10000,
                                             0x10000, 0x10000, 0x10000, 0x10000};

    // We check a pixel coordinate at the border of where linear interpolation starts as
    // we fail to get correct interpolated values when we do not normalize the GLbyte values.
    constexpr GLint kCheckedPixelX         = 16;
    constexpr GLint kCheckedPixelY         = 8;
    constexpr unsigned int kPixelTolerance = 10u;

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, kTextureColorData);
    glVertexPointer(2, GL_FLOAT, 0, kVertexPtrData);
    glTexCoordPointer(2, GL_FLOAT, 0, kTexCoordPtrData);

    // Ensure the results do not change unexpectedly regardless of the color data format

    // Test GLubyte
    glColorPointer(4, GL_UNSIGNED_BYTE, 0, kGLubyteData);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_NEAR(kCheckedPixelX, kCheckedPixelY, GLColor::red, kPixelTolerance);

    // Test GLfloat
    glColorPointer(4, GL_FLOAT, 0, kGLfloatData);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_NEAR(kCheckedPixelX, kCheckedPixelY, GLColor::red, kPixelTolerance);

    // Test GLfixed
    glColorPointer(4, GL_FIXED, 0, kGLfixedData);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_NEAR(kCheckedPixelX, kCheckedPixelY, GLColor::red, kPixelTolerance);
}

ANGLE_INSTANTIATE_TEST_ES1(DrawTextureTest);
