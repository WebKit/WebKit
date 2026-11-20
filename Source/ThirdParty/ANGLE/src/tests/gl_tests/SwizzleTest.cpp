//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "common/unsafe_buffers.h"
#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

#include <vector>

using namespace angle;

namespace
{

class SwizzleTest : public ANGLETest<>
{
  protected:
    SwizzleTest()
    {
        setWindowWidth(128);
        setWindowHeight(128);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);

        constexpr GLenum swizzles[] = {
            GL_RED, GL_GREEN, GL_BLUE, GL_ALPHA, GL_ZERO, GL_ONE,
        };

        // Only use every 13th swizzle permutation, use a prime number to make sure the permuations
        // are somewhat evenly distributed.  Reduces the permuations from 1296 to 100.
        constexpr size_t swizzleReductionFactor = 13;

        size_t swizzleCount = 0;
        for (GLenum r : swizzles)
        {
            for (GLenum g : swizzles)
            {
                for (GLenum b : swizzles)
                {
                    for (GLenum a : swizzles)
                    {
                        swizzleCount++;
                        if (swizzleCount % swizzleReductionFactor != 0)
                        {
                            continue;
                        }

                        swizzlePermutation permutation;
                        permutation.swizzleRed   = r;
                        permutation.swizzleGreen = g;
                        permutation.swizzleBlue  = b;
                        permutation.swizzleAlpha = a;
                        mPermutations.push_back(permutation);
                    }
                }
            }
        }
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
        ASSERT_NE(0u, mProgram);

        mTextureUniformLocation = glGetUniformLocation(mProgram, "tex");
        ASSERT_NE(-1, mTextureUniformLocation);

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        ASSERT_GL_NO_ERROR();
    }

    void testTearDown() override
    {
        glDeleteProgram(mProgram);
        glDeleteTextures(1, &mTexture);
    }

    bool isTextureSwizzleAvailable() const
    {
        // On Metal back-end, texture swizzle is not always supported.
        return !IsMetal() || IsMetalTextureSwizzleAvailable();
    }

    template <typename T>
    void init2DTexture(GLenum internalFormat, GLenum dataFormat, GLenum dataType, const T *data)
    {
        glGenTextures(1, &mTexture);
        glBindTexture(GL_TEXTURE_2D, mTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, 1, 1, 0, dataFormat, dataType, data);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    void init2DCompressedTexture(GLenum internalFormat,
                                 GLsizei width,
                                 GLsizei height,
                                 GLsizei dataSize,
                                 const GLubyte *data)
    {
        glGenTextures(1, &mTexture);
        glBindTexture(GL_TEXTURE_2D, mTexture);
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataSize, data);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    GLubyte getExpectedValue(GLenum swizzle, const std::array<GLubyte, 4> &unswizzled)
    {
        switch (swizzle)
        {
            case GL_RED:
                return unswizzled[0];
            case GL_GREEN:
                return unswizzled[1];
            case GL_BLUE:
                return unswizzled[2];
            case GL_ALPHA:
                return unswizzled[3];
            case GL_ZERO:
                return 0;
            case GL_ONE:
                return 255;
            default:
                return 0;
        }
    }

    void runTest2D()
    {
        glUseProgram(mProgram);
        glBindTexture(GL_TEXTURE_2D, mTexture);
        glUniform1i(mTextureUniformLocation, 0);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_GREEN);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_BLUE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_ALPHA);

        glClear(GL_COLOR_BUFFER_BIT);
        drawQuad(mProgram, "position", 0.5f);

        std::array<GLubyte, 4> unswizzled;
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, unswizzled.data());

        ASSERT_GL_NO_ERROR();

        for (const auto &permutation : mPermutations)
        {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, permutation.swizzleRed);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, permutation.swizzleGreen);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, permutation.swizzleBlue);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, permutation.swizzleAlpha);

            glClear(GL_COLOR_BUFFER_BIT);
            drawQuad(mProgram, "position", 0.5f);

            EXPECT_PIXEL_EQ(0, 0, getExpectedValue(permutation.swizzleRed, unswizzled),
                            getExpectedValue(permutation.swizzleGreen, unswizzled),
                            getExpectedValue(permutation.swizzleBlue, unswizzled),
                            getExpectedValue(permutation.swizzleAlpha, unswizzled));

            ASSERT_GL_NO_ERROR();
        }
    }

    GLuint mProgram               = 0;
    GLint mTextureUniformLocation = 0;

    GLuint mTexture = 0;

    struct swizzlePermutation
    {
        GLenum swizzleRed;
        GLenum swizzleGreen;
        GLenum swizzleBlue;
        GLenum swizzleAlpha;
    };
    std::vector<swizzlePermutation> mPermutations;
};

class SwizzleIntegerTest : public SwizzleTest
{
  protected:
    void testSetUp() override
    {
        constexpr char kVS[] = R"(#version 300 es
precision highp float;
in vec4 position;
out vec2 texcoord;
void main()
{
    gl_Position = position;
    texcoord = (position.xy * 0.5) + 0.5;
})";

        constexpr char kFS[] = R"(#version 300 es
precision highp float;
precision highp usampler2D;
uniform usampler2D tex;
in vec2 texcoord;
out vec4 my_FragColor;

void main()
{
    uvec4 s = texture(tex, texcoord);
    if (s[0] == 1u) s[0] = 255u;
    if (s[1] == 1u) s[1] = 255u;
    if (s[2] == 1u) s[2] = 255u;
    if (s[3] == 1u) s[3] = 255u;
    my_FragColor = vec4(s) / 255.0;
})";

        mProgram = CompileProgram(kVS, kFS);
        ASSERT_NE(0u, mProgram);

        mTextureUniformLocation = glGetUniformLocation(mProgram, "tex");
        ASSERT_NE(-1, mTextureUniformLocation);

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        ASSERT_GL_NO_ERROR();
    }
};

TEST_P(SwizzleTest, RGBA8_2D)
{
    ANGLE_SKIP_TEST_IF(!isTextureSwizzleAvailable());

    GLubyte data[] = {1, 64, 128, 200};
    init2DTexture(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, data);
    runTest2D();
}

TEST_P(SwizzleTest, RGB8_2D)
{
    ANGLE_SKIP_TEST_IF(!isTextureSwizzleAvailable());

    GLubyte data[] = {77, 66, 55};
    init2DTexture(GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, data);
    runTest2D();
}

TEST_P(SwizzleTest, RG8_2D)
{
    ANGLE_SKIP_TEST_IF(!isTextureSwizzleAvailable());

    GLubyte data[] = {11, 99};
    init2DTexture(GL_RG8, GL_RG, GL_UNSIGNED_BYTE, data);
    runTest2D();
}

TEST_P(SwizzleTest, R8_2D)
{
    ANGLE_SKIP_TEST_IF(!isTextureSwizzleAvailable());

    GLubyte data[] = {2};
    init2DTexture(GL_R8, GL_RED, GL_UNSIGNED_BYTE, data);
    runTest2D();
}

TEST_P(SwizzleTest, RGB10_A2_2D)
{
    ANGLE_SKIP_TEST_IF(!isTextureSwizzleAvailable());

    GLuint data[] = {20u | (40u << 10) | (60u << 20) | (2u << 30)};
    init2DTexture(GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, data);
    runTest2D();
}

TEST_P(SwizzleTest, RGB10_2D)
{
    ANGLE_SKIP_TEST_IF(!isTextureSwizzleAvailable());
    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_EXT_texture_type_2_10_10_10_REV"));

    GLuint data[] = {20u | (40u << 10) | (60u << 20) | (2u << 30)};
    init2DTexture(GL_RGB, GL_RGB, GL_UNSIGNED_INT_2_10_10_10_REV, data);
    runTest2D();
}

TEST_P(SwizzleTest, RGBA32F_2D)
{
    ANGLE_SKIP_TEST_IF(!isTextureSwizzleAvailable());

    GLfloat data[] = {0.25f, 0.5f, 0.75f, 0.8f};
    init2DTexture(GL_RGBA32F, GL_RGBA, GL_FLOAT, data);
    runTest2D();
}

TEST_P(SwizzleTest, RGB32F_2D)
{
    ANGLE_SKIP_TEST_IF(!isTextureSwizzleAvailable());

    GLfloat data[] = {0.1f, 0.2f, 0.3f};
    init2DTexture(GL_RGB32F, GL_RGB, GL_FLOAT, data);
    runTest2D();
}

TEST_P(SwizzleTest, RG32F_2D)
{
    ANGLE_SKIP_TEST_IF(!isTextureSwizzleAvailable());

    GLfloat data[] = {0.9f, 0.1f};
    init2DTexture(GL_RG32F, GL_RG, GL_FLOAT, data);
    runTest2D();
}

TEST_P(SwizzleTest, R32F_2D)
{
    ANGLE_SKIP_TEST_IF(!isTextureSwizzleAvailable());

    GLfloat data[] = {0.5f};
    init2DTexture(GL_R32F, GL_RED, GL_FLOAT, data);
    runTest2D();
}

TEST_P(SwizzleTest, D32F_2D)
{
    ANGLE_SKIP_TEST_IF(!isTextureSwizzleAvailable());

    GLfloat data[] = {0.5f};
    init2DTexture(GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT, data);
    runTest2D();
}

TEST_P(SwizzleTest, D16_2D)
{
    ANGLE_SKIP_TEST_IF(!isTextureSwizzleAvailable());

    GLushort data[] = {0xFF};
    init2DTexture(GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT, data);
    runTest2D();
}

TEST_P(SwizzleTest, D24_2D)
{
    ANGLE_SKIP_TEST_IF(!isTextureSwizzleAvailable());

    ANGLE_SKIP_TEST_IF(IsVulkan() && IsAMD() && IsWindows());  // anglebug.com/42262208
    GLuint data[] = {0xFFFF};
    init2DTexture(GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, data);
    runTest2D();
}

TEST_P(SwizzleTest, L8_2D)
{
    ANGLE_SKIP_TEST_IF(!isTextureSwizzleAvailable());

    GLubyte data[] = {0x77};
    init2DTexture(GL_LUMINANCE, GL_LUMINANCE, GL_UNSIGNED_BYTE, data);
    runTest2D();
}

TEST_P(SwizzleTest, A8_2D)
{
    ANGLE_SKIP_TEST_IF(!isTextureSwizzleAvailable());

    GLubyte data[] = {0x55};
    init2DTexture(GL_ALPHA, GL_ALPHA, GL_UNSIGNED_BYTE, data);
    runTest2D();
}

TEST_P(SwizzleTest, LA8_2D)
{
    ANGLE_SKIP_TEST_IF(!isTextureSwizzleAvailable());

    GLubyte data[] = {0x77, 0x66};
    init2DTexture(GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, data);
    runTest2D();
}

TEST_P(SwizzleTest, L32F_2D)
{
    ANGLE_SKIP_TEST_IF(!isTextureSwizzleAvailable());

    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_float"));

    GLfloat data[] = {0.7f};
    init2DTexture(GL_LUMINANCE, GL_LUMINANCE, GL_FLOAT, data);
    runTest2D();
}

TEST_P(SwizzleTest, A32F_2D)
{
    ANGLE_SKIP_TEST_IF(!isTextureSwizzleAvailable());

    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_float"));

    GLfloat data[] = {
        0.4f,
    };
    init2DTexture(GL_ALPHA, GL_ALPHA, GL_FLOAT, data);
    runTest2D();
}

TEST_P(SwizzleTest, LA32F_2D)
{
    ANGLE_SKIP_TEST_IF(!isTextureSwizzleAvailable());

    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_texture_float"));

    GLfloat data[] = {
        0.5f,
        0.6f,
    };
    init2DTexture(GL_LUMINANCE_ALPHA, GL_LUMINANCE_ALPHA, GL_FLOAT, data);
    runTest2D();
}

#include "media/pixel.inc"

TEST_P(SwizzleTest, CompressedDXT_2D)
{
    ANGLE_SKIP_TEST_IF(!isTextureSwizzleAvailable());

    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_compression_dxt1"));

    init2DCompressedTexture(GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, pixel_0_width, pixel_0_height,
                            pixel_0_size, pixel_0_data);
    runTest2D();
}

TEST_P(SwizzleTest, CompressedDXT1_RGB_2D)
{
    ANGLE_SKIP_TEST_IF(!isTextureSwizzleAvailable());

    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_compression_dxt1"));

    init2DCompressedTexture(GL_COMPRESSED_RGB_S3TC_DXT1_EXT, pixel_0_width, pixel_0_height,
                            pixel_0_size, pixel_0_data);
    runTest2D();
}

TEST_P(SwizzleIntegerTest, RGB8UI_2D)
{
    ANGLE_SKIP_TEST_IF(!isTextureSwizzleAvailable());

    ANGLE_SKIP_TEST_IF(IsVulkan());  // anglebug.com/42261870 - integer textures
    GLubyte data[] = {77, 66, 55};
    init2DTexture(GL_RGB8UI, GL_RGB_INTEGER, GL_UNSIGNED_BYTE, data);
    runTest2D();
}

// Test that updating the texture data still generates the correct swizzles
TEST_P(SwizzleTest, SubUpdate)
{
    ANGLE_SKIP_TEST_IF(!isTextureSwizzleAvailable());

    GLColor data(1, 64, 128, 200);
    init2DTexture(GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, &data);

    glUseProgram(mProgram);
    glBindTexture(GL_TEXTURE_2D, mTexture);
    glUniform1i(mTextureUniformLocation, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_G, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_A, GL_RED);

    glClear(GL_COLOR_BUFFER_BIT);
    drawQuad(mProgram, "position", 0.5f);

    GLColor expectedData(data.R, data.R, data.R, data.R);
    EXPECT_PIXEL_COLOR_EQ(0, 0, expectedData);

    GLColor updateData(32, 234, 28, 232);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, &updateData);

    glClear(GL_COLOR_BUFFER_BIT);
    drawQuad(mProgram, "position", 0.5f);

    GLColor expectedUpdateData(updateData.R, updateData.R, updateData.R, updateData.R);
    EXPECT_PIXEL_COLOR_EQ(0, 0, expectedUpdateData);
}

// Tests that updating the swizzle state on a texture currently bound as a draw framebuffer
// attachment (and sampled at a different mip level) does not cause subsequent draws to fail
// after finishing.
TEST_P(SwizzleTest, TextureSwizzleAttachedToDrawFBOThenFinish)
{
    // Define a texture to use its level 1 for sampling. Its level 0 will be drawn to later.
    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 1);

    constexpr size_t kWidth  = 64;
    constexpr size_t kHeight = 64;
    std::vector<GLColor> texColor(kWidth * kHeight, GLColor(128, 90, 255, 255));
    glTexStorage2D(GL_TEXTURE_2D, 2, GL_RGBA8, kWidth, kHeight);
    glTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, kWidth / 2, kHeight / 2, GL_RGBA, GL_UNSIGNED_BYTE,
                    texColor.data());

    // Bind the draw framebuffer to level 0 of the texture. Keep the read FBO as default.
    GLFramebuffer fbo;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glViewport(0, 0, kWidth, kHeight);
    ASSERT_GL_NO_ERROR();
    ASSERT_GLENUM_EQ(GL_FRAMEBUFFER_COMPLETE, glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER));

    // Prepare to draw a full-screen triangle.
    constexpr char kVS[] = R"(#version 300 es
in vec2 posIn;
out vec2 uv;
void main(){
    uv = posIn * 0.5 + 0.5;
    gl_Position = vec4(posIn, 0.0, 1.0);
})";
    constexpr char kFS[] = R"(#version 300 es
precision mediump float;
uniform sampler2D tex2D;
in vec2 uv;
out vec4 outColor;
void main(){
    outColor = texture(tex2D, uv);
})";

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "tex2D"), 0);

    GLVertexArray vao;
    glBindVertexArray(vao);

    std::vector<GLfloat> posData = {-1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f};
    GLBuffer posBuffer;
    glBindBuffer(GL_ARRAY_BUFFER, posBuffer);
    glBufferData(GL_ARRAY_BUFFER, posData.size() * sizeof(GLfloat), posData.data(), GL_STATIC_DRAW);

    GLint posLoc = glGetAttribLocation(program, "posIn");
    glEnableVertexAttribArray(posLoc);
    glVertexAttribPointer(posLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);

    // Issue the first draw call after clearing.
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // Dirty the draw framebuffer bit, followed by changing red swizzling on the texture.
    const GLenum drawBuffers[] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, drawBuffers);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_GREEN);

    // Issue the second draw call, and finish.
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glFinish();

    // Issue the third draw call.
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // The sampled red component on the FBO should now reflect the green component due to swizzle.
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    EXPECT_PIXEL_RECT_EQ(0, 0, kWidth, kHeight, GLColor(90, 90, 255, 255));
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SwizzleTest);
ANGLE_INSTANTIATE_TEST_ES3(SwizzleTest);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SwizzleIntegerTest);
ANGLE_INSTANTIATE_TEST_ES3(SwizzleIntegerTest);

}  // namespace
