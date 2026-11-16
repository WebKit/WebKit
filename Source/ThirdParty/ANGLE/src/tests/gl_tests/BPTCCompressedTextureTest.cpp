//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// BPTCCompressedTextureTest.cpp: Tests of the GL_EXT_texture_compression_bptc extension

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

namespace
{

const unsigned int kPixelTolerance = 1u;

// The pixel data represents a 4x4 pixel image with the left side colored red and the right side
// green. It was BC7 encoded using Microsoft's BC6HBC7Encoder.
const std::array<GLubyte, 16> kBC7Data4x4 = {0x50, 0x1f, 0xfc, 0xf, 0x0,  0xf0, 0xe3, 0xe1,
                                             0xe1, 0xe1, 0xc1, 0xf, 0xfc, 0xc0, 0xf,  0xfc};

// The pixel data represents a 4x4 pixel image with the transparent black solid color.
// Sampling from a zero-filled block is undefined, so use a valid one.
const std::array<GLubyte, 16> kBC7BlackData4x4 = {0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                                  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// BC7 Mode 6 packs fields LSB-first across 128 bits:
//   bits [0..6]   = 0b1000000 (Mode 6: 7-bit RGBA endpoints + 1-bit p-bit)
//   bits [7..13]  = R0, [14..20] = R1, [21..27] = G0, [28..34] = G1,
//   bits [35..41] = B0, [42..48] = B1, [49..55] = A0, [56..62] = A1,
//   bit  [63]     = P0, [64]     = P1, [65..127] = 16 texel indices (all 0 -> endpoint 0).
// Mode 6 with R0=0x7f (bits 7..13 -> byte 0 bit 7 = 0x80, byte 1 bits 0..5 = 0x3f),
// A0=0x7f (bits 49..55 -> byte 6 bits 1..7 = 0xfe), and P0=1 (bit 63 -> byte 7 = 0x80)
// -> endpoint 0 is solid RGBA(255, 0, 0, 255) (red).
constexpr std::array<GLubyte, 16> kBC7RedData4x4 = {0xc0, 0x3f, 0x00, 0x00, 0x00, 0x00, 0xfe, 0x80,
                                                    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
}  // anonymous namespace

class BPTCCompressedTextureTest : public ANGLETest<>
{
  protected:
    BPTCCompressedTextureTest()
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
    texcoord.y = 1.0 - texcoord.y;
})";

        constexpr char kFS[] = R"(precision highp float;
uniform sampler2D tex;
varying vec2 texcoord;

void main()
{
    gl_FragColor = texture2D(tex, texcoord);
})";

        mTextureProgram = CompileProgram(kVS, kFS);
        if (mTextureProgram == 0)
        {
            FAIL() << "shader compilation failed.";
        }

        mTextureUniformLocation = glGetUniformLocation(mTextureProgram, "tex");

        ASSERT_GL_NO_ERROR();
    }

    void testTearDown() override { glDeleteProgram(mTextureProgram); }

    void setupTextureParameters(GLuint texture)
    {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    void drawTexture()
    {
        glUseProgram(mTextureProgram);
        glUniform1i(mTextureUniformLocation, 0);
        drawQuad(mTextureProgram, "position", 0.5f);
        EXPECT_GL_NO_ERROR();
    }

    GLuint mTextureProgram;
    GLint mTextureUniformLocation;
};

class BPTCCompressedTextureTestES3 : public BPTCCompressedTextureTest
{
  public:
    BPTCCompressedTextureTestES3() : BPTCCompressedTextureTest() {}
};

// Test sampling from a BC7 non-SRGB image.
TEST_P(BPTCCompressedTextureTest, CompressedTexImageBC7)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_compression_bptc"));

    GLTexture texture;
    setupTextureParameters(texture);

    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_BPTC_UNORM_EXT, 4, 4, 0,
                           kBC7Data4x4.size(), kBC7Data4x4.data());

    EXPECT_GL_NO_ERROR();

    drawTexture();

    EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor::red, kPixelTolerance);
    EXPECT_PIXEL_COLOR_NEAR(0, getWindowHeight() - 1, GLColor::red, kPixelTolerance);
    EXPECT_PIXEL_COLOR_NEAR(getWindowWidth() - 1, 0, GLColor::green, kPixelTolerance);
    EXPECT_PIXEL_COLOR_NEAR(getWindowWidth() - 1, getWindowHeight() - 1, GLColor::green,
                            kPixelTolerance);
}

// Test sampling from a BC7 SRGB image.
TEST_P(BPTCCompressedTextureTest, CompressedTexImageBC7SRGB)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_compression_bptc"));

    GLTexture texture;
    setupTextureParameters(texture);

    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_EXT, 4, 4, 0,
                           kBC7Data4x4.size(), kBC7Data4x4.data());

    EXPECT_GL_NO_ERROR();

    drawTexture();

    EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor::red, kPixelTolerance);
    EXPECT_PIXEL_COLOR_NEAR(0, getWindowHeight() - 1, GLColor::red, kPixelTolerance);
    EXPECT_PIXEL_COLOR_NEAR(getWindowWidth() - 1, 0, GLColor::green, kPixelTolerance);
    EXPECT_PIXEL_COLOR_NEAR(getWindowWidth() - 1, getWindowHeight() - 1, GLColor::green,
                            kPixelTolerance);
}

// Test that using the BC6H floating point formats doesn't crash.
TEST_P(BPTCCompressedTextureTest, CompressedTexImageBC6HNoCrash)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_compression_bptc"));

    GLTexture texture;
    setupTextureParameters(texture);

    // This fake pixel data represents a 4x4 pixel image.
    // TODO(http://anglebug.com/40096529): Add pixel tests for these formats. These need HDR source
    // images.
    std::vector<GLubyte> data;
    data.resize(16u, 0u);

    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_EXT, 4, 4, 0,
                           data.size(), data.data());
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_EXT, 4, 4, 0,
                           data.size(), data.data());

    EXPECT_GL_NO_ERROR();

    drawTexture();
}

// Test texStorage2D with a BPTC format.
TEST_P(BPTCCompressedTextureTestES3, CompressedTexStorage)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_compression_bptc"));

    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);

    GLTexture texture;
    setupTextureParameters(texture);

    glTexStorage2D(GL_TEXTURE_2D, 1, GL_COMPRESSED_RGBA_BPTC_UNORM_EXT, 4, 4);
    EXPECT_GL_NO_ERROR();
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_COMPRESSED_RGBA_BPTC_UNORM_EXT,
                              kBC7Data4x4.size(), kBC7Data4x4.data());
    EXPECT_GL_NO_ERROR();

    drawTexture();

    EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor::red, kPixelTolerance);
    EXPECT_PIXEL_COLOR_NEAR(0, getWindowHeight() - 1, GLColor::red, kPixelTolerance);
    EXPECT_PIXEL_COLOR_NEAR(getWindowWidth() - 1, 0, GLColor::green, kPixelTolerance);
    EXPECT_PIXEL_COLOR_NEAR(getWindowWidth() - 1, getWindowHeight() - 1, GLColor::green,
                            kPixelTolerance);
}

// Test validation of glCompressedTexSubImage2D with BPTC formats
TEST_P(BPTCCompressedTextureTest, CompressedTexSubImageValidation)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_compression_bptc"));

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D, texture);

    std::vector<GLubyte> data(16 * 2 * 2);  // 2x2 blocks, thats 8x8 pixels.

    // Size mip 0 to a large size.
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_BPTC_UNORM_EXT, 8, 8, 0,
                           data.size(), data.data());
    ASSERT_GL_NO_ERROR();

    // Test a sub image with an offset that isn't a multiple of the block size.
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 1, 0, 4, 4, GL_COMPRESSED_RGBA_BPTC_UNORM_EXT,
                              kBC7Data4x4.size(), kBC7Data4x4.data());
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 3, 4, 4, GL_COMPRESSED_RGBA_BPTC_UNORM_EXT,
                              kBC7Data4x4.size(), kBC7Data4x4.data());
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);

    // Test a sub image with a negative offset.
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, -1, 0, 4, 4, GL_COMPRESSED_RGBA_BPTC_UNORM_EXT,
                              kBC7Data4x4.size(), kBC7Data4x4.data());
    ASSERT_GL_ERROR(GL_INVALID_VALUE);
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, -1, 4, 4, GL_COMPRESSED_RGBA_BPTC_UNORM_EXT,
                              kBC7Data4x4.size(), kBC7Data4x4.data());
    ASSERT_GL_ERROR(GL_INVALID_VALUE);
}

// Test that copying BPTC textures is not allowed. This restriction exists only in
// EXT_texture_compression_bptc, and not in the ARB variant.
TEST_P(BPTCCompressedTextureTest, CopyTexImage2DDisallowed)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_compression_bptc"));

    GLTexture texture;
    setupTextureParameters(texture);

    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_BPTC_UNORM_EXT, 0, 0, 4, 4, 0);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);
}

// Test that copying BPTC textures is not allowed. This restriction exists only in
// EXT_texture_compression_bptc, and not in the ARB variant.
TEST_P(BPTCCompressedTextureTest, CopyTexSubImage2DDisallowed)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_compression_bptc"));

    GLTexture texture;
    setupTextureParameters(texture);

    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_BPTC_UNORM_EXT, 4, 4, 0,
                           kBC7Data4x4.size(), kBC7Data4x4.data());
    ASSERT_GL_NO_ERROR();

    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 4, 4);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);
}

// Test that copying BPTC textures is not allowed. This restriction exists only in
// EXT_texture_compression_bptc, and not in the ARB variant.
TEST_P(BPTCCompressedTextureTestES3, CopyTexSubImage3DDisallowed)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_compression_bptc"));

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture);

    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_COMPRESSED_RGBA_BPTC_UNORM_EXT, 4, 4, 1);
    ASSERT_GL_NO_ERROR();

    glCopyTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 0, 0, 4, 4);
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);
}

// Test uploading texture data from a PBO to a texture.
TEST_P(BPTCCompressedTextureTestES3, PBOCompressedTexImage)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_compression_bptc"));

    GLTexture texture;
    setupTextureParameters(texture);

    GLBuffer buffer;
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, kBC7Data4x4.size(), kBC7Data4x4.data(), GL_STREAM_DRAW);
    ASSERT_GL_NO_ERROR();

    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_BPTC_UNORM_EXT, 4, 4, 0,
                           kBC7Data4x4.size(), nullptr);
    ASSERT_GL_NO_ERROR();

    drawTexture();

    EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor::red, kPixelTolerance);
    EXPECT_PIXEL_COLOR_NEAR(0, getWindowHeight() - 1, GLColor::red, kPixelTolerance);
    EXPECT_PIXEL_COLOR_NEAR(getWindowWidth() - 1, 0, GLColor::green, kPixelTolerance);
    EXPECT_PIXEL_COLOR_NEAR(getWindowWidth() - 1, getWindowHeight() - 1, GLColor::green,
                            kPixelTolerance);

    // Destroy the data
    glBufferData(GL_PIXEL_UNPACK_BUFFER, kBC7BlackData4x4.size(), kBC7BlackData4x4.data(),
                 GL_STREAM_DRAW);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_BPTC_UNORM_EXT, 4, 4, 0,
                           kBC7BlackData4x4.size(), nullptr);
    ASSERT_GL_NO_ERROR();

    drawTexture();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::transparentBlack);

    // Initialize again.  This time, the texture's image is already allocated, so the PBO data
    // upload could be directly done.
    glBufferData(GL_PIXEL_UNPACK_BUFFER, kBC7Data4x4.size(), kBC7Data4x4.data(), GL_STREAM_DRAW);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA_BPTC_UNORM_EXT, 4, 4, 0,
                           kBC7Data4x4.size(), nullptr);
    ASSERT_GL_NO_ERROR();

    drawTexture();

    EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor::red, kPixelTolerance);
    EXPECT_PIXEL_COLOR_NEAR(0, getWindowHeight() - 1, GLColor::red, kPixelTolerance);
    EXPECT_PIXEL_COLOR_NEAR(getWindowWidth() - 1, 0, GLColor::green, kPixelTolerance);
    EXPECT_PIXEL_COLOR_NEAR(getWindowWidth() - 1, getWindowHeight() - 1, GLColor::green,
                            kPixelTolerance);
}

// Test uploading texture data from a PBO to a non-zero base texture.
TEST_P(BPTCCompressedTextureTestES3, PBOCompressedTexImageNonZeroBase)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_compression_bptc"));

    GLTexture texture;
    setupTextureParameters(texture);

    GLBuffer buffer;
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, kBC7Data4x4.size(), kBC7Data4x4.data(), GL_STREAM_DRAW);
    ASSERT_GL_NO_ERROR();

    glCompressedTexImage2D(GL_TEXTURE_2D, 1, GL_COMPRESSED_RGBA_BPTC_UNORM_EXT, 4, 4, 0,
                           kBC7Data4x4.size(), nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 1);
    ASSERT_GL_NO_ERROR();

    drawTexture();

    EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor::red, kPixelTolerance);
    EXPECT_PIXEL_COLOR_NEAR(0, getWindowHeight() - 1, GLColor::red, kPixelTolerance);
    EXPECT_PIXEL_COLOR_NEAR(getWindowWidth() - 1, 0, GLColor::green, kPixelTolerance);
    EXPECT_PIXEL_COLOR_NEAR(getWindowWidth() - 1, getWindowHeight() - 1, GLColor::green,
                            kPixelTolerance);

    // Destroy the data
    glBufferData(GL_PIXEL_UNPACK_BUFFER, kBC7BlackData4x4.size(), kBC7BlackData4x4.data(),
                 GL_STREAM_DRAW);
    glCompressedTexImage2D(GL_TEXTURE_2D, 1, GL_COMPRESSED_RGBA_BPTC_UNORM_EXT, 4, 4, 0,
                           kBC7BlackData4x4.size(), nullptr);
    ASSERT_GL_NO_ERROR();

    drawTexture();
    EXPECT_PIXEL_COLOR_EQ(0, 0, GLColor::transparentBlack);

    // Initialize again.  This time, the texture's image is already allocated, so the PBO data
    // upload could be directly done.
    glBufferData(GL_PIXEL_UNPACK_BUFFER, kBC7Data4x4.size(), kBC7Data4x4.data(), GL_STREAM_DRAW);
    glCompressedTexImage2D(GL_TEXTURE_2D, 1, GL_COMPRESSED_RGBA_BPTC_UNORM_EXT, 4, 4, 0,
                           kBC7Data4x4.size(), nullptr);
    ASSERT_GL_NO_ERROR();

    drawTexture();

    EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor::red, kPixelTolerance);
    EXPECT_PIXEL_COLOR_NEAR(0, getWindowHeight() - 1, GLColor::red, kPixelTolerance);
    EXPECT_PIXEL_COLOR_NEAR(getWindowWidth() - 1, 0, GLColor::green, kPixelTolerance);
    EXPECT_PIXEL_COLOR_NEAR(getWindowWidth() - 1, getWindowHeight() - 1, GLColor::green,
                            kPixelTolerance);
}

// Test uploading texture data from a PBO to a texture allocated with texStorage2D.
TEST_P(BPTCCompressedTextureTestES3, PBOCompressedTexStorage)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_compression_bptc"));

    GLTexture texture;
    setupTextureParameters(texture);

    glTexStorage2D(GL_TEXTURE_2D, 1, GL_COMPRESSED_RGBA_BPTC_UNORM_EXT, 4, 4);
    ASSERT_GL_NO_ERROR();

    GLBuffer buffer;
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, kBC7Data4x4.size(), kBC7Data4x4.data(), GL_STREAM_DRAW);
    ASSERT_GL_NO_ERROR();

    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_COMPRESSED_RGBA_BPTC_UNORM_EXT,
                              kBC7Data4x4.size(), nullptr);

    ASSERT_GL_NO_ERROR();

    drawTexture();

    EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor::red, kPixelTolerance);
    EXPECT_PIXEL_COLOR_NEAR(0, getWindowHeight() - 1, GLColor::red, kPixelTolerance);
    EXPECT_PIXEL_COLOR_NEAR(getWindowWidth() - 1, 0, GLColor::green, kPixelTolerance);
    EXPECT_PIXEL_COLOR_NEAR(getWindowWidth() - 1, getWindowHeight() - 1, GLColor::green,
                            kPixelTolerance);
}

// Test validation of glCompressedTexSubImage3D with BPTC formats
TEST_P(BPTCCompressedTextureTestES3, CompressedTexSubImage3DValidation)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_compression_bptc"));

    GLTexture texture;
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture);

    std::vector<GLubyte> data(16 * 2 * 2);  // 2x2x1 blocks, thats 8x8x1 pixels.

    // Size mip 0 to a large size.
    glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGBA_BPTC_UNORM_EXT, 8, 8, 1, 0,
                           data.size(), data.data());
    ASSERT_GL_NO_ERROR();

    // Test a sub image with an offset that isn't a multiple of the block size.
    glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 2, 0, 0, 4, 4, 1,
                              GL_COMPRESSED_RGBA_BPTC_UNORM_EXT, kBC7Data4x4.size(),
                              kBC7Data4x4.data());
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);
    glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 2, 0, 4, 4, 1,
                              GL_COMPRESSED_RGBA_BPTC_UNORM_EXT, kBC7Data4x4.size(),
                              kBC7Data4x4.data());
    ASSERT_GL_ERROR(GL_INVALID_OPERATION);

    // Test a sub image with a negative offset.
    glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, -1, 0, 0, 4, 4, 1,
                              GL_COMPRESSED_RGBA_BPTC_UNORM_EXT, kBC7Data4x4.size(),
                              kBC7Data4x4.data());
    ASSERT_GL_ERROR(GL_INVALID_VALUE);
    glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, -1, 0, 4, 4, 1,
                              GL_COMPRESSED_RGBA_BPTC_UNORM_EXT, kBC7Data4x4.size(),
                              kBC7Data4x4.data());
    ASSERT_GL_ERROR(GL_INVALID_VALUE);
    glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, -1, 4, 4, 1,
                              GL_COMPRESSED_RGBA_BPTC_UNORM_EXT, kBC7Data4x4.size(),
                              kBC7Data4x4.data());
    ASSERT_GL_ERROR(GL_INVALID_VALUE);
}

// Test that uploading and sampling 3D BPTC mip levels smaller than a 4x4 block with depth > 1
// allocates a staging texture with sufficient depth at the selected mip subresource.
TEST_P(BPTCCompressedTextureTestES3, CompressedTexImage3DSubBlockMipDepth)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_compression_bptc"));

    constexpr char kVS[] = R"(#version 300 es
in vec4 a_position;
void main()
{
    gl_Position = a_position;
})";
    constexpr char kFS[] = R"(#version 300 es
precision highp float;
precision highp sampler3D;
uniform sampler3D u_tex;
uniform float u_z;
out vec4 my_FragColor;
void main()
{
    my_FragColor = texture(u_tex, vec3(0.5, 0.5, u_z));
})";
    ANGLE_GL_PROGRAM(program, kVS, kFS);
    glUseProgram(program);
    GLint zLoc = glGetUniformLocation(program, "u_z");
    ASSERT_NE(-1, zLoc);

    auto testSubBlockMipUploadAndSample = [&](GLint level, GLsizei width, GLsizei height,
                                              GLsizei depth) {
        GLTexture tex;
        glBindTexture(GL_TEXTURE_3D, tex);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_BASE_LEVEL, level);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, level);

        const size_t blocksPerSlice =
            static_cast<size_t>((width + 3) / 4) * static_cast<size_t>((height + 3) / 4);
        const size_t totalBlocks = blocksPerSlice * static_cast<size_t>(depth);
        std::vector<GLubyte> data;
        data.reserve(totalBlocks * 16);

        // Fill slices [0, depth - 2] with transparentBlack and the last slice (depth - 1) with red.
        for (size_t block = 0; block < totalBlocks; ++block)
        {
            const auto &blockData = (block < blocksPerSlice * static_cast<size_t>(depth - 1))
                                        ? kBC7BlackData4x4
                                        : kBC7RedData4x4;
            data.insert(data.end(), blockData.begin(), blockData.end());
        }

        glCompressedTexImage3D(GL_TEXTURE_3D, level, GL_COMPRESSED_RGBA_BPTC_UNORM_EXT, width,
                               height, depth, 0, static_cast<GLsizei>(data.size()), data.data());
        ASSERT_GL_NO_ERROR();

        glUniform1f(zLoc, 0.5f / static_cast<float>(depth));
        drawQuad(program, "a_position", 0.5f);
        EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor::transparentBlack, kPixelTolerance);

        glUniform1f(zLoc, (static_cast<float>(depth) - 0.5f) / static_cast<float>(depth));
        drawQuad(program, "a_position", 0.5f);
        EXPECT_PIXEL_COLOR_NEAR(0, 0, GLColor::red, kPixelTolerance);
    };

    // Level 1 at 1024x2x1024 (height < 4 -> lodOffset = 1): inputRowPitch (4096) != mapped D3D11
    // DepthPitch (32768), exercising the strided per-slice/per-row loop in LoadCompressedToNative.
    // Without expanding staging depth by lodOffset, mip 1 only has 512 physical slices instead of
    // 1024, writing 512 slices across a 16 MiB out-of-bounds span.
    testSubBlockMipUploadAndSample(1, 1024, 2, 1024);

    // Level 2 at 1x1x512 (width < 4, height < 4 -> lodOffset = 2): both inputDepthPitch and
    // mapped D3D11 DepthPitch are 16 bytes, exercising the single contiguous memcpy fast-path
    // (inputImageSize == outputImageSize) in LoadCompressedToNative.
    testSubBlockMipUploadAndSample(2, 1, 1, 512);
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(BPTCCompressedTextureTest);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(BPTCCompressedTextureTestES3);
ANGLE_INSTANTIATE_TEST_ES3(BPTCCompressedTextureTestES3);

class BPTCCompressedTextureTestES3WebGL : public BPTCCompressedTextureTestES3
{
  protected:
    BPTCCompressedTextureTestES3WebGL()
    {
        setWebGLCompatibilityEnabled(true);
        setRobustResourceInit(true);
    }
};

// Test that initializing a large 3D BPTC texture doesn't overflow the size calculation.
// This is a regression test for a bug where the size was computed as (width/4 * 16) * height *
// depth instead of (width/4 * 16) * (height/4) * depth.
TEST_P(BPTCCompressedTextureTestES3WebGL, DeferredInit3DOverflow)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_compression_bptc"));

    // The overflow happens in the D3D11 backend.
    // The dimensions 2048x2048x320 were reported to trigger it.
    // 2048/4 * 16 = 8192 (row pitch)
    // 8192 * 2048 * 320 = 5,368,709,120, which wraps to 1,073,741,824 in 32-bit GLuint.
    // The correct size is 1,342,177,280 (1.25 GB).
    // Since the wrapped buggy size is smaller than the correct size, it triggers an OOB read.
    // 1.25 GB is large enough that it might trigger GL_OUT_OF_MEMORY on some systems.

    GLTexture tex;
    glBindTexture(GL_TEXTURE_3D, tex);
    {
        ScopedIgnorePlatformMessages ignore;
        glTexStorage3D(GL_TEXTURE_3D, 1, GL_COMPRESSED_RGBA_BPTC_UNORM_EXT, 2048, 2048, 320);
    }
    GLenum err = glGetError();
    // Allow GL_OUT_OF_MEMORY as the texture is large.
    ASSERT_TRUE(err == GL_NO_ERROR || err == GL_OUT_OF_MEMORY || err == GL_INVALID_OPERATION);

    if (err != GL_OUT_OF_MEMORY && err != GL_INVALID_OPERATION)
    {
        // Trigger deferred initialization by updating a small sub-region.
        std::vector<GLubyte> data(16, 0);
        glCompressedTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 4, 4, 1,
                                  GL_COMPRESSED_RGBA_BPTC_UNORM_EXT, 16, data.data());
        err = glGetError();
        EXPECT_TRUE(err == GL_NO_ERROR || err == GL_OUT_OF_MEMORY);
    }
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(BPTCCompressedTextureTestES3WebGL);
// The overflow happens in the "slow path" of initializeContents, which is only called if
// robust resource initialization is enabled. Since it is always enabled in WebGL, we
// enable it here to reproduce the bug.
ANGLE_INSTANTIATE_TEST(BPTCCompressedTextureTestES3WebGL, ES3_D3D11());
