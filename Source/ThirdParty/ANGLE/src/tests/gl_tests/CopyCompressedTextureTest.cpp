//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// CopyCompressedTextureTest.cpp: Tests of the GL_CHROMIUM_copy_compressed_texture extension

#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

namespace angle
{

class CopyCompressedTextureTest : public ANGLETest<>
{
  protected:
    CopyCompressedTextureTest()
    {
        setWindowWidth(256);
        setWindowHeight(256);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }

    void testSetUp() override
    {
        glGenTextures(2, mTextures);

        constexpr char kVS[] =
            "attribute vec2 a_position;\n"
            "varying vec2 v_texcoord;\n"
            "void main()\n"
            "{\n"
            "   gl_Position = vec4(a_position, 0.0, 1.0);\n"
            "   v_texcoord = (a_position + 1.0) * 0.5;\n"
            "}\n";

        constexpr char kFS[] =
            "precision mediump float;\n"
            "uniform sampler2D u_texture;\n"
            "varying vec2 v_texcoord;\n"
            "void main()\n"
            "{\n"
            "    gl_FragColor = texture2D(u_texture, v_texcoord);\n"
            "}\n";

        mProgram = CompileProgram(kVS, kFS);
        ASSERT_NE(0u, mProgram);
    }

    void testTearDown() override
    {
        glDeleteTextures(2, mTextures);
        glDeleteProgram(mProgram);
    }

    bool checkExtensions() const
    {
        if (!IsGLExtensionEnabled("GL_CHROMIUM_copy_compressed_texture"))
        {
            std::cout
                << "Test skipped because GL_CHROMIUM_copy_compressed_texture is not available."
                << std::endl;
            return false;
        }

#if !defined(GL_GLEXT_PROTOTYPES) || !GL_GLEXT_PROTOTYPES
        EXPECT_NE(nullptr, glCompressedCopyTextureCHROMIUM);
        if (glCompressedCopyTextureCHROMIUM == nullptr)
        {
            return false;
        }
#endif
        return true;
    }

    GLuint mProgram     = 0;
    GLuint mTextures[2] = {0, 0};
};

namespace
{

const GLColor &CompressedImageColor = GLColor::red;

// Single compressed ATC block of source pixels all set to:
// CompressedImageColor.
const uint8_t CompressedImageATC[8] = {0x0, 0x7c, 0x0, 0xf8, 0x55, 0x55, 0x55, 0x55};

// Single compressed ATCIA block of source pixels all set to:
// CompressedImageColor.
const uint8_t CompressedImageATCIA[16] = {0xff, 0xff, 0x0, 0x0,  0x0,  0x0,  0x0,  0x0,
                                          0x0,  0x7c, 0x0, 0xf8, 0x55, 0x55, 0x55, 0x55};

// Single compressed DXT1 block of source pixels all set to:
// CompressedImageColor.
const uint8_t CompressedImageDXT1[8] = {0x00, 0xf8, 0x00, 0xf8, 0xaa, 0xaa, 0xaa, 0xaa};

// Single compressed DXT5 block of source pixels all set to:
// CompressedImageColor.
const uint8_t CompressedImageDXT5[16] = {0xff, 0xff, 0x0, 0x0,  0x0,  0x0,  0x0,  0x0,
                                         0x0,  0xf8, 0x0, 0xf8, 0xaa, 0xaa, 0xaa, 0xaa};

// Single compressed DXT1 block of source pixels all set to:
// CompressedImageColor.
const uint8_t CompressedImageETC1[8] = {0x0, 0x0, 0xf8, 0x2, 0xff, 0xff, 0x0, 0x0};

}  // anonymous namespace

// Test to ensure that the basic functionality of the extension works.
TEST_P(CopyCompressedTextureTest, Basic)
{
    ANGLE_SKIP_TEST_IF(!checkExtensions());

    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_compression_dxt1"));

    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 4, 0,
                           sizeof(CompressedImageDXT1), CompressedImageDXT1);
    ASSERT_GL_NO_ERROR();

    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glCompressedCopyTextureCHROMIUM(mTextures[0], mTextures[1]);
    ASSERT_GL_NO_ERROR();

    // Load texture.
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    GLint textureLoc = glGetUniformLocation(mProgram, "u_texture");
    glUseProgram(mProgram);
    glUniform1i(textureLoc, 0);

    // Draw.
    drawQuad(mProgram, "a_position", 0.5f);

    EXPECT_PIXEL_COLOR_EQ(getWindowWidth() / 2, getWindowHeight() / 2, CompressedImageColor);
    ASSERT_GL_NO_ERROR();
}

// Test validation of compressed formats
TEST_P(CopyCompressedTextureTest, InternalFormat)
{
    if (!checkExtensions())
    {
        return;
    }

    struct Data
    {
        GLint format;
        const uint8_t *data;
        GLsizei dataSize;

        Data() : Data(GL_NONE, nullptr, 0) {}
        Data(GLint format, const uint8_t *data, GLsizei dataSize)
            : format(format), data(data), dataSize(dataSize)
        {}
    };
    std::vector<Data> supportedFormats;

    if (IsGLExtensionEnabled("GL_AMD_compressed_ATC_texture"))
    {
        supportedFormats.push_back(
            Data(GL_ATC_RGB_AMD, CompressedImageATC, sizeof(CompressedImageATC)));
        supportedFormats.push_back(Data(GL_ATC_RGBA_INTERPOLATED_ALPHA_AMD, CompressedImageATCIA,
                                        sizeof(CompressedImageATCIA)));
    }
    if (IsGLExtensionEnabled("GL_EXT_texture_compression_dxt1"))
    {
        supportedFormats.push_back(Data(GL_COMPRESSED_RGB_S3TC_DXT1_EXT, CompressedImageDXT1,
                                        sizeof(CompressedImageDXT1)));
    }
    if (IsGLExtensionEnabled("GL_ANGLE_texture_compression_dxt5"))
    {
        supportedFormats.push_back(Data(GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, CompressedImageDXT5,
                                        sizeof(CompressedImageDXT5)));
    }
    if (IsGLExtensionEnabled("GL_OES_compressed_ETC1_RGB8_texture"))
    {
        supportedFormats.push_back(
            Data(GL_ETC1_RGB8_OES, CompressedImageETC1, sizeof(CompressedImageETC1)));
    }

    for (const auto &supportedFormat : supportedFormats)
    {
        glBindTexture(GL_TEXTURE_2D, mTextures[0]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, supportedFormat.format, 4, 4, 0,
                               supportedFormat.dataSize, supportedFormat.data);
        ASSERT_GL_NO_ERROR();

        glBindTexture(GL_TEXTURE_2D, mTextures[1]);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glCompressedCopyTextureCHROMIUM(mTextures[0], mTextures[1]);
        ASSERT_GL_NO_ERROR();
    }
}

// Test that uncompressed textures generate errors when copying
TEST_P(CopyCompressedTextureTest, InternalFormatNotSupported)
{
    if (!checkExtensions())
    {
        return;
    }

    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &GLColor::red);
    ASSERT_GL_NO_ERROR();

    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    ASSERT_GL_NO_ERROR();

    // Check that the GL_RGBA format reports an error.
    glCompressedCopyTextureCHROMIUM(mTextures[0], mTextures[1]);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Test that uncompressed to compressed textures generate errors when copying
TEST_P(CopyCompressedTextureTest, UncompressedToCompressed)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_copy_image"));

    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &GLColor::red);
    ASSERT_GL_NO_ERROR();

    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 1, 1, 0, 16, NULL);
    ASSERT_GL_NO_ERROR();

    // return GL_INVALID_OPERATION because the two formats are not compatible
    glCopyImageSubDataEXT(mTextures[0], GL_TEXTURE_2D, 0, 0, 0, 0, mTextures[1], GL_TEXTURE_2D, 0,
                          0, 0, 0, 1, 1, 1);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

// Test validation of texture IDs
TEST_P(CopyCompressedTextureTest, InvalidTextureIds)
{
    if (!checkExtensions())
    {
        return;
    }

    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_compression_dxt1"));

    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 4, 0,
                           sizeof(CompressedImageDXT1), CompressedImageDXT1);
    ASSERT_GL_NO_ERROR();

    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    ASSERT_GL_NO_ERROR();

    glCompressedCopyTextureCHROMIUM(mTextures[0], 99993);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glCompressedCopyTextureCHROMIUM(99994, mTextures[1]);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glCompressedCopyTextureCHROMIUM(99995, 99996);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);

    glCompressedCopyTextureCHROMIUM(mTextures[0], mTextures[1]);
    EXPECT_GL_NO_ERROR();
}

// Test that only 2D textures are valid
TEST_P(CopyCompressedTextureTest, BindingPoints)
{
    if (!checkExtensions())
    {
        return;
    }

    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_compression_dxt1"));

    glBindTexture(GL_TEXTURE_CUBE_MAP, mTextures[0]);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    for (GLenum face = GL_TEXTURE_CUBE_MAP_POSITIVE_X; face <= GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
         face++)
    {
        glCompressedTexImage2D(face, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 4, 0,
                               sizeof(CompressedImageDXT1), CompressedImageDXT1);
    }
    ASSERT_GL_NO_ERROR();

    glBindTexture(GL_TEXTURE_CUBE_MAP, mTextures[1]);
    glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    ASSERT_GL_NO_ERROR();

    glCompressedCopyTextureCHROMIUM(mTextures[0], mTextures[1]);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Test the destination texture cannot be immutable
TEST_P(CopyCompressedTextureTest, Immutable)
{
    if (!checkExtensions() || getClientMajorVersion() < 3)
    {
        return;
    }

    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_texture_compression_dxt1"));

    glBindTexture(GL_TEXTURE_2D, mTextures[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 4, 0,
                           sizeof(CompressedImageDXT1), CompressedImageDXT1);
    ASSERT_GL_NO_ERROR();

    glBindTexture(GL_TEXTURE_2D, mTextures[1]);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 4, 4);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    ASSERT_GL_NO_ERROR();

    glCompressedCopyTextureCHROMIUM(mTextures[0], mTextures[1]);
    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
}

class CopyCompressedTextureTestES31 : public ANGLETest<>
{
  protected:
    CopyCompressedTextureTestES31()
    {
        setWindowWidth(256);
        setWindowHeight(256);
        setConfigRedBits(8);
        setConfigGreenBits(8);
        setConfigBlueBits(8);
        setConfigAlphaBits(8);
    }
};

// Test that if the copy subregion depth is bigger than the depth range of either source texture
// image or destination texture image, glCopyImageSubData() fails with GL_INVALID_VALUE
TEST_P(CopyCompressedTextureTestES31, CopyRegionDepthOverflow)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_copy_image"));
    // Initialize texture data
    std::vector<uint8_t> compressedSrcImgDataLevel0;
    for (uint8_t i = 1; i < 32 + 1; ++i)
    {
        compressedSrcImgDataLevel0.push_back(i);
    }

    std::vector<uint8_t> compressedSrcImgDataLevel1;
    for (uint8_t i = 1; i < 16 + 1; ++i)
    {
        compressedSrcImgDataLevel1.push_back(i);
    }

    // Allocate storage for compressedTexture2D, and fills each of 2 levels with the texture data
    GLTexture compressedTexture2D;
    glBindTexture(GL_TEXTURE_2D, compressedTexture2D);
    glTexStorage2D(GL_TEXTURE_2D, 2, GL_COMPRESSED_RGBA_ASTC_6x6, 8, 4);
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 8, 4, GL_COMPRESSED_RGBA_ASTC_6x6, 32,
                              compressedSrcImgDataLevel0.data());
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, 4, 2, GL_COMPRESSED_RGBA_ASTC_6x6, 16,
                              compressedSrcImgDataLevel1.data());

    // Allocate storage for compressedTexture2DArray, and fills each of 2 levels with the texture
    // data
    GLTexture compressedTexture2DArray;
    glBindTexture(GL_TEXTURE_2D_ARRAY, compressedTexture2DArray);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 2, GL_COMPRESSED_RGBA_ASTC_6x6, 8, 4, 2);
    glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 8, 4, 1, GL_COMPRESSED_RGBA_ASTC_6x6,
                              32, compressedSrcImgDataLevel0.data());
    glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 1, 8, 4, 1, GL_COMPRESSED_RGBA_ASTC_6x6,
                              32, compressedSrcImgDataLevel0.data());
    glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 1, 0, 0, 0, 4, 2, 1, GL_COMPRESSED_RGBA_ASTC_6x6,
                              16, compressedSrcImgDataLevel1.data());
    glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 1, 0, 0, 1, 4, 2, 1, GL_COMPRESSED_RGBA_ASTC_6x6,
                              16, compressedSrcImgDataLevel1.data());

    // Perform a copy from compressedTexture2D mipmap 0 to compressedTexture2DArray mipmap 0, where
    // the copy region depth is bigger than the depth of source texture mTexture2D mipmap 0. This
    // should fail with GL_INVALID_VALUE.
    glCopyImageSubDataEXT(compressedTexture2D, GL_TEXTURE_2D, 0, 0, 0, 0, compressedTexture2DArray,
                          GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 8, 4, 2);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
    // Perform a copy from compressedTexture2DArray mipmap 0 to compressedTexture2D mipmap 0, where
    // the copy region depth is bigger than the depth of destination texture mTexture2D mipmap 0.
    // This should fail with GL_INVALID_VALUE.
    glCopyImageSubDataEXT(compressedTexture2DArray, GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0,
                          compressedTexture2D, GL_TEXTURE_2D, 0, 0, 0, 0, 8, 4, 2);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Test that if the copy subregion width and height equals to the texture level width and height,
// even if width and height are not aligned with the compressed texture block size, the
// glCopyImageSubData() should be allowed.
TEST_P(CopyCompressedTextureTestES31, CopyRegionOccupiesEntireMipDoNotNeedAlignment)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_copy_image"));
    // Initialize texture data
    std::vector<uint8_t> compressedSrcImgDataLevel0;
    for (uint8_t i = 1; i < 32 + 1; ++i)
    {
        compressedSrcImgDataLevel0.push_back(i);
    }

    std::vector<uint8_t> compressedSrcImgDataLevel1;
    for (uint8_t i = 1; i < 16 + 1; ++i)
    {
        compressedSrcImgDataLevel1.push_back(i);
    }

    // Allocate storage for compressedTexture2D, and fills each of 2 levels with the texture data
    GLTexture compressedTexture2D;
    glBindTexture(GL_TEXTURE_2D, compressedTexture2D);
    glTexStorage2D(GL_TEXTURE_2D, 2, GL_COMPRESSED_RGBA_ASTC_6x6, 8, 4);
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 8, 4, GL_COMPRESSED_RGBA_ASTC_6x6, 32,
                              compressedSrcImgDataLevel0.data());
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, 4, 2, GL_COMPRESSED_RGBA_ASTC_6x6, 16,
                              compressedSrcImgDataLevel1.data());

    // Allocate storage for compressedTexture2DArray, and fills each of 2 levels with the texture
    // data
    GLTexture compressedTexture2DArray;
    glBindTexture(GL_TEXTURE_2D_ARRAY, compressedTexture2DArray);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 2, GL_COMPRESSED_RGBA_ASTC_6x6, 8, 4, 2);
    glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 8, 4, 1, GL_COMPRESSED_RGBA_ASTC_6x6,
                              32, compressedSrcImgDataLevel0.data());
    glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 1, 8, 4, 1, GL_COMPRESSED_RGBA_ASTC_6x6,
                              32, compressedSrcImgDataLevel0.data());
    glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 1, 0, 0, 0, 4, 2, 1, GL_COMPRESSED_RGBA_ASTC_6x6,
                              16, compressedSrcImgDataLevel1.data());
    glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 1, 0, 0, 1, 4, 2, 1, GL_COMPRESSED_RGBA_ASTC_6x6,
                              16, compressedSrcImgDataLevel1.data());

    // Perform a copy from compressedTexture2D mipmap 0 to compressedTexture2DArray mipmap 0.
    // This should succeed. Even if the width and height are not multiple of 6, the region covers
    // the entire mipmap 0 of source texture mTexture2D, and the region covers the entire slice 0 of
    // mipmap 0 of destination texture mTexture2DArray
    glCopyImageSubDataEXT(compressedTexture2D, GL_TEXTURE_2D, 0, 0, 0, 0, compressedTexture2DArray,
                          GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 8, 4, 1);
    EXPECT_GL_NO_ERROR();
}

// Test that if the copy region offsets are not aligned with the compressed texture block size,
// glCopyImageSubData() should fail with a validation error.
TEST_P(CopyCompressedTextureTestES31, CopyRegionoOffSetNotAlignedShouldGenerateGLError)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_copy_image"));
    // Initialize texture data
    std::vector<uint8_t> compressedSrcImgDataLevel0;
    for (uint8_t i = 1; i < 32 + 1; ++i)
    {
        compressedSrcImgDataLevel0.push_back(i);
    }

    std::vector<uint8_t> compressedSrcImgDataLevel1;
    for (uint8_t i = 1; i < 16 + 1; ++i)
    {
        compressedSrcImgDataLevel1.push_back(i);
    }

    // Allocate storage for compressedTexture2D, and fills each of 2 levels with the texture data
    GLTexture compressedTexture2D;
    glBindTexture(GL_TEXTURE_2D, compressedTexture2D);
    glTexStorage2D(GL_TEXTURE_2D, 2, GL_COMPRESSED_RGBA_ASTC_6x6, 8, 4);
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 8, 4, GL_COMPRESSED_RGBA_ASTC_6x6, 32,
                              compressedSrcImgDataLevel0.data());
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 1, 0, 0, 4, 2, GL_COMPRESSED_RGBA_ASTC_6x6, 16,
                              compressedSrcImgDataLevel1.data());

    // Allocate storage for compressedTexture2DArray, and fills each of 2 levels with the texture
    // data
    GLTexture compressedTexture2DArray;
    glBindTexture(GL_TEXTURE_2D_ARRAY, compressedTexture2DArray);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 2, GL_COMPRESSED_RGBA_ASTC_6x6, 8, 4, 2);
    glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 8, 4, 1, GL_COMPRESSED_RGBA_ASTC_6x6,
                              32, compressedSrcImgDataLevel0.data());
    glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 1, 8, 4, 1, GL_COMPRESSED_RGBA_ASTC_6x6,
                              32, compressedSrcImgDataLevel0.data());
    glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 1, 0, 0, 0, 4, 2, 1, GL_COMPRESSED_RGBA_ASTC_6x6,
                              16, compressedSrcImgDataLevel1.data());
    glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 1, 0, 0, 1, 4, 2, 1, GL_COMPRESSED_RGBA_ASTC_6x6,
                              16, compressedSrcImgDataLevel1.data());

    // Perform a copy from compressedTexture2D mipmap 0 to compressedTexture2DArray mipmap 0.
    // The copy region offset srcX does not aligned at multiple of 6.
    // This should fail with GL_INVALID_VALUE
    glCopyImageSubDataEXT(compressedTexture2D, GL_TEXTURE_2D, 0, 2, 0, 0, compressedTexture2DArray,
                          GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 6, 4, 1);
    EXPECT_GL_ERROR(GL_INVALID_VALUE);
}

// Test that copying from uncompressed texture to compressed texture works, if their internal
// formats are compatible, and the transformed compressed texture copy subregion aligns with the
// compressed texture block.
TEST_P(CopyCompressedTextureTestES31, CopyFromUncompressedToCompressed)
{
    ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled("GL_EXT_copy_image"));

    GLTexture uncompressedTexture2D;
    glBindTexture(GL_TEXTURE_2D, uncompressedTexture2D);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32UI, 2, 1);
    constexpr GLuint kTex2DData[] = {0, 0, 0, 0, 0, 0, 0, 0};
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 2, 1, GL_RGBA_INTEGER, GL_UNSIGNED_INT, kTex2DData);
    ASSERT_GL_NO_ERROR();

    // Initialize compressed texture data
    std::vector<uint8_t> compressedSrcImgDataLevel0;
    for (uint8_t i = 1; i < 32 + 1; ++i)
    {
        compressedSrcImgDataLevel0.push_back(i);
    }

    // Allocate storage for compressedTexture2D, and fills with texture data
    GLTexture compressedTexture2D;
    glBindTexture(GL_TEXTURE_2D, compressedTexture2D);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_COMPRESSED_RGBA_ASTC_6x6, 12, 6);
    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 12, 6, GL_COMPRESSED_RGBA_ASTC_6x6, 32,
                              compressedSrcImgDataLevel0.data());

    glCopyImageSubDataEXT(uncompressedTexture2D, GL_TEXTURE_2D, 0, 0, 0, 0, compressedTexture2D,
                          GL_TEXTURE_2D, 0, 0, 0, 0, 2, 1, 1);
    EXPECT_GL_NO_ERROR();
}

// Use this to select which configurations (e.g. which renderer, which GLES major version) these
// tests should be run against.
ANGLE_INSTANTIATE_TEST_ES2_AND_ES3(CopyCompressedTextureTest);

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CopyCompressedTextureTestES31);
ANGLE_INSTANTIATE_TEST_ES31(CopyCompressedTextureTestES31);

}  // namespace angle
