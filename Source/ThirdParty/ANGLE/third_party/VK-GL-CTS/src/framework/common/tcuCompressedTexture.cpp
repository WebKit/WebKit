/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
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
 * \brief Compressed Texture Utilities.
 *//*--------------------------------------------------------------------*/

#include "tcuCompressedTexture.hpp"
#include "tcuTextureUtil.hpp"
#include "tcuAstcUtil.hpp"

#include "deStringUtil.hpp"
#include "deFloat16.h"

#include <algorithm>

namespace tcu
{

int getBlockSize(CompressedTexFormat format)
{
    if (isAstcFormat(format))
    {
        return astc::BLOCK_SIZE_BYTES;
    }
    else if (isEtcFormat(format))
    {
        switch (format)
        {
        case COMPRESSEDTEXFORMAT_ETC1_RGB8:
            return 8;
        case COMPRESSEDTEXFORMAT_EAC_R11:
            return 8;
        case COMPRESSEDTEXFORMAT_EAC_SIGNED_R11:
            return 8;
        case COMPRESSEDTEXFORMAT_EAC_RG11:
            return 16;
        case COMPRESSEDTEXFORMAT_EAC_SIGNED_RG11:
            return 16;
        case COMPRESSEDTEXFORMAT_ETC2_RGB8:
            return 8;
        case COMPRESSEDTEXFORMAT_ETC2_SRGB8:
            return 8;
        case COMPRESSEDTEXFORMAT_ETC2_RGB8_PUNCHTHROUGH_ALPHA1:
            return 8;
        case COMPRESSEDTEXFORMAT_ETC2_SRGB8_PUNCHTHROUGH_ALPHA1:
            return 8;
        case COMPRESSEDTEXFORMAT_ETC2_EAC_RGBA8:
            return 16;
        case COMPRESSEDTEXFORMAT_ETC2_EAC_SRGB8_ALPHA8:
            return 16;

        default:
            DE_ASSERT(false);
            return -1;
        }
    }
    else if (isBcFormat(format))
    {
        switch (format)
        {
        case COMPRESSEDTEXFORMAT_BC1_RGB_UNORM_BLOCK:
            return 8;
        case COMPRESSEDTEXFORMAT_BC1_RGB_SRGB_BLOCK:
            return 8;
        case COMPRESSEDTEXFORMAT_BC1_RGBA_UNORM_BLOCK:
            return 8;
        case COMPRESSEDTEXFORMAT_BC1_RGBA_SRGB_BLOCK:
            return 8;
        case COMPRESSEDTEXFORMAT_BC2_UNORM_BLOCK:
            return 16;
        case COMPRESSEDTEXFORMAT_BC2_SRGB_BLOCK:
            return 16;
        case COMPRESSEDTEXFORMAT_BC3_UNORM_BLOCK:
            return 16;
        case COMPRESSEDTEXFORMAT_BC3_SRGB_BLOCK:
            return 16;
        case COMPRESSEDTEXFORMAT_BC4_UNORM_BLOCK:
            return 8;
        case COMPRESSEDTEXFORMAT_BC4_SNORM_BLOCK:
            return 8;
        case COMPRESSEDTEXFORMAT_BC5_UNORM_BLOCK:
            return 16;
        case COMPRESSEDTEXFORMAT_BC5_SNORM_BLOCK:
            return 16;
        case COMPRESSEDTEXFORMAT_BC6H_UFLOAT_BLOCK:
            return 16;
        case COMPRESSEDTEXFORMAT_BC6H_SFLOAT_BLOCK:
            return 16;
        case COMPRESSEDTEXFORMAT_BC7_UNORM_BLOCK:
            return 16;
        case COMPRESSEDTEXFORMAT_BC7_SRGB_BLOCK:
            return 16;

        default:
            DE_ASSERT(false);
            return -1;
        }
    }
    else if (isAhbRawFormat(format))
    {
        switch (format)
        {
        case COMPRESSEDTEXFORMAT_AHB_RAW10:
            return 5;
        case COMPRESSEDTEXFORMAT_AHB_RAW12:
            return 3;

        default:
            DE_ASSERT(false);
            return -1;
        }
    }
    else
    {
        DE_ASSERT(false);
        return -1;
    }
}

IVec3 getBlockPixelSize(CompressedTexFormat format)
{
    if (isEtcFormat(format))
    {
        return IVec3(4, 4, 1);
    }
    else if (isAstcFormat(format))
    {
        switch (format)
        {
        case COMPRESSEDTEXFORMAT_ASTC_4x4_RGBA:
        case COMPRESSEDTEXFORMAT_ASTC_4x4_SFLOAT_BLOCK:
            return IVec3(4, 4, 1);
        case COMPRESSEDTEXFORMAT_ASTC_5x4_RGBA:
        case COMPRESSEDTEXFORMAT_ASTC_5x4_SFLOAT_BLOCK:
            return IVec3(5, 4, 1);
        case COMPRESSEDTEXFORMAT_ASTC_5x5_RGBA:
        case COMPRESSEDTEXFORMAT_ASTC_5x5_SFLOAT_BLOCK:
            return IVec3(5, 5, 1);
        case COMPRESSEDTEXFORMAT_ASTC_6x5_RGBA:
        case COMPRESSEDTEXFORMAT_ASTC_6x5_SFLOAT_BLOCK:
            return IVec3(6, 5, 1);
        case COMPRESSEDTEXFORMAT_ASTC_6x6_RGBA:
        case COMPRESSEDTEXFORMAT_ASTC_6x6_SFLOAT_BLOCK:
            return IVec3(6, 6, 1);
        case COMPRESSEDTEXFORMAT_ASTC_8x5_RGBA:
        case COMPRESSEDTEXFORMAT_ASTC_8x5_SFLOAT_BLOCK:
            return IVec3(8, 5, 1);
        case COMPRESSEDTEXFORMAT_ASTC_8x6_RGBA:
        case COMPRESSEDTEXFORMAT_ASTC_8x6_SFLOAT_BLOCK:
            return IVec3(8, 6, 1);
        case COMPRESSEDTEXFORMAT_ASTC_8x8_RGBA:
        case COMPRESSEDTEXFORMAT_ASTC_8x8_SFLOAT_BLOCK:
            return IVec3(8, 8, 1);
        case COMPRESSEDTEXFORMAT_ASTC_10x5_RGBA:
        case COMPRESSEDTEXFORMAT_ASTC_10x5_SFLOAT_BLOCK:
            return IVec3(10, 5, 1);
        case COMPRESSEDTEXFORMAT_ASTC_10x6_RGBA:
        case COMPRESSEDTEXFORMAT_ASTC_10x6_SFLOAT_BLOCK:
            return IVec3(10, 6, 1);
        case COMPRESSEDTEXFORMAT_ASTC_10x8_RGBA:
        case COMPRESSEDTEXFORMAT_ASTC_10x8_SFLOAT_BLOCK:
            return IVec3(10, 8, 1);
        case COMPRESSEDTEXFORMAT_ASTC_10x10_RGBA:
        case COMPRESSEDTEXFORMAT_ASTC_10x10_SFLOAT_BLOCK:
            return IVec3(10, 10, 1);
        case COMPRESSEDTEXFORMAT_ASTC_12x10_RGBA:
        case COMPRESSEDTEXFORMAT_ASTC_12x10_SFLOAT_BLOCK:
            return IVec3(12, 10, 1);
        case COMPRESSEDTEXFORMAT_ASTC_12x12_RGBA:
        case COMPRESSEDTEXFORMAT_ASTC_12x12_SFLOAT_BLOCK:
            return IVec3(12, 12, 1);
        case COMPRESSEDTEXFORMAT_ASTC_4x4_SRGB8_ALPHA8:
            return IVec3(4, 4, 1);
        case COMPRESSEDTEXFORMAT_ASTC_5x4_SRGB8_ALPHA8:
            return IVec3(5, 4, 1);
        case COMPRESSEDTEXFORMAT_ASTC_5x5_SRGB8_ALPHA8:
            return IVec3(5, 5, 1);
        case COMPRESSEDTEXFORMAT_ASTC_6x5_SRGB8_ALPHA8:
            return IVec3(6, 5, 1);
        case COMPRESSEDTEXFORMAT_ASTC_6x6_SRGB8_ALPHA8:
            return IVec3(6, 6, 1);
        case COMPRESSEDTEXFORMAT_ASTC_8x5_SRGB8_ALPHA8:
            return IVec3(8, 5, 1);
        case COMPRESSEDTEXFORMAT_ASTC_8x6_SRGB8_ALPHA8:
            return IVec3(8, 6, 1);
        case COMPRESSEDTEXFORMAT_ASTC_8x8_SRGB8_ALPHA8:
            return IVec3(8, 8, 1);
        case COMPRESSEDTEXFORMAT_ASTC_10x5_SRGB8_ALPHA8:
            return IVec3(10, 5, 1);
        case COMPRESSEDTEXFORMAT_ASTC_10x6_SRGB8_ALPHA8:
            return IVec3(10, 6, 1);
        case COMPRESSEDTEXFORMAT_ASTC_10x8_SRGB8_ALPHA8:
            return IVec3(10, 8, 1);
        case COMPRESSEDTEXFORMAT_ASTC_10x10_SRGB8_ALPHA8:
            return IVec3(10, 10, 1);
        case COMPRESSEDTEXFORMAT_ASTC_12x10_SRGB8_ALPHA8:
            return IVec3(12, 10, 1);
        case COMPRESSEDTEXFORMAT_ASTC_12x12_SRGB8_ALPHA8:
            return IVec3(12, 12, 1);
        case COMPRESSEDTEXFORMAT_ASTC_3x3x3_UNORM_BLOCK_EXT:
            return IVec3(3, 3, 3);
        case COMPRESSEDTEXFORMAT_ASTC_3x3x3_SRGB_BLOCK_EXT:
            return IVec3(3, 3, 3);
        case COMPRESSEDTEXFORMAT_ASTC_3x3x3_SFLOAT_BLOCK_EXT:
            return IVec3(3, 3, 3);
        case COMPRESSEDTEXFORMAT_ASTC_4x3x3_UNORM_BLOCK_EXT:
            return IVec3(4, 3, 3);
        case COMPRESSEDTEXFORMAT_ASTC_4x3x3_SRGB_BLOCK_EXT:
            return IVec3(4, 3, 3);
        case COMPRESSEDTEXFORMAT_ASTC_4x3x3_SFLOAT_BLOCK_EXT:
            return IVec3(4, 3, 3);
        case COMPRESSEDTEXFORMAT_ASTC_4x4x3_UNORM_BLOCK_EXT:
            return IVec3(4, 4, 3);
        case COMPRESSEDTEXFORMAT_ASTC_4x4x3_SRGB_BLOCK_EXT:
            return IVec3(4, 4, 3);
        case COMPRESSEDTEXFORMAT_ASTC_4x4x3_SFLOAT_BLOCK_EXT:
            return IVec3(4, 4, 3);
        case COMPRESSEDTEXFORMAT_ASTC_4x4x4_UNORM_BLOCK_EXT:
            return IVec3(4, 4, 4);
        case COMPRESSEDTEXFORMAT_ASTC_4x4x4_SRGB_BLOCK_EXT:
            return IVec3(4, 4, 4);
        case COMPRESSEDTEXFORMAT_ASTC_4x4x4_SFLOAT_BLOCK_EXT:
            return IVec3(4, 4, 4);
        case COMPRESSEDTEXFORMAT_ASTC_5x4x4_UNORM_BLOCK_EXT:
            return IVec3(5, 4, 4);
        case COMPRESSEDTEXFORMAT_ASTC_5x4x4_SRGB_BLOCK_EXT:
            return IVec3(5, 4, 4);
        case COMPRESSEDTEXFORMAT_ASTC_5x4x4_SFLOAT_BLOCK_EXT:
            return IVec3(5, 4, 4);
        case COMPRESSEDTEXFORMAT_ASTC_5x5x4_UNORM_BLOCK_EXT:
            return IVec3(5, 5, 4);
        case COMPRESSEDTEXFORMAT_ASTC_5x5x4_SRGB_BLOCK_EXT:
            return IVec3(5, 5, 4);
        case COMPRESSEDTEXFORMAT_ASTC_5x5x4_SFLOAT_BLOCK_EXT:
            return IVec3(5, 5, 4);
        case COMPRESSEDTEXFORMAT_ASTC_5x5x5_UNORM_BLOCK_EXT:
            return IVec3(5, 5, 5);
        case COMPRESSEDTEXFORMAT_ASTC_5x5x5_SRGB_BLOCK_EXT:
            return IVec3(5, 5, 5);
        case COMPRESSEDTEXFORMAT_ASTC_5x5x5_SFLOAT_BLOCK_EXT:
            return IVec3(5, 5, 5);
        case COMPRESSEDTEXFORMAT_ASTC_6x5x5_UNORM_BLOCK_EXT:
            return IVec3(6, 5, 5);
        case COMPRESSEDTEXFORMAT_ASTC_6x5x5_SRGB_BLOCK_EXT:
            return IVec3(6, 5, 5);
        case COMPRESSEDTEXFORMAT_ASTC_6x5x5_SFLOAT_BLOCK_EXT:
            return IVec3(6, 5, 5);
        case COMPRESSEDTEXFORMAT_ASTC_6x6x5_UNORM_BLOCK_EXT:
            return IVec3(6, 6, 5);
        case COMPRESSEDTEXFORMAT_ASTC_6x6x5_SRGB_BLOCK_EXT:
            return IVec3(6, 6, 5);
        case COMPRESSEDTEXFORMAT_ASTC_6x6x5_SFLOAT_BLOCK_EXT:
            return IVec3(6, 6, 5);
        case COMPRESSEDTEXFORMAT_ASTC_6x6x6_UNORM_BLOCK_EXT:
            return IVec3(6, 6, 6);
        case COMPRESSEDTEXFORMAT_ASTC_6x6x6_SRGB_BLOCK_EXT:
            return IVec3(6, 6, 6);
        case COMPRESSEDTEXFORMAT_ASTC_6x6x6_SFLOAT_BLOCK_EXT:
            return IVec3(6, 6, 6);

        default:
            DE_ASSERT(false);
            return IVec3();
        }
    }
    else if (isBcFormat(format))
    {
        return IVec3(4, 4, 1);
    }
    else if (isAhbRawFormat(format))
    {
        switch (format)
        {
        case COMPRESSEDTEXFORMAT_AHB_RAW10:
            return IVec3(4, 1, 1);
        case COMPRESSEDTEXFORMAT_AHB_RAW12:
            return IVec3(2, 1, 1);

        default:
            DE_ASSERT(false);
            return IVec3();
        }
    }
    else
    {
        DE_ASSERT(false);
        return IVec3(-1);
    }
}

bool isEtcFormat(CompressedTexFormat format)
{
    switch (format)
    {
    case COMPRESSEDTEXFORMAT_ETC1_RGB8:
    case COMPRESSEDTEXFORMAT_EAC_R11:
    case COMPRESSEDTEXFORMAT_EAC_SIGNED_R11:
    case COMPRESSEDTEXFORMAT_EAC_RG11:
    case COMPRESSEDTEXFORMAT_EAC_SIGNED_RG11:
    case COMPRESSEDTEXFORMAT_ETC2_RGB8:
    case COMPRESSEDTEXFORMAT_ETC2_SRGB8:
    case COMPRESSEDTEXFORMAT_ETC2_RGB8_PUNCHTHROUGH_ALPHA1:
    case COMPRESSEDTEXFORMAT_ETC2_SRGB8_PUNCHTHROUGH_ALPHA1:
    case COMPRESSEDTEXFORMAT_ETC2_EAC_RGBA8:
    case COMPRESSEDTEXFORMAT_ETC2_EAC_SRGB8_ALPHA8:
        return true;

    default:
        return false;
    }
}

bool isBcFormat(CompressedTexFormat format)
{
    switch (format)
    {
    case COMPRESSEDTEXFORMAT_BC1_RGB_UNORM_BLOCK:
    case COMPRESSEDTEXFORMAT_BC1_RGB_SRGB_BLOCK:
    case COMPRESSEDTEXFORMAT_BC1_RGBA_UNORM_BLOCK:
    case COMPRESSEDTEXFORMAT_BC1_RGBA_SRGB_BLOCK:
    case COMPRESSEDTEXFORMAT_BC2_UNORM_BLOCK:
    case COMPRESSEDTEXFORMAT_BC2_SRGB_BLOCK:
    case COMPRESSEDTEXFORMAT_BC3_UNORM_BLOCK:
    case COMPRESSEDTEXFORMAT_BC3_SRGB_BLOCK:
    case COMPRESSEDTEXFORMAT_BC4_UNORM_BLOCK:
    case COMPRESSEDTEXFORMAT_BC4_SNORM_BLOCK:
    case COMPRESSEDTEXFORMAT_BC5_UNORM_BLOCK:
    case COMPRESSEDTEXFORMAT_BC5_SNORM_BLOCK:
    case COMPRESSEDTEXFORMAT_BC6H_UFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_BC6H_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_BC7_UNORM_BLOCK:
    case COMPRESSEDTEXFORMAT_BC7_SRGB_BLOCK:
        return true;

    default:
        return false;
    }
}

bool isBcBitExactFormat(CompressedTexFormat format)
{
    switch (format)
    {
    case COMPRESSEDTEXFORMAT_BC6H_UFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_BC6H_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_BC7_UNORM_BLOCK:
    case COMPRESSEDTEXFORMAT_BC7_SRGB_BLOCK:
        return true;

    default:
        return false;
    }
}

bool isBcSRGBFormat(CompressedTexFormat format)
{
    switch (format)
    {
    case COMPRESSEDTEXFORMAT_BC1_RGB_SRGB_BLOCK:
    case COMPRESSEDTEXFORMAT_BC1_RGBA_SRGB_BLOCK:
    case COMPRESSEDTEXFORMAT_BC2_SRGB_BLOCK:
    case COMPRESSEDTEXFORMAT_BC3_SRGB_BLOCK:
    case COMPRESSEDTEXFORMAT_BC7_SRGB_BLOCK:
        return true;

    default:
        return false;
    }
}

bool isAstcFormat(CompressedTexFormat format)
{
    switch (format)
    {
    case COMPRESSEDTEXFORMAT_ASTC_4x4_RGBA:
    case COMPRESSEDTEXFORMAT_ASTC_5x4_RGBA:
    case COMPRESSEDTEXFORMAT_ASTC_5x5_RGBA:
    case COMPRESSEDTEXFORMAT_ASTC_6x5_RGBA:
    case COMPRESSEDTEXFORMAT_ASTC_6x6_RGBA:
    case COMPRESSEDTEXFORMAT_ASTC_8x5_RGBA:
    case COMPRESSEDTEXFORMAT_ASTC_8x6_RGBA:
    case COMPRESSEDTEXFORMAT_ASTC_8x8_RGBA:
    case COMPRESSEDTEXFORMAT_ASTC_10x5_RGBA:
    case COMPRESSEDTEXFORMAT_ASTC_10x6_RGBA:
    case COMPRESSEDTEXFORMAT_ASTC_10x8_RGBA:
    case COMPRESSEDTEXFORMAT_ASTC_10x10_RGBA:
    case COMPRESSEDTEXFORMAT_ASTC_12x10_RGBA:
    case COMPRESSEDTEXFORMAT_ASTC_12x12_RGBA:
    case COMPRESSEDTEXFORMAT_ASTC_4x4_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_5x4_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_5x5_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_6x5_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_6x6_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_8x5_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_8x6_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_8x8_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_10x5_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_10x6_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_10x8_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_10x10_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_12x10_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_12x12_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_3x3x3_UNORM_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_3x3x3_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_3x3x3_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x3x3_UNORM_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x3x3_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x3x3_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x4x3_UNORM_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x4x3_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x4x3_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x4x4_UNORM_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x4x4_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x4x4_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x4x4_UNORM_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x4x4_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x4x4_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x5x4_UNORM_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x5x4_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x5x4_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x5x5_UNORM_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x5x5_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x5x5_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x5x5_UNORM_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x5x5_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x5x5_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x6x5_UNORM_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x6x5_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x6x5_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x6x6_UNORM_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x6x6_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x6x6_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x4_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_5x4_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_5x5_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_6x5_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_6x6_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_8x5_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_8x6_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_8x8_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_10x5_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_10x6_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_10x8_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_10x10_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_12x10_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_12x12_SFLOAT_BLOCK:
        return true;

    default:
        return false;
    }
}

bool isAstcSRGBFormat(CompressedTexFormat format)
{
    switch (format)
    {
    case COMPRESSEDTEXFORMAT_ASTC_4x4_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_5x4_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_5x5_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_6x5_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_6x6_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_8x5_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_8x6_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_8x8_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_10x5_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_10x6_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_10x8_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_10x10_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_12x10_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_12x12_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_3x3x3_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x3x3_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x4x3_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x4x4_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x4x4_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x5x4_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x5x5_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x5x5_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x6x5_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x6x6_SRGB_BLOCK_EXT:
        return true;

    default:
        return false;
    }
}

bool isAstcSFLOATFormat(CompressedTexFormat format)
{
    switch (format)
    {
    case COMPRESSEDTEXFORMAT_ASTC_3x3x3_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x3x3_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x4x3_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x4x4_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x4x4_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x5x4_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x5x5_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x5x5_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x6x5_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x6x6_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x4_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_5x4_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_5x5_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_6x5_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_6x6_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_8x5_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_8x6_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_8x8_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_10x5_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_10x6_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_10x8_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_10x10_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_12x10_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_12x12_SFLOAT_BLOCK:
        return true;
    default:
        return false;
    }
}

bool isAstc3DFormat(CompressedTexFormat format)
{
    switch (format)
    {
    case COMPRESSEDTEXFORMAT_ASTC_3x3x3_UNORM_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_3x3x3_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_3x3x3_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x3x3_UNORM_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x3x3_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x3x3_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x4x3_UNORM_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x4x3_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x4x3_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x4x4_UNORM_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x4x4_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x4x4_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x4x4_UNORM_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x4x4_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x4x4_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x5x4_UNORM_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x5x4_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x5x4_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x5x5_UNORM_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x5x5_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x5x5_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x5x5_UNORM_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x5x5_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x5x5_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x6x5_UNORM_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x6x5_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x6x5_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x6x6_UNORM_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x6x6_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x6x6_SFLOAT_BLOCK_EXT:
        return true;

    default:
        return false;
    }
}

bool isAstcHdrFormat(CompressedTexFormat format)
{
    switch (format)
    {
    case COMPRESSEDTEXFORMAT_ASTC_4x4_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_5x4_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_5x5_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_6x5_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_6x6_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_8x5_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_8x6_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_8x8_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_10x5_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_10x6_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_10x8_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_10x10_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_12x10_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_12x12_SFLOAT_BLOCK:
        return true;

    default:
        return false;
    }
}

bool isAhbRawFormat(CompressedTexFormat format)
{
    switch (format)
    {
    case COMPRESSEDTEXFORMAT_AHB_RAW10:
    case COMPRESSEDTEXFORMAT_AHB_RAW12:
        return true;

    default:
        return false;
    }
}

TextureFormat getUncompressedFormat(CompressedTexFormat format)
{
    if (isEtcFormat(format))
    {
        switch (format)
        {
        case COMPRESSEDTEXFORMAT_ETC1_RGB8:
            return TextureFormat(TextureFormat::RGB, TextureFormat::UNORM_INT8);
        case COMPRESSEDTEXFORMAT_EAC_R11:
            return TextureFormat(TextureFormat::R, TextureFormat::UNORM_INT16);
        case COMPRESSEDTEXFORMAT_EAC_SIGNED_R11:
            return TextureFormat(TextureFormat::R, TextureFormat::SNORM_INT16);
        case COMPRESSEDTEXFORMAT_EAC_RG11:
            return TextureFormat(TextureFormat::RG, TextureFormat::UNORM_INT16);
        case COMPRESSEDTEXFORMAT_EAC_SIGNED_RG11:
            return TextureFormat(TextureFormat::RG, TextureFormat::SNORM_INT16);
        case COMPRESSEDTEXFORMAT_ETC2_RGB8:
            return TextureFormat(TextureFormat::RGB, TextureFormat::UNORM_INT8);
        case COMPRESSEDTEXFORMAT_ETC2_SRGB8:
            return TextureFormat(TextureFormat::sRGB, TextureFormat::UNORM_INT8);
        case COMPRESSEDTEXFORMAT_ETC2_RGB8_PUNCHTHROUGH_ALPHA1:
            return TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8);
        case COMPRESSEDTEXFORMAT_ETC2_SRGB8_PUNCHTHROUGH_ALPHA1:
            return TextureFormat(TextureFormat::sRGBA, TextureFormat::UNORM_INT8);
        case COMPRESSEDTEXFORMAT_ETC2_EAC_RGBA8:
            return TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8);
        case COMPRESSEDTEXFORMAT_ETC2_EAC_SRGB8_ALPHA8:
            return TextureFormat(TextureFormat::sRGBA, TextureFormat::UNORM_INT8);

        default:
            DE_ASSERT(false);
            return TextureFormat();
        }
    }
    else if (isAstcFormat(format))
    {
        if (isAstcSRGBFormat(format))
            return TextureFormat(TextureFormat::sRGBA, TextureFormat::UNORM_INT8);
        else
            return TextureFormat(TextureFormat::RGBA, TextureFormat::HALF_FLOAT);
    }
    else if (isBcFormat(format))
    {
        if (format == COMPRESSEDTEXFORMAT_BC4_UNORM_BLOCK || format == COMPRESSEDTEXFORMAT_BC4_SNORM_BLOCK)
            return TextureFormat(TextureFormat::R, TextureFormat::FLOAT);
        else if (format == COMPRESSEDTEXFORMAT_BC5_UNORM_BLOCK || format == COMPRESSEDTEXFORMAT_BC5_SNORM_BLOCK)
            return TextureFormat(TextureFormat::RG, TextureFormat::FLOAT);
        else if (format == COMPRESSEDTEXFORMAT_BC6H_UFLOAT_BLOCK || format == COMPRESSEDTEXFORMAT_BC6H_SFLOAT_BLOCK)
            return TextureFormat(TextureFormat::RGB, TextureFormat::HALF_FLOAT);
        else if (isBcSRGBFormat(format))
            return TextureFormat(TextureFormat::sRGBA, TextureFormat::UNORM_INT8);
        else
            return TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_INT8);
    }
    else if (isAhbRawFormat(format))
    {
        if (format == COMPRESSEDTEXFORMAT_AHB_RAW10)
            return TextureFormat(TextureFormat::R,
                                 TextureFormat::UNORM_SHORT_10); // Can be changed to a more fitting value if needed
        else                                                     // COMPRESSEDTEXFORMAT_AHB_RAW12
            return TextureFormat(TextureFormat::R,
                                 TextureFormat::UNORM_SHORT_12); // Can be changed to a more fitting value if needed
    }
    else
    {
        DE_ASSERT(false);
        return TextureFormat();
    }
}

CompressedTexFormat getAstcFormatByBlockSize(const IVec3 &size, bool isSRGB)
{
    if (size.z() > 1)
        throw InternalError("3D ASTC textures not currently supported");

    for (int fmtI = 0; fmtI < COMPRESSEDTEXFORMAT_LAST; fmtI++)
    {
        const CompressedTexFormat fmt = (CompressedTexFormat)fmtI;

        if (isAstcFormat(fmt) && getBlockPixelSize(fmt) == size && isAstcSRGBFormat(fmt) == isSRGB)
            return fmt;
    }

    throw InternalError("Invalid ASTC block size " + de::toString(size.x()) + "x" + de::toString(size.y()) + "x" +
                        de::toString(size.z()));
}

namespace
{

inline uint8_t extend4To8(uint8_t src)
{
    DE_ASSERT((src & ~((1 << 4) - 1)) == 0);
    return (uint8_t)((src << 4) | src);
}

inline uint8_t extend5To8(uint8_t src)
{
    DE_ASSERT((src & ~((1 << 5) - 1)) == 0);
    return (uint8_t)((src << 3) | (src >> 2));
}

inline uint8_t extend6To8(uint8_t src)
{
    DE_ASSERT((src & ~((1 << 6) - 1)) == 0);
    return (uint8_t)((src << 2) | (src >> 4));
}

// \todo [2013-08-06 nuutti] ETC and ASTC decompression codes are rather unrelated, and are already in their own "private" namespaces - should this be split to multiple files?

namespace EtcDecompressInternal
{

enum
{
    ETC2_BLOCK_WIDTH                   = 4,
    ETC2_BLOCK_HEIGHT                  = 4,
    ETC2_UNCOMPRESSED_PIXEL_SIZE_A8    = 1,
    ETC2_UNCOMPRESSED_PIXEL_SIZE_R11   = 2,
    ETC2_UNCOMPRESSED_PIXEL_SIZE_RG11  = 4,
    ETC2_UNCOMPRESSED_PIXEL_SIZE_RGB8  = 3,
    ETC2_UNCOMPRESSED_PIXEL_SIZE_RGBA8 = 4,
    ETC2_UNCOMPRESSED_BLOCK_SIZE_A8    = ETC2_BLOCK_WIDTH * ETC2_BLOCK_HEIGHT * ETC2_UNCOMPRESSED_PIXEL_SIZE_A8,
    ETC2_UNCOMPRESSED_BLOCK_SIZE_R11   = ETC2_BLOCK_WIDTH * ETC2_BLOCK_HEIGHT * ETC2_UNCOMPRESSED_PIXEL_SIZE_R11,
    ETC2_UNCOMPRESSED_BLOCK_SIZE_RG11  = ETC2_BLOCK_WIDTH * ETC2_BLOCK_HEIGHT * ETC2_UNCOMPRESSED_PIXEL_SIZE_RG11,
    ETC2_UNCOMPRESSED_BLOCK_SIZE_RGB8  = ETC2_BLOCK_WIDTH * ETC2_BLOCK_HEIGHT * ETC2_UNCOMPRESSED_PIXEL_SIZE_RGB8,
    ETC2_UNCOMPRESSED_BLOCK_SIZE_RGBA8 = ETC2_BLOCK_WIDTH * ETC2_BLOCK_HEIGHT * ETC2_UNCOMPRESSED_PIXEL_SIZE_RGBA8
};

inline uint64_t get64BitBlock(const uint8_t *src, int blockNdx)
{
    // Stored in big-endian form.
    uint64_t block = 0;

    for (int i = 0; i < 8; i++)
        block = (block << 8ull) | (uint64_t)(src[blockNdx * 8 + i]);

    return block;
}

// Return the first 64 bits of a 128 bit block.
inline uint64_t get128BitBlockStart(const uint8_t *src, int blockNdx)
{
    return get64BitBlock(src, 2 * blockNdx);
}

// Return the last 64 bits of a 128 bit block.
inline uint64_t get128BitBlockEnd(const uint8_t *src, int blockNdx)
{
    return get64BitBlock(src, 2 * blockNdx + 1);
}

inline uint32_t getBit(uint64_t src, int bit)
{
    return (src >> bit) & 1;
}

inline uint32_t getBits(uint64_t src, int low, int high)
{
    const int numBits = (high - low) + 1;
    DE_ASSERT(de::inRange(numBits, 1, 32));
    if (numBits < 32)
        return (uint32_t)((src >> low) & ((1u << numBits) - 1));
    else
        return (uint32_t)((src >> low) & 0xFFFFFFFFu);
}

inline uint8_t extend7To8(uint8_t src)
{
    DE_ASSERT((src & ~((1 << 7) - 1)) == 0);
    return (uint8_t)((src << 1) | (src >> 6));
}

inline int8_t extendSigned3To8(uint8_t src)
{
    const bool isNeg = (src & (1 << 2)) != 0;
    return (int8_t)((isNeg ? ~((1 << 3) - 1) : 0) | src);
}

inline uint8_t extend5Delta3To8(uint8_t base5, uint8_t delta3)
{
    const uint8_t t = (uint8_t)((int8_t)base5 + extendSigned3To8(delta3));
    return extend5To8(t);
}

inline uint16_t extend11To16(uint16_t src)
{
    DE_ASSERT((src & ~((1 << 11) - 1)) == 0);
    return (uint16_t)((src << 5) | (src >> 6));
}

inline int16_t extend11To16WithSign(int16_t src)
{
    if (src < 0)
        return (int16_t)(-(int16_t)extend11To16((uint16_t)(-src)));
    else
        return (int16_t)extend11To16(src);
}

void decompressETC1Block(uint8_t dst[ETC2_UNCOMPRESSED_BLOCK_SIZE_RGB8], uint64_t src)
{
    const int diffBit       = (int)getBit(src, 33);
    const int flipBit       = (int)getBit(src, 32);
    const uint32_t table[2] = {getBits(src, 37, 39), getBits(src, 34, 36)};
    uint8_t baseR[2];
    uint8_t baseG[2];
    uint8_t baseB[2];

    if (diffBit == 0)
    {
        // Individual mode.
        baseR[0] = extend4To8((uint8_t)getBits(src, 60, 63));
        baseR[1] = extend4To8((uint8_t)getBits(src, 56, 59));
        baseG[0] = extend4To8((uint8_t)getBits(src, 52, 55));
        baseG[1] = extend4To8((uint8_t)getBits(src, 48, 51));
        baseB[0] = extend4To8((uint8_t)getBits(src, 44, 47));
        baseB[1] = extend4To8((uint8_t)getBits(src, 40, 43));
    }
    else
    {
        // Differential mode (diffBit == 1).
        uint8_t bR = (uint8_t)getBits(src, 59, 63); // 5b
        uint8_t dR = (uint8_t)getBits(src, 56, 58); // 3b
        uint8_t bG = (uint8_t)getBits(src, 51, 55);
        uint8_t dG = (uint8_t)getBits(src, 48, 50);
        uint8_t bB = (uint8_t)getBits(src, 43, 47);
        uint8_t dB = (uint8_t)getBits(src, 40, 42);

        baseR[0] = extend5To8(bR);
        baseG[0] = extend5To8(bG);
        baseB[0] = extend5To8(bB);

        baseR[1] = extend5Delta3To8(bR, dR);
        baseG[1] = extend5Delta3To8(bG, dG);
        baseB[1] = extend5Delta3To8(bB, dB);
    }

    static const int modifierTable[8][4] = {//      00   01   10    11
                                            {2, 8, -2, -8},       {5, 17, -5, -17},    {9, 29, -9, -29},
                                            {13, 42, -13, -42},   {18, 60, -18, -60},  {24, 80, -24, -80},
                                            {33, 106, -33, -106}, {47, 183, -47, -183}};

    // Write final pixels.
    for (int pixelNdx = 0; pixelNdx < ETC2_BLOCK_HEIGHT * ETC2_BLOCK_WIDTH; pixelNdx++)
    {
        const int x                = pixelNdx / ETC2_BLOCK_HEIGHT;
        const int y                = pixelNdx % ETC2_BLOCK_HEIGHT;
        const int dstOffset        = (y * ETC2_BLOCK_WIDTH + x) * ETC2_UNCOMPRESSED_PIXEL_SIZE_RGB8;
        const int subBlock         = ((flipBit ? y : x) >= 2) ? 1 : 0;
        const uint32_t tableNdx    = table[subBlock];
        const uint32_t modifierNdx = (getBit(src, 16 + pixelNdx) << 1) | getBit(src, pixelNdx);
        const int modifier         = modifierTable[tableNdx][modifierNdx];

        dst[dstOffset + 0] = (uint8_t)deClamp32((int)baseR[subBlock] + modifier, 0, 255);
        dst[dstOffset + 1] = (uint8_t)deClamp32((int)baseG[subBlock] + modifier, 0, 255);
        dst[dstOffset + 2] = (uint8_t)deClamp32((int)baseB[subBlock] + modifier, 0, 255);
    }
}

// if alphaMode is true, do PUNCHTHROUGH and store alpha to alphaDst; otherwise do ordinary ETC2 RGB8.
void decompressETC2Block(uint8_t dst[ETC2_UNCOMPRESSED_BLOCK_SIZE_RGB8], uint64_t src,
                         uint8_t alphaDst[ETC2_UNCOMPRESSED_BLOCK_SIZE_A8], bool alphaMode)
{
    enum Etc2Mode
    {
        MODE_INDIVIDUAL = 0,
        MODE_DIFFERENTIAL,
        MODE_T,
        MODE_H,
        MODE_PLANAR,

        MODE_LAST
    };

    const int diffOpaqueBit = (int)getBit(src, 33);
    const int8_t selBR      = (int8_t)getBits(src, 59, 63); // 5 bits.
    const int8_t selBG      = (int8_t)getBits(src, 51, 55);
    const int8_t selBB      = (int8_t)getBits(src, 43, 47);
    const int8_t selDR      = extendSigned3To8((uint8_t)getBits(src, 56, 58)); // 3 bits.
    const int8_t selDG      = extendSigned3To8((uint8_t)getBits(src, 48, 50));
    const int8_t selDB      = extendSigned3To8((uint8_t)getBits(src, 40, 42));
    Etc2Mode mode;

    if (!alphaMode && diffOpaqueBit == 0)
        mode = MODE_INDIVIDUAL;
    else if (!de::inRange(selBR + selDR, 0, 31))
        mode = MODE_T;
    else if (!de::inRange(selBG + selDG, 0, 31))
        mode = MODE_H;
    else if (!de::inRange(selBB + selDB, 0, 31))
        mode = MODE_PLANAR;
    else
        mode = MODE_DIFFERENTIAL;

    if (mode == MODE_INDIVIDUAL || mode == MODE_DIFFERENTIAL)
    {
        // Individual and differential modes have some steps in common, handle them here.
        static const int modifierTable[8][4] = {//      00   01   10    11
                                                {2, 8, -2, -8},       {5, 17, -5, -17},    {9, 29, -9, -29},
                                                {13, 42, -13, -42},   {18, 60, -18, -60},  {24, 80, -24, -80},
                                                {33, 106, -33, -106}, {47, 183, -47, -183}};

        const int flipBit       = (int)getBit(src, 32);
        const uint32_t table[2] = {getBits(src, 37, 39), getBits(src, 34, 36)};
        uint8_t baseR[2];
        uint8_t baseG[2];
        uint8_t baseB[2];

        if (mode == MODE_INDIVIDUAL)
        {
            // Individual mode, initial values.
            baseR[0] = extend4To8((uint8_t)getBits(src, 60, 63));
            baseR[1] = extend4To8((uint8_t)getBits(src, 56, 59));
            baseG[0] = extend4To8((uint8_t)getBits(src, 52, 55));
            baseG[1] = extend4To8((uint8_t)getBits(src, 48, 51));
            baseB[0] = extend4To8((uint8_t)getBits(src, 44, 47));
            baseB[1] = extend4To8((uint8_t)getBits(src, 40, 43));
        }
        else
        {
            // Differential mode, initial values.
            baseR[0] = extend5To8(selBR);
            baseG[0] = extend5To8(selBG);
            baseB[0] = extend5To8(selBB);

            baseR[1] = extend5To8((uint8_t)(selBR + selDR));
            baseG[1] = extend5To8((uint8_t)(selBG + selDG));
            baseB[1] = extend5To8((uint8_t)(selBB + selDB));
        }

        // Write final pixels for individual or differential mode.
        for (int pixelNdx = 0; pixelNdx < ETC2_BLOCK_HEIGHT * ETC2_BLOCK_WIDTH; pixelNdx++)
        {
            const int x                = pixelNdx / ETC2_BLOCK_HEIGHT;
            const int y                = pixelNdx % ETC2_BLOCK_HEIGHT;
            const int dstOffset        = (y * ETC2_BLOCK_WIDTH + x) * ETC2_UNCOMPRESSED_PIXEL_SIZE_RGB8;
            const int subBlock         = ((flipBit ? y : x) >= 2) ? 1 : 0;
            const uint32_t tableNdx    = table[subBlock];
            const uint32_t modifierNdx = (getBit(src, 16 + pixelNdx) << 1) | getBit(src, pixelNdx);
            const int alphaDstOffset =
                (y * ETC2_BLOCK_WIDTH + x) * ETC2_UNCOMPRESSED_PIXEL_SIZE_A8; // Only needed for PUNCHTHROUGH version.

            // If doing PUNCHTHROUGH version (alphaMode), opaque bit may affect colors.
            if (alphaMode && diffOpaqueBit == 0 && modifierNdx == 2)
            {
                dst[dstOffset + 0]       = 0;
                dst[dstOffset + 1]       = 0;
                dst[dstOffset + 2]       = 0;
                alphaDst[alphaDstOffset] = 0;
            }
            else
            {
                int modifier;

                // PUNCHTHROUGH version and opaque bit may also affect modifiers.
                if (alphaMode && diffOpaqueBit == 0 && (modifierNdx == 0 || modifierNdx == 2))
                    modifier = 0;
                else
                    modifier = modifierTable[tableNdx][modifierNdx];

                dst[dstOffset + 0] = (uint8_t)deClamp32((int)baseR[subBlock] + modifier, 0, 255);
                dst[dstOffset + 1] = (uint8_t)deClamp32((int)baseG[subBlock] + modifier, 0, 255);
                dst[dstOffset + 2] = (uint8_t)deClamp32((int)baseB[subBlock] + modifier, 0, 255);

                if (alphaMode)
                    alphaDst[alphaDstOffset] = 255;
            }
        }
    }
    else if (mode == MODE_T || mode == MODE_H)
    {
        // T and H modes have some steps in common, handle them here.
        static const int distTable[8] = {3, 6, 11, 16, 23, 32, 41, 64};

        uint8_t paintR[4];
        uint8_t paintG[4];
        uint8_t paintB[4];

        if (mode == MODE_T)
        {
            // T mode, calculate paint values.
            const uint8_t R1a      = (uint8_t)getBits(src, 59, 60);
            const uint8_t R1b      = (uint8_t)getBits(src, 56, 57);
            const uint8_t G1       = (uint8_t)getBits(src, 52, 55);
            const uint8_t B1       = (uint8_t)getBits(src, 48, 51);
            const uint8_t R2       = (uint8_t)getBits(src, 44, 47);
            const uint8_t G2       = (uint8_t)getBits(src, 40, 43);
            const uint8_t B2       = (uint8_t)getBits(src, 36, 39);
            const uint32_t distNdx = (getBits(src, 34, 35) << 1) | getBit(src, 32);
            const int dist         = distTable[distNdx];

            paintR[0] = extend4To8((uint8_t)((R1a << 2) | R1b));
            paintG[0] = extend4To8(G1);
            paintB[0] = extend4To8(B1);
            paintR[2] = extend4To8(R2);
            paintG[2] = extend4To8(G2);
            paintB[2] = extend4To8(B2);
            paintR[1] = (uint8_t)deClamp32((int)paintR[2] + dist, 0, 255);
            paintG[1] = (uint8_t)deClamp32((int)paintG[2] + dist, 0, 255);
            paintB[1] = (uint8_t)deClamp32((int)paintB[2] + dist, 0, 255);
            paintR[3] = (uint8_t)deClamp32((int)paintR[2] - dist, 0, 255);
            paintG[3] = (uint8_t)deClamp32((int)paintG[2] - dist, 0, 255);
            paintB[3] = (uint8_t)deClamp32((int)paintB[2] - dist, 0, 255);
        }
        else
        {
            // H mode, calculate paint values.
            const uint8_t R1  = (uint8_t)getBits(src, 59, 62);
            const uint8_t G1a = (uint8_t)getBits(src, 56, 58);
            const uint8_t G1b = (uint8_t)getBit(src, 52);
            const uint8_t B1a = (uint8_t)getBit(src, 51);
            const uint8_t B1b = (uint8_t)getBits(src, 47, 49);
            const uint8_t R2  = (uint8_t)getBits(src, 43, 46);
            const uint8_t G2  = (uint8_t)getBits(src, 39, 42);
            const uint8_t B2  = (uint8_t)getBits(src, 35, 38);
            uint8_t baseR[2];
            uint8_t baseG[2];
            uint8_t baseB[2];
            uint32_t baseValue[2];
            uint32_t distNdx;
            int dist;

            baseR[0]     = extend4To8(R1);
            baseG[0]     = extend4To8((uint8_t)((G1a << 1) | G1b));
            baseB[0]     = extend4To8((uint8_t)((B1a << 3) | B1b));
            baseR[1]     = extend4To8(R2);
            baseG[1]     = extend4To8(G2);
            baseB[1]     = extend4To8(B2);
            baseValue[0] = (((uint32_t)baseR[0]) << 16) | (((uint32_t)baseG[0]) << 8) | baseB[0];
            baseValue[1] = (((uint32_t)baseR[1]) << 16) | (((uint32_t)baseG[1]) << 8) | baseB[1];
            distNdx      = (getBit(src, 34) << 2) | (getBit(src, 32) << 1) | (uint32_t)(baseValue[0] >= baseValue[1]);
            dist         = distTable[distNdx];

            paintR[0] = (uint8_t)deClamp32((int)baseR[0] + dist, 0, 255);
            paintG[0] = (uint8_t)deClamp32((int)baseG[0] + dist, 0, 255);
            paintB[0] = (uint8_t)deClamp32((int)baseB[0] + dist, 0, 255);
            paintR[1] = (uint8_t)deClamp32((int)baseR[0] - dist, 0, 255);
            paintG[1] = (uint8_t)deClamp32((int)baseG[0] - dist, 0, 255);
            paintB[1] = (uint8_t)deClamp32((int)baseB[0] - dist, 0, 255);
            paintR[2] = (uint8_t)deClamp32((int)baseR[1] + dist, 0, 255);
            paintG[2] = (uint8_t)deClamp32((int)baseG[1] + dist, 0, 255);
            paintB[2] = (uint8_t)deClamp32((int)baseB[1] + dist, 0, 255);
            paintR[3] = (uint8_t)deClamp32((int)baseR[1] - dist, 0, 255);
            paintG[3] = (uint8_t)deClamp32((int)baseG[1] - dist, 0, 255);
            paintB[3] = (uint8_t)deClamp32((int)baseB[1] - dist, 0, 255);
        }

        // Write final pixels for T or H mode.
        for (int pixelNdx = 0; pixelNdx < ETC2_BLOCK_HEIGHT * ETC2_BLOCK_WIDTH; pixelNdx++)
        {
            const int x             = pixelNdx / ETC2_BLOCK_HEIGHT;
            const int y             = pixelNdx % ETC2_BLOCK_HEIGHT;
            const int dstOffset     = (y * ETC2_BLOCK_WIDTH + x) * ETC2_UNCOMPRESSED_PIXEL_SIZE_RGB8;
            const uint32_t paintNdx = (getBit(src, 16 + pixelNdx) << 1) | getBit(src, pixelNdx);
            const int alphaDstOffset =
                (y * ETC2_BLOCK_WIDTH + x) * ETC2_UNCOMPRESSED_PIXEL_SIZE_A8; // Only needed for PUNCHTHROUGH version.

            if (alphaMode && diffOpaqueBit == 0 && paintNdx == 2)
            {
                dst[dstOffset + 0]       = 0;
                dst[dstOffset + 1]       = 0;
                dst[dstOffset + 2]       = 0;
                alphaDst[alphaDstOffset] = 0;
            }
            else
            {
                dst[dstOffset + 0] = (uint8_t)deClamp32((int)paintR[paintNdx], 0, 255);
                dst[dstOffset + 1] = (uint8_t)deClamp32((int)paintG[paintNdx], 0, 255);
                dst[dstOffset + 2] = (uint8_t)deClamp32((int)paintB[paintNdx], 0, 255);

                if (alphaMode)
                    alphaDst[alphaDstOffset] = 255;
            }
        }
    }
    else
    {
        // Planar mode.
        const uint8_t GO1 = (uint8_t)getBit(src, 56);
        const uint8_t GO2 = (uint8_t)getBits(src, 49, 54);
        const uint8_t BO1 = (uint8_t)getBit(src, 48);
        const uint8_t BO2 = (uint8_t)getBits(src, 43, 44);
        const uint8_t BO3 = (uint8_t)getBits(src, 39, 41);
        const uint8_t RH1 = (uint8_t)getBits(src, 34, 38);
        const uint8_t RH2 = (uint8_t)getBit(src, 32);
        const uint8_t RO  = extend6To8((uint8_t)getBits(src, 57, 62));
        const uint8_t GO  = extend7To8((uint8_t)((GO1 << 6) | GO2));
        const uint8_t BO  = extend6To8((uint8_t)((BO1 << 5) | (BO2 << 3) | BO3));
        const uint8_t RH  = extend6To8((uint8_t)((RH1 << 1) | RH2));
        const uint8_t GH  = extend7To8((uint8_t)getBits(src, 25, 31));
        const uint8_t BH  = extend6To8((uint8_t)getBits(src, 19, 24));
        const uint8_t RV  = extend6To8((uint8_t)getBits(src, 13, 18));
        const uint8_t GV  = extend7To8((uint8_t)getBits(src, 6, 12));
        const uint8_t BV  = extend6To8((uint8_t)getBits(src, 0, 5));

        // Write final pixels for planar mode.
        for (int y = 0; y < 4; y++)
        {
            for (int x = 0; x < 4; x++)
            {
                const int dstOffset      = (y * ETC2_BLOCK_WIDTH + x) * ETC2_UNCOMPRESSED_PIXEL_SIZE_RGB8;
                const int unclampedR     = (x * ((int)RH - (int)RO) + y * ((int)RV - (int)RO) + 4 * (int)RO + 2) >> 2;
                const int unclampedG     = (x * ((int)GH - (int)GO) + y * ((int)GV - (int)GO) + 4 * (int)GO + 2) >> 2;
                const int unclampedB     = (x * ((int)BH - (int)BO) + y * ((int)BV - (int)BO) + 4 * (int)BO + 2) >> 2;
                const int alphaDstOffset = (y * ETC2_BLOCK_WIDTH + x) *
                                           ETC2_UNCOMPRESSED_PIXEL_SIZE_A8; // Only needed for PUNCHTHROUGH version.

                dst[dstOffset + 0] = (uint8_t)deClamp32(unclampedR, 0, 255);
                dst[dstOffset + 1] = (uint8_t)deClamp32(unclampedG, 0, 255);
                dst[dstOffset + 2] = (uint8_t)deClamp32(unclampedB, 0, 255);

                if (alphaMode)
                    alphaDst[alphaDstOffset] = 255;
            }
        }
    }
}

void decompressEAC8Block(uint8_t dst[ETC2_UNCOMPRESSED_BLOCK_SIZE_A8], uint64_t src)
{
    static const int modifierTable[16][8] = {
        {-3, -6, -9, -15, 2, 5, 8, 14}, {-3, -7, -10, -13, 2, 6, 9, 12}, {-2, -5, -8, -13, 1, 4, 7, 12},
        {-2, -4, -6, -13, 1, 3, 5, 12}, {-3, -6, -8, -12, 2, 5, 7, 11},  {-3, -7, -9, -11, 2, 6, 8, 10},
        {-4, -7, -8, -11, 3, 6, 7, 10}, {-3, -5, -8, -11, 2, 4, 7, 10},  {-2, -6, -8, -10, 1, 5, 7, 9},
        {-2, -5, -8, -10, 1, 4, 7, 9},  {-2, -4, -8, -10, 1, 3, 7, 9},   {-2, -5, -7, -10, 1, 4, 6, 9},
        {-3, -4, -7, -10, 2, 3, 6, 9},  {-1, -2, -3, -10, 0, 1, 2, 9},   {-4, -6, -8, -9, 3, 5, 7, 8},
        {-3, -5, -7, -9, 2, 4, 6, 8}};

    const uint8_t baseCodeword = (uint8_t)getBits(src, 56, 63);
    const uint8_t multiplier   = (uint8_t)getBits(src, 52, 55);
    const uint32_t tableNdx    = getBits(src, 48, 51);

    for (int pixelNdx = 0; pixelNdx < ETC2_BLOCK_HEIGHT * ETC2_BLOCK_WIDTH; pixelNdx++)
    {
        const int x           = pixelNdx / ETC2_BLOCK_HEIGHT;
        const int y           = pixelNdx % ETC2_BLOCK_HEIGHT;
        const int dstOffset   = (y * ETC2_BLOCK_WIDTH + x) * ETC2_UNCOMPRESSED_PIXEL_SIZE_A8;
        const int pixelBitNdx = 45 - 3 * pixelNdx;
        const uint32_t modifierNdx =
            (getBit(src, pixelBitNdx + 2) << 2) | (getBit(src, pixelBitNdx + 1) << 1) | getBit(src, pixelBitNdx);
        const int modifier = modifierTable[tableNdx][modifierNdx];

        dst[dstOffset] = (uint8_t)deClamp32((int)baseCodeword + (int)multiplier * modifier, 0, 255);
    }
}

void decompressEAC11Block(uint8_t dst[ETC2_UNCOMPRESSED_BLOCK_SIZE_R11], uint64_t src, bool signedMode)
{
    static const int modifierTable[16][8] = {
        {-3, -6, -9, -15, 2, 5, 8, 14}, {-3, -7, -10, -13, 2, 6, 9, 12}, {-2, -5, -8, -13, 1, 4, 7, 12},
        {-2, -4, -6, -13, 1, 3, 5, 12}, {-3, -6, -8, -12, 2, 5, 7, 11},  {-3, -7, -9, -11, 2, 6, 8, 10},
        {-4, -7, -8, -11, 3, 6, 7, 10}, {-3, -5, -8, -11, 2, 4, 7, 10},  {-2, -6, -8, -10, 1, 5, 7, 9},
        {-2, -5, -8, -10, 1, 4, 7, 9},  {-2, -4, -8, -10, 1, 3, 7, 9},   {-2, -5, -7, -10, 1, 4, 6, 9},
        {-3, -4, -7, -10, 2, 3, 6, 9},  {-1, -2, -3, -10, 0, 1, 2, 9},   {-4, -6, -8, -9, 3, 5, 7, 8},
        {-3, -5, -7, -9, 2, 4, 6, 8}};

    const int32_t multiplier = (int32_t)getBits(src, 52, 55);
    const int32_t tableNdx   = (int32_t)getBits(src, 48, 51);
    int32_t baseCodeword     = (int32_t)getBits(src, 56, 63);

    if (signedMode)
    {
        if (baseCodeword > 127)
            baseCodeword -= 256;
        if (baseCodeword == -128)
            baseCodeword = -127;
    }

    for (int pixelNdx = 0; pixelNdx < ETC2_BLOCK_HEIGHT * ETC2_BLOCK_WIDTH; pixelNdx++)
    {
        const int x           = pixelNdx / ETC2_BLOCK_HEIGHT;
        const int y           = pixelNdx % ETC2_BLOCK_HEIGHT;
        const int dstOffset   = (y * ETC2_BLOCK_WIDTH + x) * ETC2_UNCOMPRESSED_PIXEL_SIZE_R11;
        const int pixelBitNdx = 45 - 3 * pixelNdx;
        const uint32_t modifierNdx =
            (getBit(src, pixelBitNdx + 2) << 2) | (getBit(src, pixelBitNdx + 1) << 1) | getBit(src, pixelBitNdx);
        const int modifier = modifierTable[tableNdx][modifierNdx];

        if (signedMode)
        {
            int16_t value;

            if (multiplier != 0)
                value = (int16_t)deClamp32(baseCodeword * 8 + multiplier * modifier * 8, -1023, 1023);
            else
                value = (int16_t)deClamp32(baseCodeword * 8 + modifier, -1023, 1023);

            *((int16_t *)(dst + dstOffset)) = value;
        }
        else
        {
            uint16_t value;

            if (multiplier != 0)
                value = (uint16_t)deClamp32(baseCodeword * 8 + 4 + multiplier * modifier * 8, 0, 2047);
            else
                value = (uint16_t)deClamp32(baseCodeword * 8 + 4 + modifier, 0, 2047);

            *((uint16_t *)(dst + dstOffset)) = value;
        }
    }
}

} // namespace EtcDecompressInternal

void decompressETC1(const PixelBufferAccess &dst, const uint8_t *src)
{
    using namespace EtcDecompressInternal;

    uint8_t *const dstPtr          = (uint8_t *)dst.getDataPtr();
    const uint64_t compressedBlock = get64BitBlock(src, 0);

    decompressETC1Block(dstPtr, compressedBlock);
}

void decompressETC2(const PixelBufferAccess &dst, const uint8_t *src)
{
    using namespace EtcDecompressInternal;

    uint8_t *const dstPtr          = (uint8_t *)dst.getDataPtr();
    const uint64_t compressedBlock = get64BitBlock(src, 0);

    decompressETC2Block(dstPtr, compressedBlock, NULL, false);
}

void decompressETC2_EAC_RGBA8(const PixelBufferAccess &dst, const uint8_t *src)
{
    using namespace EtcDecompressInternal;

    uint8_t *const dstPtr  = (uint8_t *)dst.getDataPtr();
    const int dstRowPitch  = dst.getRowPitch();
    const int dstPixelSize = ETC2_UNCOMPRESSED_PIXEL_SIZE_RGBA8;

    const uint64_t compressedBlockAlpha = get128BitBlockStart(src, 0);
    const uint64_t compressedBlockRGB   = get128BitBlockEnd(src, 0);
    uint8_t uncompressedBlockAlpha[ETC2_UNCOMPRESSED_BLOCK_SIZE_A8];
    uint8_t uncompressedBlockRGB[ETC2_UNCOMPRESSED_BLOCK_SIZE_RGB8];

    // Decompress.
    decompressETC2Block(uncompressedBlockRGB, compressedBlockRGB, NULL, false);
    decompressEAC8Block(uncompressedBlockAlpha, compressedBlockAlpha);

    // Write to dst.
    for (int y = 0; y < (int)ETC2_BLOCK_HEIGHT; y++)
    {
        for (int x = 0; x < (int)ETC2_BLOCK_WIDTH; x++)
        {
            const uint8_t *const srcPixelRGB =
                &uncompressedBlockRGB[(y * ETC2_BLOCK_WIDTH + x) * ETC2_UNCOMPRESSED_PIXEL_SIZE_RGB8];
            const uint8_t *const srcPixelAlpha =
                &uncompressedBlockAlpha[(y * ETC2_BLOCK_WIDTH + x) * ETC2_UNCOMPRESSED_PIXEL_SIZE_A8];
            uint8_t *const dstPixel = dstPtr + y * dstRowPitch + x * dstPixelSize;

            DE_STATIC_ASSERT(ETC2_UNCOMPRESSED_PIXEL_SIZE_RGBA8 == 4);
            dstPixel[0] = srcPixelRGB[0];
            dstPixel[1] = srcPixelRGB[1];
            dstPixel[2] = srcPixelRGB[2];
            dstPixel[3] = srcPixelAlpha[0];
        }
    }
}

void decompressETC2_RGB8_PUNCHTHROUGH_ALPHA1(const PixelBufferAccess &dst, const uint8_t *src)
{
    using namespace EtcDecompressInternal;

    uint8_t *const dstPtr  = (uint8_t *)dst.getDataPtr();
    const int dstRowPitch  = dst.getRowPitch();
    const int dstPixelSize = ETC2_UNCOMPRESSED_PIXEL_SIZE_RGBA8;

    const uint64_t compressedBlockRGBA = get64BitBlock(src, 0);
    uint8_t uncompressedBlockRGB[ETC2_UNCOMPRESSED_BLOCK_SIZE_RGB8];
    uint8_t uncompressedBlockAlpha[ETC2_UNCOMPRESSED_BLOCK_SIZE_A8];

    // Decompress.
    decompressETC2Block(uncompressedBlockRGB, compressedBlockRGBA, uncompressedBlockAlpha, true);

    // Write to dst.
    for (int y = 0; y < (int)ETC2_BLOCK_HEIGHT; y++)
    {
        for (int x = 0; x < (int)ETC2_BLOCK_WIDTH; x++)
        {
            const uint8_t *const srcPixel =
                &uncompressedBlockRGB[(y * ETC2_BLOCK_WIDTH + x) * ETC2_UNCOMPRESSED_PIXEL_SIZE_RGB8];
            const uint8_t *const srcPixelAlpha =
                &uncompressedBlockAlpha[(y * ETC2_BLOCK_WIDTH + x) * ETC2_UNCOMPRESSED_PIXEL_SIZE_A8];
            uint8_t *const dstPixel = dstPtr + y * dstRowPitch + x * dstPixelSize;

            DE_STATIC_ASSERT(ETC2_UNCOMPRESSED_PIXEL_SIZE_RGBA8 == 4);
            dstPixel[0] = srcPixel[0];
            dstPixel[1] = srcPixel[1];
            dstPixel[2] = srcPixel[2];
            dstPixel[3] = srcPixelAlpha[0];
        }
    }
}

void decompressEAC_R11(const PixelBufferAccess &dst, const uint8_t *src, bool signedMode)
{
    using namespace EtcDecompressInternal;

    uint8_t *const dstPtr  = (uint8_t *)dst.getDataPtr();
    const int dstRowPitch  = dst.getRowPitch();
    const int dstPixelSize = ETC2_UNCOMPRESSED_PIXEL_SIZE_R11;

    const uint64_t compressedBlock = get64BitBlock(src, 0);
    uint8_t uncompressedBlock[ETC2_UNCOMPRESSED_BLOCK_SIZE_R11];

    // Decompress.
    decompressEAC11Block(uncompressedBlock, compressedBlock, signedMode);

    // Write to dst.
    for (int y = 0; y < (int)ETC2_BLOCK_HEIGHT; y++)
    {
        for (int x = 0; x < (int)ETC2_BLOCK_WIDTH; x++)
        {
            DE_STATIC_ASSERT(ETC2_UNCOMPRESSED_PIXEL_SIZE_R11 == 2);

            if (signedMode)
            {
                const int16_t *const srcPixel =
                    (int16_t *)&uncompressedBlock[(y * ETC2_BLOCK_WIDTH + x) * ETC2_UNCOMPRESSED_PIXEL_SIZE_R11];
                int16_t *const dstPixel = (int16_t *)(dstPtr + y * dstRowPitch + x * dstPixelSize);

                dstPixel[0] = extend11To16WithSign(srcPixel[0]);
            }
            else
            {
                const uint16_t *const srcPixel =
                    (uint16_t *)&uncompressedBlock[(y * ETC2_BLOCK_WIDTH + x) * ETC2_UNCOMPRESSED_PIXEL_SIZE_R11];
                uint16_t *const dstPixel = (uint16_t *)(dstPtr + y * dstRowPitch + x * dstPixelSize);

                dstPixel[0] = extend11To16(srcPixel[0]);
            }
        }
    }
}

void decompressEAC_RG11(const PixelBufferAccess &dst, const uint8_t *src, bool signedMode)
{
    using namespace EtcDecompressInternal;

    uint8_t *const dstPtr  = (uint8_t *)dst.getDataPtr();
    const int dstRowPitch  = dst.getRowPitch();
    const int dstPixelSize = ETC2_UNCOMPRESSED_PIXEL_SIZE_RG11;

    const uint64_t compressedBlockR = get128BitBlockStart(src, 0);
    const uint64_t compressedBlockG = get128BitBlockEnd(src, 0);
    uint8_t uncompressedBlockR[ETC2_UNCOMPRESSED_BLOCK_SIZE_R11];
    uint8_t uncompressedBlockG[ETC2_UNCOMPRESSED_BLOCK_SIZE_R11];

    // Decompress.
    decompressEAC11Block(uncompressedBlockR, compressedBlockR, signedMode);
    decompressEAC11Block(uncompressedBlockG, compressedBlockG, signedMode);

    // Write to dst.
    for (int y = 0; y < (int)ETC2_BLOCK_HEIGHT; y++)
    {
        for (int x = 0; x < (int)ETC2_BLOCK_WIDTH; x++)
        {
            DE_STATIC_ASSERT(ETC2_UNCOMPRESSED_PIXEL_SIZE_RG11 == 4);

            if (signedMode)
            {
                const int16_t *const srcPixelR =
                    (int16_t *)&uncompressedBlockR[(y * ETC2_BLOCK_WIDTH + x) * ETC2_UNCOMPRESSED_PIXEL_SIZE_R11];
                const int16_t *const srcPixelG =
                    (int16_t *)&uncompressedBlockG[(y * ETC2_BLOCK_WIDTH + x) * ETC2_UNCOMPRESSED_PIXEL_SIZE_R11];
                int16_t *const dstPixel = (int16_t *)(dstPtr + y * dstRowPitch + x * dstPixelSize);

                dstPixel[0] = extend11To16WithSign(srcPixelR[0]);
                dstPixel[1] = extend11To16WithSign(srcPixelG[0]);
            }
            else
            {
                const uint16_t *const srcPixelR =
                    (uint16_t *)&uncompressedBlockR[(y * ETC2_BLOCK_WIDTH + x) * ETC2_UNCOMPRESSED_PIXEL_SIZE_R11];
                const uint16_t *const srcPixelG =
                    (uint16_t *)&uncompressedBlockG[(y * ETC2_BLOCK_WIDTH + x) * ETC2_UNCOMPRESSED_PIXEL_SIZE_R11];
                uint16_t *const dstPixel = (uint16_t *)(dstPtr + y * dstRowPitch + x * dstPixelSize);

                dstPixel[0] = extend11To16(srcPixelR[0]);
                dstPixel[1] = extend11To16(srcPixelG[0]);
            }
        }
    }
}

namespace BcDecompressInternal
{

enum
{
    BC_BLOCK_WIDTH  = 4,
    BC_BLOCK_HEIGHT = 4
};

static const uint8_t epBits[14] = {10, 7, 11, 11, 11, 9, 8, 8, 8, 6, 10, 11, 12, 16};

static const uint8_t partitions2[64][16] = {
    {0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1}, {0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1},
    {0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1}, {0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 1},
    {0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 1}, {0, 0, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1},
    {0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1}, {0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1}, {0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1},
    {0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1},
    {0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1},
    {0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1}, {0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0}, {0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0}, {0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1},
    {0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0}, {0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0},
    {0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0}, {0, 0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0},
    {0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0}, {0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0},
    {0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 0}, {0, 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0},
    {0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1}, {0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1},
    {0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0}, {0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0},
    {0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0}, {0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0},
    {0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1}, {0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1},
    {0, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 0}, {0, 0, 0, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0, 0},
    {0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 0, 0}, {0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0},
    {0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0}, {0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1},
    {0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1}, {0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0},
    {0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0}, {0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0}, {0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0},
    {0, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1}, {0, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1},
    {0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0}, {0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0},
    {0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1}, {0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1},
    {0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1}, {0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1},
    {0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1}, {0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0},
    {0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0}, {0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1}};

static const uint8_t partitions3[64][16] = {
    {0, 0, 1, 1, 0, 0, 1, 1, 0, 2, 2, 1, 2, 2, 2, 2}, {0, 0, 0, 1, 0, 0, 1, 1, 2, 2, 1, 1, 2, 2, 2, 1},
    {0, 0, 0, 0, 2, 0, 0, 1, 2, 2, 1, 1, 2, 2, 1, 1}, {0, 2, 2, 2, 0, 0, 2, 2, 0, 0, 1, 1, 0, 1, 1, 1},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 2, 1, 1, 2, 2}, {0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 2, 2, 0, 0, 2, 2},
    {0, 0, 2, 2, 0, 0, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1}, {0, 0, 1, 1, 0, 0, 1, 1, 2, 2, 1, 1, 2, 2, 1, 1},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2}, {0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2},
    {0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2}, {0, 0, 1, 2, 0, 0, 1, 2, 0, 0, 1, 2, 0, 0, 1, 2},
    {0, 1, 1, 2, 0, 1, 1, 2, 0, 1, 1, 2, 0, 1, 1, 2}, {0, 1, 2, 2, 0, 1, 2, 2, 0, 1, 2, 2, 0, 1, 2, 2},
    {0, 0, 1, 1, 0, 1, 1, 2, 1, 1, 2, 2, 1, 2, 2, 2}, {0, 0, 1, 1, 2, 0, 0, 1, 2, 2, 0, 0, 2, 2, 2, 0},
    {0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 2, 1, 1, 2, 2}, {0, 1, 1, 1, 0, 0, 1, 1, 2, 0, 0, 1, 2, 2, 0, 0},
    {0, 0, 0, 0, 1, 1, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2}, {0, 0, 2, 2, 0, 0, 2, 2, 0, 0, 2, 2, 1, 1, 1, 1},
    {0, 1, 1, 1, 0, 1, 1, 1, 0, 2, 2, 2, 0, 2, 2, 2}, {0, 0, 0, 1, 0, 0, 0, 1, 2, 2, 2, 1, 2, 2, 2, 1},
    {0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 2, 2, 0, 1, 2, 2}, {0, 0, 0, 0, 1, 1, 0, 0, 2, 2, 1, 0, 2, 2, 1, 0},
    {0, 1, 2, 2, 0, 1, 2, 2, 0, 0, 1, 1, 0, 0, 0, 0}, {0, 0, 1, 2, 0, 0, 1, 2, 1, 1, 2, 2, 2, 2, 2, 2},
    {0, 1, 1, 0, 1, 2, 2, 1, 1, 2, 2, 1, 0, 1, 1, 0}, {0, 0, 0, 0, 0, 1, 1, 0, 1, 2, 2, 1, 1, 2, 2, 1},
    {0, 0, 2, 2, 1, 1, 0, 2, 1, 1, 0, 2, 0, 0, 2, 2}, {0, 1, 1, 0, 0, 1, 1, 0, 2, 0, 0, 2, 2, 2, 2, 2},
    {0, 0, 1, 1, 0, 1, 2, 2, 0, 1, 2, 2, 0, 0, 1, 1}, {0, 0, 0, 0, 2, 0, 0, 0, 2, 2, 1, 1, 2, 2, 2, 1},
    {0, 0, 0, 0, 0, 0, 0, 2, 1, 1, 2, 2, 1, 2, 2, 2}, {0, 2, 2, 2, 0, 0, 2, 2, 0, 0, 1, 2, 0, 0, 1, 1},
    {0, 0, 1, 1, 0, 0, 1, 2, 0, 0, 2, 2, 0, 2, 2, 2}, {0, 1, 2, 0, 0, 1, 2, 0, 0, 1, 2, 0, 0, 1, 2, 0},
    {0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 0, 0, 0, 0}, {0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0},
    {0, 1, 2, 0, 2, 0, 1, 2, 1, 2, 0, 1, 0, 1, 2, 0}, {0, 0, 1, 1, 2, 2, 0, 0, 1, 1, 2, 2, 0, 0, 1, 1},
    {0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 0, 0, 0, 0, 1, 1}, {0, 1, 0, 1, 0, 1, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2},
    {0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 2, 1, 2, 1, 2, 1}, {0, 0, 2, 2, 1, 1, 2, 2, 0, 0, 2, 2, 1, 1, 2, 2},
    {0, 0, 2, 2, 0, 0, 1, 1, 0, 0, 2, 2, 0, 0, 1, 1}, {0, 2, 2, 0, 1, 2, 2, 1, 0, 2, 2, 0, 1, 2, 2, 1},
    {0, 1, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 0, 1, 0, 1}, {0, 0, 0, 0, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1},
    {0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 2, 2, 2, 2}, {0, 2, 2, 2, 0, 1, 1, 1, 0, 2, 2, 2, 0, 1, 1, 1},
    {0, 0, 0, 2, 1, 1, 1, 2, 0, 0, 0, 2, 1, 1, 1, 2}, {0, 0, 0, 0, 2, 1, 1, 2, 2, 1, 1, 2, 2, 1, 1, 2},
    {0, 2, 2, 2, 0, 1, 1, 1, 0, 1, 1, 1, 0, 2, 2, 2}, {0, 0, 0, 2, 1, 1, 1, 2, 1, 1, 1, 2, 0, 0, 0, 2},
    {0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 2, 2, 2, 2}, {0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 1, 2, 2, 1, 1, 2},
    {0, 1, 1, 0, 0, 1, 1, 0, 2, 2, 2, 2, 2, 2, 2, 2}, {0, 0, 2, 2, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 2, 2},
    {0, 0, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2, 0, 0, 2, 2}, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 1, 2},
    {0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 1}, {0, 2, 2, 2, 1, 2, 2, 2, 0, 2, 2, 2, 1, 2, 2, 2},
    {0, 1, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2}, {0, 1, 1, 1, 2, 0, 1, 1, 2, 2, 0, 1, 2, 2, 2, 0}};

static const uint8_t anchorIndicesSecondSubset2[64] = {15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
                                                       15, 2,  8,  2,  2,  8,  8,  15, 2,  8,  2,  2,  8,  8,  2,  2,
                                                       15, 15, 6,  8,  2,  8,  15, 15, 2,  8,  2,  2,  2,  15, 15, 6,
                                                       6,  2,  6,  8,  15, 15, 2,  2,  15, 15, 15, 15, 15, 2,  2,  15};

static const uint8_t anchorIndicesSecondSubset3[64] = {
    3, 3,  15, 15, 8, 3,  15, 15, 8,  8, 6,  6, 6,  5,  3,  3,  3, 3,  8, 15, 3, 3, 6, 10, 5, 8,  8, 6,  8,  5,  15, 15,
    8, 15, 3,  5,  6, 10, 8,  15, 15, 3, 15, 5, 15, 15, 15, 15, 3, 15, 5, 5,  5, 8, 5, 10, 5, 10, 8, 13, 15, 12, 3,  3};

static const uint8_t anchorIndicesThirdSubset[64] = {15, 8, 8,  3,  15, 15, 3,  8,  15, 15, 15, 15, 15, 15, 15, 8,
                                                     15, 8, 15, 3,  15, 8,  15, 8,  3,  15, 6,  10, 15, 15, 10, 8,
                                                     15, 3, 15, 10, 10, 8,  9,  10, 6,  15, 8,  15, 3,  6,  6,  8,
                                                     15, 3, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 3,  15, 15, 8};

static const uint16_t weights2[4]  = {0, 21, 43, 64};
static const uint16_t weights3[8]  = {0, 9, 18, 27, 37, 46, 55, 64};
static const uint16_t weights4[16] = {0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64};

inline float uint8ToFloat(uint8_t src)
{
    return ((float)src / 255.0f);
}

inline float int8ToFloat(int8_t src)
{
    return ((float)src / 128.0f);
}

inline uint32_t bgr16torgba32(uint16_t src)
{
    const uint32_t src32 = src;
    const uint8_t b5     = (src32 & 0x1f);
    const uint8_t g6     = (src32 >> 5) & 0x3f;
    const uint8_t r5     = (src32 >> 11) & 0x1f;
    const uint32_t a8    = 0xff;
    const uint32_t b8    = extend5To8(b5);
    const uint32_t g8    = extend6To8(g6);
    const uint32_t r8    = extend5To8(r5);

    return (r8 | (g8 << 8) | (b8 << 16) | (a8 << 24));
}

// Interpolates color = 1/3 * c0 + 2/3 * c1
inline uint32_t interpolateColor(uint32_t c0, uint32_t c1)
{
    const uint32_t r0 = c0 & 0xff;
    const uint32_t g0 = (c0 >> 8) & 0xff;
    const uint32_t b0 = (c0 >> 16) & 0xff;
    const uint32_t a0 = (c0 >> 24) & 0xff;

    const uint32_t r1 = c1 & 0xff;
    const uint32_t g1 = (c1 >> 8) & 0xff;
    const uint32_t b1 = (c1 >> 16) & 0xff;
    const uint32_t a1 = (c1 >> 24) & 0xff;

    const uint32_t r = (r0 + (r1 << 1)) / 3;
    const uint32_t g = (g0 + (g1 << 1)) / 3;
    const uint32_t b = (b0 + (b1 << 1)) / 3;
    const uint32_t a = (a0 + (a1 << 1)) / 3;

    return (r | (g << 8) | (b << 16) | (a << 24));
}

// Average of two colors
inline uint32_t averageColor(uint32_t c0, uint32_t c1)
{
    const uint32_t r0 = c0 & 0xff;
    const uint32_t g0 = (c0 >> 8) & 0xff;
    const uint32_t b0 = (c0 >> 16) & 0xff;
    const uint32_t a0 = (c0 >> 24) & 0xff;

    const uint32_t r1 = c1 & 0xff;
    const uint32_t g1 = (c1 >> 8) & 0xff;
    const uint32_t b1 = (c1 >> 16) & 0xff;
    const uint32_t a1 = (c1 >> 24) & 0xff;

    const uint32_t r = (r0 + r1) >> 1;
    const uint32_t g = (g0 + g1) >> 1;
    const uint32_t b = (b0 + b1) >> 1;
    const uint32_t a = (a0 + a1) >> 1;

    return (r | (g << 8) | (b << 16) | (a << 24));
}

inline int8_t extractModeBc6(uint8_t src)
{
    // Catch illegal modes
    switch (src & 0x1f)
    {
    case 0x13:
    case 0x17:
    case 0x1b:
    case 0x1f:
        return -1;
    }

    switch (src & 0x3)
    {
    case 0:
        return 0;
    case 1:
        return 1;
    case 2:
        return (int8_t)(2 + ((src >> 2) & 0x7));
    case 3:
        return (int8_t)(10 + ((src >> 2) & 0x7));
    }

    return -1;
}

inline int8_t extractModeBc7(uint8_t src)
{
    for (int8_t i = 0; i < 8; i++)
        if (src & (1 << i))
            return i;

    return -1;
}

inline uint64_t get64BitBlockLE(const uint8_t *src, int blockNdx)
{
    // Same as get64BitBlock, but little-endian.
    uint64_t block = 0;

    for (int i = 0; i < 8; i++)
        block |= (uint64_t)(src[blockNdx * 8 + i]) << (8ull * i);

    return block;
}

inline uint32_t getBits128(uint64_t low, uint64_t high, uint32_t first, uint32_t last)
{
    const uint64_t d[2] = {low, high};
    const bool reverse  = first > last;
    uint32_t ret        = 0;

    if (reverse)
    {
        const uint32_t tmp = first;
        first              = last;
        last               = tmp;
    }

    const int elementFirst = first / 64;
    const int elementLast  = last / 64;

    if (elementFirst == elementLast)
    {
        // Bits contained in one of the 64bit elements
        const uint32_t shift = first % 64;
        const uint32_t len   = last - first + 1;
        const uint32_t mask  = (1 << len) - 1;
        ret                  = (uint32_t)((d[elementFirst] >> shift) & mask);
    }
    else
    {
        // Bits contained in both of the 64bit elements
        DE_ASSERT(last > 63);
        DE_ASSERT(first < 64);
        const uint32_t len0  = 64 - first;
        const uint32_t mask0 = (1 << len0) - 1;
        const uint32_t data0 = (uint32_t)(low >> first) & mask0;
        const uint32_t len1  = last - 63;
        const uint32_t mask1 = (1 << len1) - 1;
        const uint32_t data1 = (uint32_t)(high & mask1);
        ret                  = (uint32_t)((data1 << len0) | data0);
    }

    if (reverse)
    {
        const uint32_t len  = last - first + 1;
        const uint32_t orig = ret;
        ret                 = 0;

        for (uint32_t i = 0; i < len; i++)
        {
            ret |= ((orig >> (len - 1 - i)) & 1) << i;
        }
    }

    return ret;
}

inline int32_t signExtend(int32_t value, int32_t srcBits, int32_t dstBits)
{
    uint32_t sign = value & (1 << (srcBits - 1));

    if (!sign)
        return value;

    int32_t dstMask      = (int32_t)(((uint64_t)1 << dstBits) - 1);
    int32_t extendedBits = 0xffffffff << srcBits;
    return (value | extendedBits) & dstMask;
}

inline int32_t unquantize(int32_t x, int mode, bool hasSign)
{
    if (hasSign)
    {
        bool s = false;

        if (epBits[mode] >= 16)
            return x;

        if (x < 0)
        {
            s = true;
            x = -x;
        }

        if (x == 0)
            x = 0;
        else if (x >= (((int32_t)1 << (epBits[mode] - 1)) - 1))
            x = 0x7fff;
        else
            x = (((int32_t)x << 15) + 0x4000) >> (epBits[mode] - 1);

        if (s)
            x = -x;

        return x;
    }
    else
    {
        if (epBits[mode] >= 15)
            return x;
        else if (x == 0)
            return 0;
        else if (x == (((int32_t)1 << epBits[mode]) - 1))
            return 0xffff;
        else
            return ((((int32_t)x << 15) + 0x4000) >> (epBits[mode] - 1));
    }
}

inline int32_t interpolate(int32_t a, int32_t b, uint32_t index, uint32_t indexPrecision)
{
    const uint16_t *weights[] = {weights2, weights3, weights4};
    const uint16_t *weight    = weights[indexPrecision - 2];
    DE_ASSERT(indexPrecision >= 2 && indexPrecision <= 4);

    return (((64 - weight[index]) * a + weight[index] * b + 32) >> 6);
}

inline int16_t finishUnquantize(int32_t x, bool hasSign)
{
    if (hasSign)
    {
        if (x < 0)
            x = -(((-x) * 31) >> 5);
        else
            x = (x * 31) >> 5;

        if (x < 0)
            x = (-x) | 0x8000;
    }
    else
    {
        x = (x * 31) / 64;
    }

    return (int16_t)x;
}

} // namespace BcDecompressInternal

void decompressBc1(const PixelBufferAccess &dst, const uint8_t *src, bool hasAlpha)
{
    using namespace BcDecompressInternal;

    uint8_t *const dstPtr         = (uint8_t *)dst.getDataPtr();
    const uint32_t dstRowPitch    = dst.getRowPitch();
    const uint32_t dstPixelSize   = 4;
    const uint16_t color0_16      = ((uint16_t *)src)[0];
    const uint16_t color1_16      = ((uint16_t *)src)[1];
    const uint32_t color0         = bgr16torgba32(color0_16);
    const uint32_t color1         = bgr16torgba32(color1_16);
    const uint8_t *const indices8 = &src[4];

    const bool alphaMode = color1_16 > color0_16;

    const int32_t indices[16] = {
        (indices8[0] >> 0) & 0x3, (indices8[0] >> 2) & 0x3, (indices8[0] >> 4) & 0x3, (indices8[0] >> 6) & 0x3,
        (indices8[1] >> 0) & 0x3, (indices8[1] >> 2) & 0x3, (indices8[1] >> 4) & 0x3, (indices8[1] >> 6) & 0x3,
        (indices8[2] >> 0) & 0x3, (indices8[2] >> 2) & 0x3, (indices8[2] >> 4) & 0x3, (indices8[2] >> 6) & 0x3,
        (indices8[3] >> 0) & 0x3, (indices8[3] >> 2) & 0x3, (indices8[3] >> 4) & 0x3, (indices8[3] >> 6) & 0x3};

    const uint32_t colors[4] = {color0, color1,
                                alphaMode ? averageColor(color0, color1) : interpolateColor(color1, color0),
                                alphaMode ? (hasAlpha ? 0 : 0xff000000) : interpolateColor(color0, color1)};

    for (uint32_t y = 0; y < (uint32_t)BC_BLOCK_HEIGHT; y++)
    {
        for (uint32_t x = 0; x < (uint32_t)BC_BLOCK_WIDTH; x++)
        {
            uint32_t *const dstPixel = (uint32_t *)(dstPtr + y * dstRowPitch + x * dstPixelSize);
            *dstPixel                = colors[indices[y * BC_BLOCK_WIDTH + x]];
        }
    }
}

void decompressBc2(const PixelBufferAccess &dst, const uint8_t *src)
{
    using namespace BcDecompressInternal;

    uint8_t *const dstPtr         = (uint8_t *)dst.getDataPtr();
    const uint32_t dstRowPitch    = dst.getRowPitch();
    const uint32_t dstPixelSize   = 4;
    const uint16_t color0_16      = ((uint16_t *)src)[4];
    const uint16_t color1_16      = ((uint16_t *)src)[5];
    const uint32_t color0         = bgr16torgba32(color0_16);
    const uint32_t color1         = bgr16torgba32(color1_16);
    const uint8_t *const indices8 = &src[12];
    const uint8_t *const alphas8  = src;

    const int32_t indices[16] = {
        (indices8[0] >> 0) & 0x3, (indices8[0] >> 2) & 0x3, (indices8[0] >> 4) & 0x3, (indices8[0] >> 6) & 0x3,
        (indices8[1] >> 0) & 0x3, (indices8[1] >> 2) & 0x3, (indices8[1] >> 4) & 0x3, (indices8[1] >> 6) & 0x3,
        (indices8[2] >> 0) & 0x3, (indices8[2] >> 2) & 0x3, (indices8[2] >> 4) & 0x3, (indices8[2] >> 6) & 0x3,
        (indices8[3] >> 0) & 0x3, (indices8[3] >> 2) & 0x3, (indices8[3] >> 4) & 0x3, (indices8[3] >> 6) & 0x3};

    const int32_t alphas[16] = {
        extend4To8(((alphas8[0] >> 0) & 0xf)) << 24, extend4To8(((alphas8[0] >> 4) & 0xf)) << 24,
        extend4To8(((alphas8[1] >> 0) & 0xf)) << 24, extend4To8(((alphas8[1] >> 4) & 0xf)) << 24,
        extend4To8(((alphas8[2] >> 0) & 0xf)) << 24, extend4To8(((alphas8[2] >> 4) & 0xf)) << 24,
        extend4To8(((alphas8[3] >> 0) & 0xf)) << 24, extend4To8(((alphas8[3] >> 4) & 0xf)) << 24,
        extend4To8(((alphas8[4] >> 0) & 0xf)) << 24, extend4To8(((alphas8[4] >> 4) & 0xf)) << 24,
        extend4To8(((alphas8[5] >> 0) & 0xf)) << 24, extend4To8(((alphas8[5] >> 4) & 0xf)) << 24,
        extend4To8(((alphas8[6] >> 0) & 0xf)) << 24, extend4To8(((alphas8[6] >> 4) & 0xf)) << 24,
        extend4To8(((alphas8[7] >> 0) & 0xf)) << 24, extend4To8(((alphas8[7] >> 4) & 0xf)) << 24};

    const uint32_t colors[4] = {color0, color1, interpolateColor(color1, color0), interpolateColor(color0, color1)};

    for (uint32_t y = 0; y < (uint32_t)BC_BLOCK_HEIGHT; y++)
    {
        for (uint32_t x = 0; x < (uint32_t)BC_BLOCK_WIDTH; x++)
        {
            uint32_t *const dstPixel = (uint32_t *)(dstPtr + y * dstRowPitch + x * dstPixelSize);
            *dstPixel = (colors[indices[y * BC_BLOCK_WIDTH + x]] & 0x00ffffff) | alphas[y * BC_BLOCK_WIDTH + x];
        }
    }
}

void decompressBc3(const PixelBufferAccess &dst, const uint8_t *src)
{
    using namespace BcDecompressInternal;

    uint8_t *const dstPtr         = (uint8_t *)dst.getDataPtr();
    const uint32_t dstRowPitch    = dst.getRowPitch();
    const uint32_t dstPixelSize   = 4;
    const uint8_t alpha0          = src[0];
    const uint8_t alpha1          = src[1];
    const uint16_t color0_16      = ((uint16_t *)src)[4];
    const uint16_t color1_16      = ((uint16_t *)src)[5];
    const uint32_t color0         = bgr16torgba32(color0_16);
    const uint32_t color1         = bgr16torgba32(color1_16);
    const uint8_t *const indices8 = &src[12];
    const uint64_t alphaBits      = get64BitBlockLE(src, 0) >> 16;
    uint32_t alphas[8];

    const int32_t indices[16] = {
        (indices8[0] >> 0) & 0x3, (indices8[0] >> 2) & 0x3, (indices8[0] >> 4) & 0x3, (indices8[0] >> 6) & 0x3,
        (indices8[1] >> 0) & 0x3, (indices8[1] >> 2) & 0x3, (indices8[1] >> 4) & 0x3, (indices8[1] >> 6) & 0x3,
        (indices8[2] >> 0) & 0x3, (indices8[2] >> 2) & 0x3, (indices8[2] >> 4) & 0x3, (indices8[2] >> 6) & 0x3,
        (indices8[3] >> 0) & 0x3, (indices8[3] >> 2) & 0x3, (indices8[3] >> 4) & 0x3, (indices8[3] >> 6) & 0x3};

    const int32_t alphaIndices[16] = {
        (int32_t)((alphaBits >> 0) & 0x7),  (int32_t)((alphaBits >> 3) & 0x7),  (int32_t)((alphaBits >> 6) & 0x7),
        (int32_t)((alphaBits >> 9) & 0x7),  (int32_t)((alphaBits >> 12) & 0x7), (int32_t)((alphaBits >> 15) & 0x7),
        (int32_t)((alphaBits >> 18) & 0x7), (int32_t)((alphaBits >> 21) & 0x7), (int32_t)((alphaBits >> 24) & 0x7),
        (int32_t)((alphaBits >> 27) & 0x7), (int32_t)((alphaBits >> 30) & 0x7), (int32_t)((alphaBits >> 33) & 0x7),
        (int32_t)((alphaBits >> 36) & 0x7), (int32_t)((alphaBits >> 39) & 0x7), (int32_t)((alphaBits >> 42) & 0x7),
        (int32_t)((alphaBits >> 45) & 0x7)};

    const uint32_t colors[4] = {color0, color1, interpolateColor(color1, color0), interpolateColor(color0, color1)};

    alphas[0] = alpha0 << 24;
    alphas[1] = alpha1 << 24;

    if (alpha0 > alpha1)
    {
        for (uint32_t i = 0; i < 6; i++)
            alphas[i + 2] = (((uint32_t)alpha0 * (6 - i) + (uint32_t)alpha1 * (1 + i)) / 7) << 24;
    }
    else
    {
        for (uint32_t i = 0; i < 4; i++)
            alphas[i + 2] = (((uint32_t)alpha0 * (4 - i) + (uint32_t)alpha1 * (1 + i)) / 5) << 24;
        alphas[6] = 0;
        alphas[7] = 0xff000000;
    }

    for (uint32_t y = 0; y < (uint32_t)BC_BLOCK_HEIGHT; y++)
    {
        for (uint32_t x = 0; x < (uint32_t)BC_BLOCK_WIDTH; x++)
        {
            uint32_t *const dstPixel = (uint32_t *)(dstPtr + y * dstRowPitch + x * dstPixelSize);
            *dstPixel =
                (colors[indices[y * BC_BLOCK_WIDTH + x]] & 0x00ffffff) | alphas[alphaIndices[y * BC_BLOCK_WIDTH + x]];
        }
    }
}

void decompressBc4(const PixelBufferAccess &dst, const uint8_t *src, bool hasSign)
{
    using namespace BcDecompressInternal;

    uint8_t *const dstPtr       = (uint8_t *)dst.getDataPtr();
    const uint32_t dstRowPitch  = dst.getRowPitch();
    const uint32_t dstPixelSize = 4;
    const uint8_t red0          = src[0];
    const uint8_t red1          = src[1];
    const int8_t red0s          = ((int8_t *)src)[0];
    const int8_t red1s          = ((int8_t *)src)[1];
    const uint64_t indexBits    = get64BitBlockLE(src, 0) >> 16;
    float reds[8];

    const int32_t indices[16] = {
        (int32_t)((indexBits >> 0) & 0x7),  (int32_t)((indexBits >> 3) & 0x7),  (int32_t)((indexBits >> 6) & 0x7),
        (int32_t)((indexBits >> 9) & 0x7),  (int32_t)((indexBits >> 12) & 0x7), (int32_t)((indexBits >> 15) & 0x7),
        (int32_t)((indexBits >> 18) & 0x7), (int32_t)((indexBits >> 21) & 0x7), (int32_t)((indexBits >> 24) & 0x7),
        (int32_t)((indexBits >> 27) & 0x7), (int32_t)((indexBits >> 30) & 0x7), (int32_t)((indexBits >> 33) & 0x7),
        (int32_t)((indexBits >> 36) & 0x7), (int32_t)((indexBits >> 39) & 0x7), (int32_t)((indexBits >> 42) & 0x7),
        (int32_t)((indexBits >> 45) & 0x7)};

    reds[0] = hasSign ? int8ToFloat(red0s) : uint8ToFloat(red0);
    reds[1] = hasSign ? int8ToFloat(red1s) : uint8ToFloat(red1);

    if (reds[0] > reds[1])
    {
        for (uint32_t i = 0; i < 6; i++)
            reds[i + 2] = (reds[0] * (6.0f - (float)i) + reds[1] * (1.0f + (float)i)) / 7.0f;
    }
    else
    {
        for (uint32_t i = 0; i < 4; i++)
            reds[i + 2] = (reds[0] * (4.0f - (float)i) + reds[1] * (1.0f + (float)i)) / 5.0f;
        reds[6] = hasSign ? -1.0f : 0.0f;
        reds[7] = 1.0f;
    }

    for (uint32_t y = 0; y < (uint32_t)BC_BLOCK_HEIGHT; y++)
    {
        for (uint32_t x = 0; x < (uint32_t)BC_BLOCK_WIDTH; x++)
        {
            float *const dstPixel = (float *)(dstPtr + y * dstRowPitch + x * dstPixelSize);
            *dstPixel             = reds[indices[y * BC_BLOCK_WIDTH + x]];
        }
    }
}

void decompressBc5(const PixelBufferAccess &dst, const uint8_t *src, bool hasSign)
{
    using namespace BcDecompressInternal;

    uint8_t *const dstPtr       = (uint8_t *)dst.getDataPtr();
    const uint32_t dstRowPitch  = dst.getRowPitch();
    const uint32_t dstPixelSize = 8;
    float rg[2][8];
    uint32_t indices[2][16];

    for (uint32_t c = 0; c < 2; c++)
    {
        const uint32_t offset    = c * 8;
        const uint8_t rg0        = src[offset];
        const uint8_t rg1        = src[offset + 1];
        const int8_t rg0s        = ((int8_t *)src)[offset];
        const int8_t rg1s        = ((int8_t *)src)[offset + 1];
        const uint64_t indexBits = get64BitBlockLE(src, c) >> 16;

        for (uint32_t i = 0; i < 16; i++)
            indices[c][i] = (indexBits >> (i * 3)) & 0x7;

        rg[c][0] = hasSign ? int8ToFloat(rg0s) : uint8ToFloat(rg0);
        rg[c][1] = hasSign ? int8ToFloat(rg1s) : uint8ToFloat(rg1);

        if (rg[c][0] > rg[c][1])
        {
            for (uint32_t i = 0; i < 6; i++)
                rg[c][i + 2] = (rg[c][0] * (6.0f - (float)i) + rg[c][1] * (1.0f + (float)i)) / 7.0f;
        }
        else
        {
            for (uint32_t i = 0; i < 4; i++)
                rg[c][i + 2] = (rg[c][0] * (4.0f - (float)i) + rg[c][1] * (1.0f + (float)i)) / 5.0f;
            rg[c][6] = hasSign ? -1.0f : 0.0f;
            rg[c][7] = 1.0f;
        }
    }

    for (uint32_t y = 0; y < (uint32_t)BC_BLOCK_HEIGHT; y++)
    {
        for (uint32_t x = 0; x < (uint32_t)BC_BLOCK_WIDTH; x++)
        {
            float *const dstPixel = (float *)(dstPtr + y * dstRowPitch + x * dstPixelSize);
            for (uint32_t i = 0; i < 2; i++)
                dstPixel[i] = rg[i][indices[i][y * BC_BLOCK_WIDTH + x]];
        }
    }
}

void decompressBc6H(const PixelBufferAccess &dst, const uint8_t *src, bool hasSign)
{
    using namespace BcDecompressInternal;

    uint8_t *const dstPtr       = (uint8_t *)dst.getDataPtr();
    const uint32_t dstRowPitch  = dst.getRowPitch();
    const uint32_t dstPixelSize = 6;

    int32_t mode = extractModeBc6(src[0]);
    IVec4 r(0);
    IVec4 g(0);
    IVec4 b(0);
    uint32_t deltaBitsR           = 0;
    uint32_t deltaBitsG           = 0;
    uint32_t deltaBitsB           = 0;
    const uint64_t low            = ((uint64_t *)src)[0];
    const uint64_t high           = ((uint64_t *)src)[1];
    const uint32_t d              = mode < 10 ? getBits128(low, high, 77, 81) : 0;
    const uint32_t numRegions     = mode > 9 ? 1 : 2;
    const uint32_t numEndpoints   = numRegions * 2;
    const bool transformed        = mode != 9 && mode != 10;
    const uint32_t colorIndexBC   = mode < 10 ? 3 : 4;
    uint64_t colorIndexData       = high >> (mode < 10 ? 18 : 1);
    const uint32_t anchorIndex[2] = {0, anchorIndicesSecondSubset2[d]};

    switch (mode)
    {
    case 0:
        g[2] |= getBits128(low, high, 2, 2) << 4;
        b[2] |= getBits128(low, high, 3, 3) << 4;
        b[3] |= getBits128(low, high, 4, 4) << 4;
        r[0] |= getBits128(low, high, 5, 14);
        g[0] |= getBits128(low, high, 15, 24);
        b[0] |= getBits128(low, high, 25, 34);
        r[1] |= getBits128(low, high, 35, 39);
        g[3] |= getBits128(low, high, 40, 40) << 4;
        g[2] |= getBits128(low, high, 41, 44);
        g[1] |= getBits128(low, high, 45, 49);
        b[3] |= getBits128(low, high, 50, 50);
        g[3] |= getBits128(low, high, 51, 54);
        b[1] |= getBits128(low, high, 55, 59);
        b[3] |= getBits128(low, high, 60, 60) << 1;
        b[2] |= getBits128(low, high, 61, 64);
        r[2] |= getBits128(low, high, 65, 69);
        b[3] |= getBits128(low, high, 70, 70) << 2;
        r[3] |= getBits128(low, high, 71, 75);
        b[3] |= getBits128(low, high, 76, 76) << 3;
        deltaBitsR = deltaBitsG = deltaBitsB = 5;
        break;

    case 1:
        g[2] |= getBits128(low, high, 2, 2) << 5;
        g[3] |= getBits128(low, high, 3, 3) << 4;
        g[3] |= getBits128(low, high, 4, 4) << 5;
        r[0] |= getBits128(low, high, 5, 11);
        b[3] |= getBits128(low, high, 12, 12);
        b[3] |= getBits128(low, high, 13, 13) << 1;
        b[2] |= getBits128(low, high, 14, 14) << 4;
        g[0] |= getBits128(low, high, 15, 21);
        b[2] |= getBits128(low, high, 22, 22) << 5;
        b[3] |= getBits128(low, high, 23, 23) << 2;
        g[2] |= getBits128(low, high, 24, 24) << 4;
        b[0] |= getBits128(low, high, 25, 31);
        b[3] |= getBits128(low, high, 32, 32) << 3;
        b[3] |= getBits128(low, high, 33, 33) << 5;
        b[3] |= getBits128(low, high, 34, 34) << 4;
        r[1] |= getBits128(low, high, 35, 40);
        g[2] |= getBits128(low, high, 41, 44);
        g[1] |= getBits128(low, high, 45, 50);
        g[3] |= getBits128(low, high, 51, 54);
        b[1] |= getBits128(low, high, 55, 60);
        b[2] |= getBits128(low, high, 61, 64);
        r[2] |= getBits128(low, high, 65, 70);
        r[3] |= getBits128(low, high, 71, 76);
        deltaBitsR = deltaBitsG = deltaBitsB = 6;
        break;

    case 2:
        r[0] |= getBits128(low, high, 5, 14);
        g[0] |= getBits128(low, high, 15, 24);
        b[0] |= getBits128(low, high, 25, 34);
        r[1] |= getBits128(low, high, 35, 39);
        r[0] |= getBits128(low, high, 40, 40) << 10;
        g[2] |= getBits128(low, high, 41, 44);
        g[1] |= getBits128(low, high, 45, 48);
        g[0] |= getBits128(low, high, 49, 49) << 10;
        b[3] |= getBits128(low, high, 50, 50);
        g[3] |= getBits128(low, high, 51, 54);
        b[1] |= getBits128(low, high, 55, 58);
        b[0] |= getBits128(low, high, 59, 59) << 10;
        b[3] |= getBits128(low, high, 60, 60) << 1;
        b[2] |= getBits128(low, high, 61, 64);
        r[2] |= getBits128(low, high, 65, 69);
        b[3] |= getBits128(low, high, 70, 70) << 2;
        r[3] |= getBits128(low, high, 71, 75);
        b[3] |= getBits128(low, high, 76, 76) << 3;
        deltaBitsR = 5;
        deltaBitsG = deltaBitsB = 4;
        break;

    case 3:
        r[0] |= getBits128(low, high, 5, 14);
        g[0] |= getBits128(low, high, 15, 24);
        b[0] |= getBits128(low, high, 25, 34);
        r[1] |= getBits128(low, high, 35, 38);
        r[0] |= getBits128(low, high, 39, 39) << 10;
        g[3] |= getBits128(low, high, 40, 40) << 4;
        g[2] |= getBits128(low, high, 41, 44);
        g[1] |= getBits128(low, high, 45, 49);
        g[0] |= getBits128(low, high, 50, 50) << 10;
        g[3] |= getBits128(low, high, 51, 54);
        b[1] |= getBits128(low, high, 55, 58);
        b[0] |= getBits128(low, high, 59, 59) << 10;
        b[3] |= getBits128(low, high, 60, 60) << 1;
        b[2] |= getBits128(low, high, 61, 64);
        r[2] |= getBits128(low, high, 65, 68);
        b[3] |= getBits128(low, high, 69, 69);
        b[3] |= getBits128(low, high, 70, 70) << 2;
        r[3] |= getBits128(low, high, 71, 74);
        g[2] |= getBits128(low, high, 75, 75) << 4;
        b[3] |= getBits128(low, high, 76, 76) << 3;
        deltaBitsR = deltaBitsB = 4;
        deltaBitsG              = 5;
        break;

    case 4:
        r[0] |= getBits128(low, high, 5, 14);
        g[0] |= getBits128(low, high, 15, 24);
        b[0] |= getBits128(low, high, 25, 34);
        r[1] |= getBits128(low, high, 35, 38);
        r[0] |= getBits128(low, high, 39, 39) << 10;
        b[2] |= getBits128(low, high, 40, 40) << 4;
        g[2] |= getBits128(low, high, 41, 44);
        g[1] |= getBits128(low, high, 45, 48);
        g[0] |= getBits128(low, high, 49, 49) << 10;
        b[3] |= getBits128(low, high, 50, 50);
        g[3] |= getBits128(low, high, 51, 54);
        b[1] |= getBits128(low, high, 55, 59);
        b[0] |= getBits128(low, high, 60, 60) << 10;
        b[2] |= getBits128(low, high, 61, 64);
        r[2] |= getBits128(low, high, 65, 68);
        b[3] |= getBits128(low, high, 69, 69) << 1;
        b[3] |= getBits128(low, high, 70, 70) << 2;
        r[3] |= getBits128(low, high, 71, 74);
        b[3] |= getBits128(low, high, 75, 75) << 4;
        b[3] |= getBits128(low, high, 76, 76) << 3;
        deltaBitsR = deltaBitsG = 4;
        deltaBitsB              = 5;
        break;

    case 5:
        r[0] |= getBits128(low, high, 5, 13);
        b[2] |= getBits128(low, high, 14, 14) << 4;
        g[0] |= getBits128(low, high, 15, 23);
        g[2] |= getBits128(low, high, 24, 24) << 4;
        b[0] |= getBits128(low, high, 25, 33);
        b[3] |= getBits128(low, high, 34, 34) << 4;
        r[1] |= getBits128(low, high, 35, 39);
        g[3] |= getBits128(low, high, 40, 40) << 4;
        g[2] |= getBits128(low, high, 41, 44);
        g[1] |= getBits128(low, high, 45, 49);
        b[3] |= getBits128(low, high, 50, 50);
        g[3] |= getBits128(low, high, 51, 54);
        b[1] |= getBits128(low, high, 55, 59);
        b[3] |= getBits128(low, high, 60, 60) << 1;
        b[2] |= getBits128(low, high, 61, 64);
        r[2] |= getBits128(low, high, 65, 69);
        b[3] |= getBits128(low, high, 70, 70) << 2;
        r[3] |= getBits128(low, high, 71, 75);
        b[3] |= getBits128(low, high, 76, 76) << 3;
        deltaBitsR = deltaBitsG = deltaBitsB = 5;
        break;

    case 6:
        r[0] |= getBits128(low, high, 5, 12);
        g[3] |= getBits128(low, high, 13, 13) << 4;
        b[2] |= getBits128(low, high, 14, 14) << 4;
        g[0] |= getBits128(low, high, 15, 22);
        b[3] |= getBits128(low, high, 23, 23) << 2;
        g[2] |= getBits128(low, high, 24, 24) << 4;
        b[0] |= getBits128(low, high, 25, 32);
        b[3] |= getBits128(low, high, 33, 33) << 3;
        b[3] |= getBits128(low, high, 34, 34) << 4;
        r[1] |= getBits128(low, high, 35, 40);
        g[2] |= getBits128(low, high, 41, 44);
        g[1] |= getBits128(low, high, 45, 49);
        b[3] |= getBits128(low, high, 50, 50);
        g[3] |= getBits128(low, high, 51, 54);
        b[1] |= getBits128(low, high, 55, 59);
        b[3] |= getBits128(low, high, 60, 60) << 1;
        b[2] |= getBits128(low, high, 61, 64);
        r[2] |= getBits128(low, high, 65, 70);
        r[3] |= getBits128(low, high, 71, 76);
        deltaBitsR = 6;
        deltaBitsG = deltaBitsB = 5;
        break;

    case 7:
        r[0] |= getBits128(low, high, 5, 12);
        b[3] |= getBits128(low, high, 13, 13);
        b[2] |= getBits128(low, high, 14, 14) << 4;
        g[0] |= getBits128(low, high, 15, 22);
        g[2] |= getBits128(low, high, 23, 23) << 5;
        g[2] |= getBits128(low, high, 24, 24) << 4;
        b[0] |= getBits128(low, high, 25, 32);
        g[3] |= getBits128(low, high, 33, 33) << 5;
        b[3] |= getBits128(low, high, 34, 34) << 4;
        r[1] |= getBits128(low, high, 35, 39);
        g[3] |= getBits128(low, high, 40, 40) << 4;
        g[2] |= getBits128(low, high, 41, 44);
        g[1] |= getBits128(low, high, 45, 50);
        g[3] |= getBits128(low, high, 51, 54);
        b[1] |= getBits128(low, high, 55, 59);
        b[3] |= getBits128(low, high, 60, 60) << 1;
        b[2] |= getBits128(low, high, 61, 64);
        r[2] |= getBits128(low, high, 65, 69);
        b[3] |= getBits128(low, high, 70, 70) << 2;
        r[3] |= getBits128(low, high, 71, 75);
        b[3] |= getBits128(low, high, 76, 76) << 3;
        deltaBitsR = deltaBitsB = 5;
        deltaBitsG              = 6;
        break;

    case 8:
        r[0] |= getBits128(low, high, 5, 12);
        b[3] |= getBits128(low, high, 13, 13) << 1;
        b[2] |= getBits128(low, high, 14, 14) << 4;
        g[0] |= getBits128(low, high, 15, 22);
        b[2] |= getBits128(low, high, 23, 23) << 5;
        g[2] |= getBits128(low, high, 24, 24) << 4;
        b[0] |= getBits128(low, high, 25, 32);
        b[3] |= getBits128(low, high, 33, 33) << 5;
        b[3] |= getBits128(low, high, 34, 34) << 4;
        r[1] |= getBits128(low, high, 35, 39);
        g[3] |= getBits128(low, high, 40, 40) << 4;
        g[2] |= getBits128(low, high, 41, 44);
        g[1] |= getBits128(low, high, 45, 49);
        b[3] |= getBits128(low, high, 50, 50);
        g[3] |= getBits128(low, high, 51, 54);
        b[1] |= getBits128(low, high, 55, 60);
        b[2] |= getBits128(low, high, 61, 64);
        r[2] |= getBits128(low, high, 65, 69);
        b[3] |= getBits128(low, high, 70, 70) << 2;
        r[3] |= getBits128(low, high, 71, 75);
        b[3] |= getBits128(low, high, 76, 76) << 3;
        deltaBitsR = deltaBitsG = 5;
        deltaBitsB              = 6;
        break;

    case 9:
        r[0] |= getBits128(low, high, 5, 10);
        g[3] |= getBits128(low, high, 11, 11) << 4;
        b[3] |= getBits128(low, high, 12, 13);
        b[2] |= getBits128(low, high, 14, 14) << 4;
        g[0] |= getBits128(low, high, 15, 20);
        g[2] |= getBits128(low, high, 21, 21) << 5;
        b[2] |= getBits128(low, high, 22, 22) << 5;
        b[3] |= getBits128(low, high, 23, 23) << 2;
        g[2] |= getBits128(low, high, 24, 24) << 4;
        b[0] |= getBits128(low, high, 25, 30);
        g[3] |= getBits128(low, high, 31, 31) << 5;
        b[3] |= getBits128(low, high, 32, 32) << 3;
        b[3] |= getBits128(low, high, 33, 33) << 5;
        b[3] |= getBits128(low, high, 34, 34) << 4;
        r[1] |= getBits128(low, high, 35, 40);
        g[2] |= getBits128(low, high, 41, 44);
        g[1] |= getBits128(low, high, 45, 50);
        g[3] |= getBits128(low, high, 51, 54);
        b[1] |= getBits128(low, high, 55, 60);
        b[2] |= getBits128(low, high, 61, 64);
        r[2] |= getBits128(low, high, 65, 70);
        r[3] |= getBits128(low, high, 71, 76);
        deltaBitsR = deltaBitsG = deltaBitsB = 6;
        break;

    case 10:
        r[0] |= getBits128(low, high, 5, 14);
        g[0] |= getBits128(low, high, 15, 24);
        b[0] |= getBits128(low, high, 25, 34);
        r[1] |= getBits128(low, high, 35, 44);
        g[1] |= getBits128(low, high, 45, 54);
        b[1] |= getBits128(low, high, 55, 64);
        deltaBitsR = deltaBitsG = deltaBitsB = 10;
        break;

    case 11:
        r[0] |= getBits128(low, high, 5, 14);
        g[0] |= getBits128(low, high, 15, 24);
        b[0] |= getBits128(low, high, 25, 34);
        r[1] |= getBits128(low, high, 35, 43);
        r[0] |= getBits128(low, high, 44, 44) << 10;
        g[1] |= getBits128(low, high, 45, 53);
        g[0] |= getBits128(low, high, 54, 54) << 10;
        b[1] |= getBits128(low, high, 55, 63);
        b[0] |= getBits128(low, high, 64, 64) << 10;
        deltaBitsR = deltaBitsG = deltaBitsB = 9;
        break;

    case 12:
        r[0] |= getBits128(low, high, 5, 14);
        g[0] |= getBits128(low, high, 15, 24);
        b[0] |= getBits128(low, high, 25, 34);
        r[1] |= getBits128(low, high, 35, 42);
        r[0] |= getBits128(low, high, 44, 43) << 10;
        g[1] |= getBits128(low, high, 45, 52);
        g[0] |= getBits128(low, high, 54, 53) << 10;
        b[1] |= getBits128(low, high, 55, 62);
        b[0] |= getBits128(low, high, 64, 63) << 10;
        deltaBitsR = deltaBitsG = deltaBitsB = 8;
        break;

    case 13:
        r[0] |= getBits128(low, high, 5, 14);
        g[0] |= getBits128(low, high, 15, 24);
        b[0] |= getBits128(low, high, 25, 34);
        r[1] |= getBits128(low, high, 35, 38);
        r[0] |= getBits128(low, high, 44, 39) << 10;
        g[1] |= getBits128(low, high, 45, 48);
        g[0] |= getBits128(low, high, 54, 49) << 10;
        b[1] |= getBits128(low, high, 55, 58);
        b[0] |= getBits128(low, high, 64, 59) << 10;
        deltaBitsR = deltaBitsG = deltaBitsB = 4;
        break;
    }

    if (mode != -1)
    {
        if (hasSign)
        {
            r[0] = signExtend(r[0], epBits[mode], 32);
            g[0] = signExtend(g[0], epBits[mode], 32);
            b[0] = signExtend(b[0], epBits[mode], 32);
        }

        if (transformed)
        {
            for (uint32_t i = 1; i < numEndpoints; i++)
            {
                r[i] = signExtend(r[i], deltaBitsR, 32);
                r[i] = (r[0] + r[i]) & (((uint32_t)1 << epBits[mode]) - 1);
                g[i] = signExtend(g[i], deltaBitsG, 32);
                g[i] = (g[0] + g[i]) & (((uint32_t)1 << epBits[mode]) - 1);
                b[i] = signExtend(b[i], deltaBitsB, 32);
                b[i] = (b[0] + b[i]) & (((uint32_t)1 << epBits[mode]) - 1);
            }
        }

        if (hasSign)
        {
            for (uint32_t i = 1; i < 4; i++)
            {
                r[i] = signExtend(r[i], epBits[mode], 32);
                g[i] = signExtend(g[i], epBits[mode], 32);
                b[i] = signExtend(b[i], epBits[mode], 32);
            }
        }

        for (uint32_t i = 0; i < numEndpoints; i++)
        {
            r[i] = unquantize(r[i], mode, hasSign);
            g[i] = unquantize(g[i], mode, hasSign);
            b[i] = unquantize(b[i], mode, hasSign);
        }
    }

    for (uint32_t i = 0; i < 16; i++)
    {
        const uint32_t subsetIndex   = (numRegions == 1 ? 0 : partitions2[d][i]);
        const uint32_t bits          = (i == anchorIndex[subsetIndex]) ? (colorIndexBC - 1) : colorIndexBC;
        const uint32_t colorIndex    = (uint32_t)(colorIndexData & ((1 << bits) - 1));
        const int32_t endpointStartR = r[2 * subsetIndex];
        const int32_t endpointEndR   = r[2 * subsetIndex + 1];
        const int32_t endpointStartG = g[2 * subsetIndex];
        const int32_t endpointEndG   = g[2 * subsetIndex + 1];
        const int32_t endpointStartB = b[2 * subsetIndex];
        const int32_t endpointEndB   = b[2 * subsetIndex + 1];
        const int16_t r16 =
            finishUnquantize(interpolate(endpointStartR, endpointEndR, colorIndex, colorIndexBC), hasSign);
        const int16_t g16 =
            finishUnquantize(interpolate(endpointStartG, endpointEndG, colorIndex, colorIndexBC), hasSign);
        const int16_t b16 =
            finishUnquantize(interpolate(endpointStartB, endpointEndB, colorIndex, colorIndexBC), hasSign);
        const int32_t y         = i / 4;
        const int32_t x         = i % 4;
        int16_t *const dstPixel = (int16_t *)(dstPtr + y * dstRowPitch + x * dstPixelSize);

        if (mode == -1)
        {
            dstPixel[0] = 0;
            dstPixel[1] = 0;
            dstPixel[2] = 0;
        }
        else
        {
            dstPixel[0] = r16;
            dstPixel[1] = g16;
            dstPixel[2] = b16;
        }

        colorIndexData >>= bits;
    }
}

void decompressBc7(const PixelBufferAccess &dst, const uint8_t *src)
{
    using namespace BcDecompressInternal;

    static const uint8_t subsets[]          = {3, 2, 3, 2, 1, 1, 1, 2};
    static const uint8_t partitionBits[]    = {4, 6, 6, 6, 0, 0, 0, 6};
    static const uint8_t endpointBits[8][5] = {//r, g, b, a, p
                                               {4, 4, 4, 0, 1}, {6, 6, 6, 0, 1}, {5, 5, 5, 0, 0}, {7, 7, 7, 0, 1},
                                               {5, 5, 5, 6, 0}, {7, 7, 7, 8, 0}, {7, 7, 7, 7, 1}, {5, 5, 5, 5, 1}};
    static const uint8_t indexBits[]        = {3, 3, 2, 2, 2, 2, 4, 2};

    uint8_t *const dstPtr       = (uint8_t *)dst.getDataPtr();
    const uint32_t dstRowPitch  = dst.getRowPitch();
    const uint32_t dstPixelSize = 4;

    const uint64_t low  = ((uint64_t *)src)[0];
    const uint64_t high = ((uint64_t *)src)[1];
    const int32_t mode  = extractModeBc7(src[0]);
    uint32_t numSubsets = 1;
    uint32_t offset     = mode + 1;
    uint32_t rotation   = 0;
    uint32_t idxMode    = 0;
    uint32_t endpoints[6][5];
    uint32_t partitionSetId = 0;

    // Decode partition data from explicit partition bits
    if (mode == 0 || mode == 1 || mode == 2 || mode == 3 || mode == 7)
    {
        numSubsets     = subsets[mode];
        partitionSetId = getBits128(low, high, offset, offset + partitionBits[mode] - 1);
        offset += partitionBits[mode];
    }

    // Extract rotation bits
    if (mode == 4 || mode == 5)
    {
        rotation = getBits128(low, high, offset, offset + 1);
        offset += 2;
        if (mode == 4)
        {
            idxMode = getBits128(low, high, offset, offset);
            offset++;
        }
    }

    {
        const uint32_t numEndpoints = numSubsets * 2;

        // Extract raw, compressed endpoint bits
        for (uint32_t cpnt = 0; cpnt < 5; cpnt++)
        {
            for (uint32_t ep = 0; ep < numEndpoints; ep++)
            {
                if (mode == 1 && cpnt == 4 && ep > 1)
                    continue; // Mode 1 has shared P bits

                int n = mode == -1 ? 0 : endpointBits[mode][cpnt];
                if (n > 0)
                    endpoints[ep][cpnt] = getBits128(low, high, offset, offset + n - 1);
                offset += n;
            }
        }

        // Decode endpoints
        if (mode == 0 || mode == 1 || mode == 3 || mode == 6 || mode == 7)
        {
            // First handle modes that have P-bits
            for (uint32_t ep = 0; ep < numEndpoints; ep++)
            {
                for (uint32_t cpnt = 0; cpnt < 4; cpnt++)
                {
                    endpoints[ep][cpnt] <<= 1;
                }
            }

            if (mode == 1)
            {
                // P-bit is shared
                const uint32_t pbitZero = endpoints[0][4];
                const uint32_t pbitOne  = endpoints[1][4];

                for (uint32_t cpnt = 0; cpnt < 3; cpnt++)
                {
                    endpoints[0][cpnt] |= pbitZero;
                    endpoints[1][cpnt] |= pbitZero;
                    endpoints[2][cpnt] |= pbitOne;
                    endpoints[3][cpnt] |= pbitOne;
                }
            }
            else
            {
                // Unique p-bit per endpoint
                for (uint32_t ep = 0; ep < numEndpoints; ep++)
                {
                    for (uint32_t cpnt = 0; cpnt < 4; cpnt++)
                    {
                        endpoints[ep][cpnt] |= endpoints[ep][4];
                    }
                }
            }
        }

        if (mode != -1)
        {
            for (uint32_t ep = 0; ep < numEndpoints; ep++)
            {
                // Left shift endpoint components so that their MSB lies in bit 7
                for (uint32_t cpnt = 0; cpnt < 4; cpnt++)
                    endpoints[ep][cpnt] <<= 8 - (endpointBits[mode][cpnt] + endpointBits[mode][4]);

                // Replicate each component's MSB into the LSBs revealed by the left-shift operation above
                for (uint32_t cpnt = 0; cpnt < 4; cpnt++)
                    endpoints[ep][cpnt] |= endpoints[ep][cpnt] >> (endpointBits[mode][cpnt] + endpointBits[mode][4]);
            }

            // If this mode does not explicitly define the alpha component set alpha equal to 1.0
            if (mode < 4)
            {
                for (uint32_t ep = 0; ep < numEndpoints; ep++)
                    endpoints[ep][3] = 255;
            }
        }
    }

    {
        uint32_t colorIdxOffset = offset + ((mode == 4 && idxMode) ? 31 : 0);
        uint32_t alphaIdxOffset = offset + ((mode == 5 || (mode == 4 && !idxMode)) ? 31 : 0);

        for (uint32_t pixel = 0; pixel < 16; pixel++)
        {
            const uint32_t y         = pixel / 4;
            const uint32_t x         = pixel % 4;
            uint32_t *const dstPixel = (uint32_t *)(dstPtr + y * dstRowPitch + x * dstPixelSize);
            uint32_t subsetIndex     = 0;
            uint32_t anchorIndex     = 0;
            uint32_t endpointStart[4];
            uint32_t endpointEnd[4];

            if (mode == -1)
            {
                *dstPixel = 0;
                continue;
            }

            if (numSubsets == 2)
                subsetIndex = partitions2[partitionSetId][pixel];
            else if (numSubsets == 3)
                subsetIndex = partitions3[partitionSetId][pixel];

            if (numSubsets == 2 && subsetIndex == 1)
            {
                anchorIndex = anchorIndicesSecondSubset2[partitionSetId];
            }
            else if (numSubsets == 3)
            {
                if (subsetIndex == 1)
                    anchorIndex = anchorIndicesSecondSubset3[partitionSetId];
                else if (subsetIndex == 2)
                    anchorIndex = anchorIndicesThirdSubset[partitionSetId];
            }

            for (uint32_t cpnt = 0; cpnt < 4; cpnt++)
            {
                endpointStart[cpnt] = endpoints[2 * subsetIndex][cpnt];
                endpointEnd[cpnt]   = endpoints[2 * subsetIndex + 1][cpnt];
            }

            {
                const uint32_t colorInterpolationBits = indexBits[mode] + idxMode;
                const uint32_t colorIndexBits         = colorInterpolationBits - ((anchorIndex == pixel) ? 1 : 0);
                const uint32_t alphaInterpolationBits =
                    mode == 4 ? 3 - idxMode : (mode == 5 ? 2 : colorInterpolationBits);
                const uint32_t alphaIndexBits = alphaInterpolationBits - ((anchorIndex == pixel) ? 1 : 0);
                const uint32_t colorIdx = getBits128(low, high, colorIdxOffset, colorIdxOffset + colorIndexBits - 1);
                const uint32_t alphaIdx = (mode == 4 || mode == 5) ? getBits128(low, high, alphaIdxOffset,
                                                                                alphaIdxOffset + alphaIndexBits - 1) :
                                                                     colorIdx;
                const uint32_t r = interpolate(endpointStart[0], endpointEnd[0], colorIdx, colorInterpolationBits);
                const uint32_t g = interpolate(endpointStart[1], endpointEnd[1], colorIdx, colorInterpolationBits);
                const uint32_t b = interpolate(endpointStart[2], endpointEnd[2], colorIdx, colorInterpolationBits);
                const uint32_t a = interpolate(endpointStart[3], endpointEnd[3], alphaIdx, alphaInterpolationBits);

                colorIdxOffset += colorIndexBits;
                alphaIdxOffset += alphaIndexBits;

                if ((mode == 4 || mode == 5) && rotation != 0)
                {
                    if (rotation == 1)
                        *dstPixel = a | (g << 8) | (b << 16) | (r << 24);
                    else if (rotation == 2)
                        *dstPixel = r | (a << 8) | (b << 16) | (g << 24);
                    else
                        *dstPixel = r | (g << 8) | (a << 16) | (b << 24);
                }
                else
                {
                    *dstPixel = r | (g << 8) | (b << 16) | (a << 24);
                }
            }
        }
    }
}

void decompressAhbRaw10(const PixelBufferAccess &dst, const uint8_t *src)
{
    // Packed format with 4 pixels in 5 bytes
    // Layout: https://developer.android.com/reference/android/graphics/ImageFormat#RAW10

    uint32_t firstPixel  = (*(src + 0u));
    uint32_t secondPixel = (*(src + 1u));
    uint32_t thirdPixel  = (*(src + 2u));
    uint32_t fourthPixel = (*(src + 3u));
    uint32_t packedPixel = (*(src + 4u));

    // We now need to take last bits for each pixel from the packed pixel to build all pixel values
    firstPixel  = ((firstPixel << 2u) | ((packedPixel >> 0u) & 0b00000011u));
    secondPixel = ((secondPixel << 2u) | ((packedPixel >> 2u) & 0b00000011u));
    thirdPixel  = ((thirdPixel << 2u) | ((packedPixel >> 4u) & 0b00000011u));
    fourthPixel = ((fourthPixel << 2u) | ((packedPixel >> 6u) & 0b00000011u));

    // Store values in buffer (higher bits is were data is stored)
    uint16_t *pixel = static_cast<uint16_t *>(dst.getDataPtr());
    (*pixel)        = static_cast<uint16_t>(firstPixel << 6u);

    pixel++;
    (*pixel) = static_cast<uint16_t>(secondPixel << 6u);

    pixel++;
    (*pixel) = static_cast<uint16_t>(thirdPixel << 6u);

    pixel++;
    (*pixel) = static_cast<uint16_t>(fourthPixel << 6u);
}

void decompressAhbRaw12(const PixelBufferAccess &dst, const uint8_t *src)
{
    // Packed format with 2 pixels in 3 bytes
    // Layout: https://developer.android.com/reference/android/graphics/ImageFormat#RAW12

    uint32_t firstPixel  = (*(src + 0));
    uint32_t secondPixel = (*(src + 1));
    uint32_t packedPixel = (*(src + 2));

    // We now need to take last bits for each pixel from the packed pixel to build all pixel values
    firstPixel  = ((firstPixel << 4u) | ((packedPixel >> 0u) & 0b00001111u));
    secondPixel = ((secondPixel << 4u) | ((packedPixel >> 4u) & 0b00001111u));

    // Store values in buffer (higher bits is were data is stored)
    uint16_t *pixel = static_cast<uint16_t *>(dst.getDataPtr());
    (*pixel)        = static_cast<uint16_t>(firstPixel << 6u);

    pixel++;
    (*pixel) = static_cast<uint16_t>(secondPixel << 6u);
}

void decompressBlock(CompressedTexFormat format, const PixelBufferAccess &dst, const uint8_t *src,
                     const TexDecompressionParams &params)
{
    switch (format)
    {
    case COMPRESSEDTEXFORMAT_ETC1_RGB8:
        decompressETC1(dst, src);
        break;
    case COMPRESSEDTEXFORMAT_EAC_R11:
        decompressEAC_R11(dst, src, false);
        break;
    case COMPRESSEDTEXFORMAT_EAC_SIGNED_R11:
        decompressEAC_R11(dst, src, true);
        break;
    case COMPRESSEDTEXFORMAT_EAC_RG11:
        decompressEAC_RG11(dst, src, false);
        break;
    case COMPRESSEDTEXFORMAT_EAC_SIGNED_RG11:
        decompressEAC_RG11(dst, src, true);
        break;
    case COMPRESSEDTEXFORMAT_ETC2_RGB8:
        decompressETC2(dst, src);
        break;
    case COMPRESSEDTEXFORMAT_ETC2_SRGB8:
        decompressETC2(dst, src);
        break;
    case COMPRESSEDTEXFORMAT_ETC2_RGB8_PUNCHTHROUGH_ALPHA1:
        decompressETC2_RGB8_PUNCHTHROUGH_ALPHA1(dst, src);
        break;
    case COMPRESSEDTEXFORMAT_ETC2_SRGB8_PUNCHTHROUGH_ALPHA1:
        decompressETC2_RGB8_PUNCHTHROUGH_ALPHA1(dst, src);
        break;
    case COMPRESSEDTEXFORMAT_ETC2_EAC_RGBA8:
        decompressETC2_EAC_RGBA8(dst, src);
        break;
    case COMPRESSEDTEXFORMAT_ETC2_EAC_SRGB8_ALPHA8:
        decompressETC2_EAC_RGBA8(dst, src);
        break;

    case COMPRESSEDTEXFORMAT_ASTC_4x4_RGBA:
    case COMPRESSEDTEXFORMAT_ASTC_5x4_RGBA:
    case COMPRESSEDTEXFORMAT_ASTC_5x5_RGBA:
    case COMPRESSEDTEXFORMAT_ASTC_6x5_RGBA:
    case COMPRESSEDTEXFORMAT_ASTC_6x6_RGBA:
    case COMPRESSEDTEXFORMAT_ASTC_8x5_RGBA:
    case COMPRESSEDTEXFORMAT_ASTC_8x6_RGBA:
    case COMPRESSEDTEXFORMAT_ASTC_8x8_RGBA:
    case COMPRESSEDTEXFORMAT_ASTC_10x5_RGBA:
    case COMPRESSEDTEXFORMAT_ASTC_10x6_RGBA:
    case COMPRESSEDTEXFORMAT_ASTC_10x8_RGBA:
    case COMPRESSEDTEXFORMAT_ASTC_10x10_RGBA:
    case COMPRESSEDTEXFORMAT_ASTC_12x10_RGBA:
    case COMPRESSEDTEXFORMAT_ASTC_12x12_RGBA:
    case COMPRESSEDTEXFORMAT_ASTC_4x4_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_5x4_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_5x5_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_6x5_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_6x6_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_8x5_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_8x6_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_8x8_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_10x5_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_10x6_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_10x8_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_10x10_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_12x10_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_12x12_SRGB8_ALPHA8:
    case COMPRESSEDTEXFORMAT_ASTC_4x4_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_5x4_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_5x5_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_6x5_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_6x6_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_8x5_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_8x6_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_8x8_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_10x5_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_10x6_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_10x8_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_10x10_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_12x10_SFLOAT_BLOCK:
    case COMPRESSEDTEXFORMAT_ASTC_12x12_SFLOAT_BLOCK:
        astc::decompress(dst, src, format, params.astcMode);
        break;

    case COMPRESSEDTEXFORMAT_ASTC_3x3x3_UNORM_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_3x3x3_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_3x3x3_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x3x3_UNORM_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x3x3_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x3x3_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x4x3_UNORM_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x4x3_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x4x3_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x4x4_UNORM_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x4x4_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_4x4x4_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x4x4_UNORM_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x4x4_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x4x4_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x5x4_UNORM_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x5x4_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x5x4_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x5x5_UNORM_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x5x5_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_5x5x5_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x5x5_UNORM_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x5x5_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x5x5_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x6x5_UNORM_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x6x5_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x6x5_SFLOAT_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x6x6_UNORM_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x6x6_SRGB_BLOCK_EXT:
    case COMPRESSEDTEXFORMAT_ASTC_6x6x6_SFLOAT_BLOCK_EXT:
        astc::decompress3D(dst, src, format, params.astcMode);
        break;

    case COMPRESSEDTEXFORMAT_BC1_RGB_UNORM_BLOCK:
        decompressBc1(dst, src, false);
        break;
    case COMPRESSEDTEXFORMAT_BC1_RGB_SRGB_BLOCK:
        decompressBc1(dst, src, false);
        break;
    case COMPRESSEDTEXFORMAT_BC1_RGBA_UNORM_BLOCK:
        decompressBc1(dst, src, true);
        break;
    case COMPRESSEDTEXFORMAT_BC1_RGBA_SRGB_BLOCK:
        decompressBc1(dst, src, true);
        break;
    case COMPRESSEDTEXFORMAT_BC2_UNORM_BLOCK:
        decompressBc2(dst, src);
        break;
    case COMPRESSEDTEXFORMAT_BC2_SRGB_BLOCK:
        decompressBc2(dst, src);
        break;
    case COMPRESSEDTEXFORMAT_BC3_UNORM_BLOCK:
        decompressBc3(dst, src);
        break;
    case COMPRESSEDTEXFORMAT_BC3_SRGB_BLOCK:
        decompressBc3(dst, src);
        break;
    case COMPRESSEDTEXFORMAT_BC4_UNORM_BLOCK:
        decompressBc4(dst, src, false);
        break;
    case COMPRESSEDTEXFORMAT_BC4_SNORM_BLOCK:
        decompressBc4(dst, src, true);
        break;
    case COMPRESSEDTEXFORMAT_BC5_UNORM_BLOCK:
        decompressBc5(dst, src, false);
        break;
    case COMPRESSEDTEXFORMAT_BC5_SNORM_BLOCK:
        decompressBc5(dst, src, true);
        break;
    case COMPRESSEDTEXFORMAT_BC6H_UFLOAT_BLOCK:
        decompressBc6H(dst, src, false);
        break;
    case COMPRESSEDTEXFORMAT_BC6H_SFLOAT_BLOCK:
        decompressBc6H(dst, src, true);
        break;
    case COMPRESSEDTEXFORMAT_BC7_UNORM_BLOCK:
        decompressBc7(dst, src);
        break;
    case COMPRESSEDTEXFORMAT_BC7_SRGB_BLOCK:
        decompressBc7(dst, src);
        break;

    case COMPRESSEDTEXFORMAT_AHB_RAW10:
        decompressAhbRaw10(dst, src);
        break;
    case COMPRESSEDTEXFORMAT_AHB_RAW12:
        decompressAhbRaw12(dst, src);
        break;

    default:
        DE_FATAL("Unexpected format");
        break;
    }
}

int componentSum(const IVec3 &vec)
{
    return vec.x() + vec.y() + vec.z();
}

} // namespace

void decompress(const PixelBufferAccess &dst, CompressedTexFormat fmt, const uint8_t *src,
                const TexDecompressionParams &params)
{
    const int blockSize = getBlockSize(fmt);
    const IVec3 blockPixelSize(getBlockPixelSize(fmt));
    const IVec3 blockCount(deDivRoundUp32(dst.getWidth(), blockPixelSize.x()),
                           deDivRoundUp32(dst.getHeight(), blockPixelSize.y()),
                           deDivRoundUp32(dst.getDepth(), blockPixelSize.z()));
    const IVec3 blockPitches(blockSize, blockSize * blockCount.x(), blockSize * blockCount.x() * blockCount.y());

    std::vector<uint8_t> uncompressedBlock(dst.getFormat().getPixelSize() * blockPixelSize.x() * blockPixelSize.y() *
                                           blockPixelSize.z());
    const PixelBufferAccess blockAccess(getUncompressedFormat(fmt), blockPixelSize.x(), blockPixelSize.y(),
                                        blockPixelSize.z(), &uncompressedBlock[0]);

    DE_ASSERT(dst.getFormat() == getUncompressedFormat(fmt));

    for (int blockZ = 0; blockZ < blockCount.z(); blockZ++)
        for (int blockY = 0; blockY < blockCount.y(); blockY++)
            for (int blockX = 0; blockX < blockCount.x(); blockX++)
            {
                const IVec3 blockPos(blockX, blockY, blockZ);
                const uint8_t *const blockPtr = src + componentSum(blockPos * blockPitches);
                const IVec3 copySize(de::min(blockPixelSize.x(), dst.getWidth() - blockPos.x() * blockPixelSize.x()),
                                     de::min(blockPixelSize.y(), dst.getHeight() - blockPos.y() * blockPixelSize.y()),
                                     de::min(blockPixelSize.z(), dst.getDepth() - blockPos.z() * blockPixelSize.z()));
                const IVec3 dstPixelPos = blockPos * blockPixelSize;

                decompressBlock(fmt, blockAccess, blockPtr, params);

                copy(getSubregion(dst, dstPixelPos.x(), dstPixelPos.y(), dstPixelPos.z(), copySize.x(), copySize.y(),
                                  copySize.z()),
                     getSubregion(blockAccess, 0, 0, 0, copySize.x(), copySize.y(), copySize.z()));
            }
}

CompressedTexture::CompressedTexture(void)
    : m_format(COMPRESSEDTEXFORMAT_LAST_VULKAN)
    , m_width(0)
    , m_height(0)
    , m_depth(0)
{
}

CompressedTexture::CompressedTexture(CompressedTexFormat format, int width, int height, int depth)
    : m_format(COMPRESSEDTEXFORMAT_LAST_VULKAN)
    , m_width(0)
    , m_height(0)
    , m_depth(0)
{
    setStorage(format, width, height, depth);
}

CompressedTexture::CompressedTexture(CompressedTexFormat format, int width, int height, int depth,
                                     std::vector<uint8_t> &&data)
    : m_format(COMPRESSEDTEXFORMAT_LAST_VULKAN)
    , m_width(0)
    , m_height(0)
    , m_depth(0)
    , m_data(data)
{
    setStorage(format, width, height, depth);
}

CompressedTexture::~CompressedTexture(void)
{
}

void CompressedTexture::setStorage(CompressedTexFormat format, int width, int height, int depth)
{
    m_format = format;
    m_width  = width;
    m_height = height;
    m_depth  = depth;

    if (m_format != COMPRESSEDTEXFORMAT_LAST_VULKAN)
    {
        const IVec3 blockPixelSize = getBlockPixelSize(m_format);
        const int blockSize        = getBlockSize(m_format);

        if (m_data.size() > 0)
        {
            DE_ASSERT(static_cast<int>(m_data.size()) >=
                      (deDivRoundUp32(m_width, blockPixelSize.x()) * deDivRoundUp32(m_height, blockPixelSize.y()) *
                       deDivRoundUp32(m_depth, blockPixelSize.z()) * blockSize));
        }
        else
        {
            m_data.resize(deDivRoundUp32(m_width, blockPixelSize.x()) * deDivRoundUp32(m_height, blockPixelSize.y()) *
                          deDivRoundUp32(m_depth, blockPixelSize.z()) * blockSize);
        }
    }
    else
    {
        DE_ASSERT(m_format == COMPRESSEDTEXFORMAT_LAST_VULKAN);
        DE_ASSERT(m_width == 0 && m_height == 0 && m_depth == 0);
        m_data.resize(0);
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Decode to uncompressed pixel data
 * \param dst Destination buffer
 *//*--------------------------------------------------------------------*/
void CompressedTexture::decompress(const PixelBufferAccess &dst, const TexDecompressionParams &params) const
{
    DE_ASSERT(dst.getWidth() == m_width && dst.getHeight() == m_height && dst.getDepth() == m_depth);
    DE_ASSERT(dst.getFormat() == getUncompressedFormat(m_format));

    tcu::decompress(dst, m_format, &m_data[0], params);
}

} // namespace tcu
