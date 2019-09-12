//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// FogTest.cpp: Tests basic usage of glFog.

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

#include "util/random_utils.h"

#include <stdint.h>

using namespace angle;

class FogTest : public ANGLETest
{
  protected:
    FogTest()
    {
        setWindowWidth(32);
        setWindowHeight(32);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setConfigDepthBits(24);
    }
};

// Initial state check.
TEST_P(FogTest, InitialState)
{
    EXPECT_GL_FALSE(glIsEnabled(GL_FOG));
    EXPECT_GL_NO_ERROR();

    GLint fogMode;
    glGetIntegerv(GL_FOG_MODE, &fogMode);
    EXPECT_GL_NO_ERROR();
    EXPECT_GLENUM_EQ(GL_EXP, fogMode);

    GLfloat fogModeAsFloat;
    glGetFloatv(GL_FOG_MODE, &fogModeAsFloat);
    EXPECT_GL_NO_ERROR();
    EXPECT_GLENUM_EQ(GL_EXP, fogModeAsFloat);

    GLfloat fogStart;
    GLfloat fogEnd;
    GLfloat fogDensity;

    glGetFloatv(GL_FOG_START, &fogStart);
    EXPECT_GL_NO_ERROR();
    glGetFloatv(GL_FOG_END, &fogEnd);
    EXPECT_GL_NO_ERROR();
    glGetFloatv(GL_FOG_DENSITY, &fogDensity);
    EXPECT_GL_NO_ERROR();

    EXPECT_EQ(0.0f, fogStart);
    EXPECT_EQ(1.0f, fogEnd);
    EXPECT_EQ(1.0f, fogDensity);

    const GLColor32F kInitialFogColor(0.0f, 0.0f, 0.0f, 0.0f);
    GLColor32F initialFogColor;
    glGetFloatv(GL_FOG_COLOR, &initialFogColor.R);
    EXPECT_GL_NO_ERROR();

    EXPECT_EQ(kInitialFogColor, initialFogColor);
}

// Negative test for parameter names.
TEST_P(FogTest, NegativeParameter)
{
    glFogfv(0, nullptr);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
}

// Negative test for parameter values.
TEST_P(FogTest, NegativeValues)
{
    glFogf(GL_FOG_MODE, 0.0f);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glFogf(GL_FOG_DENSITY, -1.0f);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Checks that fog state can be set.
TEST_P(FogTest, Set)
{
    GLfloat fogValue[4] = {};

    glFogf(GL_FOG_MODE, GL_EXP2);
    EXPECT_GL_NO_ERROR();

    glGetFloatv(GL_FOG_MODE, fogValue);
    EXPECT_GL_NO_ERROR();
    EXPECT_GLENUM_EQ(GL_EXP2, fogValue[0]);

    glFogf(GL_FOG_DENSITY, 2.0f);
    EXPECT_GL_NO_ERROR();

    glGetFloatv(GL_FOG_DENSITY, fogValue);
    EXPECT_GL_NO_ERROR();
    EXPECT_GLENUM_EQ(2.0f, fogValue[0]);

    glFogf(GL_FOG_START, 2.0f);
    EXPECT_GL_NO_ERROR();

    glGetFloatv(GL_FOG_START, fogValue);
    EXPECT_GL_NO_ERROR();
    EXPECT_GLENUM_EQ(2.0f, fogValue[0]);

    glFogf(GL_FOG_END, 2.0f);
    EXPECT_GL_NO_ERROR();

    glGetFloatv(GL_FOG_END, fogValue);
    EXPECT_GL_NO_ERROR();
    EXPECT_GLENUM_EQ(2.0f, fogValue[0]);

    const GLColor32F testColor(0.1f, 0.2f, 0.3f, 0.4f);
    glFogfv(GL_FOG_COLOR, &testColor.R);
    EXPECT_GL_NO_ERROR();
    glGetFloatv(GL_FOG_COLOR, fogValue);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(0.1f, fogValue[0]);
    EXPECT_EQ(0.2f, fogValue[1]);
    EXPECT_EQ(0.3f, fogValue[2]);
    EXPECT_EQ(0.4f, fogValue[3]);
}

ANGLE_INSTANTIATE_TEST(FogTest, ES1_D3D11(), ES1_OPENGL(), ES1_OPENGLES());
