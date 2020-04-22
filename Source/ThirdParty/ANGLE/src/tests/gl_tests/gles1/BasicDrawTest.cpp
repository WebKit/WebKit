//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// BasicDrawTest.cpp: Tests basic fullscreen quad draw with and without
// GL_TEXTURE_2D enabled.

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

#include <vector>

using namespace angle;

class BasicDrawTest : public ANGLETest
{
  protected:
    BasicDrawTest()
    {
        setWindowWidth(32);
        setWindowHeight(32);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setConfigDepthBits(24);
    }

    std::vector<float> mPositions = {
        -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, 1.0f, 1.0f,
    };

    void drawRedQuad()
    {
        glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
        EXPECT_GL_NO_ERROR();
        glEnableClientState(GL_VERTEX_ARRAY);
        EXPECT_GL_NO_ERROR();
        glVertexPointer(2, GL_FLOAT, 0, mPositions.data());
        EXPECT_GL_NO_ERROR();
        glDrawArrays(GL_TRIANGLES, 0, 6);
        EXPECT_GL_NO_ERROR();
        EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
    }
};

// Draws a fullscreen quad with a certain color.
TEST_P(BasicDrawTest, DrawColor)
{
    drawRedQuad();
}

// Checks that textures can be enabled or disabled.
TEST_P(BasicDrawTest, EnableDisableTexture)
{
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    // Green
    GLubyte texture[] = {
        0x00,
        0xff,
        0x00,
    };

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, texture);

    // Texturing is disabled; still red;
    drawRedQuad();

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    // Texturing enabled; is green (provided modulate w/ white)
    glEnable(GL_TEXTURE_2D);
    EXPECT_GL_NO_ERROR();
    glDrawArrays(GL_TRIANGLES, 0, 6);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::green);
}

ANGLE_INSTANTIATE_TEST_ES1(BasicDrawTest);
