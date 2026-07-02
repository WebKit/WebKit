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
 * \brief Reference Texture Implementation.
 *//*--------------------------------------------------------------------*/

#include "tcuTexture.hpp"
#include "deInt32.h"
#include "deFloat16.h"
#include "deMath.h"
#include "deMemory.h"
#include "tcuTestLog.hpp"
#include "tcuSurface.hpp"
#include "tcuFloat.hpp"
#include "tcuTextureUtil.hpp"
#include "deStringUtil.hpp"
#include "deArrayUtil.hpp"
#include "tcuMatrix.hpp"

#include <cstring>
#include <limits>

namespace tcu
{

// \note No sign. Denorms are supported.
typedef Float<uint32_t, 5, 6, 15, FLOAT_SUPPORT_DENORM> Float11;
typedef Float<uint32_t, 5, 5, 15, FLOAT_SUPPORT_DENORM> Float10;

namespace
{

// Optimized getters for common formats.
// \todo [2012-11-14 pyry] Use intrinsics if available.

inline Vec4 readRGBA8888Float(const uint8_t *ptr)
{
    return Vec4(ptr[0] / 255.0f, ptr[1] / 255.0f, ptr[2] / 255.0f, ptr[3] / 255.0f);
}
inline Vec4 readRGB888Float(const uint8_t *ptr)
{
    return Vec4(ptr[0] / 255.0f, ptr[1] / 255.0f, ptr[2] / 255.0f, 1.0f);
}
inline IVec4 readRGBA8888Int(const uint8_t *ptr)
{
    return IVec4(ptr[0], ptr[1], ptr[2], ptr[3]);
}
inline IVec4 readRGB888Int(const uint8_t *ptr)
{
    return IVec4(ptr[0], ptr[1], ptr[2], 1);
}

// Optimized setters.

inline void writeRGBA8888Int(uint8_t *ptr, const IVec4 &val)
{
    ptr[0] = (uint8_t)de::clamp(val[0], 0, 255);
    ptr[1] = (uint8_t)de::clamp(val[1], 0, 255);
    ptr[2] = (uint8_t)de::clamp(val[2], 0, 255);
    ptr[3] = (uint8_t)de::clamp(val[3], 0, 255);
}

inline void writeRGB888Int(uint8_t *ptr, const IVec4 &val)
{
    ptr[0] = (uint8_t)de::clamp(val[0], 0, 255);
    ptr[1] = (uint8_t)de::clamp(val[1], 0, 255);
    ptr[2] = (uint8_t)de::clamp(val[2], 0, 255);
}

inline void writeRGBA8888Float(uint8_t *ptr, const Vec4 &val)
{
    ptr[0] = floatToU8(val[0]);
    ptr[1] = floatToU8(val[1]);
    ptr[2] = floatToU8(val[2]);
    ptr[3] = floatToU8(val[3]);
}

inline void writeRGB888Float(uint8_t *ptr, const Vec4 &val)
{
    ptr[0] = floatToU8(val[0]);
    ptr[1] = floatToU8(val[1]);
    ptr[2] = floatToU8(val[2]);
}

inline void writeUint24(uint8_t *dst, uint32_t val)
{
#if (DE_ENDIANNESS == DE_LITTLE_ENDIAN)
    dst[0] = (uint8_t)((val & 0x0000FFu) >> 0u);
    dst[1] = (uint8_t)((val & 0x00FF00u) >> 8u);
    dst[2] = (uint8_t)((val & 0xFF0000u) >> 16u);
#else
    dst[0] = (uint8_t)((val & 0xFF0000u) >> 16u);
    dst[1] = (uint8_t)((val & 0x00FF00u) >> 8u);
    dst[2] = (uint8_t)((val & 0x0000FFu) >> 0u);
#endif
}

inline uint32_t readUint24(const uint8_t *src)
{
#if (DE_ENDIANNESS == DE_LITTLE_ENDIAN)
    return (((uint32_t)src[0]) << 0u) | (((uint32_t)src[1]) << 8u) | (((uint32_t)src[2]) << 16u);
#else
    return (((uint32_t)src[0]) << 16u) | (((uint32_t)src[1]) << 8u) | (((uint32_t)src[2]) << 0u);
#endif
}

template <typename T>
inline T readUnaligned(const uint8_t *ptr)
{
    T val;
    memcpy(&val, ptr, sizeof(T));
    return val;
}

template <typename T>
inline void writeUnaligned(uint8_t *ptr, T val)
{
    memcpy(ptr, &val, sizeof(T));
}

inline uint8_t readUint32Low8(const uint8_t *src)
{
#if (DE_ENDIANNESS == DE_LITTLE_ENDIAN)
    const uint32_t uint32ByteOffsetBits0To8 = 0; //!< least significant byte in the lowest address
#else
    const uint32_t uint32ByteOffsetBits0To8   = 3; //!< least significant byte in the highest address
#endif

    return src[uint32ByteOffsetBits0To8];
}

inline uint8_t readUint32High8(const uint8_t *src)
{
#if (DE_ENDIANNESS == DE_LITTLE_ENDIAN)
    const uint32_t uint32ByteOffsetBits24To32 = 3;
#else
    const uint32_t uint32ByteOffsetBits24To32 = 0;
#endif

    return src[uint32ByteOffsetBits24To32];
}

inline void writeUint32Low8(uint8_t *dst, uint8_t val)
{
#if (DE_ENDIANNESS == DE_LITTLE_ENDIAN)
    const uint32_t uint32ByteOffsetBits0To8 = 0; //!< least significant byte in the lowest address
#else
    const uint32_t uint32ByteOffsetBits0To8   = 3; //!< least significant byte in the highest address
#endif

    dst[uint32ByteOffsetBits0To8] = val;
}

inline void writeUint32High8(uint8_t *dst, uint8_t val)
{
#if (DE_ENDIANNESS == DE_LITTLE_ENDIAN)
    const uint32_t uint32ByteOffsetBits24To32 = 3;
#else
    const uint32_t uint32ByteOffsetBits24To32 = 0;
#endif

    dst[uint32ByteOffsetBits24To32] = val;
}

inline uint32_t readUint32High16(const uint8_t *src)
{
#if (DE_ENDIANNESS == DE_LITTLE_ENDIAN)
    const uint32_t uint32ByteOffset16To32 = 2;
#else
    const uint32_t uint32ByteOffset16To32     = 0;
#endif

    return readUnaligned<uint16_t>(src + uint32ByteOffset16To32);
}

inline void writeUint32High16(uint8_t *dst, uint16_t val)
{
#if (DE_ENDIANNESS == DE_LITTLE_ENDIAN)
    const uint32_t uint32ByteOffset16To32 = 2;
#else
    const uint32_t uint32ByteOffset16To32     = 0;
#endif

    writeUnaligned<uint16_t>(dst + uint32ByteOffset16To32, val);
}

inline uint32_t readUint32Low24(const uint8_t *src)
{
#if (DE_ENDIANNESS == DE_LITTLE_ENDIAN)
    const uint32_t uint32ByteOffset0To24 = 0;
#else
    const uint32_t uint32ByteOffset0To24      = 1;
#endif

    return readUint24(src + uint32ByteOffset0To24);
}

inline uint32_t readUint32High24(const uint8_t *src)
{
#if (DE_ENDIANNESS == DE_LITTLE_ENDIAN)
    const uint32_t uint32ByteOffset8To32 = 1;
#else
    const uint32_t uint32ByteOffset8To32      = 0;
#endif

    return readUint24(src + uint32ByteOffset8To32);
}

inline void writeUint32Low24(uint8_t *dst, uint32_t val)
{
#if (DE_ENDIANNESS == DE_LITTLE_ENDIAN)
    const uint32_t uint32ByteOffset0To24 = 0;
#else
    const uint32_t uint32ByteOffset0To24      = 1;
#endif

    writeUint24(dst + uint32ByteOffset0To24, val);
}

inline void writeUint32High24(uint8_t *dst, uint32_t val)
{
#if (DE_ENDIANNESS == DE_LITTLE_ENDIAN)
    const uint32_t uint32ByteOffset8To32 = 1;
#else
    const uint32_t uint32ByteOffset8To32      = 0;
#endif

    writeUint24(dst + uint32ByteOffset8To32, val);
}

// \todo [2011-09-21 pyry] Move to tcutil?
template <typename T>
inline T convertSatRte(float f)
{
    // \note Doesn't work for 64-bit types
    DE_STATIC_ASSERT(sizeof(T) < sizeof(uint64_t));
    DE_STATIC_ASSERT((-3 % 2 != 0) && (-4 % 2 == 0));

    int64_t minVal = std::numeric_limits<T>::min();
    int64_t maxVal = std::numeric_limits<T>::max();
    float q        = deFloatFrac(f);
    int64_t intVal = (int64_t)(f - q);

    // Rounding.
    if (q == 0.5f)
    {
        if (intVal % 2 != 0)
            intVal++;
    }
    else if (q > 0.5f)
        intVal++;
    // else Don't add anything

    // Saturate.
    intVal = de::max(minVal, de::min(maxVal, intVal));

    return (T)intVal;
}

inline uint32_t convertSatRteUint24(float f)
{
    const uint32_t rounded   = convertSatRte<uint32_t>(f);
    const uint32_t maxUint24 = 0xFFFFFFu;
    return de::min(rounded, maxUint24);
}

inline uint16_t convertSatRteUint10(float f)
{
    const uint16_t rounded   = convertSatRte<uint16_t>(f);
    const uint16_t maxUint10 = 0x3FFu;
    return de::min(rounded, maxUint10);
}

inline uint16_t convertSatRteUint12(float f)
{
    const uint16_t rounded   = convertSatRte<uint16_t>(f);
    const uint16_t maxUint12 = 0xFFFu;
    return de::min(rounded, maxUint12);
}

inline float channelToFloat(const uint8_t *value, TextureFormat::ChannelType type)
{
    // make sure this table is updated if format table is updated
    DE_STATIC_ASSERT(TextureFormat::CHANNELTYPE_LAST == 48);

    switch (type)
    {
    case TextureFormat::SNORM_INT8:
        return de::max(-1.0f, (float)*((const int8_t *)value) / 127.0f);
    case TextureFormat::SNORM_INT16:
        return de::max(-1.0f, (float)readUnaligned<int16_t>(value) / 32767.0f);
    case TextureFormat::SNORM_INT32:
        return de::max(-1.0f, (float)readUnaligned<int32_t>(value) / 2147483647.0f);
    case TextureFormat::UNORM_INT8:
        return (float)*((const uint8_t *)value) / 255.0f;
    case TextureFormat::UNORM_INT16:
        return (float)readUnaligned<uint16_t>(value) / 65535.0f;
    case TextureFormat::UNORM_INT24:
        return (float)readUint24(value) / 16777215.0f;
    case TextureFormat::UNORM_INT32:
        return (float)readUnaligned<uint32_t>(value) / 4294967295.0f;
    case TextureFormat::SIGNED_INT8:
        return (float)*((const int8_t *)value);
    case TextureFormat::SIGNED_INT16:
        return (float)readUnaligned<int16_t>(value);
    case TextureFormat::SIGNED_INT32:
        return (float)readUnaligned<int32_t>(value);
    case TextureFormat::SIGNED_INT64:
        return (float)readUnaligned<int64_t>(value);
    case TextureFormat::UNSIGNED_INT8:
        return (float)*((const uint8_t *)value);
    case TextureFormat::UNSIGNED_INT16:
        return (float)readUnaligned<uint16_t>(value);
    case TextureFormat::UNSIGNED_INT24:
        return (float)readUint24(value);
    case TextureFormat::UNSIGNED_INT32:
        return (float)readUnaligned<uint32_t>(value);
    case TextureFormat::UNSIGNED_INT64:
        return (float)readUnaligned<uint64_t>(value);
    case TextureFormat::HALF_FLOAT:
        return deFloat16To32(readUnaligned<deFloat16>(value));
    case TextureFormat::FLOAT:
        return readUnaligned<float>(value);
    case TextureFormat::FLOAT64:
        return (float)readUnaligned<double>(value);
    case TextureFormat::UNORM_SHORT_10:
        return (float)(readUnaligned<uint16_t>(value) >> 6u) / 1023.0f;
    case TextureFormat::UNORM_SHORT_12:
        return (float)(readUnaligned<uint16_t>(value) >> 4u) / 4095.0f;
    case TextureFormat::USCALED_INT8:
        return (float)*((const uint8_t *)value);
    case TextureFormat::USCALED_INT16:
        return (float)readUnaligned<uint16_t>(value);
    case TextureFormat::SSCALED_INT8:
        return (float)*((const int8_t *)value);
    case TextureFormat::SSCALED_INT16:
        return (float)readUnaligned<int16_t>(value);
    default:
        DE_ASSERT(false);
        return 0.0f;
    }
}

template <class T>
inline T channelToIntType(const uint8_t *value, TextureFormat::ChannelType type)
{
    // make sure this table is updated if format table is updated
    DE_STATIC_ASSERT(TextureFormat::CHANNELTYPE_LAST == 48);

    switch (type)
    {
    case TextureFormat::SNORM_INT8:
        return (T) * ((const int8_t *)value);
    case TextureFormat::SNORM_INT16:
        return (T)readUnaligned<int16_t>(value);
    case TextureFormat::SNORM_INT32:
        return (T)readUnaligned<int32_t>(value);
    case TextureFormat::UNORM_INT8:
        return (T) * ((const uint8_t *)value);
    case TextureFormat::UNORM_INT16:
        return (T)readUnaligned<uint16_t>(value);
    case TextureFormat::UNORM_INT24:
        return (T)readUint24(value);
    case TextureFormat::UNORM_INT32:
        return (T)readUnaligned<uint32_t>(value);
    case TextureFormat::SIGNED_INT8:
        return (T) * ((const int8_t *)value);
    case TextureFormat::SIGNED_INT16:
        return (T)readUnaligned<int16_t>(value);
    case TextureFormat::SIGNED_INT32:
        return (T)readUnaligned<int32_t>(value);
    case TextureFormat::SIGNED_INT64:
        return (T)readUnaligned<int64_t>(value);
    case TextureFormat::UNSIGNED_INT8:
        return (T) * ((const uint8_t *)value);
    case TextureFormat::UNSIGNED_INT16:
        return (T)readUnaligned<uint16_t>(value);
    case TextureFormat::UNSIGNED_INT24:
        return (T)readUint24(value);
    case TextureFormat::UNSIGNED_INT32:
        return (T)readUnaligned<uint32_t>(value);
    case TextureFormat::UNSIGNED_INT64:
        return (T)readUnaligned<uint64_t>(value);
    case TextureFormat::HALF_FLOAT:
        return (T)deFloat16To32(readUnaligned<deFloat16>(value));
    case TextureFormat::FLOAT:
        return (T)readUnaligned<float>(value);
    case TextureFormat::FLOAT64:
        return (T)readUnaligned<double>(value);
    case TextureFormat::UNORM_SHORT_10:
        return (T)(readUnaligned<uint16_t>(value) >> 6u);
    case TextureFormat::UNORM_SHORT_12:
        return (T)(readUnaligned<uint16_t>(value) >> 4u);
    case TextureFormat::USCALED_INT8:
        return (T) * ((const uint8_t *)value);
    case TextureFormat::USCALED_INT16:
        return (T)readUnaligned<uint16_t>(value);
    case TextureFormat::SSCALED_INT8:
        return (T) * ((const int8_t *)value);
    case TextureFormat::SSCALED_INT16:
        return (T)readUnaligned<int16_t>(value);
    // required by modifiers tests
    case TextureFormat::UNSIGNED_INT_11F_11F_10F_REV:
        return (T)readUnaligned<uint32_t>(value);
    case TextureFormat::UNORM_INT_1010102_REV:
        return (T)readUnaligned<uint32_t>(value);
    case TextureFormat::SNORM_INT_1010102_REV:
        return (T)readUnaligned<uint32_t>(value);
    case TextureFormat::USCALED_INT_1010102_REV:
        return (T)readUnaligned<uint32_t>(value);
    case TextureFormat::SSCALED_INT_1010102_REV:
        return (T)readUnaligned<uint32_t>(value);
    case TextureFormat::UNSIGNED_INT_1010102_REV:
        return (T)readUnaligned<uint32_t>(value);
    case TextureFormat::SIGNED_INT_1010102_REV:
        return (T)readUnaligned<uint32_t>(value);
    case TextureFormat::UNSIGNED_INT_999_E5_REV:
        return (T)readUnaligned<uint32_t>(value);
    default:
        DE_ASSERT(false);
        return 0;
    }
}

inline uint64_t retrieveChannelBitsAsUint64(const uint8_t *value, TextureFormat::ChannelType type)
{
    // make sure this table is updated if format table is updated
    DE_STATIC_ASSERT(TextureFormat::CHANNELTYPE_LAST == 48);

    switch (type)
    {
    case TextureFormat::SNORM_INT8:
        return (uint64_t) * ((const uint8_t *)value);
    case TextureFormat::SNORM_INT16:
        return (uint64_t)readUnaligned<uint16_t>(value);
    case TextureFormat::SNORM_INT32:
        return (uint64_t)readUnaligned<uint32_t>(value);
    case TextureFormat::UNORM_INT8:
        return (uint64_t) * ((const uint8_t *)value);
    case TextureFormat::UNORM_INT16:
        return (uint64_t)readUnaligned<uint16_t>(value);
    case TextureFormat::UNORM_INT24:
        return (uint64_t)readUint24(value);
    case TextureFormat::UNORM_INT32:
        return (uint64_t)readUnaligned<uint32_t>(value);
    case TextureFormat::SIGNED_INT8:
        return (uint64_t) * ((const uint8_t *)value);
    case TextureFormat::SIGNED_INT16:
        return (uint64_t)readUnaligned<uint16_t>(value);
    case TextureFormat::SIGNED_INT32:
        return (uint64_t)readUnaligned<uint32_t>(value);
    case TextureFormat::SIGNED_INT64:
        return (uint64_t)readUnaligned<uint64_t>(value);
    case TextureFormat::UNSIGNED_INT8:
        return (uint64_t) * ((const uint8_t *)value);
    case TextureFormat::UNSIGNED_INT16:
        return (uint64_t)readUnaligned<uint16_t>(value);
    case TextureFormat::UNSIGNED_INT24:
        return (uint64_t)readUint24(value);
    case TextureFormat::UNSIGNED_INT32:
        return (uint64_t)readUnaligned<uint32_t>(value);
    case TextureFormat::UNSIGNED_INT64:
        return (uint64_t)readUnaligned<uint64_t>(value);
    case TextureFormat::HALF_FLOAT:
        return (uint64_t)readUnaligned<uint16_t>(value);
    case TextureFormat::FLOAT:
        return (uint64_t)readUnaligned<uint32_t>(value);
    case TextureFormat::FLOAT64:
        return (uint64_t)readUnaligned<uint64_t>(value);
    case TextureFormat::UNORM_SHORT_10:
        return (uint64_t)(readUnaligned<uint16_t>(value) >> 6u);
    case TextureFormat::UNORM_SHORT_12:
        return (uint64_t)(readUnaligned<uint16_t>(value) >> 4u);
    case TextureFormat::USCALED_INT8:
        return (uint64_t) * ((const uint8_t *)value);
    case TextureFormat::USCALED_INT16:
        return (uint64_t)readUnaligned<uint16_t>(value);
    case TextureFormat::SSCALED_INT8:
        return (uint64_t) * ((const uint8_t *)value);
    case TextureFormat::SSCALED_INT16:
        return (uint64_t)readUnaligned<uint16_t>(value);
    default:
        DE_ASSERT(false);
        return 0;
    }
}

inline int channelToInt(const uint8_t *value, TextureFormat::ChannelType type)
{
    return channelToIntType<int>(value, type);
}

void floatToChannel(uint8_t *dst, float src, TextureFormat::ChannelType type)
{
    // make sure this table is updated if format table is updated
    DE_STATIC_ASSERT(TextureFormat::CHANNELTYPE_LAST == 48);

    switch (type)
    {
    case TextureFormat::SNORM_INT8:
        writeUnaligned<int8_t>(dst, convertSatRte<int8_t>(src * 127.0f));
        break;
    case TextureFormat::SNORM_INT16:
        writeUnaligned<int16_t>(dst, convertSatRte<int16_t>(src * 32767.0f));
        break;
    case TextureFormat::SNORM_INT32:
        writeUnaligned<int32_t>(dst, convertSatRte<int32_t>(src * 2147483647.0f));
        break;
    case TextureFormat::UNORM_INT8:
        writeUnaligned<uint8_t>(dst, convertSatRte<uint8_t>(src * 255.0f));
        break;
    case TextureFormat::UNORM_INT16:
        writeUnaligned<uint16_t>(dst, convertSatRte<uint16_t>(src * 65535.0f));
        break;
    case TextureFormat::UNORM_INT24:
        writeUint24(dst, convertSatRteUint24(src * 16777215.0f));
        break;
    case TextureFormat::UNORM_INT32:
        writeUnaligned<uint32_t>(dst, convertSatRte<uint32_t>(src * 4294967295.0f));
        break;
    case TextureFormat::SIGNED_INT8:
        writeUnaligned<int8_t>(dst, convertSatRte<int8_t>(src));
        break;
    case TextureFormat::SIGNED_INT16:
        writeUnaligned<int16_t>(dst, convertSatRte<int16_t>(src));
        break;
    case TextureFormat::SIGNED_INT32:
        writeUnaligned<int32_t>(dst, convertSatRte<int32_t>(src));
        break;
    case TextureFormat::SIGNED_INT64:
        writeUnaligned<int64_t>(dst, convertSatRte<int32_t>(src)); // Note we convert as 32-bits, but store as 64.
        break;
    case TextureFormat::UNSIGNED_INT8:
        writeUnaligned<uint8_t>(dst, convertSatRte<uint8_t>(src));
        break;
    case TextureFormat::UNSIGNED_INT16:
        writeUnaligned<uint16_t>(dst, convertSatRte<uint16_t>(src));
        break;
    case TextureFormat::UNSIGNED_INT24:
        writeUint24(dst, convertSatRteUint24(src));
        break;
    case TextureFormat::UNSIGNED_INT32:
        writeUnaligned<uint32_t>(dst, convertSatRte<uint32_t>(src));
        break;
    case TextureFormat::UNSIGNED_INT64:
        writeUnaligned<uint64_t>(dst, convertSatRte<uint32_t>(src)); // Note we convert as 32-bits, but store as 64.
        break;
    case TextureFormat::HALF_FLOAT:
        writeUnaligned<deFloat16>(dst, deFloat32To16(src));
        break;
    case TextureFormat::FLOAT:
        writeUnaligned<float>(dst, src);
        break;
    case TextureFormat::FLOAT64:
        writeUnaligned<double>(dst, (double)src);
        break;
    case TextureFormat::UNORM_SHORT_10:
        writeUnaligned<uint16_t>(dst, (uint16_t)(convertSatRteUint10(src * 1023.0f) << 6u));
        break;
    case TextureFormat::UNORM_SHORT_12:
        writeUnaligned<uint16_t>(dst, (uint16_t)(convertSatRteUint12(src * 4095.0f) << 4u));
        break;
    case TextureFormat::USCALED_INT8:
        writeUnaligned<uint8_t>(dst, convertSatRte<uint8_t>(src));
        break;
    case TextureFormat::USCALED_INT16:
        writeUnaligned<uint16_t>(dst, convertSatRte<uint16_t>(src));
        break;
    case TextureFormat::SSCALED_INT8:
        writeUnaligned<int8_t>(dst, convertSatRte<int8_t>(src));
        break;
    case TextureFormat::SSCALED_INT16:
        writeUnaligned<int16_t>(dst, convertSatRte<int16_t>(src));
        break;
    default:
        DE_ASSERT(false);
    }
}

template <typename T, typename S>
static inline T convertSat(S src)
{
    S min = (S)std::numeric_limits<T>::min();
    S max = (S)std::numeric_limits<T>::max();

    if (src < min)
        return (T)min;
    else if (src > max)
        return (T)max;
    else
        return (T)src;
}

template <typename S>
static inline uint32_t convertSatUint24(S src)
{
    S min = (S)0u;
    S max = (S)0xFFFFFFu;

    if (src < min)
        return (uint32_t)min;
    else if (src > max)
        return (uint32_t)max;
    else
        return (uint32_t)src;
}

template <typename S>
static inline uint16_t convertSatUint10(S src)
{
    S min = (S)0u;
    S max = (S)0x3FFu;

    if (src < min)
        return (uint16_t)min;
    else if (src > max)
        return (uint16_t)max;
    else
        return (uint16_t)src;
}

template <typename S>
static inline uint16_t convertSatUint12(S src)
{
    S min = (S)0u;
    S max = (S)0xFFFu;

    if (src < min)
        return (uint16_t)min;
    else if (src > max)
        return (uint16_t)max;
    else
        return (uint16_t)src;
}

void intToChannel(uint8_t *dst, int src, TextureFormat::ChannelType type)
{
    // make sure this table is updated if format table is updated
    DE_STATIC_ASSERT(TextureFormat::CHANNELTYPE_LAST == 48);

    switch (type)
    {
    case TextureFormat::SNORM_INT8:
        writeUnaligned<int8_t>(dst, convertSat<int8_t>(src));
        break;
    case TextureFormat::SNORM_INT16:
        writeUnaligned<int16_t>(dst, convertSat<int16_t>(src));
        break;
    case TextureFormat::UNORM_INT8:
        writeUnaligned<uint8_t>(dst, convertSat<uint8_t>(src));
        break;
    case TextureFormat::UNORM_INT16:
        writeUnaligned<uint16_t>(dst, convertSat<uint16_t>(src));
        break;
    case TextureFormat::UNORM_INT24:
        writeUint24(dst, convertSatUint24(src));
        break;
    case TextureFormat::SIGNED_INT8:
        writeUnaligned<int8_t>(dst, convertSat<int8_t>(src));
        break;
    case TextureFormat::SIGNED_INT16:
        writeUnaligned<int16_t>(dst, convertSat<int16_t>(src));
        break;
    case TextureFormat::SIGNED_INT32:
        writeUnaligned<int32_t>(dst, convertSat<int32_t>(src));
        break;
    case TextureFormat::SIGNED_INT64:
        writeUnaligned<int64_t>(dst, convertSat<int64_t>((int64_t)src));
        break;
    case TextureFormat::UNSIGNED_INT8:
        writeUnaligned<uint8_t>(dst, convertSat<uint8_t>((uint32_t)src));
        break;
    case TextureFormat::UNSIGNED_INT16:
        writeUnaligned<uint16_t>(dst, convertSat<uint16_t>((uint32_t)src));
        break;
    case TextureFormat::UNSIGNED_INT24:
        writeUint24(dst, convertSatUint24((uint32_t)src));
        break;
    case TextureFormat::UNSIGNED_INT32:
        writeUnaligned<uint32_t>(dst, convertSat<uint32_t>((uint32_t)src));
        break;
    case TextureFormat::UNSIGNED_INT64:
        writeUnaligned<uint64_t>(dst, convertSat<uint64_t>((uint64_t)src));
        break;
    case TextureFormat::HALF_FLOAT:
        writeUnaligned<deFloat16>(dst, deFloat32To16((float)src));
        break;
    case TextureFormat::FLOAT:
        writeUnaligned<float>(dst, (float)src);
        break;
    case TextureFormat::FLOAT64:
        writeUnaligned<double>(dst, (double)src);
        break;
    case TextureFormat::UNORM_SHORT_10:
        writeUnaligned<uint16_t>(dst, (uint16_t)(convertSatUint10(src) << 6u));
        break;
    case TextureFormat::UNORM_SHORT_12:
        writeUnaligned<uint16_t>(dst, (uint16_t)(convertSatUint12(src) << 4u));
        break;
    case TextureFormat::USCALED_INT8:
        writeUnaligned<uint8_t>(dst, convertSat<uint8_t>((uint32_t)src));
        break;
    case TextureFormat::USCALED_INT16:
        writeUnaligned<uint16_t>(dst, convertSat<uint16_t>((uint32_t)src));
        break;
    case TextureFormat::SSCALED_INT8:
        writeUnaligned<int8_t>(dst, convertSat<int8_t>(src));
        break;
    case TextureFormat::SSCALED_INT16:
        writeUnaligned<int16_t>(dst, convertSat<int16_t>(src));
        break;
    default:
        DE_ASSERT(false);
    }
}

inline float channelToUnormFloat(uint32_t src, int bits)
{
    const uint32_t maxVal = (1u << bits) - 1;

    // \note Will lose precision if bits > 23
    return (float)src / (float)maxVal;
}

//! Extend < 32b signed integer to 32b
inline int32_t signExtend(uint32_t src, int bits)
{
    const uint32_t signBit = 1u << (bits - 1);

    src |= ~((src & signBit) - 1);

    return (int32_t)src;
}

inline float channelToSnormFloat(uint32_t src, int bits)
{
    const uint32_t range = (1u << (bits - 1)) - 1;

    // \note Will lose precision if bits > 24
    return de::max(-1.0f, (float)signExtend(src, bits) / (float)range);
}

inline uint32_t unormFloatToChannel(float src, int bits)
{
    const uint32_t maxVal = (1u << bits) - 1;
    const uint32_t intVal = convertSatRte<uint32_t>(src * (float)maxVal);

    return de::min(intVal, maxVal);
}

inline uint32_t snormFloatToChannel(float src, int bits)
{
    const int32_t range  = (int32_t)((1u << (bits - 1)) - 1u);
    const uint32_t mask  = (1u << bits) - 1;
    const int32_t intVal = convertSatRte<int32_t>(src * (float)range);

    return (uint32_t)de::clamp(intVal, -range, range) & mask;
}

inline uint32_t uintToChannel(uint32_t src, int bits)
{
    const uint32_t maxVal = (1u << bits) - 1;
    return de::min(src, maxVal);
}

inline uint32_t intToChannel(int32_t src, int bits)
{
    const int32_t minVal = -(int32_t)(1u << (bits - 1));
    const int32_t maxVal = (int32_t)((1u << (bits - 1)) - 1u);
    const uint32_t mask  = (1u << bits) - 1;

    return (uint32_t)de::clamp(src, minVal, maxVal) & mask;
}

bool isColorOrder(TextureFormat::ChannelOrder order)
{
    DE_STATIC_ASSERT(TextureFormat::CHANNELORDER_LAST == 22);

    switch (order)
    {
    case TextureFormat::R:
    case TextureFormat::A:
    case TextureFormat::I:
    case TextureFormat::L:
    case TextureFormat::LA:
    case TextureFormat::RG:
    case TextureFormat::RA:
    case TextureFormat::RGB:
    case TextureFormat::RGBA:
    case TextureFormat::ARGB:
    case TextureFormat::ABGR:
    case TextureFormat::BGR:
    case TextureFormat::BGRA:
    case TextureFormat::sR:
    case TextureFormat::sRG:
    case TextureFormat::sRGB:
    case TextureFormat::sRGBA:
    case TextureFormat::sBGR:
    case TextureFormat::sBGRA:
        return true;

    default:
        return false;
    }
}

float getImageViewMinLod(ImageViewMinLod &l)
{
    return (l.mode == IMAGEVIEWMINLODMODE_PREFERRED) ? l.value : deFloatFloor(l.value);
}

} // namespace

tcu::Vec4 unpackRGB999E5(uint32_t color)
{
    const int mBits = 9;
    const int eBias = 15;

    uint32_t exp = color >> 27;
    uint32_t bs  = (color >> 18) & ((1 << 9) - 1);
    uint32_t gs  = (color >> 9) & ((1 << 9) - 1);
    uint32_t rs  = color & ((1 << 9) - 1);

    float e = deFloatPow(2.0f, (float)((int)exp - eBias - mBits));
    float r = (float)rs * e;
    float g = (float)gs * e;
    float b = (float)bs * e;

    return tcu::Vec4(r, g, b, 1.0f);
}

bool isValid(TextureFormat format)
{
    const bool isColor = isColorOrder(format.order);

    switch (format.type)
    {
    case TextureFormat::SNORM_INT8:
    case TextureFormat::SNORM_INT16:
    case TextureFormat::SNORM_INT32:
        return isColor;

    case TextureFormat::UNORM_INT8:
    case TextureFormat::UNORM_INT16:
    case TextureFormat::UNORM_INT24:
    case TextureFormat::UNORM_INT32:
        return isColor || format.order == TextureFormat::D;

    case TextureFormat::UNORM_BYTE_44:
    case TextureFormat::UNSIGNED_BYTE_44:
        return format.order == TextureFormat::RG;

    case TextureFormat::UNORM_SHORT_565:
    case TextureFormat::UNORM_SHORT_555:
    case TextureFormat::UNSIGNED_SHORT_565:
        return format.order == TextureFormat::RGB || format.order == TextureFormat::BGR;

    case TextureFormat::UNORM_SHORT_4444:
    case TextureFormat::UNORM_SHORT_5551:
    case TextureFormat::UNSIGNED_SHORT_4444:
    case TextureFormat::UNSIGNED_SHORT_5551:
        return format.order == TextureFormat::RGBA || format.order == TextureFormat::BGRA ||
               format.order == TextureFormat::ARGB || format.order == TextureFormat::ABGR;

    case TextureFormat::UNORM_SHORT_1555:
        return format.order == TextureFormat::ARGB || format.order == TextureFormat::ABGR ||
               format.order == TextureFormat::BGRA;

    case TextureFormat::UNORM_INT_101010:
        return format.order == TextureFormat::RGB;

    case TextureFormat::SNORM_INT_1010102_REV:
    case TextureFormat::UNORM_INT_1010102_REV:
    case TextureFormat::SIGNED_INT_1010102_REV:
    case TextureFormat::UNSIGNED_INT_1010102_REV:
    case TextureFormat::USCALED_INT_1010102_REV:
    case TextureFormat::SSCALED_INT_1010102_REV:
        return format.order == TextureFormat::RGBA || format.order == TextureFormat::BGRA;

    case TextureFormat::UNSIGNED_INT_11F_11F_10F_REV:
    case TextureFormat::UNSIGNED_INT_999_E5_REV:
        return format.order == TextureFormat::RGB;

    case TextureFormat::UNSIGNED_INT_16_8_8:
        return format.order == TextureFormat::DS;

    case TextureFormat::UNSIGNED_INT_24_8:
    case TextureFormat::UNSIGNED_INT_24_8_REV:
        return format.order == TextureFormat::D || format.order == TextureFormat::DS;

    case TextureFormat::SIGNED_INT8:
    case TextureFormat::SIGNED_INT16:
    case TextureFormat::SIGNED_INT32:
    case TextureFormat::SSCALED_INT8:
    case TextureFormat::SSCALED_INT16:
    case TextureFormat::SIGNED_INT64:
        return isColor;

    case TextureFormat::UNSIGNED_INT8:
    case TextureFormat::UNSIGNED_INT16:
    case TextureFormat::UNSIGNED_INT24:
    case TextureFormat::UNSIGNED_INT32:
    case TextureFormat::USCALED_INT8:
    case TextureFormat::USCALED_INT16:
    case TextureFormat::UNSIGNED_INT64:
        return isColor || format.order == TextureFormat::S;

    case TextureFormat::HALF_FLOAT:
    case TextureFormat::FLOAT:
    case TextureFormat::FLOAT64:
        return isColor || format.order == TextureFormat::D;

    case TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV:
        return format.order == TextureFormat::DS;

    case TextureFormat::UNORM_SHORT_10:
    case TextureFormat::UNORM_SHORT_12:
        return isColor;

    default:
        DE_FATAL("Unknown format");
        return 0u;
    }

    DE_STATIC_ASSERT(TextureFormat::CHANNELTYPE_LAST == 48);
}

int getNumUsedChannels(TextureFormat::ChannelOrder order)
{
    // make sure this table is updated if type table is updated
    DE_STATIC_ASSERT(TextureFormat::CHANNELORDER_LAST == 22);

    switch (order)
    {
    case TextureFormat::R:
        return 1;
    case TextureFormat::A:
        return 1;
    case TextureFormat::I:
        return 1;
    case TextureFormat::L:
        return 1;
    case TextureFormat::LA:
        return 2;
    case TextureFormat::RG:
        return 2;
    case TextureFormat::RA:
        return 2;
    case TextureFormat::RGB:
        return 3;
    case TextureFormat::RGBA:
        return 4;
    case TextureFormat::ARGB:
        return 4;
    case TextureFormat::ABGR:
        return 4;
    case TextureFormat::BGR:
        return 3;
    case TextureFormat::BGRA:
        return 4;
    case TextureFormat::sR:
        return 1;
    case TextureFormat::sRG:
        return 2;
    case TextureFormat::sRGB:
        return 3;
    case TextureFormat::sRGBA:
        return 4;
    case TextureFormat::sBGR:
        return 3;
    case TextureFormat::sBGRA:
        return 4;
    case TextureFormat::D:
        return 1;
    case TextureFormat::S:
        return 1;
    case TextureFormat::DS:
        return 2;
    default:
        DE_ASSERT(false);
        return 0;
    }
}

bool hasAlphaChannel(TextureFormat::ChannelOrder order)
{
    // make sure this table is updated if type table is updated
    DE_STATIC_ASSERT(TextureFormat::CHANNELORDER_LAST == 22);

    switch (order)
    {
    case TextureFormat::A:
    case TextureFormat::LA:
    case TextureFormat::RG:
    case TextureFormat::RA:
    case TextureFormat::RGBA:
    case TextureFormat::ARGB:
    case TextureFormat::ABGR:
    case TextureFormat::BGRA:
    case TextureFormat::sRGBA:
    case TextureFormat::sBGRA:
        return true;
    default:
        return false;
    }
}

int getChannelSize(TextureFormat::ChannelType type)
{
    // make sure this table is updated if format table is updated
    DE_STATIC_ASSERT(TextureFormat::CHANNELTYPE_LAST == 48);

    switch (type)
    {
    case TextureFormat::SNORM_INT8:
        return 1;
    case TextureFormat::SNORM_INT16:
        return 2;
    case TextureFormat::SNORM_INT32:
        return 4;
    case TextureFormat::UNORM_INT8:
        return 1;
    case TextureFormat::UNORM_INT16:
        return 2;
    case TextureFormat::UNORM_INT24:
        return 3;
    case TextureFormat::UNORM_INT32:
        return 4;
    case TextureFormat::SIGNED_INT8:
        return 1;
    case TextureFormat::SIGNED_INT16:
        return 2;
    case TextureFormat::SIGNED_INT32:
        return 4;
    case TextureFormat::SIGNED_INT64:
        return 8;
    case TextureFormat::UNSIGNED_INT8:
        return 1;
    case TextureFormat::UNSIGNED_INT16:
        return 2;
    case TextureFormat::UNSIGNED_INT24:
        return 3;
    case TextureFormat::UNSIGNED_INT32:
        return 4;
    case TextureFormat::UNSIGNED_INT64:
        return 8;
    case TextureFormat::HALF_FLOAT:
        return 2;
    case TextureFormat::FLOAT:
        return 4;
    case TextureFormat::FLOAT64:
        return 8;
    case TextureFormat::UNORM_SHORT_10:
        return 2;
    case TextureFormat::UNORM_SHORT_12:
        return 2;
    case TextureFormat::USCALED_INT8:
        return 1;
    case TextureFormat::USCALED_INT16:
        return 2;
    case TextureFormat::SSCALED_INT8:
        return 1;
    case TextureFormat::SSCALED_INT16:
        return 2;
    // required by modifiers tests
    case TextureFormat::UNSIGNED_INT_11F_11F_10F_REV:
        return 4;
    case TextureFormat::UNORM_INT_1010102_REV:
        return 4;
    case TextureFormat::SNORM_INT_1010102_REV:
        return 4;
    case TextureFormat::USCALED_INT_1010102_REV:
        return 4;
    case TextureFormat::SSCALED_INT_1010102_REV:
        return 4;
    case TextureFormat::UNSIGNED_INT_1010102_REV:
        return 4;
    case TextureFormat::SIGNED_INT_1010102_REV:
        return 4;
    case TextureFormat::UNSIGNED_INT_999_E5_REV:
        return 4;
    default:
        DE_ASSERT(false);
        return 0;
    }
}

/** Get pixel size in bytes. */
int getPixelSize(TextureFormat format)
{
    const TextureFormat::ChannelOrder order = format.order;
    const TextureFormat::ChannelType type   = format.type;

    DE_ASSERT(isValid(format));

    // make sure this table is updated if format table is updated
    DE_STATIC_ASSERT(TextureFormat::CHANNELTYPE_LAST == 48);

    switch (type)
    {
    case TextureFormat::UNORM_BYTE_44:
    case TextureFormat::UNSIGNED_BYTE_44:
        return 1;

    case TextureFormat::UNORM_SHORT_565:
    case TextureFormat::UNORM_SHORT_555:
    case TextureFormat::UNORM_SHORT_4444:
    case TextureFormat::UNORM_SHORT_5551:
    case TextureFormat::UNORM_SHORT_1555:
    case TextureFormat::UNSIGNED_SHORT_565:
    case TextureFormat::UNSIGNED_SHORT_4444:
    case TextureFormat::UNSIGNED_SHORT_5551:
        return 2;

    case TextureFormat::UNORM_INT_101010:
    case TextureFormat::UNSIGNED_INT_999_E5_REV:
    case TextureFormat::UNSIGNED_INT_11F_11F_10F_REV:
    case TextureFormat::SNORM_INT_1010102_REV:
    case TextureFormat::UNORM_INT_1010102_REV:
    case TextureFormat::SIGNED_INT_1010102_REV:
    case TextureFormat::UNSIGNED_INT_1010102_REV:
    case TextureFormat::UNSIGNED_INT_24_8:
    case TextureFormat::UNSIGNED_INT_24_8_REV:
    case TextureFormat::UNSIGNED_INT_16_8_8:
    case TextureFormat::USCALED_INT_1010102_REV:
    case TextureFormat::SSCALED_INT_1010102_REV:
        return 4;

    case TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV:
        return 8;

    default:
        return getNumUsedChannels(order) * getChannelSize(type);
    }
}

int TextureFormat::getPixelSize(void) const
{
    return ::tcu::getPixelSize(*this);
}

const TextureSwizzle &getChannelReadSwizzle(TextureFormat::ChannelOrder order)
{
    // make sure to update these tables when channel orders are updated
    DE_STATIC_ASSERT(TextureFormat::CHANNELORDER_LAST == 22);

    static const TextureSwizzle INV = {{TextureSwizzle::CHANNEL_ZERO, TextureSwizzle::CHANNEL_ZERO,
                                        TextureSwizzle::CHANNEL_ZERO, TextureSwizzle::CHANNEL_ONE}};
    static const TextureSwizzle R   = {{TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_ZERO,
                                        TextureSwizzle::CHANNEL_ZERO, TextureSwizzle::CHANNEL_ONE}};
    static const TextureSwizzle A   = {{TextureSwizzle::CHANNEL_ZERO, TextureSwizzle::CHANNEL_ZERO,
                                        TextureSwizzle::CHANNEL_ZERO, TextureSwizzle::CHANNEL_0}};
    static const TextureSwizzle I   = {
        {TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_0}};
    static const TextureSwizzle L = {
        {TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_ONE}};
    static const TextureSwizzle LA = {
        {TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_1}};
    static const TextureSwizzle RG  = {{TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_1,
                                        TextureSwizzle::CHANNEL_ZERO, TextureSwizzle::CHANNEL_ONE}};
    static const TextureSwizzle RA  = {{TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_ZERO,
                                        TextureSwizzle::CHANNEL_ZERO, TextureSwizzle::CHANNEL_1}};
    static const TextureSwizzle RGB = {
        {TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_1, TextureSwizzle::CHANNEL_2, TextureSwizzle::CHANNEL_ONE}};
    static const TextureSwizzle RGBA = {
        {TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_1, TextureSwizzle::CHANNEL_2, TextureSwizzle::CHANNEL_3}};
    static const TextureSwizzle BGR = {
        {TextureSwizzle::CHANNEL_2, TextureSwizzle::CHANNEL_1, TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_ONE}};
    static const TextureSwizzle BGRA = {
        {TextureSwizzle::CHANNEL_2, TextureSwizzle::CHANNEL_1, TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_3}};
    static const TextureSwizzle ARGB = {
        {TextureSwizzle::CHANNEL_1, TextureSwizzle::CHANNEL_2, TextureSwizzle::CHANNEL_3, TextureSwizzle::CHANNEL_0}};
    static const TextureSwizzle ABGR = {
        {TextureSwizzle::CHANNEL_3, TextureSwizzle::CHANNEL_2, TextureSwizzle::CHANNEL_1, TextureSwizzle::CHANNEL_0}};
    static const TextureSwizzle D  = {{TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_ZERO,
                                       TextureSwizzle::CHANNEL_ZERO, TextureSwizzle::CHANNEL_ONE}};
    static const TextureSwizzle S  = {{TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_ZERO,
                                       TextureSwizzle::CHANNEL_ZERO, TextureSwizzle::CHANNEL_ONE}};
    static const TextureSwizzle DS = {{TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_1,
                                       TextureSwizzle::CHANNEL_ZERO, TextureSwizzle::CHANNEL_ONE}};

    switch (order)
    {
    case TextureFormat::R:
        return R;
    case TextureFormat::A:
        return A;
    case TextureFormat::I:
        return I;
    case TextureFormat::L:
        return L;
    case TextureFormat::LA:
        return LA;
    case TextureFormat::RG:
        return RG;
    case TextureFormat::RA:
        return RA;
    case TextureFormat::RGB:
        return RGB;
    case TextureFormat::RGBA:
        return RGBA;
    case TextureFormat::ARGB:
        return ARGB;
    case TextureFormat::ABGR:
        return ABGR;
    case TextureFormat::BGR:
        return BGR;
    case TextureFormat::BGRA:
        return BGRA;
    case TextureFormat::sR:
        return R;
    case TextureFormat::sRG:
        return RG;
    case TextureFormat::sRGB:
        return RGB;
    case TextureFormat::sRGBA:
        return RGBA;
    case TextureFormat::sBGR:
        return BGR;
    case TextureFormat::sBGRA:
        return BGRA;
    case TextureFormat::D:
        return D;
    case TextureFormat::S:
        return S;
    case TextureFormat::DS:
        return DS;

    default:
        DE_ASSERT(false);
        return INV;
    }
}

const TextureSwizzle &getChannelWriteSwizzle(TextureFormat::ChannelOrder order)
{
    // make sure to update these tables when channel orders are updated
    DE_STATIC_ASSERT(TextureFormat::CHANNELORDER_LAST == 22);

    static const TextureSwizzle INV = {{TextureSwizzle::CHANNEL_LAST, TextureSwizzle::CHANNEL_LAST,
                                        TextureSwizzle::CHANNEL_LAST, TextureSwizzle::CHANNEL_LAST}};
    static const TextureSwizzle R   = {{TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_LAST,
                                        TextureSwizzle::CHANNEL_LAST, TextureSwizzle::CHANNEL_LAST}};
    static const TextureSwizzle A   = {{TextureSwizzle::CHANNEL_3, TextureSwizzle::CHANNEL_LAST,
                                        TextureSwizzle::CHANNEL_LAST, TextureSwizzle::CHANNEL_LAST}};
    static const TextureSwizzle I   = {{TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_LAST,
                                        TextureSwizzle::CHANNEL_LAST, TextureSwizzle::CHANNEL_LAST}};
    static const TextureSwizzle L   = {{TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_LAST,
                                        TextureSwizzle::CHANNEL_LAST, TextureSwizzle::CHANNEL_LAST}};
    static const TextureSwizzle LA  = {{TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_3,
                                        TextureSwizzle::CHANNEL_LAST, TextureSwizzle::CHANNEL_LAST}};
    static const TextureSwizzle RG  = {{TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_1,
                                        TextureSwizzle::CHANNEL_LAST, TextureSwizzle::CHANNEL_LAST}};
    static const TextureSwizzle RA  = {{TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_3,
                                        TextureSwizzle::CHANNEL_LAST, TextureSwizzle::CHANNEL_LAST}};
    static const TextureSwizzle RGB = {{TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_1, TextureSwizzle::CHANNEL_2,
                                        TextureSwizzle::CHANNEL_LAST}};
    static const TextureSwizzle RGBA = {
        {TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_1, TextureSwizzle::CHANNEL_2, TextureSwizzle::CHANNEL_3}};
    static const TextureSwizzle BGR = {{TextureSwizzle::CHANNEL_2, TextureSwizzle::CHANNEL_1, TextureSwizzle::CHANNEL_0,
                                        TextureSwizzle::CHANNEL_LAST}};
    static const TextureSwizzle BGRA = {
        {TextureSwizzle::CHANNEL_2, TextureSwizzle::CHANNEL_1, TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_3}};
    static const TextureSwizzle ARGB = {
        {TextureSwizzle::CHANNEL_3, TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_1, TextureSwizzle::CHANNEL_2}};
    static const TextureSwizzle ABGR = {
        {TextureSwizzle::CHANNEL_3, TextureSwizzle::CHANNEL_2, TextureSwizzle::CHANNEL_1, TextureSwizzle::CHANNEL_0}};
    static const TextureSwizzle D = {{TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_LAST,
                                      TextureSwizzle::CHANNEL_LAST, TextureSwizzle::CHANNEL_LAST}};
    static const TextureSwizzle S = {{TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_LAST,
                                      TextureSwizzle::CHANNEL_LAST, TextureSwizzle::CHANNEL_LAST}};

    switch (order)
    {
    case TextureFormat::R:
        return R;
    case TextureFormat::A:
        return A;
    case TextureFormat::I:
        return I;
    case TextureFormat::L:
        return L;
    case TextureFormat::LA:
        return LA;
    case TextureFormat::RG:
        return RG;
    case TextureFormat::RA:
        return RA;
    case TextureFormat::RGB:
        return RGB;
    case TextureFormat::RGBA:
        return RGBA;
    case TextureFormat::ARGB:
        return ARGB;
    case TextureFormat::ABGR:
        return ABGR;
    case TextureFormat::BGR:
        return BGR;
    case TextureFormat::BGRA:
        return BGRA;
    case TextureFormat::sR:
        return R;
    case TextureFormat::sRG:
        return RG;
    case TextureFormat::sRGB:
        return RGB;
    case TextureFormat::sRGBA:
        return RGBA;
    case TextureFormat::sBGR:
        return BGR;
    case TextureFormat::sBGRA:
        return BGRA;
    case TextureFormat::D:
        return D;
    case TextureFormat::S:
        return S;

    case TextureFormat::DS:
        DE_ASSERT(false); // combined formats cannot be written to
        return INV;

    default:
        DE_ASSERT(false);
        return INV;
    }
}

IVec3 calculatePackedPitch(const TextureFormat &format, const IVec3 &size)
{
    const int pixelSize  = format.getPixelSize();
    const int rowPitch   = pixelSize * size.x();
    const int slicePitch = rowPitch * size.y();

    return IVec3(pixelSize, rowPitch, slicePitch);
}

ConstPixelBufferAccess::ConstPixelBufferAccess(void) : m_size(0), m_pitch(0), m_divider(1, 1, 1), m_data(nullptr)
{
}

ConstPixelBufferAccess::ConstPixelBufferAccess(const TextureFormat &format, int width, int height, int depth,
                                               const void *data)
    : m_format(format)
    , m_size(width, height, depth)
    , m_pitch(calculatePackedPitch(m_format, m_size))
    , m_divider(1, 1, 1)
    , m_data((void *)data)
{
    DE_ASSERT(isValid(format));
}

ConstPixelBufferAccess::ConstPixelBufferAccess(const TextureFormat &format, const IVec3 &size, const void *data)
    : m_format(format)
    , m_size(size)
    , m_pitch(calculatePackedPitch(m_format, m_size))
    , m_divider(1, 1, 1)
    , m_data((void *)data)
{
    DE_ASSERT(isValid(format));
}

ConstPixelBufferAccess::ConstPixelBufferAccess(const TextureFormat &format, int width, int height, int depth,
                                               int rowPitch, int slicePitch, const void *data)
    : m_format(format)
    , m_size(width, height, depth)
    , m_pitch(format.getPixelSize(), rowPitch, slicePitch)
    , m_divider(1, 1, 1)
    , m_data((void *)data)
{
    DE_ASSERT(isValid(format));
}

ConstPixelBufferAccess::ConstPixelBufferAccess(const TextureFormat &format, const IVec3 &size, const IVec3 &pitch,
                                               const void *data)
    : m_format(format)
    , m_size(size)
    , m_pitch(pitch)
    , m_divider(1, 1, 1)
    , m_data((void *)data)
{
    DE_ASSERT(isValid(format));
    //DE_ASSERT(m_format.getPixelSize() <= m_pitch.x());
}

ConstPixelBufferAccess::ConstPixelBufferAccess(const TextureFormat &format, const IVec3 &size, const IVec3 &pitch,
                                               const IVec3 &block, const void *data)
    : m_format(format)
    , m_size(size)
    , m_pitch(pitch)
    , m_divider(block)
    , m_data((void *)data)
{
    DE_ASSERT(isValid(format));
    //DE_ASSERT(m_format.getPixelSize() <= m_pitch.x());
}

ConstPixelBufferAccess::ConstPixelBufferAccess(const TextureLevel &level)
    : m_format(level.getFormat())
    , m_size(level.getSize())
    , m_pitch(calculatePackedPitch(m_format, m_size))
    , m_divider(1, 1, 1)
    , m_data((void *)level.getPtr())
{
}

PixelBufferAccess::PixelBufferAccess(const TextureFormat &format, int width, int height, int depth, void *data)
    : ConstPixelBufferAccess(format, width, height, depth, data)
{
}

PixelBufferAccess::PixelBufferAccess(const TextureFormat &format, const IVec3 &size, void *data)
    : ConstPixelBufferAccess(format, size, data)
{
}

PixelBufferAccess::PixelBufferAccess(const TextureFormat &format, int width, int height, int depth, int rowPitch,
                                     int slicePitch, void *data)
    : ConstPixelBufferAccess(format, width, height, depth, rowPitch, slicePitch, data)
{
}

PixelBufferAccess::PixelBufferAccess(const TextureFormat &format, const IVec3 &size, const IVec3 &pitch, void *data)
    : ConstPixelBufferAccess(format, size, pitch, data)
{
}

PixelBufferAccess::PixelBufferAccess(const TextureFormat &format, const IVec3 &size, const IVec3 &pitch,
                                     const IVec3 &block, void *data)
    : ConstPixelBufferAccess(format, size, pitch, block, data)
{
}

PixelBufferAccess::PixelBufferAccess(TextureLevel &level) : ConstPixelBufferAccess(level)
{
}

//! Swizzle generally based on channel order.
template <typename T>
Vector<T, 4> swizzleGe(const Vector<T, 4> &v, TextureFormat::ChannelOrder src, TextureFormat::ChannelOrder dst)
{
    if (src == dst)
        return v;
    else
    {
        if ((src == TextureFormat::RGBA && dst == TextureFormat::ARGB) ||
            (src == TextureFormat::BGRA && dst == TextureFormat::ABGR))
            return v.swizzle(3, 0, 1, 2);

        if ((src == TextureFormat::ARGB && dst == TextureFormat::RGBA) ||
            (src == TextureFormat::ABGR && dst == TextureFormat::BGRA))
            return v.swizzle(1, 2, 3, 0);

        if ((src == TextureFormat::BGRA && dst == TextureFormat::ARGB) ||
            (src == TextureFormat::ABGR && dst == TextureFormat::RGBA) ||
            (src == TextureFormat::RGBA && dst == TextureFormat::ABGR) ||
            (src == TextureFormat::ARGB && dst == TextureFormat::BGRA))
            return v.swizzle(3, 2, 1, 0);

        if ((src == TextureFormat::RGB && dst == TextureFormat::BGR) ||
            (src == TextureFormat::BGR && dst == TextureFormat::RGB) ||
            (src == TextureFormat::RGBA && dst == TextureFormat::BGRA) ||
            (src == TextureFormat::BGRA && dst == TextureFormat::RGBA))
            return v.swizzle(2, 1, 0, 3);

        DE_ASSERT(false);
        return v;
    }
}

Vec4 ConstPixelBufferAccess::getPixel(int x, int y, int z) const
{
    DE_ASSERT(de::inBounds(x, 0, m_size.x()));
    DE_ASSERT(de::inBounds(y, 0, m_size.y()));
    DE_ASSERT(de::inBounds(z, 0, m_size.z()));
    DE_ASSERT(!isCombinedDepthStencilType(m_format.type)); // combined types cannot be accessed directly
    DE_ASSERT(m_format.order != TextureFormat::DS);        // combined formats cannot be accessed directly

    const uint8_t *pixelPtr = (const uint8_t *)getPixelPtr(x, y, z);

    // Optimized fomats.
    if (m_format.type == TextureFormat::UNORM_INT8)
    {
        if (m_format.order == TextureFormat::RGBA || m_format.order == TextureFormat::sRGBA)
            return readRGBA8888Float(pixelPtr);
        else if (m_format.order == TextureFormat::RGB || m_format.order == TextureFormat::sRGB)
            return readRGB888Float(pixelPtr);
    }

#define UI8(OFFS, COUNT) ((*((const uint8_t *)pixelPtr) >> (OFFS)) & ((1 << (COUNT)) - 1))
#define UI16(OFFS, COUNT) ((readUnaligned<uint16_t>(pixelPtr) >> (OFFS)) & ((1 << (COUNT)) - 1))
#define UI32(OFFS, COUNT) ((readUnaligned<uint32_t>(pixelPtr) >> (OFFS)) & ((1 << (COUNT)) - 1))
#define SI32(OFFS, COUNT) signExtend(UI32(OFFS, COUNT), (COUNT))
#define UN8(OFFS, COUNT) channelToUnormFloat(UI8(OFFS, COUNT), (COUNT))
#define UN16(OFFS, COUNT) channelToUnormFloat(UI16(OFFS, COUNT), (COUNT))
#define UN32(OFFS, COUNT) channelToUnormFloat(UI32(OFFS, COUNT), (COUNT))
#define SN32(OFFS, COUNT) channelToSnormFloat(UI32(OFFS, COUNT), (COUNT))

    // Packed formats.
    switch (m_format.type)
    {
    case TextureFormat::UNORM_BYTE_44:
        return Vec4(UN8(4, 4), UN8(0, 4), 0.0f, 1.0f);
    case TextureFormat::UNSIGNED_BYTE_44:
        return UVec4(UI8(4, 4), UI8(0, 4), 0u, 1u).cast<float>();
    case TextureFormat::UNORM_SHORT_565:
        return swizzleGe(Vec4(UN16(11, 5), UN16(5, 6), UN16(0, 5), 1.0f), m_format.order, TextureFormat::RGB);
    case TextureFormat::UNSIGNED_SHORT_565:
        return swizzleGe(UVec4(UI16(11, 5), UI16(5, 6), UI16(0, 5), 1u), m_format.order, TextureFormat::RGB)
            .cast<float>();
    case TextureFormat::UNORM_SHORT_555:
        return swizzleGe(Vec4(UN16(10, 5), UN16(5, 5), UN16(0, 5), 1.0f), m_format.order, TextureFormat::RGB);
    case TextureFormat::UNORM_SHORT_4444:
        return swizzleGe(Vec4(UN16(12, 4), UN16(8, 4), UN16(4, 4), UN16(0, 4)), m_format.order, TextureFormat::RGBA);
    case TextureFormat::UNSIGNED_SHORT_4444:
        return swizzleGe(UVec4(UI16(12, 4), UI16(8, 4), UI16(4, 4), UI16(0, 4)), m_format.order, TextureFormat::RGBA)
            .cast<float>();
    case TextureFormat::UNORM_SHORT_5551:
        return swizzleGe(Vec4(UN16(11, 5), UN16(6, 5), UN16(1, 5), UN16(0, 1)), m_format.order, TextureFormat::RGBA);
    case TextureFormat::UNSIGNED_SHORT_5551:
        return swizzleGe(UVec4(UI16(11, 5), UI16(6, 5), UI16(1, 5), UI16(0, 1)), m_format.order, TextureFormat::RGBA)
            .cast<float>();
    case TextureFormat::UNORM_SHORT_1555:
        return swizzleGe(Vec4(UN16(15, 1), UN16(10, 5), UN16(5, 5), UN16(0, 5)), m_format.order, TextureFormat::RGBA);
    case TextureFormat::UNORM_INT_101010:
        return Vec4(UN32(22, 10), UN32(12, 10), UN32(2, 10), 1.0f);
    case TextureFormat::UNORM_INT_1010102_REV:
        return swizzleGe(Vec4(UN32(0, 10), UN32(10, 10), UN32(20, 10), UN32(30, 2)), m_format.order,
                         TextureFormat::RGBA);
    case TextureFormat::SNORM_INT_1010102_REV:
        return swizzleGe(Vec4(SN32(0, 10), SN32(10, 10), SN32(20, 10), SN32(30, 2)), m_format.order,
                         TextureFormat::RGBA);
    case TextureFormat::USCALED_INT_1010102_REV:
    case TextureFormat::UNSIGNED_INT_1010102_REV:
        return swizzleGe(UVec4(UI32(0, 10), UI32(10, 10), UI32(20, 10), UI32(30, 2)), m_format.order,
                         TextureFormat::RGBA)
            .cast<float>();
    case TextureFormat::SSCALED_INT_1010102_REV:
        return swizzleGe(UVec4(SI32(0, 10), SI32(10, 10), SI32(20, 10), SI32(30, 2)), m_format.order,
                         TextureFormat::RGBA)
            .cast<float>();
    case TextureFormat::SIGNED_INT_1010102_REV:
        return swizzleGe(IVec4(SI32(0, 10), SI32(10, 10), SI32(20, 10), SI32(30, 2)), m_format.order,
                         TextureFormat::RGBA)
            .cast<float>();
    case TextureFormat::UNSIGNED_INT_999_E5_REV:
        return unpackRGB999E5(readUnaligned<uint32_t>(pixelPtr));

    case TextureFormat::UNSIGNED_INT_11F_11F_10F_REV:
        return Vec4(Float11(UI32(0, 11)).asFloat(), Float11(UI32(11, 11)).asFloat(), Float10(UI32(22, 10)).asFloat(),
                    1.0f);

    default:
        break;
    }

#undef UN8
#undef UN16
#undef UN32
#undef SN32
#undef SI32
#undef UI8
#undef UI16
#undef UI32

    // Generic path.
    Vec4 result;
    const TextureSwizzle::Channel *channelMap = getChannelReadSwizzle(m_format.order).components;
    int channelSize                           = getChannelSize(m_format.type);

    for (int c = 0; c < 4; c++)
    {
        switch (channelMap[c])
        {
        case TextureSwizzle::CHANNEL_0:
        case TextureSwizzle::CHANNEL_1:
        case TextureSwizzle::CHANNEL_2:
        case TextureSwizzle::CHANNEL_3:
            result[c] = channelToFloat(pixelPtr + channelSize * ((int)channelMap[c]), m_format.type);
            break;

        case TextureSwizzle::CHANNEL_ZERO:
            result[c] = 0.0f;
            break;

        case TextureSwizzle::CHANNEL_ONE:
            result[c] = 1.0f;
            break;

        default:
            DE_ASSERT(false);
        }
    }

    return result;
}

template <typename T>
static tcu::Vector<T, 4> getPixelIntGeneric(const uint8_t *pixelPtr, const tcu::TextureFormat &format)
{
    tcu::Vector<T, 4> result;

    // Generic path.
    const TextureSwizzle::Channel *channelMap = getChannelReadSwizzle(format.order).components;
    int channelSize                           = getChannelSize(format.type);

    for (int c = 0; c < 4; c++)
    {
        switch (channelMap[c])
        {
        case TextureSwizzle::CHANNEL_0:
        case TextureSwizzle::CHANNEL_1:
        case TextureSwizzle::CHANNEL_2:
        case TextureSwizzle::CHANNEL_3:
            result[c] = channelToIntType<T>(pixelPtr + channelSize * ((int)channelMap[c]), format.type);
            break;

        case TextureSwizzle::CHANNEL_ZERO:
            result[c] = 0;
            break;

        case TextureSwizzle::CHANNEL_ONE:
            result[c] = 1;
            break;

        default:
            DE_ASSERT(false);
        }
    }

    return result;
}

static U64Vec4 getPixelAsBitsUint64(const uint8_t *pixelPtr, const tcu::TextureFormat &format)
{
    U64Vec4 result;

    // Generic path.
    const TextureSwizzle::Channel *channelMap = getChannelReadSwizzle(format.order).components;
    int channelSize                           = getChannelSize(format.type);

    for (int c = 0; c < 4; c++)
    {
        switch (channelMap[c])
        {
        case TextureSwizzle::CHANNEL_0:
        case TextureSwizzle::CHANNEL_1:
        case TextureSwizzle::CHANNEL_2:
        case TextureSwizzle::CHANNEL_3:
            result[c] = retrieveChannelBitsAsUint64(pixelPtr + channelSize * ((int)channelMap[c]), format.type);
            break;

        case TextureSwizzle::CHANNEL_ZERO:
            result[c] = 0;
            break;

        case TextureSwizzle::CHANNEL_ONE:
            result[c] = 1;
            break;

        default:
            DE_ASSERT(false);
        }
    }

    return result;
}

IVec4 ConstPixelBufferAccess::getPixelInt(int x, int y, int z) const
{
    DE_ASSERT(de::inBounds(x, 0, m_size.x()));
    DE_ASSERT(de::inBounds(y, 0, m_size.y()));
    DE_ASSERT(de::inBounds(z, 0, m_size.z()));
    DE_ASSERT(!isCombinedDepthStencilType(m_format.type)); // combined types cannot be accessed directly
    DE_ASSERT(m_format.order != TextureFormat::DS);        // combined formats cannot be accessed directly

    const uint8_t *const pixelPtr = (const uint8_t *)getPixelPtr(x, y, z);

    // Optimized fomats.
    if (m_format.type == TextureFormat::UNORM_INT8)
    {
        if (m_format.order == TextureFormat::RGBA || m_format.order == TextureFormat::sRGBA)
            return readRGBA8888Int(pixelPtr);
        else if (m_format.order == TextureFormat::RGB || m_format.order == TextureFormat::sRGB)
            return readRGB888Int(pixelPtr);
    }

#define U8(OFFS, COUNT) ((*((const uint8_t *)pixelPtr) >> (OFFS)) & ((1 << (COUNT)) - 1))
#define U16(OFFS, COUNT) ((readUnaligned<uint16_t>(pixelPtr) >> (OFFS)) & ((1 << (COUNT)) - 1))
#define U32(OFFS, COUNT) ((readUnaligned<uint32_t>(pixelPtr) >> (OFFS)) & ((1 << (COUNT)) - 1))
#define S32(OFFS, COUNT) signExtend(U32(OFFS, COUNT), (COUNT))

    switch (m_format.type)
    {
    case TextureFormat::UNSIGNED_BYTE_44: // Fall-through
    case TextureFormat::UNORM_BYTE_44:
        return UVec4(U8(4, 4), U8(0, 4), 0u, 1u).cast<int>();
    case TextureFormat::UNSIGNED_SHORT_565: // Fall-through
    case TextureFormat::UNORM_SHORT_565:
        return swizzleGe(UVec4(U16(11, 5), U16(5, 6), U16(0, 5), 1).cast<int>(), m_format.order, TextureFormat::RGB);
    case TextureFormat::UNORM_SHORT_555:
        return swizzleGe(UVec4(U16(10, 5), U16(5, 5), U16(0, 5), 1).cast<int>(), m_format.order, TextureFormat::RGB);
    case TextureFormat::UNSIGNED_SHORT_4444: // Fall-through
    case TextureFormat::UNORM_SHORT_4444:
        return swizzleGe(UVec4(U16(12, 4), U16(8, 4), U16(4, 4), U16(0, 4)).cast<int>(), m_format.order,
                         TextureFormat::RGBA);
    case TextureFormat::UNSIGNED_SHORT_5551: // Fall-through
    case TextureFormat::UNORM_SHORT_5551:
        return swizzleGe(UVec4(U16(11, 5), U16(6, 5), U16(1, 5), U16(0, 1)).cast<int>(), m_format.order,
                         TextureFormat::RGBA);
    case TextureFormat::UNORM_SHORT_1555:
        return swizzleGe(UVec4(U16(15, 1), U16(10, 5), U16(5, 5), U16(0, 5)).cast<int>(), m_format.order,
                         TextureFormat::RGBA);
    case TextureFormat::UNORM_INT_101010:
        return UVec4(U32(22, 10), U32(12, 10), U32(2, 10), 1).cast<int>();
    case TextureFormat::UNORM_INT_1010102_REV:   // Fall-through
    case TextureFormat::USCALED_INT_1010102_REV: // Fall-through
    case TextureFormat::UNSIGNED_INT_1010102_REV:
        return swizzleGe(UVec4(U32(0, 10), U32(10, 10), U32(20, 10), U32(30, 2)), m_format.order, TextureFormat::RGBA)
            .cast<int>();
    case TextureFormat::SNORM_INT_1010102_REV:   // Fall-through
    case TextureFormat::SSCALED_INT_1010102_REV: // Fall-through
    case TextureFormat::SIGNED_INT_1010102_REV:
        return swizzleGe(IVec4(S32(0, 10), S32(10, 10), S32(20, 10), S32(30, 2)), m_format.order, TextureFormat::RGBA);

    default:
        break; // To generic path.
    }

#undef U8
#undef U16
#undef U32
#undef S32

    // Generic path.
    return getPixelIntGeneric<int>(pixelPtr, m_format);
}

I64Vec4 ConstPixelBufferAccess::getPixelInt64(int x, int y, int z) const
{
    // Rely on getPixelInt() for some formats.
    if (m_format.type == TextureFormat::UNORM_INT8 &&
        (m_format.order == TextureFormat::RGBA || m_format.order == TextureFormat::sRGBA ||
         m_format.order == TextureFormat::RGB || m_format.order == TextureFormat::sRGB))
    {
        return getPixelInt(x, y, z).cast<int64_t>();
    }

    switch (m_format.type)
    {
    case TextureFormat::UNSIGNED_BYTE_44:
    case TextureFormat::UNORM_BYTE_44:
    case TextureFormat::UNSIGNED_SHORT_565:
    case TextureFormat::UNORM_SHORT_565:
    case TextureFormat::UNORM_SHORT_555:
    case TextureFormat::UNSIGNED_SHORT_4444:
    case TextureFormat::UNORM_SHORT_4444:
    case TextureFormat::UNSIGNED_SHORT_5551:
    case TextureFormat::UNORM_SHORT_5551:
    case TextureFormat::UNORM_INT_101010:
    case TextureFormat::UNORM_INT_1010102_REV:
    case TextureFormat::USCALED_INT_1010102_REV:
    case TextureFormat::UNSIGNED_INT_1010102_REV:
    case TextureFormat::SNORM_INT_1010102_REV:
    case TextureFormat::SSCALED_INT_1010102_REV:
    case TextureFormat::SIGNED_INT_1010102_REV:
    case TextureFormat::UNORM_SHORT_1555:
        return getPixelInt(x, y, z).cast<int64_t>();

    default:
        break; // To generic path.
    }

    // Generic path.
    auto pixelPtr = reinterpret_cast<const uint8_t *>(getPixelPtr(x, y, z));
    return getPixelIntGeneric<int64_t>(pixelPtr, m_format);
}

U64Vec4 ConstPixelBufferAccess::getPixelBitsAsUint64(int x, int y, int z) const
{
    DE_ASSERT(de::inBounds(x, 0, m_size.x()));
    DE_ASSERT(de::inBounds(y, 0, m_size.y()));
    DE_ASSERT(de::inBounds(z, 0, m_size.z()));
    DE_ASSERT(!isCombinedDepthStencilType(m_format.type)); // combined types cannot be accessed directly
    DE_ASSERT(m_format.order != TextureFormat::DS);        // combined formats cannot be accessed directly

    const uint8_t *const pixelPtr = (const uint8_t *)getPixelPtr(x, y, z);

    if (m_format.type == TextureFormat::UNORM_INT8)
    {
        if (m_format.order == TextureFormat::RGBA || m_format.order == TextureFormat::sRGBA)
            return U64Vec4(pixelPtr[0], pixelPtr[1], pixelPtr[2], pixelPtr[3]);
        else if (m_format.order == TextureFormat::RGB || m_format.order == TextureFormat::sRGB)
            return U64Vec4(pixelPtr[0], pixelPtr[1], pixelPtr[2], 1);
    }

#define U8(OFFS, COUNT) ((*((const uint8_t *)pixelPtr) >> (OFFS)) & ((1 << (COUNT)) - 1))
#define U16(OFFS, COUNT) ((readUnaligned<uint16_t>(pixelPtr) >> (OFFS)) & ((1 << (COUNT)) - 1))
#define U32(OFFS, COUNT) ((readUnaligned<uint32_t>(pixelPtr) >> (OFFS)) & ((1 << (COUNT)) - 1))

    switch (m_format.type)
    {
    case TextureFormat::UNSIGNED_BYTE_44: // Fall-through
    case TextureFormat::UNORM_BYTE_44:
        return U64Vec4(U8(4, 4), U8(0, 4), 0u, 1u);
    case TextureFormat::UNSIGNED_SHORT_565: // Fall-through
    case TextureFormat::UNORM_SHORT_565:
        return swizzleGe(U64Vec4(U16(11, 5), U16(5, 6), U16(0, 5), 1), m_format.order, TextureFormat::RGB);
    case TextureFormat::UNORM_SHORT_555:
        return swizzleGe(U64Vec4(U16(10, 5), U16(5, 5), U16(0, 5), 1), m_format.order, TextureFormat::RGB);
    case TextureFormat::UNSIGNED_SHORT_4444: // Fall-through
    case TextureFormat::UNORM_SHORT_4444:
        return swizzleGe(U64Vec4(U16(12, 4), U16(8, 4), U16(4, 4), U16(0, 4)), m_format.order, TextureFormat::RGBA);
    case TextureFormat::UNSIGNED_SHORT_5551: // Fall-through
    case TextureFormat::UNORM_SHORT_5551:
        return swizzleGe(U64Vec4(U16(11, 5), U16(6, 5), U16(1, 5), U16(0, 1)), m_format.order, TextureFormat::RGBA);
    case TextureFormat::UNORM_INT_101010:
        return U64Vec4(U32(22, 10), U32(12, 10), U32(2, 10), 1);
    case TextureFormat::UNORM_INT_1010102_REV:   // Fall-through
    case TextureFormat::USCALED_INT_1010102_REV: // Fall-through
    case TextureFormat::UNSIGNED_INT_1010102_REV:
        return swizzleGe(U64Vec4(U32(0, 10), U32(10, 10), U32(20, 10), U32(30, 2)), m_format.order,
                         TextureFormat::RGBA);
    case TextureFormat::UNSIGNED_INT_11F_11F_10F_REV:
        return swizzleGe(U64Vec4(U32(0, 11), U32(11, 11), U32(22, 10), 1), m_format.order, TextureFormat::RGB);
    case TextureFormat::SNORM_INT_1010102_REV:   // Fall-through
    case TextureFormat::SSCALED_INT_1010102_REV: // Fall-through
    case TextureFormat::SIGNED_INT_1010102_REV:
        return swizzleGe(U64Vec4(U32(0, 10), U32(10, 10), U32(20, 10), U32(30, 2)), m_format.order,
                         TextureFormat::RGBA);
    case TextureFormat::UNORM_SHORT_1555:
        return swizzleGe(U64Vec4(U16(15, 1), U16(10, 5), U16(5, 5), U16(0, 5)), m_format.order, TextureFormat::RGBA);
    case TextureFormat::UNSIGNED_INT_999_E5_REV:
        // shared exponent in bits 27..31
        return swizzleGe(U64Vec4(U32(0, 9), U32(9, 9), U32(18, 9), 1), m_format.order, TextureFormat::RGB);
    default:
        break; // To generic path.
    }

#undef U8
#undef U16
#undef U32

    // Generic path.
    return getPixelAsBitsUint64(pixelPtr, m_format);
}

template <>
Vec4 ConstPixelBufferAccess::getPixelT(int x, int y, int z) const
{
    return getPixel(x, y, z);
}

template <>
IVec4 ConstPixelBufferAccess::getPixelT(int x, int y, int z) const
{
    return getPixelInt(x, y, z);
}

template <>
UVec4 ConstPixelBufferAccess::getPixelT(int x, int y, int z) const
{
    return getPixelUint(x, y, z);
}

template <>
I64Vec4 ConstPixelBufferAccess::getPixelT(int x, int y, int z) const
{
    return getPixelInt64(x, y, z);
}

template <>
U64Vec4 ConstPixelBufferAccess::getPixelT(int x, int y, int z) const
{
    return getPixelUint64(x, y, z);
}

float ConstPixelBufferAccess::getPixDepth(int x, int y, int z) const
{
    DE_ASSERT(de::inBounds(x, 0, getWidth()));
    DE_ASSERT(de::inBounds(y, 0, getHeight()));
    DE_ASSERT(de::inBounds(z, 0, getDepth()));

    const uint8_t *const pixelPtr = (const uint8_t *)getPixelPtr(x, y, z);

    switch (m_format.type)
    {
    case TextureFormat::UNSIGNED_INT_16_8_8:
        DE_ASSERT(m_format.order == TextureFormat::DS);
        return (float)readUint32High16(pixelPtr) / 65535.0f;

    case TextureFormat::UNSIGNED_INT_24_8:
        DE_ASSERT(m_format.order == TextureFormat::D || m_format.order == TextureFormat::DS);
        return (float)readUint32High24(pixelPtr) / 16777215.0f;

    case TextureFormat::UNSIGNED_INT_24_8_REV:
        DE_ASSERT(m_format.order == TextureFormat::D || m_format.order == TextureFormat::DS);
        return (float)readUint32Low24(pixelPtr) / 16777215.0f;

    case TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV:
        DE_ASSERT(m_format.order == TextureFormat::DS);
        return readUnaligned<float>(pixelPtr);

    default:
        DE_ASSERT(m_format.order == TextureFormat::D); // no other combined depth stencil types
        return channelToFloat(pixelPtr, m_format.type);
    }
}

int ConstPixelBufferAccess::getPixStencil(int x, int y, int z) const
{
    DE_ASSERT(de::inBounds(x, 0, getWidth()));
    DE_ASSERT(de::inBounds(y, 0, getHeight()));
    DE_ASSERT(de::inBounds(z, 0, getDepth()));

    const uint8_t *const pixelPtr = (const uint8_t *)getPixelPtr(x, y, z);

    switch (m_format.type)
    {
    case TextureFormat::UNSIGNED_INT_24_8_REV:
        DE_ASSERT(m_format.order == TextureFormat::DS);
        return (int)readUint32High8(pixelPtr);

    case TextureFormat::UNSIGNED_INT_16_8_8:
    case TextureFormat::UNSIGNED_INT_24_8:
        DE_ASSERT(m_format.order == TextureFormat::DS);
        return (int)readUint32Low8(pixelPtr);

    case TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV:
        DE_ASSERT(m_format.order == TextureFormat::DS);
        return (int)readUint32Low8(pixelPtr + 4);

    default:
    {
        DE_ASSERT(m_format.order == TextureFormat::S); // no other combined depth stencil types
        return channelToInt(pixelPtr, m_format.type);
    }
    }
}

void PixelBufferAccess::setPixel(const Vec4 &color, int x, int y, int z) const
{
    DE_ASSERT(de::inBounds(x, 0, getWidth()));
    DE_ASSERT(de::inBounds(y, 0, getHeight()));
    DE_ASSERT(de::inBounds(z, 0, getDepth()));
    DE_ASSERT(!isCombinedDepthStencilType(m_format.type)); // combined types cannot be accessed directly
    DE_ASSERT(m_format.order != TextureFormat::DS);        // combined formats cannot be accessed directly

    uint8_t *const pixelPtr = (uint8_t *)getPixelPtr(x, y, z);

    // Optimized fomats.
    if (m_format.type == TextureFormat::UNORM_INT8)
    {
        if (m_format.order == TextureFormat::RGBA || m_format.order == TextureFormat::sRGBA)
        {
            writeRGBA8888Float(pixelPtr, color);
            return;
        }
        else if (m_format.order == TextureFormat::RGB || m_format.order == TextureFormat::sRGB)
        {
            writeRGB888Float(pixelPtr, color);
            return;
        }
    }

#define PN(VAL, OFFS, BITS) (unormFloatToChannel((VAL), (BITS)) << (OFFS))
#define PS(VAL, OFFS, BITS) (snormFloatToChannel((VAL), (BITS)) << (OFFS))
#define PU(VAL, OFFS, BITS) (uintToChannel((VAL), (BITS)) << (OFFS))
#define PI(VAL, OFFS, BITS) (intToChannel((VAL), (BITS)) << (OFFS))

    switch (m_format.type)
    {
    case TextureFormat::UNORM_BYTE_44:
        pixelPtr[0] = (uint8_t)(PN(color[0], 4, 4) | PN(color[1], 0, 4));
        break;
    case TextureFormat::UNSIGNED_BYTE_44:
        pixelPtr[0] = (uint8_t)(PU((uint32_t)color[0], 4, 4) | PU((uint32_t)color[1], 0, 4));
        break;
    case TextureFormat::UNORM_INT_101010:
        writeUnaligned<uint32_t>(pixelPtr, PN(color[0], 22, 10) | PN(color[1], 12, 10) | PN(color[2], 2, 10));
        break;

    case TextureFormat::UNORM_SHORT_565:
    {
        const Vec4 swizzled = swizzleGe(color, TextureFormat::RGB, m_format.order);
        writeUnaligned<uint16_t>(pixelPtr,
                                 (uint16_t)(PN(swizzled[0], 11, 5) | PN(swizzled[1], 5, 6) | PN(swizzled[2], 0, 5)));
        break;
    }

    case TextureFormat::UNSIGNED_SHORT_565:
    {
        const UVec4 swizzled = swizzleGe(color.cast<uint32_t>(), TextureFormat::RGB, m_format.order);
        writeUnaligned<uint16_t>(pixelPtr,
                                 (uint16_t)(PU(swizzled[0], 11, 5) | PU(swizzled[1], 5, 6) | PU(swizzled[2], 0, 5)));
        break;
    }

    case TextureFormat::UNORM_SHORT_555:
    {
        const Vec4 swizzled = swizzleGe(color, TextureFormat::RGB, m_format.order);
        writeUnaligned<uint16_t>(pixelPtr,
                                 (uint16_t)(PN(swizzled[0], 10, 5) | PN(swizzled[1], 5, 5) | PN(swizzled[2], 0, 5)));
        break;
    }

    case TextureFormat::UNORM_SHORT_4444:
    {
        const Vec4 swizzled = swizzleGe(color, TextureFormat::RGBA, m_format.order);
        writeUnaligned<uint16_t>(pixelPtr, (uint16_t)(PN(swizzled[0], 12, 4) | PN(swizzled[1], 8, 4) |
                                                      PN(swizzled[2], 4, 4) | PN(swizzled[3], 0, 4)));
        break;
    }

    case TextureFormat::UNSIGNED_SHORT_4444:
    {
        const UVec4 swizzled = swizzleGe(color.cast<uint32_t>(), TextureFormat::RGBA, m_format.order);
        writeUnaligned<uint16_t>(pixelPtr, (uint16_t)(PU(swizzled[0], 12, 4) | PU(swizzled[1], 8, 4) |
                                                      PU(swizzled[2], 4, 4) | PU(swizzled[3], 0, 4)));
        break;
    }

    case TextureFormat::UNORM_SHORT_5551:
    {
        const Vec4 swizzled = swizzleGe(color, TextureFormat::RGBA, m_format.order);
        writeUnaligned<uint16_t>(pixelPtr, (uint16_t)(PN(swizzled[0], 11, 5) | PN(swizzled[1], 6, 5) |
                                                      PN(swizzled[2], 1, 5) | PN(swizzled[3], 0, 1)));
        break;
    }

    case TextureFormat::UNORM_SHORT_1555:
    {
        const Vec4 swizzled = swizzleGe(color, TextureFormat::RGBA, m_format.order);
        writeUnaligned<uint16_t>(pixelPtr, (uint16_t)(PN(swizzled[0], 15, 1) | PN(swizzled[1], 10, 5) |
                                                      PN(swizzled[2], 5, 5) | PN(swizzled[3], 0, 5)));
        break;
    }

    case TextureFormat::UNSIGNED_SHORT_5551:
    {
        const UVec4 swizzled = swizzleGe(color.cast<uint32_t>(), TextureFormat::RGBA, m_format.order);
        writeUnaligned<uint16_t>(pixelPtr, (uint16_t)(PU(swizzled[0], 11, 5) | PU(swizzled[1], 6, 5) |
                                                      PU(swizzled[2], 1, 5) | PU(swizzled[3], 0, 1)));
        break;
    }

    case TextureFormat::UNORM_INT_1010102_REV:
    {
        const Vec4 u = swizzleGe(color, TextureFormat::RGBA, m_format.order);
        writeUnaligned<uint32_t>(pixelPtr, PN(u[0], 0, 10) | PN(u[1], 10, 10) | PN(u[2], 20, 10) | PN(u[3], 30, 2));
        break;
    }

    case TextureFormat::SNORM_INT_1010102_REV:
    {
        const Vec4 u = swizzleGe(color, TextureFormat::RGBA, m_format.order);
        writeUnaligned<uint32_t>(pixelPtr, PS(u[0], 0, 10) | PS(u[1], 10, 10) | PS(u[2], 20, 10) | PS(u[3], 30, 2));
        break;
    }

    case TextureFormat::UNSIGNED_INT_1010102_REV:
    case TextureFormat::USCALED_INT_1010102_REV:
    {
        const UVec4 u = swizzleGe(color.cast<uint32_t>(), TextureFormat::RGBA, m_format.order);
        writeUnaligned<uint32_t>(pixelPtr, PU(u[0], 0, 10) | PU(u[1], 10, 10) | PU(u[2], 20, 10) | PU(u[3], 30, 2));
        break;
    }

    case TextureFormat::SIGNED_INT_1010102_REV:
    case TextureFormat::SSCALED_INT_1010102_REV:
    {
        const IVec4 u = swizzleGe(color.cast<int32_t>(), TextureFormat::RGBA, m_format.order);
        writeUnaligned<uint32_t>(pixelPtr, PI(u[0], 0, 10) | PI(u[1], 10, 10) | PI(u[2], 20, 10) | PI(u[3], 30, 2));
        break;
    }

    case TextureFormat::UNSIGNED_INT_11F_11F_10F_REV:
        writeUnaligned<uint32_t>(pixelPtr, Float11(color[0]).bits() | (Float11(color[1]).bits() << 11) |
                                               (Float10(color[2]).bits() << 22));
        break;

    case TextureFormat::UNSIGNED_INT_999_E5_REV:
        writeUnaligned<uint32_t>(pixelPtr, packRGB999E5(color));
        break;

    default:
    {
        // Generic path.
        int numChannels                    = getNumUsedChannels(m_format.order);
        const TextureSwizzle::Channel *map = getChannelWriteSwizzle(m_format.order).components;
        int channelSize                    = getChannelSize(m_format.type);

        for (int c = 0; c < numChannels; c++)
        {
            DE_ASSERT(deInRange32(map[c], TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_3));
            floatToChannel(pixelPtr + channelSize * c, color[map[c]], m_format.type);
        }
        break;
    }
    }

#undef PN
#undef PS
#undef PU
#undef PI
}

void PixelBufferAccess::setPixel(const IVec4 &color, int x, int y, int z) const
{
    DE_ASSERT(de::inBounds(x, 0, getWidth()));
    DE_ASSERT(de::inBounds(y, 0, getHeight()));
    DE_ASSERT(de::inBounds(z, 0, getDepth()));
    DE_ASSERT(!isCombinedDepthStencilType(m_format.type)); // combined types cannot be accessed directly
    DE_ASSERT(m_format.order != TextureFormat::DS);        // combined formats cannot be accessed directly

    uint8_t *const pixelPtr = (uint8_t *)getPixelPtr(x, y, z);

    // Optimized fomats.
    if (m_format.type == TextureFormat::UNORM_INT8)
    {
        if (m_format.order == TextureFormat::RGBA || m_format.order == TextureFormat::sRGBA)
        {
            writeRGBA8888Int(pixelPtr, color);
            return;
        }
        else if (m_format.order == TextureFormat::RGB || m_format.order == TextureFormat::sRGB)
        {
            writeRGB888Int(pixelPtr, color);
            return;
        }
    }

#define PU(VAL, OFFS, BITS) (uintToChannel((uint32_t)(VAL), (BITS)) << (OFFS))
#define PI(VAL, OFFS, BITS) (intToChannel((uint32_t)(VAL), (BITS)) << (OFFS))

    switch (m_format.type)
    {
    case TextureFormat::UNSIGNED_BYTE_44: // Fall-through
    case TextureFormat::UNORM_BYTE_44:
        pixelPtr[0] = (uint8_t)(PU(color[0], 4, 4) | PU(color[1], 0, 4));
        break;
    case TextureFormat::UNORM_INT_101010:
        writeUnaligned<uint32_t>(pixelPtr, PU(color[0], 22, 10) | PU(color[1], 12, 10) | PU(color[2], 2, 10));
        break;

    case TextureFormat::UNORM_SHORT_565:
    case TextureFormat::UNSIGNED_SHORT_565:
    {
        const IVec4 swizzled = swizzleGe(color, TextureFormat::RGB, m_format.order);
        writeUnaligned<uint16_t>(pixelPtr,
                                 (uint16_t)(PU(swizzled[0], 11, 5) | PU(swizzled[1], 5, 6) | PU(swizzled[2], 0, 5)));
        break;
    }

    case TextureFormat::UNORM_SHORT_555:
    {
        const IVec4 swizzled = swizzleGe(color, TextureFormat::RGB, m_format.order);
        writeUnaligned<uint16_t>(pixelPtr,
                                 (uint16_t)(PU(swizzled[0], 10, 5) | PU(swizzled[1], 5, 5) | PU(swizzled[2], 0, 5)));
        break;
    }

    case TextureFormat::UNORM_SHORT_4444:
    case TextureFormat::UNSIGNED_SHORT_4444:
    {
        const IVec4 swizzled = swizzleGe(color, TextureFormat::RGBA, m_format.order);
        writeUnaligned<uint16_t>(pixelPtr, (uint16_t)(PU(swizzled[0], 12, 4) | PU(swizzled[1], 8, 4) |
                                                      PU(swizzled[2], 4, 4) | PU(swizzled[3], 0, 4)));
        break;
    }

    case TextureFormat::UNORM_SHORT_5551:
    case TextureFormat::UNSIGNED_SHORT_5551:
    {
        const IVec4 swizzled = swizzleGe(color, TextureFormat::RGBA, m_format.order);
        writeUnaligned<uint16_t>(pixelPtr, (uint16_t)(PU(swizzled[0], 11, 5) | PU(swizzled[1], 6, 5) |
                                                      PU(swizzled[2], 1, 5) | PU(swizzled[3], 0, 1)));
        break;
    }

    case TextureFormat::UNORM_SHORT_1555:
    {
        const IVec4 swizzled = swizzleGe(color, TextureFormat::RGBA, m_format.order);
        writeUnaligned<uint16_t>(pixelPtr, (uint16_t)(PU(swizzled[0], 15, 1) | PU(swizzled[1], 10, 5) |
                                                      PU(swizzled[2], 5, 5) | PU(swizzled[3], 0, 5)));
        break;
    }

    case TextureFormat::UNORM_INT_1010102_REV:
    case TextureFormat::UNSIGNED_INT_1010102_REV:
    case TextureFormat::USCALED_INT_1010102_REV:
    {
        const IVec4 swizzled = swizzleGe(color, TextureFormat::RGBA, m_format.order);
        writeUnaligned<uint32_t>(pixelPtr, PU(swizzled[0], 0, 10) | PU(swizzled[1], 10, 10) | PU(swizzled[2], 20, 10) |
                                               PU(swizzled[3], 30, 2));
        break;
    }

    case TextureFormat::SNORM_INT_1010102_REV:
    case TextureFormat::SIGNED_INT_1010102_REV:
    case TextureFormat::SSCALED_INT_1010102_REV:
    {
        const IVec4 swizzled = swizzleGe(color, TextureFormat::RGBA, m_format.order);
        writeUnaligned<uint32_t>(pixelPtr, PI(swizzled[0], 0, 10) | PI(swizzled[1], 10, 10) | PI(swizzled[2], 20, 10) |
                                               PI(swizzled[3], 30, 2));
        break;
    }

    default:
    {
        // Generic path.
        int numChannels                    = getNumUsedChannels(m_format.order);
        const TextureSwizzle::Channel *map = getChannelWriteSwizzle(m_format.order).components;
        int channelSize                    = getChannelSize(m_format.type);

        for (int c = 0; c < numChannels; c++)
        {
            DE_ASSERT(deInRange32(map[c], TextureSwizzle::CHANNEL_0, TextureSwizzle::CHANNEL_3));
            intToChannel(pixelPtr + channelSize * c, color[map[c]], m_format.type);
        }
        break;
    }
    }

#undef PU
#undef PI
}

void PixelBufferAccess::setPixDepth(float depth, int x, int y, int z) const
{
    DE_ASSERT(de::inBounds(x, 0, getWidth()));
    DE_ASSERT(de::inBounds(y, 0, getHeight()));
    DE_ASSERT(de::inBounds(z, 0, getDepth()));

    uint8_t *const pixelPtr = (uint8_t *)getPixelPtr(x, y, z);

    switch (m_format.type)
    {
    case TextureFormat::UNSIGNED_INT_16_8_8:
        DE_ASSERT(m_format.order == TextureFormat::DS);
        writeUint32High16(pixelPtr, convertSatRte<uint16_t>(depth * 65535.0f));
        break;

    case TextureFormat::UNSIGNED_INT_24_8:
        DE_ASSERT(m_format.order == TextureFormat::D || m_format.order == TextureFormat::DS);
        writeUint32High24(pixelPtr, convertSatRteUint24(depth * 16777215.0f));
        break;

    case TextureFormat::UNSIGNED_INT_24_8_REV:
        DE_ASSERT(m_format.order == TextureFormat::D || m_format.order == TextureFormat::DS);
        writeUint32Low24(pixelPtr, convertSatRteUint24(depth * 16777215.0f));
        break;

    case TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV:
        DE_ASSERT(m_format.order == TextureFormat::DS);
        writeUnaligned<float>(pixelPtr, depth);
        break;

    default:
        DE_ASSERT(m_format.order == TextureFormat::D); // no other combined depth stencil types
        floatToChannel(pixelPtr, depth, m_format.type);
        break;
    }
}

void PixelBufferAccess::setPixStencil(int stencil, int x, int y, int z) const
{
    DE_ASSERT(de::inBounds(x, 0, getWidth()));
    DE_ASSERT(de::inBounds(y, 0, getHeight()));
    DE_ASSERT(de::inBounds(z, 0, getDepth()));

    uint8_t *const pixelPtr = (uint8_t *)getPixelPtr(x, y, z);

    switch (m_format.type)
    {
    case TextureFormat::UNSIGNED_INT_16_8_8:
    case TextureFormat::UNSIGNED_INT_24_8:
        DE_ASSERT(m_format.order == TextureFormat::DS);
        writeUint32Low8(pixelPtr, convertSat<uint8_t>((uint32_t)stencil));
        break;

    case TextureFormat::UNSIGNED_INT_24_8_REV:
        DE_ASSERT(m_format.order == TextureFormat::DS);
        writeUint32High8(pixelPtr, convertSat<uint8_t>((uint32_t)stencil));
        break;

    case TextureFormat::FLOAT_UNSIGNED_INT_24_8_REV:
        DE_ASSERT(m_format.order == TextureFormat::DS);
        writeUint32Low8(pixelPtr + 4, convertSat<uint8_t>((uint32_t)stencil));
        break;

    default:
        DE_ASSERT(m_format.order == TextureFormat::S); // no other combined depth stencil types
        intToChannel(pixelPtr, stencil, m_format.type);
        break;
    }
}

static inline int imod(int a, int b)
{
    int m = a % b;
    return m < 0 ? m + b : m;
}

static inline int mirror(int a)
{
    return a >= 0 ? a : -(1 + a);
}

// Nearest-even rounding in case of tie (fractional part 0.5), otherwise ordinary rounding.
static inline float rint(float a)
{
    DE_STATIC_ASSERT((-3 % 2 != 0) && (-4 % 2 == 0));

    float fracVal = deFloatFrac(a);

    if (fracVal != 0.5f)
        return deFloatRound(a); // Ordinary case.

    float floorVal = a - fracVal;
    bool roundUp   = (int64_t)floorVal % 2 != 0;

    return floorVal + (roundUp ? 1.0f : 0.0f);
}

static inline int wrap(Sampler::WrapMode mode, int c, int size)
{
    switch (mode)
    {
    case tcu::Sampler::CLAMP_TO_BORDER:
        return deClamp32(c, -1, size);

    case tcu::Sampler::CLAMP_TO_EDGE:
        return deClamp32(c, 0, size - 1);

    case tcu::Sampler::REPEAT_GL:
        return imod(c, size);

    case tcu::Sampler::REPEAT_CL:
        return imod(c, size);

    case tcu::Sampler::MIRRORED_ONCE:
        c = deClamp32(c, -size, size);
        // Fall-through

    case tcu::Sampler::MIRRORED_REPEAT_GL:
        return (size - 1) - mirror(imod(c, 2 * size) - size);

    case tcu::Sampler::MIRRORED_REPEAT_CL:
        return deClamp32(c, 0, size - 1); // \note Actual mirroring done already in unnormalization function.

    default:
        DE_ASSERT(false);
        return 0;
    }
}

// Special unnormalization for REPEAT_CL and MIRRORED_REPEAT_CL wrap modes; otherwise ordinary unnormalization.
static inline float unnormalize(Sampler::WrapMode mode, float c, int size)
{
    switch (mode)
    {
    case tcu::Sampler::CLAMP_TO_EDGE:
    case tcu::Sampler::CLAMP_TO_BORDER:
    case tcu::Sampler::REPEAT_GL:
    case tcu::Sampler::MIRRORED_REPEAT_GL:
    case tcu::Sampler::MIRRORED_ONCE: // Fall-through (ordinary case).
        return (float)size * c;

    case tcu::Sampler::REPEAT_CL:
        return (float)size * (c - deFloatFloor(c));

    case tcu::Sampler::MIRRORED_REPEAT_CL:
        return (float)size * deFloatAbs(c - 2.0f * rint(0.5f * c));

    default:
        DE_ASSERT(false);
        return 0.0f;
    }
}

static bool isFixedPointDepthTextureFormat(const tcu::TextureFormat &format)
{
    DE_ASSERT(format.order == TextureFormat::D || format.order == TextureFormat::R);

    const tcu::TextureChannelClass channelClass = tcu::getTextureChannelClass(format.type);
    if (channelClass == tcu::TEXTURECHANNELCLASS_FLOATING_POINT)
        return false;
    else if (channelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT)
        return true;
    else
    {
        DE_ASSERT(false);
        return false;
    }
}

// Texel lookup with color conversion.
static inline Vec4 lookup(const ConstPixelBufferAccess &access, int i, int j, int k)
{
    const TextureFormat &format = access.getFormat();

    if (isSRGB(format))
    {
        if (format.type == TextureFormat::UNORM_INT8 && format.order == TextureFormat::sRGB)
            return sRGB8ToLinear(access.getPixelUint(i, j, k));
        else if (format.type == TextureFormat::UNORM_INT8 && format.order == TextureFormat::sRGBA)
            return sRGBA8ToLinear(access.getPixelUint(i, j, k));
        else
            return sRGBToLinear(access.getPixel(i, j, k));
    }
    else
    {
        return access.getPixel(i, j, k);
    }
}

// Border texel lookup with color conversion.
static inline Vec4 lookupBorder(const tcu::TextureFormat &format, const tcu::Sampler &sampler)
{
    // "lookup" for a combined format does not make sense, disallow
    DE_ASSERT(!isCombinedDepthStencilType(format.type));

    const tcu::TextureChannelClass channelClass = tcu::getTextureChannelClass(format.type);
    const bool isFloat                          = channelClass == tcu::TEXTURECHANNELCLASS_FLOATING_POINT;
    const bool isFixed                          = channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_FIXED_POINT ||
                         channelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_FIXED_POINT;
    const bool isPureInteger         = channelClass == tcu::TEXTURECHANNELCLASS_SIGNED_INTEGER;
    const bool isPureUnsignedInteger = channelClass == tcu::TEXTURECHANNELCLASS_UNSIGNED_INTEGER;

    if (isFloat || isFixed)
        return sampleTextureBorder<float>(format, sampler);
    else if (isPureInteger)
        return sampleTextureBorder<int32_t>(format, sampler).cast<float>();
    else if (isPureUnsignedInteger)
        return sampleTextureBorder<uint32_t>(format, sampler).cast<float>();
    else
    {
        DE_ASSERT(false);
        return Vec4(-1.0);
    }
}

static inline float execCompare(const tcu::Vec4 &color, Sampler::CompareMode compare, int chanNdx, float ref_,
                                bool isFixedPoint)
{
    const bool clampValues =
        isFixedPoint; // if comparing against a floating point texture, ref (and value) is not clamped
    const float cmp = (clampValues) ? (de::clamp(color[chanNdx], 0.0f, 1.0f)) : (color[chanNdx]);
    const float ref = (clampValues) ? (de::clamp(ref_, 0.0f, 1.0f)) : (ref_);
    bool res        = false;

    switch (compare)
    {
    case Sampler::COMPAREMODE_LESS:
        res = ref < cmp;
        break;
    case Sampler::COMPAREMODE_LESS_OR_EQUAL:
        res = ref <= cmp;
        break;
    case Sampler::COMPAREMODE_GREATER:
        res = ref > cmp;
        break;
    case Sampler::COMPAREMODE_GREATER_OR_EQUAL:
        res = ref >= cmp;
        break;
    case Sampler::COMPAREMODE_EQUAL:
        res = ref == cmp;
        break;
    case Sampler::COMPAREMODE_NOT_EQUAL:
        res = ref != cmp;
        break;
    case Sampler::COMPAREMODE_ALWAYS:
        res = true;
        break;
    case Sampler::COMPAREMODE_NEVER:
        res = false;
        break;
    default:
        DE_ASSERT(false);
    }

    return res ? 1.0f : 0.0f;
}

static Vec4 sampleNearest1D(const ConstPixelBufferAccess &access, const Sampler &sampler, float u, const IVec2 &offset)
{
    int width = access.getWidth();

    int x = deFloorFloatToInt32(u) + offset.x();

    // Check for CLAMP_TO_BORDER.
    if (sampler.wrapS == Sampler::CLAMP_TO_BORDER && !deInBounds32(x, 0, width))
        return lookupBorder(access.getFormat(), sampler);

    int i = wrap(sampler.wrapS, x, width);

    return lookup(access, i, offset.y(), 0);
}

static Vec4 sampleNearest2D(const ConstPixelBufferAccess &access, const Sampler &sampler, float u, float v,
                            const IVec3 &offset)
{
    int width  = access.getWidth();
    int height = access.getHeight();

    int x = deFloorFloatToInt32(u) + offset.x();
    int y = deFloorFloatToInt32(v) + offset.y();

    // Check for CLAMP_TO_BORDER.
    if ((sampler.wrapS == Sampler::CLAMP_TO_BORDER && !deInBounds32(x, 0, width)) ||
        (sampler.wrapT == Sampler::CLAMP_TO_BORDER && !deInBounds32(y, 0, height)))
        return lookupBorder(access.getFormat(), sampler);

    int i = wrap(sampler.wrapS, x, width);
    int j = wrap(sampler.wrapT, y, height);

    return lookup(access, i, j, offset.z());
}

static Vec4 sampleNearest3D(const ConstPixelBufferAccess &access, const Sampler &sampler, float u, float v, float w,
                            const IVec3 &offset)
{
    int width  = access.getWidth();
    int height = access.getHeight();
    int depth  = access.getDepth();

    int x = deFloorFloatToInt32(u) + offset.x();
    int y = deFloorFloatToInt32(v) + offset.y();
    int z = deFloorFloatToInt32(w) + offset.z();

    // Check for CLAMP_TO_BORDER.
    if ((sampler.wrapS == Sampler::CLAMP_TO_BORDER && !deInBounds32(x, 0, width)) ||
        (sampler.wrapT == Sampler::CLAMP_TO_BORDER && !deInBounds32(y, 0, height)) ||
        (sampler.wrapR == Sampler::CLAMP_TO_BORDER && !deInBounds32(z, 0, depth)))
        return lookupBorder(access.getFormat(), sampler);

    int i = wrap(sampler.wrapS, x, width);
    int j = wrap(sampler.wrapT, y, height);
    int k = wrap(sampler.wrapR, z, depth);

    return lookup(access, i, j, k);
}

static Vec4 sampleLinear1D(const ConstPixelBufferAccess &access, const Sampler &sampler, float u, const IVec2 &offset)
{
    int w = access.getWidth();

    int x0 = deFloorFloatToInt32(u - 0.5f) + offset.x();
    int x1 = x0 + 1;

    int i0 = wrap(sampler.wrapS, x0, w);
    int i1 = wrap(sampler.wrapS, x1, w);

    float a = deFloatFrac(u - 0.5f);

    bool i0UseBorder = sampler.wrapS == Sampler::CLAMP_TO_BORDER && !de::inBounds(i0, 0, w);
    bool i1UseBorder = sampler.wrapS == Sampler::CLAMP_TO_BORDER && !de::inBounds(i1, 0, w);

    // Border color for out-of-range coordinates if using CLAMP_TO_BORDER, otherwise execute lookups.
    Vec4 p0 = i0UseBorder ? lookupBorder(access.getFormat(), sampler) : lookup(access, i0, offset.y(), 0);
    Vec4 p1 = i1UseBorder ? lookupBorder(access.getFormat(), sampler) : lookup(access, i1, offset.y(), 0);

    // Interpolate.
    return p0 * (1.0f - a) + p1 * a;
}

static Vec4 sampleCubic1D(const ConstPixelBufferAccess &access, const Sampler &sampler, float u, const IVec2 &offset)
{
    int width = access.getWidth();

    tcu::IVec4 x, i;

    x[0] = deFloorFloatToInt32(u - 1.5f) + offset.x();
    x[1] = x[0] + 1;
    x[2] = x[1] + 1;
    x[3] = x[2] + 1;

    for (uint32_t m = 0; m < 4; ++m)
        i[m] = wrap(sampler.wrapS, x[m], width);

    bool iUseBorder[4];
    for (uint32_t m = 0; m < 4; ++m)
        iUseBorder[m] = sampler.wrapS == Sampler::CLAMP_TO_BORDER && !de::inBounds(i[m], 0, width);

    // Catmull-Rom basis matrix
    static const float crValues[16] = {0.0f, 1.0f,  0.0f, 0.0f,  -0.5f, 0.0f, 0.5f,  0.0f,
                                       1.0f, -2.5f, 2.0f, -0.5f, -0.5f, 1.5f, -1.5f, 0.5f};
    static const tcu::Mat4 crBasis(crValues);

    float a = deFloatFrac(u - 0.5f);
    tcu::Vec4 alpha(1, a, a * a, a * a * a);
    tcu::Vec4 wi = alpha * crBasis;

    tcu::Vec4 result(0.0f, 0.0f, 0.0f, 0.0f);
    for (uint32_t m = 0; m < 4; ++m)
    {
        tcu::Vec4 p = (iUseBorder[m]) ? lookupBorder(access.getFormat(), sampler) : lookup(access, i[m], offset.y(), 0);
        result += wi[m] * p;
    }
    return result;
}

static Vec4 sampleLinear2D(const ConstPixelBufferAccess &access, const Sampler &sampler, float u, float v,
                           const IVec3 &offset)
{
    int w = access.getWidth();
    int h = access.getHeight();

    int x0 = deFloorFloatToInt32(u - 0.5f) + offset.x();
    int x1 = x0 + 1;
    int y0 = deFloorFloatToInt32(v - 0.5f) + offset.y();
    int y1 = y0 + 1;

    int i0 = wrap(sampler.wrapS, x0, w);
    int i1 = wrap(sampler.wrapS, x1, w);
    int j0 = wrap(sampler.wrapT, y0, h);
    int j1 = wrap(sampler.wrapT, y1, h);

    float a = deFloatFrac(u - 0.5f);
    float b = deFloatFrac(v - 0.5f);

    bool i0UseBorder = sampler.wrapS == Sampler::CLAMP_TO_BORDER && !de::inBounds(i0, 0, w);
    bool i1UseBorder = sampler.wrapS == Sampler::CLAMP_TO_BORDER && !de::inBounds(i1, 0, w);
    bool j0UseBorder = sampler.wrapT == Sampler::CLAMP_TO_BORDER && !de::inBounds(j0, 0, h);
    bool j1UseBorder = sampler.wrapT == Sampler::CLAMP_TO_BORDER && !de::inBounds(j1, 0, h);

    // Border color for out-of-range coordinates if using CLAMP_TO_BORDER, otherwise execute lookups.
    Vec4 p00 =
        (i0UseBorder || j0UseBorder) ? lookupBorder(access.getFormat(), sampler) : lookup(access, i0, j0, offset.z());
    Vec4 p10 =
        (i1UseBorder || j0UseBorder) ? lookupBorder(access.getFormat(), sampler) : lookup(access, i1, j0, offset.z());
    Vec4 p01 =
        (i0UseBorder || j1UseBorder) ? lookupBorder(access.getFormat(), sampler) : lookup(access, i0, j1, offset.z());
    Vec4 p11 =
        (i1UseBorder || j1UseBorder) ? lookupBorder(access.getFormat(), sampler) : lookup(access, i1, j1, offset.z());

    // Interpolate.
    return (p00 * (1.0f - a) * (1.0f - b)) + (p10 * (a) * (1.0f - b)) + (p01 * (1.0f - a) * (b)) + (p11 * (a) * (b));
}

static Vec4 sampleCubic2D(const ConstPixelBufferAccess &access, const Sampler &sampler, float u, float v,
                          const IVec3 &offset)
{
    int width  = access.getWidth();
    int height = access.getHeight();

    tcu::IVec4 x, y, i, j;

    x[0] = deFloorFloatToInt32(u - 1.5f) + offset.x();
    x[1] = x[0] + 1;
    x[2] = x[1] + 1;
    x[3] = x[2] + 1;
    y[0] = deFloorFloatToInt32(v - 1.5f) + offset.y();
    y[1] = y[0] + 1;
    y[2] = y[1] + 1;
    y[3] = y[2] + 1;

    for (uint32_t m = 0; m < 4; ++m)
        i[m] = wrap(sampler.wrapS, x[m], width);
    for (uint32_t n = 0; n < 4; ++n)
        j[n] = wrap(sampler.wrapT, y[n], height);

    bool iUseBorder[4], jUseBorder[4];
    for (uint32_t m = 0; m < 4; ++m)
        iUseBorder[m] = sampler.wrapS == Sampler::CLAMP_TO_BORDER && !de::inBounds(i[m], 0, width);
    for (uint32_t n = 0; n < 4; ++n)
        jUseBorder[n] = sampler.wrapT == Sampler::CLAMP_TO_BORDER && !de::inBounds(j[n], 0, height);

    // Catmull-Rom basis matrix
    static const float crValues[16] = {0.0f, 1.0f,  0.0f, 0.0f,  -0.5f, 0.0f, 0.5f,  0.0f,
                                       1.0f, -2.5f, 2.0f, -0.5f, -0.5f, 1.5f, -1.5f, 0.5f};
    static const tcu::Mat4 crBasis(crValues);

    float a = deFloatFrac(u - 0.5f);
    float b = deFloatFrac(v - 0.5f);
    tcu::Vec4 alpha(1, a, a * a, a * a * a);
    tcu::Vec4 beta(1, b, b * b, b * b * b);
    tcu::Vec4 wi = alpha * crBasis;
    tcu::Vec4 wj = beta * crBasis;

    tcu::Vec4 result(0.0f, 0.0f, 0.0f, 0.0f);
    for (uint32_t n = 0; n < 4; ++n)
        for (uint32_t m = 0; m < 4; ++m)
        {
            tcu::Vec4 p = (iUseBorder[m] || jUseBorder[n]) ? lookupBorder(access.getFormat(), sampler) :
                                                             lookup(access, i[m], j[n], offset.z());
            result += wi[m] * wj[n] * p;
        }
    return result;
}

static float sampleLinear1DCompare(const ConstPixelBufferAccess &access, const Sampler &sampler, float ref, float u,
                                   const IVec2 &offset, bool isFixedPointDepthFormat)
{
    int w = access.getWidth();

    int x0 = deFloorFloatToInt32(u - 0.5f) + offset.x();
    int x1 = x0 + 1;

    int i0 = wrap(sampler.wrapS, x0, w);
    int i1 = wrap(sampler.wrapS, x1, w);

    float a = deFloatFrac(u - 0.5f);

    bool i0UseBorder = sampler.wrapS == Sampler::CLAMP_TO_BORDER && !de::inBounds(i0, 0, w);
    bool i1UseBorder = sampler.wrapS == Sampler::CLAMP_TO_BORDER && !de::inBounds(i1, 0, w);

    // Border color for out-of-range coordinates if using CLAMP_TO_BORDER, otherwise execute lookups.
    Vec4 p0Clr = i0UseBorder ? lookupBorder(access.getFormat(), sampler) : lookup(access, i0, offset.y(), 0);
    Vec4 p1Clr = i1UseBorder ? lookupBorder(access.getFormat(), sampler) : lookup(access, i1, offset.y(), 0);

    // Execute comparisons.
    float p0 = execCompare(p0Clr, sampler.compare, sampler.compareChannel, ref, isFixedPointDepthFormat);
    float p1 = execCompare(p1Clr, sampler.compare, sampler.compareChannel, ref, isFixedPointDepthFormat);

    // Interpolate.
    return (p0 * (1.0f - a)) + (p1 * a);
}

static float sampleLinear2DCompare(const ConstPixelBufferAccess &access, const Sampler &sampler, float ref, float u,
                                   float v, const IVec3 &offset, bool isFixedPointDepthFormat)
{
    int w = access.getWidth();
    int h = access.getHeight();

    int x0 = deFloorFloatToInt32(u - 0.5f) + offset.x();
    int x1 = x0 + 1;
    int y0 = deFloorFloatToInt32(v - 0.5f) + offset.y();
    int y1 = y0 + 1;

    int i0 = wrap(sampler.wrapS, x0, w);
    int i1 = wrap(sampler.wrapS, x1, w);
    int j0 = wrap(sampler.wrapT, y0, h);
    int j1 = wrap(sampler.wrapT, y1, h);

    float a = deFloatFrac(u - 0.5f);
    float b = deFloatFrac(v - 0.5f);

    bool i0UseBorder = sampler.wrapS == Sampler::CLAMP_TO_BORDER && !de::inBounds(i0, 0, w);
    bool i1UseBorder = sampler.wrapS == Sampler::CLAMP_TO_BORDER && !de::inBounds(i1, 0, w);
    bool j0UseBorder = sampler.wrapT == Sampler::CLAMP_TO_BORDER && !de::inBounds(j0, 0, h);
    bool j1UseBorder = sampler.wrapT == Sampler::CLAMP_TO_BORDER && !de::inBounds(j1, 0, h);

    // Border color for out-of-range coordinates if using CLAMP_TO_BORDER, otherwise execute lookups.
    Vec4 p00Clr =
        (i0UseBorder || j0UseBorder) ? lookupBorder(access.getFormat(), sampler) : lookup(access, i0, j0, offset.z());
    Vec4 p10Clr =
        (i1UseBorder || j0UseBorder) ? lookupBorder(access.getFormat(), sampler) : lookup(access, i1, j0, offset.z());
    Vec4 p01Clr =
        (i0UseBorder || j1UseBorder) ? lookupBorder(access.getFormat(), sampler) : lookup(access, i0, j1, offset.z());
    Vec4 p11Clr =
        (i1UseBorder || j1UseBorder) ? lookupBorder(access.getFormat(), sampler) : lookup(access, i1, j1, offset.z());

    // Execute comparisons.
    float p00 = execCompare(p00Clr, sampler.compare, sampler.compareChannel, ref, isFixedPointDepthFormat);
    float p10 = execCompare(p10Clr, sampler.compare, sampler.compareChannel, ref, isFixedPointDepthFormat);
    float p01 = execCompare(p01Clr, sampler.compare, sampler.compareChannel, ref, isFixedPointDepthFormat);
    float p11 = execCompare(p11Clr, sampler.compare, sampler.compareChannel, ref, isFixedPointDepthFormat);

    // Interpolate.
    return (p00 * (1.0f - a) * (1.0f - b)) + (p10 * (a) * (1.0f - b)) + (p01 * (1.0f - a) * (b)) + (p11 * (a) * (b));
}

static Vec4 sampleLinear3D(const ConstPixelBufferAccess &access, const Sampler &sampler, float u, float v, float w,
                           const IVec3 &offset)
{
    int width  = access.getWidth();
    int height = access.getHeight();
    int depth  = access.getDepth();

    int x0 = deFloorFloatToInt32(u - 0.5f) + offset.x();
    int x1 = x0 + 1;
    int y0 = deFloorFloatToInt32(v - 0.5f) + offset.y();
    int y1 = y0 + 1;
    int z0 = deFloorFloatToInt32(w - 0.5f) + offset.z();
    int z1 = z0 + 1;

    int i0 = wrap(sampler.wrapS, x0, width);
    int i1 = wrap(sampler.wrapS, x1, width);
    int j0 = wrap(sampler.wrapT, y0, height);
    int j1 = wrap(sampler.wrapT, y1, height);
    int k0 = wrap(sampler.wrapR, z0, depth);
    int k1 = wrap(sampler.wrapR, z1, depth);

    float a = deFloatFrac(u - 0.5f);
    float b = deFloatFrac(v - 0.5f);
    float c = deFloatFrac(w - 0.5f);

    bool i0UseBorder = sampler.wrapS == Sampler::CLAMP_TO_BORDER && !de::inBounds(i0, 0, width);
    bool i1UseBorder = sampler.wrapS == Sampler::CLAMP_TO_BORDER && !de::inBounds(i1, 0, width);
    bool j0UseBorder = sampler.wrapT == Sampler::CLAMP_TO_BORDER && !de::inBounds(j0, 0, height);
    bool j1UseBorder = sampler.wrapT == Sampler::CLAMP_TO_BORDER && !de::inBounds(j1, 0, height);
    bool k0UseBorder = sampler.wrapR == Sampler::CLAMP_TO_BORDER && !de::inBounds(k0, 0, depth);
    bool k1UseBorder = sampler.wrapR == Sampler::CLAMP_TO_BORDER && !de::inBounds(k1, 0, depth);

    // Border color for out-of-range coordinates if using CLAMP_TO_BORDER, otherwise execute lookups.
    Vec4 p000 = (i0UseBorder || j0UseBorder || k0UseBorder) ? lookupBorder(access.getFormat(), sampler) :
                                                              lookup(access, i0, j0, k0);
    Vec4 p100 = (i1UseBorder || j0UseBorder || k0UseBorder) ? lookupBorder(access.getFormat(), sampler) :
                                                              lookup(access, i1, j0, k0);
    Vec4 p010 = (i0UseBorder || j1UseBorder || k0UseBorder) ? lookupBorder(access.getFormat(), sampler) :
                                                              lookup(access, i0, j1, k0);
    Vec4 p110 = (i1UseBorder || j1UseBorder || k0UseBorder) ? lookupBorder(access.getFormat(), sampler) :
                                                              lookup(access, i1, j1, k0);
    Vec4 p001 = (i0UseBorder || j0UseBorder || k1UseBorder) ? lookupBorder(access.getFormat(), sampler) :
                                                              lookup(access, i0, j0, k1);
    Vec4 p101 = (i1UseBorder || j0UseBorder || k1UseBorder) ? lookupBorder(access.getFormat(), sampler) :
                                                              lookup(access, i1, j0, k1);
    Vec4 p011 = (i0UseBorder || j1UseBorder || k1UseBorder) ? lookupBorder(access.getFormat(), sampler) :
                                                              lookup(access, i0, j1, k1);
    Vec4 p111 = (i1UseBorder || j1UseBorder || k1UseBorder) ? lookupBorder(access.getFormat(), sampler) :
                                                              lookup(access, i1, j1, k1);

    // Interpolate.
    return (p000 * (1.0f - a) * (1.0f - b) * (1.0f - c)) + (p100 * (a) * (1.0f - b) * (1.0f - c)) +
           (p010 * (1.0f - a) * (b) * (1.0f - c)) + (p110 * (a) * (b) * (1.0f - c)) +
           (p001 * (1.0f - a) * (1.0f - b) * (c)) + (p101 * (a) * (1.0f - b) * (c)) + (p011 * (1.0f - a) * (b) * (c)) +
           (p111 * (a) * (b) * (c));
}

static Vec4 sampleCubic3D(const ConstPixelBufferAccess &access, const Sampler &sampler, float u, float v, float w,
                          const IVec3 &offset)
{
    int width  = access.getWidth();
    int height = access.getHeight();
    int depth  = access.getDepth();

    tcu::IVec4 x, y, z, i, j, k;

    x[0] = deFloorFloatToInt32(u - 1.5f) + offset.x();
    x[1] = x[0] + 1;
    x[2] = x[1] + 1;
    x[3] = x[2] + 1;
    y[0] = deFloorFloatToInt32(v - 1.5f) + offset.y();
    y[1] = y[0] + 1;
    y[2] = y[1] + 1;
    y[3] = y[2] + 1;
    z[0] = deFloorFloatToInt32(w - 1.5f) + offset.z();
    z[1] = z[0] + 1;
    z[2] = z[1] + 1;
    z[3] = z[2] + 1;

    for (uint32_t m = 0; m < 4; ++m)
        i[m] = wrap(sampler.wrapS, x[m], width);
    for (uint32_t n = 0; n < 4; ++n)
        j[n] = wrap(sampler.wrapT, y[n], height);
    for (uint32_t o = 0; o < 4; ++o)
        k[o] = wrap(sampler.wrapR, k[o], depth);

    bool iUseBorder[4], jUseBorder[4], kUseBorder[4];
    for (uint32_t m = 0; m < 4; ++m)
        iUseBorder[m] = sampler.wrapS == Sampler::CLAMP_TO_BORDER && !de::inBounds(i[m], 0, width);
    for (uint32_t n = 0; n < 4; ++n)
        jUseBorder[n] = sampler.wrapT == Sampler::CLAMP_TO_BORDER && !de::inBounds(j[n], 0, height);
    for (uint32_t o = 0; o < 4; ++o)
        kUseBorder[o] = sampler.wrapR == Sampler::CLAMP_TO_BORDER && !de::inBounds(k[o], 0, depth);

    // Catmull-Rom basis matrix
    static const float crValues[16] = {0.0f, 1.0f,  0.0f, 0.0f,  -0.5f, 0.0f, 0.5f,  0.0f,
                                       1.0f, -2.5f, 2.0f, -0.5f, -0.5f, 1.5f, -1.5f, 0.5f};
    static const tcu::Mat4 crBasis(crValues);

    float a = deFloatFrac(u - 0.5f);
    float b = deFloatFrac(v - 0.5f);
    float c = deFloatFrac(w - 0.5f);
    tcu::Vec4 alpha(1, a, a * a, a * a * a);
    tcu::Vec4 beta(1, b, b * b, b * b * b);
    tcu::Vec4 gamma(1, c, c * c, c * c * c);
    tcu::Vec4 wi = alpha * crBasis;
    tcu::Vec4 wj = beta * crBasis;
    tcu::Vec4 wk = gamma * crBasis;

    tcu::Vec4 result(0.0f, 0.0f, 0.0f, 0.0f);
    for (uint32_t o = 0; o < 4; ++o)
        for (uint32_t n = 0; n < 4; ++n)
            for (uint32_t m = 0; m < 4; ++m)
            {
                tcu::Vec4 p = (iUseBorder[m] || jUseBorder[n] || kUseBorder[o]) ?
                                  lookupBorder(access.getFormat(), sampler) :
                                  lookup(access, i[m], j[n], k[o]);
                result += wi[m] * wj[n] * wk[o] * p;
            }
    return result;
}

Vec4 ConstPixelBufferAccess::sample1D(const Sampler &sampler, Sampler::FilterMode filter, float s, int level) const
{
    // check selected layer exists
    DE_ASSERT(de::inBounds(level, 0, m_size.y()));

    return sample1DOffset(sampler, filter, s, tcu::IVec2(0, level));
}

Vec4 ConstPixelBufferAccess::sample2D(const Sampler &sampler, Sampler::FilterMode filter, float s, float t,
                                      int depth) const
{
    // check selected layer exists
    DE_ASSERT(de::inBounds(depth, 0, m_size.z()));

    return sample2DOffset(sampler, filter, s, t, tcu::IVec3(0, 0, depth));
}

Vec4 ConstPixelBufferAccess::sample3D(const Sampler &sampler, Sampler::FilterMode filter, float s, float t,
                                      float r) const
{
    return sample3DOffset(sampler, filter, s, t, r, tcu::IVec3(0, 0, 0));
}

Vec4 ConstPixelBufferAccess::sample1DOffset(const Sampler &sampler, Sampler::FilterMode filter, float s,
                                            const IVec2 &offset) const
{
    // check selected layer exists
    // \note offset.x is X offset, offset.y is the selected layer
    DE_ASSERT(de::inBounds(offset.y(), 0, m_size.y()));

    // Non-normalized coordinates.
    float u = s;

    if (sampler.normalizedCoords)
        u = unnormalize(sampler.wrapS, s, m_size.x());

    switch (filter)
    {
    case Sampler::NEAREST:
        return sampleNearest1D(*this, sampler, u, offset);
    case Sampler::LINEAR:
        return sampleLinear1D(*this, sampler, u, offset);
    case Sampler::CUBIC:
        return sampleCubic1D(*this, sampler, u, offset);
    default:
        DE_ASSERT(false);
        return Vec4(0.0f);
    }
}

Vec4 ConstPixelBufferAccess::sample2DOffset(const Sampler &sampler, Sampler::FilterMode filter, float s, float t,
                                            const IVec3 &offset) const
{
    // check selected layer exists
    // \note offset.xy is the XY offset, offset.z is the selected layer
    DE_ASSERT(de::inBounds(offset.z(), 0, m_size.z()));

    // Non-normalized coordinates.
    float u = s;
    float v = t;

    if (sampler.normalizedCoords)
    {
        u = unnormalize(sampler.wrapS, s, m_size.x());
        v = unnormalize(sampler.wrapT, t, m_size.y());
    }

    switch (filter)
    {
    case Sampler::NEAREST:
        return sampleNearest2D(*this, sampler, u, v, offset);
    case Sampler::LINEAR:
        return sampleLinear2D(*this, sampler, u, v, offset);
    case Sampler::CUBIC:
        return sampleCubic2D(*this, sampler, u, v, offset);
    default:
        DE_ASSERT(false);
        return Vec4(0.0f);
    }
}

Vec4 ConstPixelBufferAccess::sample3DOffset(const Sampler &sampler, Sampler::FilterMode filter, float s, float t,
                                            float r, const IVec3 &offset) const
{
    // Non-normalized coordinates.
    float u = s;
    float v = t;
    float w = r;

    if (sampler.normalizedCoords)
    {
        u = unnormalize(sampler.wrapS, s, m_size.x());
        v = unnormalize(sampler.wrapT, t, m_size.y());
        w = unnormalize(sampler.wrapR, r, m_size.z());
    }

    switch (filter)
    {
    case Sampler::NEAREST:
        return sampleNearest3D(*this, sampler, u, v, w, offset);
    case Sampler::LINEAR:
        return sampleLinear3D(*this, sampler, u, v, w, offset);
    case Sampler::CUBIC:
        return sampleCubic3D(*this, sampler, u, v, w, offset);
    default:
        DE_ASSERT(false);
        return Vec4(0.0f);
    }
}

float ConstPixelBufferAccess::sample1DCompare(const Sampler &sampler, Sampler::FilterMode filter, float ref, float s,
                                              const IVec2 &offset) const
{
    // check selected layer exists
    // \note offset.x is X offset, offset.y is the selected layer
    DE_ASSERT(de::inBounds(offset.y(), 0, m_size.y()));

    // Format information for comparison function
    const bool isFixedPointDepth = isFixedPointDepthTextureFormat(m_format);

    // Non-normalized coordinates.
    float u = s;

    if (sampler.normalizedCoords)
        u = unnormalize(sampler.wrapS, s, m_size.x());

    switch (filter)
    {
    case Sampler::NEAREST:
        return execCompare(sampleNearest1D(*this, sampler, u, offset), sampler.compare, sampler.compareChannel, ref,
                           isFixedPointDepth);
    case Sampler::LINEAR:
        return sampleLinear1DCompare(*this, sampler, ref, u, offset, isFixedPointDepth);
    default:
        DE_ASSERT(false);
        return 0.0f;
    }
}

float ConstPixelBufferAccess::sample2DCompare(const Sampler &sampler, Sampler::FilterMode filter, float ref, float s,
                                              float t, const IVec3 &offset) const
{
    // check selected layer exists
    // \note offset.xy is XY offset, offset.z is the selected layer
    DE_ASSERT(de::inBounds(offset.z(), 0, m_size.z()));

    // Format information for comparison function
    const bool isFixedPointDepth = isFixedPointDepthTextureFormat(m_format);

    // Non-normalized coordinates.
    float u = s;
    float v = t;

    if (sampler.normalizedCoords)
    {
        u = unnormalize(sampler.wrapS, s, m_size.x());
        v = unnormalize(sampler.wrapT, t, m_size.y());
    }

    switch (filter)
    {
    case Sampler::NEAREST:
        return execCompare(sampleNearest2D(*this, sampler, u, v, offset), sampler.compare, sampler.compareChannel, ref,
                           isFixedPointDepth);
    case Sampler::LINEAR:
        return sampleLinear2DCompare(*this, sampler, ref, u, v, offset, isFixedPointDepth);
    default:
        DE_ASSERT(false);
        return 0.0f;
    }
}

TextureLevel::TextureLevel(void) : m_format(), m_size(0)
{
}

TextureLevel::TextureLevel(const TextureFormat &format) : m_format(format), m_size(0)
{
}

TextureLevel::TextureLevel(const TextureFormat &format, int width, int height, int depth) : m_format(format), m_size(0)
{
    setSize(width, height, depth);
}

TextureLevel::~TextureLevel(void)
{
}

void TextureLevel::setStorage(const TextureFormat &format, int width, int height, int depth)
{
    m_format = format;
    setSize(width, height, depth);
}

void TextureLevel::setSize(int width, int height, int depth)
{
    int pixelSize = m_format.getPixelSize();

    m_size = IVec3(width, height, depth);

    m_data.setStorage(m_size.x() * m_size.y() * m_size.z() * pixelSize);
}

Vec4 sampleLevelArray1D(const ConstPixelBufferAccess *levels, int numLevels, const Sampler &sampler, float s, int depth,
                        float lod)
{
    return sampleLevelArray1DOffset(levels, numLevels, sampler, s, lod,
                                    IVec2(0, depth)); // y-offset in 1D textures is layer selector
}

Vec4 sampleLevelArray2D(const ConstPixelBufferAccess *levels, int numLevels, const Sampler &sampler, float s, float t,
                        int depth, float lod, bool es2, ImageViewMinLodParams *minLodParams)
{
    return sampleLevelArray2DOffset(levels, numLevels, sampler, s, t, lod, IVec3(0, 0, depth), es2,
                                    minLodParams); // z-offset in 2D textures is layer selector
}

Vec4 sampleLevelArray3D(const ConstPixelBufferAccess *levels, int numLevels, const Sampler &sampler, float s, float t,
                        float r, float lod, ImageViewMinLodParams *minLodParams)
{
    return sampleLevelArray3DOffset(levels, numLevels, sampler, s, t, r, lod, IVec3(0, 0, 0), minLodParams);
}

Vec4 sampleLevelArray1DOffset(const ConstPixelBufferAccess *levels, int numLevels, const Sampler &sampler, float s,
                              float lod, const IVec2 &offset)
{
    bool magnified                 = lod <= sampler.lodThreshold;
    Sampler::FilterMode filterMode = magnified ? sampler.magFilter : sampler.minFilter;

    switch (filterMode)
    {
    case Sampler::NEAREST:
        return levels[0].sample1DOffset(sampler, filterMode, s, offset);
    case Sampler::LINEAR:
        return levels[0].sample1DOffset(sampler, filterMode, s, offset);

    case Sampler::NEAREST_MIPMAP_NEAREST:
    case Sampler::LINEAR_MIPMAP_NEAREST:
    {
        int maxLevel = (int)numLevels - 1;
        int level    = deClamp32((int)deFloatCeil(lod + 0.5f) - 1, 0, maxLevel);
        Sampler::FilterMode levelFilter =
            (filterMode == Sampler::LINEAR_MIPMAP_NEAREST) ? Sampler::LINEAR : Sampler::NEAREST;

        return levels[level].sample1DOffset(sampler, levelFilter, s, offset);
    }

    case Sampler::NEAREST_MIPMAP_LINEAR:
    case Sampler::LINEAR_MIPMAP_LINEAR:
    {
        int maxLevel = (int)numLevels - 1;
        int level0   = deClamp32((int)deFloatFloor(lod), 0, maxLevel);
        int level1   = de::min(maxLevel, level0 + 1);
        Sampler::FilterMode levelFilter =
            (filterMode == Sampler::LINEAR_MIPMAP_LINEAR) ? Sampler::LINEAR : Sampler::NEAREST;
        float f      = deFloatFrac(lod);
        tcu::Vec4 t0 = levels[level0].sample1DOffset(sampler, levelFilter, s, offset);
        tcu::Vec4 t1 = levels[level1].sample1DOffset(sampler, levelFilter, s, offset);

        return t0 * (1.0f - f) + t1 * f;
    }

    default:
        DE_ASSERT(false);
        return Vec4(0.0f);
    }
}

Vec4 sampleLevelArray2DOffset(const ConstPixelBufferAccess *levels, int numLevels, const Sampler &sampler, float s,
                              float t, float lod, const IVec3 &offset, bool es2, ImageViewMinLodParams *minLodParams)
{
    bool magnified;
    // minLodRelative is used to calculate the image level to sample from, when VK_EXT_image_view_min_lod extension is enabled.
    // The value is relative to baseLevel as the Texture*View was created as the baseLevel being level[0].
    const float minLodRelative =
        (minLodParams != nullptr) ? getImageViewMinLod(minLodParams->minLod) - (float)minLodParams->baseLevel : 0.0f;

    if (es2 && sampler.magFilter == Sampler::LINEAR &&
        (sampler.minFilter == Sampler::NEAREST_MIPMAP_NEAREST || sampler.minFilter == Sampler::NEAREST_MIPMAP_LINEAR))
        magnified = lod <= 0.5;
    else
        magnified = lod <= sampler.lodThreshold;

    // VK_EXT_image_view_min_lod: Integer Texel Coordinates case (with robustness2 supported)
    if (minLodParams != nullptr && minLodParams->intTexCoord)
    {
        if (lod < deFloatFloor(minLodRelative) || lod >= (float)numLevels)
            return Vec4(0.0f);

        if (s < 0.0f || s > 1.0f || t < 0.0f || t > 1.0f)
            return Vec4(0.0f);
    }

    Sampler::FilterMode filterMode = magnified ? sampler.magFilter : sampler.minFilter;
    switch (filterMode)
    {
    case Sampler::NEAREST:
    case Sampler::LINEAR:
    case Sampler::CUBIC:
    {
        bool isLinearMipmapMode = magnified && tcu::isSamplerMipmapModeLinear(sampler.minFilter);
        const int maxLevel      = (int)numLevels - 1;
        const int level0        = isLinearMipmapMode ? (int)deFloatFloor(minLodRelative) :
                                                       deClamp32((int)deFloatCeil(minLodRelative + 0.5f) - 1, 0, maxLevel);
        tcu::Vec4 t0            = levels[level0].sample2DOffset(sampler, filterMode, s, t, offset);

        if (!isLinearMipmapMode)
            return t0;

        const float frac = deFloatFrac(minLodRelative);
        const int level1 = de::min(level0 + 1, maxLevel);
        tcu::Vec4 t1     = levels[level1].sample2DOffset(sampler, filterMode, s, t, offset);
        return t0 * (1.0f - frac) + t1 * frac;
    }

    case Sampler::NEAREST_MIPMAP_NEAREST:
    case Sampler::LINEAR_MIPMAP_NEAREST:
    case Sampler::CUBIC_MIPMAP_NEAREST:
    {
        if (minLodParams != nullptr && !minLodParams->intTexCoord)
            lod = de::max(lod, minLodRelative);

        int maxLevel = (int)numLevels - 1;
        int level    = deClamp32((int)deFloatCeil(lod + 0.5f) - 1, 0, maxLevel);
        Sampler::FilterMode levelFilter;
        switch (filterMode)
        {
        case Sampler::NEAREST_MIPMAP_NEAREST:
            levelFilter = Sampler::NEAREST;
            break;
        case Sampler::LINEAR_MIPMAP_NEAREST:
            levelFilter = Sampler::LINEAR;
            break;
        case Sampler::CUBIC_MIPMAP_NEAREST:
            levelFilter = Sampler::CUBIC;
            break;
        default:
            DE_ASSERT(false);
            return Vec4(0.0f);
        }

        return levels[level].sample2DOffset(sampler, levelFilter, s, t, offset);
    }

    case Sampler::NEAREST_MIPMAP_LINEAR:
    case Sampler::LINEAR_MIPMAP_LINEAR:
    case Sampler::CUBIC_MIPMAP_LINEAR:
    {
        if (minLodParams != nullptr && !minLodParams->intTexCoord)
            lod = de::max(lod, minLodRelative);

        int maxLevel = (int)numLevels - 1;
        int level0   = deClamp32((int)deFloatFloor(lod), 0, maxLevel);
        int level1   = de::min(maxLevel, level0 + 1);
        Sampler::FilterMode levelFilter;
        switch (filterMode)
        {
        case Sampler::NEAREST_MIPMAP_LINEAR:
            levelFilter = Sampler::NEAREST;
            break;
        case Sampler::LINEAR_MIPMAP_LINEAR:
            levelFilter = Sampler::LINEAR;
            break;
        case Sampler::CUBIC_MIPMAP_LINEAR:
            levelFilter = Sampler::CUBIC;
            break;
        default:
            DE_ASSERT(false);
            return Vec4(0.0f);
        }
        float f      = deFloatFrac(lod);
        tcu::Vec4 t0 = levels[level0].sample2DOffset(sampler, levelFilter, s, t, offset);
        tcu::Vec4 t1 = levels[level1].sample2DOffset(sampler, levelFilter, s, t, offset);

        return t0 * (1.0f - f) + t1 * f;
    }

    default:
        DE_ASSERT(false);
        return Vec4(0.0f);
    }
}

Vec4 sampleLevelArray3DOffset(const ConstPixelBufferAccess *levels, int numLevels, const Sampler &sampler, float s,
                              float t, float r, float lod, const IVec3 &offset, ImageViewMinLodParams *minLodParams)
{
    // minLodRelative is used to calculate the image level to sample from, when VK_EXT_image_view_min_lod extension is enabled.
    // The value is relative to baseLevel as the Texture*View was created as the baseLevel being level[0].
    const float minLodRelative =
        (minLodParams != nullptr) ? getImageViewMinLod(minLodParams->minLod) - (float)minLodParams->baseLevel : 0.0f;
    bool magnified                 = lod <= sampler.lodThreshold;
    Sampler::FilterMode filterMode = magnified ? sampler.magFilter : sampler.minFilter;

    // VK_EXT_image_view_min_lod: Integer Texel Coordinates case (with robustness2 supported)
    if (minLodParams != nullptr && minLodParams->intTexCoord)
    {
        if (lod < deFloatFloor(minLodRelative) || lod >= (float)numLevels)
            return Vec4(0.0f);

        if (s < 0.0f || s > 1.0f || t < 0.0f || t > 1.0f)
            return Vec4(0.0f);
    }

    switch (filterMode)
    {
    case Sampler::NEAREST:
    case Sampler::LINEAR:
    {
        bool isLinearMipmapMode = magnified && tcu::isSamplerMipmapModeLinear(sampler.minFilter);
        const int maxLevel      = (int)numLevels - 1;
        const int level0        = isLinearMipmapMode ? (int)deFloatFloor(minLodRelative) :
                                                       deClamp32((int)deFloatCeil(minLodRelative + 0.5f) - 1, 0, maxLevel);
        tcu::Vec4 t0            = levels[level0].sample3DOffset(sampler, filterMode, s, t, r, offset);

        if (!isLinearMipmapMode)
            return t0;

        const float frac = deFloatFrac(minLodRelative);
        const int level1 = de::min(level0 + 1, maxLevel);
        tcu::Vec4 t1     = levels[level1].sample3DOffset(sampler, filterMode, s, t, r, offset);
        return t0 * (1.0f - frac) + t1 * frac;
    }

    case Sampler::NEAREST_MIPMAP_NEAREST:
    case Sampler::LINEAR_MIPMAP_NEAREST:
    {
        if (minLodParams != nullptr && !minLodParams->intTexCoord)
            lod = de::max(lod, minLodRelative);

        int maxLevel = (int)numLevels - 1;
        int level    = deClamp32((int)deFloatCeil(lod + 0.5f) - 1, 0, maxLevel);
        Sampler::FilterMode levelFilter =
            (filterMode == Sampler::LINEAR_MIPMAP_NEAREST) ? Sampler::LINEAR : Sampler::NEAREST;

        return levels[level].sample3DOffset(sampler, levelFilter, s, t, r, offset);
    }

    case Sampler::NEAREST_MIPMAP_LINEAR:
    case Sampler::LINEAR_MIPMAP_LINEAR:
    {
        if (minLodParams != nullptr && !minLodParams->intTexCoord)
            lod = de::max(lod, minLodRelative);

        int maxLevel = (int)numLevels - 1;
        int level0   = deClamp32((int)deFloatFloor(lod), 0, maxLevel);
        int level1   = de::min(maxLevel, level0 + 1);
        Sampler::FilterMode levelFilter =
            (filterMode == Sampler::LINEAR_MIPMAP_LINEAR) ? Sampler::LINEAR : Sampler::NEAREST;
        float f      = deFloatFrac(lod);
        tcu::Vec4 t0 = levels[level0].sample3DOffset(sampler, levelFilter, s, t, r, offset);
        tcu::Vec4 t1 = levels[level1].sample3DOffset(sampler, levelFilter, s, t, r, offset);

        return t0 * (1.0f - f) + t1 * f;
    }

    default:
        DE_ASSERT(false);
        return Vec4(0.0f);
    }
}

float sampleLevelArray1DCompare(const ConstPixelBufferAccess *levels, int numLevels, const Sampler &sampler, float ref,
                                float s, float lod, const IVec2 &offset)
{
    bool magnified                 = lod <= sampler.lodThreshold;
    Sampler::FilterMode filterMode = magnified ? sampler.magFilter : sampler.minFilter;

    switch (filterMode)
    {
    case Sampler::NEAREST:
        return levels[0].sample1DCompare(sampler, filterMode, ref, s, offset);
    case Sampler::LINEAR:
        return levels[0].sample1DCompare(sampler, filterMode, ref, s, offset);

    case Sampler::NEAREST_MIPMAP_NEAREST:
    case Sampler::LINEAR_MIPMAP_NEAREST:
    {
        int maxLevel = (int)numLevels - 1;
        int level    = deClamp32((int)deFloatCeil(lod + 0.5f) - 1, 0, maxLevel);
        Sampler::FilterMode levelFilter =
            (filterMode == Sampler::LINEAR_MIPMAP_NEAREST) ? Sampler::LINEAR : Sampler::NEAREST;

        return levels[level].sample1DCompare(sampler, levelFilter, ref, s, offset);
    }

    case Sampler::NEAREST_MIPMAP_LINEAR:
    case Sampler::LINEAR_MIPMAP_LINEAR:
    {
        int maxLevel = (int)numLevels - 1;
        int level0   = deClamp32((int)deFloatFloor(lod), 0, maxLevel);
        int level1   = de::min(maxLevel, level0 + 1);
        Sampler::FilterMode levelFilter =
            (filterMode == Sampler::LINEAR_MIPMAP_LINEAR) ? Sampler::LINEAR : Sampler::NEAREST;
        float f  = deFloatFrac(lod);
        float t0 = levels[level0].sample1DCompare(sampler, levelFilter, ref, s, offset);
        float t1 = levels[level1].sample1DCompare(sampler, levelFilter, ref, s, offset);

        return t0 * (1.0f - f) + t1 * f;
    }

    default:
        DE_ASSERT(false);
        return 0.0f;
    }
}

float sampleLevelArray2DCompare(const ConstPixelBufferAccess *levels, int numLevels, const Sampler &sampler, float ref,
                                float s, float t, float lod, const IVec3 &offset)
{
    bool magnified                 = lod <= sampler.lodThreshold;
    Sampler::FilterMode filterMode = magnified ? sampler.magFilter : sampler.minFilter;

    switch (filterMode)
    {
    case Sampler::NEAREST:
        return levels[0].sample2DCompare(sampler, filterMode, ref, s, t, offset);
    case Sampler::LINEAR:
        return levels[0].sample2DCompare(sampler, filterMode, ref, s, t, offset);

    case Sampler::NEAREST_MIPMAP_NEAREST:
    case Sampler::LINEAR_MIPMAP_NEAREST:
    {
        int maxLevel = (int)numLevels - 1;
        int level    = deClamp32((int)deFloatCeil(lod + 0.5f) - 1, 0, maxLevel);
        Sampler::FilterMode levelFilter =
            (filterMode == Sampler::LINEAR_MIPMAP_NEAREST) ? Sampler::LINEAR : Sampler::NEAREST;

        return levels[level].sample2DCompare(sampler, levelFilter, ref, s, t, offset);
    }

    case Sampler::NEAREST_MIPMAP_LINEAR:
    case Sampler::LINEAR_MIPMAP_LINEAR:
    {
        int maxLevel = (int)numLevels - 1;
        int level0   = deClamp32((int)deFloatFloor(lod), 0, maxLevel);
        int level1   = de::min(maxLevel, level0 + 1);
        Sampler::FilterMode levelFilter =
            (filterMode == Sampler::LINEAR_MIPMAP_LINEAR) ? Sampler::LINEAR : Sampler::NEAREST;
        float f  = deFloatFrac(lod);
        float t0 = levels[level0].sample2DCompare(sampler, levelFilter, ref, s, t, offset);
        float t1 = levels[level1].sample2DCompare(sampler, levelFilter, ref, s, t, offset);

        return t0 * (1.0f - f) + t1 * f;
    }

    default:
        DE_ASSERT(false);
        return 0.0f;
    }
}

static Vec4 fetchGatherArray2DOffsets(const ConstPixelBufferAccess &src, const Sampler &sampler, float s, float t,
                                      int depth, int componentNdx, const IVec2 (&offsets)[4])
{
    DE_ASSERT(de::inBounds(componentNdx, 0, 4));

    const int w   = src.getWidth();
    const int h   = src.getHeight();
    const float u = unnormalize(sampler.wrapS, s, w);
    const float v = unnormalize(sampler.wrapT, t, h);
    const int x0  = deFloorFloatToInt32(u - 0.5f);
    const int y0  = deFloorFloatToInt32(v - 0.5f);

    Vec4 result;

    for (int i = 0; i < 4; i++)
    {
        const int sampleX = wrap(sampler.wrapS, x0 + offsets[i].x(), w);
        const int sampleY = wrap(sampler.wrapT, y0 + offsets[i].y(), h);
        Vec4 pixel;

        if (deInBounds32(sampleX, 0, w) && deInBounds32(sampleY, 0, h))
            pixel = lookup(src, sampleX, sampleY, depth);
        else
            pixel = lookupBorder(src.getFormat(), sampler);

        result[i] = pixel[componentNdx];
    }

    return result;
}

Vec4 gatherArray2DOffsets(const ConstPixelBufferAccess &src, const Sampler &sampler, float s, float t, int depth,
                          int componentNdx, const IVec2 (&offsets)[4])
{
    DE_ASSERT(sampler.compare == Sampler::COMPAREMODE_NONE);
    DE_ASSERT(de::inBounds(componentNdx, 0, 4));

    return fetchGatherArray2DOffsets(src, sampler, s, t, depth, componentNdx, offsets);
}

Vec4 gatherArray2DOffsetsCompare(const ConstPixelBufferAccess &src, const Sampler &sampler, float ref, float s, float t,
                                 int depth, const IVec2 (&offsets)[4])
{
    DE_ASSERT(sampler.compare != Sampler::COMPAREMODE_NONE);
    DE_ASSERT(src.getFormat().order == TextureFormat::D || src.getFormat().order == TextureFormat::DS);
    DE_ASSERT(sampler.compareChannel == 0);

    const bool isFixedPoint = isFixedPointDepthTextureFormat(src.getFormat());
    const Vec4 gathered     = fetchGatherArray2DOffsets(src, sampler, s, t, depth, 0 /* component 0: depth */, offsets);
    Vec4 result;

    for (int i = 0; i < 4; i++)
        result[i] = execCompare(gathered, sampler.compare, i, ref, isFixedPoint);

    return result;
}

static Vec4 sampleCubeSeamlessNearest(const ConstPixelBufferAccess &faceAccess, const Sampler &sampler, float s,
                                      float t, int depth)
{
    Sampler clampingSampler = sampler;
    clampingSampler.wrapS   = Sampler::CLAMP_TO_EDGE;
    clampingSampler.wrapT   = Sampler::CLAMP_TO_EDGE;
    return faceAccess.sample2D(clampingSampler, Sampler::NEAREST, s, t, depth);
}

CubeFace selectCubeFace(const Vec3 &coords)
{
    const float x  = coords.x();
    const float y  = coords.y();
    const float z  = coords.z();
    const float ax = deFloatAbs(x);
    const float ay = deFloatAbs(y);
    const float az = deFloatAbs(z);

    if (ay < ax && az < ax)
        return x >= 0.0f ? CUBEFACE_POSITIVE_X : CUBEFACE_NEGATIVE_X;
    else if (ax < ay && az < ay)
        return y >= 0.0f ? CUBEFACE_POSITIVE_Y : CUBEFACE_NEGATIVE_Y;
    else if (ax < az && ay < az)
        return z >= 0.0f ? CUBEFACE_POSITIVE_Z : CUBEFACE_NEGATIVE_Z;
    else
    {
        // Some of the components are equal. Use tie-breaking rule.
        if (ax == ay)
        {
            if (ax < az)
                return z >= 0.0f ? CUBEFACE_POSITIVE_Z : CUBEFACE_NEGATIVE_Z;
            else
                return x >= 0.0f ? CUBEFACE_POSITIVE_X : CUBEFACE_NEGATIVE_X;
        }
        else if (ax == az)
        {
            if (az < ay)
                return y >= 0.0f ? CUBEFACE_POSITIVE_Y : CUBEFACE_NEGATIVE_Y;
            else
                return z >= 0.0f ? CUBEFACE_POSITIVE_Z : CUBEFACE_NEGATIVE_Z;
        }
        else if (ay == az)
        {
            if (ay < ax)
                return x >= 0.0f ? CUBEFACE_POSITIVE_X : CUBEFACE_NEGATIVE_X;
            else
                return y >= 0.0f ? CUBEFACE_POSITIVE_Y : CUBEFACE_NEGATIVE_Y;
        }
        else
            return x >= 0.0f ? CUBEFACE_POSITIVE_X : CUBEFACE_NEGATIVE_X;
    }
}

Vec2 projectToFace(CubeFace face, const Vec3 &coord)
{
    const float rx = coord.x();
    const float ry = coord.y();
    const float rz = coord.z();
    float sc       = 0.0f;
    float tc       = 0.0f;
    float ma       = 0.0f;
    float s;
    float t;

    switch (face)
    {
    case CUBEFACE_NEGATIVE_X:
        sc = +rz;
        tc = -ry;
        ma = -rx;
        break;
    case CUBEFACE_POSITIVE_X:
        sc = -rz;
        tc = -ry;
        ma = +rx;
        break;
    case CUBEFACE_NEGATIVE_Y:
        sc = +rx;
        tc = -rz;
        ma = -ry;
        break;
    case CUBEFACE_POSITIVE_Y:
        sc = +rx;
        tc = +rz;
        ma = +ry;
        break;
    case CUBEFACE_NEGATIVE_Z:
        sc = -rx;
        tc = -ry;
        ma = -rz;
        break;
    case CUBEFACE_POSITIVE_Z:
        sc = +rx;
        tc = -ry;
        ma = +rz;
        break;
    default:
        DE_ASSERT(false);
    }

    if (fabs(ma) < FLT_EPSILON)
    {
        return Vec2(0.0f);
    }

    // Compute s, t
    s = ((sc / ma) + 1.0f) / 2.0f;
    t = ((tc / ma) + 1.0f) / 2.0f;

    return Vec2(s, t);
}

CubeFaceFloatCoords getCubeFaceCoords(const Vec3 &coords)
{
    const CubeFace face = selectCubeFace(coords);
    return CubeFaceFloatCoords(face, projectToFace(face, coords));
}

// Checks if origCoords.coords is in bounds defined by size; if not, return a CubeFaceIntCoords with face set to the appropriate neighboring face and coords transformed accordingly.
// \note If both x and y in origCoords.coords are out of bounds, this returns with face CUBEFACE_LAST, signifying that there is no unique neighboring face.
CubeFaceIntCoords remapCubeEdgeCoords(const CubeFaceIntCoords &origCoords, int size)
{
    bool uInBounds = de::inBounds(origCoords.s, 0, size);
    bool vInBounds = de::inBounds(origCoords.t, 0, size);

    if (uInBounds && vInBounds)
        return origCoords;

    if (!uInBounds && !vInBounds)
        return CubeFaceIntCoords(CUBEFACE_LAST, -1, -1);

    IVec2 coords(wrap(Sampler::CLAMP_TO_BORDER, origCoords.s, size),
                 wrap(Sampler::CLAMP_TO_BORDER, origCoords.t, size));
    IVec3 canonizedCoords;

    // Map the uv coordinates to canonized 3d coordinates.

    switch (origCoords.face)
    {
    case CUBEFACE_NEGATIVE_X:
        canonizedCoords = IVec3(0, size - 1 - coords.y(), coords.x());
        break;
    case CUBEFACE_POSITIVE_X:
        canonizedCoords = IVec3(size - 1, size - 1 - coords.y(), size - 1 - coords.x());
        break;
    case CUBEFACE_NEGATIVE_Y:
        canonizedCoords = IVec3(coords.x(), 0, size - 1 - coords.y());
        break;
    case CUBEFACE_POSITIVE_Y:
        canonizedCoords = IVec3(coords.x(), size - 1, coords.y());
        break;
    case CUBEFACE_NEGATIVE_Z:
        canonizedCoords = IVec3(size - 1 - coords.x(), size - 1 - coords.y(), 0);
        break;
    case CUBEFACE_POSITIVE_Z:
        canonizedCoords = IVec3(coords.x(), size - 1 - coords.y(), size - 1);
        break;
    default:
        DE_ASSERT(false);
    }

    // Find an appropriate face to re-map the coordinates to.

    if (canonizedCoords.x() == -1)
        return CubeFaceIntCoords(CUBEFACE_NEGATIVE_X, IVec2(canonizedCoords.z(), size - 1 - canonizedCoords.y()));

    if (canonizedCoords.x() == size)
        return CubeFaceIntCoords(CUBEFACE_POSITIVE_X,
                                 IVec2(size - 1 - canonizedCoords.z(), size - 1 - canonizedCoords.y()));

    if (canonizedCoords.y() == -1)
        return CubeFaceIntCoords(CUBEFACE_NEGATIVE_Y, IVec2(canonizedCoords.x(), size - 1 - canonizedCoords.z()));

    if (canonizedCoords.y() == size)
        return CubeFaceIntCoords(CUBEFACE_POSITIVE_Y, IVec2(canonizedCoords.x(), canonizedCoords.z()));

    if (canonizedCoords.z() == -1)
        return CubeFaceIntCoords(CUBEFACE_NEGATIVE_Z,
                                 IVec2(size - 1 - canonizedCoords.x(), size - 1 - canonizedCoords.y()));

    if (canonizedCoords.z() == size)
        return CubeFaceIntCoords(CUBEFACE_POSITIVE_Z, IVec2(canonizedCoords.x(), size - 1 - canonizedCoords.y()));

    DE_ASSERT(false);
    return CubeFaceIntCoords(CUBEFACE_LAST, IVec2(-1));
}

static void getCubeLinearSamples(const ConstPixelBufferAccess (&faceAccesses)[CUBEFACE_LAST], CubeFace baseFace,
                                 float u, float v, int depth, Vec4 (&dst)[4])
{
    DE_ASSERT(faceAccesses[0].getWidth() == faceAccesses[0].getHeight());
    int size                  = faceAccesses[0].getWidth();
    int x0                    = deFloorFloatToInt32(u - 0.5f);
    int x1                    = x0 + 1;
    int y0                    = deFloorFloatToInt32(v - 0.5f);
    int y1                    = y0 + 1;
    IVec2 baseSampleCoords[4] = {IVec2(x0, y0), IVec2(x1, y0), IVec2(x0, y1), IVec2(x1, y1)};
    Vec4 sampleColors[4];
    bool hasBothCoordsOutOfBounds
        [4]; //!< Whether correctCubeFace() returns CUBEFACE_LAST, i.e. both u and v are out of bounds.

    // Find correct faces and coordinates for out-of-bounds sample coordinates.

    for (int i = 0; i < 4; i++)
    {
        CubeFaceIntCoords coords    = remapCubeEdgeCoords(CubeFaceIntCoords(baseFace, baseSampleCoords[i]), size);
        hasBothCoordsOutOfBounds[i] = coords.face == CUBEFACE_LAST;
        if (!hasBothCoordsOutOfBounds[i])
            sampleColors[i] = lookup(faceAccesses[coords.face], coords.s, coords.t, depth);
    }

    // If a sample was out of bounds in both u and v, we get its color from the average of the three other samples.
    // \note This averaging behavior is not required by the GLES3 spec (though it is recommended). GLES3 spec only
    //         requires that if the three other samples all have the same color, then the doubly-out-of-bounds sample
    //         must have this color as well.

    {
        int bothOutOfBoundsNdx = -1;
        for (int i = 0; i < 4; i++)
        {
            if (hasBothCoordsOutOfBounds[i])
            {
                DE_ASSERT(bothOutOfBoundsNdx < 0); // Only one sample can be out of bounds in both u and v.
                bothOutOfBoundsNdx = i;
            }
        }
        if (bothOutOfBoundsNdx != -1)
        {
            sampleColors[bothOutOfBoundsNdx] = Vec4(0.0f);
            for (int i = 0; i < 4; i++)
                if (i != bothOutOfBoundsNdx)
                    sampleColors[bothOutOfBoundsNdx] += sampleColors[i];

            sampleColors[bothOutOfBoundsNdx] = sampleColors[bothOutOfBoundsNdx] * (1.0f / 3.0f);
        }
    }

    for (int i = 0; i < DE_LENGTH_OF_ARRAY(sampleColors); i++)
        dst[i] = sampleColors[i];
}

// \todo [2014-02-19 pyry] Optimize faceAccesses
static Vec4 sampleCubeSeamlessLinear(const ConstPixelBufferAccess (&faceAccesses)[CUBEFACE_LAST], CubeFace baseFace,
                                     const Sampler &sampler, float s, float t, int depth)
{
    DE_ASSERT(faceAccesses[0].getWidth() == faceAccesses[0].getHeight());

    int size = faceAccesses[0].getWidth();
    // Non-normalized coordinates.
    float u = s;
    float v = t;

    if (sampler.normalizedCoords)
    {
        u = unnormalize(sampler.wrapS, s, size);
        v = unnormalize(sampler.wrapT, t, size);
    }

    // Get sample colors.

    Vec4 sampleColors[4];
    getCubeLinearSamples(faceAccesses, baseFace, u, v, depth, sampleColors);

    // Interpolate.

    float a = deFloatFrac(u - 0.5f);
    float b = deFloatFrac(v - 0.5f);

    return (sampleColors[0] * (1.0f - a) * (1.0f - b)) + (sampleColors[1] * (a) * (1.0f - b)) +
           (sampleColors[2] * (1.0f - a) * (b)) + (sampleColors[3] * (a) * (b));
}

static Vec4 sampleLevelArrayCubeSeamless(const ConstPixelBufferAccess *const (&faces)[CUBEFACE_LAST], int numLevels,
                                         CubeFace face, const Sampler &sampler, float s, float t, int depth, float lod,
                                         ImageViewMinLodParams *minLodParams)
{
    // minLodRelative is used to calculate the image level to sample from, when VK_EXT_image_view_min_lod extension is enabled.
    // The value is relative to baseLevel as the Texture*View was created as the baseLevel being level[0].
    const float minLodRelative =
        (minLodParams != nullptr) ? getImageViewMinLod(minLodParams->minLod) - (float)minLodParams->baseLevel : 0.0f;
    bool magnified                 = lod <= sampler.lodThreshold;
    Sampler::FilterMode filterMode = magnified ? sampler.magFilter : sampler.minFilter;

    // VK_EXT_image_view_min_lod: Integer Texel Coordinates case (with robustness2 supported)
    if (minLodParams != nullptr && minLodParams->intTexCoord)
    {
        if (lod < deFloatFloor(minLodRelative) || lod >= (float)(numLevels - 1))
            return Vec4(0.0f);

        if (s < 0.0f || s > 1.0f || t < 0.0f || t > 1.0f)
            return Vec4(0.0f);
    }

    switch (filterMode)
    {
    case Sampler::NEAREST:
    {
        bool isLinearMipmapMode = magnified && tcu::isSamplerMipmapModeLinear(sampler.minFilter);
        const int maxLevel      = (int)numLevels - 1;
        const int level0        = isLinearMipmapMode ? (int)deFloatFloor(minLodRelative) :
                                                       deClamp32((int)deFloatCeil(minLodRelative + 0.5f) - 1, 0, maxLevel);
        tcu::Vec4 t0            = sampleCubeSeamlessNearest(faces[face][level0], sampler, s, t, depth);

        if (!isLinearMipmapMode)
            return t0;

        const float frac = deFloatFrac(minLodRelative);
        const int level1 = de::min(level0 + 1, maxLevel);
        tcu::Vec4 t1     = sampleCubeSeamlessNearest(faces[face][level1], sampler, s, t, depth);
        return t0 * (1.0f - frac) + t1 * frac;
    }

    case Sampler::LINEAR:
    {
        bool cond =
            sampler.minFilter == Sampler::NEAREST_MIPMAP_LINEAR || sampler.minFilter == Sampler::LINEAR_MIPMAP_LINEAR;
        const int index = cond ? (int)deFloatFloor(minLodRelative) : ((int)deFloatCeil(minLodRelative + 0.5f) - 1u);
        ConstPixelBufferAccess faceAccesses[CUBEFACE_LAST];
        for (int i = 0; i < (int)CUBEFACE_LAST; i++)
            faceAccesses[i] = faces[i][index];

        Vec4 result = sampleCubeSeamlessLinear(faceAccesses, face, sampler, s, t, depth);

        if (cond && ((index + 1) < numLevels) && deFloatFrac(minLodRelative) != 0.0f)
        {
            // In case of a minLodRelative value with fractional part, we need to ponderate the different sample of N level
            // and sample for level N+1 accordingly.
            result = result * (1.0f - deFloatFrac(minLodRelative));

            ConstPixelBufferAccess faceAccessesNext[CUBEFACE_LAST];
            for (int i = 0; i < (int)CUBEFACE_LAST; i++)
                faceAccessesNext[i] = faces[i][index + 1];
            result += sampleCubeSeamlessLinear(faceAccesses, face, sampler, s, t, depth) * deFloatFrac(minLodRelative);
        }
        return result;
    }

    case Sampler::NEAREST_MIPMAP_NEAREST:
    case Sampler::LINEAR_MIPMAP_NEAREST:
    {
        if (minLodParams != nullptr && !minLodParams->intTexCoord)
            lod = de::max(lod, minLodRelative);

        int maxLevel = (int)numLevels - 1;
        int level    = deClamp32((int)deFloatCeil(lod + 0.5f) - 1, 0, maxLevel);
        Sampler::FilterMode levelFilter =
            (filterMode == Sampler::LINEAR_MIPMAP_NEAREST) ? Sampler::LINEAR : Sampler::NEAREST;

        if (levelFilter == Sampler::NEAREST)
            return sampleCubeSeamlessNearest(faces[face][level], sampler, s, t, depth);
        else
        {
            DE_ASSERT(levelFilter == Sampler::LINEAR);

            ConstPixelBufferAccess faceAccesses[CUBEFACE_LAST];
            for (int i = 0; i < (int)CUBEFACE_LAST; i++)
                faceAccesses[i] = faces[i][level];

            return sampleCubeSeamlessLinear(faceAccesses, face, sampler, s, t, depth);
        }
    }

    case Sampler::NEAREST_MIPMAP_LINEAR:
    case Sampler::LINEAR_MIPMAP_LINEAR:
    {
        if (minLodParams != nullptr && !minLodParams->intTexCoord)
            lod = de::max(lod, minLodRelative);

        int maxLevel = (int)numLevels - 1;
        int level0   = deClamp32((int)deFloatFloor(lod), 0, maxLevel);
        int level1   = de::min(maxLevel, level0 + 1);
        Sampler::FilterMode levelFilter =
            (filterMode == Sampler::LINEAR_MIPMAP_LINEAR) ? Sampler::LINEAR : Sampler::NEAREST;
        float f = deFloatFrac(lod);
        Vec4 t0;
        Vec4 t1;

        if (levelFilter == Sampler::NEAREST)
        {
            t0 = sampleCubeSeamlessNearest(faces[face][level0], sampler, s, t, depth);
            t1 = sampleCubeSeamlessNearest(faces[face][level1], sampler, s, t, depth);
        }
        else
        {
            DE_ASSERT(levelFilter == Sampler::LINEAR);

            ConstPixelBufferAccess faceAccesses0[CUBEFACE_LAST];
            ConstPixelBufferAccess faceAccesses1[CUBEFACE_LAST];
            for (int i = 0; i < (int)CUBEFACE_LAST; i++)
            {
                faceAccesses0[i] = faces[i][level0];
                faceAccesses1[i] = faces[i][level1];
            }

            t0 = sampleCubeSeamlessLinear(faceAccesses0, face, sampler, s, t, depth);
            t1 = sampleCubeSeamlessLinear(faceAccesses1, face, sampler, s, t, depth);
        }

        return t0 * (1.0f - f) + t1 * f;
    }

    default:
        DE_ASSERT(false);
        return Vec4(0.0f);
    }
}

static float sampleCubeSeamlessNearestCompare(const ConstPixelBufferAccess &faceAccess, const Sampler &sampler,
                                              float ref, float s, float t, int depth = 0)
{
    Sampler clampingSampler = sampler;
    clampingSampler.wrapS   = Sampler::CLAMP_TO_EDGE;
    clampingSampler.wrapT   = Sampler::CLAMP_TO_EDGE;
    return faceAccess.sample2DCompare(clampingSampler, Sampler::NEAREST, ref, s, t, IVec3(0, 0, depth));
}

static float sampleCubeSeamlessLinearCompare(const ConstPixelBufferAccess (&faceAccesses)[CUBEFACE_LAST],
                                             CubeFace baseFace, const Sampler &sampler, float ref, float s, float t)
{
    DE_ASSERT(faceAccesses[0].getWidth() == faceAccesses[0].getHeight());

    int size = faceAccesses[0].getWidth();
    // Non-normalized coordinates.
    float u = s;
    float v = t;

    if (sampler.normalizedCoords)
    {
        u = unnormalize(sampler.wrapS, s, size);
        v = unnormalize(sampler.wrapT, t, size);
    }

    int x0                    = deFloorFloatToInt32(u - 0.5f);
    int x1                    = x0 + 1;
    int y0                    = deFloorFloatToInt32(v - 0.5f);
    int y1                    = y0 + 1;
    IVec2 baseSampleCoords[4] = {IVec2(x0, y0), IVec2(x1, y0), IVec2(x0, y1), IVec2(x1, y1)};
    float sampleRes[4];
    bool hasBothCoordsOutOfBounds
        [4]; //!< Whether correctCubeFace() returns CUBEFACE_LAST, i.e. both u and v are out of bounds.

    // Find correct faces and coordinates for out-of-bounds sample coordinates.

    for (int i = 0; i < 4; i++)
    {
        CubeFaceIntCoords coords    = remapCubeEdgeCoords(CubeFaceIntCoords(baseFace, baseSampleCoords[i]), size);
        hasBothCoordsOutOfBounds[i] = coords.face == CUBEFACE_LAST;

        if (!hasBothCoordsOutOfBounds[i])
        {
            const bool isFixedPointDepth = isFixedPointDepthTextureFormat(faceAccesses[coords.face].getFormat());

            sampleRes[i] = execCompare(faceAccesses[coords.face].getPixel(coords.s, coords.t), sampler.compare,
                                       sampler.compareChannel, ref, isFixedPointDepth);
        }
    }

    // If a sample was out of bounds in both u and v, we get its color from the average of the three other samples.
    // \note This averaging behavior is not required by the GLES3 spec (though it is recommended). GLES3 spec only
    //         requires that if the three other samples all have the same color, then the doubly-out-of-bounds sample
    //         must have this color as well.

    {
        int bothOutOfBoundsNdx = -1;
        for (int i = 0; i < 4; i++)
        {
            if (hasBothCoordsOutOfBounds[i])
            {
                DE_ASSERT(bothOutOfBoundsNdx < 0); // Only one sample can be out of bounds in both u and v.
                bothOutOfBoundsNdx = i;
            }
        }
        if (bothOutOfBoundsNdx != -1)
        {
            sampleRes[bothOutOfBoundsNdx] = 0.0f;
            for (int i = 0; i < 4; i++)
                if (i != bothOutOfBoundsNdx)
                    sampleRes[bothOutOfBoundsNdx] += sampleRes[i];

            sampleRes[bothOutOfBoundsNdx] = sampleRes[bothOutOfBoundsNdx] * (1.0f / 3.0f);
        }
    }

    // Interpolate.

    float a = deFloatFrac(u - 0.5f);
    float b = deFloatFrac(v - 0.5f);

    return (sampleRes[0] * (1.0f - a) * (1.0f - b)) + (sampleRes[1] * (a) * (1.0f - b)) +
           (sampleRes[2] * (1.0f - a) * (b)) + (sampleRes[3] * (a) * (b));
}

static float sampleLevelArrayCubeSeamlessCompare(const ConstPixelBufferAccess *const (&faces)[CUBEFACE_LAST],
                                                 int numLevels, CubeFace face, const Sampler &sampler, float ref,
                                                 float s, float t, float lod)
{
    bool magnified                 = lod <= sampler.lodThreshold;
    Sampler::FilterMode filterMode = magnified ? sampler.magFilter : sampler.minFilter;

    switch (filterMode)
    {
    case Sampler::NEAREST:
        return sampleCubeSeamlessNearestCompare(faces[face][0], sampler, ref, s, t);

    case Sampler::LINEAR:
    {
        ConstPixelBufferAccess faceAccesses[CUBEFACE_LAST];
        for (int i = 0; i < (int)CUBEFACE_LAST; i++)
            faceAccesses[i] = faces[i][0];

        return sampleCubeSeamlessLinearCompare(faceAccesses, face, sampler, ref, s, t);
    }

    case Sampler::NEAREST_MIPMAP_NEAREST:
    case Sampler::LINEAR_MIPMAP_NEAREST:
    {
        int maxLevel = (int)numLevels - 1;
        int level    = deClamp32((int)deFloatCeil(lod + 0.5f) - 1, 0, maxLevel);
        Sampler::FilterMode levelFilter =
            (filterMode == Sampler::LINEAR_MIPMAP_NEAREST) ? Sampler::LINEAR : Sampler::NEAREST;

        if (levelFilter == Sampler::NEAREST)
            return sampleCubeSeamlessNearestCompare(faces[face][level], sampler, ref, s, t);
        else
        {
            DE_ASSERT(levelFilter == Sampler::LINEAR);

            ConstPixelBufferAccess faceAccesses[CUBEFACE_LAST];
            for (int i = 0; i < (int)CUBEFACE_LAST; i++)
                faceAccesses[i] = faces[i][level];

            return sampleCubeSeamlessLinearCompare(faceAccesses, face, sampler, ref, s, t);
        }
    }

    case Sampler::NEAREST_MIPMAP_LINEAR:
    case Sampler::LINEAR_MIPMAP_LINEAR:
    {
        int maxLevel = (int)numLevels - 1;
        int level0   = deClamp32((int)deFloatFloor(lod), 0, maxLevel);
        int level1   = de::min(maxLevel, level0 + 1);
        Sampler::FilterMode levelFilter =
            (filterMode == Sampler::LINEAR_MIPMAP_LINEAR) ? Sampler::LINEAR : Sampler::NEAREST;
        float f = deFloatFrac(lod);
        float t0;
        float t1;

        if (levelFilter == Sampler::NEAREST)
        {
            t0 = sampleCubeSeamlessNearestCompare(faces[face][level0], sampler, ref, s, t);
            t1 = sampleCubeSeamlessNearestCompare(faces[face][level1], sampler, ref, s, t);
        }
        else
        {
            DE_ASSERT(levelFilter == Sampler::LINEAR);

            ConstPixelBufferAccess faceAccesses0[CUBEFACE_LAST];
            ConstPixelBufferAccess faceAccesses1[CUBEFACE_LAST];
            for (int i = 0; i < (int)CUBEFACE_LAST; i++)
            {
                faceAccesses0[i] = faces[i][level0];
                faceAccesses1[i] = faces[i][level1];
            }

            t0 = sampleCubeSeamlessLinearCompare(faceAccesses0, face, sampler, ref, s, t);
            t1 = sampleCubeSeamlessLinearCompare(faceAccesses1, face, sampler, ref, s, t);
        }

        return t0 * (1.0f - f) + t1 * f;
    }

    default:
        DE_ASSERT(false);
        return 0.0f;
    }
}

// Cube map array sampling

static inline ConstPixelBufferAccess getCubeArrayFaceAccess(const ConstPixelBufferAccess *const levels, int levelNdx,
                                                            int slice, CubeFace face)
{
    const ConstPixelBufferAccess &level = levels[levelNdx];
    const int depth                     = (slice * 6) + getCubeArrayFaceIndex(face);

    return getSubregion(level, 0, 0, depth, level.getWidth(), level.getHeight(), 1);
}

static Vec4 sampleCubeArraySeamless(const ConstPixelBufferAccess *const levels, int numLevels, int slice, CubeFace face,
                                    const Sampler &sampler, float s, float t, float lod)
{
    const int faceDepth                  = (slice * 6) + getCubeArrayFaceIndex(face);
    const bool magnified                 = lod <= sampler.lodThreshold;
    const Sampler::FilterMode filterMode = magnified ? sampler.magFilter : sampler.minFilter;

    switch (filterMode)
    {
    case Sampler::NEAREST:
        return sampleCubeSeamlessNearest(levels[0], sampler, s, t, faceDepth);

    case Sampler::LINEAR:
    {
        ConstPixelBufferAccess faceAccesses[CUBEFACE_LAST];
        for (int i = 0; i < (int)CUBEFACE_LAST; i++)
            faceAccesses[i] = getCubeArrayFaceAccess(levels, 0, slice, (CubeFace)i);

        return sampleCubeSeamlessLinear(faceAccesses, face, sampler, s, t, 0);
    }

    case Sampler::NEAREST_MIPMAP_NEAREST:
    case Sampler::LINEAR_MIPMAP_NEAREST:
    {
        int maxLevel = (int)numLevels - 1;
        int level    = deClamp32((int)deFloatCeil(lod + 0.5f) - 1, 0, maxLevel);
        Sampler::FilterMode levelFilter =
            (filterMode == Sampler::LINEAR_MIPMAP_NEAREST) ? Sampler::LINEAR : Sampler::NEAREST;

        if (levelFilter == Sampler::NEAREST)
            return sampleCubeSeamlessNearest(levels[level], sampler, s, t, faceDepth);
        else
        {
            DE_ASSERT(levelFilter == Sampler::LINEAR);

            ConstPixelBufferAccess faceAccesses[CUBEFACE_LAST];
            for (int i = 0; i < (int)CUBEFACE_LAST; i++)
                faceAccesses[i] = getCubeArrayFaceAccess(levels, level, slice, (CubeFace)i);

            return sampleCubeSeamlessLinear(faceAccesses, face, sampler, s, t, 0);
        }
    }

    case Sampler::NEAREST_MIPMAP_LINEAR:
    case Sampler::LINEAR_MIPMAP_LINEAR:
    {
        int maxLevel = (int)numLevels - 1;
        int level0   = deClamp32((int)deFloatFloor(lod), 0, maxLevel);
        int level1   = de::min(maxLevel, level0 + 1);
        Sampler::FilterMode levelFilter =
            (filterMode == Sampler::LINEAR_MIPMAP_LINEAR) ? Sampler::LINEAR : Sampler::NEAREST;
        float f = deFloatFrac(lod);
        Vec4 t0;
        Vec4 t1;

        if (levelFilter == Sampler::NEAREST)
        {
            t0 = sampleCubeSeamlessNearest(levels[level0], sampler, s, t, faceDepth);
            t1 = sampleCubeSeamlessNearest(levels[level1], sampler, s, t, faceDepth);
        }
        else
        {
            DE_ASSERT(levelFilter == Sampler::LINEAR);

            ConstPixelBufferAccess faceAccesses0[CUBEFACE_LAST];
            ConstPixelBufferAccess faceAccesses1[CUBEFACE_LAST];
            for (int i = 0; i < (int)CUBEFACE_LAST; i++)
            {
                faceAccesses0[i] = getCubeArrayFaceAccess(levels, level0, slice, (CubeFace)i);
                faceAccesses1[i] = getCubeArrayFaceAccess(levels, level1, slice, (CubeFace)i);
            }

            t0 = sampleCubeSeamlessLinear(faceAccesses0, face, sampler, s, t, 0);
            t1 = sampleCubeSeamlessLinear(faceAccesses1, face, sampler, s, t, 0);
        }

        return t0 * (1.0f - f) + t1 * f;
    }

    default:
        DE_ASSERT(false);
        return Vec4(0.0f);
    }
}

static float sampleCubeArraySeamlessCompare(const ConstPixelBufferAccess *const levels, int numLevels, int slice,
                                            CubeFace face, const Sampler &sampler, float ref, float s, float t,
                                            float lod)
{
    const int faceDepth            = (slice * 6) + getCubeArrayFaceIndex(face);
    const bool magnified           = lod <= sampler.lodThreshold;
    Sampler::FilterMode filterMode = magnified ? sampler.magFilter : sampler.minFilter;

    switch (filterMode)
    {
    case Sampler::NEAREST:
        return sampleCubeSeamlessNearestCompare(levels[0], sampler, ref, s, t, faceDepth);

    case Sampler::LINEAR:
    {
        ConstPixelBufferAccess faceAccesses[CUBEFACE_LAST];
        for (int i = 0; i < (int)CUBEFACE_LAST; i++)
            faceAccesses[i] = getCubeArrayFaceAccess(levels, 0, slice, (CubeFace)i);

        return sampleCubeSeamlessLinearCompare(faceAccesses, face, sampler, ref, s, t);
    }

    case Sampler::NEAREST_MIPMAP_NEAREST:
    case Sampler::LINEAR_MIPMAP_NEAREST:
    {
        int maxLevel = (int)numLevels - 1;
        int level    = deClamp32((int)deFloatCeil(lod + 0.5f) - 1, 0, maxLevel);
        Sampler::FilterMode levelFilter =
            (filterMode == Sampler::LINEAR_MIPMAP_NEAREST) ? Sampler::LINEAR : Sampler::NEAREST;

        if (levelFilter == Sampler::NEAREST)
            return sampleCubeSeamlessNearestCompare(levels[level], sampler, ref, s, t, faceDepth);
        else
        {
            DE_ASSERT(levelFilter == Sampler::LINEAR);

            ConstPixelBufferAccess faceAccesses[CUBEFACE_LAST];
            for (int i = 0; i < (int)CUBEFACE_LAST; i++)
                faceAccesses[i] = getCubeArrayFaceAccess(levels, level, slice, (CubeFace)i);

            return sampleCubeSeamlessLinearCompare(faceAccesses, face, sampler, ref, s, t);
        }
    }

    case Sampler::NEAREST_MIPMAP_LINEAR:
    case Sampler::LINEAR_MIPMAP_LINEAR:
    {
        int maxLevel = (int)numLevels - 1;
        int level0   = deClamp32((int)deFloatFloor(lod), 0, maxLevel);
        int level1   = de::min(maxLevel, level0 + 1);
        Sampler::FilterMode levelFilter =
            (filterMode == Sampler::LINEAR_MIPMAP_LINEAR) ? Sampler::LINEAR : Sampler::NEAREST;
        float f = deFloatFrac(lod);
        float t0;
        float t1;

        if (levelFilter == Sampler::NEAREST)
        {
            t0 = sampleCubeSeamlessNearestCompare(levels[level0], sampler, ref, s, t, faceDepth);
            t1 = sampleCubeSeamlessNearestCompare(levels[level1], sampler, ref, s, t, faceDepth);
        }
        else
        {
            DE_ASSERT(levelFilter == Sampler::LINEAR);

            ConstPixelBufferAccess faceAccesses0[CUBEFACE_LAST];
            ConstPixelBufferAccess faceAccesses1[CUBEFACE_LAST];
            for (int i = 0; i < (int)CUBEFACE_LAST; i++)
            {
                faceAccesses0[i] = getCubeArrayFaceAccess(levels, level0, slice, (CubeFace)i);
                faceAccesses1[i] = getCubeArrayFaceAccess(levels, level1, slice, (CubeFace)i);
            }

            t0 = sampleCubeSeamlessLinearCompare(faceAccesses0, face, sampler, ref, s, t);
            t1 = sampleCubeSeamlessLinearCompare(faceAccesses1, face, sampler, ref, s, t);
        }

        return t0 * (1.0f - f) + t1 * f;
    }

    default:
        DE_ASSERT(false);
        return 0.0f;
    }
}

inline int computeMipPyramidLevels(int size)
{
    return deLog2Floor32(size) + 1;
}

inline int computeMipPyramidLevels(int width, int height)
{
    return deLog2Floor32(de::max(width, height)) + 1;
}

inline int computeMipPyramidLevels(int width, int height, int depth)
{
    return deLog2Floor32(de::max(width, de::max(height, depth))) + 1;
}

inline int getMipPyramidLevelSize(int baseLevelSize, int levelNdx)
{
    return de::max(baseLevelSize >> levelNdx, 1);
}

// TextureLevelPyramid

TextureLevelPyramid::TextureLevelPyramid(const TextureFormat &format, int numLevels)
    : m_format(format)
    , m_data(numLevels)
    , m_access(numLevels)
{
}

TextureLevelPyramid::TextureLevelPyramid(const TextureLevelPyramid &other)
    : m_format(other.m_format)
    , m_data(other.getNumLevels())
    , m_access(other.getNumLevels())
{
    for (int levelNdx = 0; levelNdx < other.getNumLevels(); levelNdx++)
    {
        if (!other.isLevelEmpty(levelNdx))
        {
            const tcu::ConstPixelBufferAccess &srcLevel = other.getLevel(levelNdx);

            m_data[levelNdx]   = other.m_data[levelNdx];
            m_access[levelNdx] = PixelBufferAccess(srcLevel.getFormat(), srcLevel.getWidth(), srcLevel.getHeight(),
                                                   srcLevel.getDepth(), m_data[levelNdx].getPtr());
        }
    }
}

TextureLevelPyramid &TextureLevelPyramid::operator=(const TextureLevelPyramid &other)
{
    if (this == &other)
        return *this;

    m_format = other.m_format;
    m_data.resize(other.getNumLevels());
    m_access.resize(other.getNumLevels());

    for (int levelNdx = 0; levelNdx < other.getNumLevels(); levelNdx++)
    {
        if (!other.isLevelEmpty(levelNdx))
        {
            const tcu::ConstPixelBufferAccess &srcLevel = other.getLevel(levelNdx);

            m_data[levelNdx]   = other.m_data[levelNdx];
            m_access[levelNdx] = PixelBufferAccess(srcLevel.getFormat(), srcLevel.getWidth(), srcLevel.getHeight(),
                                                   srcLevel.getDepth(), m_data[levelNdx].getPtr());
        }
        else if (!isLevelEmpty(levelNdx))
            clearLevel(levelNdx);
    }

    return *this;
}

TextureLevelPyramid::~TextureLevelPyramid(void)
{
}

void TextureLevelPyramid::allocLevel(int levelNdx, int width, int height, int depth)
{
    const int size = m_format.getPixelSize() * width * height * depth;

    DE_ASSERT(isLevelEmpty(levelNdx));

    m_data[levelNdx].setStorage(size);
    m_access[levelNdx] = PixelBufferAccess(m_format, width, height, depth, m_data[levelNdx].getPtr());
}

void TextureLevelPyramid::clearLevel(int levelNdx)
{
    DE_ASSERT(!isLevelEmpty(levelNdx));

    m_data[levelNdx].clear();
    m_access[levelNdx] = PixelBufferAccess();
}

// Texture1D

Texture1D::Texture1D(const TextureFormat &format, int width)
    : TextureLevelPyramid(format, computeMipPyramidLevels(width))
    , m_width(width)
    , m_view(getNumLevels(), getLevels())
{
}

Texture1D::Texture1D(const Texture1D &other)
    : TextureLevelPyramid(other)
    , m_width(other.m_width)
    , m_view(getNumLevels(), getLevels())
{
}

Texture1D &Texture1D::operator=(const Texture1D &other)
{
    if (this == &other)
        return *this;

    TextureLevelPyramid::operator=(other);

    m_width = other.m_width;
    m_view  = Texture1DView(getNumLevels(), getLevels());

    return *this;
}

Texture1D::~Texture1D(void)
{
}

void Texture1D::allocLevel(int levelNdx)
{
    DE_ASSERT(de::inBounds(levelNdx, 0, getNumLevels()));

    const int width = getMipPyramidLevelSize(m_width, levelNdx);

    TextureLevelPyramid::allocLevel(levelNdx, width, 1, 1);
}

// Texture2D

Texture2D::Texture2D(const TextureFormat &format, int width, int height, bool es2)
    : TextureLevelPyramid(format, computeMipPyramidLevels(width, height))
    , m_yuvTextureUsed(false)
    , m_width(width)
    , m_height(height)
    , m_view(getNumLevels(), getLevels(), es2)
{
}

Texture2D::Texture2D(const TextureFormat &format, int width, int height, int mipmaps)
    : TextureLevelPyramid(format, mipmaps)
    , m_yuvTextureUsed(false)
    , m_width(width)
    , m_height(height)
    , m_view(getNumLevels(), getLevels())
{
}

Texture2D::Texture2D(const Texture2D &other)
    : TextureLevelPyramid(other)
    , m_yuvTextureUsed(other.m_yuvTextureUsed)
    , m_width(other.m_width)
    , m_height(other.m_height)
    , m_view(getNumLevels(), getLevels(), other.getView().isES2())
{
}

Texture2D &Texture2D::operator=(const Texture2D &other)
{
    if (this == &other)
        return *this;

    TextureLevelPyramid::operator=(other);

    m_width          = other.m_width;
    m_height         = other.m_height;
    m_view           = Texture2DView(getNumLevels(), getLevels(), other.getView().isES2());
    m_yuvTextureUsed = other.m_yuvTextureUsed;
    return *this;
}

Texture2D::~Texture2D(void)
{
}

void Texture2D::allocLevel(int levelNdx)
{
    DE_ASSERT(de::inBounds(levelNdx, 0, getNumLevels()));

    const int width  = getMipPyramidLevelSize(m_width, levelNdx);
    const int height = getMipPyramidLevelSize(m_height, levelNdx);

    TextureLevelPyramid::allocLevel(levelNdx, width, height, 1);
}

// TextureCubeView

TextureCubeView::TextureCubeView(void) : m_numLevels(0), m_es2(false), m_minLodParams(nullptr)
{
    for (int ndx = 0; ndx < CUBEFACE_LAST; ndx++)
        m_levels[ndx] = nullptr;
}

TextureCubeView::TextureCubeView(int numLevels, const ConstPixelBufferAccess *const (&levels)[CUBEFACE_LAST], bool es2,
                                 ImageViewMinLodParams *minLodParams)
    : m_numLevels(numLevels)
    , m_es2(es2)
    , m_minLodParams(minLodParams)
{
    for (int ndx = 0; ndx < CUBEFACE_LAST; ndx++)
        m_levels[ndx] = levels[ndx];
}

tcu::Vec4 TextureCubeView::sample(const Sampler &sampler, float s, float t, float r, float lod) const
{
    DE_ASSERT(sampler.compare == Sampler::COMPAREMODE_NONE);

    // Computes (face, s, t).
    const CubeFaceFloatCoords coords = getCubeFaceCoords(Vec3(s, t, r));
    if (sampler.seamlessCubeMap)
        return sampleLevelArrayCubeSeamless(m_levels, m_numLevels, coords.face, sampler, coords.s, coords.t,
                                            0 /* depth */, lod, m_minLodParams);
    else
        return sampleLevelArray2D(m_levels[coords.face], m_numLevels, sampler, coords.s, coords.t, 0 /* depth */, lod,
                                  m_es2, m_minLodParams);
}

float TextureCubeView::sampleCompare(const Sampler &sampler, float ref, float s, float t, float r, float lod) const
{
    DE_ASSERT(sampler.compare != Sampler::COMPAREMODE_NONE);

    // Computes (face, s, t).
    const CubeFaceFloatCoords coords = getCubeFaceCoords(Vec3(s, t, r));
    if (sampler.seamlessCubeMap)
        return sampleLevelArrayCubeSeamlessCompare(m_levels, m_numLevels, coords.face, sampler, ref, coords.s, coords.t,
                                                   lod);
    else
        return sampleLevelArray2DCompare(m_levels[coords.face], m_numLevels, sampler, ref, coords.s, coords.t, lod,
                                         IVec3(0, 0, 0));
}

Vec4 TextureCubeView::gather(const Sampler &sampler, float s, float t, float r, int componentNdx) const
{
    DE_ASSERT(sampler.compare == Sampler::COMPAREMODE_NONE);

    ConstPixelBufferAccess faceAccesses[CUBEFACE_LAST];
    for (int i = 0; i < (int)CUBEFACE_LAST; i++)
        faceAccesses[i] = m_levels[i][0];

    const CubeFaceFloatCoords coords = getCubeFaceCoords(Vec3(s, t, r));
    const int size                   = faceAccesses[0].getWidth();
    // Non-normalized coordinates.
    float u = coords.s;
    float v = coords.t;

    if (sampler.normalizedCoords)
    {
        u = unnormalize(sampler.wrapS, coords.s, size);
        v = unnormalize(sampler.wrapT, coords.t, size);
    }

    Vec4 sampleColors[4];
    getCubeLinearSamples(faceAccesses, coords.face, u, v, 0, sampleColors);

    const int sampleIndices[4] = {2, 3, 1, 0}; // \note Gather returns the samples in a non-obvious order.
    Vec4 result;
    for (int i = 0; i < 4; i++)
        result[i] = sampleColors[sampleIndices[i]][componentNdx];

    return result;
}

Vec4 TextureCubeView::gatherCompare(const Sampler &sampler, float ref, float s, float t, float r) const
{
    DE_ASSERT(sampler.compare != Sampler::COMPAREMODE_NONE);
    DE_ASSERT(m_levels[0][0].getFormat().order == TextureFormat::D ||
              m_levels[0][0].getFormat().order == TextureFormat::DS);
    DE_ASSERT(sampler.compareChannel == 0);

    Sampler noCompareSampler = sampler;
    noCompareSampler.compare = Sampler::COMPAREMODE_NONE;

    const Vec4 gathered     = gather(noCompareSampler, s, t, r, 0 /* component 0: depth */);
    const bool isFixedPoint = isFixedPointDepthTextureFormat(m_levels[0][0].getFormat());
    Vec4 result;
    for (int i = 0; i < 4; i++)
        result[i] = execCompare(gathered, sampler.compare, i, ref, isFixedPoint);

    return result;
}

// TextureCube

TextureCube::TextureCube(const TextureFormat &format, int size, bool es2) : m_format(format), m_size(size)
{
    const int numLevels = computeMipPyramidLevels(m_size);
    const ConstPixelBufferAccess *levels[CUBEFACE_LAST];

    for (int face = 0; face < CUBEFACE_LAST; face++)
    {
        m_data[face].resize(numLevels);
        m_access[face].resize(numLevels);
        levels[face] = &m_access[face][0];
    }

    m_view = TextureCubeView(numLevels, levels, es2);
}

TextureCube::TextureCube(const TextureCube &other) : m_format(other.m_format), m_size(other.m_size)
{
    const int numLevels = computeMipPyramidLevels(m_size);
    const ConstPixelBufferAccess *levels[CUBEFACE_LAST];

    for (int face = 0; face < CUBEFACE_LAST; face++)
    {
        m_data[face].resize(numLevels);
        m_access[face].resize(numLevels);
        levels[face] = &m_access[face][0];
    }

    m_view = TextureCubeView(numLevels, levels, other.getView().isES2());

    for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
    {
        for (int face = 0; face < CUBEFACE_LAST; face++)
        {
            if (!other.isLevelEmpty((CubeFace)face, levelNdx))
            {
                allocLevel((CubeFace)face, levelNdx);
                copy(getLevelFace(levelNdx, (CubeFace)face), other.getLevelFace(levelNdx, (CubeFace)face));
            }
        }
    }
}

TextureCube &TextureCube::operator=(const TextureCube &other)
{
    if (this == &other)
        return *this;

    const int numLevels = computeMipPyramidLevels(other.m_size);
    const ConstPixelBufferAccess *levels[CUBEFACE_LAST];

    for (int face = 0; face < CUBEFACE_LAST; face++)
    {
        m_data[face].resize(numLevels);
        m_access[face].resize(numLevels);
        levels[face] = &m_access[face][0];
    }

    m_format = other.m_format;
    m_size   = other.m_size;
    m_view   = TextureCubeView(numLevels, levels, other.getView().isES2());

    for (int levelNdx = 0; levelNdx < numLevels; levelNdx++)
    {
        for (int face = 0; face < CUBEFACE_LAST; face++)
        {
            if (!isLevelEmpty((CubeFace)face, levelNdx))
                clearLevel((CubeFace)face, levelNdx);

            if (!other.isLevelEmpty((CubeFace)face, levelNdx))
            {
                allocLevel((CubeFace)face, levelNdx);
                copy(getLevelFace(levelNdx, (CubeFace)face), other.getLevelFace(levelNdx, (CubeFace)face));
            }
        }
    }

    return *this;
}

TextureCube::~TextureCube(void)
{
}

void TextureCube::allocLevel(tcu::CubeFace face, int levelNdx)
{
    const int size     = getMipPyramidLevelSize(m_size, levelNdx);
    const int dataSize = m_format.getPixelSize() * size * size;
    DE_ASSERT(isLevelEmpty(face, levelNdx));

    m_data[face][levelNdx].setStorage(dataSize);
    m_access[face][levelNdx] = PixelBufferAccess(m_format, size, size, 1, m_data[face][levelNdx].getPtr());
}

void TextureCube::clearLevel(tcu::CubeFace face, int levelNdx)
{
    DE_ASSERT(!isLevelEmpty(face, levelNdx));
    m_data[face][levelNdx].clear();
    m_access[face][levelNdx] = PixelBufferAccess();
}

// Texture1DArrayView

Texture1DArrayView::Texture1DArrayView(int numLevels, const ConstPixelBufferAccess *levels, bool es2 DE_UNUSED_ATTR,
                                       ImageViewMinLodParams *minLodParams DE_UNUSED_ATTR)
    : m_numLevels(numLevels)
    , m_levels(levels)
{
}

inline int Texture1DArrayView::selectLayer(float t) const
{
    DE_ASSERT(m_numLevels > 0 && m_levels);
    return de::clamp(deFloorFloatToInt32(t + 0.5f), 0, m_levels[0].getHeight() - 1);
}

Vec4 Texture1DArrayView::sample(const Sampler &sampler, float s, float t, float lod) const
{
    return sampleLevelArray1D(m_levels, m_numLevels, sampler, s, selectLayer(t), lod);
}

Vec4 Texture1DArrayView::sampleOffset(const Sampler &sampler, float s, float t, float lod, int32_t offset) const
{
    return sampleLevelArray1DOffset(m_levels, m_numLevels, sampler, s, lod, IVec2(offset, selectLayer(t)));
}

float Texture1DArrayView::sampleCompare(const Sampler &sampler, float ref, float s, float t, float lod) const
{
    return sampleLevelArray1DCompare(m_levels, m_numLevels, sampler, ref, s, lod, IVec2(0, selectLayer(t)));
}

float Texture1DArrayView::sampleCompareOffset(const Sampler &sampler, float ref, float s, float t, float lod,
                                              int32_t offset) const
{
    return sampleLevelArray1DCompare(m_levels, m_numLevels, sampler, ref, s, lod, IVec2(offset, selectLayer(t)));
}

// Texture2DArrayView

Texture2DArrayView::Texture2DArrayView(int numLevels, const ConstPixelBufferAccess *levels, bool es2 DE_UNUSED_ATTR,
                                       ImageViewMinLodParams *minLodParams DE_UNUSED_ATTR)
    : m_numLevels(numLevels)
    , m_levels(levels)
{
}

inline int Texture2DArrayView::selectLayer(float r) const
{
    DE_ASSERT(m_numLevels > 0 && m_levels);
    return de::clamp(deFloorFloatToInt32(r + 0.5f), 0, m_levels[0].getDepth() - 1);
}

Vec4 Texture2DArrayView::sample(const Sampler &sampler, float s, float t, float r, float lod) const
{
    return sampleLevelArray2D(m_levels, m_numLevels, sampler, s, t, selectLayer(r), lod);
}

float Texture2DArrayView::sampleCompare(const Sampler &sampler, float ref, float s, float t, float r, float lod) const
{
    return sampleLevelArray2DCompare(m_levels, m_numLevels, sampler, ref, s, t, lod, IVec3(0, 0, selectLayer(r)));
}

Vec4 Texture2DArrayView::sampleOffset(const Sampler &sampler, float s, float t, float r, float lod,
                                      const IVec2 &offset) const
{
    return sampleLevelArray2DOffset(m_levels, m_numLevels, sampler, s, t, lod,
                                    IVec3(offset.x(), offset.y(), selectLayer(r)));
}

float Texture2DArrayView::sampleCompareOffset(const Sampler &sampler, float ref, float s, float t, float r, float lod,
                                              const IVec2 &offset) const
{
    return sampleLevelArray2DCompare(m_levels, m_numLevels, sampler, ref, s, t, lod,
                                     IVec3(offset.x(), offset.y(), selectLayer(r)));
}

Vec4 Texture2DArrayView::gatherOffsets(const Sampler &sampler, float s, float t, float r, int componentNdx,
                                       const IVec2 (&offsets)[4]) const
{
    return gatherArray2DOffsets(m_levels[0], sampler, s, t, selectLayer(r), componentNdx, offsets);
}

Vec4 Texture2DArrayView::gatherOffsetsCompare(const Sampler &sampler, float ref, float s, float t, float r,
                                              const IVec2 (&offsets)[4]) const
{
    return gatherArray2DOffsetsCompare(m_levels[0], sampler, ref, s, t, selectLayer(r), offsets);
}

// Texture1DArray

Texture1DArray::Texture1DArray(const TextureFormat &format, int width, int numLayers)
    : TextureLevelPyramid(format, computeMipPyramidLevels(width))
    , m_width(width)
    , m_numLayers(numLayers)
    , m_view(getNumLevels(), getLevels())
{
}

Texture1DArray::Texture1DArray(const Texture1DArray &other)
    : TextureLevelPyramid(other)
    , m_width(other.m_width)
    , m_numLayers(other.m_numLayers)
    , m_view(getNumLevels(), getLevels())
{
}

Texture1DArray &Texture1DArray::operator=(const Texture1DArray &other)
{
    if (this == &other)
        return *this;

    TextureLevelPyramid::operator=(other);

    m_width     = other.m_width;
    m_numLayers = other.m_numLayers;
    m_view      = Texture1DArrayView(getNumLevels(), getLevels());

    return *this;
}

Texture1DArray::~Texture1DArray(void)
{
}

void Texture1DArray::allocLevel(int levelNdx)
{
    DE_ASSERT(de::inBounds(levelNdx, 0, getNumLevels()));

    const int width = getMipPyramidLevelSize(m_width, levelNdx);

    TextureLevelPyramid::allocLevel(levelNdx, width, m_numLayers, 1);
}

// Texture2DArray

Texture2DArray::Texture2DArray(const TextureFormat &format, int width, int height, int numLayers)
    : TextureLevelPyramid(format, computeMipPyramidLevels(width, height))
    , m_width(width)
    , m_height(height)
    , m_numLayers(numLayers)
    , m_view(getNumLevels(), getLevels())
{
}

Texture2DArray::Texture2DArray(const Texture2DArray &other)
    : TextureLevelPyramid(other)
    , m_width(other.m_width)
    , m_height(other.m_height)
    , m_numLayers(other.m_numLayers)
    , m_view(getNumLevels(), getLevels())
{
}

Texture2DArray &Texture2DArray::operator=(const Texture2DArray &other)
{
    if (this == &other)
        return *this;

    TextureLevelPyramid::operator=(other);

    m_width     = other.m_width;
    m_height    = other.m_height;
    m_numLayers = other.m_numLayers;
    m_view      = Texture2DArrayView(getNumLevels(), getLevels());

    return *this;
}

Texture2DArray::~Texture2DArray(void)
{
}

void Texture2DArray::allocLevel(int levelNdx)
{
    DE_ASSERT(de::inBounds(levelNdx, 0, getNumLevels()));

    const int width  = getMipPyramidLevelSize(m_width, levelNdx);
    const int height = getMipPyramidLevelSize(m_height, levelNdx);

    TextureLevelPyramid::allocLevel(levelNdx, width, height, m_numLayers);
}

// Texture3DView

Texture3DView::Texture3DView(int numLevels, const ConstPixelBufferAccess *levels, bool es2,
                             ImageViewMinLodParams *minLodParams)
    : m_numLevels(numLevels)
    , m_levels(levels)
    , m_es2(es2)
    , m_minLodParams(minLodParams)
{
}

// Texture3D

Texture3D::Texture3D(const TextureFormat &format, int width, int height, int depth)
    : TextureLevelPyramid(format, computeMipPyramidLevels(width, height, depth))
    , m_width(width)
    , m_height(height)
    , m_depth(depth)
    , m_view(getNumLevels(), getLevels())
{
}

Texture3D::Texture3D(const Texture3D &other)
    : TextureLevelPyramid(other)
    , m_width(other.m_width)
    , m_height(other.m_height)
    , m_depth(other.m_depth)
    , m_view(getNumLevels(), getLevels())
{
}

Texture3D &Texture3D::operator=(const Texture3D &other)
{
    if (this == &other)
        return *this;

    TextureLevelPyramid::operator=(other);

    m_width  = other.m_width;
    m_height = other.m_height;
    m_depth  = other.m_depth;
    m_view   = Texture3DView(getNumLevels(), getLevels());

    return *this;
}

Texture3D::~Texture3D(void)
{
}

void Texture3D::allocLevel(int levelNdx)
{
    DE_ASSERT(de::inBounds(levelNdx, 0, getNumLevels()));

    const int width  = getMipPyramidLevelSize(m_width, levelNdx);
    const int height = getMipPyramidLevelSize(m_height, levelNdx);
    const int depth  = getMipPyramidLevelSize(m_depth, levelNdx);

    TextureLevelPyramid::allocLevel(levelNdx, width, height, depth);
}

// TextureCubeArrayView

TextureCubeArrayView::TextureCubeArrayView(int numLevels, const ConstPixelBufferAccess *levels, bool es2 DE_UNUSED_ATTR,
                                           ImageViewMinLodParams *minLodParams DE_UNUSED_ATTR)
    : m_numLevels(numLevels)
    , m_levels(levels)
{
}

inline int TextureCubeArrayView::selectLayer(float q) const
{
    DE_ASSERT(m_numLevels > 0 && m_levels);
    DE_ASSERT((m_levels[0].getDepth() % 6) == 0);

    return de::clamp(deFloorFloatToInt32(q + 0.5f), 0, (m_levels[0].getDepth() / 6) - 1);
}

tcu::Vec4 TextureCubeArrayView::sample(const Sampler &sampler, float s, float t, float r, float q, float lod) const
{
    const CubeFaceFloatCoords coords = getCubeFaceCoords(Vec3(s, t, r));
    const int layer                  = selectLayer(q);
    const int faceDepth              = (layer * 6) + getCubeArrayFaceIndex(coords.face);

    DE_ASSERT(sampler.compare == Sampler::COMPAREMODE_NONE);

    if (sampler.seamlessCubeMap)
        return sampleCubeArraySeamless(m_levels, m_numLevels, layer, coords.face, sampler, coords.s, coords.t, lod);
    else
        return sampleLevelArray2D(m_levels, m_numLevels, sampler, coords.s, coords.t, faceDepth, lod);
}

float TextureCubeArrayView::sampleCompare(const Sampler &sampler, float ref, float s, float t, float r, float q,
                                          float lod) const
{
    const CubeFaceFloatCoords coords = getCubeFaceCoords(Vec3(s, t, r));
    const int layer                  = selectLayer(q);
    const int faceDepth              = (layer * 6) + getCubeArrayFaceIndex(coords.face);

    DE_ASSERT(sampler.compare != Sampler::COMPAREMODE_NONE);

    if (sampler.seamlessCubeMap)
        return sampleCubeArraySeamlessCompare(m_levels, m_numLevels, layer, coords.face, sampler, ref, coords.s,
                                              coords.t, lod);
    else
        return sampleLevelArray2DCompare(m_levels, m_numLevels, sampler, ref, coords.s, coords.t, lod,
                                         IVec3(0, 0, faceDepth));
}

// TextureCubeArray

TextureCubeArray::TextureCubeArray(const TextureFormat &format, int size, int depth)
    : TextureLevelPyramid(format, computeMipPyramidLevels(size))
    , m_size(size)
    , m_depth(depth)
    , m_view(getNumLevels(), getLevels())
{
    DE_ASSERT(m_depth % 6 == 0);
}

TextureCubeArray::TextureCubeArray(const TextureCubeArray &other)
    : TextureLevelPyramid(other)
    , m_size(other.m_size)
    , m_depth(other.m_depth)
    , m_view(getNumLevels(), getLevels())
{
    DE_ASSERT(m_depth % 6 == 0);
}

TextureCubeArray &TextureCubeArray::operator=(const TextureCubeArray &other)
{
    if (this == &other)
        return *this;

    TextureLevelPyramid::operator=(other);

    m_size  = other.m_size;
    m_depth = other.m_depth;
    m_view  = TextureCubeArrayView(getNumLevels(), getLevels());

    DE_ASSERT(m_depth % 6 == 0);

    return *this;
}

TextureCubeArray::~TextureCubeArray(void)
{
}

void TextureCubeArray::allocLevel(int levelNdx)
{
    DE_ASSERT(de::inBounds(levelNdx, 0, getNumLevels()));

    const int size = getMipPyramidLevelSize(m_size, levelNdx);

    TextureLevelPyramid::allocLevel(levelNdx, size, size, m_depth);
}

std::ostream &operator<<(std::ostream &str, TextureFormat::ChannelOrder order)
{
    const char *const orderStrings[] = {"R",   "A",    "I",    "L",     "LA",   "RG",    "RA",
                                        "RGB", "RGBA", "ARGB", "ABGR",  "BGR",  "BGRA",

                                        "sR",  "sRG",  "sRGB", "sRGBA", "sBGR", "sBGRA",

                                        "D",   "S",    "DS"};

    return str << de::getSizedArrayElement<TextureFormat::CHANNELORDER_LAST>(orderStrings, order);
}

std::ostream &operator<<(std::ostream &str, TextureFormat::ChannelType type)
{
    DE_STATIC_ASSERT(TextureFormat::CHANNELTYPE_LAST == 48);

    const char *const typeStrings[] = {"SNORM_INT8",
                                       "SNORM_INT16",
                                       "SNORM_INT32",
                                       "UNORM_INT8",
                                       "UNORM_INT16",
                                       "UNORM_INT24",
                                       "UNORM_INT32",
                                       "UNORM_BYTE_44",
                                       "UNORM_SHORT_565",
                                       "UNORM_SHORT_555",
                                       "UNORM_SHORT_4444",
                                       "UNORM_SHORT_5551",
                                       "UNORM_SHORT_1555",
                                       "UNORM_INT_101010",
                                       "SNORM_INT_1010102_REV",
                                       "UNORM_INT_1010102_REV",
                                       "UNSIGNED_BYTE_44",
                                       "UNSIGNED_SHORT_565",
                                       "UNSIGNED_SHORT_4444",
                                       "UNSIGNED_SHORT_5551",
                                       "SIGNED_INT_1010102_REV",
                                       "UNSIGNED_INT_1010102_REV",
                                       "UNSIGNED_INT_11F_11F_10F_REV",
                                       "UNSIGNED_INT_999_E5_REV",
                                       "UNSIGNED_INT_16_8_8",
                                       "UNSIGNED_INT_24_8",
                                       "UNSIGNED_INT_24_8_REV",
                                       "SIGNED_INT8",
                                       "SIGNED_INT16",
                                       "SIGNED_INT32",
                                       "SIGNED_INT64",
                                       "UNSIGNED_INT8",
                                       "UNSIGNED_INT16",
                                       "UNSIGNED_INT24",
                                       "UNSIGNED_INT32",
                                       "UNSIGNED_INT64",
                                       "HALF_FLOAT",
                                       "FLOAT",
                                       "FLOAT64",
                                       "FLOAT_UNSIGNED_INT_24_8_REV",
                                       "UNORM_SHORT_10",
                                       "UNORM_SHORT_12",
                                       "USCALED_INT8",
                                       "USCALED_INT16",
                                       "SSCALED_INT8",
                                       "SSCALED_INT16",
                                       "USCALED_INT_1010102_REV",
                                       "SSCALED_INT_1010102_REV"};

    return str << de::getSizedArrayElement<TextureFormat::CHANNELTYPE_LAST>(typeStrings, type);
}

std::ostream &operator<<(std::ostream &str, CubeFace face)
{
    switch (face)
    {
    case CUBEFACE_NEGATIVE_X:
        return str << "CUBEFACE_NEGATIVE_X";
    case CUBEFACE_POSITIVE_X:
        return str << "CUBEFACE_POSITIVE_X";
    case CUBEFACE_NEGATIVE_Y:
        return str << "CUBEFACE_NEGATIVE_Y";
    case CUBEFACE_POSITIVE_Y:
        return str << "CUBEFACE_POSITIVE_Y";
    case CUBEFACE_NEGATIVE_Z:
        return str << "CUBEFACE_NEGATIVE_Z";
    case CUBEFACE_POSITIVE_Z:
        return str << "CUBEFACE_POSITIVE_Z";
    case CUBEFACE_LAST:
        return str << "CUBEFACE_LAST";
    default:
        return str << "UNKNOWN(" << (int)face << ")";
    }
}

std::ostream &operator<<(std::ostream &str, TextureFormat format)
{
    return str << format.order << ", " << format.type << "";
}

std::ostream &operator<<(std::ostream &str, const ConstPixelBufferAccess &access)
{
    return str << "format = (" << access.getFormat() << "), size = " << access.getWidth() << " x " << access.getHeight()
               << " x " << access.getDepth() << ", pitch = " << access.getRowPitch() << " / " << access.getSlicePitch();
}

bool isSamplerMipmapModeLinear(tcu::Sampler::FilterMode filterMode)
{
    DE_STATIC_ASSERT(tcu::Sampler::FILTERMODE_LAST == 9);
    switch (filterMode)
    {
    case tcu::Sampler::NEAREST:
    case tcu::Sampler::LINEAR:
    case tcu::Sampler::CUBIC:
    case tcu::Sampler::NEAREST_MIPMAP_NEAREST:
    case tcu::Sampler::LINEAR_MIPMAP_NEAREST:
    case tcu::Sampler::CUBIC_MIPMAP_NEAREST:
        return false;
    case tcu::Sampler::NEAREST_MIPMAP_LINEAR:
    case tcu::Sampler::LINEAR_MIPMAP_LINEAR:
    case tcu::Sampler::CUBIC_MIPMAP_LINEAR:
        return true;
    default:
        DE_FATAL("Illegal filter mode");
        return false;
    }
}
} // namespace tcu
