//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// TextureFilteringHintTest.cpp : Tests of the GL_CHROMIUM_texture_filtering_hint extension.

#include "test_utils/ANGLETest.h"

#include "test_utils/gl_raii.h"

namespace angle
{

class TextureFilteringHintTest : public ANGLETest
{
  protected:
    TextureFilteringHintTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }
};

// Test that setting the hint works only when the extension is enabled
TEST_P(TextureFilteringHintTest, Validation)
{
    if (IsGLExtensionEnabled("GL_CHROMIUM_texture_filtering_hint"))
    {
        GLint intValue = 0;
        glGetIntegerv(GL_TEXTURE_FILTERING_HINT_CHROMIUM, &intValue);
        EXPECT_GL_NO_ERROR();
        EXPECT_GLENUM_EQ(GL_DONT_CARE, intValue);

        glHint(GL_TEXTURE_FILTERING_HINT_CHROMIUM, GL_FASTEST);
        glGetIntegerv(GL_TEXTURE_FILTERING_HINT_CHROMIUM, &intValue);
        EXPECT_GL_NO_ERROR();
        EXPECT_GLENUM_EQ(GL_FASTEST, intValue);

        glHint(GL_TEXTURE_FILTERING_HINT_CHROMIUM, GL_NICEST);
        glGetIntegerv(GL_TEXTURE_FILTERING_HINT_CHROMIUM, &intValue);
        EXPECT_GL_NO_ERROR();
        EXPECT_GLENUM_EQ(GL_NICEST, intValue);

        glHint(GL_TEXTURE_FILTERING_HINT_CHROMIUM, GL_TEXTURE_2D);
        glGetIntegerv(GL_TEXTURE_FILTERING_HINT_CHROMIUM, &intValue);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);
    }
    else
    {
        glHint(GL_TEXTURE_FILTERING_HINT_CHROMIUM, GL_FASTEST);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);
    }
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(TextureFilteringHintTest);
}  // namespace angle
