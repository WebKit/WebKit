//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// GetTexLevelParameterTest.cpp : Tests of the GL_ANGLE_get_tex_level_parameter extension.

#include "test_utils/ANGLETest.h"

#include "test_utils/gl_raii.h"

namespace angle
{

class GetTexLevelParameterTest : public ANGLETest
{
  protected:
    GetTexLevelParameterTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
        setExtensionsEnabled(false);
    }
};

// Extension is requestable so it should be disabled by default.
TEST_P(GetTexLevelParameterTest, ExtensionStringExposed)
{
    EXPECT_FALSE(IsGLExtensionEnabled("GL_ANGLE_get_tex_level_parameter"));

    if (IsGLExtensionRequestable("GL_ANGLE_get_tex_level_parameter"))
    {
        glRequestExtensionANGLE("GL_ANGLE_get_tex_level_parameter");
        EXPECT_GL_NO_ERROR();

        EXPECT_TRUE(IsGLExtensionEnabled("GL_ANGLE_get_tex_level_parameter"));
    }
}

// Test various queries exposed by GL_ANGLE_get_tex_level_parameter
TEST_P(GetTexLevelParameterTest, Queries)
{
    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_ANGLE_get_tex_level_parameter"));

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    {
        GLint width = 0;
        glGetTexLevelParameterivANGLE(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
        EXPECT_GL_NO_ERROR();
        EXPECT_EQ(1, width);
    }

    {
        GLint height = 0;
        glGetTexLevelParameterivANGLE(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
        EXPECT_GL_NO_ERROR();
        EXPECT_EQ(2, height);
    }

    {
        GLint internalFormat = 0;
        glGetTexLevelParameterivANGLE(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT,
                                      &internalFormat);
        EXPECT_GL_NO_ERROR();
        EXPECT_GLENUM_EQ(GL_RGBA, internalFormat);
    }
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(GetTexLevelParameterTest);
}  // namespace angle
