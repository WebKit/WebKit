//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// RobustClientMemoryTest.cpp : Tests of the GL_ANGLE_robust_client_memory extension.

#include "test_utils/ANGLETest.h"

#include "test_utils/gl_raii.h"

namespace angle
{

constexpr GLsizei kWindowSize = 128;

class RobustClientMemoryTest : public ANGLETest<>
{
  protected:
    RobustClientMemoryTest()
    {
        setWindowWidth(kWindowSize);
        setWindowHeight(kWindowSize);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }
};

// Test basic usage and validation of glGetIntegervRobustANGLE
TEST_P(RobustClientMemoryTest, GetInteger)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_robust_client_memory"));

    // Verify that the robust and regular entry points return the same values
    GLint resultRobust;
    GLsizei length;
    glGetIntegervRobustANGLE(GL_MAX_VERTEX_ATTRIBS, 1, &length, &resultRobust);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(1, length);

    GLint resultRegular;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &resultRegular);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(resultRegular, resultRobust);

    // Query a dynamic value
    GLint numCompressedFormats;
    glGetIntegervRobustANGLE(GL_NUM_COMPRESSED_TEXTURE_FORMATS, 1, &length, &numCompressedFormats);
    ASSERT_GL_NO_ERROR();
    EXPECT_EQ(1, length);

    if (numCompressedFormats > 0)
    {
        std::vector<GLint> resultBuf(numCompressedFormats * 2, 0);

        // Test when the bufSize is too low
        glGetIntegervRobustANGLE(GL_COMPRESSED_TEXTURE_FORMATS, numCompressedFormats - 1, &length,
                                 resultBuf.data());
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
        EXPECT_TRUE(std::all_of(resultBuf.begin(), resultBuf.end(),
                                [](GLint value) { return value == 0; }));

        // Make sure the GL doesn't touch the end of the buffer
        glGetIntegervRobustANGLE(GL_COMPRESSED_TEXTURE_FORMATS,
                                 static_cast<GLsizei>(resultBuf.size()), &length, resultBuf.data());
        EXPECT_GL_NO_ERROR();
        EXPECT_EQ(numCompressedFormats, length);
        EXPECT_TRUE(std::none_of(resultBuf.begin(), resultBuf.begin() + length,
                                 [](GLint value) { return value == 0; }));
        EXPECT_TRUE(std::all_of(resultBuf.begin() + length, resultBuf.end(),
                                [](GLint value) { return value == 0; }));
    }

    // Test with null length
    glGetIntegervRobustANGLE(GL_MAX_VARYING_VECTORS, 1, nullptr, &resultRobust);
    EXPECT_GL_NO_ERROR();

    glGetIntegervRobustANGLE(GL_MAX_VIEWPORT_DIMS, 1, nullptr, &resultRobust);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    GLint maxViewportDims[2];
    glGetIntegervRobustANGLE(GL_MAX_VIEWPORT_DIMS, 2, nullptr, maxViewportDims);
    EXPECT_GL_NO_ERROR();
}

// Test basic usage and validation of glGetInteger64vRobustANGLE
TEST_P(RobustClientMemoryTest, GetInteger64)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_robust_client_memory"));
    ANGLE_SKIP_TEST_IF(getClientMajorVersion() < 3);

    constexpr GLint64 kMinRequiredMaxElementIndex = 16777215;

    // Verify that the regular and robust entry points return the same values
    GLint64 resultRegular;
    glGetInteger64v(GL_MAX_ELEMENT_INDEX, &resultRegular);
    ASSERT_GL_NO_ERROR();
    ASSERT_GE(resultRegular, kMinRequiredMaxElementIndex);

    GLsizei length;
    GLint64 resultRobust;
    glGetInteger64vRobustANGLE(GL_MAX_ELEMENT_INDEX, 1, &length, &resultRobust);
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(1, length);
    EXPECT_EQ(resultRegular, resultRobust);
}

// Test basic usage and validation of glTexImage2DRobustANGLE and glTexSubImage2DRobustANGLE
TEST_P(RobustClientMemoryTest, TexImage2D)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_robust_client_memory"));

    GLTexture tex;
    glBindTexture(GL_TEXTURE_2D, tex);

    GLsizei dataDimension = 1024;
    std::vector<GLubyte> rgbaData(dataDimension * dataDimension * 4);

    // Test the regular case
    glTexImage2DRobustANGLE(GL_TEXTURE_2D, 0, GL_RGBA, dataDimension, dataDimension, 0, GL_RGBA,
                            GL_UNSIGNED_BYTE, static_cast<GLsizei>(rgbaData.size()),
                            rgbaData.data());
    EXPECT_GL_NO_ERROR();

    glTexSubImage2DRobustANGLE(GL_TEXTURE_2D, 0, 0, 0, dataDimension, dataDimension, GL_RGBA,
                               GL_UNSIGNED_BYTE, static_cast<GLsizei>(rgbaData.size()),
                               rgbaData.data());
    EXPECT_GL_NO_ERROR();

    // Test with a data size that is too small
    glTexImage2DRobustANGLE(GL_TEXTURE_2D, 0, GL_RGBA, dataDimension, dataDimension, 0, GL_RGBA,
                            GL_UNSIGNED_BYTE, static_cast<GLsizei>(rgbaData.size()) / 2,
                            rgbaData.data());
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    glTexSubImage2DRobustANGLE(GL_TEXTURE_2D, 0, 0, 0, dataDimension, dataDimension, GL_RGBA,
                               GL_UNSIGNED_BYTE, static_cast<GLsizei>(rgbaData.size()) / 2,
                               rgbaData.data());
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    if (getClientMajorVersion() >= 3)
    {
        // Set an unpack parameter that would cause the driver to read past the end of the buffer
        glPixelStorei(GL_UNPACK_ROW_LENGTH, dataDimension + 1);
        glTexImage2DRobustANGLE(GL_TEXTURE_2D, 0, GL_RGBA, dataDimension, dataDimension, 0, GL_RGBA,
                                GL_UNSIGNED_BYTE, static_cast<GLsizei>(rgbaData.size()),
                                rgbaData.data());
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    }
}

// Test basic usage and validation of glCompressedTexImage2DRobustANGLE
// and glCompressedTexSubImage2DRobustANGLE
TEST_P(RobustClientMemoryTest, CompressedTexImage2D)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_robust_client_memory"));

    // Either ETC1 or BC1 should be supported everywhere
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_compression_dxt1") &&
                       !IsGLExtensionEnabled("GL_OES_compressed_ETC1_RGB8_texture"));

    const GLenum format = IsGLExtensionEnabled("GL_EXT_texture_compression_dxt1")
                              ? GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
                              : GL_ETC1_RGB8_OES;

    // Both ETC1 and BC1 use 4x4 blocks of 8 bytes
    constexpr GLint smallDimension = 4;
    std::array<GLubyte, smallDimension * smallDimension / 2> smallData;

    constexpr GLint largeDimension = 1024;
    constexpr GLint largeSize      = 1024 * 1024 / 2;

    // Test the regular case
    {
        GLTexture tex;
        glBindTexture(GL_TEXTURE_2D, tex);
        glCompressedTexImage2DRobustANGLE(GL_TEXTURE_2D, 0, format, smallDimension, smallDimension,
                                          0, smallData.size(), smallData.size(), smallData.data());
        EXPECT_GL_NO_ERROR();

        glCompressedTexSubImage2DRobustANGLE(GL_TEXTURE_2D, 0, 0, 0, smallDimension, smallDimension,
                                             format, smallData.size(), smallData.size(),
                                             smallData.data());
        EXPECT_GL_NO_ERROR();
    }

    // Test creating a large texture with small data size
    {
        GLTexture tex;
        glBindTexture(GL_TEXTURE_2D, tex);
        glCompressedTexImage2DRobustANGLE(GL_TEXTURE_2D, 0, format, largeDimension, largeDimension,
                                          0, largeSize, smallData.size(), smallData.data());
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    }

    // Test updating a large texture with small data size
    {
        GLTexture tex;
        glBindTexture(GL_TEXTURE_2D, tex);
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, format, largeDimension, largeDimension, 0,
                               largeSize, nullptr);
        ASSERT_GL_NO_ERROR();

        glCompressedTexImage2DRobustANGLE(GL_TEXTURE_2D, 0, format, largeDimension, largeDimension,
                                          0, largeSize, smallData.size(), smallData.data());
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    }
}

// Test basic usage and validation of glReadPixelsRobustANGLE
TEST_P(RobustClientMemoryTest, ReadPixels)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_robust_client_memory"));

    // TODO(ynovikov): Looks like a driver bug on Intel HD 530 http://anglebug.com/42260689
    ANGLE_SKIP_TEST_IF(IsLinux() && IsIntel() && IsDesktopOpenGL());

    GLsizei dataDimension = 16;
    std::vector<GLubyte> rgbaData(dataDimension * dataDimension * 4);

    // Test the regular case
    GLsizei length = 0;
    GLsizei width  = 0;
    GLsizei height = 0;
    glReadPixelsRobustANGLE(0, 0, dataDimension, dataDimension, GL_RGBA, GL_UNSIGNED_BYTE,
                            static_cast<GLsizei>(rgbaData.size()), &length, &width, &height,
                            rgbaData.data());
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(static_cast<GLsizei>(rgbaData.size()), length);
    EXPECT_EQ(dataDimension, width);
    EXPECT_EQ(dataDimension, height);

    // Test a case that would be partially clipped
    glReadPixelsRobustANGLE(-1, kWindowSize - dataDimension + 3, dataDimension, dataDimension,
                            GL_RGBA, GL_UNSIGNED_BYTE, static_cast<GLsizei>(rgbaData.size()),
                            &length, &width, &height, rgbaData.data());
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(static_cast<GLsizei>(rgbaData.size()), length);
    EXPECT_EQ(dataDimension - 1, width);
    EXPECT_EQ(dataDimension - 3, height);

    // Test with a data size that is too small
    glReadPixelsRobustANGLE(0, 0, dataDimension, dataDimension, GL_RGBA, GL_UNSIGNED_BYTE,
                            static_cast<GLsizei>(rgbaData.size()) - 1, &length, nullptr, nullptr,
                            rgbaData.data());
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);

    if (getClientMajorVersion() >= 3)
    {
        // Set a pack parameter that would cause the driver to write past the end of the buffer
        glPixelStorei(GL_PACK_ROW_LENGTH, dataDimension + 1);
        glReadPixelsRobustANGLE(0, 0, dataDimension, dataDimension, GL_RGBA, GL_UNSIGNED_BYTE,
                                static_cast<GLsizei>(rgbaData.size()), &length, nullptr, nullptr,
                                rgbaData.data());
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    }
}

class RobustClientMemoryNoExtensionsTest : public RobustClientMemoryTest
{
  protected:
    RobustClientMemoryNoExtensionsTest() : RobustClientMemoryTest() { setExtensionsEnabled(false); }
};

// Test with empty result
TEST_P(RobustClientMemoryNoExtensionsTest, GetEmpty)
{
    GLint numFormats = 10;
    glGetIntegerv(GL_NUM_COMPRESSED_TEXTURE_FORMATS, &numFormats);
    ASSERT_GL_NO_ERROR();
    ASSERT_EQ(numFormats, 0);  // Must be zero on unextended OpenGL ES 2.0

    GLsizei length = 1;
    std::vector<GLint> resultBuf(2, 3);

    // Test non-robust with empty response
    {
        glGetIntegerv(GL_COMPRESSED_TEXTURE_FORMATS, resultBuf.data());
        EXPECT_GL_NO_ERROR();

        // Must not touch the buffer
        EXPECT_TRUE(std::all_of(resultBuf.begin(), resultBuf.end(),
                                [](GLint value) { return value == 3; }));
    }

    ANGLE_SKIP_TEST_IF(!EnsureGLExtensionEnabled("GL_ANGLE_robust_client_memory"));

    // Test robust with empty response
    {
        glGetIntegervRobustANGLE(GL_COMPRESSED_TEXTURE_FORMATS, resultBuf.size(), &length,
                                 resultBuf.data());
        EXPECT_GL_NO_ERROR();

        // Must update length
        EXPECT_EQ(length, 0);

        // Must not touch the buffer
        EXPECT_TRUE(std::all_of(resultBuf.begin(), resultBuf.end(),
                                [](GLint value) { return value == 3; }));
    }

    // Test robust with empty response and null length
    {
        glGetIntegervRobustANGLE(GL_COMPRESSED_TEXTURE_FORMATS, resultBuf.size(), nullptr,
                                 resultBuf.data());
        EXPECT_GL_NO_ERROR();

        // Must not touch the buffer
        EXPECT_TRUE(std::all_of(resultBuf.begin(), resultBuf.end(),
                                [](GLint value) { return value == 3; }));
    }

    // Test robust with empty response, zero buffer size, and null length
    {
        glGetIntegervRobustANGLE(GL_COMPRESSED_TEXTURE_FORMATS, 0, nullptr, resultBuf.data());
        EXPECT_GL_NO_ERROR();

        // Must not touch the buffer
        EXPECT_TRUE(std::all_of(resultBuf.begin(), resultBuf.end(),
                                [](GLint value) { return value == 3; }));
    }
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(RobustClientMemoryTest);

ANGLE_INSTANTIATE_TEST_ES2(RobustClientMemoryNoExtensionsTest);

}  // namespace angle
