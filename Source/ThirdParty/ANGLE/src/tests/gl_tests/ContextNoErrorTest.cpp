//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ContextNoErrorTest:
//   Tests pertaining to GL_KHR_no_error
//

#include <gtest/gtest.h>

#include "common/platform.h"
#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

class ContextNoErrorTest : public ANGLETest
{
  protected:
    ContextNoErrorTest() : mNaughtyTexture(0) { setNoErrorEnabled(true); }

    void testTearDown() override
    {
        if (mNaughtyTexture != 0)
        {
            glDeleteTextures(1, &mNaughtyTexture);
        }
    }

    void bindNaughtyTexture()
    {
        glGenTextures(1, &mNaughtyTexture);
        ASSERT_GL_NO_ERROR();
        glBindTexture(GL_TEXTURE_CUBE_MAP, mNaughtyTexture);
        ASSERT_GL_NO_ERROR();

        // mNaughtyTexture should now be a GL_TEXTURE_CUBE_MAP texture, so rebinding it to
        // GL_TEXTURE_2D is an error
        glBindTexture(GL_TEXTURE_2D, mNaughtyTexture);
    }

    GLuint mNaughtyTexture;
};

// Tests that error reporting is suppressed when GL_KHR_no_error is enabled
TEST_P(ContextNoErrorTest, NoError)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_KHR_no_error"));

    bindNaughtyTexture();
    EXPECT_GL_NO_ERROR();
}

ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(ContextNoErrorTest);
