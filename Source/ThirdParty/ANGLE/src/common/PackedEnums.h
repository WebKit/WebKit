// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// PackedGLEnums_autogen.h:
//   Declares ANGLE-specific enums classes for GLEnum and functions operating
//   on them.

#ifndef COMMON_PACKEDGLENUMS_H_
#define COMMON_PACKEDGLENUMS_H_

#include "common/PackedEGLEnums_autogen.h"
#include "common/PackedGLEnums_autogen.h"

#include <array>
#include <bitset>
#include <cstddef>

#include <EGL/egl.h>

#include "common/bitset_utils.h"

namespace angle
{

// Return the number of elements of a packed enum, including the InvalidEnum element.
template <typename E>
constexpr size_t EnumSize()
{
    using UnderlyingType = typename std::underlying_type<E>::type;
    return static_cast<UnderlyingType>(E::EnumCount);
}

// Implementation of AllEnums which allows iterating over all the possible values for a packed enums
// like so:
//     for (auto value : AllEnums<MyPackedEnum>()) {
//         // Do something with the enum.
//     }

template <typename E>
class EnumIterator final
{
  private:
    using UnderlyingType = typename std::underlying_type<E>::type;

  public:
    EnumIterator(E value) : mValue(static_cast<UnderlyingType>(value)) {}
    EnumIterator &operator++()
    {
        mValue++;
        return *this;
    }
    bool operator==(const EnumIterator &other) const { return mValue == other.mValue; }
    bool operator!=(const EnumIterator &other) const { return mValue != other.mValue; }
    E operator*() const { return static_cast<E>(mValue); }

  private:
    UnderlyingType mValue;
};

template <typename E>
struct AllEnums
{
    EnumIterator<E> begin() const { return {static_cast<E>(0)}; }
    EnumIterator<E> end() const { return {E::InvalidEnum}; }
};

// PackedEnumMap<E, T> is like an std::array<T, E::EnumCount> but is indexed with enum values. It
// implements all of the std::array interface except with enum values instead of indices.
template <typename E, typename T, size_t MaxSize = EnumSize<E>()>
class PackedEnumMap
{
    using UnderlyingType = typename std::underlying_type<E>::type;
    using Storage        = std::array<T, MaxSize>;

  public:
    using InitPair = std::pair<E, T>;

    constexpr PackedEnumMap() = default;

    constexpr PackedEnumMap(std::initializer_list<InitPair> init) : mPrivateData{}
    {
        // We use a for loop instead of range-for to work around a limitation in MSVC.
        for (const InitPair *it = init.begin(); it != init.end(); ++it)
        {
            mPrivateData[static_cast<UnderlyingType>(it->first)] = it->second;
        }
    }

    // types:
    using value_type      = T;
    using pointer         = T *;
    using const_pointer   = const T *;
    using reference       = T &;
    using const_reference = const T &;

    using size_type       = size_t;
    using difference_type = ptrdiff_t;

    using iterator               = typename Storage::iterator;
    using const_iterator         = typename Storage::const_iterator;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    // No explicit construct/copy/destroy for aggregate type
    void fill(const T &u) { mPrivateData.fill(u); }
    void swap(PackedEnumMap<E, T> &a) noexcept { mPrivateData.swap(a.mPrivateData); }

    // iterators:
    iterator begin() noexcept { return mPrivateData.begin(); }
    const_iterator begin() const noexcept { return mPrivateData.begin(); }
    iterator end() noexcept { return mPrivateData.end(); }
    const_iterator end() const noexcept { return mPrivateData.end(); }

    reverse_iterator rbegin() noexcept { return mPrivateData.rbegin(); }
    const_reverse_iterator rbegin() const noexcept { return mPrivateData.rbegin(); }
    reverse_iterator rend() noexcept { return mPrivateData.rend(); }
    const_reverse_iterator rend() const noexcept { return mPrivateData.rend(); }

    // capacity:
    constexpr size_type size() const noexcept { return mPrivateData.size(); }
    constexpr size_type max_size() const noexcept { return mPrivateData.max_size(); }
    constexpr bool empty() const noexcept { return mPrivateData.empty(); }

    // element access:
    reference operator[](E n)
    {
        ASSERT(static_cast<size_t>(n) < mPrivateData.size());
        return mPrivateData[static_cast<UnderlyingType>(n)];
    }

    constexpr const_reference operator[](E n) const
    {
        ASSERT(static_cast<size_t>(n) < mPrivateData.size());
        return mPrivateData[static_cast<UnderlyingType>(n)];
    }

    const_reference at(E n) const { return mPrivateData.at(static_cast<UnderlyingType>(n)); }
    reference at(E n) { return mPrivateData.at(static_cast<UnderlyingType>(n)); }

    reference front() { return mPrivateData.front(); }
    const_reference front() const { return mPrivateData.front(); }
    reference back() { return mPrivateData.back(); }
    const_reference back() const { return mPrivateData.back(); }

    T *data() noexcept { return mPrivateData.data(); }
    const T *data() const noexcept { return mPrivateData.data(); }

  private:
    Storage mPrivateData;
};

// PackedEnumBitSetE> is like an std::bitset<E::EnumCount> but is indexed with enum values. It
// implements the std::bitset interface except with enum values instead of indices.
template <typename E, typename DataT = uint32_t>
using PackedEnumBitSet = BitSetT<EnumSize<E>(), DataT, E>;

}  // namespace angle

namespace gl
{

TextureType TextureTargetToType(TextureTarget target);
TextureTarget NonCubeTextureTypeToTarget(TextureType type);

TextureTarget CubeFaceIndexToTextureTarget(size_t face);
size_t CubeMapTextureTargetToFaceIndex(TextureTarget target);
bool IsCubeMapFaceTarget(TextureTarget target);

constexpr TextureTarget kCubeMapTextureTargetMin = TextureTarget::CubeMapPositiveX;
constexpr TextureTarget kCubeMapTextureTargetMax = TextureTarget::CubeMapNegativeZ;
constexpr TextureTarget kAfterCubeMapTextureTargetMax =
    static_cast<TextureTarget>(static_cast<uint8_t>(kCubeMapTextureTargetMax) + 1);
struct AllCubeFaceTextureTargets
{
    angle::EnumIterator<TextureTarget> begin() const { return kCubeMapTextureTargetMin; }
    angle::EnumIterator<TextureTarget> end() const { return kAfterCubeMapTextureTargetMax; }
};

constexpr ShaderType kGLES2ShaderTypeMin = ShaderType::Vertex;
constexpr ShaderType kGLES2ShaderTypeMax = ShaderType::Fragment;
constexpr ShaderType kAfterGLES2ShaderTypeMax =
    static_cast<ShaderType>(static_cast<uint8_t>(kGLES2ShaderTypeMax) + 1);
struct AllGLES2ShaderTypes
{
    angle::EnumIterator<ShaderType> begin() const { return kGLES2ShaderTypeMin; }
    angle::EnumIterator<ShaderType> end() const { return kAfterGLES2ShaderTypeMax; }
};

constexpr ShaderType kShaderTypeMin = ShaderType::Vertex;
constexpr ShaderType kShaderTypeMax = ShaderType::Compute;
constexpr ShaderType kAfterShaderTypeMax =
    static_cast<ShaderType>(static_cast<uint8_t>(kShaderTypeMax) + 1);
struct AllShaderTypes
{
    angle::EnumIterator<ShaderType> begin() const { return kShaderTypeMin; }
    angle::EnumIterator<ShaderType> end() const { return kAfterShaderTypeMax; }
};

constexpr size_t kGraphicsShaderCount = static_cast<size_t>(ShaderType::EnumCount) - 1u;
// Arrange the shader types in the order of rendering pipeline
constexpr std::array<ShaderType, kGraphicsShaderCount> kAllGraphicsShaderTypes = {
    ShaderType::Vertex, ShaderType::Geometry, ShaderType::Fragment};

using ShaderBitSet = angle::PackedEnumBitSet<ShaderType, uint8_t>;
static_assert(sizeof(ShaderBitSet) == sizeof(uint8_t), "Unexpected size");

template <typename T>
using ShaderMap = angle::PackedEnumMap<ShaderType, T>;

TextureType SamplerTypeToTextureType(GLenum samplerType);

bool IsMultisampled(gl::TextureType type);

enum class PrimitiveMode : uint8_t
{
    Points                 = 0x0,
    Lines                  = 0x1,
    LineLoop               = 0x2,
    LineStrip              = 0x3,
    Triangles              = 0x4,
    TriangleStrip          = 0x5,
    TriangleFan            = 0x6,
    Unused1                = 0x7,
    Unused2                = 0x8,
    Unused3                = 0x9,
    LinesAdjacency         = 0xA,
    LineStripAdjacency     = 0xB,
    TrianglesAdjacency     = 0xC,
    TriangleStripAdjacency = 0xD,

    InvalidEnum = 0xE,
    EnumCount   = 0xE,
};

template <>
constexpr PrimitiveMode FromGLenum<PrimitiveMode>(GLenum from)
{
    if (from >= static_cast<GLenum>(PrimitiveMode::EnumCount))
    {
        return PrimitiveMode::InvalidEnum;
    }

    return static_cast<PrimitiveMode>(from);
}

constexpr GLenum ToGLenum(PrimitiveMode from)
{
    return static_cast<GLenum>(from);
}

static_assert(ToGLenum(PrimitiveMode::Points) == GL_POINTS, "PrimitiveMode violation");
static_assert(ToGLenum(PrimitiveMode::Lines) == GL_LINES, "PrimitiveMode violation");
static_assert(ToGLenum(PrimitiveMode::LineLoop) == GL_LINE_LOOP, "PrimitiveMode violation");
static_assert(ToGLenum(PrimitiveMode::LineStrip) == GL_LINE_STRIP, "PrimitiveMode violation");
static_assert(ToGLenum(PrimitiveMode::Triangles) == GL_TRIANGLES, "PrimitiveMode violation");
static_assert(ToGLenum(PrimitiveMode::TriangleStrip) == GL_TRIANGLE_STRIP,
              "PrimitiveMode violation");
static_assert(ToGLenum(PrimitiveMode::TriangleFan) == GL_TRIANGLE_FAN, "PrimitiveMode violation");
static_assert(ToGLenum(PrimitiveMode::LinesAdjacency) == GL_LINES_ADJACENCY,
              "PrimitiveMode violation");
static_assert(ToGLenum(PrimitiveMode::LineStripAdjacency) == GL_LINE_STRIP_ADJACENCY,
              "PrimitiveMode violation");
static_assert(ToGLenum(PrimitiveMode::TrianglesAdjacency) == GL_TRIANGLES_ADJACENCY,
              "PrimitiveMode violation");
static_assert(ToGLenum(PrimitiveMode::TriangleStripAdjacency) == GL_TRIANGLE_STRIP_ADJACENCY,
              "PrimitiveMode violation");

enum class DrawElementsType : size_t
{
    UnsignedByte  = 0,
    UnsignedShort = 1,
    UnsignedInt   = 2,
    InvalidEnum   = 3,
    EnumCount     = 3,
};

template <>
constexpr DrawElementsType FromGLenum<DrawElementsType>(GLenum from)
{

    GLenum scaled = (from - GL_UNSIGNED_BYTE);
    // This code sequence generates a ROR instruction on x86/arm. We want to check if the lowest bit
    // of scaled is set and if (scaled >> 1) is greater than a non-pot value. If we rotate the
    // lowest bit to the hightest bit both conditions can be checked with a single test.
    static_assert(sizeof(GLenum) == 4, "Update (scaled << 31) to sizeof(GLenum) * 8 - 1");
    GLenum packed = (scaled >> 1) | (scaled << 31);

    // operator ? with a simple assignment usually translates to a cmov instruction and thus avoids
    // a branch.
    packed = (packed >= static_cast<GLenum>(DrawElementsType::EnumCount))
                 ? static_cast<GLenum>(DrawElementsType::InvalidEnum)
                 : packed;

    return static_cast<DrawElementsType>(packed);
}

constexpr GLenum ToGLenum(DrawElementsType from)
{
    return ((static_cast<GLenum>(from) << 1) + GL_UNSIGNED_BYTE);
}

#define ANGLE_VALIDATE_PACKED_ENUM(type, packed, glenum)                 \
    static_assert(ToGLenum(type::packed) == glenum, #type " violation"); \
    static_assert(FromGLenum<type>(glenum) == type::packed, #type " violation")

ANGLE_VALIDATE_PACKED_ENUM(DrawElementsType, UnsignedByte, GL_UNSIGNED_BYTE);
ANGLE_VALIDATE_PACKED_ENUM(DrawElementsType, UnsignedShort, GL_UNSIGNED_SHORT);
ANGLE_VALIDATE_PACKED_ENUM(DrawElementsType, UnsignedInt, GL_UNSIGNED_INT);

enum class VertexAttribType
{
    Byte               = 0,   // GLenum == 0x1400
    UnsignedByte       = 1,   // GLenum == 0x1401
    Short              = 2,   // GLenum == 0x1402
    UnsignedShort      = 3,   // GLenum == 0x1403
    Int                = 4,   // GLenum == 0x1404
    UnsignedInt        = 5,   // GLenum == 0x1405
    Float              = 6,   // GLenum == 0x1406
    Unused1            = 7,   // GLenum == 0x1407
    Unused2            = 8,   // GLenum == 0x1408
    Unused3            = 9,   // GLenum == 0x1409
    Unused4            = 10,  // GLenum == 0x140A
    HalfFloat          = 11,  // GLenum == 0x140B
    Fixed              = 12,  // GLenum == 0x140C
    MaxBasicType       = 12,
    UnsignedInt2101010 = 13,  // GLenum == 0x8368
    Int2101010         = 14,  // GLenum == 0x8D9F
    InvalidEnum        = 15,
    EnumCount          = 15,
};

template <>
constexpr VertexAttribType FromGLenum<VertexAttribType>(GLenum from)
{
    GLenum packed = from - GL_BYTE;
    if (packed <= static_cast<GLenum>(VertexAttribType::MaxBasicType))
        return static_cast<VertexAttribType>(packed);
    if (from == GL_UNSIGNED_INT_2_10_10_10_REV)
        return VertexAttribType::UnsignedInt2101010;
    if (from == GL_INT_2_10_10_10_REV)
        return VertexAttribType::Int2101010;
    return VertexAttribType::InvalidEnum;
}

constexpr GLenum ToGLenum(VertexAttribType from)
{
    // This could be optimized using a constexpr table.
    if (from == VertexAttribType::Int2101010)
        return GL_INT_2_10_10_10_REV;
    if (from == VertexAttribType::UnsignedInt2101010)
        return GL_UNSIGNED_INT_2_10_10_10_REV;
    return static_cast<GLenum>(from) + GL_BYTE;
}

ANGLE_VALIDATE_PACKED_ENUM(VertexAttribType, Byte, GL_BYTE);
ANGLE_VALIDATE_PACKED_ENUM(VertexAttribType, UnsignedByte, GL_UNSIGNED_BYTE);
ANGLE_VALIDATE_PACKED_ENUM(VertexAttribType, Short, GL_SHORT);
ANGLE_VALIDATE_PACKED_ENUM(VertexAttribType, UnsignedShort, GL_UNSIGNED_SHORT);
ANGLE_VALIDATE_PACKED_ENUM(VertexAttribType, Int, GL_INT);
ANGLE_VALIDATE_PACKED_ENUM(VertexAttribType, UnsignedInt, GL_UNSIGNED_INT);
ANGLE_VALIDATE_PACKED_ENUM(VertexAttribType, Float, GL_FLOAT);
ANGLE_VALIDATE_PACKED_ENUM(VertexAttribType, HalfFloat, GL_HALF_FLOAT);
ANGLE_VALIDATE_PACKED_ENUM(VertexAttribType, Fixed, GL_FIXED);
ANGLE_VALIDATE_PACKED_ENUM(VertexAttribType, Int2101010, GL_INT_2_10_10_10_REV);
ANGLE_VALIDATE_PACKED_ENUM(VertexAttribType, UnsignedInt2101010, GL_UNSIGNED_INT_2_10_10_10_REV);
}  // namespace gl

namespace egl
{
MessageType ErrorCodeToMessageType(EGLint errorCode);
}  // namespace egl

namespace egl_gl
{
gl::TextureTarget EGLCubeMapTargetToCubeMapTarget(EGLenum eglTarget);
gl::TextureTarget EGLImageTargetToTextureTarget(EGLenum eglTarget);
gl::TextureType EGLTextureTargetToTextureType(EGLenum eglTarget);
}  // namespace egl_gl

#endif  // COMMON_PACKEDGLENUMS_H_
