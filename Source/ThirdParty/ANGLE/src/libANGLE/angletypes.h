//
// Copyright 2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// angletypes.h : Defines a variety of structures and enum types that are used throughout libGLESv2

#ifndef LIBANGLE_ANGLETYPES_H_
#define LIBANGLE_ANGLETYPES_H_

#include "common/Color.h"
#include "common/FixedVector.h"
#include "common/PackedEnums.h"
#include "common/bitset_utils.h"
#include "common/vector_utils.h"
#include "libANGLE/Constants.h"
#include "libANGLE/Error.h"
#include "libANGLE/RefCountObject.h"

#include <inttypes.h>
#include <stdint.h>

#include <bitset>
#include <map>
#include <unordered_map>

namespace gl
{
class Buffer;
class Texture;

struct Rectangle
{
    Rectangle() : x(0), y(0), width(0), height(0) {}
    constexpr Rectangle(int x_in, int y_in, int width_in, int height_in)
        : x(x_in), y(y_in), width(width_in), height(height_in)
    {}

    int x0() const { return x; }
    int y0() const { return y; }
    int x1() const { return x + width; }
    int y1() const { return y + height; }

    bool isReversedX() const { return width < 0; }
    bool isReversedY() const { return height < 0; }

    // Returns a rectangle with the same area but flipped in X, Y, neither or both.
    Rectangle flip(bool flipX, bool flipY) const;

    // Returns a rectangle with the same area but with height and width guaranteed to be positive.
    Rectangle removeReversal() const;

    bool encloses(const gl::Rectangle &inside) const;

    int x;
    int y;
    int width;
    int height;
};

bool operator==(const Rectangle &a, const Rectangle &b);
bool operator!=(const Rectangle &a, const Rectangle &b);

bool ClipRectangle(const Rectangle &source, const Rectangle &clip, Rectangle *intersection);

struct Offset
{
    constexpr Offset() : x(0), y(0), z(0) {}
    constexpr Offset(int x_in, int y_in, int z_in) : x(x_in), y(y_in), z(z_in) {}

    int x;
    int y;
    int z;
};

constexpr Offset kOffsetZero(0, 0, 0);

bool operator==(const Offset &a, const Offset &b);
bool operator!=(const Offset &a, const Offset &b);

struct Extents
{
    Extents() : width(0), height(0), depth(0) {}
    Extents(int width_, int height_, int depth_) : width(width_), height(height_), depth(depth_) {}

    Extents(const Extents &other) = default;
    Extents &operator=(const Extents &other) = default;

    bool empty() const { return (width * height * depth) == 0; }

    int width;
    int height;
    int depth;
};

bool operator==(const Extents &lhs, const Extents &rhs);
bool operator!=(const Extents &lhs, const Extents &rhs);

struct Box
{
    Box() : x(0), y(0), z(0), width(0), height(0), depth(0) {}
    Box(int x_in, int y_in, int z_in, int width_in, int height_in, int depth_in)
        : x(x_in), y(y_in), z(z_in), width(width_in), height(height_in), depth(depth_in)
    {}
    template <typename O, typename E>
    Box(const O &offset, const E &size)
        : x(offset.x),
          y(offset.y),
          z(offset.z),
          width(size.width),
          height(size.height),
          depth(size.depth)
    {}
    bool operator==(const Box &other) const;
    bool operator!=(const Box &other) const;
    Rectangle toRect() const;

    // Whether the Box has offset 0 and the same extents as argument.
    bool coversSameExtent(const Extents &size) const;

    int x;
    int y;
    int z;
    int width;
    int height;
    int depth;
};

struct RasterizerState final
{
    // This will zero-initialize the struct, including padding.
    RasterizerState();
    RasterizerState(const RasterizerState &other);

    bool cullFace;
    CullFaceMode cullMode;
    GLenum frontFace;

    bool polygonOffsetFill;
    GLfloat polygonOffsetFactor;
    GLfloat polygonOffsetUnits;

    bool pointDrawMode;
    bool multiSample;

    bool rasterizerDiscard;

    bool dither;
};

bool operator==(const RasterizerState &a, const RasterizerState &b);
bool operator!=(const RasterizerState &a, const RasterizerState &b);

struct BlendState final
{
    // This will zero-initialize the struct, including padding.
    BlendState();
    BlendState(const BlendState &other);

    bool blend;
    GLenum sourceBlendRGB;
    GLenum destBlendRGB;
    GLenum sourceBlendAlpha;
    GLenum destBlendAlpha;
    GLenum blendEquationRGB;
    GLenum blendEquationAlpha;

    bool colorMaskRed;
    bool colorMaskGreen;
    bool colorMaskBlue;
    bool colorMaskAlpha;
};

bool operator==(const BlendState &a, const BlendState &b);
bool operator!=(const BlendState &a, const BlendState &b);

using BlendStateArray = std::array<BlendState, IMPLEMENTATION_MAX_DRAW_BUFFERS>;

struct DepthStencilState final
{
    // This will zero-initialize the struct, including padding.
    DepthStencilState();
    DepthStencilState(const DepthStencilState &other);

    bool isDepthMaskedOut() const;
    bool isStencilMaskedOut() const;

    bool depthTest;
    GLenum depthFunc;
    bool depthMask;

    bool stencilTest;
    GLenum stencilFunc;
    GLuint stencilMask;
    GLenum stencilFail;
    GLenum stencilPassDepthFail;
    GLenum stencilPassDepthPass;
    GLuint stencilWritemask;
    GLenum stencilBackFunc;
    GLuint stencilBackMask;
    GLenum stencilBackFail;
    GLenum stencilBackPassDepthFail;
    GLenum stencilBackPassDepthPass;
    GLuint stencilBackWritemask;
};

bool operator==(const DepthStencilState &a, const DepthStencilState &b);
bool operator!=(const DepthStencilState &a, const DepthStencilState &b);

// Packs a sampler state for completeness checks:
// * minFilter: 5 values (3 bits)
// * magFilter: 2 values (1 bit)
// * wrapS:     3 values (2 bits)
// * wrapT:     3 values (2 bits)
// * compareMode: 1 bit (for == GL_NONE).
// This makes a total of 9 bits. We can pack this easily into 32 bits:
// * minFilter: 8 bits
// * magFilter: 8 bits
// * wrapS:     8 bits
// * wrapT:     4 bits
// * compareMode: 4 bits

struct PackedSamplerCompleteness
{
    uint8_t minFilter;
    uint8_t magFilter;
    uint8_t wrapS;
    uint8_t wrapTCompareMode;
};

static_assert(sizeof(PackedSamplerCompleteness) == sizeof(uint32_t), "Unexpected size");

// State from Table 6.10 (state per sampler object)
class SamplerState final
{
  public:
    // This will zero-initialize the struct, including padding.
    SamplerState();
    SamplerState(const SamplerState &other);

    static SamplerState CreateDefaultForTarget(TextureType type);

    GLenum getMinFilter() const { return mMinFilter; }

    void setMinFilter(GLenum minFilter);

    GLenum getMagFilter() const { return mMagFilter; }

    void setMagFilter(GLenum magFilter);

    GLenum getWrapS() const { return mWrapS; }

    void setWrapS(GLenum wrapS);

    GLenum getWrapT() const { return mWrapT; }

    void setWrapT(GLenum wrapT);

    GLenum getWrapR() const { return mWrapR; }

    void setWrapR(GLenum wrapR);

    float getMaxAnisotropy() const { return mMaxAnisotropy; }

    void setMaxAnisotropy(float maxAnisotropy);

    GLfloat getMinLod() const { return mMinLod; }

    void setMinLod(GLfloat minLod);

    GLfloat getMaxLod() const { return mMaxLod; }

    void setMaxLod(GLfloat maxLod);

    GLenum getCompareMode() const { return mCompareMode; }

    void setCompareMode(GLenum compareMode);

    GLenum getCompareFunc() const { return mCompareFunc; }

    void setCompareFunc(GLenum compareFunc);

    GLenum getSRGBDecode() const { return mSRGBDecode; }

    void setSRGBDecode(GLenum sRGBDecode);

    void setBorderColor(const ColorGeneric &color);

    const ColorGeneric &getBorderColor() const { return mBorderColor; }

    bool sameCompleteness(const SamplerState &samplerState) const
    {
        return mCompleteness.packed == samplerState.mCompleteness.packed;
    }

  private:
    void updateWrapTCompareMode();

    GLenum mMinFilter;
    GLenum mMagFilter;

    GLenum mWrapS;
    GLenum mWrapT;
    GLenum mWrapR;

    // From EXT_texture_filter_anisotropic
    float mMaxAnisotropy;

    GLfloat mMinLod;
    GLfloat mMaxLod;

    GLenum mCompareMode;
    GLenum mCompareFunc;

    GLenum mSRGBDecode;

    ColorGeneric mBorderColor;

    union Completeness
    {
        uint32_t packed;
        PackedSamplerCompleteness typed;
    };

    Completeness mCompleteness;
};

bool operator==(const SamplerState &a, const SamplerState &b);
bool operator!=(const SamplerState &a, const SamplerState &b);

struct DrawArraysIndirectCommand
{
    GLuint count;
    GLuint instanceCount;
    GLuint first;
    GLuint baseInstance;
};
static_assert(sizeof(DrawArraysIndirectCommand) == 16,
              "Unexpected size of DrawArraysIndirectCommand");

struct DrawElementsIndirectCommand
{
    GLuint count;
    GLuint primCount;
    GLuint firstIndex;
    GLint baseVertex;
    GLuint baseInstance;
};
static_assert(sizeof(DrawElementsIndirectCommand) == 20,
              "Unexpected size of DrawElementsIndirectCommand");

struct ImageUnit
{
    ImageUnit();
    ImageUnit(const ImageUnit &other);
    ~ImageUnit();

    BindingPointer<Texture> texture;
    GLint level;
    GLboolean layered;
    GLint layer;
    GLenum access;
    GLenum format;
};

using ImageUnitTextureTypeMap = std::map<unsigned int, gl::TextureType>;

struct PixelStoreStateBase
{
    GLint alignment   = 4;
    GLint rowLength   = 0;
    GLint skipRows    = 0;
    GLint skipPixels  = 0;
    GLint imageHeight = 0;
    GLint skipImages  = 0;
};

struct PixelUnpackState : PixelStoreStateBase
{};

struct PixelPackState : PixelStoreStateBase
{
    bool reverseRowOrder = false;
};

// Used in Program and VertexArray.
using AttributesMask = angle::BitSet<MAX_VERTEX_ATTRIBS>;

// Used in Program
using UniformBlockBindingMask = angle::BitSet<IMPLEMENTATION_MAX_COMBINED_SHADER_UNIFORM_BUFFERS>;

// Used in Framebuffer / Program
using DrawBufferMask = angle::BitSet<IMPLEMENTATION_MAX_DRAW_BUFFERS>;

class BlendStateExt final
{
    static_assert(IMPLEMENTATION_MAX_DRAW_BUFFERS == 8, "Only up to 8 draw buffers supported.");

  public:
    template <typename ElementType, size_t ElementCount>
    struct StorageType final
    {
        static_assert(ElementCount <= 256, "ElementCount cannot exceed 256.");

#if defined(ANGLE_IS_64_BIT_CPU)
        // Always use uint64_t on 64-bit systems
        static constexpr size_t kBits = 8;
#else
        static constexpr size_t kBits = ElementCount > 16 ? 8 : 4;
#endif

        using Type = typename std::conditional<kBits == 8, uint64_t, uint32_t>::type;

        static constexpr Type kMaxValueMask = (kBits == 8) ? 0xFF : 0xF;

        static constexpr Type GetMask(const size_t drawBuffers)
        {
            ASSERT(drawBuffers <= IMPLEMENTATION_MAX_DRAW_BUFFERS);
            return static_cast<Type>(0xFFFFFFFFFFFFFFFFull >> (64 - drawBuffers * kBits));
        }

        // A multiplier that is used to replicate 4- or 8-bit value 8 times.
        static constexpr Type kReplicator = (kBits == 8) ? 0x0101010101010101ull : 0x11111111;

        // Extract packed `Bits`-bit value of index `index`. `values` variable contains up to 8
        // packed values.
        static constexpr ElementType GetValueIndexed(const size_t index, const Type values)
        {
            ASSERT(index < IMPLEMENTATION_MAX_DRAW_BUFFERS);

            return static_cast<ElementType>((values >> (index * kBits)) & kMaxValueMask);
        }

        // Replicate `Bits`-bit value 8 times and mask the result.
        static constexpr Type GetReplicatedValue(const ElementType value, const Type mask)
        {
            ASSERT(static_cast<size_t>(value) <= kMaxValueMask);
            return (static_cast<size_t>(value) * kReplicator) & mask;
        }

        // Replace `Bits`-bit value of index `index` in `target` with `value`.
        static constexpr void SetValueIndexed(const size_t index,
                                              const ElementType value,
                                              Type *target)
        {
            ASSERT(static_cast<size_t>(value) <= kMaxValueMask);
            ASSERT(index < IMPLEMENTATION_MAX_DRAW_BUFFERS);

            // Bitmask with set bits that contain the value of index `index`.
            const Type selector = kMaxValueMask << (index * kBits);

            // Shift the new `value` to its position in the packed value.
            const Type builtValue = static_cast<Type>(value) << (index * kBits);

            // Mark differing bits of `target` and `builtValue`, then flip the bits on those
            // positions in `target`.
            // Taken from https://graphics.stanford.edu/~seander/bithacks.html#MaskedMerge
            *target = *target ^ ((*target ^ builtValue) & selector);
        }

        // Compare two packed sets of eight 4-bit values and return an 8-bit diff mask.
        static constexpr DrawBufferMask GetDiffMask(const uint32_t packedValue1,
                                                    const uint32_t packedValue2)
        {
            uint32_t diff = packedValue1 ^ packedValue2;

            // For each 4-bit value that is different between inputs, set the msb to 1 and other
            // bits to 0.
            diff = (diff | ((diff & 0x77777777) + 0x77777777)) & 0x88888888;

            // By this point, `diff` looks like a...b...c...d...e...f...g...h... (dots mean zeros).
            // To get DrawBufferMask, we need to compress this 32-bit value to 8 bits, i.e. abcdefgh

            // Multiplying the lower half of `diff` by 0x249 (0x200 + 0x40 + 0x8 + 0x1) produces:
            // ................e...f...g...h... +
            // .............e...f...g...h...... +
            // ..........e...f...g...h......... +
            // .......e...f...g...h............
            // ________________________________ =
            // .......e..ef.efgefghfgh.gh..h...
            //                 ^^^^
            // Similar operation is applied to the upper word.
            // This calculation could be replaced with a single PEXT instruction from BMI2 set.
            diff = ((((diff & 0xFFFF0000) * 0x249) >> 24) & 0xF0) | (((diff * 0x249) >> 12) & 0xF);

            return DrawBufferMask(diff);
        }

        // Compare two packed sets of eight 8-bit values and return an 8-bit diff mask.
        static constexpr DrawBufferMask GetDiffMask(const uint64_t packedValue1,
                                                    const uint64_t packedValue2)
        {
            uint64_t diff = packedValue1 ^ packedValue2;

            // For each 8-bit value that is different between inputs, set the msb to 1 and other
            // bits to 0.
            diff = (diff | ((diff & 0x7F7F7F7F7F7F7F7F) + 0x7F7F7F7F7F7F7F7F)) & 0x8080808080808080;

            // By this point, `diff` looks like (dots mean zeros):
            // a.......b.......c.......d.......e.......f.......g.......h.......
            // To get DrawBufferMask, we need to compress this 64-bit value to 8 bits, i.e. abcdefgh

            // Multiplying `diff` by 0x0002040810204081 produces:
            // a.......b.......c.......d.......e.......f.......g.......h....... +
            // .b.......c.......d.......e.......f.......g.......h.............. +
            // ..c.......d.......e.......f.......g.......h..................... +
            // ...d.......e.......f.......g.......h............................ +
            // ....e.......f.......g.......h................................... +
            // .....f.......g.......h.......................................... +
            // ......g.......h................................................. +
            // .......h........................................................
            // ________________________________________________________________ =
            // abcdefghbcdefgh.cdefgh..defgh...efgh....fgh.....gh......h.......
            // ^^^^^^^^
            // This operation could be replaced with a single PEXT instruction from BMI2 set.
            diff = 0x0002040810204081 * diff >> 56;

            return DrawBufferMask(static_cast<uint32_t>(diff));
        }
    };

    using FactorStorage    = StorageType<BlendFactorType, angle::EnumSize<BlendFactorType>()>;
    using EquationStorage  = StorageType<BlendEquationType, angle::EnumSize<BlendEquationType>()>;
    using ColorMaskStorage = StorageType<uint8_t, 16>;

    BlendStateExt(const size_t drawBuffers = 1);

    BlendStateExt &operator=(const BlendStateExt &other);

    ///////// Blending Toggle /////////

    void setEnabled(const bool enabled);
    void setEnabledIndexed(const size_t index, const bool enabled);

    ///////// Color Write Mask /////////

    static constexpr size_t PackColorMask(const bool red,
                                          const bool green,
                                          const bool blue,
                                          const bool alpha)
    {
        return (red ? 1 : 0) | (green ? 2 : 0) | (blue ? 4 : 0) | (alpha ? 8 : 0);
    }

    static constexpr void UnpackColorMask(const size_t value,
                                          bool *red,
                                          bool *green,
                                          bool *blue,
                                          bool *alpha)
    {
        *red   = static_cast<bool>(value & 1);
        *green = static_cast<bool>(value & 2);
        *blue  = static_cast<bool>(value & 4);
        *alpha = static_cast<bool>(value & 8);
    }

    ColorMaskStorage::Type expandColorMaskValue(const bool red,
                                                const bool green,
                                                const bool blue,
                                                const bool alpha) const;
    ColorMaskStorage::Type expandColorMaskIndexed(const size_t index) const;
    void setColorMask(const bool red, const bool green, const bool blue, const bool alpha);
    void setColorMaskIndexed(const size_t index, const uint8_t value);
    void setColorMaskIndexed(const size_t index,
                             const bool red,
                             const bool green,
                             const bool blue,
                             const bool alpha);
    uint8_t getColorMaskIndexed(const size_t index) const;
    void getColorMaskIndexed(const size_t index,
                             bool *red,
                             bool *green,
                             bool *blue,
                             bool *alpha) const;
    DrawBufferMask compareColorMask(ColorMaskStorage::Type other) const;

    ///////// Blend Equation /////////

    EquationStorage::Type expandEquationValue(const GLenum mode) const;
    EquationStorage::Type expandEquationColorIndexed(const size_t index) const;
    EquationStorage::Type expandEquationAlphaIndexed(const size_t index) const;
    void setEquations(const GLenum modeColor, const GLenum modeAlpha);
    void setEquationsIndexed(const size_t index, const GLenum modeColor, const GLenum modeAlpha);
    void setEquationsIndexed(const size_t index,
                             const size_t otherIndex,
                             const BlendStateExt &other);
    GLenum getEquationColorIndexed(size_t index) const;
    GLenum getEquationAlphaIndexed(size_t index) const;
    DrawBufferMask compareEquations(const EquationStorage::Type color,
                                    const EquationStorage::Type alpha) const;

    ///////// Blend Factors /////////

    FactorStorage::Type expandFactorValue(const GLenum func) const;
    FactorStorage::Type expandSrcColorIndexed(const size_t index) const;
    FactorStorage::Type expandDstColorIndexed(const size_t index) const;
    FactorStorage::Type expandSrcAlphaIndexed(const size_t index) const;
    FactorStorage::Type expandDstAlphaIndexed(const size_t index) const;
    void setFactors(const GLenum srcColor,
                    const GLenum dstColor,
                    const GLenum srcAlpha,
                    const GLenum dstAlpha);
    void setFactorsIndexed(const size_t index,
                           const GLenum srcColor,
                           const GLenum dstColor,
                           const GLenum srcAlpha,
                           const GLenum dstAlpha);
    void setFactorsIndexed(const size_t index, const size_t otherIndex, const BlendStateExt &other);
    GLenum getSrcColorIndexed(size_t index) const;
    GLenum getDstColorIndexed(size_t index) const;
    GLenum getSrcAlphaIndexed(size_t index) const;
    GLenum getDstAlphaIndexed(size_t index) const;
    DrawBufferMask compareFactors(const FactorStorage::Type srcColor,
                                  const FactorStorage::Type dstColor,
                                  const FactorStorage::Type srcAlpha,
                                  const FactorStorage::Type dstAlpha) const;

    ///////// Data Members /////////

    const FactorStorage::Type mMaxFactorMask;
    FactorStorage::Type mSrcColor;
    FactorStorage::Type mDstColor;
    FactorStorage::Type mSrcAlpha;
    FactorStorage::Type mDstAlpha;

    const EquationStorage::Type mMaxEquationMask;
    EquationStorage::Type mEquationColor;
    EquationStorage::Type mEquationAlpha;

    const ColorMaskStorage::Type mMaxColorMask;
    ColorMaskStorage::Type mColorMask;

    const DrawBufferMask mMaxEnabledMask;
    DrawBufferMask mEnabledMask;

    const size_t mMaxDrawBuffers;
};

// Used in StateCache
using StorageBuffersMask = angle::BitSet<IMPLEMENTATION_MAX_SHADER_STORAGE_BUFFER_BINDINGS>;

template <typename T>
using TexLevelArray = std::array<T, IMPLEMENTATION_MAX_TEXTURE_LEVELS>;

using TexLevelMask = angle::BitSet<IMPLEMENTATION_MAX_TEXTURE_LEVELS>;

enum class ComponentType
{
    Float       = 0,
    Int         = 1,
    UnsignedInt = 2,
    NoType      = 3,
    EnumCount   = 4,
    InvalidEnum = 4,
};

constexpr ComponentType GLenumToComponentType(GLenum componentType)
{
    switch (componentType)
    {
        case GL_FLOAT:
            return ComponentType::Float;
        case GL_INT:
            return ComponentType::Int;
        case GL_UNSIGNED_INT:
            return ComponentType::UnsignedInt;
        case GL_NONE:
            return ComponentType::NoType;
        default:
            return ComponentType::InvalidEnum;
    }
}

constexpr angle::PackedEnumMap<ComponentType, uint32_t> kComponentMasks = {{
    {ComponentType::Float, 0x10001},
    {ComponentType::Int, 0x00001},
    {ComponentType::UnsignedInt, 0x10000},
}};

constexpr size_t kMaxComponentTypeMaskIndex = 16;
using ComponentTypeMask                     = angle::BitSet<kMaxComponentTypeMaskIndex * 2>;

ANGLE_INLINE void SetComponentTypeMask(ComponentType type, size_t index, ComponentTypeMask *mask)
{
    ASSERT(index <= kMaxComponentTypeMaskIndex);
    *mask &= ~(0x10001 << index);
    *mask |= kComponentMasks[type] << index;
}

ANGLE_INLINE ComponentType GetComponentTypeMask(const ComponentTypeMask &mask, size_t index)
{
    ASSERT(index <= kMaxComponentTypeMaskIndex);
    uint32_t mask_bits = static_cast<uint32_t>((mask.to_ulong() >> index) & 0x10001);
    switch (mask_bits)
    {
        case 0x10001:
            return ComponentType::Float;
        case 0x00001:
            return ComponentType::Int;
        case 0x10000:
            return ComponentType::UnsignedInt;
        default:
            return ComponentType::InvalidEnum;
    }
}

bool ValidateComponentTypeMasks(unsigned long outputTypes,
                                unsigned long inputTypes,
                                unsigned long outputMask,
                                unsigned long inputMask);

using ContextID = uintptr_t;

constexpr size_t kCubeFaceCount = 6;

template <typename T>
using TextureTypeMap = angle::PackedEnumMap<TextureType, T>;
using TextureMap     = TextureTypeMap<BindingPointer<Texture>>;

// ShaderVector can contain one item per shader.  It differs from ShaderMap in that the values are
// not indexed by ShaderType.
template <typename T>
using ShaderVector = angle::FixedVector<T, static_cast<size_t>(ShaderType::EnumCount)>;

template <typename T>
using AttachmentArray = std::array<T, IMPLEMENTATION_MAX_FRAMEBUFFER_ATTACHMENTS>;

using AttachmentsMask = angle::BitSet<IMPLEMENTATION_MAX_FRAMEBUFFER_ATTACHMENTS>;

template <typename T>
using DrawBuffersArray = std::array<T, IMPLEMENTATION_MAX_DRAW_BUFFERS>;

template <typename T>
using DrawBuffersVector = angle::FixedVector<T, IMPLEMENTATION_MAX_DRAW_BUFFERS>;

template <typename T>
using AttribArray = std::array<T, MAX_VERTEX_ATTRIBS>;

using ActiveTextureMask = angle::BitSet<IMPLEMENTATION_MAX_ACTIVE_TEXTURES>;

template <typename T>
using ActiveTextureArray = std::array<T, IMPLEMENTATION_MAX_ACTIVE_TEXTURES>;

using ActiveTextureTypeArray = ActiveTextureArray<TextureType>;

template <typename T>
using UniformBuffersArray = std::array<T, IMPLEMENTATION_MAX_UNIFORM_BUFFER_BINDINGS>;
template <typename T>
using StorageBuffersArray = std::array<T, IMPLEMENTATION_MAX_SHADER_STORAGE_BUFFER_BINDINGS>;
template <typename T>
using AtomicCounterBuffersArray = std::array<T, IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFERS>;
using AtomicCounterBufferMask   = angle::BitSet<IMPLEMENTATION_MAX_ATOMIC_COUNTER_BUFFERS>;
template <typename T>
using ImagesArray = std::array<T, IMPLEMENTATION_MAX_IMAGE_UNITS>;

using ImageUnitMask = angle::BitSet<IMPLEMENTATION_MAX_IMAGE_UNITS>;

using SupportedSampleSet = std::set<GLuint>;

template <typename T>
using TransformFeedbackBuffersArray =
    std::array<T, gl::IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS>;

constexpr size_t kBarrierVectorDefaultSize = 16;

template <typename T>
using BarrierVector = angle::FastVector<T, kBarrierVectorDefaultSize>;

using BufferBarrierVector = BarrierVector<Buffer *>;

struct TextureAndLayout
{
    Texture *texture;
    GLenum layout;
};
using TextureBarrierVector = BarrierVector<TextureAndLayout>;

// OffsetBindingPointer.getSize() returns the size specified by the user, which may be larger than
// the size of the bound buffer. This function reduces the returned size to fit the bound buffer if
// necessary. Returns 0 if no buffer is bound or if integer overflow occurs.
GLsizeiptr GetBoundBufferAvailableSize(const OffsetBindingPointer<Buffer> &binding);

}  // namespace gl

namespace rx
{
// A macro that determines whether an object has a given runtime type.
#if defined(__clang__)
#    if __has_feature(cxx_rtti)
#        define ANGLE_HAS_DYNAMIC_CAST 1
#    endif
#elif !defined(NDEBUG) && (!defined(_MSC_VER) || defined(_CPPRTTI)) &&              \
    (!defined(__GNUC__) || __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 3) || \
     defined(__GXX_RTTI))
#    define ANGLE_HAS_DYNAMIC_CAST 1
#endif

#ifdef ANGLE_HAS_DYNAMIC_CAST
#    define ANGLE_HAS_DYNAMIC_TYPE(type, obj) (dynamic_cast<type>(obj) != nullptr)
#    undef ANGLE_HAS_DYNAMIC_CAST
#else
#    define ANGLE_HAS_DYNAMIC_TYPE(type, obj) (obj != nullptr)
#endif

// Downcast a base implementation object (EG TextureImpl to TextureD3D)
template <typename DestT, typename SrcT>
inline DestT *GetAs(SrcT *src)
{
    ASSERT(ANGLE_HAS_DYNAMIC_TYPE(DestT *, src));
    return static_cast<DestT *>(src);
}

template <typename DestT, typename SrcT>
inline const DestT *GetAs(const SrcT *src)
{
    ASSERT(ANGLE_HAS_DYNAMIC_TYPE(const DestT *, src));
    return static_cast<const DestT *>(src);
}

#undef ANGLE_HAS_DYNAMIC_TYPE

// Downcast a GL object to an Impl (EG gl::Texture to rx::TextureD3D)
template <typename DestT, typename SrcT>
inline DestT *GetImplAs(SrcT *src)
{
    return GetAs<DestT>(src->getImplementation());
}

template <typename DestT, typename SrcT>
inline DestT *SafeGetImplAs(SrcT *src)
{
    return src != nullptr ? GetAs<DestT>(src->getImplementation()) : nullptr;
}

}  // namespace rx

#include "angletypes.inc"

namespace angle
{
// Zero-based for better array indexing
enum FramebufferBinding
{
    FramebufferBindingRead = 0,
    FramebufferBindingDraw,
    FramebufferBindingSingletonMax,
    FramebufferBindingBoth = FramebufferBindingSingletonMax,
    FramebufferBindingMax,
    FramebufferBindingUnknown = FramebufferBindingMax,
};

inline FramebufferBinding EnumToFramebufferBinding(GLenum enumValue)
{
    switch (enumValue)
    {
        case GL_READ_FRAMEBUFFER:
            return FramebufferBindingRead;
        case GL_DRAW_FRAMEBUFFER:
            return FramebufferBindingDraw;
        case GL_FRAMEBUFFER:
            return FramebufferBindingBoth;
        default:
            UNREACHABLE();
            return FramebufferBindingUnknown;
    }
}

inline GLenum FramebufferBindingToEnum(FramebufferBinding binding)
{
    switch (binding)
    {
        case FramebufferBindingRead:
            return GL_READ_FRAMEBUFFER;
        case FramebufferBindingDraw:
            return GL_DRAW_FRAMEBUFFER;
        case FramebufferBindingBoth:
            return GL_FRAMEBUFFER;
        default:
            UNREACHABLE();
            return GL_NONE;
    }
}

template <typename ObjT, typename ContextT>
class DestroyThenDelete
{
  public:
    DestroyThenDelete(const ContextT *context) : mContext(context) {}

    void operator()(ObjT *obj)
    {
        (void)(obj->onDestroy(mContext));
        delete obj;
    }

  private:
    const ContextT *mContext;
};

// Helper class for wrapping an onDestroy function.
template <typename ObjT, typename DeleterT>
class UniqueObjectPointerBase : angle::NonCopyable
{
  public:
    template <typename ContextT>
    UniqueObjectPointerBase(const ContextT *context) : mObject(nullptr), mDeleter(context)
    {}

    template <typename ContextT>
    UniqueObjectPointerBase(ObjT *obj, const ContextT *context) : mObject(obj), mDeleter(context)
    {}

    ~UniqueObjectPointerBase()
    {
        if (mObject)
        {
            mDeleter(mObject);
        }
    }

    ObjT *operator->() const { return mObject; }

    ObjT *release()
    {
        auto obj = mObject;
        mObject  = nullptr;
        return obj;
    }

    ObjT *get() const { return mObject; }

    void reset(ObjT *obj)
    {
        if (mObject)
        {
            mDeleter(mObject);
        }
        mObject = obj;
    }

  private:
    ObjT *mObject;
    DeleterT mDeleter;
};

template <typename ObjT, typename ContextT>
using UniqueObjectPointer = UniqueObjectPointerBase<ObjT, DestroyThenDelete<ObjT, ContextT>>;

}  // namespace angle

namespace gl
{
class State;
}  // namespace gl

#endif  // LIBANGLE_ANGLETYPES_H_
