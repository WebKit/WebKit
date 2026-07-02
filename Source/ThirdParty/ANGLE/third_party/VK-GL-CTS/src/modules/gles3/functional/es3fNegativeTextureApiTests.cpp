/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.0 Module
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

#include "es3fNegativeTextureApiTests.hpp"
#include "es3fApiCase.hpp"
#include "gluContextInfo.hpp"
#include "tcuFormatUtil.hpp"
#include "gluTextureUtil.hpp"

#include <vector>
#include <algorithm>

#include "glwDefs.hpp"
#include "glwEnums.hpp"

using namespace glw; // GL types

namespace deqp
{
namespace gles3
{
namespace Functional
{

using glu::mapGLCompressedTexFormat;
using std::vector;
using tcu::CompressedTexFormat;
using tcu::getBlockPixelSize;
using tcu::getBlockSize;
using tcu::IVec3;
using tcu::TestLog;

static inline int divRoundUp(int a, int b)
{
    return a / b + (a % b != 0 ? 1 : 0);
}

static inline int etc2DataSize(int width, int height)
{
    return (int)(divRoundUp(width, 4) * divRoundUp(height, 4) * sizeof(uint64_t));
}

static inline int etc2EacDataSize(int width, int height)
{
    return 2 * etc2DataSize(width, height);
}

static const GLuint s_astcFormats[] = {
    GL_COMPRESSED_RGBA_ASTC_4x4_KHR,           GL_COMPRESSED_RGBA_ASTC_5x4_KHR,
    GL_COMPRESSED_RGBA_ASTC_5x5_KHR,           GL_COMPRESSED_RGBA_ASTC_6x5_KHR,
    GL_COMPRESSED_RGBA_ASTC_6x6_KHR,           GL_COMPRESSED_RGBA_ASTC_8x5_KHR,
    GL_COMPRESSED_RGBA_ASTC_8x6_KHR,           GL_COMPRESSED_RGBA_ASTC_8x8_KHR,
    GL_COMPRESSED_RGBA_ASTC_10x5_KHR,          GL_COMPRESSED_RGBA_ASTC_10x6_KHR,
    GL_COMPRESSED_RGBA_ASTC_10x8_KHR,          GL_COMPRESSED_RGBA_ASTC_10x10_KHR,
    GL_COMPRESSED_RGBA_ASTC_12x10_KHR,         GL_COMPRESSED_RGBA_ASTC_12x12_KHR,
    GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR,   GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR,
    GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR,   GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR,
    GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR,   GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR,
    GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR,   GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR,
    GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR,  GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR,
    GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR,  GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR,
    GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR};

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

    ES3F_ADD_API_CASE(activetexture, "Invalid glActiveTexture() usage", {
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

    ES3F_ADD_API_CASE(bindtexture, "Invalid glBindTexture() usage", {
        GLuint texture[2];
        glGenTextures(2, texture);

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is not one of the allowable values.");
        glBindTexture(0, 1);
        expectError(GL_INVALID_ENUM);
        glBindTexture(GL_FRAMEBUFFER, 1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if texture was previously created with a "
                                      "target that doesn't match that of target.");
        glBindTexture(GL_TEXTURE_2D, texture[0]);
        expectError(GL_NO_ERROR);
        glBindTexture(GL_TEXTURE_CUBE_MAP, texture[0]);
        expectError(GL_INVALID_OPERATION);
        glBindTexture(GL_TEXTURE_3D, texture[0]);
        expectError(GL_INVALID_OPERATION);
        glBindTexture(GL_TEXTURE_2D_ARRAY, texture[0]);
        expectError(GL_INVALID_OPERATION);

        glBindTexture(GL_TEXTURE_CUBE_MAP, texture[1]);
        expectError(GL_NO_ERROR);
        glBindTexture(GL_TEXTURE_2D, texture[1]);
        expectError(GL_INVALID_OPERATION);
        glBindTexture(GL_TEXTURE_3D, texture[1]);
        expectError(GL_INVALID_OPERATION);
        glBindTexture(GL_TEXTURE_2D_ARRAY, texture[1]);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteTextures(2, texture);
    });

    // glCompressedTexImage2D

    ES3F_ADD_API_CASE(compressedteximage2d_invalid_target, "Invalid glCompressedTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is invalid.");
        glCompressedTexImage2D(0, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_ENUM);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(compressedteximage2d_invalid_format, "Invalid glCompressedTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if internalformat is not a supported format "
                                      "returned in GL_COMPRESSED_TEXTURE_FORMATS.");
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_ENUM);
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_ENUM);
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_ENUM);
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
    ES3F_ADD_API_CASE(compressedteximage2d_neg_level, "Invalid glCompressedTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if level is less than 0.");
        glCompressedTexImage2D(GL_TEXTURE_2D, -1, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, -1, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, -1, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, -1, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, -1, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, -1, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, -1, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(compressedteximage2d_max_level, "Invalid glCompressedTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if level is greater than "
                                      "log_2(GL_MAX_TEXTURE_SIZE) for a 2d texture target.");
        uint32_t log2MaxTextureSize = deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_TEXTURE_SIZE)) + 1;
        glCompressedTexImage2D(GL_TEXTURE_2D, log2MaxTextureSize, GL_COMPRESSED_RGB8_ETC2, 16, 16, 0,
                               etc2DataSize(16, 16), 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if level is greater than "
                                      "log_2(GL_MAX_CUBE_MAP_TEXTURE_SIZE) for a cubemap target.");
        uint32_t log2MaxCubemapSize =
            deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE)) + 1;
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, log2MaxCubemapSize, GL_COMPRESSED_RGBA8_ETC2_EAC, 16, 16,
                               0, etc2EacDataSize(16, 16), 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, log2MaxCubemapSize, GL_COMPRESSED_RGBA8_ETC2_EAC, 16, 16,
                               0, etc2EacDataSize(16, 16), 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, log2MaxCubemapSize, GL_COMPRESSED_RGBA8_ETC2_EAC, 16, 16,
                               0, etc2EacDataSize(16, 16), 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, log2MaxCubemapSize, GL_COMPRESSED_RGBA8_ETC2_EAC, 16, 16,
                               0, etc2EacDataSize(16, 16), 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, log2MaxCubemapSize, GL_COMPRESSED_RGBA8_ETC2_EAC, 16, 16,
                               0, etc2EacDataSize(16, 16), 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, log2MaxCubemapSize, GL_COMPRESSED_RGBA8_ETC2_EAC, 16, 16,
                               0, etc2EacDataSize(16, 16), 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(compressedteximage2d_neg_width_height, "Invalid glCompressedTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width or height is less than 0.");

        m_log << TestLog::Section("", "GL_TEXTURE_2D target");
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, -1, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, -1, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, -1, -1, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_POSITIVE_X target");
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, -1, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, -1, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, -1, -1, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_POSITIVE_Y target");
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, -1, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, -1, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, -1, -1, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_POSITIVE_Z target");
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, -1, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, -1, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, -1, -1, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_NEGATIVE_X target");
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, -1, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, -1, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, -1, -1, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_NEGATIVE_Y target");
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, -1, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, -1, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, -1, -1, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_NEGATIVE_Z target");
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, -1, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, -1, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, -1, -1, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(compressedteximage2d_max_width_height, "Invalid glCompressedTexImage2D() usage", {
        int maxTextureSize = m_context.getContextInfo().getInt(GL_MAX_TEXTURE_SIZE) + 1;
        int maxCubemapSize = m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE) + 1;
        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if width or height is greater than GL_MAX_TEXTURE_SIZE.");

        m_log << TestLog::Section("", "GL_TEXTURE_2D target");
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, maxTextureSize, 1, 0,
                               etc2EacDataSize(maxTextureSize, 1), 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 1, maxTextureSize, 0,
                               etc2EacDataSize(1, maxTextureSize), 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, maxTextureSize, maxTextureSize, 0,
                               etc2EacDataSize(maxTextureSize, maxTextureSize), 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_POSITIVE_X target");
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, maxCubemapSize, 1, 0,
                               etc2EacDataSize(maxCubemapSize, 1), 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 1, maxCubemapSize, 0,
                               etc2EacDataSize(1, maxCubemapSize), 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, maxCubemapSize,
                               maxCubemapSize, 0, etc2EacDataSize(maxCubemapSize, maxCubemapSize), 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_POSITIVE_Y target");
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, maxCubemapSize, 1, 0,
                               etc2EacDataSize(maxCubemapSize, 1), 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 1, maxCubemapSize, 0,
                               etc2EacDataSize(1, maxCubemapSize), 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, maxCubemapSize,
                               maxCubemapSize, 0, etc2EacDataSize(maxCubemapSize, maxCubemapSize), 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_POSITIVE_Z target");
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, maxCubemapSize, 1, 0,
                               etc2EacDataSize(maxCubemapSize, 1), 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 1, maxCubemapSize, 0,
                               etc2EacDataSize(1, maxCubemapSize), 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, maxCubemapSize,
                               maxCubemapSize, 0, etc2EacDataSize(maxCubemapSize, maxCubemapSize), 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_NEGATIVE_X target");
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, maxCubemapSize, 1, 0,
                               etc2EacDataSize(maxCubemapSize, 1), 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 1, maxCubemapSize, 0,
                               etc2EacDataSize(1, maxCubemapSize), 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, maxCubemapSize,
                               maxCubemapSize, 0, etc2EacDataSize(maxCubemapSize, maxCubemapSize), 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_NEGATIVE_Y target");
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, maxCubemapSize, 1, 0,
                               etc2EacDataSize(maxCubemapSize, 1), 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 1, maxCubemapSize, 0,
                               etc2EacDataSize(1, maxCubemapSize), 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, maxCubemapSize,
                               maxCubemapSize, 0, etc2EacDataSize(maxCubemapSize, maxCubemapSize), 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_NEGATIVE_Z target");
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, maxCubemapSize, 1, 0,
                               etc2EacDataSize(maxCubemapSize, 1), 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 1, maxCubemapSize, 0,
                               etc2EacDataSize(1, maxCubemapSize), 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, maxCubemapSize,
                               maxCubemapSize, 0, etc2EacDataSize(maxCubemapSize, maxCubemapSize), 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(compressedteximage2d_invalid_border, "Invalid glCompressedTexImage2D() usage", {
        bool isES    = glu::isContextTypeES(m_context.getRenderContext().getType());
        GLenum error = isES ? GL_INVALID_VALUE : GL_INVALID_OPERATION;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if border is not 0.");

        m_log << TestLog::Section("", "GL_TEXTURE_2D target");
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, 1, 0, 0);
        expectError(error);
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, -1, 0, 0);
        expectError(error);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_POSITIVE_X target");
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, 1, 0, 0);
        expectError(error);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, -1, 0, 0);
        expectError(error);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_POSITIVE_Y target");
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, 1, 0, 0);
        expectError(error);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, -1, 0, 0);
        expectError(error);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_POSITIVE_Z target");
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, 1, 0, 0);
        expectError(error);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, -1, 0, 0);
        expectError(error);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_NEGATIVE_X target");
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, 1, 0, 0);
        expectError(error);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, -1, 0, 0);
        expectError(error);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_NEGATIVE_Y target");
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, 1, 0, 0);
        expectError(error);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, -1, 0, 0);
        expectError(error);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_NEGATIVE_Z target");
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, 1, 0, 0);
        expectError(error);
        glCompressedTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, -1, 0, 0);
        expectError(error);
        m_log << TestLog::EndSection;

        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(compressedteximage2d_invalid_size, "Invalid glCompressedTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if imageSize is not consistent with the format, "
                                      "dimensions, and contents of the specified compressed image data.");
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, 0, -1, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 16, 16, 0, 4 * 4 * 8, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB8_ETC2, 16, 16, 0, 4 * 4 * 16, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_SIGNED_R11_EAC, 16, 16, 0, 4 * 4 * 16, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(compressedteximage2d_invalid_buffer_target, "Invalid glCompressedTexImage2D() usage", {
        uint32_t buf;
        std::vector<GLubyte> data(64);

        glGenBuffers(1, &buf);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buf);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, 64, &data[0], GL_DYNAMIC_COPY);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if a non-zero buffer object name is bound to the "
                "GL_PIXEL_UNPACK_BUFFER target and the buffer object's data store is currently mapped.");
        glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, 32, GL_MAP_WRITE_BIT);
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB8_ETC2, 4, 4, 0, etc2DataSize(4, 4), 0);
        expectError(GL_INVALID_OPERATION);
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("",
                                  "GL_INVALID_OPERATION is generated if a non-zero buffer object name is bound to the "
                                  "GL_PIXEL_UNPACK_BUFFER target and the data would be unpacked from the buffer object "
                                  "such that the memory reads required would exceed the data store size.");
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGB8_ETC2, 16, 16, 0, etc2DataSize(16, 16), 0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteBuffers(1, &buf);
    });
    ES3F_ADD_API_CASE(
        compressedteximage2d_invalid_astc_target, "ASTC formats should not be supported without a proper extension.", {
            if (m_context.getContextInfo().isExtensionSupported("GL_KHR_texture_compression_astc_ldr"))
            {
                m_log.writeMessage("ASTC supported. No negative API requirements.");
            }
            else
            {
                m_log.writeMessage("GL_INVALID_ENUM should be generated if no ASTC extensions are present.");

                for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(s_astcFormats); formatNdx++)
                {
                    const GLuint format                 = s_astcFormats[formatNdx];
                    const CompressedTexFormat tcuFormat = mapGLCompressedTexFormat(format);
                    const IVec3 blockPixels             = getBlockPixelSize(tcuFormat);
                    {
                        const size_t blockBytes = getBlockSize(tcuFormat);
                        const vector<uint8_t> unusedData(blockBytes);

                        glCompressedTexImage2D(GL_TEXTURE_2D, 0, format, blockPixels.x(), blockPixels.y(), 0,
                                               (int)blockBytes, &unusedData[0]);
                        expectError(GL_INVALID_ENUM);
                    }
                    FOR_CUBE_FACES(faceGL, {
                        const int32_t cubeSize =
                            blockPixels.x() * blockPixels.y(); // Divisible by the block size and square
                        const size_t blockBytes = getBlockSize(tcuFormat) * cubeSize; // We have a x * y grid of blocks
                        const vector<uint8_t> unusedData(blockBytes);

                        glCompressedTexImage2D(faceGL, 0, format, cubeSize, cubeSize, 0, (int)blockBytes,
                                               &unusedData[0]);
                        expectError(GL_INVALID_ENUM);
                    });
                }
            }
        });

    // glCopyTexImage2D

    ES3F_ADD_API_CASE(copyteximage2d_invalid_target, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is invalid.");
        glCopyTexImage2D(0, 0, GL_RGB, 0, 0, 64, 64, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(copyteximage2d_invalid_format, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM or GL_INVALID_VALUE is generated if internalformat is not an accepted format.");
        glCopyTexImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 64, 64, 0);
        expectError(GL_INVALID_ENUM, GL_INVALID_VALUE);
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
    ES3F_ADD_API_CASE(copyteximage2d_inequal_width_height_cube, "Invalid glCopyTexImage2D() usage", {
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
    ES3F_ADD_API_CASE(copyteximage2d_neg_level, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if level is less than 0.");
        glCopyTexImage2D(GL_TEXTURE_2D, -1, GL_RGB, 0, 0, 64, 64, 0);
        expectError(GL_INVALID_VALUE);
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
    ES3F_ADD_API_CASE(copyteximage2d_max_level, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section("",
                                  "GL_INVALID_VALUE is generated if level is greater than log_2(GL_MAX_TEXTURE_SIZE).");
        uint32_t log2MaxTextureSize = deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_TEXTURE_SIZE)) + 1;
        glCopyTexImage2D(GL_TEXTURE_2D, log2MaxTextureSize, GL_RGB, 0, 0, 64, 64, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if level is greater than log_2(GL_MAX_CUBE_MAP_TEXTURE_SIZE).");
        uint32_t log2MaxCubemapSize =
            deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE)) + 1;
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, log2MaxCubemapSize, GL_RGB, 0, 0, 16, 16, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, log2MaxCubemapSize, GL_RGB, 0, 0, 16, 16, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, log2MaxCubemapSize, GL_RGB, 0, 0, 16, 16, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, log2MaxCubemapSize, GL_RGB, 0, 0, 16, 16, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, log2MaxCubemapSize, GL_RGB, 0, 0, 16, 16, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, log2MaxCubemapSize, GL_RGB, 0, 0, 16, 16, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(copyteximage2d_neg_width_height, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width or height is less than 0.");

        m_log << TestLog::Section("", "GL_TEXTURE_2D target");
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, -1, 1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 1, -1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, -1, -1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_POSITIVE_X target");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 0, 0, -1, 1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 0, 0, 1, -1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 0, 0, -1, -1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_POSITIVE_Y target");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, 0, 0, -1, 1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, 0, 0, 1, -1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, 0, 0, -1, -1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_POSITIVE_Z target");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, 0, 0, -1, 1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, 0, 0, 1, -1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, 0, 0, -1, -1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_NEGATIVE_X target");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, 0, 0, -1, 1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, 0, 0, 1, -1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, 0, 0, -1, -1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_NEGATIVE_Y target");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, 0, 0, -1, 1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, 0, 0, 1, -1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, 0, 0, -1, -1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_NEGATIVE_Z target");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, 0, 0, -1, 1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, 0, 0, 1, -1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, 0, 0, -1, -1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(copyteximage2d_max_width_height, "Invalid glCopyTexImage2D() usage", {
        int maxTextureSize = m_context.getContextInfo().getInt(GL_MAX_TEXTURE_SIZE) + 1;
        int maxCubemapSize = m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE) + 1;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if width or height is greater than GL_MAX_TEXTURE_SIZE.");

        m_log << TestLog::Section("", "GL_TEXTURE_2D target");
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, maxTextureSize, 1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 1, maxTextureSize, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, maxTextureSize, maxTextureSize, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_POSITIVE_X target");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 0, 0, 1, maxCubemapSize, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 0, 0, maxCubemapSize, 1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 0, 0, maxCubemapSize, maxCubemapSize, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_POSITIVE_Y target");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, 0, 0, 1, maxCubemapSize, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, 0, 0, maxCubemapSize, 1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, 0, 0, maxCubemapSize, maxCubemapSize, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_POSITIVE_Z target");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, 0, 0, 1, maxCubemapSize, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, 0, 0, maxCubemapSize, 1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, 0, 0, maxCubemapSize, maxCubemapSize, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_NEGATIVE_X target");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, 0, 0, 1, maxCubemapSize, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, 0, 0, maxCubemapSize, 1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, 0, 0, maxCubemapSize, maxCubemapSize, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_NEGATIVE_Y target");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, 0, 0, 1, maxCubemapSize, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, 0, 0, maxCubemapSize, 1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, 0, 0, maxCubemapSize, maxCubemapSize, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_NEGATIVE_Z target");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, 0, 0, 1, maxCubemapSize, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, 0, 0, maxCubemapSize, 1, 0);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, 0, 0, maxCubemapSize, maxCubemapSize, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(copyteximage2d_invalid_border, "Invalid glCopyTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if border is not 0.");

        m_log << TestLog::Section("", "GL_TEXTURE_2D target");
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 0, 0, -1);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 0, 0, 1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_POSITIVE_X target");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 0, 0, 0, 0, -1);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 0, 0, 0, 0, 1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_POSITIVE_Y target");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, 0, 0, 0, 0, -1);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, 0, 0, 0, 0, 1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_2D target");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, 0, 0, 0, 0, -1);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, 0, 0, 0, 0, 1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_NEGATIVE_X target");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, 0, 0, 0, 0, -1);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, 0, 0, 0, 0, 1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_NEGATIVE_Y target");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, 0, 0, 0, 0, -1);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, 0, 0, 0, 0, 1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_NEGATIVE_Z target");
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, 0, 0, 0, 0, -1);
        expectError(GL_INVALID_VALUE);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, 0, 0, 0, 0, 1);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(copyteximage2d_incomplete_framebuffer, "Invalid glCopyTexImage2D() usage", {
        GLuint fbo;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glCheckFramebufferStatus(GL_FRAMEBUFFER);

        m_log << tcu::TestLog::Section("", "GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound "
                                           "framebuffer is not framebuffer complete.");
        glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGBA8, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGBA8, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGBA8, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGBA8, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGBA8, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glCopyTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGBA8, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        m_log << tcu::TestLog::EndSection;

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
    });

    // glCopyTexSubImage2D

    ES3F_ADD_API_CASE(copytexsubimage2d_invalid_target, "Invalid glCopyTexSubImage2D() usage", {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is invalid.");
        glCopyTexSubImage2D(0, 0, 0, 0, 0, 0, 4, 4);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES3F_ADD_API_CASE(copytexsubimage2d_neg_level, "Invalid glCopyTexSubImage2D() usage", {
        GLuint textures[2];
        glGenTextures(2, &textures[0]);
        glBindTexture(GL_TEXTURE_2D, textures[0]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, textures[1]);
        FOR_CUBE_FACES(faceGL, glTexImage2D(faceGL, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0););

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if level is less than 0.");
        glCopyTexSubImage2D(GL_TEXTURE_2D, -1, 0, 0, 0, 0, 4, 4);
        expectError(GL_INVALID_VALUE);
        FOR_CUBE_FACES(faceGL, {
            glCopyTexSubImage2D(faceGL, -1, 0, 0, 0, 0, 4, 4);
            expectError(GL_INVALID_VALUE);
        });
        m_log << TestLog::EndSection;

        glDeleteTextures(2, &textures[0]);
    });
    ES3F_ADD_API_CASE(copytexsubimage2d_max_level, "Invalid glCopyTexSubImage2D() usage", {
        GLuint textures[2];
        glGenTextures(2, &textures[0]);
        glBindTexture(GL_TEXTURE_2D, textures[0]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, textures[1]);
        FOR_CUBE_FACES(faceGL, glTexImage2D(faceGL, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0););

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if level is greater than "
                                      "log_2(GL_MAX_TEXTURE_SIZE) for 2D texture targets.");
        uint32_t log2MaxTextureSize = deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_TEXTURE_SIZE)) + 1;
        glCopyTexSubImage2D(GL_TEXTURE_2D, log2MaxTextureSize, 0, 0, 0, 0, 4, 4);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "",
            "GL_INVALID_VALUE is generated if level is greater than log_2(GL_MAX_CUBE_MAP_SIZE) for cubemap targets.");
        uint32_t log2MaxCubemapSize =
            deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE)) + 1;
        FOR_CUBE_FACES(faceGL, {
            glCopyTexSubImage2D(faceGL, log2MaxCubemapSize, 0, 0, 0, 0, 4, 4);
            expectError(GL_INVALID_VALUE);
        });
        m_log << TestLog::EndSection;

        glDeleteTextures(2, &textures[0]);
    });
    ES3F_ADD_API_CASE(copytexsubimage2d_neg_offset, "Invalid glCopyTexSubImage2D() usage", {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if xoffset < 0 or yoffset < 0.");
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, -1, 0, 0, 0, 4, 4);
        expectError(GL_INVALID_VALUE);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, -1, 0, 0, 4, 4);
        expectError(GL_INVALID_VALUE);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, -1, -1, 0, 0, 4, 4);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES3F_ADD_API_CASE(copytexsubimage2d_invalid_offset, "Invalid glCopyTexSubImage2D() usage", {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

        m_log << TestLog::Section(
            "",
            "GL_INVALID_VALUE is generated if xoffset + width > texture_width or yoffset + height > texture_height.");
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 14, 0, 0, 0, 4, 4);
        expectError(GL_INVALID_VALUE);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 14, 0, 0, 4, 4);
        expectError(GL_INVALID_VALUE);
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 14, 14, 0, 0, 4, 4);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES3F_ADD_API_CASE(copytexsubimage2d_neg_width_height, "Invalid glCopyTexSubImage2D() usage", {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

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
    ES3F_ADD_API_CASE(copytexsubimage2d_incomplete_framebuffer, "Invalid glCopyTexSubImage2D() usage", {
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

    ES3F_ADD_API_CASE(deletetextures, "Invalid glDeleteTextures() usage", {
        GLuint texture;
        glGenTextures(1, &texture);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if n is negative.");
        glDeleteTextures(-1, 0);
        expectError(GL_INVALID_VALUE);

        glBindTexture(GL_TEXTURE_2D, texture);
        glDeleteTextures(-1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });

    // glGenerateMipmap

    ES3F_ADD_API_CASE(generatemipmap, "Invalid glGenerateMipmap() usage", {
        GLuint texture[2];
        glGenTextures(2, texture);

        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM is generated if target is not GL_TEXTURE_2D or GL_TEXTURE_CUBE_MAP.");
        glGenerateMipmap(0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "INVALID_OPERATION is generated if the texture bound to target is not cube complete.");
        glBindTexture(GL_TEXTURE_CUBE_MAP, texture[0]);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 0, 0, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        expectError(GL_INVALID_OPERATION);

        glBindTexture(GL_TEXTURE_CUBE_MAP, texture[0]);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 16, 16, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, 16, 16, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, 16, 16, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, 16, 16, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, 16, 16, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, 32, 32, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        if (glu::isContextTypeES(m_context.getRenderContext().getType()))
        {
            m_log << TestLog::Section(
                "",
                "GL_INVALID_OPERATION is generated if the zero level array is stored in a compressed internal format.");
            glBindTexture(GL_TEXTURE_2D, texture[1]);
            glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, 0, 0, 0);
            glGenerateMipmap(GL_TEXTURE_2D);
            expectError(GL_INVALID_OPERATION);
            m_log << TestLog::EndSection;

            m_log << TestLog::Section(
                "", "GL_INVALID_OPERATION is generated if the level base array was not specified with an unsized "
                    "internal format or a sized internal format that is both color-renderable and texture-filterable.");
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8_SNORM, 0, 0, 0, GL_RGB, GL_BYTE, 0);
            glGenerateMipmap(GL_TEXTURE_2D);
            expectError(GL_INVALID_OPERATION);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8I, 0, 0, 0, GL_RED_INTEGER, GL_BYTE, 0);
            glGenerateMipmap(GL_TEXTURE_2D);
            expectError(GL_INVALID_OPERATION);

            if (!(m_context.getContextInfo().isExtensionSupported("GL_EXT_color_buffer_float") &&
                  m_context.getContextInfo().isExtensionSupported("GL_OES_texture_float_linear")))
            {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, 0, 0, 0, GL_RGBA, GL_FLOAT, 0);
                glGenerateMipmap(GL_TEXTURE_2D);
                expectError(GL_INVALID_OPERATION);
            }

            m_log << TestLog::EndSection;
        }

        glDeleteTextures(2, texture);
    });

    // glGenTextures

    ES3F_ADD_API_CASE(gentextures, "Invalid glGenTextures() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if n is negative.");
        glGenTextures(-1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });

    // glPixelStorei

    ES3F_ADD_API_CASE(pixelstorei, "Invalid glPixelStorei() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if pname is not an accepted value.");
        glPixelStorei(0, 1);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if a negative row length, pixel skip, or row skip "
                                      "value is specified, or if alignment is specified as other than 1, 2, 4, or 8.");
        glPixelStorei(GL_PACK_ROW_LENGTH, -1);
        expectError(GL_INVALID_VALUE);
        glPixelStorei(GL_PACK_SKIP_ROWS, -1);
        expectError(GL_INVALID_VALUE);
        glPixelStorei(GL_PACK_SKIP_PIXELS, -1);
        expectError(GL_INVALID_VALUE);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, -1);
        expectError(GL_INVALID_VALUE);
        glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, -1);
        expectError(GL_INVALID_VALUE);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, -1);
        expectError(GL_INVALID_VALUE);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, -1);
        expectError(GL_INVALID_VALUE);
        glPixelStorei(GL_UNPACK_SKIP_IMAGES, -1);
        expectError(GL_INVALID_VALUE);
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

    ES3F_ADD_API_CASE(teximage2d, "Invalid glTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is invalid.");
        glTexImage2D(0, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if type is not a type constant.");
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, 0, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if the combination of internalFormat, format and type is invalid.");
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGB, GL_UNSIGNED_SHORT_4_4_4_4, 0);
        expectError(GL_INVALID_OPERATION);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 1, 1, 0, GL_RGB, GL_UNSIGNED_SHORT_5_5_5_1, 0);
        expectError(GL_INVALID_OPERATION);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB10_A2, 1, 1, 0, GL_RGB, GL_UNSIGNED_INT_2_10_10_10_REV, 0);
        expectError(GL_INVALID_OPERATION);
        if (glu::isContextTypeES(m_context.getRenderContext().getType()))
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
            expectError(GL_INVALID_OPERATION);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, 1, 1, 0, GL_RGBA_INTEGER, GL_INT, 0);
            expectError(GL_INVALID_OPERATION);
        }
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(teximage2d_inequal_width_height_cube, "Invalid glTexImage2D() usage", {
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
    ES3F_ADD_API_CASE(teximage2d_neg_level, "Invalid glTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if level is less than 0.");
        glTexImage2D(GL_TEXTURE_2D, -1, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

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
    ES3F_ADD_API_CASE(teximage2d_max_level, "Invalid glTexImage2D() usage", {
        m_log << TestLog::Section("",
                                  "GL_INVALID_VALUE is generated if level is greater than log_2(GL_MAX_TEXTURE_SIZE).");
        uint32_t log2MaxTextureSize = deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_TEXTURE_SIZE)) + 1;
        glTexImage2D(GL_TEXTURE_2D, log2MaxTextureSize, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if level is greater than log_2(GL_MAX_CUBE_MAP_TEXTURE_SIZE).");
        uint32_t log2MaxCubemapSize =
            deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE)) + 1;
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, log2MaxCubemapSize, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, log2MaxCubemapSize, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, log2MaxCubemapSize, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, log2MaxCubemapSize, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, log2MaxCubemapSize, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, log2MaxCubemapSize, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(teximage2d_neg_width_height, "Invalid glTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width or height is less than 0.");

        m_log << TestLog::Section("", "GL_TEXTURE_2D target");
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, -1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, -1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, -1, -1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_POSITIVE_X target");
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, -1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 1, -1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, -1, -1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_POSITIVE_Y target");
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, -1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, 1, -1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, -1, -1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_POSITIVE_Z target");
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, -1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, 1, -1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, -1, -1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_NEGATIVE_X target");
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, -1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, 1, -1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, -1, -1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_NEGATIVE_Y target");
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, -1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, 1, -1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, -1, -1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_NEGATIVE_Z target");
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, -1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, 1, -1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, -1, -1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(teximage2d_max_width_height, "Invalid glTexImage2D() usage", {
        int maxTextureSize = m_context.getContextInfo().getInt(GL_MAX_TEXTURE_SIZE) + 1;
        int maxCubemapSize = m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE) + 1;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if width or height is greater than GL_MAX_TEXTURE_SIZE.");
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, maxTextureSize, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, maxTextureSize, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, maxTextureSize, maxTextureSize, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if width or height is greater than GL_MAX_CUBE_MAP_TEXTURE_SIZE.");

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_POSITIVE_X target");
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, maxCubemapSize, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 1, maxCubemapSize, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, maxCubemapSize, maxCubemapSize, 0, GL_RGB,
                     GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_POSITIVE_Y target");
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, maxCubemapSize, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, 1, maxCubemapSize, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, maxCubemapSize, maxCubemapSize, 0, GL_RGB,
                     GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_POSITIVE_Z target");
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, maxCubemapSize, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, 1, maxCubemapSize, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, maxCubemapSize, maxCubemapSize, 0, GL_RGB,
                     GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_NEGATIVE_X target");
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, maxCubemapSize, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, 1, maxCubemapSize, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, maxCubemapSize, maxCubemapSize, 0, GL_RGB,
                     GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_NEGATIVE_Y target");
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, maxCubemapSize, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, 1, maxCubemapSize, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, maxCubemapSize, maxCubemapSize, 0, GL_RGB,
                     GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_TEXTURE_CUBE_MAP_NEGATIVE_Z target");
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, maxCubemapSize, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, 1, maxCubemapSize, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, maxCubemapSize, maxCubemapSize, 0, GL_RGB,
                     GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(teximage2d_invalid_border, "Invalid glTexImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if border is not 0.");
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, -1, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, 1, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y, 0, GL_RGB, 1, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z, 0, GL_RGB, 1, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X, 0, GL_RGB, 1, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, 0, GL_RGB, 1, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, 0, GL_RGB, 1, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(teximage2d_invalid_buffer_target, "Invalid glTexImage2D() usage", {
        uint32_t buf;
        uint32_t texture;
        std::vector<GLubyte> data(64);

        glGenBuffers(1, &buf);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buf);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, 64, &data[0], GL_DYNAMIC_COPY);
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if a non-zero buffer object name is bound to "
                                      "the GL_PIXEL_UNPACK_BUFFER target and...");
        m_log << TestLog::Section("", "...the buffer object's data store is currently mapped.");
        glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, 32, GL_MAP_WRITE_BIT);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_OPERATION);
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "...the data would be unpacked from the buffer object such that the memory reads "
                                      "required would exceed the data store size.");
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "...data is not evenly divisible into the number of bytes needed to store in "
                                      "memory a datum indicated by type.");
        m_log << TestLog::Message << "// Set byte offset = 3" << TestLog::EndMessage;
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, 4, 4, 0, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, (const GLvoid *)3);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;
        m_log << TestLog::EndSection;

        glDeleteBuffers(1, &buf);
        glDeleteTextures(1, &texture);
    });

    // glTexSubImage2D

    ES3F_ADD_API_CASE(texsubimage2d, "Invalid glTexSubImage2D() usage", {
        uint32_t texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is invalid.");
        glTexSubImage2D(0, 0, 0, 0, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if format is not an accepted format constant.");
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 4, 4, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if type is not a type constant.");
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_RGB, 0, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if the combination of internalFormat of "
                                           "the previously specified texture array, format and type is not valid.");
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_RGBA, GL_UNSIGNED_SHORT_5_6_5, 0);
        expectError(GL_INVALID_OPERATION);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_RGB, GL_UNSIGNED_SHORT_4_4_4_4, 0);
        expectError(GL_INVALID_OPERATION);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_RGB, GL_UNSIGNED_SHORT_5_5_5_1, 0);
        expectError(GL_INVALID_OPERATION);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_RGB, GL_UNSIGNED_SHORT_5_5_5_1, 0);
        expectError(GL_INVALID_OPERATION);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_RGBA_INTEGER, GL_UNSIGNED_INT, 0);
        expectError(GL_INVALID_OPERATION);
        if (glu::isContextTypeES(m_context.getRenderContext().getType()))
        {
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_RGB, GL_FLOAT, 0);
            expectError(GL_INVALID_OPERATION);
        }
        m_log << tcu::TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES3F_ADD_API_CASE(texsubimage2d_neg_level, "Invalid glTexSubImage2D() usage", {
        uint32_t textures[2];
        glGenTextures(2, &textures[0]);
        glBindTexture(GL_TEXTURE_2D, textures[0]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 32, 32, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        glBindTexture(GL_TEXTURE_2D, textures[1]);
        FOR_CUBE_FACES(faceGL, glTexImage2D(faceGL, 0, GL_RGB, 32, 32, 0, GL_RGB, GL_UNSIGNED_BYTE, 0););
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if level is less than 0.");
        glTexSubImage2D(GL_TEXTURE_2D, -1, 0, 0, 0, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if level is less than 0.");
        FOR_CUBE_FACES(faceGL, {
            glTexSubImage2D(faceGL, -1, 0, 0, 0, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
            expectError(GL_INVALID_VALUE);
        });
        m_log << TestLog::EndSection;

        glDeleteTextures(2, &textures[0]);
    });
    ES3F_ADD_API_CASE(texsubimage2d_max_level, "Invalid glTexSubImage2D() usage", {
        uint32_t textures[2];
        glGenTextures(2, &textures[0]);
        glBindTexture(GL_TEXTURE_2D, textures[0]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 32, 32, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, textures[1]);
        FOR_CUBE_FACES(faceGL, glTexImage2D(faceGL, 0, GL_RGB, 32, 32, 0, GL_RGB, GL_UNSIGNED_BYTE, 0););
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("",
                                  "GL_INVALID_VALUE is generated if level is greater than log_2(GL_MAX_TEXTURE_SIZE).");
        uint32_t log2MaxTextureSize = deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_TEXTURE_SIZE)) + 1;
        glTexSubImage2D(GL_TEXTURE_2D, log2MaxTextureSize, 0, 0, 0, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if level is greater than log_2(GL_MAX_CUBE_MAP_TEXTURE_SIZE).");
        uint32_t log2MaxCubemapSize =
            deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE)) + 1;
        FOR_CUBE_FACES(faceGL, {
            glTexSubImage2D(faceGL, log2MaxCubemapSize, 0, 0, 0, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
            expectError(GL_INVALID_VALUE);
        });
        m_log << TestLog::EndSection;

        glDeleteTextures(2, &textures[0]);
    });
    ES3F_ADD_API_CASE(texsubimage2d_neg_offset, "Invalid glTexSubImage2D() usage", {
        uint32_t texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 32, 32, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if xoffset or yoffset are negative.");
        glTexSubImage2D(GL_TEXTURE_2D, 0, -1, 0, 0, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, -1, 0, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexSubImage2D(GL_TEXTURE_2D, 0, -1, -1, 0, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES3F_ADD_API_CASE(texsubimage2d_invalid_offset, "Invalid glTexSubImage2D() usage", {
        uint32_t texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section(
            "",
            "GL_INVALID_VALUE is generated if xoffset + width > texture_width or yoffset + height > texture_height.");
        glTexSubImage2D(GL_TEXTURE_2D, 0, 30, 0, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 30, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 30, 30, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES3F_ADD_API_CASE(texsubimage2d_neg_width_height, "Invalid glTexSubImage2D() usage", {
        uint32_t texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_NO_ERROR);

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
    ES3F_ADD_API_CASE(texsubimage2d_invalid_buffer_target, "Invalid glTexSubImage2D() usage", {
        uint32_t buf;
        uint32_t texture;
        std::vector<GLubyte> data(64);

        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glGenBuffers(1, &buf);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buf);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, 64, &data[0], GL_DYNAMIC_COPY);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if a non-zero buffer object name is bound to "
                                      "the GL_PIXEL_UNPACK_BUFFER target and...");
        m_log << TestLog::Section("", "...the buffer object's data store is currently mapped.");
        glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, 32, GL_MAP_WRITE_BIT);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_OPERATION);
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "...the data would be unpacked from the buffer object such that the memory reads "
                                      "required would exceed the data store size.");
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 32, 32, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "...data is not evenly divisible into the number of bytes needed to store in "
                                      "memory a datum indicated by type.");
        m_log << TestLog::Message << "// Set byte offset = 3" << TestLog::EndMessage;
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, 0);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buf);
        expectError(GL_NO_ERROR);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, (const GLvoid *)3);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;
        m_log << TestLog::EndSection;

        glDeleteBuffers(1, &buf);
        glDeleteTextures(1, &texture);
    });

    // glTexParameteri

    ES3F_ADD_API_CASE(texparameteri, "Invalid glTexParameteri() usage", {
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

    ES3F_ADD_API_CASE(texparameterf, "Invalid glTexParameterf() usage", {
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

    ES3F_ADD_API_CASE(texparameteriv, "Invalid glTexParameteriv() usage", {
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

        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);

        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM is generated if target or pname is not one of the accepted defined values.");
        params[0] = GL_LINEAR;
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

    ES3F_ADD_API_CASE(texparameterfv, "Invalid glTexParameterfv() usage", {
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

        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);

        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM is generated if target or pname is not one of the accepted defined values.");
        params[0] = GL_LINEAR;
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

    ES3F_ADD_API_CASE(compressedtexsubimage2d, "Invalid glCompressedTexSubImage2D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is invalid.");
        glCompressedTexSubImage2D(0, 0, 0, 0, 0, 0, GL_COMPRESSED_RGB8_ETC2, 0, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        uint32_t texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 18, 18, 0, etc2EacDataSize(18, 18), 0);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if format does not match the internal format "
                                      "of the texture image being modified.");
        glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, GL_COMPRESSED_RGB8_ETC2, 0, 0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("",
                                  "For ETC2/EAC images GL_INVALID_OPERATION is generated if width is not a multiple of "
                                  "four, and width + xoffset is not equal to the width of the texture level.");
        glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 4, 0, 10, 4, GL_COMPRESSED_RGBA8_ETC2_EAC, etc2EacDataSize(10, 4),
                                  0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("",
                                  "For ETC2/EAC images GL_INVALID_OPERATION is generated if height is not a multiple "
                                  "of four, and height + yoffset is not equal to the height of the texture level.");
        glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 4, 4, 10, GL_COMPRESSED_RGBA8_ETC2_EAC, etc2EacDataSize(4, 10),
                                  0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "",
            "For ETC2/EAC images GL_INVALID_OPERATION is generated if xoffset or yoffset is not a multiple of four.");
        glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 1, 4, 4, GL_COMPRESSED_RGBA8_ETC2_EAC, etc2EacDataSize(4, 4), 0);
        expectError(GL_INVALID_OPERATION);
        glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 1, 0, 4, 4, GL_COMPRESSED_RGBA8_ETC2_EAC, etc2EacDataSize(4, 4), 0);
        expectError(GL_INVALID_OPERATION);
        glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 1, 1, 4, 4, GL_COMPRESSED_RGBA8_ETC2_EAC, etc2EacDataSize(4, 4), 0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES3F_ADD_API_CASE(compressedtexsubimage2d_neg_level, "Invalid glCompressedTexSubImage2D() usage", {
        uint32_t textures[2];
        glGenTextures(2, &textures[0]);
        glBindTexture(GL_TEXTURE_2D, textures[0]);
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 18, 18, 0, etc2EacDataSize(18, 18), 0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, textures[1]);
        FOR_CUBE_FACES(faceGL, glCompressedTexImage2D(faceGL, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 18, 18, 0,
                                                      etc2EacDataSize(18, 18), 0););
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if level is less than 0.");
        glCompressedTexSubImage2D(GL_TEXTURE_2D, -1, 0, 0, 0, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if level is less than 0.");
        FOR_CUBE_FACES(faceGL, {
            glCompressedTexSubImage2D(faceGL, -1, 0, 0, 0, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0);
            expectError(GL_INVALID_VALUE);
        });
        m_log << TestLog::EndSection;

        glDeleteTextures(2, &textures[0]);
    });
    ES3F_ADD_API_CASE(compressedtexsubimage2d_max_level, "Invalid glCompressedTexSubImage2D() usage", {
        uint32_t textures[2];
        glGenTextures(2, &textures[0]);
        glBindTexture(GL_TEXTURE_2D, textures[0]);
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 18, 18, 0, etc2EacDataSize(18, 18), 0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, textures[1]);
        FOR_CUBE_FACES(faceGL, glCompressedTexImage2D(faceGL, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 18, 18, 0,
                                                      etc2EacDataSize(18, 18), 0););
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("",
                                  "GL_INVALID_VALUE is generated if level is greater than log_2(GL_MAX_TEXTURE_SIZE).");
        uint32_t log2MaxTextureSize = deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_TEXTURE_SIZE)) + 1;
        glCompressedTexSubImage2D(GL_TEXTURE_2D, log2MaxTextureSize, 0, 0, 0, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if level is greater than log_2(GL_MAX_CUBE_MAP_TEXTURE_SIZE).");
        uint32_t log2MaxCubemapSize =
            deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_CUBE_MAP_TEXTURE_SIZE)) + 1;
        FOR_CUBE_FACES(faceGL, {
            glCompressedTexSubImage2D(faceGL, log2MaxCubemapSize, 0, 0, 0, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0);
            expectError(GL_INVALID_VALUE);
        });
        m_log << TestLog::EndSection;

        glDeleteTextures(2, &textures[0]);
    });
    ES3F_ADD_API_CASE(compressedtexsubimage2d_neg_offset, "Invalid glCompressedTexSubImage2D() usage", {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 8, 8, 0, etc2EacDataSize(8, 8), 0);

        // \note Both GL_INVALID_VALUE and GL_INVALID_OPERATION are valid here since implementation may
        //         first check if offsets are valid for certain format and only after that check that they
        //         are not negative.
        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE or GL_INVALID_OPERATION is generated if xoffset or yoffset are negative.");

        glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, -4, 0, 0, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0);
        expectError(GL_INVALID_VALUE, GL_INVALID_OPERATION);
        glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, -4, 0, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0);
        expectError(GL_INVALID_VALUE, GL_INVALID_OPERATION);
        glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, -4, -4, 0, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0);
        expectError(GL_INVALID_VALUE, GL_INVALID_OPERATION);

        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES3F_ADD_API_CASE(compressedtexsubimage2d_invalid_offset, "Invalid glCompressedTexSubImage2D() usage", {
        uint32_t texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 16, 16, 0, etc2EacDataSize(16, 16), 0);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_VALUE or GL_INVALID_OPERATION is generated if xoffset + width > "
                                      "texture_width or yoffset + height > texture_height.");

        glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 12, 0, 8, 4, GL_COMPRESSED_RGBA8_ETC2_EAC, etc2EacDataSize(8, 4),
                                  0);
        expectError(GL_INVALID_VALUE, GL_INVALID_OPERATION);
        glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 12, 4, 8, GL_COMPRESSED_RGBA8_ETC2_EAC, etc2EacDataSize(4, 8),
                                  0);
        expectError(GL_INVALID_VALUE, GL_INVALID_OPERATION);
        glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 12, 12, 8, 8, GL_COMPRESSED_RGBA8_ETC2_EAC, etc2EacDataSize(8, 8),
                                  0);
        expectError(GL_INVALID_VALUE, GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES3F_ADD_API_CASE(compressedtexsubimage2d_neg_width_height, "Invalid glCompressedTexSubImage2D() usage", {
        uint32_t texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 16, 16, 0, etc2EacDataSize(16, 16), 0);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE or GL_INVALID_OPERATION is generated if width or height is less than 0.");
        glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, -4, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0);
        expectError(GL_INVALID_VALUE, GL_INVALID_OPERATION);
        glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, -4, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0);
        expectError(GL_INVALID_VALUE, GL_INVALID_OPERATION);
        glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, -4, -4, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0);
        expectError(GL_INVALID_VALUE, GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES3F_ADD_API_CASE(compressedtexsubimage2d_invalid_size, "Invalid glCompressedTexImage2D() usage", {
        uint32_t texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 16, 16, 0, etc2EacDataSize(16, 16), 0);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if imageSize is not consistent with the format, "
                                      "dimensions, and contents of the specified compressed image data.");
        glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, -1, 0);
        expectError(GL_INVALID_VALUE);

        glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 16, 16, GL_COMPRESSED_RGBA8_ETC2_EAC, 4 * 4 * 16 - 1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES3F_ADD_API_CASE(compressedtexsubimage2d_invalid_buffer_target, "Invalid glCompressedTexSubImage2D() usage", {
        uint32_t buf;
        uint32_t texture;
        std::vector<GLubyte> data(128);

        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 16, 16, 0, etc2EacDataSize(16, 16), 0);
        glGenBuffers(1, &buf);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buf);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, 128, &data[0], GL_DYNAMIC_COPY);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if a non-zero buffer object name is bound to "
                                      "the GL_PIXEL_UNPACK_BUFFER target and...");
        m_log << TestLog::Section("", "...the buffer object's data store is currently mapped.");
        glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, 128, GL_MAP_WRITE_BIT);
        glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, 4, GL_COMPRESSED_RGBA8_ETC2_EAC, etc2EacDataSize(4, 4), 0);
        expectError(GL_INVALID_OPERATION);
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "...the data would be unpacked from the buffer object such that the memory reads "
                                      "required would exceed the data store size.");
        glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 16, 16, GL_COMPRESSED_RGBA8_ETC2_EAC, etc2EacDataSize(16, 16),
                                  0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;
        m_log << TestLog::EndSection;

        glDeleteBuffers(1, &buf);
        glDeleteTextures(1, &texture);
    });

    // glTexImage3D

    ES3F_ADD_API_CASE(teximage3d, "Invalid glTexImage3D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is invalid.");
        glTexImage3D(0, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_ENUM);
        glTexImage3D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if type is not a type constant.");
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGBA, 0, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if format is not an accepted format constant.");
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 1, 1, 1, 0, 0, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if internalFormat is not one of the accepted resolution and format "
                "symbolic constants "
                "or GL_INVALID_OPERATION is generated if internalformat, format and type are not compatible.");
        glTexImage3D(GL_TEXTURE_3D, 0, 0, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE, GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if target is GL_TEXTURE_3D and format is "
                                      "GL_DEPTH_COMPONENT, or GL_DEPTH_STENCIL.");
        glTexImage3D(GL_TEXTURE_3D, 0, GL_DEPTH_STENCIL, 1, 1, 1, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, 0);
        expectError(GL_INVALID_OPERATION);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_DEPTH_COMPONENT, 1, 1, 1, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if the combination of internalFormat, format and type is invalid.");
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 1, 1, 1, 0, GL_RGB, GL_UNSIGNED_SHORT_4_4_4_4, 0);
        expectError(GL_INVALID_OPERATION);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB5_A1, 1, 1, 1, 0, GL_RGB, GL_UNSIGNED_SHORT_5_5_5_1, 0);
        expectError(GL_INVALID_OPERATION);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB10_A2, 1, 1, 1, 0, GL_RGB, GL_UNSIGNED_INT_2_10_10_10_REV, 0);
        expectError(GL_INVALID_OPERATION);
        if (glu::isContextTypeES(m_context.getRenderContext().getType()))
        {
            glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB, 1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
            expectError(GL_INVALID_OPERATION);
            glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32UI, 1, 1, 1, 0, GL_RGBA_INTEGER, GL_INT, 0);
            expectError(GL_INVALID_OPERATION);
        }
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(teximage3d_neg_level, "Invalid glTexImage3D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if level is less than 0.");
        glTexImage3D(GL_TEXTURE_3D, -1, GL_RGB, 1, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, -1, GL_RGB, 1, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(teximage3d_max_level, "Invalid glTexImage3D() usage", {
        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if level is greater than log_2(GL_MAX_3D_TEXTURE_SIZE).");
        uint32_t log2Max3DTextureSize = deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_3D_TEXTURE_SIZE)) + 1;
        glTexImage3D(GL_TEXTURE_3D, log2Max3DTextureSize, GL_RGB, 1, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("",
                                  "GL_INVALID_VALUE is generated if level is greater than log_2(GL_MAX_TEXTURE_SIZE).");
        uint32_t log2MaxTextureSize = deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_TEXTURE_SIZE)) + 1;
        glTexImage3D(GL_TEXTURE_2D_ARRAY, log2MaxTextureSize, GL_RGB, 1, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(teximage3d_neg_width_height_depth, "Invalid glTexImage3D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width or height is less than 0.");
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, -1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 1, -1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 1, 1, -1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, -1, -1, -1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);

        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, -1, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, 1, -1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, 1, 1, -1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, -1, -1, -1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(teximage3d_max_width_height_depth, "Invalid glTexImage3D() usage", {
        int max3DTextureSize = m_context.getContextInfo().getInt(GL_MAX_3D_TEXTURE_SIZE) + 1;
        int maxTextureSize   = m_context.getContextInfo().getInt(GL_MAX_TEXTURE_SIZE) + 1;
        int maxTextureLayers = m_context.getContextInfo().getInt(GL_MAX_ARRAY_TEXTURE_LAYERS) + 1;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if width, height or depth is greater than GL_MAX_3D_TEXTURE_SIZE.");
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, max3DTextureSize, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 1, max3DTextureSize, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 1, 1, max3DTextureSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, max3DTextureSize, max3DTextureSize, max3DTextureSize, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if width, height or depth is greater than GL_MAX_TEXTURE_SIZE.");
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, maxTextureSize, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, 1, maxTextureSize, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, 1, 1, maxTextureLayers, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, maxTextureSize, maxTextureSize, maxTextureLayers, 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(teximage3d_invalid_border, "Invalid glTexImage3D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if border is not 0 or 1.");
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB, 1, 1, 1, -1, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB, 1, 1, 1, 2, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB, 1, 1, 1, -1, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB, 1, 1, 1, 2, GL_RGB, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(teximage3d_invalid_buffer_target, "Invalid glTexImage3D() usage", {
        uint32_t buf;
        uint32_t texture;
        std::vector<GLubyte> data(512);

        glGenBuffers(1, &buf);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buf);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, 512, &data[0], GL_DYNAMIC_COPY);
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_3D, texture);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if a non-zero buffer object name is bound to "
                                      "the GL_PIXEL_UNPACK_BUFFER target and...");

        m_log << TestLog::Section("", "...the buffer object's data store is currently mapped.");
        glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, 128, GL_MAP_WRITE_BIT);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_OPERATION);
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "...the data would be unpacked from the buffer object such that the memory reads "
                                      "required would exceed the data store size.");
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 64, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "...data is not evenly divisible into the number of bytes needed to store in "
                                      "memory a datum indicated by type.");
        m_log << TestLog::Message << "// Set byte offset = 3" << TestLog::EndMessage;
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB5_A1, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, (const GLvoid *)3);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::EndSection;

        glDeleteBuffers(1, &buf);
        glDeleteTextures(1, &texture);
    });

    // glTexSubImage3D

    ES3F_ADD_API_CASE(texsubimage3d, "Invalid glTexSubImage3D() usage", {
        uint32_t texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_3D, texture);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is invalid.");
        glTexSubImage3D(0, 0, 0, 0, 0, 4, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_ENUM);
        glTexSubImage3D(GL_TEXTURE_2D, 0, 0, 0, 0, 4, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if format is not an accepted format constant.");
        glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 0, 4, 4, 4, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if type is not a type constant.");
        glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 4, 4, 4, GL_RGB, 0, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << tcu::TestLog::Section("", "GL_INVALID_OPERATION is generated if the combination of internalFormat of "
                                           "the previously specified texture array, format and type is not valid.");
        glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 4, 4, 4, GL_RGB, GL_UNSIGNED_SHORT_4_4_4_4, 0);
        expectError(GL_INVALID_OPERATION);
        glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 4, 4, 4, GL_RGB, GL_UNSIGNED_SHORT_5_5_5_1, 0);
        expectError(GL_INVALID_OPERATION);
        glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 4, 4, 4, GL_RGB, GL_UNSIGNED_SHORT_5_5_5_1, 0);
        expectError(GL_INVALID_OPERATION);
        glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 4, 4, 4, GL_RGBA_INTEGER, GL_UNSIGNED_INT, 0);
        expectError(GL_INVALID_OPERATION);
        if (glu::isContextTypeES(m_context.getRenderContext().getType()))
        {
            glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 4, 4, 4, GL_RGB, GL_FLOAT, 0);
            expectError(GL_INVALID_OPERATION);
        }
        m_log << tcu::TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES3F_ADD_API_CASE(texsubimage3d_neg_level, "Invalid glTexSubImage3D() usage", {
        uint32_t textures[2];
        glGenTextures(2, &textures[0]);
        glBindTexture(GL_TEXTURE_3D, textures[0]);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, textures[1]);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if level is less than 0.");
        glTexSubImage3D(GL_TEXTURE_3D, -1, 0, 0, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, -1, 0, 0, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(2, &textures[0]);
    });
    ES3F_ADD_API_CASE(texsubimage3d_max_level, "Invalid glTexSubImage3D() usage", {
        uint32_t textures[2];
        glGenTextures(2, &textures[0]);
        glBindTexture(GL_TEXTURE_3D, textures[0]);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, textures[1]);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_NO_ERROR);

        uint32_t log2Max3DTextureSize = deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_3D_TEXTURE_SIZE)) + 1;
        uint32_t log2MaxTextureSize   = deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_TEXTURE_SIZE)) + 1;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if level is greater than log_2(GL_MAX_3D_TEXTURE_SIZE).");
        glTexSubImage3D(GL_TEXTURE_3D, log2Max3DTextureSize, 0, 0, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("",
                                  "GL_INVALID_VALUE is generated if level is greater than log_2(GL_MAX_TEXTURE_SIZE).");
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, log2MaxTextureSize, 0, 0, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(2, &textures[0]);
    });
    ES3F_ADD_API_CASE(texsubimage3d_neg_offset, "Invalid glTexSubImage3D() usage", {
        uint32_t textures[2];
        glGenTextures(2, &textures[0]);
        glBindTexture(GL_TEXTURE_3D, textures[0]);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, textures[1]);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if xoffset, yoffset or zoffset are negative.");
        glTexSubImage3D(GL_TEXTURE_3D, 0, -1, 0, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexSubImage3D(GL_TEXTURE_3D, 0, 0, -1, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, -1, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexSubImage3D(GL_TEXTURE_3D, 0, -1, -1, -1, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, -1, 0, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, -1, 0, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, -1, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, -1, -1, -1, 0, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(2, &textures[0]);
    });
    ES3F_ADD_API_CASE(texsubimage3d_invalid_offset, "Invalid glTexSubImage3D() usage", {
        uint32_t texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_3D, texture);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if xoffset + width > texture_width.");
        glTexSubImage3D(GL_TEXTURE_3D, 0, 2, 0, 0, 4, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if yoffset + height > texture_height.");
        glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 2, 0, 4, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if zoffset + depth > texture_depth.");
        glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 2, 4, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES3F_ADD_API_CASE(texsubimage3d_neg_width_height, "Invalid glTexSubImage3D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width, height or depth is less than 0.");
        glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, -1, 0, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 0, -1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 0, 0, -1, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, -1, -1, -1, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(texsubimage3d_invalid_buffer_target, "Invalid glTexSubImage3D() usage", {
        uint32_t buf;
        uint32_t texture;
        std::vector<GLubyte> data(512);

        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_3D, texture);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 16, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glGenBuffers(1, &buf);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buf);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, 512, &data[0], GL_DYNAMIC_COPY);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if a non-zero buffer object name is bound to "
                                      "the GL_PIXEL_UNPACK_BUFFER target and...");

        m_log << TestLog::Section("", "...the buffer object's data store is currently mapped.");
        glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, 512, GL_MAP_WRITE_BIT);
        glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 4, 4, 4, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_OPERATION);
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "...the data would be unpacked from the buffer object such that the memory reads "
                                      "required would exceed the data store size.");
        glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 16, 16, 16, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "...data is not evenly divisible into the number of bytes needed to store in "
                                      "memory a datum indicated by type.");
        m_log << TestLog::Message << "// Set byte offset = 3" << TestLog::EndMessage;
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA4, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, 0);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buf);
        expectError(GL_NO_ERROR);
        glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 4, 4, 4, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, (const GLvoid *)3);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::EndSection;

        glDeleteBuffers(1, &buf);
        glDeleteTextures(1, &texture);
    });

    // glCopyTexSubImage3D

    ES3F_ADD_API_CASE(copytexsubimage3d, "Invalid glCopyTexSubImage3D() usage", {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_3D, texture);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is invalid.");
        glCopyTexSubImage3D(0, 0, 0, 0, 0, 0, 0, 4, 4);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES3F_ADD_API_CASE(copytexsubimage3d_neg_level, "Invalid glCopyTexSubImage3D() usage", {
        uint32_t textures[2];
        glGenTextures(2, &textures[0]);
        glBindTexture(GL_TEXTURE_3D, textures[0]);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, textures[1]);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if level is less than 0.");
        glCopyTexSubImage3D(GL_TEXTURE_3D, -1, 0, 0, 0, 0, 0, 4, 4);
        expectError(GL_INVALID_VALUE);
        glCopyTexSubImage3D(GL_TEXTURE_2D_ARRAY, -1, 0, 0, 0, 0, 0, 4, 4);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(2, &textures[0]);
    });
    ES3F_ADD_API_CASE(copytexsubimage3d_max_level, "Invalid glCopyTexSubImage3D() usage", {
        uint32_t log2Max3DTextureSize = deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_3D_TEXTURE_SIZE)) + 1;
        uint32_t log2MaxTextureSize   = deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_TEXTURE_SIZE)) + 1;

        uint32_t textures[2];
        glGenTextures(2, &textures[0]);
        glBindTexture(GL_TEXTURE_3D, textures[0]);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, textures[1]);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if level is greater than log_2(GL_MAX_3D_TEXTURE_SIZE).");
        glCopyTexSubImage3D(GL_TEXTURE_3D, log2Max3DTextureSize, 0, 0, 0, 0, 0, 4, 4);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("",
                                  "GL_INVALID_VALUE is generated if level is greater than log_2(GL_MAX_TEXTURE_SIZE).");
        glCopyTexSubImage3D(GL_TEXTURE_2D_ARRAY, log2MaxTextureSize, 0, 0, 0, 0, 0, 4, 4);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(2, &textures[0]);
    });
    ES3F_ADD_API_CASE(copytexsubimage3d_neg_offset, "Invalid glCopyTexSubImage3D() usage", {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_3D, texture);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if xoffset, yoffset or zoffset is negative.");
        glCopyTexSubImage3D(GL_TEXTURE_3D, 0, -1, 0, 0, 0, 0, 4, 4);
        expectError(GL_INVALID_VALUE);
        glCopyTexSubImage3D(GL_TEXTURE_3D, 0, 0, -1, 0, 0, 0, 4, 4);
        expectError(GL_INVALID_VALUE);
        glCopyTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, -1, 0, 0, 4, 4);
        expectError(GL_INVALID_VALUE);
        glCopyTexSubImage3D(GL_TEXTURE_3D, 0, -1, -1, -1, 0, 0, 4, 4);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES3F_ADD_API_CASE(copytexsubimage3d_invalid_offset, "Invalid glCopyTexSubImage3D() usage", {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_3D, texture);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if xoffset + width > texture_width.");
        glCopyTexSubImage3D(GL_TEXTURE_3D, 0, 1, 0, 0, 0, 0, 4, 4);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if yoffset + height > texture_height.");
        glCopyTexSubImage3D(GL_TEXTURE_3D, 0, 0, 1, 0, 0, 0, 4, 4);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if zoffset + 1 > texture_depth.");
        glCopyTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 4, 0, 0, 4, 4);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES3F_ADD_API_CASE(copytexsubimage3d_neg_width_height, "Invalid glCopyTexSubImage3D() usage", {
        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_3D, texture);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width < 0.");
        glCopyTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 0, 0, -4, 4);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if height < 0.");
        glCopyTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 0, 0, 4, -4);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES3F_ADD_API_CASE(copytexsubimage3d_incomplete_framebuffer, "Invalid glCopyTexSubImage3D() usage", {
        GLuint fbo;
        GLuint texture[2];

        glGenTextures(2, texture);
        glBindTexture(GL_TEXTURE_3D, texture[0]);
        glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, texture[1]);
        glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, 4, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
        glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);

        m_log << tcu::TestLog::Section("", "GL_INVALID_FRAMEBUFFER_OPERATION is generated if the currently bound "
                                           "framebuffer is not framebuffer complete.");
        glCopyTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 0, 0, 4, 4);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        glCopyTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 0, 0, 4, 4);
        expectError(GL_INVALID_FRAMEBUFFER_OPERATION);
        m_log << tcu::TestLog::EndSection;

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &fbo);
        glDeleteTextures(2, texture);
    });

    // glCompressedTexImage3D

    ES3F_ADD_API_CASE(compressedteximage3d, "Invalid glCompressedTexImage3D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is invalid.");
        glCompressedTexImage3D(0, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_ENUM);
        glCompressedTexImage3D(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "",
            "GL_INVALID_ENUM is generated if internalformat is not one of the specific compressed internal formats.");
        glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_ENUM);
        glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(compressedteximage3d_neg_level, "Invalid glCompressedTexImage3D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if level is less than 0.");
        glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, -1, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(compressedteximage3d_max_level, "Invalid glCompressedTexImage3D() usage", {
        m_log << TestLog::Section("",
                                  "GL_INVALID_VALUE is generated if level is greater than log_2(GL_MAX_TEXTURE_SIZE).");
        uint32_t log2MaxTextureSize = deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_TEXTURE_SIZE)) + 1;
        glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, log2MaxTextureSize, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(compressedteximage3d_neg_width_height_depth, "Invalid glCompressedTexImage3D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width, height or depth is less than 0.");
        glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, -1, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, -1, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, -1, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, -1, -1, -1, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(compressedteximage3d_max_width_height_depth, "Invalid glCompressedTexImage3D() usage", {
        int maxTextureSize   = m_context.getContextInfo().getInt(GL_MAX_TEXTURE_SIZE) + 1;
        int maxTextureLayers = m_context.getContextInfo().getInt(GL_MAX_ARRAY_TEXTURE_LAYERS) + 1;

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE is generated if width, height or depth is greater than GL_MAX_TEXTURE_SIZE.");
        glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, maxTextureSize, 0, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, maxTextureSize, 0, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, maxTextureLayers, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, maxTextureSize, maxTextureSize,
                               maxTextureLayers, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(compressedteximage3d_invalid_border, "Invalid glCompressedTexImage3D() usage", {
        bool isES    = glu::isContextTypeES(m_context.getRenderContext().getType());
        GLenum error = isES ? GL_INVALID_VALUE : GL_INVALID_OPERATION;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if border is not 0.");
        glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, 0, -1, 0, 0);
        expectError(error);
        glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, 0, 1, 0, 0);
        expectError(error);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(compressedteximage3d_invalid_size, "Invalid glCompressedTexImage3D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if imageSize is not consistent with the format, "
                                      "dimensions, and contents of the specified compressed image data.");
        glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0, 0, 0, -1, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 16, 16, 1, 0, 4 * 4 * 8, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGB8_ETC2, 16, 16, 1, 0, 4 * 4 * 16, 0);
        expectError(GL_INVALID_VALUE);
        glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_SIGNED_R11_EAC, 16, 16, 1, 0, 4 * 4 * 16, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;
    });
    ES3F_ADD_API_CASE(compressedteximage3d_invalid_buffer_target, "Invalid glCompressedTexImage3D() usage", {
        uint32_t buf;
        std::vector<GLubyte> data(512);

        glGenBuffers(1, &buf);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buf);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, 64, &data[0], GL_DYNAMIC_COPY);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if a non-zero buffer object name is bound to the "
                "GL_PIXEL_UNPACK_BUFFER target and the buffer object's data store is currently mapped.");
        glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, 64, GL_MAP_WRITE_BIT);
        glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGB8_ETC2, 4, 4, 1, 0, etc2DataSize(4, 4), 0);
        expectError(GL_INVALID_OPERATION);
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("",
                                  "GL_INVALID_OPERATION is generated if a non-zero buffer object name is bound to the "
                                  "GL_PIXEL_UNPACK_BUFFER target and the data would be unpacked from the buffer object "
                                  "such that the memory reads required would exceed the data store size.");
        glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGB8_ETC2, 16, 16, 1, 0, etc2DataSize(16, 16), 0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteBuffers(1, &buf);
    });
    ES3F_ADD_API_CASE(compressedteximage3d_invalid_astc_target, "Invalid glCompressedTexImage3D() ASTC 3D targets", {
        // GLES 3.0.4, Sec 3.8.6, p.147: For example, the
        // compressed image format might be supported only for 2D
        // textures ... result in an INVALID_OPERATION error.
        // Also, if LDR is supported, formats cannot be invalid enums

        if (m_context.getContextInfo().isExtensionSupported("GL_KHR_texture_compression_astc_sliced_3d") ||
            m_context.getContextInfo().isExtensionSupported("GL_KHR_texture_compression_astc_hdr") ||
            m_context.getContextInfo().isExtensionSupported("GL_OES_texture_compression_astc"))
        {
            m_log.writeMessage("Full ASTC supported. No negative API requirements.");
        }
        else
        {
            const GLuint requiredError =
                m_context.getContextInfo().isExtensionSupported("GL_KHR_texture_compression_astc_ldr") ?
                    GL_INVALID_OPERATION :
                    GL_INVALID_ENUM;

            if (requiredError == GL_INVALID_OPERATION)
                m_log.writeMessage("GL_INVALID_OPERATION should be generated if using TEXTURE_3D with LDR ASTC.");
            else
                m_log.writeMessage("GL_INVALID_ENUM should be generated if no ASTC extensions are present.");

            for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(s_astcFormats); formatNdx++)
            {
                const GLuint format                 = s_astcFormats[formatNdx];
                const CompressedTexFormat tcuFormat = mapGLCompressedTexFormat(format);
                const IVec3 blockPixels             = getBlockPixelSize(tcuFormat);
                const size_t blockBytes             = getBlockSize(tcuFormat);
                const vector<uint8_t> unusedData(blockBytes);

                glCompressedTexImage3D(GL_TEXTURE_3D, 0, format, blockPixels.x(), blockPixels.y(), blockPixels.z(), 0,
                                       (int)blockBytes, &unusedData[0]);
                expectError(requiredError);
            }
        }
    });

    // glCompressedTexSubImage3D

    ES3F_ADD_API_CASE(compressedtexsubimage3d, "Invalid glCompressedTexSubImage3D() usage", {
        m_log << TestLog::Section("", "GL_INVALID_ENUM is generated if target is invalid.");
        glCompressedTexSubImage3D(0, 0, 0, 0, 0, 0, 0, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        uint32_t texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
        glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 18, 18, 1, 0,
                               etc2EacDataSize(18, 18), 0);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if format does not match the internal format "
                                      "of the texture image being modified.");
        glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 0, 0, 0, GL_COMPRESSED_RGB8_ETC2, 0, 0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if internalformat is an ETC2/EAC format and "
                                      "target is not GL_TEXTURE_2D_ARRAY.");
        glCompressedTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, 18, 18, 1, GL_COMPRESSED_RGBA8_ETC2_EAC,
                                  etc2EacDataSize(18, 18), 0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("",
                                  "For ETC2/EAC images GL_INVALID_OPERATION is generated if width is not a multiple of "
                                  "four, and width + xoffset is not equal to the width of the texture level.");
        glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 4, 0, 0, 10, 4, 1, GL_COMPRESSED_RGBA8_ETC2_EAC,
                                  etc2EacDataSize(10, 4), 0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("",
                                  "For ETC2/EAC images GL_INVALID_OPERATION is generated if height is not a multiple "
                                  "of four, and height + yoffset is not equal to the height of the texture level.");
        glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 4, 0, 4, 10, 1, GL_COMPRESSED_RGBA8_ETC2_EAC,
                                  etc2EacDataSize(4, 10), 0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "",
            "For ETC2/EAC images GL_INVALID_OPERATION is generated if xoffset or yoffset is not a multiple of four.");
        glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 1, 0, 0, 4, 4, 1, GL_COMPRESSED_RGBA8_ETC2_EAC,
                                  etc2EacDataSize(4, 4), 0);
        expectError(GL_INVALID_OPERATION);
        glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 1, 0, 4, 4, 1, GL_COMPRESSED_RGBA8_ETC2_EAC,
                                  etc2EacDataSize(4, 4), 0);
        expectError(GL_INVALID_OPERATION);
        glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 1, 1, 0, 4, 4, 1, GL_COMPRESSED_RGBA8_ETC2_EAC,
                                  etc2EacDataSize(4, 4), 0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES3F_ADD_API_CASE(compressedtexsubimage3d_neg_level, "Invalid glCompressedTexSubImage3D() usage", {
        uint32_t texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
        glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 16, 16, 1, 0,
                               etc2EacDataSize(16, 16), 0);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if level is less than 0.");
        glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, -1, 0, 0, 0, 0, 0, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES3F_ADD_API_CASE(compressedtexsubimage3d_max_level, "Invalid glCompressedTexSubImage3D() usage", {
        uint32_t texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
        glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 16, 16, 1, 0,
                               etc2EacDataSize(16, 16), 0);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("",
                                  "GL_INVALID_VALUE is generated if level is greater than log_2(GL_MAX_TEXTURE_SIZE).");
        uint32_t log2MaxTextureSize = deLog2Floor32(m_context.getContextInfo().getInt(GL_MAX_TEXTURE_SIZE)) + 1;
        glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, log2MaxTextureSize, 0, 0, 0, 0, 0, 0,
                                  GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES3F_ADD_API_CASE(compressedtexsubimage3d_neg_offset, "Invalid glCompressedTexSubImage3D() usage", {
        uint32_t texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
        glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 16, 16, 1, 0,
                               etc2EacDataSize(16, 16), 0);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE or GL_INVALID_OPERATION is generated if xoffset, yoffset or zoffset are negative.");
        glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, -4, 0, 0, 0, 0, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0);
        expectError(GL_INVALID_VALUE, GL_INVALID_OPERATION);
        glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, -4, 0, 0, 0, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0);
        expectError(GL_INVALID_VALUE, GL_INVALID_OPERATION);
        glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, -4, 0, 0, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0);
        expectError(GL_INVALID_VALUE, GL_INVALID_OPERATION);
        glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, -4, -4, -4, 0, 0, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0);
        expectError(GL_INVALID_VALUE, GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES3F_ADD_API_CASE(compressedtexsubimage3d_invalid_offset, "Invalid glCompressedTexSubImage3D() usage", {
        uint32_t texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
        glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 4, 4, 1, 0, etc2EacDataSize(4, 4),
                               0);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_VALUE or GL_INVALID_OPERATION is generated if xoffset + width > "
                                      "texture_width or yoffset + height > texture_height.");

        glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 12, 0, 0, 8, 4, 1, GL_COMPRESSED_RGBA8_ETC2_EAC,
                                  etc2EacDataSize(8, 4), 0);
        expectError(GL_INVALID_VALUE, GL_INVALID_OPERATION);
        glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 12, 0, 4, 8, 1, GL_COMPRESSED_RGBA8_ETC2_EAC,
                                  etc2EacDataSize(4, 8), 0);
        expectError(GL_INVALID_VALUE, GL_INVALID_OPERATION);
        glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 12, 4, 4, 1, GL_COMPRESSED_RGBA8_ETC2_EAC,
                                  etc2EacDataSize(4, 4), 0);
        expectError(GL_INVALID_VALUE, GL_INVALID_OPERATION);
        glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 12, 12, 12, 8, 8, 1, GL_COMPRESSED_RGBA8_ETC2_EAC,
                                  etc2EacDataSize(8, 8), 0);
        expectError(GL_INVALID_VALUE, GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES3F_ADD_API_CASE(compressedtexsubimage3d_neg_width_height_depth, "Invalid glCompressedTexSubImage3D() usage", {
        uint32_t texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
        glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 16, 16, 1, 0,
                               etc2EacDataSize(16, 16), 0);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section(
            "", "GL_INVALID_VALUE or GL_INVALID_OPERATION is generated if width, height or depth are negative.");
        glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, -4, 0, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0);
        expectError(GL_INVALID_VALUE, GL_INVALID_OPERATION);
        glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 0, -4, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0);
        expectError(GL_INVALID_VALUE, GL_INVALID_OPERATION);
        glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 0, 0, -4, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0);
        expectError(GL_INVALID_VALUE, GL_INVALID_OPERATION);
        glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, -4, -4, -4, GL_COMPRESSED_RGBA8_ETC2_EAC, 0, 0);
        expectError(GL_INVALID_VALUE, GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES3F_ADD_API_CASE(compressedtexsubimage3d_invalid_size, "Invalid glCompressedTexSubImage3D() usage", {
        uint32_t texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
        glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 16, 16, 1, 0, 4 * 4 * 16, 0);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if imageSize is not consistent with the format, "
                                      "dimensions, and contents of the specified compressed image data.");
        glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 16, 16, 1, GL_COMPRESSED_RGBA8_ETC2_EAC, -1, 0);
        expectError(GL_INVALID_VALUE);

        glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 16, 16, 1, GL_COMPRESSED_RGBA8_ETC2_EAC,
                                  4 * 4 * 16 - 1, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES3F_ADD_API_CASE(compressedtexsubimage3d_invalid_buffer_target, "Invalid glCompressedTexSubImage3D() usage", {
        uint32_t buf;
        uint32_t texture;
        GLsizei bufferSize = etc2EacDataSize(4, 4);
        std::vector<GLubyte> data(bufferSize);

        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
        glCompressedTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_COMPRESSED_RGBA8_ETC2_EAC, 16, 16, 1, 0,
                               etc2EacDataSize(16, 16), 0);
        glGenBuffers(1, &buf);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buf);
        glBufferData(GL_PIXEL_UNPACK_BUFFER, bufferSize, &data[0], GL_DYNAMIC_COPY);
        expectError(GL_NO_ERROR);

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if a non-zero buffer object name is bound to "
                                      "the GL_PIXEL_UNPACK_BUFFER target and...");
        m_log << TestLog::Section("", "...the buffer object's data store is currently mapped.");
        glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, bufferSize, GL_MAP_WRITE_BIT);
        glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 4, 4, 1, GL_COMPRESSED_RGBA8_ETC2_EAC,
                                  etc2EacDataSize(4, 4), 0);
        expectError(GL_INVALID_OPERATION);
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "...the data would be unpacked from the buffer object such that the memory reads "
                                      "required would exceed the data store size.");
        glCompressedTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, 16, 16, 1, GL_COMPRESSED_RGBA8_ETC2_EAC,
                                  etc2EacDataSize(16, 16), 0);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;
        m_log << TestLog::EndSection;

        glDeleteBuffers(1, &buf);
        glDeleteTextures(1, &texture);
    });

    // glTexStorage2D

    ES3F_ADD_API_CASE(texstorage2d, "Invalid glTexStorage2D() usage", {
        uint32_t texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);

        m_log << TestLog::Section(
            "",
            "GL_INVALID_ENUM or GL_INVALID_VALUE is generated if internalformat is not a valid sized internal format.");
        glTexStorage2D(GL_TEXTURE_2D, 1, 0, 16, 16);
        expectError(GL_INVALID_ENUM, GL_INVALID_VALUE);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA_INTEGER, 16, 16);
        expectError(GL_INVALID_ENUM, GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM is generated if target is not one of the accepted target enumerants.");
        glTexStorage2D(0, 1, GL_RGBA8, 16, 16);
        expectError(GL_INVALID_ENUM);
        glTexStorage2D(GL_TEXTURE_3D, 1, GL_RGBA8, 16, 16);
        expectError(GL_INVALID_ENUM);
        glTexStorage2D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8, 16, 16);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width or height are less than 1.");
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 0, 16);
        expectError(GL_INVALID_VALUE);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 16, 0);
        expectError(GL_INVALID_VALUE);
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });

    ES3F_ADD_API_CASE(texstorage2d_invalid_binding, "Invalid glTexStorage2D() usage", {
        glBindTexture(GL_TEXTURE_2D, 0);

        m_log << TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if the default texture object is curently bound to target.");
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 16, 16);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        uint32_t texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if the texture object currently bound to "
                                      "target already has GL_TEXTURE_IMMUTABLE_FORMAT set to GL_TRUE.");
        int32_t immutable = -1;
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_IMMUTABLE_FORMAT, &immutable);
        m_log << TestLog::Message << "// GL_TEXTURE_IMMUTABLE_FORMAT = " << ((immutable != 0) ? "GL_TRUE" : "GL_FALSE")
              << TestLog::EndMessage;
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 16, 16);
        expectError(GL_NO_ERROR);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_IMMUTABLE_FORMAT, &immutable);
        m_log << TestLog::Message << "// GL_TEXTURE_IMMUTABLE_FORMAT = " << ((immutable != 0) ? "GL_TRUE" : "GL_FALSE")
              << TestLog::EndMessage;
        glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 16, 16);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES3F_ADD_API_CASE(texstorage2d_invalid_levels, "Invalid glTexStorage2D() usage", {
        uint32_t texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if levels is less than 1.");
        glTexStorage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 16, 16);
        expectError(GL_INVALID_VALUE);
        glTexStorage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if levels is greater than floor(log_2(max(width, height))) + 1");
        uint32_t log2MaxSize = deLog2Floor32(deMax32(16, 4)) + 1 + 1;
        glTexStorage2D(GL_TEXTURE_2D, log2MaxSize, GL_RGBA8, 16, 4);
        expectError(GL_INVALID_OPERATION);
        glTexStorage2D(GL_TEXTURE_2D, log2MaxSize, GL_RGBA8, 4, 16);
        expectError(GL_INVALID_OPERATION);
        glTexStorage2D(GL_TEXTURE_2D, log2MaxSize, GL_RGBA8, 16, 16);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES3F_ADD_API_CASE(texstorage2d_invalid_astc_target, "ASTC formats require extensions present.", {
        // GLES 3.0.4, Sec 3.8.4, p.136: If there is no imageSize
        // for which this command would have been valid, an
        // INVALID_OPERATION error is generated. Also: If
        // executing the pseudo-code would result in any other
        // error, the error is generated and the command will have
        // no effect.
        // In conclusion: Expect same errors as with TexImage?D

        if (m_context.getContextInfo().isExtensionSupported("GL_KHR_texture_compression_astc_ldr"))
        {
            m_log.writeMessage("ASTC supported. No negative API requirements.");
        }
        else
        {
            // In earlier tests both codes are accepted for invalid target format.
            m_log.writeMessage(
                "GL_INVALID_ENUM or GL_INVALID_VALUE should be generated if no ASTC extensions are present.");

            for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(s_astcFormats); formatNdx++)
            {
                const GLuint format                 = s_astcFormats[formatNdx];
                const CompressedTexFormat tcuFormat = mapGLCompressedTexFormat(format);
                const IVec3 blockPixels             = getBlockPixelSize(tcuFormat);
                const int32_t cubeSize = blockPixels.x() * blockPixels.y(); // Divisible by the block size and square
                uint32_t texture       = 0;

                glGenTextures(1, &texture);
                glBindTexture(GL_TEXTURE_2D, texture);

                glTexStorage2D(GL_TEXTURE_2D, 1, format, blockPixels.x(), blockPixels.y());
                expectError(GL_INVALID_ENUM, GL_INVALID_VALUE);

                glDeleteTextures(1, &texture);

                glGenTextures(1, &texture);
                glBindTexture(GL_TEXTURE_CUBE_MAP, texture);

                glTexStorage2D(GL_TEXTURE_CUBE_MAP, 1, format, cubeSize, cubeSize);
                expectError(GL_INVALID_ENUM, GL_INVALID_VALUE);

                glDeleteTextures(1, &texture);
            }
        }
    });

    // glTexStorage3D

    ES3F_ADD_API_CASE(texstorage3d, "Invalid glTexStorage3D() usage", {
        uint32_t texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_3D, texture);

        m_log << TestLog::Section(
            "",
            "GL_INVALID_ENUM or GL_INVALID_VALUE is generated if internalformat is not a valid sized internal format.");
        glTexStorage3D(GL_TEXTURE_3D, 1, 0, 4, 4, 4);
        expectError(GL_INVALID_ENUM, GL_INVALID_VALUE);
        glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA_INTEGER, 4, 4, 4);
        expectError(GL_INVALID_ENUM, GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "", "GL_INVALID_ENUM is generated if target is not one of the accepted target enumerants.");
        glTexStorage3D(0, 1, GL_RGBA8, 4, 4, 4);
        expectError(GL_INVALID_ENUM);
        glTexStorage3D(GL_TEXTURE_CUBE_MAP, 1, GL_RGBA8, 4, 4, 4);
        expectError(GL_INVALID_ENUM);
        glTexStorage3D(GL_TEXTURE_2D, 1, GL_RGBA8, 4, 4, 4);
        expectError(GL_INVALID_ENUM);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if width, height or depth are less than 1.");
        glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA8, 0, 4, 4);
        expectError(GL_INVALID_VALUE);
        glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA8, 4, 0, 4);
        expectError(GL_INVALID_VALUE);
        glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA8, 4, 4, 0);
        expectError(GL_INVALID_VALUE);
        glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA8, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES3F_ADD_API_CASE(texstorage3d_invalid_binding, "Invalid glTexStorage3D() usage", {
        glBindTexture(GL_TEXTURE_3D, 0);

        m_log << TestLog::Section(
            "", "GL_INVALID_OPERATION is generated if the default texture object is curently bound to target.");
        glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA8, 4, 4, 4);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        uint32_t texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_3D, texture);

        m_log << TestLog::Section("", "GL_INVALID_OPERATION is generated if the texture object currently bound to "
                                      "target already has GL_TEXTURE_IMMUTABLE_FORMAT set to GL_TRUE.");
        int32_t immutable = -1;
        glGetTexParameteriv(GL_TEXTURE_3D, GL_TEXTURE_IMMUTABLE_FORMAT, &immutable);
        m_log << TestLog::Message << "// GL_TEXTURE_IMMUTABLE_FORMAT = " << ((immutable != 0) ? "GL_TRUE" : "GL_FALSE")
              << TestLog::EndMessage;
        glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA8, 4, 4, 4);
        expectError(GL_NO_ERROR);
        glGetTexParameteriv(GL_TEXTURE_3D, GL_TEXTURE_IMMUTABLE_FORMAT, &immutable);
        m_log << TestLog::Message << "// GL_TEXTURE_IMMUTABLE_FORMAT = " << ((immutable != 0) ? "GL_TRUE" : "GL_FALSE")
              << TestLog::EndMessage;
        glTexStorage3D(GL_TEXTURE_3D, 1, GL_RGBA8, 4, 4, 4);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });
    ES3F_ADD_API_CASE(texstorage3d_invalid_levels, "Invalid glTexStorage3D() usage", {
        uint32_t texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_3D, texture);

        m_log << TestLog::Section("", "GL_INVALID_VALUE is generated if levels is less than 1.");
        glTexStorage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 4, 4, 4);
        expectError(GL_INVALID_VALUE);
        glTexStorage3D(GL_TEXTURE_3D, 0, GL_RGBA8, 0, 0, 0);
        expectError(GL_INVALID_VALUE);
        m_log << TestLog::EndSection;

        m_log << TestLog::Section(
            "",
            "GL_INVALID_OPERATION is generated if levels is greater than floor(log_2(max(width, height, depth))) + 1");
        uint32_t log2MaxSize = deLog2Floor32(8) + 1 + 1;
        glTexStorage3D(GL_TEXTURE_3D, log2MaxSize, GL_RGBA8, 8, 2, 2);
        expectError(GL_INVALID_OPERATION);
        glTexStorage3D(GL_TEXTURE_3D, log2MaxSize, GL_RGBA8, 2, 8, 2);
        expectError(GL_INVALID_OPERATION);
        glTexStorage3D(GL_TEXTURE_3D, log2MaxSize, GL_RGBA8, 2, 2, 8);
        expectError(GL_INVALID_OPERATION);
        glTexStorage3D(GL_TEXTURE_3D, log2MaxSize, GL_RGBA8, 8, 8, 8);
        expectError(GL_INVALID_OPERATION);
        m_log << TestLog::EndSection;

        glDeleteTextures(1, &texture);
    });

    ES3F_ADD_API_CASE(texstorage3d_invalid_astc_target, "Invalid glTexStorage3D() ASTC 3D targets", {
        // GLES 3.0.4, Sec 3.8.4, p.136: If there is no imageSize
        // for which this command would have been valid, an
        // INVALID_OPERATION error is generated. Also: If
        // executing the pseudo-code would result in any other
        // error, the error is generated and the command will have
        // no effect.
        // In conclusion: Expect same errors as with TexImage?D

        if (m_context.getContextInfo().isExtensionSupported("GL_KHR_texture_compression_astc_sliced_3d") ||
            m_context.getContextInfo().isExtensionSupported("GL_KHR_texture_compression_astc_hdr") ||
            m_context.getContextInfo().isExtensionSupported("GL_OES_texture_compression_astc"))
        {
            m_log.writeMessage("Full ASTC supported. No negative API requirements.");
        }
        else
        {
            const bool ldrAstcSupported =
                m_context.getContextInfo().isExtensionSupported("GL_KHR_texture_compression_astc_ldr");
            if (ldrAstcSupported)
                m_log.writeMessage("GL_INVALID_OPERATION should be generated if using TEXTURE_3D with LDR.");
            else
                // In earlier tests both codes are accepted for invalid target format.
                m_log.writeMessage(
                    "GL_INVALID_ENUM or GL_INVALID_VALUE should be generated if no ASTC extensions are present.");

            for (int formatNdx = 0; formatNdx < DE_LENGTH_OF_ARRAY(s_astcFormats); formatNdx++)
            {
                const GLuint format                 = s_astcFormats[formatNdx];
                const CompressedTexFormat tcuFormat = mapGLCompressedTexFormat(format);
                const IVec3 blockPixels             = getBlockPixelSize(tcuFormat);
                uint32_t texture                    = 0;

                glGenTextures(1, &texture);
                glBindTexture(GL_TEXTURE_3D, texture);

                glTexStorage3D(GL_TEXTURE_3D, 1, format, blockPixels.x(), blockPixels.y(), blockPixels.z());

                if (ldrAstcSupported)
                    expectError(GL_INVALID_OPERATION);
                else
                    expectError(GL_INVALID_ENUM, GL_INVALID_VALUE);

                glDeleteTextures(1, &texture);
            }
        }
    });
}

} // namespace Functional
} // namespace gles3
} // namespace deqp
