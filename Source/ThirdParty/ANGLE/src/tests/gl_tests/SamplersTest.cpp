//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SamplerTest.cpp : Tests for samplers.

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "gtest/gtest.h"
#include "test_utils/ANGLETest.h"

#include "test_utils/angle_test_configs.h"
#include "test_utils/gl_raii.h"
#include "util/shader_utils.h"

namespace angle
{

using BasicSamplersTest = ANGLETest<>;

// Basic sampler test.
TEST_P(BasicSamplersTest, SampleATexture)
{
    constexpr int kWidth  = 2;
    constexpr int kHeight = 2;

    const GLchar *vertString = R"(precision highp float;
attribute vec2 a_position;
varying vec2 texCoord;
void main()
{
    gl_Position = vec4(a_position.x, a_position.y, 0.0, 1.0);
    texCoord = a_position * 0.5 + vec2(0.5);
})";

    const GLchar *fragString = R"(precision highp float;
varying vec2 texCoord;
uniform sampler2D tex;
void main()
{
    gl_FragColor = texture2D(tex, texCoord);
})";

    std::array<GLColor, kWidth * kHeight> redColor = {
        {GLColor::red, GLColor::red, GLColor::red, GLColor::red}};
    std::array<GLColor, kWidth * kHeight> greenColor = {
        {GLColor::green, GLColor::green, GLColor::green, GLColor::green}};

    // Create a red texture and bind to texture unit 0
    GLTexture redTex;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, redTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 redColor.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ASSERT_GL_NO_ERROR();
    // Create a green texture and bind to texture unit 1
    GLTexture greenTex;
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, greenTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 greenColor.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glActiveTexture(GL_TEXTURE0);
    ASSERT_GL_NO_ERROR();

    GLProgram program;
    program.makeRaster(vertString, fragString);
    ASSERT_NE(0u, program);
    glUseProgram(program);

    GLint location = glGetUniformLocation(program, "tex");
    ASSERT_NE(location, -1);
    ASSERT_GL_NO_ERROR();

    // Draw red
    glUniform1i(location, 0);
    ASSERT_GL_NO_ERROR();
    drawQuad(program, "a_position", 0.5f);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_RECT_EQ(0, 0, kWidth, kHeight, GLColor::red);

    // Draw green
    glUniform1i(location, 1);
    ASSERT_GL_NO_ERROR();
    drawQuad(program, "a_position", 0.5f);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_RECT_EQ(0, 0, kWidth, kHeight, GLColor::green);
}

class SampleFromRenderedTextureTest : public ANGLETest<>
{
  protected:
    const GLchar *vertString  = R"(precision highp float;
attribute vec2 a_position;
varying vec2 texCoord;
void main()
{
    gl_Position = vec4(a_position.x, a_position.y, 0.0, 1.0);
    texCoord = a_position * 0.5 + vec2(0.5);
})";
    const GLchar *vertString2 = R"(precision highp float;
attribute vec2 a_position;
varying vec2 texCoord;
void main()
{
    gl_Position = vec4(a_position.x, a_position.y, 0.0, 1.0);
    texCoord = a_position * 0.25 + vec2(0.5);
})";

    const GLchar *fragString = R"(precision highp float;
varying vec2 texCoord;
uniform sampler2D tex;
void main()
{
    gl_FragColor = texture2D(tex, texCoord);
})";

    virtual GLsizei getTextureWidth()    = 0;
    virtual GLsizei getTextureHeight()   = 0;
    virtual GLsizei getViewportOriginX() = 0;
    virtual GLsizei getViewportOriginY() = 0;

    void testSetUp() override
    {
        glViewport(getViewportOriginX(), getViewportOriginY(), getTextureWidth(),
                   getTextureHeight());
        ANGLETest<>::testSetUp();
    }

    GLuint createGradientTexture()
    {
        GLuint gradientTex;
        glGenTextures(1, &gradientTex);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gradientTex);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        std::vector<GLubyte> gradientPixels(getTextureWidth() * getTextureHeight() * 4);
        for (GLubyte y = 0; y < getTextureHeight(); y++)
        {
            for (GLubyte x = 0; x < getTextureWidth(); x++)
            {
                GLubyte *pixel = &gradientPixels[0] + ((y * getTextureWidth() + x) * 4);

                // Draw a gradient, red in x direction, green in y direction
                pixel[0] = x;
                pixel[1] = y;
                pixel[2] = 0u;
                pixel[3] = 255u;
            }
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, getTextureWidth(), getTextureHeight(), 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, gradientPixels.data());
        EXPECT_GL_NO_ERROR();

        glBindTexture(GL_TEXTURE_2D, 0);

        return gradientTex;
    }

    void installProgram(bool halfScreen)
    {
        if (halfScreen)
        {
            mProgram.makeRaster(vertString2, fragString);
            ASSERT_NE(0u, mProgram);
            glUseProgram(mProgram);
        }
        else
        {
            mProgram.makeRaster(vertString, fragString);
            ASSERT_NE(0u, mProgram);
            glUseProgram(mProgram);
        }
    }

    void createBoundFramebufferWithColorAttachment(GLuint *fboId, GLuint *colorAttachment)
    {
        // Create a texture to use as the non-default FBO's color attachment.
        glGenTextures(1, colorAttachment);
        glBindTexture(GL_TEXTURE_2D, *colorAttachment);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, getWindowWidth(), getWindowHeight(), 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, NULL);
        EXPECT_GL_NO_ERROR();

        // Create the non-default fbo
        if (fboId)
        {
            glGenFramebuffers(1, fboId);
            glBindFramebuffer(GL_FRAMEBUFFER, *fboId);
            EXPECT_GL_NO_ERROR();
        }

        // Attach the texture to the fbo
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               *colorAttachment, 0);
        ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_FRAMEBUFFER));
        ASSERT_GL_NO_ERROR();

        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void bindActiveTextureToProgram(GLuint activeTextureUnit, GLuint tex)
    {
        GLint texLocation = glGetUniformLocation(mProgram, "tex");
        ASSERT_NE(texLocation, -1);
        ASSERT_GL_NO_ERROR();

        glActiveTexture(GL_TEXTURE0 + activeTextureUnit);
        glBindTexture(GL_TEXTURE_2D, tex);
        ASSERT_GL_NO_ERROR();
        glUniform1i(texLocation, activeTextureUnit);
        ASSERT_GL_NO_ERROR();
    }

    void drawAndCheckGradient(bool strict)
    {
        drawQuad(mProgram, "a_position", 0.5f);
        ASSERT_GL_NO_ERROR();

        std::vector<GLubyte> pixels(getTextureWidth() * getTextureHeight() * 4);
        glReadPixels(getViewportOriginX(), getViewportOriginY(), getTextureWidth(),
                     getTextureHeight(), GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        ASSERT_GL_NO_ERROR();

        // Check the pixels match the gradient.
        size_t checkWidth  = getTextureWidth();
        size_t checkHeight = getTextureHeight();
        if (!strict)
        {
            // Don't check the last row or column if not strict.
            checkWidth--;
            checkHeight--;
        }
        for (size_t y = 1; y < checkHeight; y++)
        {
            for (size_t x = 1; x < checkWidth; x++)
            {
                const GLubyte *prevPixel =
                    pixels.data() + (((y - 1) * getTextureWidth() + (x - 1)) * 4);
                const GLubyte *curPixel = pixels.data() + ((y * getTextureWidth() + x) * 4);

                if (strict)
                {
                    EXPECT_EQ(curPixel[0], prevPixel[0] + 1)
                        << " failed at (" << x << ", " << y << ")";
                    EXPECT_EQ(curPixel[1], prevPixel[1] + 1)
                        << " failed at (" << x << ", " << y << ")";
                }
                else
                {
                    EXPECT_GE(curPixel[0], prevPixel[0]) << " failed at (" << x << ", " << y << ")";
                    EXPECT_GE(curPixel[1], prevPixel[1]) << " failed at (" << x << ", " << y << ")";
                }
                EXPECT_EQ(curPixel[2], prevPixel[2]);
                EXPECT_EQ(curPixel[3], prevPixel[3]);
            }
        }
    }

    GLProgram mProgram;
};

class SampleFromRenderedTextureTestHalfWindow : public SampleFromRenderedTextureTest
{
  protected:
    static constexpr GLsizei kTextureWidth  = 255;
    static constexpr GLsizei kTextureHeight = 255;

    static constexpr GLsizei kViewportOriginX = kTextureWidth / 2;
    static constexpr GLsizei kViewportOriginY = kTextureHeight / 2;

    static constexpr GLsizei kWindowWidth  = kTextureWidth * 2;
    static constexpr GLsizei kWindowHeight = kTextureHeight * 2;

    GLsizei getTextureWidth() override { return kTextureWidth; }
    GLsizei getTextureHeight() override { return kTextureHeight; }
    GLsizei getViewportOriginX() override { return kViewportOriginX; }
    GLsizei getViewportOriginY() override { return kViewportOriginY; }

    SampleFromRenderedTextureTestHalfWindow()
    {
        setWindowWidth(kWindowWidth);
        setWindowHeight(kWindowHeight);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }
};

// Renders a gradient to a texture (twice the size) attached to an FBO, then samples from that
// texture in a second pass, effectively copying the gradient to the middle of the default
// framebuffer. Tests that the gradient remains intact.
TEST_P(SampleFromRenderedTextureTestHalfWindow, RenderToTextureAndSampleFromIt)
{
    // Create a gradient texture to use as the original source texture.
    GLuint gradientTex = createGradientTexture();

    installProgram(/*halfScreen=*/false);

    GLuint fboId;
    GLuint fboTextureAttachment;
    createBoundFramebufferWithColorAttachment(&fboId, &fboTextureAttachment);

    // The source texture used by the fragment shader should be the gradient texture.
    bindActiveTextureToProgram(0, gradientTex);

    drawAndCheckGradient(/*strict=*/true);

    // Sample from the texture only in the current viewport (half the screen).
    installProgram(/*halfScreen=*/true);

    // Now bind the default framebuffer.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    ASSERT_GL_NO_ERROR();

    // Use the texture attached to the first framebuffer as the source texture for this draw call.
    bindActiveTextureToProgram(0, fboTextureAttachment);

    // Draw and check the pixels, but in the default framebuffer
    drawAndCheckGradient(/*strict=*/false);
}

class SampleFromRenderedTextureTestFullWindow : public SampleFromRenderedTextureTest
{
  protected:
    static constexpr GLsizei kTextureWidth  = 255;
    static constexpr GLsizei kTextureHeight = 255;

    static constexpr GLsizei kViewportOriginX = 0;
    static constexpr GLsizei kViewportOriginY = 0;

    static constexpr GLsizei kWindowWidth  = kTextureWidth;
    static constexpr GLsizei kWindowHeight = kTextureHeight;

    GLsizei getTextureWidth() override { return kTextureWidth; }
    GLsizei getTextureHeight() override { return kTextureHeight; }
    GLsizei getViewportOriginX() override { return kViewportOriginX; }
    GLsizei getViewportOriginY() override { return kViewportOriginY; }

    SampleFromRenderedTextureTestFullWindow()
    {
        setWindowWidth(kWindowWidth);
        setWindowHeight(kWindowHeight);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }
};

// Renders a gradient to a texture attached to an FBO, then samples from that texture in a second
// pass, effectively copying the gradient to the default framebuffer. Tests that the gradient
// remains intact.
TEST_P(SampleFromRenderedTextureTestFullWindow, RenderToTextureAndSampleFromIt)
{
    // Setup the program.
    installProgram(/*halfScreen=*/false);

    // Create a gradient texture to use as the original source texture.
    GLuint gradientTex = createGradientTexture();

    // Create a texture to use as the non-default FBO's color attachment.
    GLuint fboId;
    GLuint fboTextureAttachment;
    createBoundFramebufferWithColorAttachment(&fboId, &fboTextureAttachment);

    bindActiveTextureToProgram(0, gradientTex);

    drawAndCheckGradient(/*strict=*/true);

    // Now bind the default framebuffer.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    ASSERT_GL_NO_ERROR();

    // Use the texture attached to the first framebuffer as the source texture for this draw call.
    bindActiveTextureToProgram(0, fboTextureAttachment);

    drawAndCheckGradient(/*strict=*/true);
}

// Renders a gradient to a texture attached to an FBO, then samples from that texture in a second
// pass rendering to another texture attached to the FBO. Finally that texture is rendered to the
// default framebuffer. Tests that the gradient remains intact.
TEST_P(SampleFromRenderedTextureTestFullWindow, RenderToTextureTwiceAndSampleFromIt)
{
    // Setup the program.
    installProgram(/*halfScreen=*/false);

    // Create a gradient texture to use as the original source texture.
    GLuint gradientTex = createGradientTexture();

    // Create a texture to use as the non-default FBO's color attachment.
    GLuint fboId;
    GLuint fboTextureAttachment;
    createBoundFramebufferWithColorAttachment(&fboId, &fboTextureAttachment);

    bindActiveTextureToProgram(0, gradientTex);

    drawAndCheckGradient(/*strict=*/true);

    // Create another texture to use as the non-default FBO's color attachment.
    GLuint fboTextureAttachment2;
    createBoundFramebufferWithColorAttachment(nullptr, &fboTextureAttachment2);

    // Use the texture attached to the first framebuffer as the source texture for this draw call.
    bindActiveTextureToProgram(0, fboTextureAttachment);

    drawAndCheckGradient(/*strict=*/true);

    // Now bind the default framebuffer.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    ASSERT_GL_NO_ERROR();

    // Use the second texture attached to the first framebuffer as the source texture for this draw
    // call.
    bindActiveTextureToProgram(0, fboTextureAttachment2);

    drawAndCheckGradient(/*strict=*/true);
}

class SamplersTest : public ANGLETest<>
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

class SamplersTest31 : public SamplersTest
{};

// Verify that samplerParameterf supports TEXTURE_MAX_ANISOTROPY_EXT valid values.
TEST_P(SamplersTest, ValidTextureSamplerMaxAnisotropyExt)
{
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
    GLSampler sampler;

    // Under min
    validateInvalidAnisotropy(sampler, 0.0f);
}

// Verify an error is thrown if we try to go over the max value for
// GL_TEXTURE_MAX_ANISOTROPY_EXT
TEST_P(SamplersTest, InvalidOverTextureSamplerMaxAnisotropyExt)
{
    GLSampler sampler;

    GLfloat maxValue = 0.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxValue);
    maxValue += 1;

    validateInvalidAnisotropy(sampler, maxValue);
}

// Test that updating a sampler uniform in a program behaves correctly.
TEST_P(SamplersTest31, SampleTextureAThenTextureB)
{
    ANGLE_SKIP_TEST_IF(!IsVulkan());

    constexpr int kWidth  = 2;
    constexpr int kHeight = 2;

    const GLchar *vertString = R"(#version 310 es
precision highp float;
in vec2 a_position;
out vec2 texCoord;
void main()
{
    gl_Position = vec4(a_position, 0, 1);
    texCoord = a_position * 0.5 + vec2(0.5);
})";

    const GLchar *fragString = R"(#version 310 es
precision highp float;
in vec2 texCoord;
uniform sampler2D tex;
out vec4 my_FragColor;
void main()
{
    my_FragColor = texture(tex, texCoord);
})";

    std::array<GLColor, kWidth * kHeight> redColor = {
        {GLColor::red, GLColor::red, GLColor::red, GLColor::red}};
    std::array<GLColor, kWidth * kHeight> greenColor = {
        {GLColor::green, GLColor::green, GLColor::green, GLColor::green}};

    // Create a red texture and bind to texture unit 0
    GLTexture redTex;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, redTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 redColor.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    ASSERT_GL_NO_ERROR();
    // Create a green texture and bind to texture unit 1
    GLTexture greenTex;
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, greenTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                 greenColor.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glActiveTexture(GL_TEXTURE0);
    ASSERT_GL_NO_ERROR();

    GLProgram program;
    program.makeRaster(vertString, fragString);
    ASSERT_NE(0u, program);
    glUseProgram(program);

    GLint location = glGetUniformLocation(program, "tex");
    ASSERT_NE(location, -1);
    ASSERT_GL_NO_ERROR();

    // Draw red
    glUniform1i(location, 0);
    ASSERT_GL_NO_ERROR();
    drawQuad(program, "a_position", 0.5f);
    ASSERT_GL_NO_ERROR();

    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_ONE, GL_ONE);

    // Draw green
    glUniform1i(location, 1);
    ASSERT_GL_NO_ERROR();
    drawQuad(program, "a_position", 0.5f);
    ASSERT_GL_NO_ERROR();

    // Draw red
    glUniform1i(location, 0);
    ASSERT_GL_NO_ERROR();
    drawQuad(program, "a_position", 0.5f);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_RECT_EQ(0, 0, kWidth, kHeight, GLColor::yellow);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(BasicSamplersTest);
ANGLE_INSTANTIATE_TEST_ES2(BasicSamplersTest);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SampleFromRenderedTextureTestHalfWindow);
ANGLE_INSTANTIATE_TEST_ES2(SampleFromRenderedTextureTestHalfWindow);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SampleFromRenderedTextureTestFullWindow);
ANGLE_INSTANTIATE_TEST_ES2(SampleFromRenderedTextureTestFullWindow);

// Samplers are only supported on ES3.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SamplersTest);
ANGLE_INSTANTIATE_TEST_ES3(SamplersTest);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SamplersTest31);
ANGLE_INSTANTIATE_TEST_ES31(SamplersTest31);
}  // namespace angle
