//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// CompressedTextureFormatsTest:
//   Tests that only the appropriate entry points are affected after
//   enabling compressed texture extensions.
//

#include "common/gl_enum_utils.h"
#include "test_utils/ANGLETest.h"
#include "test_utils/gl_raii.h"

using namespace angle;

namespace
{

struct FormatInfo
{
    GLenum internalFormat;
    GLenum format;
    GLenum sizedFormat;
    bool issRGB;
    struct
    {
        GLsizei x, y;
    } blockSize;
};

// List of compressed texture formats (table 8.17)
const FormatInfo compressedFormats[] = {
    // ETC (table C.2)
    // internalFormat, format, sizedFormat, issRGB, blockSize
    {GL_COMPRESSED_R11_EAC, GL_RED, GL_R8, false, {4, 4}},
    {GL_COMPRESSED_SIGNED_R11_EAC, GL_RED, GL_R8, false, {4, 4}},
    {GL_COMPRESSED_RG11_EAC, GL_RG, GL_RG8, false, {4, 4}},
    {GL_COMPRESSED_SIGNED_RG11_EAC, GL_RG, GL_RG8, false, {4, 4}},
    {GL_COMPRESSED_RGB8_ETC2, GL_RGB, GL_RGB8, false, {4, 4}},
    {GL_COMPRESSED_SRGB8_ETC2, GL_RGB, GL_SRGB8, true, {4, 4}},
    {GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2, GL_RGBA, GL_RGBA8, false, {4, 4}},
    {GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2, GL_RGBA, GL_SRGB8_ALPHA8, true, {4, 4}},
    {GL_COMPRESSED_RGBA8_ETC2_EAC, GL_RGBA, GL_RGBA8, false, {4, 4}},
    {GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC, GL_RGBA, GL_SRGB8_ALPHA8, true, {4, 4}},
    // ASTC (table C.1)
    // internalFormat, format, sizedFormat, issRGB, blockSize
    {GL_COMPRESSED_RGBA_ASTC_4x4, GL_RGBA, GL_RGBA8, false, {4, 4}},
    {GL_COMPRESSED_RGBA_ASTC_5x4, GL_RGBA, GL_RGBA8, false, {5, 4}},
    {GL_COMPRESSED_RGBA_ASTC_5x5, GL_RGBA, GL_RGBA8, false, {5, 5}},
    {GL_COMPRESSED_RGBA_ASTC_6x5, GL_RGBA, GL_RGBA8, false, {6, 5}},
    {GL_COMPRESSED_RGBA_ASTC_6x6, GL_RGBA, GL_RGBA8, false, {6, 6}},
    {GL_COMPRESSED_RGBA_ASTC_8x5, GL_RGBA, GL_RGBA8, false, {8, 5}},
    {GL_COMPRESSED_RGBA_ASTC_8x6, GL_RGBA, GL_RGBA8, false, {8, 6}},
    {GL_COMPRESSED_RGBA_ASTC_8x8, GL_RGBA, GL_RGBA8, false, {8, 8}},
    {GL_COMPRESSED_RGBA_ASTC_10x5, GL_RGBA, GL_RGBA8, false, {10, 5}},
    {GL_COMPRESSED_RGBA_ASTC_10x6, GL_RGBA, GL_RGBA8, false, {10, 6}},
    {GL_COMPRESSED_RGBA_ASTC_10x8, GL_RGBA, GL_RGBA8, false, {10, 8}},
    {GL_COMPRESSED_RGBA_ASTC_10x10, GL_RGBA, GL_RGBA8, false, {10, 10}},
    {GL_COMPRESSED_RGBA_ASTC_12x10, GL_RGBA, GL_RGBA8, false, {12, 10}},
    {GL_COMPRESSED_RGBA_ASTC_12x12, GL_RGBA, GL_RGBA8, false, {12, 12}},
    {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4, GL_RGBA, GL_SRGB8_ALPHA8, true, {4, 4}},
    {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4, GL_RGBA, GL_SRGB8_ALPHA8, true, {5, 4}},
    {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5, GL_RGBA, GL_SRGB8_ALPHA8, true, {5, 5}},
    {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5, GL_RGBA, GL_SRGB8_ALPHA8, true, {6, 5}},
    {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6, GL_RGBA, GL_SRGB8_ALPHA8, true, {6, 6}},
    {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5, GL_RGBA, GL_SRGB8_ALPHA8, true, {8, 5}},
    {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6, GL_RGBA, GL_SRGB8_ALPHA8, true, {8, 6}},
    {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8, GL_RGBA, GL_SRGB8_ALPHA8, true, {8, 8}},
    {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5, GL_RGBA, GL_SRGB8_ALPHA8, true, {10, 5}},
    {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6, GL_RGBA, GL_SRGB8_ALPHA8, true, {10, 6}},
    {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8, GL_RGBA, GL_SRGB8_ALPHA8, true, {10, 8}},
    {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10, GL_RGBA, GL_SRGB8_ALPHA8, true, {10, 10}},
    {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10, GL_RGBA, GL_SRGB8_ALPHA8, true, {12, 10}},
    {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12, GL_RGBA, GL_SRGB8_ALPHA8, true, {12, 12}},
};

const FormatInfo *getCompressedFormatInfo(GLenum format)
{
    for (const FormatInfo &item : compressedFormats)
    {
        if (item.internalFormat == format)
        {
            return &item;
        }
    }
    return nullptr;
}

using FormatDesc                  = std::pair<GLenum, GLsizei>;
using CompressedTextureTestParams = std::tuple<angle::PlatformParameters, FormatDesc>;

class CompressedTextureFormatsTest : public ANGLETest<CompressedTextureTestParams>
{
  public:
    CompressedTextureFormatsTest(const std::string ext1,
                                 const std::string ext2,
                                 const bool supportsUpdates,
                                 const bool supports2DArray,
                                 const bool supports3D,
                                 const bool alwaysOnES3)
        : mExtNames({ext1, ext2}),
          mSupportsUpdates(supportsUpdates),
          mSupports2DArray(supports2DArray),
          mSupports3D(supports3D),
          mAlwaysOnES3(alwaysOnES3)
    {
        setExtensionsEnabled(false);
    }

    void testSetUp() override
    {
        // Older Metal versions do not support compressed TEXTURE_3D.
        mDisableTexture3D = IsMetal() && !IsMetalCompressedTexture3DAvailable();
    }

    void checkSubImage2D(GLenum format, GLsizei size)
    {
        GLubyte data[32];

        // The semantic of this call is to take uncompressed data, compress it on-the-fly,
        // and perform a partial update of an existing GPU-compressed texture. This
        // operation is not supported in OpenGL ES.
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);

        // Compressed texture extensions never extend TexSubImage2D.
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, format, GL_UNSIGNED_BYTE, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);

        // The semantic of this call is to take pixel data from the current framebuffer, compress it
        // on-the-fly, and perform a partial update of an existing GPU-compressed texture. This
        // operation is not supported in OpenGL ES.
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 4, 4);
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);

        glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, format, size, data);
        EXPECT_GL_ERROR(mSupportsUpdates ? GL_NO_ERROR : GL_INVALID_OPERATION);

        const FormatInfo *formatInfo = getCompressedFormatInfo(format);
        if (formatInfo)
        {
            // Try offset which is not aligned with block size. This operation is not supported
            // in OpenGL ES.
            glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, formatInfo->blockSize.x - 2, 0, 4, 4,
                                      formatInfo->internalFormat, size, data);
            EXPECT_GL_ERROR(GL_INVALID_OPERATION);

            glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, formatInfo->blockSize.y - 2, 4, 4,
                                      formatInfo->internalFormat, size, data);
            EXPECT_GL_ERROR(GL_INVALID_OPERATION);
        }
    }

    void checkSubImage3D(GLenum target, GLenum format, GLsizei size)
    {
        GLubyte data[32];

        // The semantic of this call is to take uncompressed data, compress it on-the-fly,
        // and perform a partial update of an existing GPU-compressed texture. This
        // operation is not supported in OpenGL ES.
        glTexSubImage3D(target, 0, 0, 0, 0, 4, 4, 1, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);

        // Compressed texture extensions never extend TexSubImage3D.
        glTexSubImage3D(target, 0, 0, 0, 0, 4, 4, 1, format, GL_UNSIGNED_BYTE, nullptr);
        EXPECT_GL_ERROR(GL_INVALID_ENUM);

        // The semantic of this call is to take pixel data from the current framebuffer, compress it
        // on-the-fly, and perform a partial update of an existing GPU-compressed texture. This
        // operation is not supported in OpenGL ES.
        glCopyTexSubImage3D(target, 0, 0, 0, 0, 0, 0, 4, 4);
        EXPECT_GL_ERROR(GL_INVALID_OPERATION);

        glCompressedTexSubImage3D(target, 0, 0, 0, 0, 4, 4, 1, format, size, data);
        EXPECT_GL_NO_ERROR();
    }

    void check2D(const bool compressedFormatEnabled)
    {
        const GLenum format = ::testing::get<1>(GetParam()).first;
        const GLsizei size  = ::testing::get<1>(GetParam()).second;

        {
            GLTexture texture;
            glBindTexture(GL_TEXTURE_2D, texture);

            // The semantic of this call is to take uncompressed data and compress it on-the-fly.
            // This operation is not supported in OpenGL ES.
            glTexImage2D(GL_TEXTURE_2D, 0, format, 4, 4, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
            EXPECT_GL_ERROR(GL_INVALID_VALUE);

            // Try compressed enum as format. Compressed texture extensions never allow this.
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 4, 4, 0, format, GL_UNSIGNED_BYTE, nullptr);
            EXPECT_GL_ERROR(GL_INVALID_ENUM);

            // The semantic of this call is to take pixel data from the current framebuffer
            // and create a compressed texture from it on-the-fly. This operation is not supported
            // in OpenGL ES.
            glCopyTexImage2D(GL_TEXTURE_2D, 0, format, 0, 0, 4, 4, 0);
            EXPECT_GL_ERROR(GL_INVALID_OPERATION);

            glCompressedTexImage2D(GL_TEXTURE_2D, 0, format, 4, 4, 0, size, nullptr);
            if (compressedFormatEnabled)
            {
                EXPECT_GL_NO_ERROR();

                checkSubImage2D(format, size);
            }
            else
            {
                EXPECT_GL_ERROR(GL_INVALID_ENUM);
            }
        }

        if (getClientMajorVersion() >= 3)
        {
            GLTexture texture;
            glBindTexture(GL_TEXTURE_2D, texture);

            glTexStorage2D(GL_TEXTURE_2D, 1, format, 4, 4);
            if (compressedFormatEnabled)
            {
                EXPECT_GL_NO_ERROR();

                checkSubImage2D(format, size);
            }
            else
            {
                EXPECT_GL_ERROR(GL_INVALID_ENUM);
            }
        }

        if (EnsureGLExtensionEnabled("GL_EXT_texture_storage"))
        {
            GLTexture texture;
            glBindTexture(GL_TEXTURE_2D, texture);

            glTexStorage2DEXT(GL_TEXTURE_2D, 1, format, 4, 4);
            if (compressedFormatEnabled)
            {
                EXPECT_GL_NO_ERROR();

                checkSubImage2D(format, size);
            }
            else
            {
                EXPECT_GL_ERROR(GL_INVALID_ENUM);
            }
        }
    }

    void check3D(GLenum target, const bool compressedFormatEnabled, const bool supportsTarget)
    {
        const GLenum format = ::testing::get<1>(GetParam()).first;
        const GLsizei size  = ::testing::get<1>(GetParam()).second;

        {
            GLTexture texture;
            glBindTexture(target, texture);

            // Try compressed enum as internalformat. The semantic of this call is to take
            // uncompressed data and compress it on-the-fly. This operation is not supported in
            // OpenGL ES.
            glTexImage3D(target, 0, format, 4, 4, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
            EXPECT_GL_ERROR(GL_INVALID_VALUE);

            // Try compressed enum as format. Compressed texture extensions never allow this.
            glTexImage3D(target, 0, GL_RGB, 4, 4, 1, 0, format, GL_UNSIGNED_BYTE, nullptr);
            EXPECT_GL_ERROR(GL_INVALID_ENUM);

            glCompressedTexImage3D(target, 0, format, 4, 4, 1, 0, size, nullptr);
            if (compressedFormatEnabled)
            {
                if (supportsTarget)
                {
                    EXPECT_GL_NO_ERROR();

                    checkSubImage3D(target, format, size);
                }
                else
                {
                    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
                }
            }
            else
            {
                EXPECT_GL_ERROR(GL_INVALID_ENUM);
            }
        }

        {
            GLTexture texture;
            glBindTexture(target, texture);

            glTexStorage3D(target, 1, format, 4, 4, 1);
            if (compressedFormatEnabled)
            {
                if (supportsTarget)
                {
                    EXPECT_GL_NO_ERROR();

                    checkSubImage3D(target, format, size);
                }
                else
                {
                    EXPECT_GL_ERROR(GL_INVALID_OPERATION);
                }
            }
            else
            {
                EXPECT_GL_ERROR(GL_INVALID_ENUM);
            }
        }
    }

    void test()
    {
        // ETC2/EAC formats always pass validation on ES3 contexts but in some cases fail in drivers
        // because their emulation is not implemented for OpenGL renderer.
        // https://crbug.com/angleproject/6300
        if (mAlwaysOnES3)
        {
            ANGLE_SKIP_TEST_IF(getClientMajorVersion() >= 3 &&
                               !IsGLExtensionRequestable(mExtNames[0]));
        }

        // It's not possible to disable ETC2/EAC support on ES 3.0.
        const bool compressedFormatEnabled = mAlwaysOnES3 && getClientMajorVersion() >= 3;
        check2D(compressedFormatEnabled);
        if (getClientMajorVersion() >= 3)
        {
            check3D(GL_TEXTURE_2D_ARRAY, compressedFormatEnabled, mSupports2DArray);
            check3D(GL_TEXTURE_3D, compressedFormatEnabled, mSupports3D && !mDisableTexture3D);
        }

        for (const std::string &extName : mExtNames)
        {
            if (!extName.empty())
            {
                if (IsGLExtensionRequestable(extName))
                {
                    glRequestExtensionANGLE(extName.c_str());
                }
                ANGLE_SKIP_TEST_IF(!IsGLExtensionEnabled(extName));
            }
        }

        // Repeat all checks after enabling the extensions.
        check2D(true);
        if (getClientMajorVersion() >= 3)
        {
            check3D(GL_TEXTURE_2D_ARRAY, true, mSupports2DArray);
            check3D(GL_TEXTURE_3D, true, mSupports3D && !mDisableTexture3D);
        }
    }

  private:
    bool mDisableTexture3D = false;
    const std::vector<std::string> mExtNames;
    const bool mSupportsUpdates;
    const bool mSupports2DArray;
    const bool mSupports3D;
    const bool mAlwaysOnES3;
};

template <char const *ext1,
          char const *ext2,
          bool supports_updates,
          bool supports_2d_array,
          bool supports_3d,
          bool always_on_es3>
class _Test : public CompressedTextureFormatsTest
{
  public:
    _Test()
        : CompressedTextureFormatsTest(ext1,
                                       ext2,
                                       supports_updates,
                                       supports_2d_array,
                                       supports_3d,
                                       always_on_es3)
    {}
};

const char kDXT1[]     = "GL_EXT_texture_compression_dxt1";
const char kDXT3[]     = "GL_ANGLE_texture_compression_dxt3";
const char kDXT5[]     = "GL_ANGLE_texture_compression_dxt5";
const char kS3TCSRGB[] = "GL_EXT_texture_compression_s3tc_srgb";
const char kRGTC[]     = "GL_EXT_texture_compression_rgtc";
const char kBPTC[]     = "GL_EXT_texture_compression_bptc";

const char kETC1[]    = "GL_OES_compressed_ETC1_RGB8_texture";
const char kETC1Sub[] = "GL_EXT_compressed_ETC1_RGB8_sub_texture";

const char kEACR11U[]  = "GL_OES_compressed_EAC_R11_unsigned_texture";
const char kEACR11S[]  = "GL_OES_compressed_EAC_R11_signed_texture";
const char kEACRG11U[] = "GL_OES_compressed_EAC_RG11_unsigned_texture";
const char kEACRG11S[] = "GL_OES_compressed_EAC_RG11_signed_texture";

const char kETC2RGB8[]       = "GL_OES_compressed_ETC2_RGB8_texture";
const char kETC2RGB8SRGB[]   = "GL_OES_compressed_ETC2_sRGB8_texture";
const char kETC2RGB8A1[]     = "GL_OES_compressed_ETC2_punchthroughA_RGBA8_texture";
const char kETC2RGB8A1SRGB[] = "GL_OES_compressed_ETC2_punchthroughA_sRGB8_alpha_texture";
const char kETC2RGBA8[]      = "GL_OES_compressed_ETC2_RGBA8_texture";
const char kETC2RGBA8SRGB[]  = "GL_OES_compressed_ETC2_sRGB8_alpha8_texture";

const char kASTC[]         = "GL_KHR_texture_compression_astc_ldr";
const char kASTCSliced3D[] = "GL_KHR_texture_compression_astc_sliced_3d";

const char kPVRTC1[]    = "GL_IMG_texture_compression_pvrtc";
const char kPVRTCSRGB[] = "GL_EXT_pvrtc_sRGB";

const char kEmpty[] = "";

// clang-format off
using CompressedTextureDXT1Test     = _Test<kDXT1,     kEmpty, true, true, false, false>;
using CompressedTextureDXT3Test     = _Test<kDXT3,     kEmpty, true, true, false, false>;
using CompressedTextureDXT5Test     = _Test<kDXT5,     kEmpty, true, true, false, false>;
using CompressedTextureS3TCSRGBTest = _Test<kS3TCSRGB, kEmpty, true, true, false, false>;
using CompressedTextureRGTCTest     = _Test<kRGTC,     kEmpty, true, true, false, false>;
using CompressedTextureBPTCTest     = _Test<kBPTC,     kEmpty, true, true, true,  false>;

using CompressedTextureETC1Test    = _Test<kETC1, kEmpty,   false, false, false, false>;
using CompressedTextureETC1SubTest = _Test<kETC1, kETC1Sub, true,  false, false, false>;

using CompressedTextureEACR11UTest  = _Test<kEACR11U,  kEmpty, true, true, false, true>;
using CompressedTextureEACR11STest  = _Test<kEACR11S,  kEmpty, true, true, false, true>;
using CompressedTextureEACRG11UTest = _Test<kEACRG11U, kEmpty, true, true, false, true>;
using CompressedTextureEACRG11STest = _Test<kEACRG11S, kEmpty, true, true, false, true>;

using CompressedTextureETC2RGB8Test       = _Test<kETC2RGB8,       kEmpty, true, true, false, true>;
using CompressedTextureETC2RGB8SRGBTest   = _Test<kETC2RGB8SRGB,   kEmpty, true, true, false, true>;
using CompressedTextureETC2RGB8A1Test     = _Test<kETC2RGB8A1,     kEmpty, true, true, false, true>;
using CompressedTextureETC2RGB8A1SRGBTest = _Test<kETC2RGB8A1SRGB, kEmpty, true, true, false, true>;
using CompressedTextureETC2RGBA8Test      = _Test<kETC2RGBA8,      kEmpty, true, true, false, true>;
using CompressedTextureETC2RGBA8SRGBTest  = _Test<kETC2RGBA8SRGB,  kEmpty, true, true, false, true>;

using CompressedTextureASTCTest         = _Test<kASTC, kEmpty,        true, true, false, false>;
using CompressedTextureASTCSliced3DTest = _Test<kASTC, kASTCSliced3D, true, true, true,  false>;

using CompressedTexturePVRTC1Test     = _Test<kPVRTC1, kEmpty,     true, false, false, false>;
using CompressedTexturePVRTC1SRGBTest = _Test<kPVRTC1, kPVRTCSRGB, true, false, false, false>;
// clang-format on

std::string PrintToStringParamName(
    const ::testing::TestParamInfo<CompressedTextureTestParams> &info)
{
    std::string name = gl::GLinternalFormatToString(std::get<1>(info.param).first);
    name.erase(0, 3);  // Remove GL_
    if (name.find("COMPRESSED_") == 0)
    {
        name.erase(0, 11);
    }
    for (std::string str : {"_EXT", "_IMG", "_KHR", "_OES"})
    {
        if (name.find(str) != std::string::npos)
        {
            name.erase(name.length() - 4, 4);
            break;
        }
    }
    std::stringstream nameStr;
    nameStr << std::get<0>(info.param) << "__" << name;
    return nameStr.str();
}

static const FormatDesc kDXT1Formats[] = {{GL_COMPRESSED_RGB_S3TC_DXT1_EXT, 8},
                                          {GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 8}};

static const FormatDesc kDXT3Formats[] = {{GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, 16}};

static const FormatDesc kDXT5Formats[] = {{GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, 16}};

static const FormatDesc kS3TCSRGBFormats[] = {{GL_COMPRESSED_SRGB_S3TC_DXT1_EXT, 8},
                                              {GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT, 8},
                                              {GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT, 16},
                                              {GL_COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT, 16}};

static const FormatDesc kRGTCFormats[] = {{GL_COMPRESSED_RED_RGTC1_EXT, 8},
                                          {GL_COMPRESSED_SIGNED_RED_RGTC1_EXT, 8},
                                          {GL_COMPRESSED_RED_GREEN_RGTC2_EXT, 16},
                                          {GL_COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT, 16}};

static const FormatDesc kBPTCFormats[] = {{GL_COMPRESSED_RGBA_BPTC_UNORM_EXT, 16},
                                          {GL_COMPRESSED_SRGB_ALPHA_BPTC_UNORM_EXT, 16},
                                          {GL_COMPRESSED_RGB_BPTC_SIGNED_FLOAT_EXT, 16},
                                          {GL_COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_EXT, 16}};

static const FormatDesc kETC1Formats[] = {{GL_ETC1_RGB8_OES, 8}};

// clang-format off
static const FormatDesc kEACR11UFormats[]        = {{GL_COMPRESSED_R11_EAC, 8}};
static const FormatDesc kEACR11SFormats[]        = {{GL_COMPRESSED_SIGNED_R11_EAC, 8}};
static const FormatDesc kEACRG11UFormats[]       = {{GL_COMPRESSED_RG11_EAC, 16}};
static const FormatDesc kEACRG11SFormats[]       = {{GL_COMPRESSED_SIGNED_RG11_EAC, 16}};
static const FormatDesc kETC2RGB8Formats[]       = {{GL_COMPRESSED_RGB8_ETC2, 8}};
static const FormatDesc kETC2RGB8SRGBFormats[]   = {{GL_COMPRESSED_SRGB8_ETC2, 8}};
static const FormatDesc kETC2RGB8A1Formats[]     = {{GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2, 8}};
static const FormatDesc kETC2RGB8A1SRGBFormats[] = {{GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2, 8}};
static const FormatDesc kETC2RGBA8Formats[]      = {{GL_COMPRESSED_RGBA8_ETC2_EAC, 16}};
static const FormatDesc kETC2RGBA8SRGBFormats[]  = {{GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC, 16}};
// clang-format on

static const FormatDesc kASTCFormats[] = {{GL_COMPRESSED_RGBA_ASTC_4x4_KHR, 16},
                                          {GL_COMPRESSED_RGBA_ASTC_5x4_KHR, 16},
                                          {GL_COMPRESSED_RGBA_ASTC_5x5_KHR, 16},
                                          {GL_COMPRESSED_RGBA_ASTC_6x5_KHR, 16},
                                          {GL_COMPRESSED_RGBA_ASTC_6x6_KHR, 16},
                                          {GL_COMPRESSED_RGBA_ASTC_8x5_KHR, 16},
                                          {GL_COMPRESSED_RGBA_ASTC_8x6_KHR, 16},
                                          {GL_COMPRESSED_RGBA_ASTC_8x8_KHR, 16},
                                          {GL_COMPRESSED_RGBA_ASTC_10x5_KHR, 16},
                                          {GL_COMPRESSED_RGBA_ASTC_10x6_KHR, 16},
                                          {GL_COMPRESSED_RGBA_ASTC_10x8_KHR, 16},
                                          {GL_COMPRESSED_RGBA_ASTC_10x10_KHR, 16},
                                          {GL_COMPRESSED_RGBA_ASTC_12x10_KHR, 16},
                                          {GL_COMPRESSED_RGBA_ASTC_12x12_KHR, 16},
                                          {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR, 16},
                                          {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR, 16},
                                          {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR, 16},
                                          {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR, 16},
                                          {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR, 16},
                                          {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR, 16},
                                          {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR, 16},
                                          {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR, 16},
                                          {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR, 16},
                                          {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR, 16},
                                          {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR, 16},
                                          {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR, 16},
                                          {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR, 16},
                                          {GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR, 16}};

static const FormatDesc kPVRTC1Formats[] = {{GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG, 32},
                                            {GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG, 32},
                                            {GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG, 32},
                                            {GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG, 32}};

static const FormatDesc kPVRTC1SRGBFormats[] = {{GL_COMPRESSED_SRGB_PVRTC_2BPPV1_EXT, 32},
                                                {GL_COMPRESSED_SRGB_PVRTC_4BPPV1_EXT, 32},
                                                {GL_COMPRESSED_SRGB_ALPHA_PVRTC_2BPPV1_EXT, 32},
                                                {GL_COMPRESSED_SRGB_ALPHA_PVRTC_4BPPV1_EXT, 32}};

ANGLE_INSTANTIATE_TEST_COMBINE_1(CompressedTextureDXT1Test,
                                 PrintToStringParamName,
                                 testing::ValuesIn(kDXT1Formats),
                                 ANGLE_ALL_TEST_PLATFORMS_ES2,
                                 ANGLE_ALL_TEST_PLATFORMS_ES3);

ANGLE_INSTANTIATE_TEST_COMBINE_1(CompressedTextureDXT3Test,
                                 PrintToStringParamName,
                                 testing::ValuesIn(kDXT3Formats),
                                 ANGLE_ALL_TEST_PLATFORMS_ES2,
                                 ANGLE_ALL_TEST_PLATFORMS_ES3);

ANGLE_INSTANTIATE_TEST_COMBINE_1(CompressedTextureDXT5Test,
                                 PrintToStringParamName,
                                 testing::ValuesIn(kDXT5Formats),
                                 ANGLE_ALL_TEST_PLATFORMS_ES2,
                                 ANGLE_ALL_TEST_PLATFORMS_ES3);

ANGLE_INSTANTIATE_TEST_COMBINE_1(CompressedTextureS3TCSRGBTest,
                                 PrintToStringParamName,
                                 testing::ValuesIn(kS3TCSRGBFormats),
                                 ANGLE_ALL_TEST_PLATFORMS_ES2,
                                 ANGLE_ALL_TEST_PLATFORMS_ES3);

ANGLE_INSTANTIATE_TEST_COMBINE_1(CompressedTextureRGTCTest,
                                 PrintToStringParamName,
                                 testing::ValuesIn(kRGTCFormats),
                                 ANGLE_ALL_TEST_PLATFORMS_ES2,
                                 ANGLE_ALL_TEST_PLATFORMS_ES3);

ANGLE_INSTANTIATE_TEST_COMBINE_1(CompressedTextureBPTCTest,
                                 PrintToStringParamName,
                                 testing::ValuesIn(kBPTCFormats),
                                 ANGLE_ALL_TEST_PLATFORMS_ES2,
                                 ANGLE_ALL_TEST_PLATFORMS_ES3);

ANGLE_INSTANTIATE_TEST_COMBINE_1(CompressedTextureETC1Test,
                                 PrintToStringParamName,
                                 testing::ValuesIn(kETC1Formats),
                                 ANGLE_ALL_TEST_PLATFORMS_ES2,
                                 ANGLE_ALL_TEST_PLATFORMS_ES3);

ANGLE_INSTANTIATE_TEST_COMBINE_1(CompressedTextureETC1SubTest,
                                 PrintToStringParamName,
                                 testing::ValuesIn(kETC1Formats),
                                 ANGLE_ALL_TEST_PLATFORMS_ES2,
                                 ANGLE_ALL_TEST_PLATFORMS_ES3);

ANGLE_INSTANTIATE_TEST_COMBINE_1(CompressedTextureEACR11UTest,
                                 PrintToStringParamName,
                                 testing::ValuesIn(kEACR11UFormats),
                                 ANGLE_ALL_TEST_PLATFORMS_ES2,
                                 ANGLE_ALL_TEST_PLATFORMS_ES3);

ANGLE_INSTANTIATE_TEST_COMBINE_1(CompressedTextureEACR11STest,
                                 PrintToStringParamName,
                                 testing::ValuesIn(kEACR11SFormats),
                                 ANGLE_ALL_TEST_PLATFORMS_ES2,
                                 ANGLE_ALL_TEST_PLATFORMS_ES3);

ANGLE_INSTANTIATE_TEST_COMBINE_1(CompressedTextureEACRG11UTest,
                                 PrintToStringParamName,
                                 testing::ValuesIn(kEACRG11UFormats),
                                 ANGLE_ALL_TEST_PLATFORMS_ES2,
                                 ANGLE_ALL_TEST_PLATFORMS_ES3);

ANGLE_INSTANTIATE_TEST_COMBINE_1(CompressedTextureEACRG11STest,
                                 PrintToStringParamName,
                                 testing::ValuesIn(kEACRG11SFormats),
                                 ANGLE_ALL_TEST_PLATFORMS_ES2,
                                 ANGLE_ALL_TEST_PLATFORMS_ES3);

ANGLE_INSTANTIATE_TEST_COMBINE_1(CompressedTextureETC2RGB8Test,
                                 PrintToStringParamName,
                                 testing::ValuesIn(kETC2RGB8Formats),
                                 ANGLE_ALL_TEST_PLATFORMS_ES2,
                                 ANGLE_ALL_TEST_PLATFORMS_ES3);

ANGLE_INSTANTIATE_TEST_COMBINE_1(CompressedTextureETC2RGB8SRGBTest,
                                 PrintToStringParamName,
                                 testing::ValuesIn(kETC2RGB8SRGBFormats),
                                 ANGLE_ALL_TEST_PLATFORMS_ES2,
                                 ANGLE_ALL_TEST_PLATFORMS_ES3);

ANGLE_INSTANTIATE_TEST_COMBINE_1(CompressedTextureETC2RGB8A1Test,
                                 PrintToStringParamName,
                                 testing::ValuesIn(kETC2RGB8A1Formats),
                                 ANGLE_ALL_TEST_PLATFORMS_ES2,
                                 ANGLE_ALL_TEST_PLATFORMS_ES3);

ANGLE_INSTANTIATE_TEST_COMBINE_1(CompressedTextureETC2RGB8A1SRGBTest,
                                 PrintToStringParamName,
                                 testing::ValuesIn(kETC2RGB8A1SRGBFormats),
                                 ANGLE_ALL_TEST_PLATFORMS_ES2,
                                 ANGLE_ALL_TEST_PLATFORMS_ES3);

ANGLE_INSTANTIATE_TEST_COMBINE_1(CompressedTextureETC2RGBA8Test,
                                 PrintToStringParamName,
                                 testing::ValuesIn(kETC2RGBA8Formats),
                                 ANGLE_ALL_TEST_PLATFORMS_ES2,
                                 ANGLE_ALL_TEST_PLATFORMS_ES3);

ANGLE_INSTANTIATE_TEST_COMBINE_1(CompressedTextureETC2RGBA8SRGBTest,
                                 PrintToStringParamName,
                                 testing::ValuesIn(kETC2RGBA8SRGBFormats),
                                 ANGLE_ALL_TEST_PLATFORMS_ES2,
                                 ANGLE_ALL_TEST_PLATFORMS_ES3);

ANGLE_INSTANTIATE_TEST_COMBINE_1(CompressedTextureASTCTest,
                                 PrintToStringParamName,
                                 testing::ValuesIn(kASTCFormats),
                                 ANGLE_ALL_TEST_PLATFORMS_ES2,
                                 ANGLE_ALL_TEST_PLATFORMS_ES3);

ANGLE_INSTANTIATE_TEST_COMBINE_1(CompressedTextureASTCSliced3DTest,
                                 PrintToStringParamName,
                                 testing::ValuesIn(kASTCFormats),
                                 ANGLE_ALL_TEST_PLATFORMS_ES2,
                                 ANGLE_ALL_TEST_PLATFORMS_ES3);

ANGLE_INSTANTIATE_TEST_COMBINE_1(CompressedTexturePVRTC1Test,
                                 PrintToStringParamName,
                                 testing::ValuesIn(kPVRTC1Formats),
                                 ANGLE_ALL_TEST_PLATFORMS_ES2,
                                 ANGLE_ALL_TEST_PLATFORMS_ES3);

ANGLE_INSTANTIATE_TEST_COMBINE_1(CompressedTexturePVRTC1SRGBTest,
                                 PrintToStringParamName,
                                 testing::ValuesIn(kPVRTC1SRGBFormats),
                                 ANGLE_ALL_TEST_PLATFORMS_ES2,
                                 ANGLE_ALL_TEST_PLATFORMS_ES3);

// clang-format off
TEST_P(CompressedTextureDXT1Test,     Test) { test(); }
TEST_P(CompressedTextureDXT3Test,     Test) { test(); }
TEST_P(CompressedTextureDXT5Test,     Test) { test(); }
TEST_P(CompressedTextureS3TCSRGBTest, Test) { test(); }
TEST_P(CompressedTextureRGTCTest,     Test) { test(); }
TEST_P(CompressedTextureBPTCTest,     Test) { test(); }

TEST_P(CompressedTextureETC1Test,    Test) { test(); }
TEST_P(CompressedTextureETC1SubTest, Test) { test(); }

TEST_P(CompressedTextureEACR11UTest,  Test) { test(); }
TEST_P(CompressedTextureEACR11STest,  Test) { test(); }
TEST_P(CompressedTextureEACRG11UTest, Test) { test(); }
TEST_P(CompressedTextureEACRG11STest, Test) { test(); }

TEST_P(CompressedTextureETC2RGB8Test,       Test) { test(); }
TEST_P(CompressedTextureETC2RGB8SRGBTest,   Test) { test(); }
TEST_P(CompressedTextureETC2RGB8A1Test,     Test) { test(); }
TEST_P(CompressedTextureETC2RGB8A1SRGBTest, Test) { test(); }
TEST_P(CompressedTextureETC2RGBA8Test,      Test) { test(); }
TEST_P(CompressedTextureETC2RGBA8SRGBTest,  Test) { test(); }

TEST_P(CompressedTextureASTCTest,         Test) { test(); }
TEST_P(CompressedTextureASTCSliced3DTest, Test) { test(); }

TEST_P(CompressedTexturePVRTC1Test,     Test) { test(); }
TEST_P(CompressedTexturePVRTC1SRGBTest, Test) { test(); }
// clang-format on
}  // namespace
