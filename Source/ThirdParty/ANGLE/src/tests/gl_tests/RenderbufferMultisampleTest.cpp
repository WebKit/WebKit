//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RenderbufferMultisampleTest: Tests of multisampled renderbuffer

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

namespace
{

class RenderbufferMultisampleTest : public ANGLETest
{
  protected:
    RenderbufferMultisampleTest()
    {
        setWindowWidth(64);
        setWindowHeight(64);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void SetUp() override
    {
        ANGLETest::SetUp();

        glGenRenderbuffers(1, &mRenderbuffer);

        ASSERT_GL_NO_ERROR();
    }

    void TearDown() override
    {
        glDeleteRenderbuffers(1, &mRenderbuffer);
        mRenderbuffer = 0;

        ANGLETest::TearDown();
    }

    GLuint mRenderbuffer = 0;
};

// In GLES 3.0, if internalformat is integer (signed or unsigned), to allocate multisample
// renderbuffer storage for that internalformat is not supported. An INVALID_OPERATION is
// generated. In GLES 3.1, it is OK to allocate multisample renderbuffer storage for interger
// internalformat, but the max samples should be less than MAX_INTEGER_SAMPLES.
// MAX_INTEGER_SAMPLES should be at least 1.
TEST_P(RenderbufferMultisampleTest, IntegerInternalformat)
{
    glBindRenderbuffer(GL_RENDERBUFFER, mRenderbuffer);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 1, GL_RGBA8I, 64, 64);
    if (getClientMajorVersion() < 3 || getClientMinorVersion() < 1)
    {
        ASSERT_GL_ERROR(GL_INVALID_OPERATION);
    }
    else
    {
        ASSERT_GL_NO_ERROR();

        GLint maxSamplesRGBA8I = 0;
        glGetInternalformativ(GL_RENDERBUFFER, GL_RGBA8I, GL_SAMPLES, 1, &maxSamplesRGBA8I);
        GLint maxIntegerSamples = 0;
        glGetIntegerv(GL_MAX_INTEGER_SAMPLES, &maxIntegerSamples);
        ASSERT_GL_NO_ERROR();
        EXPECT_GE(maxIntegerSamples, 1);

        glRenderbufferStorageMultisample(GL_RENDERBUFFER, maxSamplesRGBA8I + 1, GL_RGBA8I, 64, 64);
        ASSERT_GL_ERROR(GL_INVALID_OPERATION);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, maxIntegerSamples + 1, GL_RGBA8I, 64, 64);
        ASSERT_GL_ERROR(GL_INVALID_OPERATION);
    }
}

ANGLE_INSTANTIATE_TEST(RenderbufferMultisampleTest,
                       ES3_D3D11(),
                       ES3_OPENGL(),
                       ES3_OPENGLES(),
                       ES31_D3D11(),
                       ES31_OPENGL(),
                       ES31_OPENGLES());
}  // namespace
