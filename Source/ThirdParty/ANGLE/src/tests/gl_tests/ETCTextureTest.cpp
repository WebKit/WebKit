//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ETCTextureTest:
//   Tests for ETC lossy decode formats.
//

#include "test_utils/ANGLETest.h"

using namespace angle;

namespace
{

class ETCTextureTest : public ANGLETest
{
  protected:
    ETCTextureTest() : mTexture(0u)
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void SetUp() override
    {
        ANGLETest::SetUp();

        glGenTextures(1, &mTexture);
        ASSERT_GL_NO_ERROR();
    }

    void TearDown() override
    {
        glDeleteTextures(1, &mTexture);

        ANGLETest::TearDown();
    }

    GLuint mTexture;
};

// Tests a texture with ETC1 lossy decode format
TEST_P(ETCTextureTest, ETC1Validation)
{
    bool supported = extensionEnabled("GL_ANGLE_lossy_etc_decode");

    glBindTexture(GL_TEXTURE_2D, mTexture);

    GLubyte pixel[8] = { 0x0, 0x0, 0xf8, 0x2, 0x43, 0xff, 0x4, 0x12 };
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_ETC1_RGB8_LOSSY_DECODE_ANGLE, 4, 4, 0,
                           sizeof(pixel), pixel);
    if (supported)
    {
        EXPECT_GL_NO_ERROR();

        glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_ETC1_RGB8_LOSSY_DECODE_ANGLE,
                                  sizeof(pixel), pixel);
        EXPECT_GL_NO_ERROR();


        glCompressedTexImage2D(GL_TEXTURE_2D, 1, GL_ETC1_RGB8_LOSSY_DECODE_ANGLE, 2, 2, 0,
            sizeof(pixel), pixel);
        glCompressedTexImage2D(GL_TEXTURE_2D, 2, GL_ETC1_RGB8_LOSSY_DECODE_ANGLE, 1, 1, 0,
            sizeof(pixel), pixel);
    }
    else
    {
        EXPECT_GL_ERROR(GL_INVALID_ENUM);
    }
}

ANGLE_INSTANTIATE_TEST(ETCTextureTest,
                       ES2_D3D9(),
                       ES2_D3D11(),
                       ES2_D3D11_FL9_3(),
                       ES3_D3D11(),
                       ES2_OPENGL(),
                       ES3_OPENGL());
}  // anonymous namespace
