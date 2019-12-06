//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SamplerTest.cpp : Tests for samplers.

#include "test_utils/ANGLETest.h"

#include "test_utils/gl_raii.h"

namespace angle
{

class SamplersTest : public ANGLETest
{
  protected:
    SamplersTest() {}

    // Sets a value for GL_TEXTURE_MAX_ANISOTROPY_EXT and expects it to fail.
    void validateInvalidAnisotropy(GLSampler &sampler, float invalidValue)
    {
        glSamplerParameterf(sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, invalidValue);
        EXPECT_GL_ERROR(GL_INVALID_VALUE);
    }

    // Sets a value for GL_TEXTURE_MAX_ANISOTROPY_EXT and expects it to work.
    void validateValidAnisotropy(GLSampler &sampler, float validValue)
    {
        glSamplerParameterf(sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, validValue);
        EXPECT_GL_NO_ERROR();

        GLfloat valueToVerify = 0.0f;
        glGetSamplerParameterfv(sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, &valueToVerify);
        ASSERT_EQ(valueToVerify, validValue);
    }
};

// Verify that samplerParameterf supports TEXTURE_MAX_ANISOTROPY_EXT valid values.
TEST_P(SamplersTest, ValidTextureSamplerMaxAnisotropyExt)
{
    // http://anglebug.com/4092
    ANGLE_SKIP_TEST_IF(isSwiftshader());
    GLSampler sampler;

    // Exact min
    validateValidAnisotropy(sampler, 1.0f);

    GLfloat maxValue = 0.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxValue);

    // Max value
    validateValidAnisotropy(sampler, maxValue - 1);

    // In-between
    GLfloat between = (1.0f + maxValue) / 2;
    validateValidAnisotropy(sampler, between);
}

// Verify an error is thrown if we try to go under the minimum value for
// GL_TEXTURE_MAX_ANISOTROPY_EXT
TEST_P(SamplersTest, InvalidUnderTextureSamplerMaxAnisotropyExt)
{
    // http://anglebug.com/4092
    ANGLE_SKIP_TEST_IF(isSwiftshader());
    GLSampler sampler;

    // Under min
    validateInvalidAnisotropy(sampler, 0.0f);
}

// Verify an error is thrown if we try to go over the max value for
// GL_TEXTURE_MAX_ANISOTROPY_EXT
TEST_P(SamplersTest, InvalidOverTextureSamplerMaxAnisotropyExt)
{
    // http://anglebug.com/4092
    ANGLE_SKIP_TEST_IF(isSwiftshader());
    GLSampler sampler;

    GLfloat maxValue = 0.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxValue);
    maxValue += 1;

    validateInvalidAnisotropy(sampler, maxValue);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
// Samplers are only supported on ES3.
ANGLE_INSTANTIATE_TEST_ES3(SamplersTest);
}  // namespace angle
