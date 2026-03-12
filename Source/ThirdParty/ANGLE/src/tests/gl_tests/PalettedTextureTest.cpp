//
// Copyright 2022 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// PalettedTextureTest.cpp: Tests paletted texture decompression

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

#include "common/matrix_utils.h"
#include "common/vector_utils.h"
#include "util/random_utils.h"

#include <stdint.h>

#include <vector>

using namespace angle;

class PalettedTextureTest : public ANGLETest<>
{
  protected:
    PalettedTextureTest()
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

class PalettedTextureTestES2 : public PalettedTextureTest
{};

struct TestImage
{
    GLColor palette[16];
    uint8_t texels[2];  // 2x2, each texel is 4-bit
};

const TestImage testImage = {
    {
        GLColor::cyan,
        GLColor::yellow,
        GLColor::magenta,
        GLColor::red,
    },
    {
        0x01,
        0x23,
    },
};

// Check that paletted formats are reported as supported.
TEST_P(PalettedTextureTest, PalettedFormatsAreSupported)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_compressed_paletted_texture"));

    std::vector<GLint> mustSupportFormats{
        GL_PALETTE4_RGB8_OES,    GL_PALETTE4_RGBA8_OES,    GL_PALETTE4_R5_G6_B5_OES,
        GL_PALETTE4_RGBA4_OES,   GL_PALETTE4_RGB5_A1_OES,  GL_PALETTE8_RGB8_OES,
        GL_PALETTE8_RGBA8_OES,   GL_PALETTE8_R5_G6_B5_OES, GL_PALETTE8_RGBA4_OES,
        GL_PALETTE8_RGB5_A1_OES,
    };

    GLint numSupportedFormats;
    glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &numSupportedFormats);

    std::vector<GLint> supportedFormats(numSupportedFormats, 0);
    glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, supportedFormats.data());

    for (GLint format : mustSupportFormats)
    {
        EXPECT_TRUE(std::find(supportedFormats.begin(), supportedFormats.end(), format) !=
                    supportedFormats.end());
    }
}

// Check that sampling a paletted texture works.
TEST_P(PalettedTextureTest, PalettedTextureSampling)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_compressed_paletted_texture"));

    struct Vertex
    {
        GLfloat position[3];
        GLfloat uv[2];
    };

    glEnable(GL_TEXTURE_2D);

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_PALETTE4_RGBA8_OES, 2, 2, 0, sizeof testImage,
                           &testImage);
    EXPECT_GL_NO_ERROR();

    glClearColor(0.4f, 0.4f, 0.4f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    for (int i = 0; i < 10; i++)
    {
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glFrustumf(-1, 1, -1, 1, 5.0, 60.0);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef(0.0f, 0.0f, -8.0f);

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);

        std::vector<Vertex> vertices = {
            {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f}},
            {{-1.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
            {{1.0f, -1.0f, 0.0f}, {1.0f, 0.0f}},
            {{1.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
        };
        glVertexPointer(3, GL_FLOAT, sizeof vertices[0], &vertices[0].position);
        glTexCoordPointer(2, GL_FLOAT, sizeof vertices[0], &vertices[0].uv);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, vertices.size());
        EXPECT_GL_NO_ERROR();

        EXPECT_PIXEL_COLOR_NEAR(8, 8, testImage.palette[0], 0);
        EXPECT_PIXEL_COLOR_NEAR(24, 8, testImage.palette[1], 0);
        EXPECT_PIXEL_COLOR_NEAR(8, 24, testImage.palette[2], 0);
        EXPECT_PIXEL_COLOR_NEAR(24, 24, testImage.palette[3], 0);

        swapBuffers();
    }
}

// Check that validation for paletted formats is correct.
TEST_P(PalettedTextureTestES2, PalettedFormatsValidation)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_compressed_paletted_texture"));

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);

    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_PALETTE4_RGBA8_OES, 2, 2, 0, sizeof testImage,
                           &testImage);
    EXPECT_GL_NO_ERROR();

    // Generate GL_INVALID_OPERATION, paletted formats cannot be used for glCompressedTexSubImage2D
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, GL_PALETTE4_RGBA8_OES, 0, nullptr);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Check that rendering using paletted texture works in GLES2 context.
TEST_P(PalettedTextureTestES2, PalettedTextureDraw)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_OES_compressed_paletted_texture"));

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

    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glUseProgram(program);

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_PALETTE4_RGBA8_OES, 2, 2, 0, sizeof testImage,
                           &testImage);
    EXPECT_GL_NO_ERROR();

    GLint textureUniformLocation = glGetUniformLocation(program, "tex");
    ASSERT_NE(-1, textureUniformLocation);
    glUniform1i(textureUniformLocation, 0);

    drawQuad(program, "position", 0.0f);
    EXPECT_GL_NO_ERROR();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::cyan);
    EXPECT_PIXEL_COLOR_EQ(31, 0, GLColor::yellow);
    EXPECT_PIXEL_COLOR_EQ(0, 31, GLColor::magenta);
    EXPECT_PIXEL_COLOR_EQ(31, 31, GLColor::red);
}

ANGLE_INSTANTIATE_TEST_ES1(PalettedTextureTest);

ANGLE_INSTANTIATE_TEST_ES2(PalettedTextureTestES2);
