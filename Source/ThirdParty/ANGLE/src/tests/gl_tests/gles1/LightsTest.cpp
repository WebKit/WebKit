//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// LightsTest.cpp: Tests basic usage of glLight*.

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

#include "common/matrix_utils.h"
#include "common/vector_utils.h"
#include "util/random_utils.h"

#include <stdint.h>

#include <vector>

using namespace angle;

class LightsTest : public ANGLETest
{
  protected:
    LightsTest()
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

// Check that the initial lighting parameters state is correct,
// including spec minimum for light count.
TEST_P(LightsTest, InitialState)
{
    const GLColor32F kAmbientInitial(0.2f, 0.2f, 0.2f, 1.0f);
    GLboolean kLightModelTwoSideInitial = GL_FALSE;

    GLColor32F lightModelAmbient;
    GLboolean lightModelTwoSide;

    glGetFloatv(GL_LIGHT_MODEL_AMBIENT, &lightModelAmbient.R);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(kAmbientInitial, lightModelAmbient);

    glGetBooleanv(GL_LIGHT_MODEL_TWO_SIDE, &lightModelTwoSide);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(kLightModelTwoSideInitial, lightModelTwoSide);

    EXPECT_GL_FALSE(glIsEnabled(GL_LIGHTING));
    EXPECT_GL_NO_ERROR();
    EXPECT_GL_FALSE(glIsEnabled(GL_NORMALIZE));
    EXPECT_GL_NO_ERROR();
    EXPECT_GL_FALSE(glIsEnabled(GL_RESCALE_NORMAL));
    EXPECT_GL_NO_ERROR();
    EXPECT_GL_FALSE(glIsEnabled(GL_COLOR_MATERIAL));
    EXPECT_GL_NO_ERROR();

    GLint maxLights = 0;
    glGetIntegerv(GL_MAX_LIGHTS, &maxLights);
    EXPECT_GL_NO_ERROR();
    EXPECT_GE(8, maxLights);

    const GLColor32F kLightnAmbient(0.0f, 0.0f, 0.0f, 1.0f);
    const GLColor32F kLightnDiffuse(0.0f, 0.0f, 0.0f, 1.0f);
    const GLColor32F kLightnSpecular(0.0f, 0.0f, 0.0f, 1.0f);
    const GLColor32F kLight0Diffuse(1.0f, 1.0f, 1.0f, 1.0f);
    const GLColor32F kLight0Specular(1.0f, 1.0f, 1.0f, 1.0f);
    const angle::Vector4 kLightnPosition(0.0f, 0.0f, 1.0f, 0.0f);
    const angle::Vector3 kLightnDirection(0.0f, 0.0f, -1.0f);
    const GLfloat kLightnSpotlightExponent    = 0.0f;
    const GLfloat kLightnSpotlightCutoffAngle = 180.0f;
    const GLfloat kLightnAttenuationConst     = 1.0f;
    const GLfloat kLightnAttenuationLinear    = 0.0f;
    const GLfloat kLightnAttenuationQuadratic = 0.0f;

    for (int i = 0; i < maxLights; i++)
    {
        EXPECT_GL_FALSE(glIsEnabled(GL_LIGHT0 + i));
        EXPECT_GL_NO_ERROR();

        GLColor32F actualColor;
        angle::Vector4 actualPosition;
        angle::Vector3 actualDirection;
        GLfloat actualFloatValue;

        glGetLightfv(GL_LIGHT0 + i, GL_AMBIENT, &actualColor.R);
        EXPECT_GL_NO_ERROR();
        EXPECT_EQ(kLightnAmbient, actualColor);

        glGetLightfv(GL_LIGHT0 + i, GL_DIFFUSE, &actualColor.R);
        EXPECT_GL_NO_ERROR();
        EXPECT_EQ(i == 0 ? kLight0Diffuse : kLightnDiffuse, actualColor);

        glGetLightfv(GL_LIGHT0 + i, GL_SPECULAR, &actualColor.R);
        EXPECT_GL_NO_ERROR();
        EXPECT_EQ(i == 0 ? kLight0Specular : kLightnSpecular, actualColor);

        glGetLightfv(GL_LIGHT0 + i, GL_POSITION, actualPosition.data());
        EXPECT_GL_NO_ERROR();
        EXPECT_EQ(kLightnPosition, actualPosition);

        glGetLightfv(GL_LIGHT0 + i, GL_SPOT_DIRECTION, actualDirection.data());
        EXPECT_GL_NO_ERROR();
        EXPECT_EQ(kLightnDirection, actualDirection);

        glGetLightfv(GL_LIGHT0 + i, GL_SPOT_EXPONENT, &actualFloatValue);
        EXPECT_GL_NO_ERROR();
        EXPECT_EQ(kLightnSpotlightExponent, actualFloatValue);

        glGetLightfv(GL_LIGHT0 + i, GL_SPOT_CUTOFF, &actualFloatValue);
        EXPECT_GL_NO_ERROR();
        EXPECT_EQ(kLightnSpotlightCutoffAngle, actualFloatValue);

        glGetLightfv(GL_LIGHT0 + i, GL_CONSTANT_ATTENUATION, &actualFloatValue);
        EXPECT_GL_NO_ERROR();
        EXPECT_EQ(kLightnAttenuationConst, actualFloatValue);

        glGetLightfv(GL_LIGHT0 + i, GL_LINEAR_ATTENUATION, &actualFloatValue);
        EXPECT_GL_NO_ERROR();
        EXPECT_EQ(kLightnAttenuationLinear, actualFloatValue);

        glGetLightfv(GL_LIGHT0 + i, GL_QUADRATIC_ATTENUATION, &actualFloatValue);
        EXPECT_GL_NO_ERROR();
        EXPECT_EQ(kLightnAttenuationQuadratic, actualFloatValue);
    }
}

// Negative test for invalid parameter names.
TEST_P(LightsTest, NegativeInvalidEnum)
{
    GLint maxLights = 0;
    glGetIntegerv(GL_MAX_LIGHTS, &maxLights);

    glIsEnabled(GL_LIGHT0 + maxLights);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    glLightfv(GL_LIGHT0 + maxLights, GL_AMBIENT, nullptr);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    glLightModelfv(GL_LIGHT0, nullptr);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    glLightModelf(GL_LIGHT0, 0.0f);
    EXPECT_GL_ERROR(GL_INVALID_ENUM);

    for (int i = 0; i < maxLights; i++)
    {
        glLightf(GL_LIGHT0 + i, GL_TEXTURE_2D, 0.0f);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);

        glLightfv(GL_LIGHT0 + i, GL_TEXTURE_2D, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);
    }
}

// Negative test for invalid parameter values.
TEST_P(LightsTest, NegativeInvalidValue)
{
    GLint maxLights = 0;
    glGetIntegerv(GL_MAX_LIGHTS, &maxLights);

    std::vector<GLenum> attenuationParams = {
        GL_CONSTANT_ATTENUATION,
        GL_LINEAR_ATTENUATION,
        GL_QUADRATIC_ATTENUATION,
    };

    for (int i = 0; i < maxLights; i++)
    {
        glLightf(GL_LIGHT0 + i, GL_SPOT_EXPONENT, -1.0f);
        EXPECT_GL_ERROR(GL_INVALID_VALUE);
        GLfloat previousVal = -1.0f;
        glGetLightfv(GL_LIGHT0 + i, GL_SPOT_EXPONENT, &previousVal);
        EXPECT_NE(-1.0f, previousVal);

        glLightf(GL_LIGHT0 + i, GL_SPOT_EXPONENT, 128.1f);
        EXPECT_GL_ERROR(GL_INVALID_VALUE);
        previousVal = 128.1f;
        glGetLightfv(GL_LIGHT0 + i, GL_SPOT_EXPONENT, &previousVal);
        EXPECT_NE(128.1f, previousVal);

        glLightf(GL_LIGHT0 + i, GL_SPOT_CUTOFF, -1.0f);
        EXPECT_GL_ERROR(GL_INVALID_VALUE);
        previousVal = -1.0f;
        glGetLightfv(GL_LIGHT0 + i, GL_SPOT_CUTOFF, &previousVal);
        EXPECT_NE(-1.0f, previousVal);

        glLightf(GL_LIGHT0 + i, GL_SPOT_CUTOFF, 90.1f);
        EXPECT_GL_ERROR(GL_INVALID_VALUE);
        previousVal = 90.1f;
        glGetLightfv(GL_LIGHT0 + i, GL_SPOT_CUTOFF, &previousVal);
        EXPECT_NE(90.1f, previousVal);

        for (GLenum pname : attenuationParams)
        {
            glLightf(GL_LIGHT0 + i, pname, -1.0f);
            EXPECT_GL_ERROR(GL_INVALID_VALUE);
            previousVal = -1.0f;
            glGetLightfv(GL_LIGHT0 + i, pname, &previousVal);
            EXPECT_NE(-1.0f, previousVal);
        }
    }
}

// Test to see if we can set and retrieve the light parameters.
TEST_P(LightsTest, Set)
{
    angle::RNG rng(0);

    GLint maxLights = 0;
    glGetIntegerv(GL_MAX_LIGHTS, &maxLights);

    constexpr int kNumTrials = 100;

    GLColor32F actualColor;
    angle::Vector4 actualPosition;
    angle::Vector3 actualDirection;
    GLfloat actualFloatValue;
    GLboolean actualBooleanValue;

    for (int k = 0; k < kNumTrials; ++k)
    {
        const GLColor32F lightModelAmbient(rng.randomFloat(), rng.randomFloat(), rng.randomFloat(),
                                           rng.randomFloat());
        const GLfloat lightModelTwoSide = rng.randomBool() ? 1.0f : 0.0f;

        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, &lightModelAmbient.R);
        EXPECT_GL_NO_ERROR();
        glGetFloatv(GL_LIGHT_MODEL_AMBIENT, &actualColor.R);
        EXPECT_EQ(lightModelAmbient, actualColor);

        glLightModelf(GL_LIGHT_MODEL_TWO_SIDE, lightModelTwoSide);
        EXPECT_GL_NO_ERROR();
        glGetFloatv(GL_LIGHT_MODEL_TWO_SIDE, &actualFloatValue);
        EXPECT_EQ(lightModelTwoSide, actualFloatValue);
        glGetBooleanv(GL_LIGHT_MODEL_TWO_SIDE, &actualBooleanValue);
        EXPECT_EQ(lightModelTwoSide == 1.0f ? GL_TRUE : GL_FALSE, actualBooleanValue);

        for (int i = 0; i < maxLights; i++)
        {

            const GLColor32F ambient(rng.randomFloat(), rng.randomFloat(), rng.randomFloat(),
                                     rng.randomFloat());
            const GLColor32F diffuse(rng.randomFloat(), rng.randomFloat(), rng.randomFloat(),
                                     rng.randomFloat());
            const GLColor32F specular(rng.randomFloat(), rng.randomFloat(), rng.randomFloat(),
                                      rng.randomFloat());
            const angle::Vector4 position(rng.randomFloat(), rng.randomFloat(), rng.randomFloat(),
                                          rng.randomFloat());
            const angle::Vector3 direction(rng.randomFloat(), rng.randomFloat(), rng.randomFloat());
            const GLfloat spotlightExponent = rng.randomFloatBetween(0.0f, 128.0f);
            const GLfloat spotlightCutoffAngle =
                rng.randomBool() ? rng.randomFloatBetween(0.0f, 90.0f) : 180.0f;
            const GLfloat attenuationConst     = rng.randomFloatNonnegative();
            const GLfloat attenuationLinear    = rng.randomFloatNonnegative();
            const GLfloat attenuationQuadratic = rng.randomFloatNonnegative();

            glLightfv(GL_LIGHT0 + i, GL_AMBIENT, &ambient.R);
            EXPECT_GL_NO_ERROR();
            glGetLightfv(GL_LIGHT0 + i, GL_AMBIENT, &actualColor.R);
            EXPECT_EQ(ambient, actualColor);

            glLightfv(GL_LIGHT0 + i, GL_DIFFUSE, &diffuse.R);
            EXPECT_GL_NO_ERROR();
            glGetLightfv(GL_LIGHT0 + i, GL_DIFFUSE, &actualColor.R);
            EXPECT_EQ(diffuse, actualColor);

            glLightfv(GL_LIGHT0 + i, GL_SPECULAR, &specular.R);
            EXPECT_GL_NO_ERROR();
            glGetLightfv(GL_LIGHT0 + i, GL_SPECULAR, &actualColor.R);
            EXPECT_EQ(specular, actualColor);

            glLightfv(GL_LIGHT0 + i, GL_POSITION, position.data());
            EXPECT_GL_NO_ERROR();
            glGetLightfv(GL_LIGHT0 + i, GL_POSITION, actualPosition.data());
            EXPECT_EQ(position, actualPosition);

            glLightfv(GL_LIGHT0 + i, GL_SPOT_DIRECTION, direction.data());
            EXPECT_GL_NO_ERROR();
            glGetLightfv(GL_LIGHT0 + i, GL_SPOT_DIRECTION, actualDirection.data());
            EXPECT_EQ(direction, actualDirection);

            glLightfv(GL_LIGHT0 + i, GL_SPOT_EXPONENT, &spotlightExponent);
            EXPECT_GL_NO_ERROR();
            glGetLightfv(GL_LIGHT0 + i, GL_SPOT_EXPONENT, &actualFloatValue);
            EXPECT_EQ(spotlightExponent, actualFloatValue);

            glLightfv(GL_LIGHT0 + i, GL_SPOT_CUTOFF, &spotlightCutoffAngle);
            EXPECT_GL_NO_ERROR();
            glGetLightfv(GL_LIGHT0 + i, GL_SPOT_CUTOFF, &actualFloatValue);
            EXPECT_EQ(spotlightCutoffAngle, actualFloatValue);

            glLightfv(GL_LIGHT0 + i, GL_CONSTANT_ATTENUATION, &attenuationConst);
            EXPECT_GL_NO_ERROR();
            glGetLightfv(GL_LIGHT0 + i, GL_CONSTANT_ATTENUATION, &actualFloatValue);
            EXPECT_EQ(attenuationConst, actualFloatValue);

            glLightfv(GL_LIGHT0 + i, GL_LINEAR_ATTENUATION, &attenuationLinear);
            EXPECT_GL_NO_ERROR();
            glGetLightfv(GL_LIGHT0 + i, GL_LINEAR_ATTENUATION, &actualFloatValue);
            EXPECT_EQ(attenuationLinear, actualFloatValue);

            glLightfv(GL_LIGHT0 + i, GL_LINEAR_ATTENUATION, &attenuationQuadratic);
            EXPECT_GL_NO_ERROR();
            glGetLightfv(GL_LIGHT0 + i, GL_LINEAR_ATTENUATION, &actualFloatValue);
            EXPECT_EQ(attenuationQuadratic, actualFloatValue);
        }
    }
}

ANGLE_INSTANTIATE_TEST(LightsTest, ES1_D3D11(), ES1_OPENGL(), ES1_OPENGLES());
