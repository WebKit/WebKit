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

// Test basic usage and validation of glReadPixelsRobustANGLE
TEST_P(RobustClientMemoryTest, ReadPixels)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_robust_client_memory"));

    // TODO(ynovikov): Looks like a driver bug on Intel HD 530 http://anglebug.com/42260689
    ANGLE_SKIP_TEST_IF(IsLinux() && IsIntel() && IsDesktopOpenGL());

    GLsizei dataDimension = 16;
    std::vector<GLubyte> rgbaData(dataDimension * dataDimension * 4);

    // Test the regular case
    GLsizei length  = -1;
    GLsizei columns = -1;
    GLsizei rows    = -1;
    glReadPixelsRobustANGLE(0, 0, dataDimension, dataDimension, GL_RGBA, GL_UNSIGNED_BYTE,
                            static_cast<GLsizei>(rgbaData.size()), &length, &columns, &rows,
                            rgbaData.data());
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(static_cast<GLsizei>(rgbaData.size()), length);
    EXPECT_EQ(dataDimension, columns);
    EXPECT_EQ(dataDimension, rows);

    // Test a case that would be partially clipped
    length  = -1;
    columns = -1;
    rows    = -1;
    glReadPixelsRobustANGLE(-1, kWindowSize - dataDimension + 3, dataDimension, dataDimension,
                            GL_RGBA, GL_UNSIGNED_BYTE, static_cast<GLsizei>(rgbaData.size()),
                            &length, &columns, &rows, rgbaData.data());
    EXPECT_GL_NO_ERROR();
    EXPECT_EQ(static_cast<GLsizei>(rgbaData.size()), length);
    EXPECT_EQ(dataDimension - 1, columns);
    EXPECT_EQ(dataDimension - 3, rows);

    length  = 123;
    columns = 456;
    rows    = 789;
    // Test with a data size that is too small, the output params must not be updated
    glReadPixelsRobustANGLE(0, 0, dataDimension, dataDimension, GL_RGBA, GL_UNSIGNED_BYTE,
                            static_cast<GLsizei>(rgbaData.size()) - 1, &length, &columns, &rows,
                            rgbaData.data());
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    EXPECT_EQ(length, 123);
    EXPECT_EQ(columns, 456);
    EXPECT_EQ(rows, 789);

    // Test that all output parameters may be null
    glReadPixelsRobustANGLE(0, 0, dataDimension, dataDimension, GL_RGBA, GL_UNSIGNED_BYTE,
                            static_cast<GLsizei>(rgbaData.size()), nullptr, nullptr, nullptr,
                            rgbaData.data());
    EXPECT_GL_NO_ERROR();

    if (getClientMajorVersion() >= 3)
    {
        {
            // Test that non-negative bufSize is ignored when a PBO is bound
            GLBuffer buffer;
            glBindBuffer(GL_PIXEL_PACK_BUFFER, buffer);
            glBufferData(GL_PIXEL_PACK_BUFFER, rgbaData.size(), nullptr, GL_STATIC_COPY);

            // Zero bufSize must be accepted
            glReadPixelsRobustANGLE(0, 0, dataDimension, dataDimension, GL_RGBA, GL_UNSIGNED_BYTE,
                                    0, nullptr, nullptr, nullptr, reinterpret_cast<void *>(0));
            EXPECT_GL_NO_ERROR();

            // Small bufSize must be accepted
            glReadPixelsRobustANGLE(0, 0, dataDimension, dataDimension, GL_RGBA, GL_UNSIGNED_BYTE,
                                    16, nullptr, nullptr, nullptr, reinterpret_cast<void *>(0));
            EXPECT_GL_NO_ERROR();

            // Test that negative bufSize is still invalid when a PBO is bound
            glReadPixelsRobustANGLE(0, 0, dataDimension, dataDimension, GL_RGBA, GL_UNSIGNED_BYTE,
                                    -1, nullptr, nullptr, nullptr, reinterpret_cast<void *>(0));
            EXPECT_GL_ERROR(GL_INVALID_VALUE);

            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        }

        // Set a pack parameter that would cause the driver to write past the end of the buffer
        glPixelStorei(GL_PACK_ROW_LENGTH, dataDimension + 1);
        glReadPixelsRobustANGLE(0, 0, dataDimension, dataDimension, GL_RGBA, GL_UNSIGNED_BYTE,
                                static_cast<GLsizei>(rgbaData.size()), &length, nullptr, nullptr,
                                rgbaData.data());
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);
    }
}

// Test the output length parameter
TEST_P(RobustClientMemoryTest, ReadPixelsLengthOutput)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_robust_client_memory"));

    // Buffer twice as large as the window
    std::vector<GLubyte> rgbaData(2 * kWindowSize * kWindowSize * 4);

    // Test that output length is computed based on the input dimensions
    {
        // Input x, y, width, height, output length
        const int testValues[7][5] = {
            {0, 0, 0, 0, 0},               // Zero input area
            {0, 0, 128, 0, 0},             // Zero input height
            {0, 0, 0, 128, 0},             // Zero input width
            {0, 0, 128, 128, 65536},       // Full size
            {64, 0, 64, 128, 32768},       // Half width with positive offset
            {0, -128, 128, 256, 131072},   // Full width, double height with negative offset
            {128, 128, 128, 256, 131072},  // Origin OOB, zero intersection
        };

        for (auto &test : testValues)
        {
            GLsizei length = -1;
            glReadPixelsRobustANGLE(test[0], test[1], test[2], test[3], GL_RGBA, GL_UNSIGNED_BYTE,
                                    static_cast<GLsizei>(rgbaData.size()), &length, nullptr,
                                    nullptr, rgbaData.data());
            EXPECT_GL_NO_ERROR();
            EXPECT_EQ(length, test[4]);
        }
    }

    // Test that output length is zero when reading to PBO
    if (getClientMajorVersion() >= 3)
    {
        GLsizei length = -1;
        GLBuffer buffer;
        glBindBuffer(GL_PIXEL_PACK_BUFFER, buffer);
        glBufferData(GL_PIXEL_PACK_BUFFER, rgbaData.size(), nullptr, GL_STATIC_COPY);
        glReadPixelsRobustANGLE(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE,
                                static_cast<GLsizei>(rgbaData.size()), &length, nullptr, nullptr,
                                reinterpret_cast<void *>(0));
        EXPECT_GL_NO_ERROR();
        EXPECT_EQ(length, 0);

        // Unbind and repeat
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        glReadPixelsRobustANGLE(0, 0, 128, 128, GL_RGBA, GL_UNSIGNED_BYTE,
                                static_cast<GLsizei>(rgbaData.size()), &length, nullptr, nullptr,
                                rgbaData.data());
        EXPECT_GL_NO_ERROR();
        EXPECT_EQ(length, 128 * 128 * 4);
    }
}

// Test edge cases of the returned image dimensions
TEST_P(RobustClientMemoryTest, ReadPixelsClipping)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_ANGLE_robust_client_memory"));

    // Buffer twice as large as the window
    std::vector<GLubyte> rgbaData(2 * kWindowSize * kWindowSize * 4);

    // Origin, input dimension, output dimension
    const int testValues[16][3] = {
        {-15, 0, 0},      // Negative origin with zero length
        {-15, 16, 1},     // Negative origin with overlapping region
        {-16, 16, 0},     // Negative origin with adjacent region
        {-17, 16, 0},     // Negative origin with non-overlapping region
        {-16, 144, 128},  // Negative origin with exact length
        {-16, 160, 128},  // Negative origin with large length
        {0, 0, 0},        // Zero origin with zero length
        {0, 128, 128},    // Zero origin with exact length
        {0, 16, 16},      // Zero origin with small length
        {0, 144, 128},    // Zero origin with large length
        {16, 0, 0},       // Positive origin with zero length
        {16, 112, 112},   // Positive origin with exact length
        {16, 32, 32},     // Positive origin with small length
        {16, 128, 112},   // Positive origin with large width
        {144, 0, 0},      // Out of range origin with zero length
        {144, 16, 0},     // Out of range origin with non-zero length
    };

    for (auto &test : testValues)
    {
        // Test columns and rows separately
        {
            GLsizei columns = -1;
            glReadPixelsRobustANGLE(test[0], 0, test[1], 128, GL_RGBA, GL_UNSIGNED_BYTE,
                                    static_cast<GLsizei>(rgbaData.size()), nullptr, &columns,
                                    nullptr, rgbaData.data());
            EXPECT_GL_NO_ERROR();
            EXPECT_EQ(columns, test[2]);

            GLsizei rows = -1;
            glReadPixelsRobustANGLE(0, test[0], 128, test[1], GL_RGBA, GL_UNSIGNED_BYTE,
                                    static_cast<GLsizei>(rgbaData.size()), nullptr, nullptr, &rows,
                                    rgbaData.data());
            EXPECT_GL_NO_ERROR();
            EXPECT_EQ(rows, test[2]);
        }

        // Test that if one output is zero, then the other output is also zero
        if (test[2] == 0)
        {
            GLsizei columns = -1;
            GLsizei rows    = -1;
            glReadPixelsRobustANGLE(test[0], 0, test[1], 128, GL_RGBA, GL_UNSIGNED_BYTE,
                                    static_cast<GLsizei>(rgbaData.size()), nullptr, &columns, &rows,
                                    rgbaData.data());
            EXPECT_GL_NO_ERROR();
            EXPECT_EQ(columns, 0);
            EXPECT_EQ(rows, 0);

            columns = -1;
            rows    = -1;
            glReadPixelsRobustANGLE(0, test[0], 128, test[1], GL_RGBA, GL_UNSIGNED_BYTE,
                                    static_cast<GLsizei>(rgbaData.size()), nullptr, &columns, &rows,
                                    rgbaData.data());
            EXPECT_GL_NO_ERROR();
            EXPECT_EQ(columns, 0);
            EXPECT_EQ(rows, 0);
        }
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
