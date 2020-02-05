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

    void testSetUp() override
    {
        glGenRenderbuffers(1, &mRenderbuffer);

        ASSERT_GL_NO_ERROR();
    }

    void testTearDown() override
    {
        glDeleteRenderbuffers(1, &mRenderbuffer);
        mRenderbuffer = 0;
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

// Ensure that the following spec language is correctly implemented:
//
//   the resulting value for RENDERBUFFER_SAMPLES is guaranteed to be greater than or equal to
//   samples and no more than the next larger sample count supported by the implementation.
//
// For example, if 2, 4, and 8 samples are supported, if 5 samples are requested, ANGLE will
// use 8 samples, and return 8 when GL_RENDERBUFFER_SAMPLES is queried.
TEST_P(RenderbufferMultisampleTest, OddSampleCount)
{
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);

    glBindRenderbuffer(GL_RENDERBUFFER, mRenderbuffer);
    ASSERT_GL_NO_ERROR();

    // Lookup the supported number of sample counts
    GLint numSampleCounts = 0;
    std::vector<GLint> sampleCounts;
    GLsizei queryBufferSize = 1;
    glGetInternalformativ(GL_RENDERBUFFER, GL_RGBA8, GL_NUM_SAMPLE_COUNTS, queryBufferSize,
                          &numSampleCounts);
    ANGLE_SKIP_TEST_IF((numSampleCounts < 2));
    sampleCounts.resize(numSampleCounts);
    queryBufferSize = numSampleCounts;
    glGetInternalformativ(GL_RENDERBUFFER, GL_RGBA8, GL_SAMPLES, queryBufferSize,
                          sampleCounts.data());

    // Look for two sample counts that are not 1 apart (e.g. 2 and 4).  Request a sample count
    // that's between those two samples counts (e.g. 3) and ensure that GL_RENDERBUFFER_SAMPLES
    // is the higher number.
    for (int i = 1; i < numSampleCounts; i++)
    {
        if (sampleCounts[i - 1] > (sampleCounts[i] + 1))
        {
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, sampleCounts[i] + 1, GL_RGBA8, 64,
                                             64);
            ASSERT_GL_NO_ERROR();
            GLint renderbufferSamples = 0;
            glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES,
                                         &renderbufferSamples);
            ASSERT_GL_NO_ERROR();
            EXPECT_EQ(renderbufferSamples, sampleCounts[i - 1]);
            break;
        }
    }
}

ANGLE_INSTANTIATE_TEST_ES3_AND_ES31(RenderbufferMultisampleTest);
}  // namespace
