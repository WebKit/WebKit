/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES Utilities
 * ------------------------------------------------
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
 * \brief Texture format utilities.
 *//*--------------------------------------------------------------------*/

#include "gluTextureUtil.hpp"
#include "gluRenderContext.hpp"
#include "gluContextInfo.hpp"
#include "gluTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuFormatUtil.hpp"
#include "glwEnums.hpp"

namespace glu
{

using std::string;

/*--------------------------------------------------------------------*//*!
 * \brief Map tcu::TextureFormat to GL pixel transfer format.
 *
 * Maps generic texture format description to GL pixel transfer format.
 * If no mapping is found, throws tcu::InternalError.
 *
 * \param texFormat Generic texture format.
 * \return GL pixel transfer format.
 *//*--------------------------------------------------------------------*/
TransferFormat getTransferFormat(tcu::TextureFormat texFormat)
{
    using tcu::TextureFormat;

    uint32_t format = GL_NONE;
    uint32_t type   = GL_NONE;
    bool isInt      = false;

    switch (texFormat.type)
    {
    case TextureFormat::SIGNED_INT8:
    case TextureFormat::SIGNED_INT16:
    case TextureFormat::SIGNED_INT32:
    case TextureFormat::UNSIGNED_INT8:
    case TextureFormat::UNSIGNED_INT16:
    case TextureFormat::UNSIGNED_INT32:
    case TextureFormat::UNSIGNED_INT_1010102_REV:
        isInt = true;
        break;

    default:
        isInt = false;
        break;
    }

    switch (texFormat.order)
    {
    case TextureFormat::A:
        format = GL_ALPHA;
        break;
    case TextureFormat::L:
        format = GL_LUMINANCE;
        break;
    case TextureFormat::LA:
        format = GL_LUMINANCE_ALPHA;
        break;
    case TextureFormat::R:
        format = isInt ? GL_RED_INTEGER : GL_RED;
        break;
    case TextureFormat::RG:
        format = isInt ? GL_RG_INTEGER : GL_RG;
        break;
    case TextureFormat::RGB:
        format = isInt ? GL_RGB_INTEGER : GL_RGB;
        break;
    case TextureFormat::RGBA:
        format = isInt ? GL_RGBA_INTEGER : GL_RGBA;
        break;
    case TextureFormat::sR:
        format = GL_RED;
        break;
    case TextureFormat::sRG:
        format = GL_RG;
        break;
    case TextureFormat::sRGB:
        format = GL_RGB;
        break;
    case TextureFormat::sRGBA:
        format = GL_RGBA;
        break;
    case TextureFormat::D:
        format = GL_DEPTH_COMPONENT;
        break;
    case TextureFormat::DS:
        format = GL_DEPTH_STENCIL;
        break;
    case TextureFormat::S:
        format = GL_STENCIL_INDEX;
        break;

    case TextureFormat::BGRA:
        DE_ASSERT(!isInt);
        format = GL_BGRA;
        break;

    default:
        DE_ASSERT(false);
    }

    switch (texFormat.type)
    {
    case TextureFormat::SNORM_INT8:
        type = GL_BYTE;
        break;
    case TextureFormat::SNORM_INT16:
        type = GL_SHORT;
        break;
    case TextureFormat::UNORM_INT8:
        type = GL_UNSIGNED_BYTE;
        break;
    case TextureFormat::UNORM_INT16:
        type = GL_UNSIGNED_SHORT;
        break;
    case TextureFormat::UNORM_SHORT_565:
        type = GL_UNSIGNED_SHORT_5_6_5;
        break;
    case TextureFormat::UNORM_SHORT_4444:
        type = GL_UNSIGNED_SHORT_4_4_4_4;
        break;
    case TextureFormat::UNORM_SHORT_5551:
        type = GL_UNSIGNED_SHORT_5_5_5_1;
        break;
    case TextureFormat::SIGNED_INT8:
        type = GL_BYTE;
        break;
    case TextureFormat::SIGNED_INT16:
        type = GL_SHORT;
        break;
    case TextureFormat::SIGNED_INT32:
        type = GL_INT;
        break;
    case TextureFormat::UNSIGNED_INT8:
        type = GL_UNSIGNED_BYTE;
        break;
    case TextureFormat::UNSIGNED_INT16:
        type = GL_UNSIGNED_SHORT;
        break;
    case TextureFormat::UNSIGNED_INT32:
        type = GL_UNSIGNED_INT;
        break;
    case TextureFormat::FLOAT:
        type = GL_FLOAT;
        break;
    case TextureFormat::UNORM_INT_101010:
        type = GL_UNSIGNED_INT_2_10_10_10_REV;
        break;
    case TextureFormat::UNORM_INT_1010102_REV:
        type = GL_UNSIGNED_INT_2_10_10_10_REV;
        break;
    case TextureFormat::UNSIGNED_INT_1010102_REV:
        type = GL_UNSIGNED_INT_2_10_10_10_REV;
        break;
    case TextureFormat::UNSIGNED_INT_11F_11F_10F_REV:
        type = GL_UNSIGNED_INT_10F_11F_11F_REV;
        break;
    case TextureFormat::UNSIGNED_INT_999_E5_REV:
        type = GL_UNSIGNED_INT_5_9_9_9_REV;
        break;
    case TextureFormat::HALF_FLOAT:
        type = GL_HALF_FLOAT;
        break;
    case TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV:
        type = GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
        break;
    case TextureFormat::UNSIGNED_INT_24_8:
        type = texFormat.order == TextureFormat::D ? GL_UNSIGNED_INT : GL_UNSIGNED_INT_24_8;
        break;

    default:
        throw tcu::InternalError("Can't map texture format to GL transfer format");
    }

    return TransferFormat(format, type);
}

/*--------------------------------------------------------------------*//*!
 * \brief Map tcu::TextureFormat to GL internal sized format.
 *
 * Maps generic texture format description to GL internal format.
 * If no mapping is found, throws tcu::InternalError.
 *
 * \param texFormat Generic texture format.
 * \return GL sized internal format.
 *//*--------------------------------------------------------------------*/
uint32_t getInternalFormat(tcu::TextureFormat texFormat)
{
    DE_STATIC_ASSERT(tcu::TextureFormat::CHANNELORDER_LAST < (1 << 16));
    DE_STATIC_ASSERT(tcu::TextureFormat::CHANNELTYPE_LAST < (1 << 16));

#define PACK_FMT(ORDER, TYPE) ((int(ORDER) << 16) | int(TYPE))
#define FMT_CASE(ORDER, TYPE) PACK_FMT(tcu::TextureFormat::ORDER, tcu::TextureFormat::TYPE)

    switch (PACK_FMT(texFormat.order, texFormat.type))
    {
    case FMT_CASE(RGBA, UNORM_SHORT_5551):
        return GL_RGB5_A1;
    case FMT_CASE(RGBA, UNORM_SHORT_4444):
        return GL_RGBA4;
    case FMT_CASE(RGB, UNORM_SHORT_565):
        return GL_RGB565;
    case FMT_CASE(D, UNORM_INT16):
        return GL_DEPTH_COMPONENT16;
    case FMT_CASE(S, UNSIGNED_INT8):
        return GL_STENCIL_INDEX8;

    case FMT_CASE(RGBA, FLOAT):
        return GL_RGBA32F;
    case FMT_CASE(RGBA, SIGNED_INT32):
        return GL_RGBA32I;
    case FMT_CASE(RGBA, UNSIGNED_INT32):
        return GL_RGBA32UI;
    case FMT_CASE(RGBA, UNORM_INT16):
        return GL_RGBA16;
    case FMT_CASE(RGBA, SNORM_INT16):
        return GL_RGBA16_SNORM;
    case FMT_CASE(RGBA, HALF_FLOAT):
        return GL_RGBA16F;
    case FMT_CASE(RGBA, SIGNED_INT16):
        return GL_RGBA16I;
    case FMT_CASE(RGBA, UNSIGNED_INT16):
        return GL_RGBA16UI;
    case FMT_CASE(RGBA, UNORM_INT8):
        return GL_RGBA8;
    case FMT_CASE(RGBA, SIGNED_INT8):
        return GL_RGBA8I;
    case FMT_CASE(RGBA, UNSIGNED_INT8):
        return GL_RGBA8UI;
    case FMT_CASE(sRGBA, UNORM_INT8):
        return GL_SRGB8_ALPHA8;
    case FMT_CASE(RGBA, UNORM_INT_1010102_REV):
        return GL_RGB10_A2;
    case FMT_CASE(RGBA, UNSIGNED_INT_1010102_REV):
        return GL_RGB10_A2UI;
    case FMT_CASE(RGBA, SNORM_INT8):
        return GL_RGBA8_SNORM;

    case FMT_CASE(RGB, UNORM_INT8):
        return GL_RGB8;
    case FMT_CASE(RGB, UNSIGNED_INT_11F_11F_10F_REV):
        return GL_R11F_G11F_B10F;
    case FMT_CASE(RGB, FLOAT):
        return GL_RGB32F;
    case FMT_CASE(RGB, SIGNED_INT32):
        return GL_RGB32I;
    case FMT_CASE(RGB, UNSIGNED_INT32):
        return GL_RGB32UI;
    case FMT_CASE(RGB, UNORM_INT16):
        return GL_RGB16;
    case FMT_CASE(RGB, SNORM_INT16):
        return GL_RGB16_SNORM;
    case FMT_CASE(RGB, HALF_FLOAT):
        return GL_RGB16F;
    case FMT_CASE(RGB, SIGNED_INT16):
        return GL_RGB16I;
    case FMT_CASE(RGB, UNSIGNED_INT16):
        return GL_RGB16UI;
    case FMT_CASE(RGB, SNORM_INT8):
        return GL_RGB8_SNORM;
    case FMT_CASE(RGB, SIGNED_INT8):
        return GL_RGB8I;
    case FMT_CASE(RGB, UNSIGNED_INT8):
        return GL_RGB8UI;
    case FMT_CASE(sRGB, UNORM_INT8):
        return GL_SRGB8;
    case FMT_CASE(RGB, UNSIGNED_INT_999_E5_REV):
        return GL_RGB9_E5;
    case FMT_CASE(RGB, UNORM_INT_1010102_REV):
        return GL_RGB10;

    case FMT_CASE(RG, FLOAT):
        return GL_RG32F;
    case FMT_CASE(RG, SIGNED_INT32):
        return GL_RG32I;
    case FMT_CASE(RG, UNSIGNED_INT32):
        return GL_RG32UI;
    case FMT_CASE(RG, UNORM_INT16):
        return GL_RG16;
    case FMT_CASE(RG, SNORM_INT16):
        return GL_RG16_SNORM;
    case FMT_CASE(RG, HALF_FLOAT):
        return GL_RG16F;
    case FMT_CASE(RG, SIGNED_INT16):
        return GL_RG16I;
    case FMT_CASE(RG, UNSIGNED_INT16):
        return GL_RG16UI;
    case FMT_CASE(RG, UNORM_INT8):
        return GL_RG8;
    case FMT_CASE(RG, SIGNED_INT8):
        return GL_RG8I;
    case FMT_CASE(RG, UNSIGNED_INT8):
        return GL_RG8UI;
    case FMT_CASE(RG, SNORM_INT8):
        return GL_RG8_SNORM;
    case FMT_CASE(sRG, UNORM_INT8):
        return GL_SRG8_EXT;

    case FMT_CASE(R, FLOAT):
        return GL_R32F;
    case FMT_CASE(R, SIGNED_INT32):
        return GL_R32I;
    case FMT_CASE(R, UNSIGNED_INT32):
        return GL_R32UI;
    case FMT_CASE(R, UNORM_INT16):
        return GL_R16;
    case FMT_CASE(R, SNORM_INT16):
        return GL_R16_SNORM;
    case FMT_CASE(R, HALF_FLOAT):
        return GL_R16F;
    case FMT_CASE(R, SIGNED_INT16):
        return GL_R16I;
    case FMT_CASE(R, UNSIGNED_INT16):
        return GL_R16UI;
    case FMT_CASE(R, UNORM_INT8):
        return GL_R8;
    case FMT_CASE(R, SIGNED_INT8):
        return GL_R8I;
    case FMT_CASE(R, UNSIGNED_INT8):
        return GL_R8UI;
    case FMT_CASE(R, SNORM_INT8):
        return GL_R8_SNORM;
    case FMT_CASE(sR, UNORM_INT8):
        return GL_SR8_EXT;

    case FMT_CASE(D, FLOAT):
        return GL_DEPTH_COMPONENT32F;
    case FMT_CASE(D, UNSIGNED_INT_24_8):
        return GL_DEPTH_COMPONENT24;
    case FMT_CASE(D, UNSIGNED_INT32):
        return GL_DEPTH_COMPONENT32;
    case FMT_CASE(DS, FLOAT_UNSIGNED_INT_24_8_REV):
        return GL_DEPTH32F_STENCIL8;
    case FMT_CASE(DS, UNSIGNED_INT_24_8):
        return GL_DEPTH24_STENCIL8;

    default:
        throw tcu::InternalError("Can't map texture format to GL internal format");
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Map generic compressed format to GL compressed format enum.
 *
 * Maps generic compressed format to GL compressed format enum value.
 * If no mapping is found, throws tcu::InternalError.
 *
 * \param format Generic compressed format.
 * \return GL compressed texture format.
 *//*--------------------------------------------------------------------*/
uint32_t getGLFormat(tcu::CompressedTexFormat format)
{
    switch (format)
    {
    case tcu::COMPRESSEDTEXFORMAT_ETC1_RGB8:
        return GL_ETC1_RGB8_OES;
    case tcu::COMPRESSEDTEXFORMAT_EAC_R11:
        return GL_COMPRESSED_R11_EAC;
    case tcu::COMPRESSEDTEXFORMAT_EAC_SIGNED_R11:
        return GL_COMPRESSED_SIGNED_R11_EAC;
    case tcu::COMPRESSEDTEXFORMAT_EAC_RG11:
        return GL_COMPRESSED_RG11_EAC;
    case tcu::COMPRESSEDTEXFORMAT_EAC_SIGNED_RG11:
        return GL_COMPRESSED_SIGNED_RG11_EAC;
    case tcu::COMPRESSEDTEXFORMAT_ETC2_RGB8:
        return GL_COMPRESSED_RGB8_ETC2;
    case tcu::COMPRESSEDTEXFORMAT_ETC2_SRGB8:
        return GL_COMPRESSED_SRGB8_ETC2;
    case tcu::COMPRESSEDTEXFORMAT_ETC2_RGB8_PUNCHTHROUGH_ALPHA1:
        return GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2;
    case tcu::COMPRESSEDTEXFORMAT_ETC2_SRGB8_PUNCHTHROUGH_ALPHA1:
        return GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2;
    case tcu::COMPRESSEDTEXFORMAT_ETC2_EAC_RGBA8:
        return GL_COMPRESSED_RGBA8_ETC2_EAC;
    case tcu::COMPRESSEDTEXFORMAT_ETC2_EAC_SRGB8_ALPHA8:
        return GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC;

    case tcu::COMPRESSEDTEXFORMAT_ASTC_4x4_RGBA:
        return GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
    case tcu::COMPRESSEDTEXFORMAT_ASTC_5x4_RGBA:
        return GL_COMPRESSED_RGBA_ASTC_5x4_KHR;
    case tcu::COMPRESSEDTEXFORMAT_ASTC_5x5_RGBA:
        return GL_COMPRESSED_RGBA_ASTC_5x5_KHR;
    case tcu::COMPRESSEDTEXFORMAT_ASTC_6x5_RGBA:
        return GL_COMPRESSED_RGBA_ASTC_6x5_KHR;
    case tcu::COMPRESSEDTEXFORMAT_ASTC_6x6_RGBA:
        return GL_COMPRESSED_RGBA_ASTC_6x6_KHR;
    case tcu::COMPRESSEDTEXFORMAT_ASTC_8x5_RGBA:
        return GL_COMPRESSED_RGBA_ASTC_8x5_KHR;
    case tcu::COMPRESSEDTEXFORMAT_ASTC_8x6_RGBA:
        return GL_COMPRESSED_RGBA_ASTC_8x6_KHR;
    case tcu::COMPRESSEDTEXFORMAT_ASTC_8x8_RGBA:
        return GL_COMPRESSED_RGBA_ASTC_8x8_KHR;
    case tcu::COMPRESSEDTEXFORMAT_ASTC_10x5_RGBA:
        return GL_COMPRESSED_RGBA_ASTC_10x5_KHR;
    case tcu::COMPRESSEDTEXFORMAT_ASTC_10x6_RGBA:
        return GL_COMPRESSED_RGBA_ASTC_10x6_KHR;
    case tcu::COMPRESSEDTEXFORMAT_ASTC_10x8_RGBA:
        return GL_COMPRESSED_RGBA_ASTC_10x8_KHR;
    case tcu::COMPRESSEDTEXFORMAT_ASTC_10x10_RGBA:
        return GL_COMPRESSED_RGBA_ASTC_10x10_KHR;
    case tcu::COMPRESSEDTEXFORMAT_ASTC_12x10_RGBA:
        return GL_COMPRESSED_RGBA_ASTC_12x10_KHR;
    case tcu::COMPRESSEDTEXFORMAT_ASTC_12x12_RGBA:
        return GL_COMPRESSED_RGBA_ASTC_12x12_KHR;
    case tcu::COMPRESSEDTEXFORMAT_ASTC_4x4_SRGB8_ALPHA8:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR;
    case tcu::COMPRESSEDTEXFORMAT_ASTC_5x4_SRGB8_ALPHA8:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR;
    case tcu::COMPRESSEDTEXFORMAT_ASTC_5x5_SRGB8_ALPHA8:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR;
    case tcu::COMPRESSEDTEXFORMAT_ASTC_6x5_SRGB8_ALPHA8:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR;
    case tcu::COMPRESSEDTEXFORMAT_ASTC_6x6_SRGB8_ALPHA8:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR;
    case tcu::COMPRESSEDTEXFORMAT_ASTC_8x5_SRGB8_ALPHA8:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR;
    case tcu::COMPRESSEDTEXFORMAT_ASTC_8x6_SRGB8_ALPHA8:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR;
    case tcu::COMPRESSEDTEXFORMAT_ASTC_8x8_SRGB8_ALPHA8:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR;
    case tcu::COMPRESSEDTEXFORMAT_ASTC_10x5_SRGB8_ALPHA8:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR;
    case tcu::COMPRESSEDTEXFORMAT_ASTC_10x6_SRGB8_ALPHA8:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR;
    case tcu::COMPRESSEDTEXFORMAT_ASTC_10x8_SRGB8_ALPHA8:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR;
    case tcu::COMPRESSEDTEXFORMAT_ASTC_10x10_SRGB8_ALPHA8:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR;
    case tcu::COMPRESSEDTEXFORMAT_ASTC_12x10_SRGB8_ALPHA8:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR;
    case tcu::COMPRESSEDTEXFORMAT_ASTC_12x12_SRGB8_ALPHA8:
        return GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR;

    default:
        throw tcu::InternalError("Can't map compressed format to GL format");
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Map compressed GL format to generic compressed format.
 *
 * Maps compressed GL format to generic compressed format.
 * If no mapping is found, throws tcu::InternalError.
 *
 * \param GL compressed texture format.
 * \return format Generic compressed format.
 *//*--------------------------------------------------------------------*/
tcu::CompressedTexFormat mapGLCompressedTexFormat(uint32_t format)
{
    switch (format)
    {
    case GL_ETC1_RGB8_OES:
        return tcu::COMPRESSEDTEXFORMAT_ETC1_RGB8;
    case GL_COMPRESSED_R11_EAC:
        return tcu::COMPRESSEDTEXFORMAT_EAC_R11;
    case GL_COMPRESSED_SIGNED_R11_EAC:
        return tcu::COMPRESSEDTEXFORMAT_EAC_SIGNED_R11;
    case GL_COMPRESSED_RG11_EAC:
        return tcu::COMPRESSEDTEXFORMAT_EAC_RG11;
    case GL_COMPRESSED_SIGNED_RG11_EAC:
        return tcu::COMPRESSEDTEXFORMAT_EAC_SIGNED_RG11;
    case GL_COMPRESSED_RGB8_ETC2:
        return tcu::COMPRESSEDTEXFORMAT_ETC2_RGB8;
    case GL_COMPRESSED_SRGB8_ETC2:
        return tcu::COMPRESSEDTEXFORMAT_ETC2_SRGB8;
    case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
        return tcu::COMPRESSEDTEXFORMAT_ETC2_RGB8_PUNCHTHROUGH_ALPHA1;
    case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
        return tcu::COMPRESSEDTEXFORMAT_ETC2_SRGB8_PUNCHTHROUGH_ALPHA1;
    case GL_COMPRESSED_RGBA8_ETC2_EAC:
        return tcu::COMPRESSEDTEXFORMAT_ETC2_EAC_RGBA8;
    case GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
        return tcu::COMPRESSEDTEXFORMAT_ETC2_EAC_SRGB8_ALPHA8;

    case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
        return tcu::COMPRESSEDTEXFORMAT_ASTC_4x4_RGBA;
    case GL_COMPRESSED_RGBA_ASTC_5x4_KHR:
        return tcu::COMPRESSEDTEXFORMAT_ASTC_5x4_RGBA;
    case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:
        return tcu::COMPRESSEDTEXFORMAT_ASTC_5x5_RGBA;
    case GL_COMPRESSED_RGBA_ASTC_6x5_KHR:
        return tcu::COMPRESSEDTEXFORMAT_ASTC_6x5_RGBA;
    case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:
        return tcu::COMPRESSEDTEXFORMAT_ASTC_6x6_RGBA;
    case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:
        return tcu::COMPRESSEDTEXFORMAT_ASTC_8x5_RGBA;
    case GL_COMPRESSED_RGBA_ASTC_8x6_KHR:
        return tcu::COMPRESSEDTEXFORMAT_ASTC_8x6_RGBA;
    case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:
        return tcu::COMPRESSEDTEXFORMAT_ASTC_8x8_RGBA;
    case GL_COMPRESSED_RGBA_ASTC_10x5_KHR:
        return tcu::COMPRESSEDTEXFORMAT_ASTC_10x5_RGBA;
    case GL_COMPRESSED_RGBA_ASTC_10x6_KHR:
        return tcu::COMPRESSEDTEXFORMAT_ASTC_10x6_RGBA;
    case GL_COMPRESSED_RGBA_ASTC_10x8_KHR:
        return tcu::COMPRESSEDTEXFORMAT_ASTC_10x8_RGBA;
    case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:
        return tcu::COMPRESSEDTEXFORMAT_ASTC_10x10_RGBA;
    case GL_COMPRESSED_RGBA_ASTC_12x10_KHR:
        return tcu::COMPRESSEDTEXFORMAT_ASTC_12x10_RGBA;
    case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:
        return tcu::COMPRESSEDTEXFORMAT_ASTC_12x12_RGBA;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
        return tcu::COMPRESSEDTEXFORMAT_ASTC_4x4_SRGB8_ALPHA8;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
        return tcu::COMPRESSEDTEXFORMAT_ASTC_5x4_SRGB8_ALPHA8;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
        return tcu::COMPRESSEDTEXFORMAT_ASTC_5x5_SRGB8_ALPHA8;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
        return tcu::COMPRESSEDTEXFORMAT_ASTC_6x5_SRGB8_ALPHA8;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
        return tcu::COMPRESSEDTEXFORMAT_ASTC_6x6_SRGB8_ALPHA8;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
        return tcu::COMPRESSEDTEXFORMAT_ASTC_8x5_SRGB8_ALPHA8;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:
        return tcu::COMPRESSEDTEXFORMAT_ASTC_8x6_SRGB8_ALPHA8;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
        return tcu::COMPRESSEDTEXFORMAT_ASTC_8x8_SRGB8_ALPHA8;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
        return tcu::COMPRESSEDTEXFORMAT_ASTC_10x5_SRGB8_ALPHA8;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
        return tcu::COMPRESSEDTEXFORMAT_ASTC_10x6_SRGB8_ALPHA8;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
        return tcu::COMPRESSEDTEXFORMAT_ASTC_10x8_SRGB8_ALPHA8;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
        return tcu::COMPRESSEDTEXFORMAT_ASTC_10x10_SRGB8_ALPHA8;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
        return tcu::COMPRESSEDTEXFORMAT_ASTC_12x10_SRGB8_ALPHA8;
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:
        return tcu::COMPRESSEDTEXFORMAT_ASTC_12x12_SRGB8_ALPHA8;

    default:
        throw tcu::InternalError("Can't map compressed GL format to compressed format");
    }
}

bool isCompressedFormat(uint32_t internalFormat)
{
    switch (internalFormat)
    {
    case GL_ETC1_RGB8_OES:
    case GL_COMPRESSED_R11_EAC:
    case GL_COMPRESSED_SIGNED_R11_EAC:
    case GL_COMPRESSED_RG11_EAC:
    case GL_COMPRESSED_SIGNED_RG11_EAC:
    case GL_COMPRESSED_RGB8_ETC2:
    case GL_COMPRESSED_SRGB8_ETC2:
    case GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2:
    case GL_COMPRESSED_RGBA8_ETC2_EAC:
    case GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC:
    case GL_COMPRESSED_RGBA_ASTC_4x4_KHR:
    case GL_COMPRESSED_RGBA_ASTC_5x4_KHR:
    case GL_COMPRESSED_RGBA_ASTC_5x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_6x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_6x6_KHR:
    case GL_COMPRESSED_RGBA_ASTC_8x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_8x6_KHR:
    case GL_COMPRESSED_RGBA_ASTC_8x8_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x5_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x6_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x8_KHR:
    case GL_COMPRESSED_RGBA_ASTC_10x10_KHR:
    case GL_COMPRESSED_RGBA_ASTC_12x10_KHR:
    case GL_COMPRESSED_RGBA_ASTC_12x12_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR:
    case GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR:
        return true;

    default:
        return false;
    }
}

static tcu::TextureFormat::ChannelType mapGLChannelType(uint32_t dataType, bool normalized)
{
    // \note Normalized bit is ignored where it doesn't apply.
    using tcu::TextureFormat;

    switch (dataType)
    {
    case GL_UNSIGNED_BYTE:
        return normalized ? TextureFormat::UNORM_INT8 : TextureFormat::UNSIGNED_INT8;
    case GL_BYTE:
        return normalized ? TextureFormat::SNORM_INT8 : TextureFormat::SIGNED_INT8;
    case GL_UNSIGNED_SHORT:
        return normalized ? TextureFormat::UNORM_INT16 : TextureFormat::UNSIGNED_INT16;
    case GL_SHORT:
        return normalized ? TextureFormat::SNORM_INT16 : TextureFormat::SIGNED_INT16;
    case GL_UNSIGNED_INT:
        return normalized ? TextureFormat::UNORM_INT32 : TextureFormat::UNSIGNED_INT32;
    case GL_INT:
        return normalized ? TextureFormat::SNORM_INT32 : TextureFormat::SIGNED_INT32;
    case GL_FLOAT:
        return TextureFormat::FLOAT;
    case GL_UNSIGNED_SHORT_4_4_4_4:
        return TextureFormat::UNORM_SHORT_4444;
    case GL_UNSIGNED_SHORT_5_5_5_1:
        return TextureFormat::UNORM_SHORT_5551;
    case GL_UNSIGNED_SHORT_5_6_5:
        return TextureFormat::UNORM_SHORT_565;
    case GL_HALF_FLOAT:
        return TextureFormat::HALF_FLOAT;
    case GL_UNSIGNED_INT_2_10_10_10_REV:
        return normalized ? TextureFormat::UNORM_INT_1010102_REV : TextureFormat::UNSIGNED_INT_1010102_REV;
    case GL_UNSIGNED_INT_10F_11F_11F_REV:
        return TextureFormat::UNSIGNED_INT_11F_11F_10F_REV;
    case GL_UNSIGNED_INT_24_8:
        return TextureFormat::UNSIGNED_INT_24_8;
    case GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
        return TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV;
    case GL_UNSIGNED_INT_5_9_9_9_REV:
        return TextureFormat::UNSIGNED_INT_999_E5_REV;

    // GL_OES_texture_half_float
    case GL_HALF_FLOAT_OES:
        return TextureFormat::HALF_FLOAT;

    default:
        DE_ASSERT(false);
        return TextureFormat::CHANNELTYPE_LAST;
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Map GL pixel transfer format to tcu::TextureFormat.
 *
 * If no mapping is found, throws tcu::InternalError.
 *
 * \param format    GL pixel format.
 * \param dataType    GL data type.
 * \return Generic texture format.
 *//*--------------------------------------------------------------------*/
tcu::TextureFormat mapGLTransferFormat(uint32_t format, uint32_t dataType)
{
    using tcu::TextureFormat;
    switch (format)
    {
    case GL_ALPHA:
        return TextureFormat(TextureFormat::A, mapGLChannelType(dataType, true));
    case GL_LUMINANCE:
        return TextureFormat(TextureFormat::L, mapGLChannelType(dataType, true));
    case GL_LUMINANCE_ALPHA:
        return TextureFormat(TextureFormat::LA, mapGLChannelType(dataType, true));
    case GL_RGB:
        return TextureFormat(TextureFormat::RGB, mapGLChannelType(dataType, true));
    case GL_RGBA:
        return TextureFormat(TextureFormat::RGBA, mapGLChannelType(dataType, true));
    case GL_BGRA:
        return TextureFormat(TextureFormat::BGRA, mapGLChannelType(dataType, true));
    case GL_RG:
        return TextureFormat(TextureFormat::RG, mapGLChannelType(dataType, true));
    case GL_RED:
        return TextureFormat(TextureFormat::R, mapGLChannelType(dataType, true));
    case GL_RGBA_INTEGER:
        return TextureFormat(TextureFormat::RGBA, mapGLChannelType(dataType, false));
    case GL_RGB_INTEGER:
        return TextureFormat(TextureFormat::RGB, mapGLChannelType(dataType, false));
    case GL_RG_INTEGER:
        return TextureFormat(TextureFormat::RG, mapGLChannelType(dataType, false));
    case GL_RED_INTEGER:
        return TextureFormat(TextureFormat::R, mapGLChannelType(dataType, false));
    case GL_SRGB:
        return TextureFormat(TextureFormat::sRGB, mapGLChannelType(dataType, false));
    case GL_SRGB_ALPHA:
        return TextureFormat(TextureFormat::sRGBA, mapGLChannelType(dataType, false));

    case GL_DEPTH_COMPONENT:
        return TextureFormat(TextureFormat::D, mapGLChannelType(dataType, true));
    case GL_DEPTH_STENCIL:
        return TextureFormat(TextureFormat::DS, mapGLChannelType(dataType, true));

    default:
        throw tcu::InternalError(string("Can't map GL pixel format (") + tcu::toHex(format).toString() + ", " +
                                 tcu::toHex(dataType).toString() + ") to texture format");
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Map GL internal texture format to tcu::TextureFormat.
 *
 * If no mapping is found, throws tcu::InternalError.
 *
 * \param internalFormat Sized internal format.
 * \return Generic texture format.
 *//*--------------------------------------------------------------------*/
tcu::TextureFormat mapGLInternalFormat(uint32_t internalFormat)
{
    using tcu::TextureFormat;
    switch (internalFormat)
    {
    case GL_RGB5_A1:
        return TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_SHORT_5551);
    case GL_RGBA4:
        return TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_SHORT_4444);
    case GL_RGB565:
        return TextureFormat(TextureFormat::RGB, TextureFormat::UNORM_SHORT_565);
    case GL_DEPTH_COMPONENT16:
        return TextureFormat(TextureFormat::D, TextureFormat::UNORM_INT16);
    case GL_STENCIL_INDEX8:
        return TextureFormat(TextureFormat::S, TextureFormat::UNSIGNED_INT8);

    case GL_RGBA32F:
        return TextureFormat(TextureFormat::RGBA, TextureFormat::FLOAT);
    case GL_RGBA32I:
        return TextureFormat(TextureFormat::RGBA, TextureFormat::SIGNED_INT32);
    case GL_RGBA32UI:
        return TextureFormat(TextureFormat::RGBA, TextureFormat::UNSIGNED_INT32);
    case GL_RGBA16:
        return TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT16);
    case GL_RGBA16_SNORM:
        return TextureFormat(TextureFormat::RGBA, TextureFormat::SNORM_INT16);
    case GL_RGBA16F:
        return TextureFormat(TextureFormat::RGBA, TextureFormat::HALF_FLOAT);
    case GL_RGBA16I:
        return TextureFormat(TextureFormat::RGBA, TextureFormat::SIGNED_INT16);
    case GL_RGBA16UI:
        return TextureFormat(TextureFormat::RGBA, TextureFormat::UNSIGNED_INT16);
    case GL_RGBA8:
        return TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8);
    case GL_RGBA8I:
        return TextureFormat(TextureFormat::RGBA, TextureFormat::SIGNED_INT8);
    case GL_RGBA8UI:
        return TextureFormat(TextureFormat::RGBA, TextureFormat::UNSIGNED_INT8);
    case GL_SRGB8_ALPHA8:
        return TextureFormat(TextureFormat::sRGBA, TextureFormat::UNORM_INT8);
    case GL_RGB10_A2:
        return TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT_1010102_REV);
    case GL_RGB10_A2UI:
        return TextureFormat(TextureFormat::RGBA, TextureFormat::UNSIGNED_INT_1010102_REV);
    case GL_RGBA8_SNORM:
        return TextureFormat(TextureFormat::RGBA, TextureFormat::SNORM_INT8);

    case GL_RGB8:
        return TextureFormat(TextureFormat::RGB, TextureFormat::UNORM_INT8);
    case GL_R11F_G11F_B10F:
        return TextureFormat(TextureFormat::RGB, TextureFormat::UNSIGNED_INT_11F_11F_10F_REV);
    case GL_RGB32F:
        return TextureFormat(TextureFormat::RGB, TextureFormat::FLOAT);
    case GL_RGB32I:
        return TextureFormat(TextureFormat::RGB, TextureFormat::SIGNED_INT32);
    case GL_RGB32UI:
        return TextureFormat(TextureFormat::RGB, TextureFormat::UNSIGNED_INT32);
    case GL_RGB16:
        return TextureFormat(TextureFormat::RGB, TextureFormat::UNORM_INT16);
    case GL_RGB16_SNORM:
        return TextureFormat(TextureFormat::RGB, TextureFormat::SNORM_INT16);
    case GL_RGB16F:
        return TextureFormat(TextureFormat::RGB, TextureFormat::HALF_FLOAT);
    case GL_RGB16I:
        return TextureFormat(TextureFormat::RGB, TextureFormat::SIGNED_INT16);
    case GL_RGB16UI:
        return TextureFormat(TextureFormat::RGB, TextureFormat::UNSIGNED_INT16);
    case GL_RGB8_SNORM:
        return TextureFormat(TextureFormat::RGB, TextureFormat::SNORM_INT8);
    case GL_RGB8I:
        return TextureFormat(TextureFormat::RGB, TextureFormat::SIGNED_INT8);
    case GL_RGB8UI:
        return TextureFormat(TextureFormat::RGB, TextureFormat::UNSIGNED_INT8);
    case GL_SRGB8:
        return TextureFormat(TextureFormat::sRGB, TextureFormat::UNORM_INT8);
    case GL_RGB9_E5:
        return TextureFormat(TextureFormat::RGB, TextureFormat::UNSIGNED_INT_999_E5_REV);
    case GL_RGB10:
        return TextureFormat(TextureFormat::RGB, TextureFormat::UNORM_INT_1010102_REV);

    case GL_RG32F:
        return TextureFormat(TextureFormat::RG, TextureFormat::FLOAT);
    case GL_RG32I:
        return TextureFormat(TextureFormat::RG, TextureFormat::SIGNED_INT32);
    case GL_RG32UI:
        return TextureFormat(TextureFormat::RG, TextureFormat::UNSIGNED_INT32);
    case GL_RG16:
        return TextureFormat(TextureFormat::RG, TextureFormat::UNORM_INT16);
    case GL_RG16_SNORM:
        return TextureFormat(TextureFormat::RG, TextureFormat::SNORM_INT16);
    case GL_RG16F:
        return TextureFormat(TextureFormat::RG, TextureFormat::HALF_FLOAT);
    case GL_RG16I:
        return TextureFormat(TextureFormat::RG, TextureFormat::SIGNED_INT16);
    case GL_RG16UI:
        return TextureFormat(TextureFormat::RG, TextureFormat::UNSIGNED_INT16);
    case GL_RG8:
        return TextureFormat(TextureFormat::RG, TextureFormat::UNORM_INT8);
    case GL_RG8I:
        return TextureFormat(TextureFormat::RG, TextureFormat::SIGNED_INT8);
    case GL_RG8UI:
        return TextureFormat(TextureFormat::RG, TextureFormat::UNSIGNED_INT8);
    case GL_RG8_SNORM:
        return TextureFormat(TextureFormat::RG, TextureFormat::SNORM_INT8);
    case GL_SRG8_EXT:
        return TextureFormat(TextureFormat::sRG, TextureFormat::UNORM_INT8);

    case GL_R32F:
        return TextureFormat(TextureFormat::R, TextureFormat::FLOAT);
    case GL_R32I:
        return TextureFormat(TextureFormat::R, TextureFormat::SIGNED_INT32);
    case GL_R32UI:
        return TextureFormat(TextureFormat::R, TextureFormat::UNSIGNED_INT32);
    case GL_R16:
        return TextureFormat(TextureFormat::R, TextureFormat::UNORM_INT16);
    case GL_R16_SNORM:
        return TextureFormat(TextureFormat::R, TextureFormat::SNORM_INT16);
    case GL_R16F:
        return TextureFormat(TextureFormat::R, TextureFormat::HALF_FLOAT);
    case GL_R16I:
        return TextureFormat(TextureFormat::R, TextureFormat::SIGNED_INT16);
    case GL_R16UI:
        return TextureFormat(TextureFormat::R, TextureFormat::UNSIGNED_INT16);
    case GL_R8:
        return TextureFormat(TextureFormat::R, TextureFormat::UNORM_INT8);
    case GL_R8I:
        return TextureFormat(TextureFormat::R, TextureFormat::SIGNED_INT8);
    case GL_R8UI:
        return TextureFormat(TextureFormat::R, TextureFormat::UNSIGNED_INT8);
    case GL_R8_SNORM:
        return TextureFormat(TextureFormat::R, TextureFormat::SNORM_INT8);
    case GL_SR8_EXT:
        return TextureFormat(TextureFormat::sR, TextureFormat::UNORM_INT8);

    case GL_BGRA:
        return TextureFormat(TextureFormat::BGRA, TextureFormat::UNORM_INT8);
    case GL_BGRA8_EXT:
        return TextureFormat(TextureFormat::BGRA, TextureFormat::UNORM_INT8);

    case GL_DEPTH_COMPONENT32F:
        return TextureFormat(TextureFormat::D, TextureFormat::FLOAT);
    case GL_DEPTH_COMPONENT24:
        return TextureFormat(TextureFormat::D, TextureFormat::UNSIGNED_INT_24_8);
    case GL_DEPTH_COMPONENT32:
        return TextureFormat(TextureFormat::D, TextureFormat::UNSIGNED_INT32);
    case GL_DEPTH32F_STENCIL8:
        return TextureFormat(TextureFormat::DS, TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV);
    case GL_DEPTH24_STENCIL8:
        return TextureFormat(TextureFormat::DS, TextureFormat::UNSIGNED_INT_24_8);

    default:
        throw tcu::InternalError(string("Can't map GL sized internal format (") +
                                 tcu::toHex(internalFormat).toString() + ") to texture format");
    }
}

bool isGLInternalColorFormatFilterable(uint32_t format)
{
    switch (format)
    {
    case GL_R8:
    case GL_R8_SNORM:
    case GL_RG8:
    case GL_RG8_SNORM:
    case GL_RGB8:
    case GL_RGB8_SNORM:
    case GL_RGB565:
    case GL_RGBA4:
    case GL_RGB5_A1:
    case GL_RGBA8:
    case GL_RGBA8_SNORM:
    case GL_RGB10_A2:
    case GL_SR8_EXT:
    case GL_SRG8_EXT:
    case GL_SRGB8:
    case GL_SRGB8_ALPHA8:
    case GL_R16F:
    case GL_RG16F:
    case GL_RGB16F:
    case GL_RGBA16F:
    case GL_R11F_G11F_B10F:
    case GL_RGB9_E5:
        return true;

    case GL_RGB10_A2UI:
    case GL_R32F:
    case GL_RG32F:
    case GL_RGB32F:
    case GL_RGBA32F:
    case GL_R8I:
    case GL_R8UI:
    case GL_R16I:
    case GL_R16UI:
    case GL_R32I:
    case GL_R32UI:
    case GL_RG8I:
    case GL_RG8UI:
    case GL_RG16I:
    case GL_RG16UI:
    case GL_RG32I:
    case GL_RG32UI:
    case GL_RGB8I:
    case GL_RGB8UI:
    case GL_RGB16I:
    case GL_RGB16UI:
    case GL_RGB32I:
    case GL_RGB32UI:
    case GL_RGBA8I:
    case GL_RGBA8UI:
    case GL_RGBA16I:
    case GL_RGBA16UI:
    case GL_RGBA32I:
    case GL_RGBA32UI:
        return false;

    default:
        DE_ASSERT(false);
        return false;
    }
}

static inline tcu::Sampler::WrapMode mapGLWrapMode(uint32_t wrapMode)
{
    switch (wrapMode)
    {
    case GL_CLAMP_TO_EDGE:
        return tcu::Sampler::CLAMP_TO_EDGE;
    case GL_CLAMP_TO_BORDER:
        return tcu::Sampler::CLAMP_TO_BORDER;
    case GL_REPEAT:
        return tcu::Sampler::REPEAT_GL;
    case GL_MIRRORED_REPEAT:
        return tcu::Sampler::MIRRORED_REPEAT_GL;
    default:
        throw tcu::InternalError("Can't map GL wrap mode " + tcu::toHex(wrapMode).toString());
    }
}

static inline tcu::Sampler::FilterMode mapGLMinFilterMode(uint32_t filterMode)
{
    switch (filterMode)
    {
    case GL_NEAREST:
        return tcu::Sampler::NEAREST;
    case GL_LINEAR:
        return tcu::Sampler::LINEAR;
    case GL_NEAREST_MIPMAP_NEAREST:
        return tcu::Sampler::NEAREST_MIPMAP_NEAREST;
    case GL_NEAREST_MIPMAP_LINEAR:
        return tcu::Sampler::NEAREST_MIPMAP_LINEAR;
    case GL_LINEAR_MIPMAP_NEAREST:
        return tcu::Sampler::LINEAR_MIPMAP_NEAREST;
    case GL_LINEAR_MIPMAP_LINEAR:
        return tcu::Sampler::LINEAR_MIPMAP_LINEAR;
    default:
        throw tcu::InternalError("Can't map GL min filter mode" + tcu::toHex(filterMode).toString());
    }
}

static inline tcu::Sampler::FilterMode mapGLMagFilterMode(uint32_t filterMode)
{
    switch (filterMode)
    {
    case GL_NEAREST:
        return tcu::Sampler::NEAREST;
    case GL_LINEAR:
        return tcu::Sampler::LINEAR;
    default:
        throw tcu::InternalError("Can't map GL mag filter mode" + tcu::toHex(filterMode).toString());
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Map GL sampler parameters to tcu::Sampler.
 *
 * If no mapping is found, throws tcu::InternalError.
 *
 * \param wrapS        S-component wrap mode
 * \param minFilter    Minification filter mode
 * \param magFilter    Magnification filter mode
 * \return Sampler description.
 *//*--------------------------------------------------------------------*/
tcu::Sampler mapGLSampler(uint32_t wrapS, uint32_t minFilter, uint32_t magFilter)
{
    return mapGLSampler(wrapS, wrapS, wrapS, minFilter, magFilter);
}

/*--------------------------------------------------------------------*//*!
 * \brief Map GL sampler parameters to tcu::Sampler.
 *
 * If no mapping is found, throws tcu::InternalError.
 *
 * \param wrapS        S-component wrap mode
 * \param wrapT        T-component wrap mode
 * \param minFilter    Minification filter mode
 * \param magFilter    Magnification filter mode
 * \return Sampler description.
 *//*--------------------------------------------------------------------*/
tcu::Sampler mapGLSampler(uint32_t wrapS, uint32_t wrapT, uint32_t minFilter, uint32_t magFilter)
{
    return mapGLSampler(wrapS, wrapT, wrapS, minFilter, magFilter);
}

/*--------------------------------------------------------------------*//*!
 * \brief Map GL sampler parameters to tcu::Sampler.
 *
 * If no mapping is found, throws tcu::InternalError.
 *
 * \param wrapS        S-component wrap mode
 * \param wrapT        T-component wrap mode
 * \param wrapR        R-component wrap mode
 * \param minFilter    Minification filter mode
 * \param magFilter    Magnification filter mode
 * \return Sampler description.
 *//*--------------------------------------------------------------------*/
tcu::Sampler mapGLSampler(uint32_t wrapS, uint32_t wrapT, uint32_t wrapR, uint32_t minFilter, uint32_t magFilter)
{
    return tcu::Sampler(mapGLWrapMode(wrapS), mapGLWrapMode(wrapT), mapGLWrapMode(wrapR), mapGLMinFilterMode(minFilter),
                        mapGLMagFilterMode(magFilter), 0.0f /* lod threshold */, true /* normalized coords */,
                        tcu::Sampler::COMPAREMODE_NONE /* no compare */, 0 /* compare channel */,
                        tcu::Vec4(0.0f) /* border color, not used */);
}

/*--------------------------------------------------------------------*//*!
 * \brief Map GL compare function to tcu::Sampler::CompareMode.
 *
 * If no mapping is found, throws tcu::InternalError.
 *
 * \param mode GL compare mode
 * \return Compare mode
 *//*--------------------------------------------------------------------*/
tcu::Sampler::CompareMode mapGLCompareFunc(uint32_t mode)
{
    switch (mode)
    {
    case GL_LESS:
        return tcu::Sampler::COMPAREMODE_LESS;
    case GL_LEQUAL:
        return tcu::Sampler::COMPAREMODE_LESS_OR_EQUAL;
    case GL_GREATER:
        return tcu::Sampler::COMPAREMODE_GREATER;
    case GL_GEQUAL:
        return tcu::Sampler::COMPAREMODE_GREATER_OR_EQUAL;
    case GL_EQUAL:
        return tcu::Sampler::COMPAREMODE_EQUAL;
    case GL_NOTEQUAL:
        return tcu::Sampler::COMPAREMODE_NOT_EQUAL;
    case GL_ALWAYS:
        return tcu::Sampler::COMPAREMODE_ALWAYS;
    case GL_NEVER:
        return tcu::Sampler::COMPAREMODE_NEVER;
    default:
        throw tcu::InternalError("Can't map GL compare mode " + tcu::toHex(mode).toString());
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Get GL wrap mode.
 *
 * If no mapping is found, throws tcu::InternalError.
 *
 * \param wrapMode Wrap mode
 * \return GL wrap mode
 *//*--------------------------------------------------------------------*/
uint32_t getGLWrapMode(tcu::Sampler::WrapMode wrapMode)
{
    DE_ASSERT(wrapMode != tcu::Sampler::WRAPMODE_LAST);
    switch (wrapMode)
    {
    case tcu::Sampler::CLAMP_TO_EDGE:
        return GL_CLAMP_TO_EDGE;
    case tcu::Sampler::CLAMP_TO_BORDER:
        return GL_CLAMP_TO_BORDER;
    case tcu::Sampler::REPEAT_GL:
        return GL_REPEAT;
    case tcu::Sampler::MIRRORED_REPEAT_GL:
        return GL_MIRRORED_REPEAT;
    default:
        throw tcu::InternalError("Can't map wrap mode");
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Get GL filter mode.
 *
 * If no mapping is found, throws tcu::InternalError.
 *
 * \param filterMode Filter mode
 * \return GL filter mode
 *//*--------------------------------------------------------------------*/
uint32_t getGLFilterMode(tcu::Sampler::FilterMode filterMode)
{
    DE_ASSERT(filterMode != tcu::Sampler::FILTERMODE_LAST);
    switch (filterMode)
    {
    case tcu::Sampler::NEAREST:
        return GL_NEAREST;
    case tcu::Sampler::LINEAR:
        return GL_LINEAR;
    case tcu::Sampler::NEAREST_MIPMAP_NEAREST:
        return GL_NEAREST_MIPMAP_NEAREST;
    case tcu::Sampler::NEAREST_MIPMAP_LINEAR:
        return GL_NEAREST_MIPMAP_LINEAR;
    case tcu::Sampler::LINEAR_MIPMAP_NEAREST:
        return GL_LINEAR_MIPMAP_NEAREST;
    case tcu::Sampler::LINEAR_MIPMAP_LINEAR:
        return GL_LINEAR_MIPMAP_LINEAR;
    default:
        throw tcu::InternalError("Can't map filter mode");
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Get GL compare mode.
 *
 * If no mapping is found, throws tcu::InternalError.
 *
 * \param compareMode Compare mode
 * \return GL compare mode
 *//*--------------------------------------------------------------------*/
uint32_t getGLCompareFunc(tcu::Sampler::CompareMode compareMode)
{
    DE_ASSERT(compareMode != tcu::Sampler::COMPAREMODE_NONE);
    switch (compareMode)
    {
    case tcu::Sampler::COMPAREMODE_NONE:
        return GL_NONE;
    case tcu::Sampler::COMPAREMODE_LESS:
        return GL_LESS;
    case tcu::Sampler::COMPAREMODE_LESS_OR_EQUAL:
        return GL_LEQUAL;
    case tcu::Sampler::COMPAREMODE_GREATER:
        return GL_GREATER;
    case tcu::Sampler::COMPAREMODE_GREATER_OR_EQUAL:
        return GL_GEQUAL;
    case tcu::Sampler::COMPAREMODE_EQUAL:
        return GL_EQUAL;
    case tcu::Sampler::COMPAREMODE_NOT_EQUAL:
        return GL_NOTEQUAL;
    case tcu::Sampler::COMPAREMODE_ALWAYS:
        return GL_ALWAYS;
    case tcu::Sampler::COMPAREMODE_NEVER:
        return GL_NEVER;
    default:
        throw tcu::InternalError("Can't map compare mode");
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Get GL cube face.
 *
 * If no mapping is found, throws tcu::InternalError.
 *
 * \param face Cube face
 * \return GL cube face
 *//*--------------------------------------------------------------------*/
uint32_t getGLCubeFace(tcu::CubeFace face)
{
    DE_ASSERT(face != tcu::CUBEFACE_LAST);
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
        throw tcu::InternalError("Can't map cube face");
    }
}

tcu::CubeFace getCubeFaceFromGL(uint32_t face)
{
    switch (face)
    {
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        return tcu::CUBEFACE_NEGATIVE_X;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        return tcu::CUBEFACE_POSITIVE_X;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        return tcu::CUBEFACE_NEGATIVE_Y;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        return tcu::CUBEFACE_POSITIVE_Y;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        return tcu::CUBEFACE_NEGATIVE_Z;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        return tcu::CUBEFACE_POSITIVE_Z;
    default:
        throw tcu::InternalError("Can't map cube face");
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Get GLSL sampler type for texture format.
 *
 * If no mapping is found, glu::TYPE_LAST is returned.
 *
 * \param format Texture format
 * \return GLSL 1D sampler type for format
 *//*--------------------------------------------------------------------*/
DataType getSampler1DType(tcu::TextureFormat format)
{
    using tcu::TextureFormat;

    if (format.order == TextureFormat::D || format.order == TextureFormat::DS)
        return TYPE_SAMPLER_1D;

    if (format.order == TextureFormat::S)
        return TYPE_LAST;

    switch (tcu::getTextureChannelClass(format.type))
    {
    case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
    case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
    case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
        return glu::TYPE_SAMPLER_1D;

    case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
        return glu::TYPE_INT_SAMPLER_1D;

    case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
        return glu::TYPE_UINT_SAMPLER_1D;

    default:
        return glu::TYPE_LAST;
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Get GLSL sampler type for texture format.
 *
 * If no mapping is found, glu::TYPE_LAST is returned.
 *
 * \param format Texture format
 * \return GLSL 2D sampler type for format
 *//*--------------------------------------------------------------------*/
DataType getSampler2DType(tcu::TextureFormat format)
{
    using tcu::TextureFormat;

    if (format.order == TextureFormat::D || format.order == TextureFormat::DS)
        return TYPE_SAMPLER_2D;

    if (format.order == TextureFormat::S)
        return TYPE_LAST;

    switch (tcu::getTextureChannelClass(format.type))
    {
    case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
    case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
    case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
        return glu::TYPE_SAMPLER_2D;

    case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
        return glu::TYPE_INT_SAMPLER_2D;

    case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
        return glu::TYPE_UINT_SAMPLER_2D;

    default:
        return glu::TYPE_LAST;
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Get GLSL sampler type for texture format.
 *
 * If no mapping is found, glu::TYPE_LAST is returned.
 *
 * \param format Texture format
 * \return GLSL cube map sampler type for format
 *//*--------------------------------------------------------------------*/
DataType getSamplerCubeType(tcu::TextureFormat format)
{
    using tcu::TextureFormat;

    if (format.order == TextureFormat::D || format.order == TextureFormat::DS)
        return TYPE_SAMPLER_CUBE;

    if (format.order == TextureFormat::S)
        return TYPE_LAST;

    switch (tcu::getTextureChannelClass(format.type))
    {
    case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
    case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
    case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
        return glu::TYPE_SAMPLER_CUBE;

    case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
        return glu::TYPE_INT_SAMPLER_CUBE;

    case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
        return glu::TYPE_UINT_SAMPLER_CUBE;

    default:
        return glu::TYPE_LAST;
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Get GLSL sampler type for texture format.
 *
 * If no mapping is found, glu::TYPE_LAST is returned.
 *
 * \param format Texture format
 * \return GLSL 1D array sampler type for format
 *//*--------------------------------------------------------------------*/
DataType getSampler1DArrayType(tcu::TextureFormat format)
{
    using tcu::TextureFormat;

    if (format.order == TextureFormat::D || format.order == TextureFormat::DS)
        return TYPE_SAMPLER_1D_ARRAY;

    if (format.order == TextureFormat::S)
        return TYPE_LAST;

    switch (tcu::getTextureChannelClass(format.type))
    {
    case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
    case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
    case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
        return glu::TYPE_SAMPLER_1D_ARRAY;

    case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
        return glu::TYPE_INT_SAMPLER_1D_ARRAY;

    case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
        return glu::TYPE_UINT_SAMPLER_1D_ARRAY;

    default:
        return glu::TYPE_LAST;
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Get GLSL sampler type for texture format.
 *
 * If no mapping is found, glu::TYPE_LAST is returned.
 *
 * \param format Texture format
 * \return GLSL 2D array sampler type for format
 *//*--------------------------------------------------------------------*/
DataType getSampler2DArrayType(tcu::TextureFormat format)
{
    using tcu::TextureFormat;

    if (format.order == TextureFormat::D || format.order == TextureFormat::DS)
        return TYPE_SAMPLER_2D_ARRAY;

    if (format.order == TextureFormat::S)
        return TYPE_LAST;

    switch (tcu::getTextureChannelClass(format.type))
    {
    case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
    case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
    case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
        return glu::TYPE_SAMPLER_2D_ARRAY;

    case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
        return glu::TYPE_INT_SAMPLER_2D_ARRAY;

    case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
        return glu::TYPE_UINT_SAMPLER_2D_ARRAY;

    default:
        return glu::TYPE_LAST;
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Get GLSL sampler type for texture format.
 *
 * If no mapping is found, glu::TYPE_LAST is returned.
 *
 * \param format Texture format
 * \return GLSL 3D sampler type for format
 *//*--------------------------------------------------------------------*/
DataType getSampler3DType(tcu::TextureFormat format)
{
    using tcu::TextureFormat;

    if (format.order == TextureFormat::D || format.order == TextureFormat::DS)
        return TYPE_SAMPLER_3D;

    if (format.order == TextureFormat::S)
        return TYPE_LAST;

    switch (tcu::getTextureChannelClass(format.type))
    {
    case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
    case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
    case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
        return glu::TYPE_SAMPLER_3D;

    case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
        return glu::TYPE_INT_SAMPLER_3D;

    case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
        return glu::TYPE_UINT_SAMPLER_3D;

    default:
        return glu::TYPE_LAST;
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Get GLSL sampler type for texture format.
 *
 * If no mapping is found, glu::TYPE_LAST is returned.
 *
 * \param format Texture format
 * \return GLSL cube map array sampler type for format
 *//*--------------------------------------------------------------------*/
DataType getSamplerCubeArrayType(tcu::TextureFormat format)
{
    using tcu::TextureFormat;

    if (format.order == TextureFormat::D || format.order == TextureFormat::DS)
        return TYPE_SAMPLER_CUBE_ARRAY;

    if (format.order == TextureFormat::S)
        return TYPE_LAST;

    switch (tcu::getTextureChannelClass(format.type))
    {
    case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
    case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
    case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
        return glu::TYPE_SAMPLER_CUBE_ARRAY;

    case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
        return glu::TYPE_INT_SAMPLER_CUBE_ARRAY;

    case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
        return glu::TYPE_UINT_SAMPLER_CUBE_ARRAY;

    default:
        return glu::TYPE_LAST;
    }
}

enum RenderableType
{
    RENDERABLE_COLOR   = (1 << 0),
    RENDERABLE_DEPTH   = (1 << 1),
    RENDERABLE_STENCIL = (1 << 2)
};

static uint32_t getRenderableBitsES3(const ContextInfo &contextInfo, uint32_t internalFormat)
{
    switch (internalFormat)
    {
    // Color-renderable formats
    case GL_RGBA32I:
    case GL_RGBA32UI:
    case GL_RGBA16I:
    case GL_RGBA16UI:
    case GL_RGBA8:
    case GL_RGBA8I:
    case GL_RGBA8UI:
    case GL_SRGB8_ALPHA8:
    case GL_RGB10_A2:
    case GL_RGB10_A2UI:
    case GL_RGBA4:
    case GL_RGB5_A1:
    case GL_RGB8:
    case GL_RGB565:
    case GL_RG32I:
    case GL_RG32UI:
    case GL_RG16I:
    case GL_RG16UI:
    case GL_RG8:
    case GL_RG8I:
    case GL_RG8UI:
    case GL_R32I:
    case GL_R32UI:
    case GL_R16I:
    case GL_R16UI:
    case GL_R8:
    case GL_R8I:
    case GL_R8UI:
        return RENDERABLE_COLOR;

    // GL_EXT_color_buffer_float
    case GL_RGBA32F:
    case GL_R11F_G11F_B10F:
    case GL_RG32F:
    case GL_R32F:
        if (contextInfo.isExtensionSupported("GL_EXT_color_buffer_float"))
            return RENDERABLE_COLOR;
        else
            return 0;

    // GL_EXT_color_buffer_float / GL_EXT_color_buffer_half_float
    case GL_RGBA16F:
    case GL_RG16F:
    case GL_R16F:
        if (contextInfo.isExtensionSupported("GL_EXT_color_buffer_float") ||
            contextInfo.isExtensionSupported("GL_EXT_color_buffer_half_float"))
            return RENDERABLE_COLOR;
        else
            return 0;

    // Depth formats
    case GL_DEPTH_COMPONENT32F:
    case GL_DEPTH_COMPONENT24:
    case GL_DEPTH_COMPONENT16:
        return RENDERABLE_DEPTH;

    // Depth+stencil formats
    case GL_DEPTH32F_STENCIL8:
    case GL_DEPTH24_STENCIL8:
        return RENDERABLE_DEPTH | RENDERABLE_STENCIL;

    // Stencil formats
    case GL_STENCIL_INDEX8:
        return RENDERABLE_STENCIL;

    default:
        return 0;
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Check if sized internal format is color-renderable.
 * \note Works currently only on ES3 context.
 *//*--------------------------------------------------------------------*/
bool isSizedFormatColorRenderable(const RenderContext &renderCtx, const ContextInfo &contextInfo, uint32_t sizedFormat)
{
    uint32_t renderable = 0;

    if (renderCtx.getType().getAPI() == ApiType::es(3, 0))
        renderable = getRenderableBitsES3(contextInfo, sizedFormat);
    else
        throw tcu::InternalError("Context type not supported in query");

    return (renderable & RENDERABLE_COLOR) != 0;
}

const tcu::IVec2 (&getDefaultGatherOffsets(void))[4]
{
    static const tcu::IVec2 s_defaultOffsets[4] = {
        tcu::IVec2(0, 1),
        tcu::IVec2(1, 1),
        tcu::IVec2(1, 0),
        tcu::IVec2(0, 0),
    };
    return s_defaultOffsets;
}

tcu::PixelBufferAccess getTextureBufferEffectiveRefTexture(TextureBuffer &buffer, int maxTextureBufferSize)
{
    DE_ASSERT(maxTextureBufferSize > 0);

    const tcu::PixelBufferAccess &fullAccess = buffer.getFullRefTexture();

    return tcu::PixelBufferAccess(fullAccess.getFormat(),
                                  tcu::IVec3(de::min(fullAccess.getWidth(), maxTextureBufferSize), 1, 1),
                                  fullAccess.getPitch(), fullAccess.getDataPtr());
}

tcu::ConstPixelBufferAccess getTextureBufferEffectiveRefTexture(const TextureBuffer &buffer, int maxTextureBufferSize)
{
    return getTextureBufferEffectiveRefTexture(const_cast<TextureBuffer &>(buffer), maxTextureBufferSize);
}

} // namespace glu
