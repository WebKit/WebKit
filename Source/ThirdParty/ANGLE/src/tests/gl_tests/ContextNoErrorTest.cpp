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

class ContextNoErrorTest : public ANGLETest<>
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

class ContextNoErrorTest31 : public ContextNoErrorTest
{
  protected:
    ~ContextNoErrorTest31()
    {
        glDeleteProgram(mVertProg);
        glDeleteProgram(mFragProg);
        glDeleteProgramPipelines(1, &mPipeline);
    }

    void bindProgramPipeline(const GLchar *vertString, const GLchar *fragString);
    void drawQuadWithPPO(const std::string &positionAttribName,
                         const GLfloat positionAttribZ,
                         const GLfloat positionAttribXYScale);

    GLuint mVertProg;
    GLuint mFragProg;
    GLuint mPipeline;
};

void ContextNoErrorTest31::bindProgramPipeline(const GLchar *vertString, const GLchar *fragString)
{
    mVertProg = glCreateShaderProgramv(GL_VERTEX_SHADER, 1, &vertString);
    ASSERT_NE(mVertProg, 0u);
    mFragProg = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &fragString);
    ASSERT_NE(mFragProg, 0u);

    // Generate a program pipeline and attach the programs to their respective stages
    glGenProgramPipelines(1, &mPipeline);
    EXPECT_GL_NO_ERROR();
    glUseProgramStages(mPipeline, GL_VERTEX_SHADER_BIT, mVertProg);
    EXPECT_GL_NO_ERROR();
    glUseProgramStages(mPipeline, GL_FRAGMENT_SHADER_BIT, mFragProg);
    EXPECT_GL_NO_ERROR();
    glBindProgramPipeline(mPipeline);
    EXPECT_GL_NO_ERROR();
}

void ContextNoErrorTest31::drawQuadWithPPO(const std::string &positionAttribName,
                                           const GLfloat positionAttribZ,
                                           const GLfloat positionAttribXYScale)
{
    glUseProgram(0);

    std::array<Vector3, 6> quadVertices = ANGLETestBase::GetQuadVertices();

    for (Vector3 &vertex : quadVertices)
    {
        vertex.x() *= positionAttribXYScale;
        vertex.y() *= positionAttribXYScale;
        vertex.z() = positionAttribZ;
    }

    GLint positionLocation = glGetAttribLocation(mVertProg, positionAttribName.c_str());

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glVertexAttribPointer(positionLocation, 3, GL_FLOAT, GL_FALSE, 0, quadVertices.data());
    glEnableVertexAttribArray(positionLocation);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    glDisableVertexAttribArray(positionLocation);
    glVertexAttribPointer(positionLocation, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
}

// Tests that error reporting is suppressed when GL_KHR_no_error is enabled
TEST_P(ContextNoErrorTest, NoError)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_KHR_no_error"));

    bindNaughtyTexture();
    EXPECT_GL_NO_ERROR();
}

// Test glDetachShader to make sure it resolves linking with a no error context and doesn't assert
TEST_P(ContextNoErrorTest, DetachAfterLink)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_KHR_no_error"));

    GLuint vs      = CompileShader(GL_VERTEX_SHADER, essl1_shaders::vs::Simple());
    GLuint fs      = CompileShader(GL_FRAGMENT_SHADER, essl1_shaders::fs::Red());
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    glDetachShader(program, vs);
    glDetachShader(program, fs);

    glDeleteShader(vs);
    glDeleteShader(fs);
    glDeleteProgram(program);
    EXPECT_GL_NO_ERROR();
}

// Tests that we can draw with a program pipeline when GL_KHR_no_error is enabled.
TEST_P(ContextNoErrorTest31, DrawWithPPO)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_KHR_no_error"));

    // Only the Vulkan backend supports PPOs
    ANGLE_SKIP_TEST_IF(!IsVulkan());

    // TODO(http://anglebug.com/5102): Linking PPOs is currently done during draw call validation,
    // so drawing with a PPO fails without validation enabled.
    ANGLE_SKIP_TEST_IF(IsGLExtensionEnabled("GL_KHR_no_error"));

    // Create two separable program objects from a
    // single source string respectively (vertSrc and fragSrc)
    const GLchar *vertString = essl31_shaders::vs::Simple();
    const GLchar *fragString = essl31_shaders::fs::Red();

    bindProgramPipeline(vertString, fragString);

    drawQuadWithPPO("a_position", 0.5f, 1.0f);
    ASSERT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);
}

// Tests that an incorrect enum to GetInteger does not cause an application crash.
TEST_P(ContextNoErrorTest, InvalidGetIntegerDoesNotCrash)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_KHR_no_error"));

    GLint value = 1;
    glGetIntegerv(GL_TEXTURE_2D, &value);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(value, 1);
}

// Test that we ignore an invalid texture type when EGL_KHR_create_context_no_error is enabled.
TEST_P(ContextNoErrorTest, InvalidTextureType)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_KHR_no_error"));

    GLTexture texture;
    constexpr GLenum kInvalidTextureType = 0;

    glBindTexture(kInvalidTextureType, texture);
    ASSERT_GL_NO_ERROR();

    glTexParameteri(kInvalidTextureType, GL_TEXTURE_BASE_LEVEL, 0);
    ASSERT_GL_NO_ERROR();
}

ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(ContextNoErrorTest);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ContextNoErrorTest31);
ANGLE_INSTANTIATE_TEST_ES31(ContextNoErrorTest31);
