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

    void testSetUp() override
    {
        constexpr char kVS[] = R"(precision highp float;
attribute vec4 position;
varying vec2 texcoord;

void main()
{
    gl_Position = position;
    texcoord = (position.xy * 0.5) + 0.5;
})";

        constexpr char kFS[] = R"(precision highp float;
uniform sampler2D tex;
varying vec2 texcoord;

void main()
{
    gl_FragColor = texture2D(tex, texcoord);
})";

        mProgram = CompileProgram(kVS, kFS);
        if (mProgram == 0)
        {
            FAIL() << "shader compilation failed.";
        }

        mTextureUniformLocation = glGetUniformLocation(mProgram, "tex");
    }

    void testTearDown() override { glDeleteProgram(mProgram); }

    GLuint mProgram;
    GLint mTextureUniformLocation;
};

class IncompleteTextureTestES3 : public ANGLETest
{
  protected:
    IncompleteTextureTestES3()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }
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

// Tests that incomplete textures don't get initialized with the unpack buffer contents.
TEST_P(IncompleteTextureTestES3, UnpackBufferBound)
{
    // http://anglebug.com/4092
    ANGLE_SKIP_TEST_IF(IsVulkan());
    std::vector<GLColor> red(16, GLColor::red);

    GLBuffer unpackBuffer;
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpackBuffer);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, red.size() * sizeof(GLColor), red.data(), GL_STATIC_DRAW);

    draw2DTexturedQuad(0.5f, 1.0f, true);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::black);
}

// Tests that the incomplete multisample texture has the correct alpha value.
TEST_P(IncompleteTextureTestES31, MultisampleTexture)
{
    // http://anglebug.com/4092
    ANGLE_SKIP_TEST_IF(IsVulkan());
    constexpr char kVS[] = R"(#version 310 es
in vec2 position;
out vec2 texCoord;
void main()
{
    gl_Position = vec4(position, 0, 1);
    texCoord = (position * 0.5) + 0.5;
})";

    constexpr char kFS[] = R"(#version 310 es
precision mediump float;
in vec2 texCoord;
out vec4 color;
uniform mediump sampler2DMS tex;
void main()
{
    ivec2 texSize = textureSize(tex);
    ivec2 texel = ivec2(vec2(texSize) * texCoord);
    color = texelFetch(tex, texel, 0);
})";

    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::red);

    // The zero texture will be incomplete by default.
    ANGLE_GL_PROGRAM(program, kVS, kFS);
    drawQuad(program, "position", 0.5f);
    ASSERT_GL_NO_ERROR();

    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::black);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST_ES2(IncompleteTextureTest);
ANGLE_INSTANTIATE_TEST_ES3(IncompleteTextureTestES3);
ANGLE_INSTANTIATE_TEST_ES31(IncompleteTextureTestES31);
