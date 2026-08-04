//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// QueryTest.cpp: Tests basic boolean query of GLES1 enums.

#include "test_utils/ANGLETest.h"

#include <stdint.h>

using namespace angle;

class QueryTest : public ANGLETest<>
{
  protected:
    QueryTest()
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

// Test that glGetBooleanv works for GLES1 enums
TEST_P(QueryTest, Basic)
{
    std::vector<GLenum> pnames = {GL_ALPHA_TEST,
                                  GL_CLIP_PLANE0,
                                  GL_CLIP_PLANE1,
                                  GL_CLIP_PLANE2,
                                  GL_CLIP_PLANE3,
                                  GL_CLIP_PLANE4,
                                  GL_CLIP_PLANE5,
                                  GL_COLOR_ARRAY,
                                  GL_COLOR_LOGIC_OP,
                                  GL_COLOR_MATERIAL,
                                  GL_FOG,
                                  GL_LIGHT0,
                                  GL_LIGHT1,
                                  GL_LIGHT2,
                                  GL_LIGHT3,
                                  GL_LIGHT4,
                                  GL_LIGHT5,
                                  GL_LIGHT6,
                                  GL_LIGHT7,
                                  GL_LIGHTING,
                                  GL_LINE_SMOOTH,
                                  GL_NORMAL_ARRAY,
                                  GL_NORMALIZE,
                                  GL_POINT_SIZE_ARRAY_OES,
                                  GL_POINT_SMOOTH,
                                  GL_POINT_SPRITE_OES,
                                  GL_RESCALE_NORMAL,
                                  GL_TEXTURE_2D,
                                  GL_TEXTURE_CUBE_MAP,
                                  GL_TEXTURE_COORD_ARRAY,
                                  GL_VERTEX_ARRAY};

    for (GLenum pname : pnames)
    {
        GLboolean data = GL_FALSE;
        glGetBooleanv(pname, &data);
        EXPECT_GL_NO_ERROR();
    }
}

// Test setting and querying blend equation state
TEST_P(QueryTest, BlendEquation)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_blend_subtract"));

    {
        GLint mode = -1;
        glGetIntegerv(GL_BLEND_EQUATION_OES, &mode);
        EXPECT_GL_NO_ERROR();
        EXPECT_EQ(mode, GL_FUNC_ADD_OES);
    }

    for (GLenum mode : {GL_FUNC_ADD_OES, GL_FUNC_SUBTRACT_OES, GL_FUNC_REVERSE_SUBTRACT_OES})
    {
        glBlendEquationOES(mode);
        EXPECT_GL_NO_ERROR();

        GLint value = -1;
        glGetIntegerv(GL_BLEND_EQUATION_OES, &value);
        EXPECT_GL_NO_ERROR();
        EXPECT_EQ(mode, static_cast<GLenum>(value));
    }

    if (IsGLExtensionEnabled("GL_EXT_blend_minmax"))
    {
        for (GLenum mode : {GL_MIN_EXT, GL_MAX_EXT})
        {
            glBlendEquationOES(mode);
            EXPECT_GL_NO_ERROR();

            GLint value = -1;
            glGetIntegerv(GL_BLEND_EQUATION_OES, &value);
            EXPECT_GL_NO_ERROR();
            EXPECT_EQ(mode, static_cast<GLenum>(value));
        }
    }

    EXPECT_FALSE(IsGLExtensionEnabled("GL_KHR_blend_equation_advanced"));
    EXPECT_FALSE(IsGLExtensionEnabled("GL_KHR_blend_equation_advanced_coherent"));
}

ANGLE_INSTANTIATE_TEST_ES1(QueryTest);
