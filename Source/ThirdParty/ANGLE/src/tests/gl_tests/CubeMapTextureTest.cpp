//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"

using namespace angle;

class CubeMapTextureTest : public ANGLETest
{
  protected:
    CubeMapTextureTest()
    {
        setWindowWidth(256);
        setWindowHeight(256);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    virtual void SetUp()
    {
        ANGLETest::SetUp();

        mProgram = CompileProgram(essl1_shaders::vs::Simple(), essl1_shaders::fs::UniformColor());
        if (mProgram == 0)
        {
            FAIL() << "shader compilation failed.";
        }

        mColorLocation = glGetUniformLocation(mProgram, essl1_shaders::ColorUniform());

        glUseProgram(mProgram);

        glClearColor(0, 0, 0, 0);
        glClearDepthf(0.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);

        ASSERT_GL_NO_ERROR();
    }

    virtual void TearDown()
    {
        glDeleteProgram(mProgram);

        ANGLETest::TearDown();
    }

    GLuint mProgram;
    GLint mColorLocation;
};

// Verify that rendering to the faces of a cube map consecutively will correctly render to each
// face.
TEST_P(CubeMapTextureTest, RenderToFacesConsecutively)
{
    // TODO: Diagnose and fix. http://anglebug.com/2954
    ANGLE_SKIP_TEST_IF(IsVulkan() && IsIntel() && IsWindows());

    // http://anglebug.com/3145
    ANGLE_SKIP_TEST_IF(IsVulkan() && IsIntel() && IsFuchsia());

    const GLfloat faceColors[] = {
        1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f,
    };

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex);
    for (GLenum face = 0; face < 6; face++)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGBA, 1, 1, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, nullptr);
    }
    EXPECT_GL_NO_ERROR();

    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    EXPECT_GL_NO_ERROR();

    for (GLenum face = 0; face < 6; face++)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, tex, 0);
        EXPECT_GL_NO_ERROR();

        glUseProgram(mProgram);

        const GLfloat *faceColor = faceColors + (face * 4);
        glUniform4f(mColorLocation, faceColor[0], faceColor[1], faceColor[2], faceColor[3]);

        drawQuad(mProgram, essl1_shaders::PositionAttrib(), 0.5f);
        EXPECT_GL_NO_ERROR();
    }

    for (GLenum face = 0; face < 6; face++)
    {
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, tex, 0);
        EXPECT_GL_NO_ERROR();

        const GLfloat *faceColor = faceColors + (face * 4);
        EXPECT_PIXEL_EQ(0, 0, faceColor[0] * 255, faceColor[1] * 255, faceColor[2] * 255,
                        faceColor[3] * 255);
        EXPECT_GL_NO_ERROR();
    }

    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);

    EXPECT_GL_NO_ERROR();
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST(CubeMapTextureTest,
                       ES2_D3D11(),
                       ES2_D3D11_FL10_0(),
                       ES2_D3D11_FL9_3(),
                       ES2_OPENGL(),
                       ES3_OPENGL(),
                       ES2_OPENGLES(),
                       ES3_OPENGLES(),
                       ES2_VULKAN());
