/*-------------------------------------------------------------------------
 * drawElements Quality Program Reference Renderer
 * -----------------------------------------------
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
 * \brief Vertex attribute fetch.
 *//*--------------------------------------------------------------------*/

#include "rrVertexAttrib.hpp"
#include "tcuFloat.hpp"
#include "deInt32.h"
#include "deMemory.h"

namespace rr
{

namespace
{

struct NormalOrder
{
    enum
    {
        T0 = 0,
        T1 = 1,
        T2 = 2,
        T3 = 3,
    };
};

struct BGRAOrder
{
    enum
    {
        T0 = 2,
        T1 = 1,
        T2 = 0,
        T3 = 3,
    };
};

// readers

template <typename SrcScalarType, typename DstScalarType, typename Order>
inline void readOrder(typename tcu::Vector<DstScalarType, 4> &dst, const int size, const void *ptr)
{
    SrcScalarType aligned[4];
    deMemcpy(aligned, ptr, size * sizeof(SrcScalarType));

    dst[Order::T0] = DstScalarType(aligned[0]);
    if (size >= 2)
        dst[Order::T1] = DstScalarType(aligned[1]);
    if (size >= 3)
        dst[Order::T2] = DstScalarType(aligned[2]);
    if (size >= 4)
        dst[Order::T3] = DstScalarType(aligned[3]);
}

template <typename SrcScalarType, typename Order>
inline void readUnormOrder(tcu::Vec4 &dst, const int size, const void *ptr)
{
    const uint32_t range = (uint32_t)((1ull << (sizeof(SrcScalarType) * 8)) - 1);

    SrcScalarType aligned[4];
    deMemcpy(aligned, ptr, size * sizeof(SrcScalarType));

    dst[Order::T0] = float(aligned[0]) / float(range);
    if (size >= 2)
        dst[Order::T1] = float(aligned[1]) / float(range);
    if (size >= 3)
        dst[Order::T2] = float(aligned[2]) / float(range);
    if (size >= 4)
        dst[Order::T3] = float(aligned[3]) / float(range);
}

template <typename SrcScalarType>
inline void readSnormClamp(tcu::Vec4 &dst, const int size, const void *ptr)
{
    // Clamped formats, GLES3-style conversion: max{c / (2^(b-1) - 1), -1 }
    const uint32_t range = (uint32_t)((1ull << (sizeof(SrcScalarType) * 8 - 1)) - 1);

    SrcScalarType aligned[4];
    deMemcpy(aligned, ptr, size * sizeof(SrcScalarType));

    dst[0] = de::max(-1.0f, float(aligned[0]) / float(range));
    if (size >= 2)
        dst[1] = de::max(-1.0f, float(aligned[1]) / float(range));
    if (size >= 3)
        dst[2] = de::max(-1.0f, float(aligned[2]) / float(range));
    if (size >= 4)
        dst[3] = de::max(-1.0f, float(aligned[3]) / float(range));
}

template <typename SrcScalarType>
inline void readSnormScale(tcu::Vec4 &dst, const int size, const void *ptr)
{
    // Scaled formats, GLES2-style conversion: (2c + 1) / (2^b - 1)
    const uint32_t range = (uint32_t)((1ull << (sizeof(SrcScalarType) * 8)) - 1);

    SrcScalarType aligned[4];
    deMemcpy(aligned, ptr, size * sizeof(SrcScalarType));

    dst[0] = (float(aligned[0]) * 2.0f + 1.0f) / float(range);
    if (size >= 2)
        dst[1] = (float(aligned[1]) * 2.0f + 1.0f) / float(range);
    if (size >= 3)
        dst[2] = (float(aligned[2]) * 2.0f + 1.0f) / float(range);
    if (size >= 4)
        dst[3] = (float(aligned[3]) * 2.0f + 1.0f) / float(range);
}

inline void readHalf(tcu::Vec4 &dst, const int size, const void *ptr)
{
    uint16_t aligned[4];
    deMemcpy(aligned, ptr, size * sizeof(uint16_t));

    dst[0] = tcu::Float16(aligned[0]).asFloat();
    if (size >= 2)
        dst[1] = tcu::Float16(aligned[1]).asFloat();
    if (size >= 3)
        dst[2] = tcu::Float16(aligned[2]).asFloat();
    if (size >= 4)
        dst[3] = tcu::Float16(aligned[3]).asFloat();
}

inline void readFixed(tcu::Vec4 &dst, const int size, const void *ptr)
{
    int32_t aligned[4];
    deMemcpy(aligned, ptr, size * sizeof(int32_t));

    dst[0] = float(aligned[0]) / float(1 << 16);
    if (size >= 2)
        dst[1] = float(aligned[1]) / float(1 << 16);
    if (size >= 3)
        dst[2] = float(aligned[2]) / float(1 << 16);
    if (size >= 4)
        dst[3] = float(aligned[3]) / float(1 << 16);
}

inline void readDouble(tcu::Vec4 &dst, const int size, const void *ptr)
{
    double aligned[4];
    deMemcpy(aligned, ptr, size * sizeof(double));

    dst[0] = float(aligned[0]);
    if (size >= 2)
        dst[1] = float(aligned[1]);
    if (size >= 3)
        dst[2] = float(aligned[2]);
    if (size >= 4)
        dst[3] = float(aligned[3]);
}

template <int integerLen>
inline int32_t extendSign(uint32_t integer)
{
    return uint32_t(0 - int32_t((integer & (1 << (integerLen - 1))) << 1)) | integer;
}

template <typename DstScalarType>
inline void readUint2101010Rev(typename tcu::Vector<DstScalarType, 4> &dst, const int size, const void *ptr)
{
    uint32_t aligned;
    deMemcpy(&aligned, ptr, sizeof(uint32_t));

    dst[0] = DstScalarType((aligned >> 0) & ((1 << 10) - 1));
    if (size >= 2)
        dst[1] = DstScalarType((aligned >> 10) & ((1 << 10) - 1));
    if (size >= 3)
        dst[2] = DstScalarType((aligned >> 20) & ((1 << 10) - 1));
    if (size >= 4)
        dst[3] = DstScalarType((aligned >> 30) & ((1 << 2) - 1));
}

template <typename DstScalarType>
inline void readInt2101010Rev(typename tcu::Vector<DstScalarType, 4> &dst, const int size, const void *ptr)
{
    uint32_t aligned;
    deMemcpy(&aligned, ptr, sizeof(uint32_t));

    dst[0] = (DstScalarType)extendSign<10>((aligned >> 0) & ((1 << 10) - 1));
    if (size >= 2)
        dst[1] = (DstScalarType)extendSign<10>((aligned >> 10) & ((1 << 10) - 1));
    if (size >= 3)
        dst[2] = (DstScalarType)extendSign<10>((aligned >> 20) & ((1 << 10) - 1));
    if (size >= 4)
        dst[3] = (DstScalarType)extendSign<2>((aligned >> 30) & ((1 << 2) - 1));
}

template <typename Order>
inline void readUnorm2101010RevOrder(tcu::Vec4 &dst, const int size, const void *ptr)
{
    const uint32_t range10 = (uint32_t)((1ull << 10) - 1);
    const uint32_t range2  = (uint32_t)((1ull << 2) - 1);

    uint32_t aligned;
    deMemcpy(&aligned, ptr, sizeof(uint32_t));

    dst[Order::T0] = float((aligned >> 0) & ((1 << 10) - 1)) / float(range10);
    if (size >= 2)
        dst[Order::T1] = float((aligned >> 10) & ((1 << 10) - 1)) / float(range10);
    if (size >= 3)
        dst[Order::T2] = float((aligned >> 20) & ((1 << 10) - 1)) / float(range10);
    if (size >= 4)
        dst[Order::T3] = float((aligned >> 30) & ((1 << 2) - 1)) / float(range2);
}

template <typename Order>
inline void readSnorm2101010RevClampOrder(tcu::Vec4 &dst, const int size, const void *ptr)
{
    // Clamped formats, GLES3-style conversion: max{c / (2^(b-1) - 1), -1 }
    const uint32_t range10 = (uint32_t)((1ull << (10 - 1)) - 1);
    const uint32_t range2  = (uint32_t)((1ull << (2 - 1)) - 1);

    uint32_t aligned;
    deMemcpy(&aligned, ptr, sizeof(uint32_t));

    dst[Order::T0] = de::max(-1.0f, float(extendSign<10>((aligned >> 0) & ((1 << 10) - 1))) / float(range10));
    if (size >= 2)
        dst[Order::T1] = de::max(-1.0f, float(extendSign<10>((aligned >> 10) & ((1 << 10) - 1))) / float(range10));
    if (size >= 3)
        dst[Order::T2] = de::max(-1.0f, float(extendSign<10>((aligned >> 20) & ((1 << 10) - 1))) / float(range10));
    if (size >= 4)
        dst[Order::T3] = de::max(-1.0f, float(extendSign<2>((aligned >> 30) & ((1 << 2) - 1))) / float(range2));
}

template <typename Order>
inline void readSnorm2101010RevScaleOrder(tcu::Vec4 &dst, const int size, const void *ptr)
{
    // Scaled formats, GLES2-style conversion: (2c + 1) / (2^b - 1)
    const uint32_t range10 = (uint32_t)((1ull << 10) - 1);
    const uint32_t range2  = (uint32_t)((1ull << 2) - 1);

    uint32_t aligned;
    deMemcpy(&aligned, ptr, sizeof(uint32_t));

    dst[Order::T0] = (float(extendSign<10>((aligned >> 0) & ((1 << 10) - 1))) * 2.0f + 1.0f) / float(range10);
    if (size >= 2)
        dst[Order::T1] = (float(extendSign<10>((aligned >> 10) & ((1 << 10) - 1))) * 2.0f + 1.0f) / float(range10);
    if (size >= 3)
        dst[Order::T2] = (float(extendSign<10>((aligned >> 20) & ((1 << 10) - 1))) * 2.0f + 1.0f) / float(range10);
    if (size >= 4)
        dst[Order::T3] = (float(extendSign<2>((aligned >> 30) & ((1 << 2) - 1))) * 2.0f + 1.0f) / float(range2);
}

// ordered readers

template <typename SrcScalarType, typename DstScalarType>
inline void read(typename tcu::Vector<DstScalarType, 4> &dst, const int size, const void *ptr)
{
    readOrder<SrcScalarType, DstScalarType, NormalOrder>(dst, size, ptr);
}

template <typename SrcScalarType>
inline void readUnorm(tcu::Vec4 &dst, const int size, const void *ptr)
{
    readUnormOrder<SrcScalarType, NormalOrder>(dst, size, ptr);
}

template <typename SrcScalarType>
inline void readUnormBGRA(tcu::Vec4 &dst, const int size, const void *ptr)
{
    readUnormOrder<SrcScalarType, BGRAOrder>(dst, size, ptr);
}

inline void readUnorm2101010Rev(tcu::Vec4 &dst, const int size, const void *ptr)
{
    readUnorm2101010RevOrder<NormalOrder>(dst, size, ptr);
}

inline void readUnorm2101010RevBGRA(tcu::Vec4 &dst, const int size, const void *ptr)
{
    readUnorm2101010RevOrder<BGRAOrder>(dst, size, ptr);
}

inline void readSnorm2101010RevClamp(tcu::Vec4 &dst, const int size, const void *ptr)
{
    readSnorm2101010RevClampOrder<NormalOrder>(dst, size, ptr);
}

inline void readSnorm2101010RevClampBGRA(tcu::Vec4 &dst, const int size, const void *ptr)
{
    readSnorm2101010RevClampOrder<BGRAOrder>(dst, size, ptr);
}

inline void readSnorm2101010RevScale(tcu::Vec4 &dst, const int size, const void *ptr)
{
    readSnorm2101010RevScaleOrder<NormalOrder>(dst, size, ptr);
}

inline void readSnorm2101010RevScaleBGRA(tcu::Vec4 &dst, const int size, const void *ptr)
{
    readSnorm2101010RevScaleOrder<BGRAOrder>(dst, size, ptr);
}

// utils

void readFloat(tcu::Vec4 &dst, const VertexAttribType type, const int size, const void *ptr)
{
    switch (type)
    {
    case VERTEXATTRIBTYPE_FLOAT:
        read<float>(dst, size, ptr);
        break;
    case VERTEXATTRIBTYPE_HALF:
        readHalf(dst, size, ptr);
        break;
    case VERTEXATTRIBTYPE_FIXED:
        readFixed(dst, size, ptr);
        break;
    case VERTEXATTRIBTYPE_DOUBLE:
        readDouble(dst, size, ptr);
        break;
    case VERTEXATTRIBTYPE_NONPURE_UNORM8:
        readUnorm<uint8_t>(dst, size, ptr);
        break;
    case VERTEXATTRIBTYPE_NONPURE_UNORM16:
        readUnorm<uint16_t>(dst, size, ptr);
        break;
    case VERTEXATTRIBTYPE_NONPURE_UNORM32:
        readUnorm<uint32_t>(dst, size, ptr);
        break;
    case VERTEXATTRIBTYPE_NONPURE_UNORM_2_10_10_10_REV:
        readUnorm2101010Rev(dst, size, ptr);
        break;
    case VERTEXATTRIBTYPE_NONPURE_SNORM8_CLAMP:
        readSnormClamp<int8_t>(dst, size, ptr);
        break;
    case VERTEXATTRIBTYPE_NONPURE_SNORM16_CLAMP:
        readSnormClamp<int16_t>(dst, size, ptr);
        break;
    case VERTEXATTRIBTYPE_NONPURE_SNORM32_CLAMP:
        readSnormClamp<int32_t>(dst, size, ptr);
        break;
    case VERTEXATTRIBTYPE_NONPURE_SNORM_2_10_10_10_REV_CLAMP:
        readSnorm2101010RevClamp(dst, size, ptr);
        break;
    case VERTEXATTRIBTYPE_NONPURE_SNORM8_SCALE:
        readSnormScale<int8_t>(dst, size, ptr);
        break;
    case VERTEXATTRIBTYPE_NONPURE_SNORM16_SCALE:
        readSnormScale<int16_t>(dst, size, ptr);
        break;
    case VERTEXATTRIBTYPE_NONPURE_SNORM32_SCALE:
        readSnormScale<int32_t>(dst, size, ptr);
        break;
    case VERTEXATTRIBTYPE_NONPURE_SNORM_2_10_10_10_REV_SCALE:
        readSnorm2101010RevScale(dst, size, ptr);
        break;
    case VERTEXATTRIBTYPE_NONPURE_UINT8:
        read<uint8_t>(dst, size, ptr);
        break;
    case VERTEXATTRIBTYPE_NONPURE_UINT16:
        read<uint16_t>(dst, size, ptr);
        break;
    case VERTEXATTRIBTYPE_NONPURE_UINT32:
        read<uint32_t>(dst, size, ptr);
        break;
    case VERTEXATTRIBTYPE_NONPURE_INT8:
        read<int8_t>(dst, size, ptr);
        break;
    case VERTEXATTRIBTYPE_NONPURE_INT16:
        read<int16_t>(dst, size, ptr);
        break;
    case VERTEXATTRIBTYPE_NONPURE_INT32:
        read<int32_t>(dst, size, ptr);
        break;
    case VERTEXATTRIBTYPE_NONPURE_UINT_2_10_10_10_REV:
        readUint2101010Rev(dst, size, ptr);
        break;
    case VERTEXATTRIBTYPE_NONPURE_INT_2_10_10_10_REV:
        readInt2101010Rev(dst, size, ptr);
        break;
    case VERTEXATTRIBTYPE_NONPURE_UNORM8_BGRA:
        readUnormBGRA<uint8_t>(dst, size, ptr);
        break;
    case VERTEXATTRIBTYPE_NONPURE_UNORM_2_10_10_10_REV_BGRA:
        readUnorm2101010RevBGRA(dst, size, ptr);
        break;
    case VERTEXATTRIBTYPE_NONPURE_SNORM_2_10_10_10_REV_CLAMP_BGRA:
        readSnorm2101010RevClampBGRA(dst, size, ptr);
        break;
    case VERTEXATTRIBTYPE_NONPURE_SNORM_2_10_10_10_REV_SCALE_BGRA:
        readSnorm2101010RevScaleBGRA(dst, size, ptr);
        break;

    case VERTEXATTRIBTYPE_PURE_UINT8:
    case VERTEXATTRIBTYPE_PURE_UINT16:
    case VERTEXATTRIBTYPE_PURE_UINT32:
    case VERTEXATTRIBTYPE_PURE_INT8:
    case VERTEXATTRIBTYPE_PURE_INT16:
    case VERTEXATTRIBTYPE_PURE_INT32:
        DE_FATAL("Invalid read");
        break;

    default:
        DE_ASSERT(false);
    }
}

void readInt(tcu::IVec4 &dst, const VertexAttribType type, const int size, const void *ptr)
{
    switch (type)
    {
    case VERTEXATTRIBTYPE_PURE_INT8:
        read<int8_t>(dst, size, ptr);
        break;
    case VERTEXATTRIBTYPE_PURE_INT16:
        read<int16_t>(dst, size, ptr);
        break;
    case VERTEXATTRIBTYPE_PURE_INT32:
        read<int32_t>(dst, size, ptr);
        break;

    case VERTEXATTRIBTYPE_FLOAT:
    case VERTEXATTRIBTYPE_HALF:
    case VERTEXATTRIBTYPE_FIXED:
    case VERTEXATTRIBTYPE_DOUBLE:
    case VERTEXATTRIBTYPE_NONPURE_UNORM8:
    case VERTEXATTRIBTYPE_NONPURE_UNORM16:
    case VERTEXATTRIBTYPE_NONPURE_UNORM32:
    case VERTEXATTRIBTYPE_NONPURE_UNORM_2_10_10_10_REV:
    case VERTEXATTRIBTYPE_NONPURE_SNORM8_CLAMP:
    case VERTEXATTRIBTYPE_NONPURE_SNORM16_CLAMP:
    case VERTEXATTRIBTYPE_NONPURE_SNORM32_CLAMP:
    case VERTEXATTRIBTYPE_NONPURE_SNORM_2_10_10_10_REV_CLAMP:
    case VERTEXATTRIBTYPE_NONPURE_SNORM8_SCALE:
    case VERTEXATTRIBTYPE_NONPURE_SNORM16_SCALE:
    case VERTEXATTRIBTYPE_NONPURE_SNORM32_SCALE:
    case VERTEXATTRIBTYPE_NONPURE_SNORM_2_10_10_10_REV_SCALE:
    case VERTEXATTRIBTYPE_NONPURE_UINT8:
    case VERTEXATTRIBTYPE_NONPURE_UINT16:
    case VERTEXATTRIBTYPE_NONPURE_UINT32:
    case VERTEXATTRIBTYPE_NONPURE_INT8:
    case VERTEXATTRIBTYPE_NONPURE_INT16:
    case VERTEXATTRIBTYPE_NONPURE_INT32:
    case VERTEXATTRIBTYPE_NONPURE_UINT_2_10_10_10_REV:
    case VERTEXATTRIBTYPE_NONPURE_INT_2_10_10_10_REV:
    case VERTEXATTRIBTYPE_PURE_UINT8:
    case VERTEXATTRIBTYPE_PURE_UINT16:
    case VERTEXATTRIBTYPE_PURE_UINT32:
    case VERTEXATTRIBTYPE_NONPURE_UNORM8_BGRA:
    case VERTEXATTRIBTYPE_NONPURE_UNORM_2_10_10_10_REV_BGRA:
    case VERTEXATTRIBTYPE_NONPURE_SNORM_2_10_10_10_REV_CLAMP_BGRA:
    case VERTEXATTRIBTYPE_NONPURE_SNORM_2_10_10_10_REV_SCALE_BGRA:
        DE_FATAL("Invalid read");
        break;

    default:
        DE_ASSERT(false);
    }
}

void readUint(tcu::UVec4 &dst, const VertexAttribType type, const int size, const void *ptr)
{
    switch (type)
    {
    case VERTEXATTRIBTYPE_PURE_UINT8:
        read<uint8_t>(dst, size, ptr);
        break;
    case VERTEXATTRIBTYPE_PURE_UINT16:
        read<uint16_t>(dst, size, ptr);
        break;
    case VERTEXATTRIBTYPE_PURE_UINT32:
        read<uint32_t>(dst, size, ptr);
        break;

    case VERTEXATTRIBTYPE_FLOAT:
    case VERTEXATTRIBTYPE_HALF:
    case VERTEXATTRIBTYPE_FIXED:
    case VERTEXATTRIBTYPE_DOUBLE:
    case VERTEXATTRIBTYPE_NONPURE_UNORM8:
    case VERTEXATTRIBTYPE_NONPURE_UNORM16:
    case VERTEXATTRIBTYPE_NONPURE_UNORM32:
    case VERTEXATTRIBTYPE_NONPURE_UNORM_2_10_10_10_REV:
    case VERTEXATTRIBTYPE_NONPURE_SNORM8_CLAMP:
    case VERTEXATTRIBTYPE_NONPURE_SNORM16_CLAMP:
    case VERTEXATTRIBTYPE_NONPURE_SNORM32_CLAMP:
    case VERTEXATTRIBTYPE_NONPURE_SNORM_2_10_10_10_REV_CLAMP:
    case VERTEXATTRIBTYPE_NONPURE_SNORM8_SCALE:
    case VERTEXATTRIBTYPE_NONPURE_SNORM16_SCALE:
    case VERTEXATTRIBTYPE_NONPURE_SNORM32_SCALE:
    case VERTEXATTRIBTYPE_NONPURE_SNORM_2_10_10_10_REV_SCALE:
    case VERTEXATTRIBTYPE_NONPURE_UINT8:
    case VERTEXATTRIBTYPE_NONPURE_UINT16:
    case VERTEXATTRIBTYPE_NONPURE_UINT32:
    case VERTEXATTRIBTYPE_NONPURE_INT8:
    case VERTEXATTRIBTYPE_NONPURE_INT16:
    case VERTEXATTRIBTYPE_NONPURE_INT32:
    case VERTEXATTRIBTYPE_NONPURE_UINT_2_10_10_10_REV:
    case VERTEXATTRIBTYPE_NONPURE_INT_2_10_10_10_REV:
    case VERTEXATTRIBTYPE_PURE_INT8:
    case VERTEXATTRIBTYPE_PURE_INT16:
    case VERTEXATTRIBTYPE_PURE_INT32:
    case VERTEXATTRIBTYPE_NONPURE_UNORM8_BGRA:
    case VERTEXATTRIBTYPE_NONPURE_UNORM_2_10_10_10_REV_BGRA:
    case VERTEXATTRIBTYPE_NONPURE_SNORM_2_10_10_10_REV_CLAMP_BGRA:
    case VERTEXATTRIBTYPE_NONPURE_SNORM_2_10_10_10_REV_SCALE_BGRA:
        DE_FATAL("Invalid read");
        break;

    default:
        DE_ASSERT(false);
    }
}

int getComponentSize(const VertexAttribType type)
{
    switch (type)
    {
    case VERTEXATTRIBTYPE_FLOAT:
        return 4;
    case VERTEXATTRIBTYPE_HALF:
        return 2;
    case VERTEXATTRIBTYPE_FIXED:
        return 4;
    case VERTEXATTRIBTYPE_DOUBLE:
        return (int)sizeof(double);
    case VERTEXATTRIBTYPE_NONPURE_UNORM8:
        return 1;
    case VERTEXATTRIBTYPE_NONPURE_UNORM16:
        return 2;
    case VERTEXATTRIBTYPE_NONPURE_UNORM32:
        return 4;
    case VERTEXATTRIBTYPE_NONPURE_UNORM_2_10_10_10_REV:
        return (int)sizeof(uint32_t) / 4;
    case VERTEXATTRIBTYPE_NONPURE_SNORM8_CLAMP:
        return 1;
    case VERTEXATTRIBTYPE_NONPURE_SNORM16_CLAMP:
        return 2;
    case VERTEXATTRIBTYPE_NONPURE_SNORM32_CLAMP:
        return 4;
    case VERTEXATTRIBTYPE_NONPURE_SNORM_2_10_10_10_REV_CLAMP:
        return (int)sizeof(uint32_t) / 4;
    case VERTEXATTRIBTYPE_NONPURE_SNORM8_SCALE:
        return 1;
    case VERTEXATTRIBTYPE_NONPURE_SNORM16_SCALE:
        return 2;
    case VERTEXATTRIBTYPE_NONPURE_SNORM32_SCALE:
        return 4;
    case VERTEXATTRIBTYPE_NONPURE_SNORM_2_10_10_10_REV_SCALE:
        return (int)sizeof(uint32_t) / 4;
    case VERTEXATTRIBTYPE_NONPURE_UINT8:
        return 1;
    case VERTEXATTRIBTYPE_NONPURE_UINT16:
        return 2;
    case VERTEXATTRIBTYPE_NONPURE_UINT32:
        return 4;
    case VERTEXATTRIBTYPE_NONPURE_INT8:
        return 1;
    case VERTEXATTRIBTYPE_NONPURE_INT16:
        return 2;
    case VERTEXATTRIBTYPE_NONPURE_INT32:
        return 4;
    case VERTEXATTRIBTYPE_NONPURE_UINT_2_10_10_10_REV:
        return (int)sizeof(uint32_t) / 4;
    case VERTEXATTRIBTYPE_NONPURE_INT_2_10_10_10_REV:
        return (int)sizeof(uint32_t) / 4;
    case VERTEXATTRIBTYPE_PURE_UINT8:
        return 1;
    case VERTEXATTRIBTYPE_PURE_UINT16:
        return 2;
    case VERTEXATTRIBTYPE_PURE_UINT32:
        return 4;
    case VERTEXATTRIBTYPE_PURE_INT8:
        return 1;
    case VERTEXATTRIBTYPE_PURE_INT16:
        return 2;
    case VERTEXATTRIBTYPE_PURE_INT32:
        return 4;
    case VERTEXATTRIBTYPE_NONPURE_UNORM8_BGRA:
        return 1;
    case VERTEXATTRIBTYPE_NONPURE_UNORM_2_10_10_10_REV_BGRA:
        return (int)sizeof(uint32_t) / 4;
    case VERTEXATTRIBTYPE_NONPURE_SNORM_2_10_10_10_REV_CLAMP_BGRA:
        return (int)sizeof(uint32_t) / 4;
    case VERTEXATTRIBTYPE_NONPURE_SNORM_2_10_10_10_REV_SCALE_BGRA:
        return (int)sizeof(uint32_t) / 4;
    default:
        DE_ASSERT(false);
        return 0;
    }
}

} // namespace

bool isValidVertexAttrib(const VertexAttrib &vertexAttrib)
{
    // Trivial range checks.
    if (!de::inBounds<int>(vertexAttrib.type, 0, VERTEXATTRIBTYPE_LAST) || !de::inRange(vertexAttrib.size, 0, 4) ||
        vertexAttrib.instanceDivisor < 0)
        return false;

    // Generic attributes
    if (!vertexAttrib.pointer && vertexAttrib.type != VERTEXATTRIBTYPE_DONT_CARE)
        return false;

    // Packed formats
    if ((vertexAttrib.type == VERTEXATTRIBTYPE_NONPURE_INT_2_10_10_10_REV ||
         vertexAttrib.type == VERTEXATTRIBTYPE_NONPURE_UINT_2_10_10_10_REV ||
         vertexAttrib.type == VERTEXATTRIBTYPE_NONPURE_UNORM_2_10_10_10_REV ||
         vertexAttrib.type == VERTEXATTRIBTYPE_NONPURE_SNORM_2_10_10_10_REV_CLAMP ||
         vertexAttrib.type == VERTEXATTRIBTYPE_NONPURE_SNORM_2_10_10_10_REV_SCALE ||
         vertexAttrib.type == VERTEXATTRIBTYPE_NONPURE_UNORM_2_10_10_10_REV_BGRA ||
         vertexAttrib.type == VERTEXATTRIBTYPE_NONPURE_SNORM_2_10_10_10_REV_CLAMP_BGRA ||
         vertexAttrib.type == VERTEXATTRIBTYPE_NONPURE_SNORM_2_10_10_10_REV_SCALE_BGRA) &&
        vertexAttrib.size != 4)
        return false;

    return true;
}

void readVertexAttrib(tcu::Vec4 &dst, const VertexAttrib &vertexAttrib, const int instanceNdx, const int vertexNdx,
                      const int baseInstanceNdx)
{
    DE_ASSERT(isValidVertexAttrib(vertexAttrib));

    if (vertexAttrib.pointer)
    {
        const int elementNdx = (vertexAttrib.instanceDivisor != 0) ?
                                   baseInstanceNdx + (instanceNdx / vertexAttrib.instanceDivisor) :
                                   vertexNdx;
        const int compSize   = getComponentSize(vertexAttrib.type);
        const int stride     = (vertexAttrib.stride != 0) ? (vertexAttrib.stride) : (vertexAttrib.size * compSize);
        const int byteOffset = elementNdx * stride;

        dst = tcu::Vec4(0, 0, 0, 1); // defaults
        readFloat(dst, vertexAttrib.type, vertexAttrib.size, (const uint8_t *)vertexAttrib.pointer + byteOffset);
    }
    else
    {
        dst = vertexAttrib.generic.get<float>();
    }
}

void readVertexAttrib(tcu::IVec4 &dst, const VertexAttrib &vertexAttrib, const int instanceNdx, const int vertexNdx,
                      const int baseInstanceNdx)
{
    DE_ASSERT(isValidVertexAttrib(vertexAttrib));

    if (vertexAttrib.pointer)
    {
        const int elementNdx = (vertexAttrib.instanceDivisor != 0) ?
                                   baseInstanceNdx + (instanceNdx / vertexAttrib.instanceDivisor) :
                                   vertexNdx;
        const int compSize   = getComponentSize(vertexAttrib.type);
        const int stride     = (vertexAttrib.stride != 0) ? (vertexAttrib.stride) : (vertexAttrib.size * compSize);
        const int byteOffset = elementNdx * stride;

        dst = tcu::IVec4(0, 0, 0, 1); // defaults
        readInt(dst, vertexAttrib.type, vertexAttrib.size, (const uint8_t *)vertexAttrib.pointer + byteOffset);
    }
    else
    {
        dst = vertexAttrib.generic.get<int32_t>();
    }
}

void readVertexAttrib(tcu::UVec4 &dst, const VertexAttrib &vertexAttrib, const int instanceNdx, const int vertexNdx,
                      const int baseInstanceNdx)
{
    DE_ASSERT(isValidVertexAttrib(vertexAttrib));

    if (vertexAttrib.pointer)
    {
        const int elementNdx = (vertexAttrib.instanceDivisor != 0) ?
                                   baseInstanceNdx + (instanceNdx / vertexAttrib.instanceDivisor) :
                                   vertexNdx;
        const int compSize   = getComponentSize(vertexAttrib.type);
        const int stride     = (vertexAttrib.stride != 0) ? (vertexAttrib.stride) : (vertexAttrib.size * compSize);
        const int byteOffset = elementNdx * stride;

        dst = tcu::UVec4(0, 0, 0, 1); // defaults
        readUint(dst, vertexAttrib.type, vertexAttrib.size, (const uint8_t *)vertexAttrib.pointer + byteOffset);
    }
    else
    {
        dst = vertexAttrib.generic.get<uint32_t>();
    }
}

} // namespace rr
