/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 2.0 Module
 * -------------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Negative Texture API tests.
 *//*--------------------------------------------------------------------*/

#include "es2fNegativeTextureApiTests.hpp"
#include "es2fApiCase.hpp"
#include "tcuFormatUtil.hpp"
#include "tcuCompressedTexture.hpp"
#include "gluTextureUtil.hpp"
#include "gluContextInfo.hpp"

#include <vector>
#include <algorithm>

#include "glwEnums.hpp"
#include "glwDefs.hpp"

using namespace glw; // GL types

namespace deqp
{
namespace gles2
{
namespace Functional
{

using std::vector;
using tcu::TestLog;

static uint32_t cubeFaceToGLFace(tcu::CubeFace face)
{
    switch (face)
    {
    case tcu::CUBEFACE_NEGATIVE_X:
        return GL_TEXTURE_CUBE_MAP_NEGATIVE_X;
    case tcu::CUBEFACE_POSITIVE_X:
        return GL_TEXTURE_CUBE_MAP_POSITIVE_X;
    case tcu::CUBEFACE_NEGATIVE_Y:
        return GL_TEXTURE_CUBE_MAP_NEGATIVE_Y;
    case tcu::CUBEFACE_POSITIVE_Y:
        return GL_TEXTURE_CUBE_MAP_POSITIVE_Y;
    case tcu::CUBEFACE_NEGATIVE_Z:
        return GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
    case tcu::CUBEFACE_POSITIVE_Z:
        return GL_TEXTURE_CUBE_MAP_POSITIVE_Z;
    default:
        DE_ASSERT(false);
        return GL_NONE;
    }
}

#define FOR_CUBE_FACES(FACE_GL_VAR, BODY)                                            \
    do                                                                               \
    {                                                                                \
        for (int faceIterTcu = 0; faceIterTcu < tcu::CUBEFACE_LAST; faceIterTcu++)   \
        {                                                                            \
            const GLenum FACE_GL_VAR = cubeFaceToGLFace((tcu::CubeFace)faceIterTcu); \
            BODY                                                                     \
        }                                                                            \
    } while (false)

static void getCompressedTexSubImage2DFormat(const vector<int32_t> &supported, vector<int32_t> &accepted)
{
    // Find a supported compressed texture format that is accepted by compressedTexSubImage2D()

    static const GLuint compressedTexSubImage2DFormats[] = {
        0x83F0, // GL_COMPRESSED_RGB_S3TC_DXT1_EXT
        0x83F1, // GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
        0x8C00, // GL_COMPRESSED_RGB_PVRTC_4BPPV1_IMG
        0x8C01, // GL_COMPRESSED_RGB_PVRTC_2BPPV1_IMG
        0x8C02, // GL_COMPRESSED_RGBA_PVRTC_4BPPV1_IMG
        0x8C03  // GL_COMPRESSED_RGBA_PVRTC_2BPPV1_IMG
    };

    for (int i = 0; i < DE_LENGTH_OF_ARRAY(compressedTexSubImage2DFormats); i++)
    {
        vector<int32_t>::const_iterator fmt =
            std::find(supported.begin(), supported.end(), compressedTexSubImage2DFormats[i]);
        if (fmt != supported.end())
            accepted.push_back(*fmt);
    }
}

NegativeTextureApiTests::NegativeTextureApiTests(Context &context)
    : TestCaseGroup(context, "texture", "Negative Texture API Cases")
{
}

NegativeTextureApiTests::~NegativeTextureApiTests(void)
{
}

void NegativeTextureApiTests::init(void)
{
    // glActiveTexture

    ES2F_ADD_API_CASE(activetexture_invalid_texture, "Invalid glActiveTexture() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if texture is not one of GL_TEXTUREi, where i "
                                      "ranges from 0 to (GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS - 1).");
        glActiveTexture(-1);
        expectError(GL_INVALID_ENUM);
        int numMaxTextureUnits = m_context.getContextInfo().getInt(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
        glActiveTexture(GL_TEXTURE0 + numMaxTextureUnits);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });

    // glBindTexture

    ES2F_ADD_API_CASE(bindtexture_invalid_target, "Invalid glBindTexture() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is not one of the allowable values.");
        glBindTexture(0, 1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(bindtexture_type_mismatch, "Invalid glBindTexture() usage", {
        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if texture was previously created with a "
                                      "target that doesn't match that of target.");
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
        expectError(GL_INVALID_OPERATION);
        glDeleteTextures(1, &texture);
        m_log << TestLog::EndSection;
    });

    // glCompressedTexImage2D

    ES2F_ADD_API_CASE(compressedteximage_2d_invalid_target, "Invalid glCompressedTexImage2D() usage", {
        vector<int32_t> compressedFormats;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, compressedFormats);
        if (!compressedFormats.empty())
        {
            m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is invalid.");
            glCompressedTexImage2D(0, 0, compressedFormats[0], 0, 0, 0, 0, 0);
            expectError(GL_INVALID_ENUM);
            m_log << TestLog::EndSection;
        }
    });
    ES2F_ADD_API_CASE(compressedteximage_2d_invalid_format_tex2d, "Invalid glCompressedTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if internalformat is not a supported format "
                                      "returned in GL_COMPRESSED_TEXTURE_FORMATS.");
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(compressedteximage_2d_invalid_format_cube, "Invalid glCompressedTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if internalformat is not a supported format "
                                      "returned in GL_COMPRESSED_TEXTURE_FORMATS.");
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_ENUM);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_ENUM);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_ENUM);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_ENUM);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_ENUM);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(compressedteximage2d_neg_level_tex2d, "Invalid glCompressedTexImage2D() usage", {
        vector<int32_t> compressedFormats;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, compressedFormats);
        if (!compressedFormats.empty())
        {
            size_t firstNonPalettedFormatNdx = 0;
            // Negtive values are valid for palette formats
            if (m_context.getContextInfo().isExtensionSupported("GL_OES_compressed_paletted_texture"))
            {
                while (GL_PALETTE4_RGB8_OES <= compressedFormats[firstNonPalettedFormatNdx] &&
                       GL_PALETTE8_RGB5_A1_OES >= compressedFormats[firstNonPalettedFormatNdx])
                {
                    ++firstNonPalettedFormatNdx;
                }
            }
            if (firstNonPalettedFormatNdx < compressedFormats.size())
            {
                m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if level is less than 0.");
                glCompressedTexImage2D(GL_TEXTURE_2D, -1, compressedFormats[firstNonPalettedFormatNdx], 0, 0, 0, 0, 0);
                expectError(GL_INVALID_VALUE);
                m_log << TestLog::EndSection;
            }
        }
    });
    ES2F_ADD_API_CASE(compressedteximage2d_neg_level_cube, "Invalid glCompressedTexImage2D() usage", {
        vector<int32_t> compressedFormats;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, compressedFormats);
        if (!compressedFormats.empty())
        {
            size_t firstNonPalettedFormatNdx = 0;
            // Negtive values are valid for palette formats
            if (m_context.getContextInfo().isExtensionSupported("GL_OES_compressed_paletted_texture"))
            {
                while (GL_PALETTE4_RGB8_OES <= compressedFormats[firstNonPalettedFormatNdx] &&
                       GL_PALETTE8_RGB5_A1_OES >= compressedFormats[firstNonPalettedFormatNdx])
                {
                    ++firstNonPalettedFormatNdx;
                }
            }
            if (firstNonPalettedFormatNdx < compressedFormats.size())
            {
                m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if level is less than 0.");
                glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, -1, compressedFormats[firstNonPalettedFormatNdx],
                                       0, 0, 0, 0, 0);
                expectError(GL_INVALID_VALUE);
                glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, -1, compressedFormats[firstNonPalettedFormatNdx],
                                       0, 0, 0, 0, 0);
                expectError(GL_INVALID_VALUE);
                glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, -1, compressedFormats[firstNonPalettedFormatNdx],
                                       0, 0, 0, 0, 0);
                expectError(GL_INVALID_VALUE);
                glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, -1, compressedFormats[firstNonPalettedFormatNdx],
                                       0, 0, 0, 0, 0);
                expectError(GL_INVALID_VALUE);
                glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, -1, compressedFormats[firstNonPalettedFormatNdx],
                                       0, 0, 0, 0, 0);
                expectError(GL_INVALID_VALUE);
                glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, -1, compressedFormats[firstNonPalettedFormatNdx],
                                       0, 0, 0, 0, 0);
                expectError(GL_INVALID_VALUE);
                m_log << TestLog::EndSection;
            }
        }
    });
    ES2F_ADD_API_CASE(compressedteximage2d_level_max_tex2d, "Invalid glCompressedTexImage2D() usage", {
        vector<int32_t> compressedFormats;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, compressedFormats);
        if (!compressedFormats.empty())
        {
            m_log << TestLog::Section(
                "", "GL_INVALID_VALUE is generated if level is greater than log_2(GL_MAX_TEXTURE_SIZE).");
            uint32_t log2MaxTextureSize = deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_TEXTURE_SIZE)) + 1;
            glCompressedTexImage2D(GL_TEXTURE_2D, log2MaxTextureSize, compressedFormats[0], 0, 0, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            m_log << TestLog::EndSection;
        }
    });
    ES2F_ADD_API_CASE(compressedteximage2d_level_max_cube_pos, "Invalid glCompressedTexImage2D() usage", {
        vector<int32_t> compressedFormats;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, compressedFormats);
        if (!compressedFormats.empty())
        {
            m_log << TestLog::Section(
                "", "GL_INVALID_VALUE is generated if level is greater than log_2(GL_MAX_TEXTURE_SIZE).");
            uint32_t log2MaxTextureSize =
                deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE)) + 1;
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, log2MaxTextureSize, compressedFormats[0], 0, 0, 0, 0,
                                   0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, log2MaxTextureSize, compressedFormats[0], 0, 0, 0, 0,
                                   0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, log2MaxTextureSize, compressedFormats[0], 0, 0, 0, 0,
                                   0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, log2MaxTextureSize, compressedFormats[0], 0, 0, 0, 0,
                                   0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, log2MaxTextureSize, compressedFormats[0], 0, 0, 0, 0,
                                   0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, log2MaxTextureSize, compressedFormats[0], 0, 0, 0, 0,
                                   0);
            expectError(GL_INVALID_VALUE);
            m_log << TestLog::EndSection;
        }
    });
    ES2F_ADD_API_CASE(compressedteximage2d_neg_width_height_tex2d, "Invalid glCompressedTexImage2D() usage", {
        vector<int32_t> compressedFormats;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, compressedFormats);
        if (!compressedFormats.empty())
        {
            m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width or height is less than 0.");
            glCompressedTexImage2D(GL_TEXTURE_2D, 0, compressedFormats[0], -1, 0, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_2D, 0, compressedFormats[0], 0, -1, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_2D, 0, compressedFormats[0], -1, -1, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            m_log << TestLog::EndSection;
        }
    });
    ES2F_ADD_API_CASE(compressedteximage2d_neg_width_height_cube_pos_x, "Invalid glCompressedTexImage2D() usage", {
        vector<int32_t> compressedFormats;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, compressedFormats);
        if (!compressedFormats.empty())
        {
            m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width or height is less than 0.");
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, compressedFormats[0], -1, 0, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, compressedFormats[0], 0, -1, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, compressedFormats[0], -1, -1, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            m_log << TestLog::EndSection;
        }
    });
    ES2F_ADD_API_CASE(compressedteximage2d_neg_width_height_cube_pos_y, "Invalid glCompressedTexImage2D() usage", {
        vector<int32_t> compressedFormats;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, compressedFormats);
        if (!compressedFormats.empty())
        {
            m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width or height is less than 0.");
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, compressedFormats[0], -1, 0, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, compressedFormats[0], 0, -1, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, compressedFormats[0], -1, -1, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            m_log << TestLog::EndSection;
        }
    });
    ES2F_ADD_API_CASE(compressedteximage2d_neg_width_height_cube_pos_z, "Invalid glCompressedTexImage2D() usage", {
        vector<int32_t> compressedFormats;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, compressedFormats);
        if (!compressedFormats.empty())
        {
            m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width or height is less than 0.");
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, compressedFormats[0], -1, 0, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, compressedFormats[0], 0, -1, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, compressedFormats[0], -1, -1, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            m_log << TestLog::EndSection;
        }
    });
    ES2F_ADD_API_CASE(compressedteximage2d_neg_width_height_cube_neg_x, "Invalid glCompressedTexImage2D() usage", {
        vector<int32_t> compressedFormats;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, compressedFormats);
        if (!compressedFormats.empty())
        {
            m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width or height is less than 0.");
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, compressedFormats[0], -1, 0, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, compressedFormats[0], 0, -1, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, compressedFormats[0], -1, -1, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            m_log << TestLog::EndSection;
        }
    });
    ES2F_ADD_API_CASE(compressedteximage2d_neg_width_height_cube_neg_y, "Invalid glCompressedTexImage2D() usage", {
        vector<int32_t> compressedFormats;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, compressedFormats);
        if (!compressedFormats.empty())
        {
            m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width or height is less than 0.");
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, compressedFormats[0], -1, 0, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, compressedFormats[0], 0, -1, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, compressedFormats[0], -1, -1, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            m_log << TestLog::EndSection;
        }
    });
    ES2F_ADD_API_CASE(compressedteximage2d_neg_width_height_cube_neg_z, "Invalid glCompressedTexImage2D() usage", {
        vector<int32_t> compressedFormats;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, compressedFormats);
        if (!compressedFormats.empty())
        {
            m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width or height is less than 0.");
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, compressedFormats[0], -1, 0, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, compressedFormats[0], 0, -1, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, compressedFormats[0], -1, -1, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            m_log << TestLog::EndSection;
        }
    });
    ES2F_ADD_API_CASE(compressedteximage2d_width_height_max_tex2d, "Invalid glCompressedTexImage2D() usage", {
        vector<int32_t> compressedFormats;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, compressedFormats);
        if (!compressedFormats.empty())
        {
            m_log << TestLog::Section(
                "", "GL_INVALID_VALUE is generated if width or height is greater than GL_MAX_TEXTURE_SIZE.");
            int maxTextureSize = m_context.getContextInfo().getInt(GL_MAX_TEXTURE_SIZE) + 1;
            glCompressedTexImage2D(GL_TEXTURE_2D, 0, compressedFormats[0], maxTextureSize, 0, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_2D, 0, compressedFormats[0], 0, maxTextureSize, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_2D, 0, compressedFormats[0], maxTextureSize, maxTextureSize, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            m_log << TestLog::EndSection;
        }
    });
    ES2F_ADD_API_CASE(compressedteximage2d_width_height_max_cube_pos_x, "Invalid glCompressedTexImage2D() usage", {
        vector<int32_t> compressedFormats;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, compressedFormats);
        if (!compressedFormats.empty())
        {
            m_log << TestLog::Section(
                "", "GL_INVALID_VALUE is generated if width or height is greater than GL_MAX_CUBE_MAP_TEXTURE_SIZE.");
            int maxTextureSize = m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE) + 1;
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, compressedFormats[0], maxTextureSize, 0, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, compressedFormats[0], 0, maxTextureSize, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, compressedFormats[0], maxTextureSize,
                                   maxTextureSize, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            m_log << TestLog::EndSection;
        }
    });
    ES2F_ADD_API_CASE(compressedteximage2d_width_height_max_cube_pos_y, "Invalid glCompressedTexImage2D() usage", {
        vector<int32_t> compressedFormats;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, compressedFormats);
        if (!compressedFormats.empty())
        {
            m_log << TestLog::Section(
                "", "GL_INVALID_VALUE is generated if width or height is greater than GL_MAX_CUBE_MAP_TEXTURE_SIZE.");
            int maxTextureSize = m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE) + 1;
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, compressedFormats[0], maxTextureSize, 0, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, compressedFormats[0], 0, maxTextureSize, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, compressedFormats[0], maxTextureSize,
                                   maxTextureSize, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            m_log << TestLog::EndSection;
        }
    });
    ES2F_ADD_API_CASE(compressedteximage2d_width_height_max_cube_pos_z, "Invalid glCompressedTexImage2D() usage", {
        vector<int32_t> compressedFormats;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, compressedFormats);
        if (!compressedFormats.empty())
        {
            m_log << TestLog::Section(
                "", "GL_INVALID_VALUE is generated if width or height is greater than GL_MAX_CUBE_MAP_TEXTURE_SIZE.");
            int maxTextureSize = m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE) + 1;
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, compressedFormats[0], maxTextureSize, 0, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, compressedFormats[0], 0, maxTextureSize, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, compressedFormats[0], maxTextureSize,
                                   maxTextureSize, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            m_log << TestLog::EndSection;
        }
    });
    ES2F_ADD_API_CASE(compressedteximage2d_width_height_max_cube_neg_x, "Invalid glCompressedTexImage2D() usage", {
        vector<int32_t> compressedFormats;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, compressedFormats);
        if (!compressedFormats.empty())
        {
            m_log << TestLog::Section(
                "", "GL_INVALID_VALUE is generated if width or height is greater than GL_MAX_CUBE_MAP_TEXTURE_SIZE.");
            int maxTextureSize = m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE) + 1;
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, compressedFormats[0], maxTextureSize, 0, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, compressedFormats[0], 0, maxTextureSize, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, compressedFormats[0], maxTextureSize,
                                   maxTextureSize, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            m_log << TestLog::EndSection;
        }
    });
    ES2F_ADD_API_CASE(compressedteximage2d_width_height_max_cube_neg_y, "Invalid glCompressedTexImage2D() usage", {
        vector<int32_t> compressedFormats;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, compressedFormats);
        if (!compressedFormats.empty())
        {
            m_log << TestLog::Section(
                "", "GL_INVALID_VALUE is generated if width or height is greater than GL_MAX_CUBE_MAP_TEXTURE_SIZE.");
            int maxTextureSize = m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE) + 1;
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, compressedFormats[0], maxTextureSize, 0, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, compressedFormats[0], 0, maxTextureSize, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, compressedFormats[0], maxTextureSize,
                                   maxTextureSize, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            m_log << TestLog::EndSection;
        }
    });
    ES2F_ADD_API_CASE(compressedteximage2d_width_height_max_cube_neg_z, "Invalid glCompressedTexImage2D() usage", {
        vector<int32_t> compressedFormats;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, compressedFormats);
        if (!compressedFormats.empty())
        {
            m_log << TestLog::Section(
                "", "GL_INVALID_VALUE is generated if width or height is greater than GL_MAX_CUBE_MAP_TEXTURE_SIZE.");
            int maxTextureSize = m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE) + 1;
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, compressedFormats[0], maxTextureSize, 0, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, compressedFormats[0], 0, maxTextureSize, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, compressedFormats[0], maxTextureSize,
                                   maxTextureSize, 0, 0, 0);
            expectError(GL_INVALID_VALUE);
            m_log << TestLog::EndSection;
        }
    });
    ES2F_ADD_API_CASE(compressedteximage2d_invalid_border, "Invalid glCompressedTexImage2D() usage", {
        vector<int32_t> compressedFormats;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, compressedFormats);
        if (!compressedFormats.empty())
        {
            m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if border is not 0.");
            glCompressedTexImage2D(GL_TEXTURE_2D, 0, compressedFormats[0], 0, 0, 1, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_2D, 0, compressedFormats[0], 0, 0, -1, 0, 0);
            expectError(GL_INVALID_VALUE);
            m_log << TestLog::EndSection;
        }
    });

    ES2F_ADD_API_CASE(compressedteximage2d_invalid_border_cube_pos_x, "Invalid glCompressedTexImage2D() usage", {
        vector<int32_t> compressedFormats;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, compressedFormats);
        if (!compressedFormats.empty())
        {
            m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if border is not 0.");
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, compressedFormats[0], 0, 0, 1, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, compressedFormats[0], 0, 0, -1, 0, 0);
            expectError(GL_INVALID_VALUE);
            m_log << TestLog::EndSection;
        }
    });
    ES2F_ADD_API_CASE(compressedteximage2d_invalid_border_cube_pos_y, "Invalid glCompressedTexImage2D() usage", {
        vector<int32_t> compressedFormats;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, compressedFormats);
        if (!compressedFormats.empty())
        {
            m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if border is not 0.");
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, compressedFormats[0], 0, 0, 1, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, compressedFormats[0], 0, 0, -1, 0, 0);
            expectError(GL_INVALID_VALUE);
            m_log << TestLog::EndSection;
        }
    });
    ES2F_ADD_API_CASE(compressedteximage2d_invalid_border_cube_pos_z, "Invalid glCompressedTexImage2D() usage", {
        vector<int32_t> compressedFormats;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, compressedFormats);
        if (!compressedFormats.empty())
        {
            m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if border is not 0.");
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, compressedFormats[0], 0, 0, 1, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, compressedFormats[0], 0, 0, -1, 0, 0);
            expectError(GL_INVALID_VALUE);
            m_log << TestLog::EndSection;
        }
    });
    ES2F_ADD_API_CASE(compressedteximage2d_invalid_border_cube_neg_x, "Invalid glCompressedTexImage2D() usage", {
        vector<int32_t> compressedFormats;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, compressedFormats);
        if (!compressedFormats.empty())
        {
            m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if border is not 0.");
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, compressedFormats[0], 0, 0, 1, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, compressedFormats[0], 0, 0, -1, 0, 0);
            expectError(GL_INVALID_VALUE);
            m_log << TestLog::EndSection;
        }
    });
    ES2F_ADD_API_CASE(compressedteximage2d_invalid_border_cube_neg_y, "Invalid glCompressedTexImage2D() usage", {
        vector<int32_t> compressedFormats;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, compressedFormats);
        if (!compressedFormats.empty())
        {
            m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if border is not 0.");
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, compressedFormats[0], 0, 0, 1, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, compressedFormats[0], 0, 0, -1, 0, 0);
            expectError(GL_INVALID_VALUE);
            m_log << TestLog::EndSection;
        }
    });
    ES2F_ADD_API_CASE(compressedteximage2d_invalid_border_cube_neg_z, "Invalid glCompressedTexImage2D() usage", {
        vector<int32_t> compressedFormats;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, compressedFormats);
        if (!compressedFormats.empty())
        {
            m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if border is not 0.");
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, compressedFormats[0], 0, 0, 1, 0, 0);
            expectError(GL_INVALID_VALUE);
            glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, compressedFormats[0], 0, 0, -1, 0, 0);
            expectError(GL_INVALID_VALUE);
            m_log << TestLog::EndSection;
        }
    });
    ES2F_ADD_API_CASE(compressedteximage2d_invalid_size, "Invalid glCompressedTexImage2D() usage", {
        vector<int32_t> compressedFormats;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, compressedFormats);
        if (!compressedFormats.empty())
        {
            m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if imageSize is not consistent with the "
                                          "format, dimensions, and contents of the specified compressed image data.");
            glCompressedTexImage2D(GL_TEXTURE_2D, 0, compressedFormats[0], 0, 0, 0, -1, 0);
            expectError(GL_INVALID_VALUE);
            m_log << TestLog::EndSection;
        }
    });

    // glCopyTexImage2D

    ES2F_ADD_API_CASE(copyteximage2d_invalid_target, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is invalid.");
        glCopyTexImage2D(0, 0, GL_RGB, 0, 0, 64, 64, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(copyteximage2d_invalid_format_tex2d, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM or GL_INVALID_VALUE is generated if internalformat is not an accepted format.");
        glCopyTexImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 64, 64, 0);
        expectError(GL_INVALID_ENUM, GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(copyteximage2d_invalid_format_cube, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM or GL_INVALID_VALUE is generated if internalformat is not an accepted format.");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, 0, 0, 0, 16, 16, 0);
        expectError(GL_INVALID_ENUM, GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, 0, 0, 0, 16, 16, 0);
        expectError(GL_INVALID_ENUM, GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, 0, 0, 0, 16, 16, 0);
        expectError(GL_INVALID_ENUM, GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, 0, 0, 0, 16, 16, 0);
        expectError(GL_INVALID_ENUM, GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, 0, 0, 0, 16, 16, 0);
        expectError(GL_INVALID_ENUM, GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, 0, 0, 0, 16, 16, 0);
        expectError(GL_INVALID_ENUM, GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(copyteximage2d_inequal_width_height_cube, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if target is one of the six cube map 2D image "
                                      "targets and the width and height parameters are not equal.");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 0, 0, 16, 17, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, 0, 0, 16, 17, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, 0, 0, 16, 17, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, 0, 0, 16, 17, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, 0, 0, 16, 17, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, 0, 0, 16, 17, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(copyteximage2d_neg_level_tex2d, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section("", "");
        glCopyTexImage2D(GL_TEXTURE_2D, -1, GL_RGB, 0, 0, 64, 64, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(copyteximage2d_neg_level_cube, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if level is less than 0.");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, -1, GL_RGB, 0, 0, 16, 16, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, -1, GL_RGB, 0, 0, 16, 16, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, -1, GL_RGB, 0, 0, 16, 16, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, -1, GL_RGB, 0, 0, 16, 16, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, -1, GL_RGB, 0, 0, 16, 16, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, -1, GL_RGB, 0, 0, 16, 16, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(copyteximage2d_level_max_tex2d, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section("",
                                  "GL_INVALID_VALUE is generated if level is greater than log_2(GL_MAX_TEXTURE_SIZE).");
        uint32_t log2MaxTextureSize = deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_TEXTURE_SIZE)) + 1;
        glCopyTexImage2D(GL_TEXTURE_2D, log2MaxTextureSize, GL_RGB, 0, 0, 64, 64, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(copyteximage2d_level_max_cube, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if level is greater than log_2(GL_MAX_CUBE_MAP_TEXTURE_SIZE).");
        uint32_t log2MaxTextureSize =
            deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE)) + 1;
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, log2MaxTextureSize, GL_RGB, 0, 0, 16, 16, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, log2MaxTextureSize, GL_RGB, 0, 0, 16, 16, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, log2MaxTextureSize, GL_RGB, 0, 0, 16, 16, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, log2MaxTextureSize, GL_RGB, 0, 0, 16, 16, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, log2MaxTextureSize, GL_RGB, 0, 0, 16, 16, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, log2MaxTextureSize, GL_RGB, 0, 0, 16, 16, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(copyteximage2d_invalid_width_height_tex2d, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width or height is less than 0.");
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, -1, 1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 1, -1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, -1, -1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(copyteximage2d_invalid_width_height_cube_pos_x, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width or height is less than 0.");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 0, 0, -1, 1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 0, 0, 1, -1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 0, 0, -1, -1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(copyteximage2d_invalid_width_height_cube_pos_y, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width or height is less than 0.");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, 0, 0, -1, 1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, 0, 0, 1, -1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, 0, 0, -1, -1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(copyteximage2d_invalid_width_height_cube_pos_z, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width or height is less than 0.");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, 0, 0, -1, 1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, 0, 0, 1, -1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, 0, 0, -1, -1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(copyteximage2d_invalid_width_height_cube_neg_x, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width or height is less than 0.");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, 0, 0, -1, 1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, 0, 0, 1, -1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, 0, 0, -1, -1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(copyteximage2d_invalid_width_height_cube_neg_y, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width or height is less than 0.");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, 0, 0, -1, 1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, 0, 0, 1, -1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, 0, 0, -1, -1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(copyteximage2d_invalid_width_height_cube_neg_z, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width or height is less than 0.");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, 0, 0, -1, 1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, 0, 0, 1, -1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, 0, 0, -1, -1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(copyteximage2d_width_height_max_tex2d, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if width or height is greater than GL_MAX_TEXTURE_SIZE.");
        int maxTextureSize = m_context.getContextInfo().getInt(GL_MAX_TEXTURE_SIZE) + 1;
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, maxTextureSize, 1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 1, maxTextureSize, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, maxTextureSize, maxTextureSize, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(copyteximage2d_width_height_max_cube_pos_x, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if width or height is greater than GL_MAX_CUBE_MAP_TEXTURE_SIZE.");
        int maxTextureSize = m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE) + 1;
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 0, 0, 1, maxTextureSize, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 0, 0, maxTextureSize, 1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 0, 0, maxTextureSize, maxTextureSize, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(copyteximage2d_width_height_max_cube_pos_y, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if width or height is greater than GL_MAX_CUBE_MAP_TEXTURE_SIZE.");
        int maxTextureSize = m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE) + 1;
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, 0, 0, 1, maxTextureSize, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, 0, 0, maxTextureSize, 1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, 0, 0, maxTextureSize, maxTextureSize, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(copyteximage2d_width_height_max_cube_pos_z, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if width or height is greater than GL_MAX_CUBE_MAP_TEXTURE_SIZE.");
        int maxTextureSize = m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE) + 1;
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, 0, 0, 1, maxTextureSize, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, 0, 0, maxTextureSize, 1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, 0, 0, maxTextureSize, maxTextureSize, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(copyteximage2d_width_height_max_cube_neg_x, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if width or height is greater than GL_MAX_CUBE_MAP_TEXTURE_SIZE.");
        int maxTextureSize = m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE) + 1;
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, 0, 0, 1, maxTextureSize, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, 0, 0, maxTextureSize, 1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, 0, 0, maxTextureSize, maxTextureSize, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(copyteximage2d_width_height_max_cube_neg_y, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if width or height is greater than GL_MAX_CUBE_MAP_TEXTURE_SIZE.");
        int maxTextureSize = m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE) + 1;
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, 0, 0, 1, maxTextureSize, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, 0, 0, maxTextureSize, 1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, 0, 0, maxTextureSize, maxTextureSize, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(copyteximage2d_width_height_max_cube_neg_z, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if width or height is greater than GL_MAX_CUBE_MAP_TEXTURE_SIZE.");
        int maxTextureSize = m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE) + 1;
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, 0, 0, 1, maxTextureSize, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, 0, 0, maxTextureSize, 1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, 0, 0, maxTextureSize, maxTextureSize, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(copyteximage2d_invalid_border_tex2d, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if border is not 0.");
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 64, 64, -1);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 64, 64, 1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(copyteximage2d_invalid_border_cube_pos_x, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if border is not 0.");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 0, 0, 16, 16, -1);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 0, 0, 16, 16, 1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(copyteximage2d_invalid_border_cube_pos_y, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if border is not 0.");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, 0, 0, 16, 16, -1);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, 0, 0, 16, 16, 1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(copyteximage2d_invalid_border_cube_pos_z, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if border is not 0.");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, 0, 0, 16, 16, -1);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, 0, 0, 16, 16, 1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(copyteximage2d_invalid_border_cube_neg_x, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if border is not 0.");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, 0, 0, 16, 16, -1);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, 0, 0, 16, 16, 1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(copyteximage2d_invalid_border_cube_neg_y, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if border is not 0.");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, 0, 0, 16, 16, -1);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, 0, 0, 16, 16, 1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(copyteximage2d_invalid_border_cube_neg_z, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if border is not 0.");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, 0, 0, 16, 16, -1);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, 0, 0, 16, 16, 1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });

    ES2F_ADD_API_CASE(copyteximage2d_incomplete_framebuffer, "Invalid glCopyTexImage2D() usage", {
        m_log << tcu::TestLog::Section("", "GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound "
                                           "framebuffer is not framebuffer complete.");
        GLuint fbo;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);

        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 64, 64, 0);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 0, 0, 16, 16, 0);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, 0, 0, 16, 16, 0);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, 0, 0, 16, 16, 0);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, 0, 0, 16, 16, 0);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, 0, 0, 16, 16, 0);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, 0, 0, 16, 16, 0);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        m_log << tcu::TestLog::EndSection;
    });

    // glCopyTexSubImage2D

    ES2F_ADD_API_CASE(copytexsubimage2d_invalid_target, "Invalid glCopyTexSubImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is invalid.");
        glCopyTexSubImage2D(0, 0, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(copytexsubimage2d_neg_level_tex2d, "Invalid glCopyTexSubImage2D() usage", {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if level is less than 0.");
        glCopyTexSubImage2D(GL_TEXTURE_2D, -1, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES2F_ADD_API_CASE(copytexsubimage2d_neg_level_cube, "Invalid glCopyTexSubImage2D() usage", {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
        FOR_CUBE_FACES(faceGL, glTexImage2D(faceGL, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr););

        m_log << TestLog::Section("",
                                  "GL_INVALID_VALUE is generated if level is greater than log_2(GL_MAX_TEXTURE_SIZE).");
        glCopyTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, -1, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, -1, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, -1, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, -1, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, -1, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, -1, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES2F_ADD_API_CASE(copytexsubimage2d_level_max_tex2d, "Invalid glCopyTexSubImage2D() usage", {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        m_log << TestLog::Section("",
                                  "GL_INVALID_VALUE is generated if level is greater than log_2(GL_MAX_TEXTURE_SIZE).");
        uint32_t log2MaxTextureSize = deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_TEXTURE_SIZE)) + 1;
        glCopyTexSubImage2D(GL_TEXTURE_2D, log2MaxTextureSize, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES2F_ADD_API_CASE(copytexsubimage2d_level_max_cube_pos, "Invalid glCopyTexSubImage2D() usage", {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
        FOR_CUBE_FACES(faceGL, glTexImage2D(faceGL, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr););

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if level is greater than log_2(GL_MAX_CUBE_MAP_SIZE).");
        uint32_t log2MaxTextureSize =
            deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE)) + 1;
        glCopyTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, log2MaxTextureSize, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, log2MaxTextureSize, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, log2MaxTextureSize, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, log2MaxTextureSize, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, log2MaxTextureSize, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, log2MaxTextureSize, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES2F_ADD_API_CASE(copytexsubimage2d_neg_offset, "Invalid glCopyTexSubImage2D() usage", {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if xoffset < 0 or yoffset < 0.");
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, -1, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, -1, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, -1, -1, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES2F_ADD_API_CASE(copytexsubimage2d_offset_allowed, "Invalid glCopyTexSubImage2D() usage", {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        m_log << TestLog::Section(
            "",
            "GL_INVALID_VALUE is generated if xoffset + width > texture_width or yoffset + height > texture_height.");
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 8, 4, 0, 0, 10, 10);
        expectError(GL_INVALID_VALUE);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 4, 8, 0, 0, 10, 10);
        expectError(GL_INVALID_VALUE);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 8, 8, 0, 0, 10, 10);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES2F_ADD_API_CASE(copytexsubimage2d_neg_wdt_hgt, "Invalid glCopyTexSubImage2D() usage", {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width or height is less than 0.");
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, -1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 0, -1);
        expectError(GL_INVALID_VALUE);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, -1, -1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES2F_ADD_API_CASE(copytexsubimage2d_incomplete_framebuffer, "Invalid glCopyTexSubImage2D() usage", {
        m_log << tcu::TestLog::Section("", "GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound "
                                           "framebuffer is not framebuffer complete.");

        GLuint texture[2];
        GLuint fbo;

        glGenTextures(2, texture);
        glBindTexture(GL_TEXTURE_2D, texture[0]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, texture[1]);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_NO_ERROR);

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);
        expectError(GL_NO_ERROR);

        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glCopyTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glCopyTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glCopyTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glCopyTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glCopyTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glCopyTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(2, texture);

        m_log << tcu::TestLog::EndSection;
    });

    // glDeleteTextures

    ES2F_ADD_API_CASE(deletetextures_invalid_number, "Invalid glDeleteTextures() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if n is negative.");
        glDeleteTextures(-1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(deletetextures_invalid_number_bind, "Invalid glDeleteTextures() usage", {
        GLuint texture;
        glGenTextures(1, &texture);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if n is negative.");
        glBindTexture(GL_TEXTURE_2D, texture);
        glDeleteTextures(-1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });

    // glGenerateMipmap

    ES2F_ADD_API_CASE(generatemipmap_invalid_target, "Invalid glGenerateMipmap() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM is generated if target is not GL_TEXTURE_2D or GL_TEXTURE_CUBE_MAP.");
        glGenerateMipmap(0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(generatemipmap_npot_wdt_hgt, "Invalid glGenerateMipmap() usage", {
        GLuint texture;

        if (m_context.getContextInfo().isExtensionSupported("GL_OES_texture_npot"))
        {
            m_log << tcu::TestLog::Message << "GL_OES_texture_npot extension removes error condition, skipping test"
                  << tcu::TestLog::EndMessage;
            return;
        }

        glActiveTexture(GL_TEXTURE0);
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if either the width or height of the zero "
                                      "level array is not a power of two.");
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 257, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        glGenerateMipmap(GL_TEXTURE_2D);
        expectError(GL_INVALID_OPERATION);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 257, 256, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        glGenerateMipmap(GL_TEXTURE_2D);
        expectError(GL_INVALID_OPERATION);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 257, 257, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        glGenerateMipmap(GL_TEXTURE_2D);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES2F_ADD_API_CASE(generatemipmap_zero_level_array_compressed, "Invalid glGenerateMipmap() usage", {
        vector<int32_t> compressedFormats;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, compressedFormats);
        if (!compressedFormats.empty())
        {
            GLuint texture;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            m_log << TestLog::Section(
                "",
                "GL_INVALID_OPERATION is generated if the zero level array is stored in a compressed internal format.");
            glCompressedTexImage2D(GL_TEXTURE_2D, 0, compressedFormats[0], 0, 0, 0, 0, 0);
            glGenerateMipmap(GL_TEXTURE_2D);
            expectError(GL_INVALID_OPERATION);
            m_log << TestLog::EndSection;
            glDeleteTextures(1, &texture);
        }
    });
    ES2F_ADD_API_CASE(generatemipmap_incomplete_cube, "Invalid glGenerateMipmap() usage", {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_CUBE_MAP, texture);

        m_log << TestLog::Section(
            "", "INVALID_OPERATION is generated if the texture bound to target is not cube complete.");
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 16, 16, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, 16, 16, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, 16, 16, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, 16, 16, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, 16, 16, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, 32, 32, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });

    // glGenTextures

    ES2F_ADD_API_CASE(gentextures_invalid_size, "Invalid glGenTextures() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if n is negative.");
        glGenTextures(-1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });

    // glPixelStorei

    ES2F_ADD_API_CASE(pixelstorei_invalid_pname, "Invalid glPixelStorei() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if pname is not an accepted value.");
        glPixelStorei(0, 1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(pixelstorei_invalid_param, "Invalid glPixelStorei() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if alignment is specified as other than 1, 2, 4, or 8.");
        glPixelStorei(GL_PACK_ALIGNMENT, 0);
        expectError(GL_INVALID_VALUE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 0);
        expectError(GL_INVALID_VALUE);
        glPixelStorei(GL_PACK_ALIGNMENT, 16);
        expectError(GL_INVALID_VALUE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 16);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });

    // glTexImage2D

    ES2F_ADD_API_CASE(teximage2d_invalid_target, "Invalid glTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is invalid.");
        glTexImage2D(0, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(teximage2d_invalid_format, "Invalid glTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if format or type is not an accepted value.");
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, 0, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(teximage2d_invalid_type, "Invalid glTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if format or type is not an accepted value.");
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, 0, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(teximage2d_inequal_width_height_cube, "Invalid glTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if target is one of the six cube map 2D image "
                                      "targets and the width and height parameters are not equal.");
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, 1, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(teximage2d_neg_level_tex2d, "Invalid glTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if level is less than 0.");
        glTexImage2D(GL_TEXTURE_2D, -1, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(teximage2d_neg_level_cube, "Invalid glTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if level is less than 0.");
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, -1, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, -1, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, -1, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, -1, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, -1, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, -1, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(teximage2d_level_max_tex2d, "Invalid glTexImage2D() usage", {
        m_log << TestLog::Section("",
                                  "GL_INVALID_VALUE is generated if level is greater than log_2(GL_MAX_TEXTURE_SIZE).");
        uint32_t log2MaxTextureSize = deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_TEXTURE_SIZE)) + 1;
        glTexImage2D(GL_TEXTURE_2D, log2MaxTextureSize, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(teximage2d_level_max_cube, "Invalid glTexImage2D() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if level is greater than log_2(GL_MAX_CUBE_MAP_TEXTURE_SIZE).");
        uint32_t log2MaxTextureSize =
            deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE)) + 1;
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, log2MaxTextureSize, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, log2MaxTextureSize, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, log2MaxTextureSize, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, log2MaxTextureSize, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, log2MaxTextureSize, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, log2MaxTextureSize, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(teximage2d_invalid_internalformat, "Invalid glTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if internalformat is not an accepted format.");
        glTexImage2D(GL_TEXTURE_2D, 0, 0, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(teximage2d_neg_width_height_tex2d, "Invalid glTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width or height is less than 0.");
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, -1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, -1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, -1, -1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(teximage2d_neg_width_height_cube_pos_x, "Invalid glTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width or height is less than 0.");
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, -1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 1, -1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, -1, -1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(teximage2d_neg_width_height_cube_pos_y, "Invalid glTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width or height is less than 0.");
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, -1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, 1, -1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, -1, -1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(teximage2d_neg_width_height_cube_pos_z, "Invalid glTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width or height is less than 0.");
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, -1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, 1, -1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, -1, -1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(teximage2d_neg_width_height_cube_neg_x, "Invalid glTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width or height is less than 0.");
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, -1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, 1, -1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, -1, -1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(teximage2d_neg_width_height_cube_neg_y, "Invalid glTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width or height is less than 0.");
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, -1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, 1, -1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, -1, -1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(teximage2d_neg_width_height_cube_neg_z, "Invalid glTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width or height is less than 0.");
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, -1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, 1, -1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, -1, -1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(teximage2d_width_height_max_tex2d, "Invalid glTexImage2D() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if width or height is greater than GL_MAX_TEXTURE_SIZE.");
        int maxTextureSize = m_context.getContextInfo().getInt(GL_MAX_TEXTURE_SIZE) + 1;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, maxTextureSize, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, maxTextureSize, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, maxTextureSize, maxTextureSize, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(teximage2d_width_height_max_cube_pos_x, "Invalid glTexImage2D() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if width or height is greater than GL_MAX_CUBE_MAP_TEXTURE_SIZE.");
        int maxTextureSize = m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE) + 1;
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, maxTextureSize, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 1, maxTextureSize, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, maxTextureSize, maxTextureSize, 0, GL_RGB,
                     GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(teximage2d_width_height_max_cube_pos_y, "Invalid glTexImage2D() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if width or height is greater than GL_MAX_CUBE_MAP_TEXTURE_SIZE.");
        int maxTextureSize = m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE) + 1;
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, maxTextureSize, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, 1, maxTextureSize, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, maxTextureSize, maxTextureSize, 0, GL_RGB,
                     GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(teximage2d_width_height_max_cube_pos_z, "Invalid glTexImage2D() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if width or height is greater than GL_MAX_CUBE_MAP_TEXTURE_SIZE.");
        int maxTextureSize = m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE) + 1;
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, maxTextureSize, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, 1, maxTextureSize, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, maxTextureSize, maxTextureSize, 0, GL_RGB,
                     GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(teximage2d_width_height_max_cube_neg_x, "Invalid glTexImage2D() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if width or height is greater than GL_MAX_CUBE_MAP_TEXTURE_SIZE.");
        int maxTextureSize = m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE) + 1;
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, maxTextureSize, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, 1, maxTextureSize, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, maxTextureSize, maxTextureSize, 0, GL_RGB,
                     GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(teximage2d_width_height_max_cube_neg_y, "Invalid glTexImage2D() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if width or height is greater than GL_MAX_CUBE_MAP_TEXTURE_SIZE.");
        int maxTextureSize = m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE) + 1;
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, maxTextureSize, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, 1, maxTextureSize, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, maxTextureSize, maxTextureSize, 0, GL_RGB,
                     GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(teximage2d_width_height_max_cube_neg_z, "Invalid glTexImage2D() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if width or height is greater than GL_MAX_CUBE_MAP_TEXTURE_SIZE.");
        int maxTextureSize = m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE) + 1;
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, maxTextureSize, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, 1, maxTextureSize, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, maxTextureSize, maxTextureSize, 0, GL_RGB,
                     GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(teximage2d_invalid_border, "Invalid glTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if border is not 0.");
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, -1, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(teximage2d_format_mismatch, "Invalid glTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if format does not match internalformat.");
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(teximage2d_type_format_mismatch, "Invalid glTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if type is GL_UNSIGNED_SHORT_4_4_4_4 or "
                                      "GL_UNSIGNED_SHORT_5_5_5_1 and format is not GL_RGBA.");
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_SHORT_4_4_4_4, 0);
        expectError(GL_INVALID_OPERATION);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_SHORT_5_5_5_1, 0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;
    });

    // glTexSubImage2D

    ES2F_ADD_API_CASE(texsubimage2d_invalid_target, "Invalid glTexSubImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is invalid.");
        glTexSubImage2D(0, 0, 0, 0, 0, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(texsubimage2d_invalid_format, "Invalid glTexSubImage2D() usage", {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if format or type is not an accepted value.");
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 0, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES2F_ADD_API_CASE(texsubimage2d_invalid_type, "Invalid glTexSubImage2D() usage", {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if format or type is not an accepted value.");
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, GL_RGB, 0, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES2F_ADD_API_CASE(texsubimage2d_neg_level_tex2d, "Invalid glTexSubImage2D() usage", {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if level is less than 0.");
        glTexSubImage2D(GL_TEXTURE_2D, -1, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES2F_ADD_API_CASE(texsubimage2d_neg_level_cube, "Invalid glTexSubImage2D() usage", {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
        FOR_CUBE_FACES(faceGL, glTexImage2D(faceGL, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr););

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if level is less than 0.");
        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, -1, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, -1, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, -1, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, -1, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, -1, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, -1, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES2F_ADD_API_CASE(texsubimage2d_level_max_tex2d, "Invalid glTexSubImage2D() usage", {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        m_log << TestLog::Section("",
                                  "GL_INVALID_VALUE is generated if level is greater than log_2(GL_MAX_TEXTURE_SIZE).");
        uint32_t log2MaxTextureSize = deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_TEXTURE_SIZE)) + 1;
        glTexSubImage2D(GL_TEXTURE_2D, log2MaxTextureSize, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES2F_ADD_API_CASE(texsubimage2d_level_max_cube, "Invalid glTexSubImage2D() usage", {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_CUBE_MAP, texture);
        FOR_CUBE_FACES(faceGL, glTexImage2D(faceGL, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr););

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if level is greater than log_2(GL_MAX_CUBE_MAP_TEXTURE_SIZE).");
        uint32_t log2MaxTextureSize =
            deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE)) + 1;
        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, log2MaxTextureSize, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, log2MaxTextureSize, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, log2MaxTextureSize, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, log2MaxTextureSize, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, log2MaxTextureSize, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, log2MaxTextureSize, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES2F_ADD_API_CASE(texsubimage2d_neg_offset, "Invalid glTexSubImage2D() usage", {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if xoffset or yoffset are negative.");
        glTexSubImage2D(GL_TEXTURE_2D, 0, -1, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, -1, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexSubImage2D(GL_TEXTURE_2D, 0, -1, -1, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES2F_ADD_API_CASE(texsubimage2d_offset_allowed, "Invalid glTexSubImage2D() usage", {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        m_log << TestLog::Section(
            "",
            "GL_INVALID_VALUE is generated if xoffset + width > texture_width or yoffset + height > texture_height.");
        glTexSubImage2D(GL_TEXTURE_2D, 0, 8, 4, 10, 10, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 4, 8, 10, 10, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 8, 8, 10, 10, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES2F_ADD_API_CASE(texsubimage2d_neg_wdt_hgt, "Invalid glTexSubImage2D() usage", {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width or height is less than 0.");
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, -1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, -1, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, -1, -1, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES2F_ADD_API_CASE(texsubimage2d_type_format_mismatch, "Invalid glTexSubImage2D() usage", {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        m_log << tcu::TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if type is GL_UNSIGNED_SHORT_5_6_5 and format is not GL_RGB");
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_SHORT_5_6_5, 0);
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if type is GL_UNSIGNED_SHORT_4_4_4_4 or "
                                           "GL_UNSIGNED_SHORT_5_5_5_1 and format is not GL_RGBA.");
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, GL_RGB, GL_UNSIGNED_SHORT_4_4_4_4, 0);
        expectError(GL_INVALID_OPERATION);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, GL_RGB, GL_UNSIGNED_SHORT_5_5_5_1, 0);
        expectError(GL_INVALID_OPERATION);
        m_log << tcu::TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });

    // glTexParameteri

    ES2F_ADD_API_CASE(texparameteri, "Invalid glTexParameteri() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM is generated if target or pname is not one of the accepted defined values.");
        glTexParameteri(0, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        expectError(GL_INVALID_ENUM);
        glTexParameteri(GL_TEXTURE_2D, 0, GL_LINEAR);
        expectError(GL_INVALID_ENUM);
        glTexParameteri(0, 0, GL_LINEAR);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if params should have a defined symbolic constant "
                                      "value (based on the value of pname) and does not.");
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 0);
        expectError(GL_INVALID_ENUM);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_REPEAT);
        expectError(GL_INVALID_ENUM);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0);
        expectError(GL_INVALID_ENUM);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_NEAREST);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(texparameteri_bind, "Invalid glTexParameteri() usage", {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);

        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM is generated if target or pname is not one of the accepted defined values.");
        glTexParameteri(0, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        expectError(GL_INVALID_ENUM);
        glTexParameteri(GL_TEXTURE_2D, 0, GL_LINEAR);
        expectError(GL_INVALID_ENUM);
        glTexParameteri(0, 0, GL_LINEAR);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if params should have a defined symbolic constant "
                                      "value (based on the value of pname) and does not.");
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 0);
        expectError(GL_INVALID_ENUM);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_REPEAT);
        expectError(GL_INVALID_ENUM);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0);
        expectError(GL_INVALID_ENUM);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_NEAREST);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });

    // glTexParameterf

    ES2F_ADD_API_CASE(texparameterf, "Invalid glTexParameterf() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM is generated if target or pname is not one of the accepted defined values.");
        glTexParameterf(0, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        expectError(GL_INVALID_ENUM);
        glTexParameterf(GL_TEXTURE_2D, 0, GL_LINEAR);
        expectError(GL_INVALID_ENUM);
        glTexParameterf(0, 0, GL_LINEAR);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if params should have a defined symbolic constant "
                                      "value (based on the value of pname) and does not.");
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 0);
        expectError(GL_INVALID_ENUM);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_REPEAT);
        expectError(GL_INVALID_ENUM);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0);
        expectError(GL_INVALID_ENUM);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_NEAREST);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(texparameterf_bind, "Invalid glTexParameterf() usage", {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);

        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM is generated if target or pname is not one of the accepted defined values.");
        glTexParameterf(0, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        expectError(GL_INVALID_ENUM);
        glTexParameterf(GL_TEXTURE_2D, 0, GL_LINEAR);
        expectError(GL_INVALID_ENUM);
        glTexParameterf(0, 0, GL_LINEAR);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if params should have a defined symbolic constant "
                                      "value (based on the value of pname) and does not.");
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 0);
        expectError(GL_INVALID_ENUM);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_REPEAT);
        expectError(GL_INVALID_ENUM);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0);
        expectError(GL_INVALID_ENUM);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_NEAREST);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });

    // glTexParameteriv

    ES2F_ADD_API_CASE(texparameteriv, "Invalid glTexParameteriv() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM is generated if target or pname is not one of the accepted defined values.");
        GLint params[1] = {GL_LINEAR};
        glTexParameteriv(0, GL_TEXTURE_MIN_FILTER, &params[0]);
        expectError(GL_INVALID_ENUM);
        glTexParameteriv(GL_TEXTURE_2D, 0, &params[0]);
        expectError(GL_INVALID_ENUM);
        glTexParameteriv(0, 0, &params[0]);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if params should have a defined symbolic constant "
                                      "value (based on the value of pname) and does not.");
        params[0] = 0;
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &params[0]);
        expectError(GL_INVALID_ENUM);
        params[0] = GL_REPEAT;
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &params[0]);
        expectError(GL_INVALID_ENUM);
        params[0] = 0;
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &params[0]);
        expectError(GL_INVALID_ENUM);
        params[0] = GL_NEAREST;
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &params[0]);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(texparameteriv_bind, "Invalid glTexParameteriv() usage", {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);

        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM is generated if target or pname is not one of the accepted defined values.");
        GLint params[1] = {GL_LINEAR};
        glTexParameteriv(0, GL_TEXTURE_MIN_FILTER, &params[0]);
        expectError(GL_INVALID_ENUM);
        glTexParameteriv(GL_TEXTURE_2D, 0, &params[0]);
        expectError(GL_INVALID_ENUM);
        glTexParameteriv(0, 0, &params[0]);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if params should have a defined symbolic constant "
                                      "value (based on the value of pname) and does not.");
        params[0] = 0;
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &params[0]);
        expectError(GL_INVALID_ENUM);
        params[0] = GL_REPEAT;
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &params[0]);
        expectError(GL_INVALID_ENUM);
        params[0] = 0;
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &params[0]);
        expectError(GL_INVALID_ENUM);
        params[0] = GL_NEAREST;
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &params[0]);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });

    // glTexParameterfv

    ES2F_ADD_API_CASE(texparameterfv, "Invalid glTexParameterfv() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM is generated if target or pname is not one of the accepted defined values.");
        GLfloat params[1] = {GL_LINEAR};
        glTexParameterfv(0, GL_TEXTURE_MIN_FILTER, &params[0]);
        expectError(GL_INVALID_ENUM);
        glTexParameterfv(GL_TEXTURE_2D, 0, &params[0]);
        expectError(GL_INVALID_ENUM);
        glTexParameterfv(0, 0, &params[0]);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if params should have a defined symbolic constant "
                                      "value (based on the value of pname) and does not.");
        params[0] = 0.0f;
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &params[0]);
        expectError(GL_INVALID_ENUM);
        params[0] = GL_REPEAT;
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &params[0]);
        expectError(GL_INVALID_ENUM);
        params[0] = 0.0f;
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &params[0]);
        expectError(GL_INVALID_ENUM);
        params[0] = GL_NEAREST;
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &params[0]);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES2F_ADD_API_CASE(texparameterfv_bind, "Invalid glTexParameterfv() usage", {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);

        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM is generated if target or pname is not one of the accepted defined values.");
        GLfloat params[1] = {GL_LINEAR};
        glTexParameterfv(0, GL_TEXTURE_MIN_FILTER, &params[0]);
        expectError(GL_INVALID_ENUM);
        glTexParameterfv(GL_TEXTURE_2D, 0, &params[0]);
        expectError(GL_INVALID_ENUM);
        glTexParameterfv(0, 0, &params[0]);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if params should have a defined symbolic constant "
                                      "value (based on the value of pname) and does not.");
        params[0] = 0.0f;
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &params[0]);
        expectError(GL_INVALID_ENUM);
        params[0] = GL_REPEAT;
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &params[0]);
        expectError(GL_INVALID_ENUM);
        params[0] = 0.0f;
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &params[0]);
        expectError(GL_INVALID_ENUM);
        params[0] = GL_NEAREST;
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &params[0]);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });

    // glCompressedTexSubImage2D

    ES2F_ADD_API_CASE(compressedtexsubimage2d_invalid_target, "Invalid glCompressedTexSubImage2D() usage", {
        vector<int32_t> supported;
        vector<int32_t> accepted;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, supported);
        getCompressedTexSubImage2DFormat(supported, accepted);

        if (accepted.empty())
        {
            m_log << TestLog::Message << "// No suitable compressed formats found, cannot evaluate test."
                  << TestLog::EndMessage;
            m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "No suitable compressed formats found");
        }
        else
        {
            for (int i = 0; i < (int)accepted.size(); i++)
            {
                const int32_t glFormat = accepted[i];

                try
                {
                    const tcu::CompressedTexFormat format = glu::mapGLCompressedTexFormat(glFormat);
                    const tcu::IVec3 blockPixelSize       = tcu::getBlockPixelSize(format);
                    const int blockSize                   = tcu::getBlockSize(format);
                    const std::vector<uint8_t> data(blockSize, 0);
                    GLuint texture = 0;

                    glGenTextures(1, &texture);
                    glBindTexture(GL_TEXTURE_2D, texture);
                    expectError(GL_NO_ERROR);

                    glCompressedTexImage2D(GL_TEXTURE_2D, 0, glFormat, blockPixelSize.x(), blockPixelSize.y(), 0,
                                           blockSize, nullptr);
                    expectError(GL_NO_ERROR);

                    m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is invalid.");
                    m_log << TestLog::Message << "// Using texture format " << tcu::toHex(accepted[i])
                          << TestLog::EndMessage;
                    //glCompressedTexImage2D(GL_TEXTURE_2D, 0, accepted[i], 0, 0, 0, 0, 0);
                    glCompressedTexSubImage2D(0, 0, 0, 0, 0, 0, accepted[i], 0, 0);
                    expectError(GL_INVALID_ENUM);
                    m_log << TestLog::EndSection;

                    glDeleteTextures(1, &texture);
                    expectError(GL_NO_ERROR);
                }
                catch (const tcu::InternalError &)
                {
                    m_log << TestLog::Message << "Skipping unknown format: " << glFormat << TestLog::EndMessage;
                }
            }
        }
    });
    ES2F_ADD_API_CASE(compressedtexsubimage2d_neg_level_tex2d, "Invalid glCompressedTexSubImage2D() usage", {
        vector<int32_t> supported;
        vector<int32_t> accepted;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, supported);
        getCompressedTexSubImage2DFormat(supported, accepted);

        if (accepted.empty())
        {
            m_log << TestLog::Message << "// No suitable compressed formats found, cannot evaluate test."
                  << TestLog::EndMessage;
            m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "No suitable compressed formats found");
        }
        else
        {
            for (int i = 0; i < (int)accepted.size(); i++)
            {
                const int32_t glFormat = accepted[i];

                try
                {
                    const tcu::CompressedTexFormat format = glu::mapGLCompressedTexFormat(glFormat);
                    const tcu::IVec3 blockPixelSize       = tcu::getBlockPixelSize(format);
                    const int blockSize                   = tcu::getBlockSize(format);
                    const std::vector<uint8_t> data(blockSize, 0);
                    GLuint texture = 0;

                    glGenTextures(1, &texture);
                    glBindTexture(GL_TEXTURE_2D, texture);
                    expectError(GL_NO_ERROR);

                    glCompressedTexImage2D(GL_TEXTURE_2D, 0, glFormat, blockPixelSize.x(), blockPixelSize.y(), 0,
                                           blockSize, nullptr);
                    expectError(GL_NO_ERROR);

                    m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if level is less than 0.");
                    m_log << TestLog::Message << "// Using texture format " << tcu::toHex(accepted[i])
                          << TestLog::EndMessage;
                    //glCompressedTexImage2D(GL_TEXTURE_2D, 0, accepted[i], 0, 0, 0, 0, 0);
                    //expectError(GL_NO_ERROR);
                    glCompressedTexSubImage2D(GL_TEXTURE_2D, -1, 0, 0, 0, 0, accepted[i], 0, 0);
                    expectError(GL_INVALID_VALUE);
                    m_log << TestLog::EndSection;

                    glDeleteTextures(1, &texture);
                    expectError(GL_NO_ERROR);
                }
                catch (const tcu::InternalError &)
                {
                    m_log << TestLog::Message << "Skipping unknown format: " << glFormat << TestLog::EndMessage;
                }
            }
        }
    });
    ES2F_ADD_API_CASE(compressedtexsubimage2d_neg_level_cube, "Invalid glCompressedTexSubImage2D() usage", {
        vector<int32_t> supported;
        vector<int32_t> accepted;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, supported);
        getCompressedTexSubImage2DFormat(supported, accepted);

        if (accepted.empty())
        {
            m_log << TestLog::Message << "// No suitable compressed formats found, cannot evaluate test."
                  << TestLog::EndMessage;
            m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "No suitable compressed formats found");
        }
        else
        {
            for (int i = 0; i < (int)accepted.size(); i++)
            {
                const int32_t glFormat = accepted[i];

                try
                {
                    const tcu::CompressedTexFormat format = glu::mapGLCompressedTexFormat(glFormat);
                    const tcu::IVec3 blockPixelSize       = tcu::getBlockPixelSize(format);
                    const int blockSize                   = tcu::getBlockSize(format);
                    const std::vector<uint8_t> data(blockSize, 0);
                    GLuint texture = 0;

                    glGenTextures(1, &texture);
                    glBindTexture(GL_TEXTURE_2D, texture);
                    expectError(GL_NO_ERROR);

                    glCompressedTexImage2D(GL_TEXTURE_2D, 0, glFormat, blockPixelSize.x(), blockPixelSize.y(), 0,
                                           blockSize, nullptr);
                    expectError(GL_NO_ERROR);

                    m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if level is less than 0.");
                    m_log << TestLog::Message << "// Using texture format " << tcu::toHex(accepted[i])
                          << TestLog::EndMessage;
                    //glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, accepted[i], 0, 0, 0, 0, 0);
                    //expectError(GL_NO_ERROR);
                    glCompressedTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, -1, 0, 0, 0, 0, accepted[i], 0, 0);
                    expectError(GL_INVALID_VALUE);
                    //glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, accepted[i], 0, 0, 0, 0, 0);
                    //expectError(GL_NO_ERROR);
                    glCompressedTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, -1, 0, 0, 0, 0, accepted[i], 0, 0);
                    expectError(GL_INVALID_VALUE);
                    //glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, accepted[i], 0, 0, 0, 0, 0);
                    //expectError(GL_NO_ERROR);
                    glCompressedTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, -1, 0, 0, 0, 0, accepted[i], 0, 0);
                    expectError(GL_INVALID_VALUE);
                    //glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, accepted[i], 0, 0, 0, 0, 0);
                    //expectError(GL_NO_ERROR);
                    glCompressedTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, -1, 0, 0, 0, 0, accepted[i], 0, 0);
                    expectError(GL_INVALID_VALUE);
                    //glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, accepted[i], 0, 0, 0, 0, 0);
                    //expectError(GL_NO_ERROR);
                    glCompressedTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, -1, 0, 0, 0, 0, accepted[i], 0, 0);
                    expectError(GL_INVALID_VALUE);
                    //glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, accepted[i], 0, 0, 0, 0, 0);
                    //expectError(GL_NO_ERROR);
                    glCompressedTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, -1, 0, 0, 0, 0, accepted[i], 0, 0);
                    expectError(GL_INVALID_VALUE);
                    m_log << TestLog::EndSection;

                    glDeleteTextures(1, &texture);
                    expectError(GL_NO_ERROR);
                }
                catch (const tcu::InternalError &)
                {
                    m_log << TestLog::Message << "Skipping unknown format: " << glFormat << TestLog::EndMessage;
                }
            }
        }
    });
    ES2F_ADD_API_CASE(compressedtexsubimage2d_level_max_tex2d, "Invalid glCompressedTexSubImage2D() usage", {
        vector<int32_t> supported;
        vector<int32_t> accepted;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, supported);
        getCompressedTexSubImage2DFormat(supported, accepted);

        if (accepted.empty())
        {
            m_log << TestLog::Message << "// No suitable compressed formats found, cannot evaluate test."
                  << TestLog::EndMessage;
            m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "No suitable compressed formats found");
        }
        else
        {
            for (int i = 0; i < (int)accepted.size(); i++)
            {
                const int32_t glFormat = accepted[i];

                try
                {
                    const tcu::CompressedTexFormat format = glu::mapGLCompressedTexFormat(glFormat);
                    const tcu::IVec3 blockPixelSize       = tcu::getBlockPixelSize(format);
                    const int blockSize                   = tcu::getBlockSize(format);
                    const std::vector<uint8_t> data(blockSize, 0);
                    GLuint texture = 0;

                    glGenTextures(1, &texture);
                    glBindTexture(GL_TEXTURE_2D, texture);
                    expectError(GL_NO_ERROR);

                    glCompressedTexImage2D(GL_TEXTURE_2D, 0, glFormat, blockPixelSize.x(), blockPixelSize.y(), 0,
                                           blockSize, nullptr);
                    expectError(GL_NO_ERROR);

                    m_log << TestLog::Section(
                        "", "GL_INVALID_VALUE is generated if level is greater than log_2(GL_MAX_TEXTURE_SIZE).");
                    m_log << TestLog::Message << "// Using texture format " << tcu::toHex(accepted[i])
                          << TestLog::EndMessage;
                    uint32_t log2MaxTextureSize =
                        deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_TEXTURE_SIZE)) + 1;
                    //glCompressedTexImage2D(GL_TEXTURE_2D, 0, accepted[i], 0, 0, 0, 0, 0);
                    //expectError(GL_NO_ERROR);
                    glCompressedTexSubImage2D(GL_TEXTURE_2D, log2MaxTextureSize, 0, 0, 0, 0, accepted[i], 0, 0);
                    expectError(GL_INVALID_VALUE);
                    m_log << TestLog::EndSection;

                    glDeleteTextures(1, &texture);
                    expectError(GL_NO_ERROR);
                }
                catch (const tcu::InternalError &)
                {
                    m_log << TestLog::Message << "Skipping unknown format: " << glFormat << TestLog::EndMessage;
                }
            }
        }
    });
    ES2F_ADD_API_CASE(compressedtexsubimage2d_level_max_cube, "Invalid glCompressedTexSubImage2D() usage", {
        vector<int32_t> supported;
        vector<int32_t> accepted;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, supported);
        getCompressedTexSubImage2DFormat(supported, accepted);

        if (accepted.empty())
        {
            m_log << TestLog::Message << "// No suitable compressed formats found, cannot evaluate test."
                  << TestLog::EndMessage;
            m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "No suitable compressed formats found");
        }
        else
        {
            for (int i = 0; i < (int)accepted.size(); i++)
            {
                const int32_t glFormat = accepted[i];

                try
                {
                    const tcu::CompressedTexFormat format = glu::mapGLCompressedTexFormat(glFormat);
                    const tcu::IVec3 blockPixelSize       = tcu::getBlockPixelSize(format);
                    const int blockSize                   = tcu::getBlockSize(format);
                    const std::vector<uint8_t> data(blockSize, 0);
                    GLuint texture = 0;

                    glGenTextures(1, &texture);
                    glBindTexture(GL_TEXTURE_2D, texture);
                    expectError(GL_NO_ERROR);

                    glCompressedTexImage2D(GL_TEXTURE_2D, 0, glFormat, blockPixelSize.x(), blockPixelSize.y(), 0,
                                           blockSize, nullptr);
                    expectError(GL_NO_ERROR);

                    m_log << TestLog::Section(
                        "",
                        "GL_INVALID_VALUE is generated if level is greater than log_2(GL_MAX_CUBE_MAP_TEXTURE_SIZE).");
                    m_log << TestLog::Message << "// Using texture format " << tcu::toHex(accepted[i])
                          << TestLog::EndMessage;
                    uint32_t log2MaxTextureSize =
                        deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE)) + 1;
                    //glCompressedTexImage2D(GL_TEXTURE_2D, 0, accepted[i], 0, 0, 0, 0, 0);
                    //expectError(GL_NO_ERROR);
                    glCompressedTexSubImage2D(GL_TEXTURE_2D, log2MaxTextureSize, 0, 0, 0, 0, accepted[i], 0, 0);
                    expectError(GL_INVALID_VALUE);
                    //glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, accepted[i], 0, 0, 0, 0, 0);
                    //expectError(GL_NO_ERROR);
                    glCompressedTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, log2MaxTextureSize, 0, 0, 0, 0,
                                              accepted[i], 0, 0);
                    expectError(GL_INVALID_VALUE);
                    //glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, accepted[i], 0, 0, 0, 0, 0);
                    //expectError(GL_NO_ERROR);
                    glCompressedTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, log2MaxTextureSize, 0, 0, 0, 0,
                                              accepted[i], 0, 0);
                    expectError(GL_INVALID_VALUE);
                    //glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, accepted[i], 0, 0, 0, 0, 0);
                    //expectError(GL_NO_ERROR);
                    glCompressedTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, log2MaxTextureSize, 0, 0, 0, 0,
                                              accepted[i], 0, 0);
                    expectError(GL_INVALID_VALUE);
                    //glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, accepted[i], 0, 0, 0, 0, 0);
                    //expectError(GL_NO_ERROR);
                    glCompressedTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, log2MaxTextureSize, 0, 0, 0, 0,
                                              accepted[i], 0, 0);
                    expectError(GL_INVALID_VALUE);
                    //glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, accepted[i], 0, 0, 0, 0, 0);
                    //expectError(GL_NO_ERROR);
                    glCompressedTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, log2MaxTextureSize, 0, 0, 0, 0,
                                              accepted[i], 0, 0);
                    expectError(GL_INVALID_VALUE);
                    //glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, accepted[i], 0, 0, 0, 0, 0);
                    //expectError(GL_NO_ERROR);
                    glCompressedTexSubImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, log2MaxTextureSize, 0, 0, 0, 0,
                                              accepted[i], 0, 0);
                    expectError(GL_INVALID_VALUE);
                    m_log << TestLog::EndSection;

                    glDeleteTextures(1, &texture);
                    expectError(GL_NO_ERROR);
                }
                catch (const tcu::InternalError &)
                {
                    m_log << TestLog::Message << "Skipping unknown format: " << glFormat << TestLog::EndMessage;
                }
            }
        }
    });
    ES2F_ADD_API_CASE(compressedtexsubimage2d_neg_offset, "Invalid glCompressedTexSubImage2D() usage", {
        vector<int32_t> supported;
        vector<int32_t> accepted;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, supported);
        getCompressedTexSubImage2DFormat(supported, accepted);

        if (accepted.empty())
        {
            m_log << TestLog::Message << "// No suitable compressed formats found, cannot evaluate test."
                  << TestLog::EndMessage;
            m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "No suitable compressed formats found");
        }
        else
        {
            for (int i = 0; i < (int)accepted.size(); i++)
            {
                const int32_t glFormat = accepted[i];

                try
                {
                    const tcu::CompressedTexFormat format = glu::mapGLCompressedTexFormat(glFormat);
                    const tcu::IVec3 blockPixelSize       = tcu::getBlockPixelSize(format);
                    const int blockSize                   = tcu::getBlockSize(format);
                    const std::vector<uint8_t> data(blockSize, 0);
                    GLuint texture = 0;

                    glGenTextures(1, &texture);
                    glBindTexture(GL_TEXTURE_2D, texture);
                    expectError(GL_NO_ERROR);

                    glCompressedTexImage2D(GL_TEXTURE_2D, 0, glFormat, blockPixelSize.x(), blockPixelSize.y(), 0,
                                           blockSize, nullptr);
                    expectError(GL_NO_ERROR);

                    m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if xoffset or yoffset are negative.");
                    m_log << TestLog::Message << "// Using texture format " << tcu::toHex(accepted[i])
                          << TestLog::EndMessage;
                    //glCompressedTexImage2D(GL_TEXTURE_2D, 0, accepted[i], 0, 0, 0, 0, 0);
                    //expectError(GL_NO_ERROR);
                    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, -1, 0, 0, 0, accepted[i], 0, 0);
                    expectError(GL_INVALID_VALUE);
                    //glCompressedTexImage2D(GL_TEXTURE_2D, 0, accepted[i], 0, 0, 0, 0, 0);
                    //expectError(GL_NO_ERROR);
                    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, -1, 0, 0, accepted[i], 0, 0);
                    expectError(GL_INVALID_VALUE);
                    //glCompressedTexImage2D(GL_TEXTURE_2D, 0, accepted[i], 0, 0, 0, 0, 0);
                    //expectError(GL_NO_ERROR);
                    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, -1, -1, 0, 0, accepted[i], 0, 0);
                    expectError(GL_INVALID_VALUE);
                    m_log << TestLog::EndSection;

                    glDeleteTextures(1, &texture);
                    expectError(GL_NO_ERROR);
                }
                catch (const tcu::InternalError &)
                {
                    m_log << TestLog::Message << "Skipping unknown format: " << glFormat << TestLog::EndMessage;
                }
            }
        }
    });
    ES2F_ADD_API_CASE(compressedtexsubimage2d_offset_allowed, "Invalid glCompressedTexSubImage2D() usage", {
        vector<int32_t> supported;
        vector<int32_t> accepted;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, supported);
        getCompressedTexSubImage2DFormat(supported, accepted);

        if (accepted.empty())
        {
            m_log << TestLog::Message << "// No suitable compressed formats found, cannot evaluate test."
                  << TestLog::EndMessage;
            m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "No suitable compressed formats found");
        }
        else
        {
            for (int i = 0; i < (int)accepted.size(); i++)
            {
                const int32_t glFormat = accepted[i];

                try
                {
                    const tcu::CompressedTexFormat format = glu::mapGLCompressedTexFormat(glFormat);
                    const tcu::IVec3 blockPixelSize       = tcu::getBlockPixelSize(format);
                    const int blockSize                   = tcu::getBlockSize(format);
                    const std::vector<uint8_t> data(blockSize, 0);
                    GLuint texture = 0;

                    glGenTextures(1, &texture);
                    glBindTexture(GL_TEXTURE_2D, texture);
                    expectError(GL_NO_ERROR);

                    glCompressedTexImage2D(GL_TEXTURE_2D, 0, glFormat, blockPixelSize.x(), blockPixelSize.y(), 0,
                                           blockSize, nullptr);
                    expectError(GL_NO_ERROR);

                    uint32_t maxTextureSize = m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE) + 1;
                    m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if xoffset + width > texture_width or "
                                                  "yoffset + height > texture_height.");
                    m_log << TestLog::Message << "// Using texture format " << tcu::toHex(accepted[i])
                          << TestLog::EndMessage;
                    //glCompressedTexImage2D(GL_TEXTURE_2D, 0, accepted[i], 0, 0, 0, 0, 0);
                    //expectError(GL_NO_ERROR);
                    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, maxTextureSize, 0, 0, 0, accepted[i], 0, 0);
                    expectError(GL_INVALID_VALUE);
                    //glCompressedTexImage2D(GL_TEXTURE_2D, 0, accepted[i], 0, 0, 0, 0, 0);
                    //expectError(GL_NO_ERROR);
                    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, maxTextureSize, 0, 0, accepted[i], 0, 0);
                    expectError(GL_INVALID_VALUE);
                    //glCompressedTexImage2D(GL_TEXTURE_2D, 0, accepted[i], 0, 0, 0, 0, 0);
                    //expectError(GL_NO_ERROR);
                    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, maxTextureSize, maxTextureSize, 0, 0, accepted[i], 0,
                                              0);
                    expectError(GL_INVALID_VALUE);
                    m_log << TestLog::EndSection;

                    glDeleteTextures(1, &texture);
                    expectError(GL_NO_ERROR);
                }
                catch (const tcu::InternalError &)
                {
                    m_log << TestLog::Message << "Skipping unknown format: " << glFormat << TestLog::EndMessage;
                }
            }
        }
    });
    ES2F_ADD_API_CASE(compressedtexsubimage2d_neg_wdt_hgt, "Invalid glCompressedTexSubImage2D() usage", {
        vector<int32_t> supported;
        vector<int32_t> accepted;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, supported);
        getCompressedTexSubImage2DFormat(supported, accepted);

        if (accepted.empty())
        {
            m_log << TestLog::Message << "// No suitable compressed formats found, cannot evaluate test."
                  << TestLog::EndMessage;
            m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "No suitable compressed formats found");
        }
        else
        {
            for (int i = 0; i < (int)accepted.size(); i++)
            {
                const int32_t glFormat = accepted[i];

                try
                {
                    const tcu::CompressedTexFormat format = glu::mapGLCompressedTexFormat(glFormat);
                    const tcu::IVec3 blockPixelSize       = tcu::getBlockPixelSize(format);
                    const int blockSize                   = tcu::getBlockSize(format);
                    const std::vector<uint8_t> data(blockSize, 0);
                    GLuint texture = 0;

                    glGenTextures(1, &texture);
                    glBindTexture(GL_TEXTURE_2D, texture);
                    expectError(GL_NO_ERROR);

                    glCompressedTexImage2D(GL_TEXTURE_2D, 0, glFormat, blockPixelSize.x(), blockPixelSize.y(), 0,
                                           blockSize, nullptr);
                    expectError(GL_NO_ERROR);

                    m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width or height is less than 0.");
                    m_log << TestLog::Message << "// Using texture format " << tcu::toHex(accepted[i])
                          << TestLog::EndMessage;
                    //glCompressedTexImage2D(GL_TEXTURE_2D, 0, accepted[i], 0, 0, 0, 0, 0);
                    //expectError(GL_NO_ERROR);
                    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, -1, 0, accepted[i], 0, 0);
                    expectError(GL_INVALID_VALUE);
                    //glCompressedTexImage2D(GL_TEXTURE_2D, 0, accepted[i], 0, 0, 0, 0, 0);
                    //expectError(GL_NO_ERROR);
                    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, -1, accepted[i], 0, 0);
                    expectError(GL_INVALID_VALUE);
                    //glCompressedTexImage2D(GL_TEXTURE_2D, 0, accepted[i], 0, 0, 0, 0, 0);
                    //expectError(GL_NO_ERROR);
                    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, -1, -1, accepted[i], 0, 0);
                    expectError(GL_INVALID_VALUE);
                    m_log << TestLog::EndSection;

                    glDeleteTextures(1, &texture);
                    expectError(GL_NO_ERROR);
                }
                catch (const tcu::InternalError &)
                {
                    m_log << TestLog::Message << "Skipping unknown format: " << glFormat << TestLog::EndMessage;
                }
            }
        }
    });
    ES2F_ADD_API_CASE(compressedtexsubimage2d_invalid_size, "Invalid glCompressedTexImage2D() usage", {
        vector<int32_t> supported;
        vector<int32_t> accepted;
        getSupportedExtensions(GL_NUM_COMPRESSED_TEXTURE_FORMATS, GL_COMPRESSED_TEXTURE_FORMATS, supported);
        getCompressedTexSubImage2DFormat(supported, accepted);

        if (accepted.empty())
        {
            m_log << TestLog::Message << "// No suitable compressed formats found, cannot evaluate test."
                  << TestLog::EndMessage;
            m_testCtx.setTestResult(QP_TEST_RESULT_NOT_SUPPORTED, "No suitable compressed formats found");
        }
        else
        {
            for (int i = 0; i < (int)accepted.size(); i++)
            {
                const int32_t glFormat = accepted[i];

                try
                {
                    const tcu::CompressedTexFormat format = glu::mapGLCompressedTexFormat(glFormat);
                    const tcu::IVec3 blockPixelSize       = tcu::getBlockPixelSize(format);
                    const int blockSize                   = tcu::getBlockSize(format);
                    const std::vector<uint8_t> data(blockSize, 0);
                    GLuint texture = 0;

                    glGenTextures(1, &texture);
                    glBindTexture(GL_TEXTURE_2D, texture);
                    expectError(GL_NO_ERROR);

                    glCompressedTexImage2D(GL_TEXTURE_2D, 0, glFormat, blockPixelSize.x(), blockPixelSize.y(), 0,
                                           blockSize, nullptr);
                    expectError(GL_NO_ERROR);

                    m_log << TestLog::Section(
                        "", "GL_INVALID_VALUE is generated if imageSize is not consistent with the format, dimensions, "
                            "and contents of the specified compressed image data.");
                    m_log << TestLog::Message << "// Using texture format " << tcu::toHex(glFormat)
                          << TestLog::EndMessage;
                    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, blockPixelSize.x(), blockPixelSize.y(), glFormat,
                                              -1, &data[0]);
                    expectError(GL_INVALID_VALUE);

                    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, blockPixelSize.x(), blockPixelSize.y(), glFormat,
                                              blockSize / 2, &data[0]);
                    expectError(GL_INVALID_VALUE);

                    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, blockPixelSize.x(), blockPixelSize.y(), glFormat,
                                              blockSize * 2, &data[0]);
                    expectError(GL_INVALID_VALUE);

                    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, blockPixelSize.x(), blockPixelSize.y() * 2,
                                              glFormat, blockSize, &data[0]);
                    expectError(GL_INVALID_VALUE);

                    glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, blockPixelSize.x() * 2, blockPixelSize.y(),
                                              glFormat, blockSize, &data[0]);
                    expectError(GL_INVALID_VALUE);

                    m_log << TestLog::EndSection;

                    glDeleteTextures(1, &texture);
                    expectError(GL_NO_ERROR);
                }
                catch (const tcu::InternalError &)
                {
                    m_log << TestLog::Message << "Skipping unknown format: " << glFormat << TestLog::EndMessage;
                }
            }
        }
    });
}

} // namespace Functional
} // namespace gles2
} // namespace deqp
