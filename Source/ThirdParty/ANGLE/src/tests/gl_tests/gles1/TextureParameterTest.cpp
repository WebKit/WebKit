//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TextureParameterTest.cpp: Tests GLES1-specific usage of glTexParameter.

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

#include "util/random_utils.h"

#include <stdint.h>

using namespace angle;

class TextureParameterTest : public ANGLETest
{
  protected:
    TextureParameterTest()
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

// Initial state check
TEST_P(TextureParameterTest, InitialState)
{
    GLint params[4] = {};

    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, params);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(GL_NEAREST_MIPMAP_LINEAR, params[0]);

    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, params);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(GL_LINEAR, params[0]);

    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, params);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(GL_REPEAT, params[0]);

    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, params);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(GL_REPEAT, params[0]);

    glGetTexParameteriv(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, params);
    EXPECT_GL_NO_ERROR();
    EXPECT_GL_FALSE(params[0]);

    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, params);
    EXPECT_GL_NO_ERROR();

    EXPECT_EQ(0, params[0]);
    EXPECT_EQ(0, params[1]);
    EXPECT_EQ(0, params[2]);
    EXPECT_EQ(0, params[3]);
}

// Negative test: invalid enum / operation
TEST_P(TextureParameterTest, NegativeEnum)
{
    // Invalid target (not supported)
    glGetTexParameteriv(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, nullptr);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    // Invalid parameter name
    glGetTexParameteriv(GL_TEXTURE_2D, 0, nullptr);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    // Not enough buffer
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, 3);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    // Not supported in GLES1
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);
}

// Checks that GLES1-specific texture parameters can be set.
TEST_P(TextureParameterTest, Set)
{
    GLint params[4] = {};

    glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
    EXPECT_GL_NO_ERROR();

    glGetTexParameteriv(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, params);
    EXPECT_GL_NO_ERROR();

    EXPECT_GL_TRUE(params[0]);

    GLint cropRect[4] = {10, 20, 30, 40};

    glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, cropRect);
    EXPECT_GL_NO_ERROR();

    glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_CROP_RECT_OES, params);
    EXPECT_GL_NO_ERROR();

    for (int i = 0; i < 4; i++)
    {
        EXPECT_EQ(cropRect[i], params[i]);
    }
}

ANGLE_INSTANTIATE_TEST(TextureParameterTest, ES1_D3D11(), ES1_OPENGL(), ES1_OPENGLES());
