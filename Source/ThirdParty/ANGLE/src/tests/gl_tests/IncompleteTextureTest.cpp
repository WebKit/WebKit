//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETest.h"

#include <vector>
#include "test_utils/gl_raii.h"

using namespace angle;

class IncompleteTextureTest : public ANGLETest
{
  protected:
    IncompleteTextureTest()
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

        const std::string vertexShaderSource =
            R"(precision highp float;
            attribute vec4 position;
            varying vec2 texcoord;

            void main()
            {
                gl_Position = position;
                texcoord = (position.xy * 0.5) + 0.5;
            })";

        const std::string fragmentShaderSource =
            R"(precision highp float;
            uniform sampler2D tex;
            varying vec2 texcoord;

            void main()
            {
                gl_FragColor = texture2D(tex, texcoord);
            })";

        mProgram = CompileProgram(vertexShaderSource, fragmentShaderSource);
        if (mProgram == 0)
        {
            FAIL() << "shader compilation failed.";
        }

        mTextureUniformLocation = glGetUniformLocation(mProgram, "tex");
    }

    void TearDown() override
    {
        glDeleteProgram(mProgram);

        ANGLETest::TearDown();
    }

    GLuint mProgram;
    GLint mTextureUniformLocation;
};

class IncompleteTextureTestES31 : public ANGLETest
{
  protected:
    IncompleteTextureTestES31()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }
};

// Test rendering with an incomplete texture.
TEST_P(IncompleteTextureTest, IncompleteTexture2D)
{
    GLTexture tex;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    glUseProgram(mProgram);
    glUniform1i(mTextureUniformLocation, 0);

    constexpr GLsizei kTextureSize = 2;
    std::vector<GLColor> textureData(kTextureSize * kTextureSize, GLColor::red);

    // Make a complete texture.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kTextureSize, kTextureSize, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, textureData.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    // Should be complete - expect red.
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red) << "complete texture should be red";

    // Make texture incomplete.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

    // Should be incomplete - expect black.
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::black) << "incomplete texture should be black";

    // Make texture complete by defining the second mip.
    glTexImage2D(GL_TEXTURE_2D, 1, GL_RGBA, kTextureSize >> 1, kTextureSize >> 1, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, textureData.data());

    // Should be complete - expect red.
    drawQuad(mProgram, "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red) << "mip-complete texture should be red";
}

// Tests redefining a texture with half the size works as expected.
TEST_P(IncompleteTextureTest, UpdateTexture)
{
    GLTexture tex;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);

    glUseProgram(mProgram);
    glUniform1i(mTextureUniformLocation, 0);

    constexpr GLsizei redTextureSize = 64;
    std::vector<GLColor> redTextureData(redTextureSize * redTextureSize, GLColor::red);
    for (GLint mip = 0; mip < 7; ++mip)
    {
        const GLsizei mipSize = redTextureSize >> mip;

        glTexImage2D(GL_TEXTURE_2D, mip, GL_RGBA, mipSize, mipSize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     redTextureData.data());
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    drawQuad(mProgram, "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    constexpr GLsizei greenTextureSize = 32;
    std::vector<GLColor> greenTextureData(greenTextureSize * greenTextureSize, GLColor::green);

    for (GLint mip = 0; mip < 6; ++mip)
    {
        const GLsizei mipSize = greenTextureSize >> mip;

        glTexSubImage2D(GL_TEXTURE_2D, mip, mipSize, mipSize, mipSize, mipSize, GL_RGBA,
                        GL_UNSIGNED_BYTE, greenTextureData.data());
    }

    drawQuad(mProgram, "position", 0.5f);
    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() - greenTextureSize, getWindowHeight() - greenTextureSize,
                          GLColor::green);
}

// Tests that the incomplete multisample texture has the correct alpha value.
TEST_P(IncompleteTextureTestES31, MultisampleTexture)
{
    const std::string vertexShader = R"(#version 310 es
in vec2 position;
out vec2 texCoord;
void main()
{
    gl_Position = vec4(position, 0, 1);
    texCoord = (position * 0.5) + 0.5;
}
)";

    const std::string fragmentShader = R"(#version 310 es
precision mediump float;
in vec2 texCoord;
out vec4 color;
uniform mediump sampler2DMS tex;
void main()
{
    ivec2 texSize = textureSize(tex);
    ivec2 texel = ivec2(vec2(texSize) * texCoord);
    color = texelFetch(tex, texel, 0);
}
)";

    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    // The zero texture will be incomplete by default.
    ANGLE_GL_PROGRAM(program, vertexShader, fragmentShader);
    drawQuad(program, "position", 0.5f);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::black);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these tests should be run against.
ANGLE_INSTANTIATE_TEST(IncompleteTextureTest,
                       ES2_D3D9(),
                       ES2_D3D11(),
                       ES2_OPENGL(),
                       ES2_OPENGLES());

ANGLE_INSTANTIATE_TEST(IncompleteTextureTestES31, ES31_D3D11(), ES31_OPENGL(), ES31_OPENGLES());
