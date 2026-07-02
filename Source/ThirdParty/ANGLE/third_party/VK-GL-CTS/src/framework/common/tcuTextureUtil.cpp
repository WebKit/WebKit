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
 * \brief Texture utilities.
 *//*--------------------------------------------------------------------*/

#include "tcuTextureUtil.hpp"
#include "tcuVectorUtil.hpp"
#include "deRandom.hpp"
#include "deMath.h"
#include "deMemory.h"

#include <limits>
#include <cmath>

namespace tcu
{

float sRGBChannelToLinear(float cs)
{
    if (cs <= 0.04045)
        return cs / 12.92f;
    else
        return deFloatPow((cs + 0.055f) / 1.055f, 2.4f);
}

static const uint32_t s_srgb8Lut[256] = {
#include "tcuSRGB8Lut.inl"
};

static inline float sRGB8ChannelToLinear(uint32_t cs)
{
    DE_ASSERT(cs < 256);

    // \note This triggers UB, but in practice it doesn't cause any problems
    return ((const float *)s_srgb8Lut)[cs];
}

float linearChannelToSRGB(float cl)
{
    if (cl <= 0.0f)
        return 0.0f;
    else if (cl < 0.0031308f)
        return 12.92f * cl;
    else if (cl < 1.0f)
        return 1.055f * deFloatPow(cl, 0.41666f) - 0.055f;
    else
        return 1.0f;
}

//! Convert sRGB to linear colorspace
Vec4 sRGBToLinear(const Vec4 &cs)
{
    return Vec4(sRGBChannelToLinear(cs[0]), sRGBChannelToLinear(cs[1]), sRGBChannelToLinear(cs[2]), cs[3]);
}

Vec4 sRGB8ToLinear(const UVec4 &cs)
{
    return Vec4(sRGB8ChannelToLinear(cs[0]), sRGB8ChannelToLinear(cs[1]), sRGB8ChannelToLinear(cs[2]), 1.0f);
}

Vec4 sRGBA8ToLinear(const UVec4 &cs)
{
    return Vec4(sRGB8ChannelToLinear(cs[0]), sRGB8ChannelToLinear(cs[1]), sRGB8ChannelToLinear(cs[2]),
                (float)cs[3] / 255.0f);
}

//! Convert from linear to sRGB colorspace
Vec4 linearToSRGB(const Vec4 &cl)
{
    return Vec4(linearChannelToSRGB(cl[0]), linearChannelToSRGB(cl[1]), linearChannelToSRGB(cl[2]), cl[3]);
}

bool isSRGB(TextureFormat format)
{
    // make sure to update this if type table is updated
    DE_STATIC_ASSERT(TextureFormat::CHANNELORDER_LAST == 22);

    return format.order == TextureFormat::sR || format.order == TextureFormat::sRG ||
           format.order == TextureFormat::sRGB || format.order == TextureFormat::sRGBA ||
           format.order == TextureFormat::sBGR || format.order == TextureFormat::sBGRA;
}

tcu::Vec4 linearToSRGBIfNeeded(const TextureFormat &format, const tcu::Vec4 &color)
{
    return isSRGB(format) ? linearToSRGB(color) : color;
}

bool isCombinedDepthStencilType(TextureFormat::ChannelType type)
{
    // make sure to update this if type table is updated
    DE_STATIC_ASSERT(TextureFormat::CHANNELTYPE_LAST == 48);

    return type == TextureFormat::UNSIGNED_INT_16_8_8 || type == TextureFormat::UNSIGNED_INT_24_8 ||
           type == TextureFormat::UNSIGNED_INT_24_8_REV || type == TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV;
}

bool hasStencilComponent(TextureFormat::ChannelOrder order)
{
    DE_STATIC_ASSERT(TextureFormat::CHANNELORDER_LAST == 22);

    switch (order)
    {
    case TextureFormat::S:
    case TextureFormat::DS:
        return true;

    default:
        return false;
    }
}

bool hasDepthComponent(TextureFormat::ChannelOrder order)
{
    DE_STATIC_ASSERT(TextureFormat::CHANNELORDER_LAST == 22);

    switch (order)
    {
    case TextureFormat::D:
    case TextureFormat::DS:
        return true;

    default:
        return false;
    }
}

//! Get texture channel class for format - how the values are stored (not how they are sampled)
TextureChannelClass getTextureChannelClass(TextureFormat::ChannelType channelType)
{
    // make sure this table is updated if format table is updated
    DE_STATIC_ASSERT(TextureFormat::CHANNELTYPE_LAST == 48);

    switch (channelType)
    {
    case TextureFormat::SNORM_INT8:
        return TEXTURECHANNELCLASS_SIGNED_FIXED_POINT;
    case TextureFormat::SNORM_INT16:
        return TEXTURECHANNELCLASS_SIGNED_FIXED_POINT;
    case TextureFormat::SNORM_INT32:
        return TEXTURECHANNELCLASS_SIGNED_FIXED_POINT;
    case TextureFormat::UNORM_INT8:
        return TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT;
    case TextureFormat::UNORM_INT16:
        return TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT;
    case TextureFormat::UNORM_INT24:
        return TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT;
    case TextureFormat::UNORM_INT32:
        return TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT;
    case TextureFormat::UNORM_BYTE_44:
        return TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT;
    case TextureFormat::UNORM_SHORT_565:
        return TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT;
    case TextureFormat::UNORM_SHORT_555:
        return TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT;
    case TextureFormat::UNORM_SHORT_4444:
        return TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT;
    case TextureFormat::UNORM_SHORT_5551:
        return TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT;
    case TextureFormat::UNORM_SHORT_1555:
        return TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT;
    case TextureFormat::UNSIGNED_BYTE_44:
        return TEXTURECHANNELCLASS_UNSIGNED_INTEGER;
    case TextureFormat::UNSIGNED_SHORT_565:
        return TEXTURECHANNELCLASS_UNSIGNED_INTEGER;
    case TextureFormat::UNSIGNED_SHORT_4444:
        return TEXTURECHANNELCLASS_UNSIGNED_INTEGER;
    case TextureFormat::UNSIGNED_SHORT_5551:
        return TEXTURECHANNELCLASS_UNSIGNED_INTEGER;
    case TextureFormat::UNORM_INT_101010:
        return TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT;
    case TextureFormat::SNORM_INT_1010102_REV:
        return TEXTURECHANNELCLASS_SIGNED_FIXED_POINT;
    case TextureFormat::UNORM_INT_1010102_REV:
        return TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT;
    case TextureFormat::SIGNED_INT_1010102_REV:
        return TEXTURECHANNELCLASS_SIGNED_INTEGER;
    case TextureFormat::UNSIGNED_INT_1010102_REV:
        return TEXTURECHANNELCLASS_UNSIGNED_INTEGER;
    case TextureFormat::UNSIGNED_INT_11F_11F_10F_REV:
        return TEXTURECHANNELCLASS_FLOATING_POINT;
    case TextureFormat::UNSIGNED_INT_999_E5_REV:
        return TEXTURECHANNELCLASS_FLOATING_POINT;
    case TextureFormat::UNSIGNED_INT_16_8_8:
        return TEXTURECHANNELCLASS_LAST; //!< packed unorm16-x8-uint8
    case TextureFormat::UNSIGNED_INT_24_8:
        return TEXTURECHANNELCLASS_LAST; //!< packed unorm24-uint8
    case TextureFormat::UNSIGNED_INT_24_8_REV:
        return TEXTURECHANNELCLASS_LAST; //!< packed unorm24-uint8
    case TextureFormat::SIGNED_INT8:
        return TEXTURECHANNELCLASS_SIGNED_INTEGER;
    case TextureFormat::SIGNED_INT16:
        return TEXTURECHANNELCLASS_SIGNED_INTEGER;
    case TextureFormat::SIGNED_INT32:
        return TEXTURECHANNELCLASS_SIGNED_INTEGER;
    case TextureFormat::SIGNED_INT64:
        return TEXTURECHANNELCLASS_SIGNED_INTEGER;
    case TextureFormat::UNSIGNED_INT8:
        return TEXTURECHANNELCLASS_UNSIGNED_INTEGER;
    case TextureFormat::UNSIGNED_INT16:
        return TEXTURECHANNELCLASS_UNSIGNED_INTEGER;
    case TextureFormat::UNSIGNED_INT24:
        return TEXTURECHANNELCLASS_UNSIGNED_INTEGER;
    case TextureFormat::UNSIGNED_INT32:
        return TEXTURECHANNELCLASS_UNSIGNED_INTEGER;
    case TextureFormat::UNSIGNED_INT64:
        return TEXTURECHANNELCLASS_UNSIGNED_INTEGER;
    case TextureFormat::HALF_FLOAT:
        return TEXTURECHANNELCLASS_FLOATING_POINT;
    case TextureFormat::FLOAT:
        return TEXTURECHANNELCLASS_FLOATING_POINT;
    case TextureFormat::FLOAT64:
        return TEXTURECHANNELCLASS_FLOATING_POINT;
    case TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV:
        return TEXTURECHANNELCLASS_LAST; //!< packed float32-pad24-uint8
    case TextureFormat::UNORM_SHORT_10:
        return TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT;
    case TextureFormat::UNORM_SHORT_12:
        return TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT;
    case TextureFormat::USCALED_INT8:
        return TEXTURECHANNELCLASS_UNSIGNED_INTEGER;
    case TextureFormat::USCALED_INT16:
        return TEXTURECHANNELCLASS_UNSIGNED_INTEGER;
    case TextureFormat::SSCALED_INT8:
        return TEXTURECHANNELCLASS_SIGNED_INTEGER;
    case TextureFormat::SSCALED_INT16:
        return TEXTURECHANNELCLASS_SIGNED_INTEGER;
    case TextureFormat::USCALED_INT_1010102_REV:
        return TEXTURECHANNELCLASS_UNSIGNED_INTEGER;
    case TextureFormat::SSCALED_INT_1010102_REV:
        return TEXTURECHANNELCLASS_SIGNED_INTEGER;
    default:
        DE_FATAL("Unknown channel type");
        return TEXTURECHANNELCLASS_LAST;
    }
}

bool isAccessValid(TextureFormat format, TextureAccessType type)
{
    DE_ASSERT(isValid(format));

    if (format.order == TextureFormat::DS)
    {
        // It is never allowed to access combined depth-stencil format with getPixel().
        // Instead either getPixDepth() or getPixStencil(), or effective depth- or stencil-
        // access must be used.
        return false;
    }
    else if (format.order == TextureFormat::D)
        return type == TEXTUREACCESSTYPE_FLOAT;
    else if (format.order == TextureFormat::S)
        return type == TEXTUREACCESSTYPE_UNSIGNED_INT;
    else
    {
        // A few packed color formats have access type restrictions
        if (format.type == TextureFormat::UNSIGNED_INT_11F_11F_10F_REV ||
            format.type == TextureFormat::UNSIGNED_INT_999_E5_REV)
            return type == TEXTUREACCESSTYPE_FLOAT;
        else
            return true;
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Get access to subregion of pixel buffer
 * \param access    Parent access object
 * \param x            X offset
 * \param y            Y offset
 * \param z            Z offset
 * \param width        Width
 * \param height    Height
 * \param depth        Depth
 * \return Access object that targets given subregion of parent access object
 *//*--------------------------------------------------------------------*/
ConstPixelBufferAccess getSubregion(const ConstPixelBufferAccess &access, int x, int y, int z, int width, int height,
                                    int depth)
{
    DE_ASSERT(de::inBounds(x, 0, access.getWidth()));
    DE_ASSERT(de::inRange(x + width, x + 1, access.getWidth()));

    DE_ASSERT(de::inBounds(y, 0, access.getHeight()));
    DE_ASSERT(de::inRange(y + height, y + 1, access.getHeight()));

    DE_ASSERT(de::inBounds(z, 0, access.getDepth()));
    if (depth != -1) // Handles case of VK_REMAINING_ARRAY_LAYERS
        DE_ASSERT(de::inRange(z + depth, z + 1, access.getDepth()));

    return ConstPixelBufferAccess(access.getFormat(), tcu::IVec3(width, height, depth), access.getPitch(),
                                  (const uint8_t *)access.getDataPtr() + access.getPixelPitch() * x +
                                      access.getRowPitch() * y + access.getSlicePitch() * z);
}

/*--------------------------------------------------------------------*//*!
 * \brief Get access to subregion of pixel buffer
 * \param access    Parent access object
 * \param x            X offset
 * \param y            Y offset
 * \param z            Z offset
 * \param width        Width
 * \param height    Height
 * \param depth        Depth
 * \return Access object that targets given subregion of parent access object
 *//*--------------------------------------------------------------------*/
PixelBufferAccess getSubregion(const PixelBufferAccess &access, int x, int y, int z, int width, int height, int depth)
{
    DE_ASSERT(de::inBounds(x, 0, access.getWidth()));
    DE_ASSERT(de::inRange(x + width, x + 1, access.getWidth()));

    DE_ASSERT(de::inBounds(y, 0, access.getHeight()));
    DE_ASSERT(de::inRange(y + height, y + 1, access.getHeight()));

    DE_ASSERT(de::inBounds(z, 0, access.getDepth()));
    if (depth != -1) // Handles case of VK_REMAINING_ARRAY_LAYERS
        DE_ASSERT(de::inRange(z + depth, z + 1, access.getDepth()));

    return PixelBufferAccess(access.getFormat(), tcu::IVec3(width, height, depth), access.getPitch(),
                             (uint8_t *)access.getDataPtr() + access.getPixelPitch() * x + access.getRowPitch() * y +
                                 access.getSlicePitch() * z);
}

/*--------------------------------------------------------------------*//*!
 * \brief Get access to subregion of pixel buffer
 * \param access    Parent access object
 * \param x            X offset
 * \param y            Y offset
 * \param width        Width
 * \param height    Height
 * \return Access object that targets given subregion of parent access object
 *//*--------------------------------------------------------------------*/
PixelBufferAccess getSubregion(const PixelBufferAccess &access, int x, int y, int width, int height)
{
    return getSubregion(access, x, y, 0, width, height, 1);
}

/*--------------------------------------------------------------------*//*!
 * \brief Get access to subregion of pixel buffer
 * \param access    Parent access object
 * \param x            X offset
 * \param y            Y offset
 * \param width        Width
 * \param height    Height
 * \return Access object that targets given subregion of parent access object
 *//*--------------------------------------------------------------------*/
ConstPixelBufferAccess getSubregion(const ConstPixelBufferAccess &access, int x, int y, int width, int height)
{
    return getSubregion(access, x, y, 0, width, height, 1);
}

/*--------------------------------------------------------------------*//*!
 * \brief Flip rows in Y direction
 * \param access Access object
 * \return Modified access object where Y coordinates are reversed
 *//*--------------------------------------------------------------------*/
PixelBufferAccess flipYAccess(const PixelBufferAccess &access)
{
    const int rowPitch     = access.getRowPitch();
    const int offsetToLast = rowPitch * (access.getHeight() - 1);
    const tcu::IVec3 pitch(access.getPixelPitch(), -rowPitch, access.getSlicePitch());

    return PixelBufferAccess(access.getFormat(), access.getSize(), pitch,
                             (uint8_t *)access.getDataPtr() + offsetToLast);
}

/*--------------------------------------------------------------------*//*!
 * \brief Flip rows in Y direction
 * \param access Access object
 * \return Modified access object where Y coordinates are reversed
 *//*--------------------------------------------------------------------*/
ConstPixelBufferAccess flipYAccess(const ConstPixelBufferAccess &access)
{
    const int rowPitch     = access.getRowPitch();
    const int offsetToLast = rowPitch * (access.getHeight() - 1);
    const tcu::IVec3 pitch(access.getPixelPitch(), -rowPitch, access.getSlicePitch());

    return ConstPixelBufferAccess(access.getFormat(), access.getSize(), pitch,
                                  (uint8_t *)access.getDataPtr() + offsetToLast);
}

static Vec2 getFloatChannelValueRange(TextureFormat::ChannelType channelType)
{
    // make sure this table is updated if format table is updated
    DE_STATIC_ASSERT(TextureFormat::CHANNELTYPE_LAST == 48);

    float cMin = 0.0f;
    float cMax = 0.0f;

    switch (channelType)
    {
    // Signed normalized formats.
    case TextureFormat::SNORM_INT8:
    case TextureFormat::SNORM_INT16:
    case TextureFormat::SNORM_INT32:
    case TextureFormat::SNORM_INT_1010102_REV:
        cMin = -1.0f;
        cMax = 1.0f;
        break;

    // Unsigned normalized formats.
    case TextureFormat::UNORM_INT8:
    case TextureFormat::UNORM_INT16:
    case TextureFormat::UNORM_INT24:
    case TextureFormat::UNORM_INT32:
    case TextureFormat::UNORM_BYTE_44:
    case TextureFormat::UNORM_SHORT_565:
    case TextureFormat::UNORM_SHORT_555:
    case TextureFormat::UNORM_SHORT_4444:
    case TextureFormat::UNORM_SHORT_5551:
    case TextureFormat::UNORM_SHORT_1555:
    case TextureFormat::UNORM_INT_101010:
    case TextureFormat::UNORM_INT_1010102_REV:
    case TextureFormat::UNORM_SHORT_10:
    case TextureFormat::UNORM_SHORT_12:
        cMin = 0.0f;
        cMax = 1.0f;
        break;

    // Misc formats.
    case TextureFormat::SIGNED_INT8:
        cMin = -128.0f;
        cMax = 127.0f;
        break;
    case TextureFormat::SIGNED_INT16:
        cMin = -32768.0f;
        cMax = 32767.0f;
        break;
    case TextureFormat::SIGNED_INT32:
        cMin = -2147483520.0f;
        cMax = 2147483520.0f;
        break; // Maximum exactly representable 31-bit integer: (2^24 - 1) * 2^7
    case TextureFormat::UNSIGNED_INT8:
        cMin = 0.0f;
        cMax = 255.0f;
        break;
    case TextureFormat::UNSIGNED_INT16:
        cMin = 0.0f;
        cMax = 65535.0f;
        break;
    case TextureFormat::UNSIGNED_INT24:
        cMin = 0.0f;
        cMax = 16777215.0f;
        break;
    case TextureFormat::UNSIGNED_INT32:
        cMin = 0.0f;
        cMax = 4294967040.f;
        break; // Maximum exactly representable 32-bit integer: (2^24 - 1) * 2^8
    case TextureFormat::HALF_FLOAT:
        cMin = -1e3f;
        cMax = 1e3f;
        break;
    case TextureFormat::FLOAT:
        cMin = -1e5f;
        cMax = 1e5f;
        break;
    case TextureFormat::FLOAT64:
        cMin = -1e5f;
        cMax = 1e5f;
        break;
    case TextureFormat::UNSIGNED_INT_11F_11F_10F_REV:
        cMin = 0.0f;
        cMax = 1e4f;
        break;
    case TextureFormat::UNSIGNED_INT_999_E5_REV:
        cMin = 0.0f;
        cMax = 0.5e5f;
        break;
    case TextureFormat::UNSIGNED_BYTE_44:
        cMin = 0.0f;
        cMax = 15.f;
        break;
    case TextureFormat::UNSIGNED_SHORT_4444:
        cMin = 0.0f;
        cMax = 15.f;
        break;
    case TextureFormat::USCALED_INT8:
        cMin = 0.0f;
        cMax = 255.0f;
        break;
    case TextureFormat::USCALED_INT16:
        cMin = 0.0f;
        cMax = 65535.0f;
        break;
    case TextureFormat::SSCALED_INT8:
        cMin = -128.0f;
        cMax = 127.0f;
        break;
    case TextureFormat::SSCALED_INT16:
        cMin = -32768.0f;
        cMax = 32767.0f;
        break;
    case TextureFormat::USCALED_INT_1010102_REV:
        cMin = 0.0f;
        cMax = 1023.0f;
        break;
    case TextureFormat::SSCALED_INT_1010102_REV:
        cMin = -512.0f;
        cMax = 511.0f;
        break;

    default:
        DE_ASSERT(false);
    }

    return Vec2(cMin, cMax);
}

/*--------------------------------------------------------------------*//*!
 * \brief Get standard parameters for testing texture format
 *
 * Returns TextureFormatInfo that describes good parameters for exercising
 * given TextureFormat. Parameters include value ranges per channel and
 * suitable lookup scaling and bias in order to reduce result back to
 * 0..1 range.
 *//*--------------------------------------------------------------------*/
TextureFormatInfo getTextureFormatInfo(const TextureFormat &format)
{
    // Special cases.
    if (format.type == TextureFormat::UNSIGNED_INT_1010102_REV)
        return TextureFormatInfo(Vec4(0.0f, 0.0f, 0.0f, 0.0f), Vec4(1023.0f, 1023.0f, 1023.0f, 3.0f),
                                 Vec4(1.0f / 1023.f, 1.0f / 1023.0f, 1.0f / 1023.0f, 1.0f / 3.0f),
                                 Vec4(0.0f, 0.0f, 0.0f, 0.0f));
    if (format.type == TextureFormat::SIGNED_INT_1010102_REV)
        return TextureFormatInfo(Vec4(-512.0f, -512.0f, -512.0f, -2.0f), Vec4(511.0f, 511.0f, 511.0f, 1.0f),
                                 Vec4(1.0f / 1023.f, 1.0f / 1023.0f, 1.0f / 1023.0f, 1.0f / 3.0f),
                                 Vec4(0.5f, 0.5f, 0.5f, 0.5f));
    else if (format.order == TextureFormat::D || format.order == TextureFormat::DS)
        return TextureFormatInfo(Vec4(0.0f, 0.0f, 0.0f, 0.0f), Vec4(1.0f, 1.0f, 1.0f, 255.0f),
                                 Vec4(1.0f, 1.0f, 1.0f, 1.0f),
                                 Vec4(0.0f, 0.0f, 0.0f, 0.0f)); // Depth / stencil formats.
    else if (format == TextureFormat(TextureFormat::RGBA, TextureFormat::UNORM_SHORT_5551))
        return TextureFormatInfo(Vec4(0.0f, 0.0f, 0.0f, 0.5f), Vec4(1.0f, 1.0f, 1.0f, 1.5f),
                                 Vec4(1.0f, 1.0f, 1.0f, 1.0f), Vec4(0.0f, 0.0f, 0.0f, 0.0f));
    else if (format.type == TextureFormat::UNSIGNED_SHORT_5551)
        return TextureFormatInfo(Vec4(0.0f, 0.0f, 0.0f, 0.0f), Vec4(31.0f, 31.0f, 31.0f, 1.0f),
                                 Vec4(1.0f / 31.f, 1.0f / 31.0f, 1.0f / 31.0f, 1.0f), Vec4(0.0f, 0.0f, 0.0f, 0.0f));
    else if (format.type == TextureFormat::UNSIGNED_SHORT_565)
        return TextureFormatInfo(Vec4(0.0f, 0.0f, 0.0f, 0.0f), Vec4(31.0f, 63.0f, 31.0f, 0.0f),
                                 Vec4(1.0f / 31.f, 1.0f / 63.0f, 1.0f / 31.0f, 1.0f), Vec4(0.0f, 0.0f, 0.0f, 0.0f));

    const Vec2 cRange                  = getFloatChannelValueRange(format.type);
    const TextureSwizzle::Channel *map = getChannelReadSwizzle(format.order).components;
    const BVec4 chnMask = BVec4(deInRange32(map[0], TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_3) == true,
                                deInRange32(map[1], TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_3) == true,
                                deInRange32(map[2], TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_3) == true,
                                deInRange32(map[3], TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_3) == true);
    const float scale   = 1.0f / (cRange[1] - cRange[0]);
    const float bias    = -cRange[0] * scale;

    return TextureFormatInfo(select(cRange[0], 0.0f, chnMask), select(cRange[1], 0.0f, chnMask),
                             select(scale, 1.0f, chnMask), select(bias, 0.0f, chnMask));
}

IVec4 getFormatMinIntValue(const TextureFormat &format)
{
    DE_ASSERT(getTextureChannelClass(format.type) == TEXTURECHANNELCLASS_SIGNED_INTEGER);

    switch (format.type)
    {
    case TextureFormat::SIGNED_INT8:
        return IVec4(std::numeric_limits<int8_t>::min());
    case TextureFormat::SIGNED_INT16:
        return IVec4(std::numeric_limits<int16_t>::min());
    case TextureFormat::SIGNED_INT32:
        return IVec4(std::numeric_limits<int32_t>::min());

    default:
        DE_FATAL("Invalid channel type");
        return IVec4(0);
    }
}

IVec4 getFormatMaxIntValue(const TextureFormat &format)
{
    DE_ASSERT(getTextureChannelClass(format.type) == TEXTURECHANNELCLASS_SIGNED_INTEGER);

    if (format == TextureFormat(TextureFormat::RGBA, TextureFormat::SIGNED_INT_1010102_REV) ||
        format == TextureFormat(TextureFormat::BGRA, TextureFormat::SSCALED_INT_1010102_REV) ||
        format == TextureFormat(TextureFormat::RGBA, TextureFormat::SSCALED_INT_1010102_REV) ||
        format == TextureFormat(TextureFormat::BGRA, TextureFormat::SIGNED_INT_1010102_REV))
        return IVec4(511, 511, 511, 1);

    switch (format.type)
    {
    case TextureFormat::SIGNED_INT8:
        return IVec4(std::numeric_limits<int8_t>::max());
    case TextureFormat::SIGNED_INT16:
        return IVec4(std::numeric_limits<int16_t>::max());
    case TextureFormat::SIGNED_INT32:
        return IVec4(std::numeric_limits<int32_t>::max());

    case TextureFormat::SSCALED_INT8:
        return IVec4(std::numeric_limits<int8_t>::max());
    case TextureFormat::SSCALED_INT16:
        return IVec4(std::numeric_limits<int16_t>::max());

    default:
        DE_FATAL("Invalid channel type");
        return IVec4(0);
    }
}

UVec4 getFormatMaxUintValue(const TextureFormat &format)
{
    DE_ASSERT(getTextureChannelClass(format.type) == TEXTURECHANNELCLASS_UNSIGNED_INTEGER);

    if (format == TextureFormat(TextureFormat::RGBA, TextureFormat::UNSIGNED_INT_1010102_REV) ||
        format == TextureFormat(TextureFormat::RGBA, TextureFormat::USCALED_INT_1010102_REV) ||
        format == TextureFormat(TextureFormat::BGRA, TextureFormat::USCALED_INT_1010102_REV) ||
        format == TextureFormat(TextureFormat::BGRA, TextureFormat::UNSIGNED_INT_1010102_REV))
        return UVec4(1023u, 1023u, 1023u, 3u);

    switch (format.type)
    {
    case TextureFormat::UNSIGNED_INT8:
        return UVec4(std::numeric_limits<uint8_t>::max());
    case TextureFormat::UNSIGNED_INT16:
        return UVec4(std::numeric_limits<uint16_t>::max());
    case TextureFormat::UNSIGNED_INT24:
        return UVec4(0xffffffu);
    case TextureFormat::UNSIGNED_INT32:
        return UVec4(std::numeric_limits<uint32_t>::max());

    case TextureFormat::USCALED_INT8:
        return UVec4(std::numeric_limits<uint8_t>::max());
    case TextureFormat::USCALED_INT16:
        return UVec4(std::numeric_limits<uint16_t>::max());

    default:
        DE_FATAL("Invalid channel type");
        return UVec4(0);
    }
}

static IVec4 getChannelBitDepth(TextureFormat::ChannelType channelType)
{
    // make sure this table is updated if format table is updated
    DE_STATIC_ASSERT(TextureFormat::CHANNELTYPE_LAST == 48);

    switch (channelType)
    {
    case TextureFormat::SNORM_INT8:
        return IVec4(8);
    case TextureFormat::SNORM_INT16:
        return IVec4(16);
    case TextureFormat::SNORM_INT32:
        return IVec4(32);
    case TextureFormat::UNORM_INT8:
        return IVec4(8);
    case TextureFormat::UNORM_INT16:
        return IVec4(16);
    case TextureFormat::UNORM_INT24:
        return IVec4(24);
    case TextureFormat::UNORM_INT32:
        return IVec4(32);
    case TextureFormat::UNORM_BYTE_44:
        return IVec4(4, 4, 0, 0);
    case TextureFormat::UNORM_SHORT_565:
        return IVec4(5, 6, 5, 0);
    case TextureFormat::UNORM_SHORT_4444:
        return IVec4(4);
    case TextureFormat::UNORM_SHORT_555:
        return IVec4(5, 5, 5, 0);
    case TextureFormat::UNORM_SHORT_5551:
        return IVec4(5, 5, 5, 1);
    case TextureFormat::UNORM_SHORT_1555:
        return IVec4(1, 5, 5, 5);
    case TextureFormat::UNSIGNED_BYTE_44:
        return IVec4(4, 4, 0, 0);
    case TextureFormat::UNSIGNED_SHORT_565:
        return IVec4(5, 6, 5, 0);
    case TextureFormat::UNSIGNED_SHORT_4444:
        return IVec4(4);
    case TextureFormat::UNSIGNED_SHORT_5551:
        return IVec4(5, 5, 5, 1);
    case TextureFormat::UNORM_INT_101010:
        return IVec4(10, 10, 10, 0);
    case TextureFormat::SNORM_INT_1010102_REV:
        return IVec4(10, 10, 10, 2);
    case TextureFormat::UNORM_INT_1010102_REV:
        return IVec4(10, 10, 10, 2);
    case TextureFormat::SIGNED_INT8:
        return IVec4(8);
    case TextureFormat::SIGNED_INT16:
        return IVec4(16);
    case TextureFormat::SIGNED_INT32:
        return IVec4(32);
    case TextureFormat::SIGNED_INT64:
        return IVec4(64);
    case TextureFormat::UNSIGNED_INT8:
        return IVec4(8);
    case TextureFormat::UNSIGNED_INT16:
        return IVec4(16);
    case TextureFormat::UNSIGNED_INT24:
        return IVec4(24);
    case TextureFormat::UNSIGNED_INT32:
        return IVec4(32);
    case TextureFormat::UNSIGNED_INT64:
        return IVec4(64);
    case TextureFormat::SIGNED_INT_1010102_REV:
        return IVec4(10, 10, 10, 2);
    case TextureFormat::UNSIGNED_INT_1010102_REV:
        return IVec4(10, 10, 10, 2);
    case TextureFormat::UNSIGNED_INT_16_8_8:
        return IVec4(16, 8, 0, 0);
    case TextureFormat::UNSIGNED_INT_24_8:
        return IVec4(24, 8, 0, 0);
    case TextureFormat::UNSIGNED_INT_24_8_REV:
        return IVec4(24, 8, 0, 0);
    case TextureFormat::HALF_FLOAT:
        return IVec4(16);
    case TextureFormat::FLOAT:
        return IVec4(32);
    case TextureFormat::FLOAT64:
        return IVec4(64);
    case TextureFormat::UNSIGNED_INT_11F_11F_10F_REV:
        return IVec4(11, 11, 10, 0);
    case TextureFormat::UNSIGNED_INT_999_E5_REV:
        return IVec4(9, 9, 9, 0);
    case TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV:
        return IVec4(32, 8, 0, 0);
    case TextureFormat::UNORM_SHORT_10:
        return IVec4(10);
    case TextureFormat::UNORM_SHORT_12:
        return IVec4(12);
    case TextureFormat::USCALED_INT8:
        return IVec4(8);
    case TextureFormat::USCALED_INT16:
        return IVec4(16);
    case TextureFormat::SSCALED_INT8:
        return IVec4(8);
    case TextureFormat::SSCALED_INT16:
        return IVec4(16);
    case TextureFormat::USCALED_INT_1010102_REV:
        return IVec4(10, 10, 10, 2);
    case TextureFormat::SSCALED_INT_1010102_REV:
        return IVec4(10, 10, 10, 2);
    default:
        DE_ASSERT(false);
        return IVec4(0);
    }
}

IVec4 getTextureFormatBitDepth(const TextureFormat &format)
{
    const IVec4 chnBits                = getChannelBitDepth(format.type);
    const TextureSwizzle::Channel *map = getChannelReadSwizzle(format.order).components;
    const BVec4 chnMask = BVec4(deInRange32(map[0], TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_3) == true,
                                deInRange32(map[1], TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_3) == true,
                                deInRange32(map[2], TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_3) == true,
                                deInRange32(map[3], TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_3) == true);
    const IVec4 chnSwz  = IVec4((chnMask[0]) ? ((int)map[0]) : (0), (chnMask[1]) ? ((int)map[1]) : (0),
                               (chnMask[2]) ? ((int)map[2]) : (0), (chnMask[3]) ? ((int)map[3]) : (0));

    return select(chnBits.swizzle(chnSwz.x(), chnSwz.y(), chnSwz.z(), chnSwz.w()), IVec4(0), chnMask);
}

static IVec4 getChannelMantissaBitDepth(TextureFormat::ChannelType channelType)
{
    // make sure this table is updated if format table is updated
    DE_STATIC_ASSERT(TextureFormat::CHANNELTYPE_LAST == 48);

    switch (channelType)
    {
    case TextureFormat::SNORM_INT8:
    case TextureFormat::SNORM_INT16:
    case TextureFormat::SNORM_INT32:
    case TextureFormat::UNORM_INT8:
    case TextureFormat::UNORM_INT16:
    case TextureFormat::UNORM_INT24:
    case TextureFormat::UNORM_INT32:
    case TextureFormat::UNORM_BYTE_44:
    case TextureFormat::UNORM_SHORT_565:
    case TextureFormat::UNORM_SHORT_4444:
    case TextureFormat::UNORM_SHORT_555:
    case TextureFormat::UNORM_SHORT_5551:
    case TextureFormat::UNORM_SHORT_1555:
    case TextureFormat::UNSIGNED_BYTE_44:
    case TextureFormat::UNSIGNED_SHORT_565:
    case TextureFormat::UNSIGNED_SHORT_4444:
    case TextureFormat::UNSIGNED_SHORT_5551:
    case TextureFormat::UNORM_INT_101010:
    case TextureFormat::SNORM_INT_1010102_REV:
    case TextureFormat::UNORM_INT_1010102_REV:
    case TextureFormat::SIGNED_INT8:
    case TextureFormat::SIGNED_INT16:
    case TextureFormat::SIGNED_INT32:
    case TextureFormat::UNSIGNED_INT8:
    case TextureFormat::UNSIGNED_INT16:
    case TextureFormat::UNSIGNED_INT24:
    case TextureFormat::UNSIGNED_INT32:
    case TextureFormat::SIGNED_INT_1010102_REV:
    case TextureFormat::UNSIGNED_INT_1010102_REV:
    case TextureFormat::UNSIGNED_INT_16_8_8:
    case TextureFormat::UNSIGNED_INT_24_8:
    case TextureFormat::UNSIGNED_INT_24_8_REV:
    case TextureFormat::UNSIGNED_INT_999_E5_REV:
    case TextureFormat::UNORM_SHORT_10:
    case TextureFormat::UNORM_SHORT_12:
    case TextureFormat::USCALED_INT8:
    case TextureFormat::USCALED_INT16:
    case TextureFormat::SSCALED_INT8:
    case TextureFormat::SSCALED_INT16:
    case TextureFormat::USCALED_INT_1010102_REV:
    case TextureFormat::SSCALED_INT_1010102_REV:
        return getChannelBitDepth(channelType);

    case TextureFormat::HALF_FLOAT:
        return IVec4(10);
    case TextureFormat::FLOAT:
        return IVec4(23);
    case TextureFormat::FLOAT64:
        return IVec4(52);
    case TextureFormat::UNSIGNED_INT_11F_11F_10F_REV:
        return IVec4(6, 6, 5, 0);
    case TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV:
        return IVec4(23, 8, 0, 0);
    default:
        DE_ASSERT(false);
        return IVec4(0);
    }
}

IVec4 getTextureFormatMantissaBitDepth(const TextureFormat &format)
{
    const IVec4 chnBits                = getChannelMantissaBitDepth(format.type);
    const TextureSwizzle::Channel *map = getChannelReadSwizzle(format.order).components;
    const BVec4 chnMask = BVec4(deInRange32(map[0], TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_3) == true,
                                deInRange32(map[1], TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_3) == true,
                                deInRange32(map[2], TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_3) == true,
                                deInRange32(map[3], TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_3) == true);
    const IVec4 chnSwz  = IVec4((chnMask[0]) ? ((int)map[0]) : (0), (chnMask[1]) ? ((int)map[1]) : (0),
                               (chnMask[2]) ? ((int)map[2]) : (0), (chnMask[3]) ? ((int)map[3]) : (0));

    return select(chnBits.swizzle(chnSwz.x(), chnSwz.y(), chnSwz.z(), chnSwz.w()), IVec4(0), chnMask);
}

BVec4 getTextureFormatChannelMask(const TextureFormat &format)
{
    const TextureSwizzle::Channel *const map = getChannelReadSwizzle(format.order).components;
    return BVec4(deInRange32(map[0], TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_3) == true,
                 deInRange32(map[1], TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_3) == true,
                 deInRange32(map[2], TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_3) == true,
                 deInRange32(map[3], TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_3) == true);
}

static inline float linearInterpolate(float t, float minVal, float maxVal)
{
    return minVal + (maxVal - minVal) * t;
}

static inline Vec4 linearInterpolate(float t, const Vec4 &a, const Vec4 &b)
{
    return a + (b - a) * t;
}

enum
{
    CLEAR_OPTIMIZE_THRESHOLD      = 128,
    CLEAR_OPTIMIZE_MAX_PIXEL_SIZE = 8
};

inline void fillRow(const PixelBufferAccess &dst, int y, int z, int pixelSize, const uint8_t *pixel)
{
    DE_ASSERT(dst.getPixelPitch() == pixelSize); // only tightly packed

    uint8_t *dstPtr = (uint8_t *)dst.getPixelPtr(0, y, z);
    int width       = dst.getWidth();

    if (pixelSize == 8 && deIsAlignedPtr(dstPtr, pixelSize))
    {
        uint64_t val;
        memcpy(&val, pixel, sizeof(val));

        for (int i = 0; i < width; i++)
            ((uint64_t *)dstPtr)[i] = val;
    }
    else if (pixelSize == 4 && deIsAlignedPtr(dstPtr, pixelSize))
    {
        uint32_t val;
        memcpy(&val, pixel, sizeof(val));

        for (int i = 0; i < width; i++)
            ((uint32_t *)dstPtr)[i] = val;
    }
    else
    {
        for (int i = 0; i < width; i++)
            for (int j = 0; j < pixelSize; j++)
                dstPtr[i * pixelSize + j] = pixel[j];
    }
}

void clear(const PixelBufferAccess &access, const Vec4 &color)
{
    const int pixelSize               = access.getFormat().getPixelSize();
    const int pixelPitch              = access.getPixelPitch();
    const bool rowPixelsTightlyPacked = (pixelSize == pixelPitch);

    if (access.getWidth() * access.getHeight() * access.getDepth() >= CLEAR_OPTIMIZE_THRESHOLD &&
        pixelSize < CLEAR_OPTIMIZE_MAX_PIXEL_SIZE && rowPixelsTightlyPacked)
    {
        // Convert to destination format.
        union
        {
            uint8_t u8[CLEAR_OPTIMIZE_MAX_PIXEL_SIZE];
            uint64_t u64; // Forces 64-bit alignment.
        } pixel;
        DE_STATIC_ASSERT(sizeof(pixel) == CLEAR_OPTIMIZE_MAX_PIXEL_SIZE);
        PixelBufferAccess(access.getFormat(), 1, 1, 1, 0, 0, &pixel.u8[0]).setPixel(color, 0, 0);

        for (int z = 0; z < access.getDepth(); z++)
            for (int y = 0; y < access.getHeight(); y++)
                fillRow(access, y, z, pixelSize, &pixel.u8[0]);
    }
    else
    {
        for (int z = 0; z < access.getDepth(); z++)
            for (int y = 0; y < access.getHeight(); y++)
                for (int x = 0; x < access.getWidth(); x++)
                    access.setPixel(color, x, y, z);
    }
}

void clear(const PixelBufferAccess &access, const IVec4 &color)
{
    const int pixelSize               = access.getFormat().getPixelSize();
    const int pixelPitch              = access.getPixelPitch();
    const bool rowPixelsTightlyPacked = (pixelSize == pixelPitch);

    if (access.getWidth() * access.getHeight() * access.getDepth() >= CLEAR_OPTIMIZE_THRESHOLD &&
        pixelSize < CLEAR_OPTIMIZE_MAX_PIXEL_SIZE && rowPixelsTightlyPacked)
    {
        // Convert to destination format.
        union
        {
            uint8_t u8[CLEAR_OPTIMIZE_MAX_PIXEL_SIZE];
            uint64_t u64; // Forces 64-bit alignment.
        } pixel;
        DE_STATIC_ASSERT(sizeof(pixel) == CLEAR_OPTIMIZE_MAX_PIXEL_SIZE);
        PixelBufferAccess(access.getFormat(), 1, 1, 1, 0, 0, &pixel.u8[0]).setPixel(color, 0, 0);

        for (int z = 0; z < access.getDepth(); z++)
            for (int y = 0; y < access.getHeight(); y++)
                fillRow(access, y, z, pixelSize, &pixel.u8[0]);
    }
    else
    {
        for (int z = 0; z < access.getDepth(); z++)
            for (int y = 0; y < access.getHeight(); y++)
                for (int x = 0; x < access.getWidth(); x++)
                    access.setPixel(color, x, y, z);
    }
}

void clear(const PixelBufferAccess &access, const UVec4 &color)
{
    clear(access, color.cast<int32_t>());
}

void clearDepth(const PixelBufferAccess &access, float depth)
{
    DE_ASSERT(access.getFormat().order == TextureFormat::DS || access.getFormat().order == TextureFormat::D);

    clear(getEffectiveDepthStencilAccess(access, Sampler::MODE_DEPTH), tcu::Vec4(depth, 0.0f, 0.0f, 0.0f));
}

void clearStencil(const PixelBufferAccess &access, int stencil)
{
    DE_ASSERT(access.getFormat().order == TextureFormat::DS || access.getFormat().order == TextureFormat::S);

    clear(getEffectiveDepthStencilAccess(access, Sampler::MODE_STENCIL), tcu::UVec4(stencil, 0u, 0u, 0u));
}

enum GradientStyle
{
    GRADIENT_STYLE_OLD     = 0,
    GRADIENT_STYLE_NEW     = 1,
    GRADIENT_STYLE_PYRAMID = 2
};

static void fillWithComponentGradients1D(const PixelBufferAccess &access, const Vec4 &minVal, const Vec4 &maxVal,
                                         GradientStyle)
{
    DE_ASSERT(access.getHeight() == 1);
    for (int x = 0; x < access.getWidth(); x++)
    {
        float s = ((float)x + 0.5f) / (float)access.getWidth();

        float r = linearInterpolate(s, minVal.x(), maxVal.x());
        float g = linearInterpolate(s, minVal.y(), maxVal.y());
        float b = linearInterpolate(s, minVal.z(), maxVal.z());
        float a = linearInterpolate(s, minVal.w(), maxVal.w());

        access.setPixel(tcu::Vec4(r, g, b, a), x, 0);
    }
}

static void fillWithComponentGradients2D(const PixelBufferAccess &access, const Vec4 &minVal, const Vec4 &maxVal,
                                         GradientStyle style)
{
    if (style == GRADIENT_STYLE_PYRAMID)
    {
        int xedge = deFloorFloatToInt32(float(access.getWidth()) * 0.6f);
        int yedge = deFloorFloatToInt32(float(access.getHeight()) * 0.6f);

        for (int y = 0; y < access.getHeight(); y++)
        {
            for (int x = 0; x < access.getWidth(); x++)
            {
                float s     = ((float)x + 0.5f) / (float)access.getWidth();
                float t     = ((float)y + 0.5f) / (float)access.getHeight();
                float coefR = 0.0f;
                float coefG = 0.0f;
                float coefB = 0.0f;
                float coefA = 0.0f;

                coefR = (x < xedge) ? s * 0.4f : (1 - s) * 0.6f;
                coefG = (x < xedge) ? s * 0.4f : (1 - s) * 0.6f;
                coefB = (x < xedge) ? (1.0f - s) * 0.4f : s * 0.6f - 0.2f;
                coefA = (x < xedge) ? (1.0f - s) * 0.4f : s * 0.6f - 0.2f;

                coefR += (y < yedge) ? t * 0.4f : (1 - t) * 0.6f;
                coefG += (y < yedge) ? (1.0f - t) * 0.4f : t * 0.6f - 0.2f;
                coefB += (y < yedge) ? t * 0.4f : (1 - t) * 0.6f;
                coefA += (y < yedge) ? (1.0f - t) * 0.4f : t * 0.6f - 0.2f;

                float r = linearInterpolate(coefR, minVal.x(), maxVal.x());
                float g = linearInterpolate(coefG, minVal.y(), maxVal.y());
                float b = linearInterpolate(coefB, minVal.z(), maxVal.z());
                float a = linearInterpolate(coefA, minVal.w(), maxVal.w());

                access.setPixel(tcu::Vec4(r, g, b, a), x, y);
            }
        }
    }
    else
    {
        for (int y = 0; y < access.getHeight(); y++)
        {
            for (int x = 0; x < access.getWidth(); x++)
            {
                float s = ((float)x + 0.5f) / (float)access.getWidth();
                float t = ((float)y + 0.5f) / (float)access.getHeight();

                float r = linearInterpolate((s + t) * 0.5f, minVal.x(), maxVal.x());
                float g = linearInterpolate((s + (1.0f - t)) * 0.5f, minVal.y(), maxVal.y());
                float b = linearInterpolate(((1.0f - s) + t) * 0.5f, minVal.z(), maxVal.z());
                float a = linearInterpolate(((1.0f - s) + (1.0f - t)) * 0.5f, minVal.w(), maxVal.w());

                access.setPixel(tcu::Vec4(r, g, b, a), x, y);
            }
        }
    }
}

static void fillWithComponentGradients3D(const PixelBufferAccess &dst, const Vec4 &minVal, const Vec4 &maxVal,
                                         GradientStyle style)
{
    for (int z = 0; z < dst.getDepth(); z++)
    {
        for (int y = 0; y < dst.getHeight(); y++)
        {
            for (int x = 0; x < dst.getWidth(); x++)
            {
                float s = ((float)x + 0.5f) / (float)dst.getWidth();
                float t = ((float)y + 0.5f) / (float)dst.getHeight();
                float p = ((float)z + 0.5f) / (float)dst.getDepth();

                float r, g, b, a;

                if (style == GRADIENT_STYLE_NEW)
                {
                    // R, G, B and A all depend on every coordinate.
                    r = linearInterpolate((s + t + p) / 3.0f, minVal.x(), maxVal.x());
                    g = linearInterpolate((s + (1.0f - (t + p) * 0.5f) * 2.0f) / 3.0f, minVal.y(), maxVal.y());
                    b = linearInterpolate(((1.0f - (s + t) * 0.5f) * 2.0f + p) / 3.0f, minVal.z(), maxVal.z());
                    a = linearInterpolate(1.0f - (s + t + p) / 3.0f, minVal.w(), maxVal.w());
                }
                else // GRADIENT_STYLE_OLD
                {
                    // Each of R, G and B only depend on X, Y and Z, respectively.
                    r = linearInterpolate(s, minVal.x(), maxVal.x());
                    g = linearInterpolate(t, minVal.y(), maxVal.y());
                    b = linearInterpolate(p, minVal.z(), maxVal.z());
                    a = linearInterpolate(1.0f - (s + t + p) / 3.0f, minVal.w(), maxVal.w());
                }

                dst.setPixel(tcu::Vec4(r, g, b, a), x, y, z);
            }
        }
    }
}

void fillWithComponentGradientsStyled(const PixelBufferAccess &access, const Vec4 &minVal, const Vec4 &maxVal,
                                      GradientStyle style)
{
    if (isCombinedDepthStencilType(access.getFormat().type))
    {
        const bool hasDepth =
            access.getFormat().order == tcu::TextureFormat::DS || access.getFormat().order == tcu::TextureFormat::D;
        const bool hasStencil =
            access.getFormat().order == tcu::TextureFormat::DS || access.getFormat().order == tcu::TextureFormat::S;

        DE_ASSERT(hasDepth || hasStencil);

        // For combined formats, treat D and S as separate channels
        if (hasDepth)
            fillWithComponentGradientsStyled(getEffectiveDepthStencilAccess(access, tcu::Sampler::MODE_DEPTH), minVal,
                                             maxVal, style);
        if (hasStencil)
            fillWithComponentGradientsStyled(getEffectiveDepthStencilAccess(access, tcu::Sampler::MODE_STENCIL),
                                             minVal.swizzle(3, 2, 1, 0), maxVal.swizzle(3, 2, 1, 0), style);
    }
    else
    {
        if (access.getHeight() == 1 && access.getDepth() == 1)
            fillWithComponentGradients1D(access, minVal, maxVal, style);
        else if (access.getDepth() == 1)
            fillWithComponentGradients2D(access, minVal, maxVal, style);
        else
            fillWithComponentGradients3D(access, minVal, maxVal, style);
    }
}

void fillWithComponentGradients(const PixelBufferAccess &access, const Vec4 &minVal, const Vec4 &maxVal)
{
    fillWithComponentGradientsStyled(access, minVal, maxVal, GRADIENT_STYLE_OLD);
}

void fillWithComponentGradients2(const PixelBufferAccess &access, const Vec4 &minVal, const Vec4 &maxVal)
{
    fillWithComponentGradientsStyled(access, minVal, maxVal, GRADIENT_STYLE_NEW);
}

void fillWithComponentGradients3(const PixelBufferAccess &access, const Vec4 &minVal, const Vec4 &maxVal)
{
    fillWithComponentGradientsStyled(access, minVal, maxVal, GRADIENT_STYLE_PYRAMID);
}

static void fillWithGrid1D(const PixelBufferAccess &access, int cellSize, const Vec4 &colorA, const Vec4 &colorB)
{
    for (int x = 0; x < access.getWidth(); x++)
    {
        int mx = (x / cellSize) % 2;

        if (mx)
            access.setPixel(colorB, x, 0);
        else
            access.setPixel(colorA, x, 0);
    }
}

static void fillWithGrid2D(const PixelBufferAccess &access, int cellSize, const Vec4 &colorA, const Vec4 &colorB)
{
    for (int y = 0; y < access.getHeight(); y++)
    {
        for (int x = 0; x < access.getWidth(); x++)
        {
            int mx = (x / cellSize) % 2;
            int my = (y / cellSize) % 2;

            if (mx ^ my)
                access.setPixel(colorB, x, y);
            else
                access.setPixel(colorA, x, y);
        }
    }
}

static void fillWithGrid3D(const PixelBufferAccess &access, int cellSize, const Vec4 &colorA, const Vec4 &colorB)
{
    for (int z = 0; z < access.getDepth(); z++)
    {
        for (int y = 0; y < access.getHeight(); y++)
        {
            for (int x = 0; x < access.getWidth(); x++)
            {
                int mx = (x / cellSize) % 2;
                int my = (y / cellSize) % 2;
                int mz = (z / cellSize) % 2;

                if (mx ^ my ^ mz)
                    access.setPixel(colorB, x, y, z);
                else
                    access.setPixel(colorA, x, y, z);
            }
        }
    }
}

void fillWithGrid(const PixelBufferAccess &access, int cellSize, const Vec4 &colorA, const Vec4 &colorB)
{
    if (isCombinedDepthStencilType(access.getFormat().type))
    {
        const bool hasDepth =
            access.getFormat().order == tcu::TextureFormat::DS || access.getFormat().order == tcu::TextureFormat::D;
        const bool hasStencil =
            access.getFormat().order == tcu::TextureFormat::DS || access.getFormat().order == tcu::TextureFormat::S;

        DE_ASSERT(hasDepth || hasStencil);

        // For combined formats, treat D and S as separate channels
        if (hasDepth)
            fillWithGrid(getEffectiveDepthStencilAccess(access, tcu::Sampler::MODE_DEPTH), cellSize, colorA, colorB);
        if (hasStencil)
            fillWithGrid(getEffectiveDepthStencilAccess(access, tcu::Sampler::MODE_STENCIL), cellSize,
                         colorA.swizzle(3, 2, 1, 0), colorB.swizzle(3, 2, 1, 0));
    }
    else
    {
        if (access.getHeight() == 1 && access.getDepth() == 1)
            fillWithGrid1D(access, cellSize, colorA, colorB);
        else if (access.getDepth() == 1)
            fillWithGrid2D(access, cellSize, colorA, colorB);
        else
            fillWithGrid3D(access, cellSize, colorA, colorB);
    }
}

void fillWithRepeatableGradient(const PixelBufferAccess &access, const Vec4 &colorA, const Vec4 &colorB)
{
    for (int y = 0; y < access.getHeight(); y++)
    {
        for (int x = 0; x < access.getWidth(); x++)
        {
            float s = ((float)x + 0.5f) / (float)access.getWidth();
            float t = ((float)y + 0.5f) / (float)access.getHeight();

            float a = s > 0.5f ? (2.0f - 2.0f * s) : 2.0f * s;
            float b = t > 0.5f ? (2.0f - 2.0f * t) : 2.0f * t;

            float p = deFloatClamp(deFloatSqrt(a * a + b * b), 0.0f, 1.0f);
            access.setPixel(linearInterpolate(p, colorA, colorB), x, y);
        }
    }
}

void fillWithRGBAQuads(const PixelBufferAccess &dst)
{
    TCU_CHECK_INTERNAL(dst.getDepth() == 1);
    int width  = dst.getWidth();
    int height = dst.getHeight();
    int left   = width / 2;
    int top    = height / 2;

    clear(getSubregion(dst, 0, 0, 0, left, top, 1), Vec4(1.0f, 0.0f, 0.0f, 1.0f));
    clear(getSubregion(dst, left, 0, 0, width - left, top, 1), Vec4(0.0f, 1.0f, 0.0f, 1.0f));
    clear(getSubregion(dst, 0, top, 0, left, height - top, 1), Vec4(0.0f, 0.0f, 1.0f, 0.0f));
    clear(getSubregion(dst, left, top, 0, width - left, height - top, 1), Vec4(0.5f, 0.5f, 0.5f, 1.0f));
}

// \todo [2012-11-13 pyry] There is much better metaballs code in CL SIR value generators.
void fillWithMetaballs(const PixelBufferAccess &dst, int numBalls, uint32_t seed)
{
    TCU_CHECK_INTERNAL(dst.getDepth() == 1);
    std::vector<Vec2> points(numBalls);
    de::Random rnd(seed);

    for (int i = 0; i < numBalls; i++)
    {
        float x   = rnd.getFloat();
        float y   = rnd.getFloat();
        points[i] = (Vec2(x, y));
    }

    for (int y = 0; y < dst.getHeight(); y++)
        for (int x = 0; x < dst.getWidth(); x++)
        {
            Vec2 p((float)x / (float)dst.getWidth(), (float)y / (float)dst.getHeight());

            float sum = 0.0f;
            for (std::vector<Vec2>::const_iterator i = points.begin(); i != points.end(); i++)
            {
                Vec2 d  = p - *i;
                float f = 0.01f / (d.x() * d.x() + d.y() * d.y());

                sum += f;
            }

            dst.setPixel(Vec4(sum), x, y);
        }
}

void copy(const PixelBufferAccess &dst, const ConstPixelBufferAccess &src, const bool clearUnused)
{
    DE_ASSERT(src.getSize() == dst.getSize());

    const int width  = dst.getWidth();
    const int height = dst.getHeight();
    const int depth  = dst.getDepth();

    const int srcPixelSize      = src.getFormat().getPixelSize();
    const int dstPixelSize      = dst.getFormat().getPixelSize();
    const int srcPixelPitch     = src.getPixelPitch();
    const int dstPixelPitch     = dst.getPixelPitch();
    const bool srcTightlyPacked = (srcPixelSize == srcPixelPitch);
    const bool dstTightlyPacked = (dstPixelSize == dstPixelPitch);

    const bool srcHasDepth =
        (src.getFormat().order == tcu::TextureFormat::DS || src.getFormat().order == tcu::TextureFormat::D);
    const bool srcHasStencil =
        (src.getFormat().order == tcu::TextureFormat::DS || src.getFormat().order == tcu::TextureFormat::S);
    const bool dstHasDepth =
        (dst.getFormat().order == tcu::TextureFormat::DS || dst.getFormat().order == tcu::TextureFormat::D);
    const bool dstHasStencil =
        (dst.getFormat().order == tcu::TextureFormat::DS || dst.getFormat().order == tcu::TextureFormat::S);

    if (src.getFormat() == dst.getFormat() && srcTightlyPacked && dstTightlyPacked)
    {
        // Fast-path for matching formats.
        for (int z = 0; z < depth; z++)
            for (int y = 0; y < height; y++)
                deMemcpy(dst.getPixelPtr(0, y, z), src.getPixelPtr(0, y, z), srcPixelSize * width);
    }
    else if (src.getFormat() == dst.getFormat())
    {
        // Bit-exact copy for matching formats.
        for (int z = 0; z < depth; z++)
            for (int y = 0; y < height; y++)
                for (int x = 0; x < width; x++)
                    deMemcpy(dst.getPixelPtr(x, y, z), src.getPixelPtr(x, y, z), srcPixelSize);
    }
    else if (srcHasDepth || srcHasStencil || dstHasDepth || dstHasStencil)
    {
        DE_ASSERT((srcHasDepth && dstHasDepth) ||
                  (srcHasStencil && dstHasStencil)); // must have at least one common channel

        if (dstHasDepth && srcHasDepth)
        {
            for (int z = 0; z < depth; z++)
                for (int y = 0; y < height; y++)
                    for (int x = 0; x < width; x++)
                        dst.setPixDepth(src.getPixDepth(x, y, z), x, y, z);
        }
        else if (dstHasDepth && !srcHasDepth && clearUnused)
        {
            // consistency with color copies
            tcu::clearDepth(dst, 0.0f);
        }

        if (dstHasStencil && srcHasStencil)
        {
            for (int z = 0; z < depth; z++)
                for (int y = 0; y < height; y++)
                    for (int x = 0; x < width; x++)
                        dst.setPixStencil(src.getPixStencil(x, y, z), x, y, z);
        }
        else if (dstHasStencil && !srcHasStencil && clearUnused)
        {
            // consistency with color copies
            tcu::clearStencil(dst, 0u);
        }
    }
    else
    {
        TextureChannelClass srcClass = getTextureChannelClass(src.getFormat().type);
        TextureChannelClass dstClass = getTextureChannelClass(dst.getFormat().type);
        bool srcIsInt =
            srcClass == TEXTURECHANNELCLASS_SIGNED_INTEGER || srcClass == TEXTURECHANNELCLASS_UNSIGNED_INTEGER;
        bool dstIsInt =
            dstClass == TEXTURECHANNELCLASS_SIGNED_INTEGER || dstClass == TEXTURECHANNELCLASS_UNSIGNED_INTEGER;

        if (srcIsInt && dstIsInt)
        {
            for (int z = 0; z < depth; z++)
                for (int y = 0; y < height; y++)
                    for (int x = 0; x < width; x++)
                        dst.setPixel(src.getPixelInt(x, y, z), x, y, z);
        }
        else
        {
            for (int z = 0; z < depth; z++)
                for (int y = 0; y < height; y++)
                    for (int x = 0; x < width; x++)
                        dst.setPixel(src.getPixel(x, y, z), x, y, z);
        }
    }
}

void scale(const PixelBufferAccess &dst, const ConstPixelBufferAccess &src, Sampler::FilterMode filter)
{
    DE_ASSERT(filter == Sampler::NEAREST || filter == Sampler::LINEAR);

    Sampler sampler(Sampler::CLAMP_TO_EDGE, Sampler::CLAMP_TO_EDGE, Sampler::CLAMP_TO_EDGE, filter, filter, 0.0f,
                    false);

    float sX = (float)src.getWidth() / (float)dst.getWidth();
    float sY = (float)src.getHeight() / (float)dst.getHeight();
    float sZ = (float)src.getDepth() / (float)dst.getDepth();

    if (dst.getDepth() == 1 && src.getDepth() == 1)
    {
        for (int y = 0; y < dst.getHeight(); y++)
            for (int x = 0; x < dst.getWidth(); x++)
                dst.setPixel(linearToSRGBIfNeeded(dst.getFormat(), src.sample2D(sampler, filter, ((float)x + 0.5f) * sX,
                                                                                ((float)y + 0.5f) * sY, 0)),
                             x, y);
    }
    else
    {
        for (int z = 0; z < dst.getDepth(); z++)
            for (int y = 0; y < dst.getHeight(); y++)
                for (int x = 0; x < dst.getWidth(); x++)
                    dst.setPixel(linearToSRGBIfNeeded(dst.getFormat(),
                                                      src.sample3D(sampler, filter, ((float)x + 0.5f) * sX,
                                                                   ((float)y + 0.5f) * sY, ((float)z + 0.5f) * sZ)),
                                 x, y, z);
    }
}

void estimatePixelValueRange(const ConstPixelBufferAccess &access, Vec4 &minVal, Vec4 &maxVal)
{
    const TextureFormat &format = access.getFormat();

    switch (getTextureChannelClass(format.type))
    {
    case TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
        // Normalized unsigned formats.
        minVal = Vec4(0.0f);
        maxVal = Vec4(1.0f);
        break;

    case TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
        // Normalized signed formats.
        minVal = Vec4(-1.0f);
        maxVal = Vec4(+1.0f);
        break;

    default:
        // \note Samples every 4/8th pixel.
        minVal = Vec4(std::numeric_limits<float>::max());
        maxVal = Vec4(std::numeric_limits<float>::min());

        for (int z = 0; z < access.getDepth(); z += 2)
        {
            for (int y = 0; y < access.getHeight(); y += 2)
            {
                for (int x = 0; x < access.getWidth(); x += 2)
                {
                    Vec4 p = access.getPixel(x, y, z);

                    minVal[0] = (std::isnan(p[0]) ? minVal[0] : de::min(minVal[0], p[0]));
                    minVal[1] = (std::isnan(p[1]) ? minVal[1] : de::min(minVal[1], p[1]));
                    minVal[2] = (std::isnan(p[2]) ? minVal[2] : de::min(minVal[2], p[2]));
                    minVal[3] = (std::isnan(p[3]) ? minVal[3] : de::min(minVal[3], p[3]));

                    maxVal[0] = (std::isnan(p[0]) ? maxVal[0] : de::max(maxVal[0], p[0]));
                    maxVal[1] = (std::isnan(p[1]) ? maxVal[1] : de::max(maxVal[1], p[1]));
                    maxVal[2] = (std::isnan(p[2]) ? maxVal[2] : de::max(maxVal[2], p[2]));
                    maxVal[3] = (std::isnan(p[3]) ? maxVal[3] : de::max(maxVal[3], p[3]));
                }
            }
        }
        break;
    }
}

void computePixelScaleBias(const ConstPixelBufferAccess &access, Vec4 &scale, Vec4 &bias)
{
    Vec4 minVal, maxVal;
    estimatePixelValueRange(access, minVal, maxVal);

    const float eps = 0.0001f;

    for (int c = 0; c < 4; c++)
    {
        if (maxVal[c] - minVal[c] < eps)
        {
            scale[c] = (maxVal[c] < eps) ? 1.0f : (1.0f / maxVal[c]);
            bias[c]  = (c == 3) ? (1.0f - maxVal[c] * scale[c]) : (0.0f - minVal[c] * scale[c]);
        }
        else
        {
            scale[c] = 1.0f / (maxVal[c] - minVal[c]);
            bias[c]  = 0.0f - minVal[c] * scale[c];
        }
    }
}

int getCubeArrayFaceIndex(CubeFace face)
{
    DE_ASSERT((int)face >= 0 && face < CUBEFACE_LAST);

    switch (face)
    {
    case CUBEFACE_POSITIVE_X:
        return 0;
    case CUBEFACE_NEGATIVE_X:
        return 1;
    case CUBEFACE_POSITIVE_Y:
        return 2;
    case CUBEFACE_NEGATIVE_Y:
        return 3;
    case CUBEFACE_POSITIVE_Z:
        return 4;
    case CUBEFACE_NEGATIVE_Z:
        return 5;

    default:
        return -1;
    }
}

uint32_t packRGB999E5(const tcu::Vec4 &color)
{
    const int mBits    = 9;
    const int eBits    = 5;
    const int eBias    = 15;
    const int eMax     = (1 << eBits) - 1;
    const float maxVal = (float)(((1 << mBits) - 1) * (1 << (eMax - eBias))) / (float)(1 << mBits);

    float rc       = deFloatClamp(color[0], 0.0f, maxVal);
    float gc       = deFloatClamp(color[1], 0.0f, maxVal);
    float bc       = deFloatClamp(color[2], 0.0f, maxVal);
    float maxc     = de::max(rc, de::max(gc, bc));
    float log2c    = deFloatLog2(maxc);
    int32_t floorc = std::isinf(log2c) ? std::numeric_limits<int32_t>::min() : deFloorFloatToInt32(log2c);
    int exps       = de::max(-eBias - 1, floorc) + 1 + eBias;
    float e        = deFloatPow(2.0f, (float)(exps - eBias - mBits));
    int maxs       = deFloorFloatToInt32(maxc / e + 0.5f);

    if (maxs == (1 << mBits))
    {
        exps++;
        e *= 2.0f;
    }

    uint32_t rs = (uint32_t)deFloorFloatToInt32(rc / e + 0.5f);
    uint32_t gs = (uint32_t)deFloorFloatToInt32(gc / e + 0.5f);
    uint32_t bs = (uint32_t)deFloorFloatToInt32(bc / e + 0.5f);

    DE_ASSERT((exps & ~((1 << 5) - 1)) == 0);
    DE_ASSERT((rs & ~((1 << 9) - 1)) == 0);
    DE_ASSERT((gs & ~((1 << 9) - 1)) == 0);
    DE_ASSERT((bs & ~((1 << 9) - 1)) == 0);

    return rs | (gs << 9) | (bs << 18) | (exps << 27);
}

// Sampler utils

static const void *addOffset(const void *ptr, int numBytes)
{
    return ptr ? (const uint8_t *)ptr + numBytes : reinterpret_cast<const uint8_t *>(numBytes);
}

static void *addOffset(void *ptr, int numBytes)
{
    return ptr ? (uint8_t *)ptr + numBytes : reinterpret_cast<uint8_t *>(numBytes);
}

template <typename AccessType>
static AccessType toSamplerAccess(const AccessType &baseAccess, Sampler::DepthStencilMode mode)
{
    // make sure to update this if type table is updated
    DE_STATIC_ASSERT(TextureFormat::CHANNELTYPE_LAST == 48);

    if (!isCombinedDepthStencilType(baseAccess.getFormat().type))
        return baseAccess;
    else
    {
#if (DE_ENDIANNESS == DE_LITTLE_ENDIAN)
        const uint32_t uint32ByteOffsetBits0To8   = 0; //!< least significant byte in the lowest address
        const uint32_t uint32ByteOffsetBits0To24  = 0;
        const uint32_t uint32ByteOffsetBits8To32  = 1;
        const uint32_t uint32ByteOffsetBits16To32 = 2;
        const uint32_t uint32ByteOffsetBits24To32 = 3;
#else
        const uint32_t uint32ByteOffsetBits0To8   = 3; //!< least significant byte in the highest address
        const uint32_t uint32ByteOffsetBits0To24  = 1;
        const uint32_t uint32ByteOffsetBits8To32  = 0;
        const uint32_t uint32ByteOffsetBits16To32 = 0;
        const uint32_t uint32ByteOffsetBits24To32 = 0;
#endif

        // Sampled channel must exist
        DE_ASSERT(baseAccess.getFormat().order == TextureFormat::DS ||
                  (mode == Sampler::MODE_DEPTH && baseAccess.getFormat().order == TextureFormat::D) ||
                  (mode == Sampler::MODE_STENCIL && baseAccess.getFormat().order == TextureFormat::S));

        // combined formats have multiple channel classes, detect on sampler settings
        switch (baseAccess.getFormat().type)
        {
        case TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV:
        {
            if (mode == Sampler::MODE_DEPTH)
            {
                // select the float component
                return AccessType(TextureFormat(TextureFormat::D, TextureFormat::FLOAT), baseAccess.getSize(),
                                  baseAccess.getPitch(), baseAccess.getDataPtr());
            }
            else if (mode == Sampler::MODE_STENCIL)
            {
                // select the uint 8 component
                return AccessType(TextureFormat(TextureFormat::S, TextureFormat::UNSIGNED_INT8), baseAccess.getSize(),
                                  baseAccess.getPitch(),
                                  addOffset(baseAccess.getDataPtr(), 4 + uint32ByteOffsetBits0To8));
            }
            else
            {
                // unknown sampler mode
                DE_ASSERT(false);
                return AccessType();
            }
        }

        case TextureFormat::UNSIGNED_INT_16_8_8:
        {
            if (mode == Sampler::MODE_DEPTH)
            {
                // select the unorm16 component
                return AccessType(TextureFormat(TextureFormat::D, TextureFormat::UNORM_INT16), baseAccess.getSize(),
                                  baseAccess.getPitch(),
                                  addOffset(baseAccess.getDataPtr(), uint32ByteOffsetBits16To32));
            }
            else if (mode == Sampler::MODE_STENCIL)
            {
                // select the uint 8 component
                return AccessType(TextureFormat(TextureFormat::S, TextureFormat::UNSIGNED_INT8), baseAccess.getSize(),
                                  baseAccess.getPitch(), addOffset(baseAccess.getDataPtr(), uint32ByteOffsetBits0To8));
            }
            else
            {
                // unknown sampler mode
                DE_ASSERT(false);
                return AccessType();
            }
        }

        case TextureFormat::UNSIGNED_INT_24_8:
        {
            if (mode == Sampler::MODE_DEPTH)
            {
                // select the unorm24 component
                return AccessType(TextureFormat(TextureFormat::D, TextureFormat::UNORM_INT24), baseAccess.getSize(),
                                  baseAccess.getPitch(), addOffset(baseAccess.getDataPtr(), uint32ByteOffsetBits8To32));
            }
            else if (mode == Sampler::MODE_STENCIL)
            {
                // select the uint 8 component
                return AccessType(TextureFormat(TextureFormat::S, TextureFormat::UNSIGNED_INT8), baseAccess.getSize(),
                                  baseAccess.getPitch(), addOffset(baseAccess.getDataPtr(), uint32ByteOffsetBits0To8));
            }
            else
            {
                // unknown sampler mode
                DE_ASSERT(false);
                return AccessType();
            }
        }

        case TextureFormat::UNSIGNED_INT_24_8_REV:
        {
            if (mode == Sampler::MODE_DEPTH)
            {
                // select the unorm24 component
                return AccessType(TextureFormat(TextureFormat::D, TextureFormat::UNORM_INT24), baseAccess.getSize(),
                                  baseAccess.getPitch(), addOffset(baseAccess.getDataPtr(), uint32ByteOffsetBits0To24));
            }
            else if (mode == Sampler::MODE_STENCIL)
            {
                // select the uint 8 component
                return AccessType(TextureFormat(TextureFormat::S, TextureFormat::UNSIGNED_INT8), baseAccess.getSize(),
                                  baseAccess.getPitch(),
                                  addOffset(baseAccess.getDataPtr(), uint32ByteOffsetBits24To32));
            }
            else
            {
                // unknown sampler mode
                DE_ASSERT(false);
                return AccessType();
            }
        }

        default:
        {
            // unknown combined format
            DE_ASSERT(false);
            return AccessType();
        }
        }
    }
}

PixelBufferAccess getEffectiveDepthStencilAccess(const PixelBufferAccess &baseAccess, Sampler::DepthStencilMode mode)
{
    return toSamplerAccess<PixelBufferAccess>(baseAccess, mode);
}

ConstPixelBufferAccess getEffectiveDepthStencilAccess(const ConstPixelBufferAccess &baseAccess,
                                                      Sampler::DepthStencilMode mode)
{
    return toSamplerAccess<ConstPixelBufferAccess>(baseAccess, mode);
}

TextureFormat getEffectiveDepthStencilTextureFormat(const TextureFormat &baseFormat, Sampler::DepthStencilMode mode)
{
    return toSamplerAccess(ConstPixelBufferAccess(baseFormat, IVec3(0, 0, 0), nullptr), mode).getFormat();
}

template <typename ViewType>
ViewType getEffectiveTView(const ViewType &src, std::vector<tcu::ConstPixelBufferAccess> &storage,
                           const tcu::Sampler &sampler)
{
    storage.resize(src.getNumLevels());

    ViewType view = ViewType(src.getNumLevels(), &storage[0], src.isES2(), src.getImageViewMinLodParams());

    for (int levelNdx = 0; levelNdx < src.getNumLevels(); ++levelNdx)
        storage[levelNdx] = tcu::getEffectiveDepthStencilAccess(src.getLevel(levelNdx), sampler.depthStencilMode);

    return view;
}

tcu::TextureCubeView getEffectiveTView(const tcu::TextureCubeView &src,
                                       std::vector<tcu::ConstPixelBufferAccess> &storage, const tcu::Sampler &sampler)
{
    storage.resize(tcu::CUBEFACE_LAST * src.getNumLevels());

    const tcu::ConstPixelBufferAccess *storagePtrs[tcu::CUBEFACE_LAST] = {
        &storage[0 * src.getNumLevels()], &storage[1 * src.getNumLevels()], &storage[2 * src.getNumLevels()],
        &storage[3 * src.getNumLevels()], &storage[4 * src.getNumLevels()], &storage[5 * src.getNumLevels()],
    };

    tcu::TextureCubeView view =
        tcu::TextureCubeView(src.getNumLevels(), storagePtrs, false, src.getImageViewMinLodParams());

    for (int faceNdx = 0; faceNdx < tcu::CUBEFACE_LAST; ++faceNdx)
        for (int levelNdx = 0; levelNdx < src.getNumLevels(); ++levelNdx)
            storage[faceNdx * src.getNumLevels() + levelNdx] = tcu::getEffectiveDepthStencilAccess(
                src.getLevelFace(levelNdx, (tcu::CubeFace)faceNdx), sampler.depthStencilMode);

    return view;
}

tcu::Texture1DView getEffectiveTextureView(const tcu::Texture1DView &src,
                                           std::vector<tcu::ConstPixelBufferAccess> &storage,
                                           const tcu::Sampler &sampler)
{
    return getEffectiveTView(src, storage, sampler);
}

tcu::Texture2DView getEffectiveTextureView(const tcu::Texture2DView &src,
                                           std::vector<tcu::ConstPixelBufferAccess> &storage,
                                           const tcu::Sampler &sampler)
{
    return getEffectiveTView(src, storage, sampler);
}

tcu::Texture3DView getEffectiveTextureView(const tcu::Texture3DView &src,
                                           std::vector<tcu::ConstPixelBufferAccess> &storage,
                                           const tcu::Sampler &sampler)
{
    return getEffectiveTView(src, storage, sampler);
}

tcu::Texture1DArrayView getEffectiveTextureView(const tcu::Texture1DArrayView &src,
                                                std::vector<tcu::ConstPixelBufferAccess> &storage,
                                                const tcu::Sampler &sampler)
{
    return getEffectiveTView(src, storage, sampler);
}

tcu::Texture2DArrayView getEffectiveTextureView(const tcu::Texture2DArrayView &src,
                                                std::vector<tcu::ConstPixelBufferAccess> &storage,
                                                const tcu::Sampler &sampler)
{
    return getEffectiveTView(src, storage, sampler);
}

tcu::TextureCubeView getEffectiveTextureView(const tcu::TextureCubeView &src,
                                             std::vector<tcu::ConstPixelBufferAccess> &storage,
                                             const tcu::Sampler &sampler)
{
    return getEffectiveTView(src, storage, sampler);
}

tcu::TextureCubeArrayView getEffectiveTextureView(const tcu::TextureCubeArrayView &src,
                                                  std::vector<tcu::ConstPixelBufferAccess> &storage,
                                                  const tcu::Sampler &sampler)
{
    return getEffectiveTView(src, storage, sampler);
}

//! Returns the effective swizzle of a border color. The effective swizzle is the
//! equal to first writing an RGBA color with a write swizzle and then reading
//! it back using a read swizzle, i.e. BorderSwizzle(c) == readSwizzle(writeSwizzle(C))
static const TextureSwizzle &getBorderColorReadSwizzle(TextureFormat::ChannelOrder order)
{
    // make sure to update these tables when channel orders are updated
    DE_STATIC_ASSERT(TextureFormat::CHANNELORDER_LAST == 22);

    static const TextureSwizzle INV = {{TextureSwizzle::CHANNEL_ZERO, TextureSwizzle::CHANNEL_ZERO,
                                        TextureSwizzle::CHANNEL_ZERO, TextureSwizzle::CHANNEL_ONE}};
    static const TextureSwizzle R   = {{TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_ZERO,
                                        TextureSwizzle::CHANNEL_ZERO, TextureSwizzle::CHANNEL_ONE}};
    static const TextureSwizzle A   = {{TextureSwizzle::CHANNEL_ZERO, TextureSwizzle::CHANNEL_ZERO,
                                        TextureSwizzle::CHANNEL_ZERO, TextureSwizzle::CHANNEL_3}};
    static const TextureSwizzle I   = {
        {TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_0}};
    static const TextureSwizzle L = {
        {TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_ONE}};
    static const TextureSwizzle LA = {
        {TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_3}};
    static const TextureSwizzle RG  = {{TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_1,
                                        TextureSwizzle::CHANNEL_ZERO, TextureSwizzle::CHANNEL_ONE}};
    static const TextureSwizzle RA  = {{TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_ZERO,
                                        TextureSwizzle::CHANNEL_ZERO, TextureSwizzle::CHANNEL_3}};
    static const TextureSwizzle RGB = {
        {TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_1, TextureSwizzle::CHANNEL_2, TextureSwizzle::CHANNEL_ONE}};
    static const TextureSwizzle RGBA = {
        {TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_1, TextureSwizzle::CHANNEL_2, TextureSwizzle::CHANNEL_3}};
    static const TextureSwizzle D = {{TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_ZERO,
                                      TextureSwizzle::CHANNEL_ZERO, TextureSwizzle::CHANNEL_ONE}};
    static const TextureSwizzle S = {{TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_ZERO,
                                      TextureSwizzle::CHANNEL_ZERO, TextureSwizzle::CHANNEL_ONE}};

    const TextureSwizzle *swizzle;

    switch (order)
    {
    case TextureFormat::R:
        swizzle = &R;
        break;
    case TextureFormat::A:
        swizzle = &A;
        break;
    case TextureFormat::I:
        swizzle = &I;
        break;
    case TextureFormat::L:
        swizzle = &L;
        break;
    case TextureFormat::LA:
        swizzle = &LA;
        break;
    case TextureFormat::RG:
        swizzle = &RG;
        break;
    case TextureFormat::RA:
        swizzle = &RA;
        break;
    case TextureFormat::RGB:
        swizzle = &RGB;
        break;
    case TextureFormat::RGBA:
        swizzle = &RGBA;
        break;
    case TextureFormat::ARGB:
        swizzle = &RGBA;
        break;
    case TextureFormat::ABGR:
        swizzle = &RGBA;
        break;
    case TextureFormat::BGR:
        swizzle = &RGB;
        break;
    case TextureFormat::BGRA:
        swizzle = &RGBA;
        break;
    case TextureFormat::sR:
        swizzle = &R;
        break;
    case TextureFormat::sRG:
        swizzle = &RG;
        break;
    case TextureFormat::sRGB:
        swizzle = &RGB;
        break;
    case TextureFormat::sRGBA:
        swizzle = &RGBA;
        break;
    case TextureFormat::sBGR:
        swizzle = &RGB;
        break;
    case TextureFormat::sBGRA:
        swizzle = &RGBA;
        break;
    case TextureFormat::D:
        swizzle = &D;
        break;
    case TextureFormat::S:
        swizzle = &S;
        break;

    case TextureFormat::DS:
        DE_ASSERT(false); // combined depth-stencil border color?
        swizzle = &INV;
        break;

    default:
        DE_ASSERT(false);
        swizzle = &INV;
        break;
    }

#ifdef DE_DEBUG

    {
        // check that BorderSwizzle(c) == readSwizzle(writeSwizzle(C))
        const TextureSwizzle &readSwizzle  = getChannelReadSwizzle(order);
        const TextureSwizzle &writeSwizzle = getChannelWriteSwizzle(order);

        for (int ndx = 0; ndx < 4; ++ndx)
        {
            TextureSwizzle::Channel writeRead = readSwizzle.components[ndx];
            if (deInRange32(writeRead, TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_3) == true)
                writeRead = writeSwizzle.components[(int)writeRead];
            DE_ASSERT(writeRead == swizzle->components[ndx]);
        }
    }

#endif

    return *swizzle;
}

static tcu::UVec4 getNBitUnsignedIntegerVec4MaxValue(const tcu::IVec4 &numBits)
{
    return tcu::UVec4((numBits[0] > 0) ? (deUintMaxValue32(numBits[0])) : (0),
                      (numBits[1] > 0) ? (deUintMaxValue32(numBits[1])) : (0),
                      (numBits[2] > 0) ? (deUintMaxValue32(numBits[2])) : (0),
                      (numBits[3] > 0) ? (deUintMaxValue32(numBits[3])) : (0));
}

static tcu::IVec4 getNBitSignedIntegerVec4MaxValue(const tcu::IVec4 &numBits)
{
    return tcu::IVec4(
        (numBits[0] > 0) ? (deIntMaxValue32(numBits[0])) : (0), (numBits[1] > 0) ? (deIntMaxValue32(numBits[1])) : (0),
        (numBits[2] > 0) ? (deIntMaxValue32(numBits[2])) : (0), (numBits[3] > 0) ? (deIntMaxValue32(numBits[3])) : (0));
}

static tcu::IVec4 getNBitSignedIntegerVec4MinValue(const tcu::IVec4 &numBits)
{
    return tcu::IVec4(
        (numBits[0] > 0) ? (deIntMinValue32(numBits[0])) : (0), (numBits[1] > 0) ? (deIntMinValue32(numBits[1])) : (0),
        (numBits[2] > 0) ? (deIntMinValue32(numBits[2])) : (0), (numBits[3] > 0) ? (deIntMinValue32(numBits[3])) : (0));
}

static tcu::Vec4 getTextureBorderColorFloat(const TextureFormat &format, const Sampler &sampler)
{
    const tcu::TextureChannelClass channelClass = getTextureChannelClass(format.type);
    const TextureSwizzle::Channel *channelMap   = getBorderColorReadSwizzle(format.order).components;
    const bool isFloat                          = channelClass == tcu::TEXTURECHANNELCLASS_FLOATING_POINT;
    const bool isSigned                         = channelClass != tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT;
    const float valueMin                        = (isSigned) ? (-1.0f) : (0.0f);
    const float valueMax                        = 1.0f;
    Vec4 result;

    DE_ASSERT(channelClass == tcu::TEXTURECHANNELCLASS_FLOATING_POINT ||
              channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT ||
              channelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT);

    for (int c = 0; c < 4; c++)
    {
        const TextureSwizzle::Channel map = channelMap[c];
        if (map == TextureSwizzle::CHANNEL_ZERO)
            result[c] = 0.0f;
        else if (map == TextureSwizzle::CHANNEL_ONE)
            result[c] = 1.0f;
        else if (isFloat)
        {
            // floating point values are not clamped
            result[c] = sampler.borderColor.getAccess<float>()[(int)map];
        }
        else
        {
            // fixed point values are clamped to a representable range
            result[c] = de::clamp(sampler.borderColor.getAccess<float>()[(int)map], valueMin, valueMax);
        }
    }

    return result;
}

static tcu::IVec4 getTextureBorderColorInt(const TextureFormat &format, const Sampler &sampler)
{
    const tcu::TextureChannelClass channelClass = getTextureChannelClass(format.type);
    const TextureSwizzle::Channel *channelMap   = getBorderColorReadSwizzle(format.order).components;
    const IVec4 channelBits                     = getChannelBitDepth(format.type);
    const IVec4 valueMin                        = getNBitSignedIntegerVec4MinValue(channelBits);
    const IVec4 valueMax                        = getNBitSignedIntegerVec4MaxValue(channelBits);
    IVec4 result;

    DE_ASSERT(channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER);
    DE_UNREF(channelClass);

    for (int c = 0; c < 4; c++)
    {
        const TextureSwizzle::Channel map = channelMap[c];
        if (map == TextureSwizzle::CHANNEL_ZERO)
            result[c] = 0;
        else if (map == TextureSwizzle::CHANNEL_ONE)
            result[c] = 1;
        else
        {
            // integer values are clamped to a representable range
            result[c] =
                de::clamp(sampler.borderColor.getAccess<int32_t>()[(int)map], valueMin[(int)map], valueMax[(int)map]);
        }
    }

    return result;
}

static tcu::UVec4 getTextureBorderColorUint(const TextureFormat &format, const Sampler &sampler)
{
    const tcu::TextureChannelClass channelClass = getTextureChannelClass(format.type);
    const TextureSwizzle::Channel *channelMap   = getBorderColorReadSwizzle(format.order).components;
    const IVec4 channelBits                     = getChannelBitDepth(format.type);
    const UVec4 valueMax                        = getNBitUnsignedIntegerVec4MaxValue(channelBits);
    UVec4 result;

    DE_ASSERT(channelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER);
    DE_UNREF(channelClass);

    for (int c = 0; c < 4; c++)
    {
        const TextureSwizzle::Channel map = channelMap[c];
        if (map == TextureSwizzle::CHANNEL_ZERO)
            result[c] = 0;
        else if (map == TextureSwizzle::CHANNEL_ONE)
            result[c] = 1;
        else
        {
            // integer values are clamped to a representable range
            result[c] = de::min(sampler.borderColor.getAccess<uint32_t>()[(int)map], valueMax[(int)map]);
        }
    }

    return result;
}

template <typename ScalarType>
tcu::Vector<ScalarType, 4> sampleTextureBorder(const TextureFormat &format, const Sampler &sampler)
{
    const tcu::TextureChannelClass channelClass = getTextureChannelClass(format.type);

    switch (channelClass)
    {
    case tcu::TEXTURECHANNELCLASS_FLOATING_POINT:
    case tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT:
    case tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT:
        return getTextureBorderColorFloat(format, sampler).cast<ScalarType>();

    case tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER:
        return getTextureBorderColorInt(format, sampler).cast<ScalarType>();

    case tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER:
        return getTextureBorderColorUint(format, sampler).cast<ScalarType>();

    default:
        DE_ASSERT(false);
        return tcu::Vector<ScalarType, 4>();
    }
}

// instantiation
template tcu::Vector<float, 4> sampleTextureBorder(const TextureFormat &format, const Sampler &sampler);
template tcu::Vector<int32_t, 4> sampleTextureBorder(const TextureFormat &format, const Sampler &sampler);
template tcu::Vector<uint32_t, 4> sampleTextureBorder(const TextureFormat &format, const Sampler &sampler);

} // namespace tcu
