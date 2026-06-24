//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// mtl_state_cache.h:
//    Defines the class interface for StateCache, RenderPipelineCache and various
//    C struct versions of Metal sampler, depth stencil, render pass, render pipeline descriptors.
//

#ifndef LIBANGLE_RENDERER_METAL_MTL_STATE_CACHE_H_
#define LIBANGLE_RENDERER_METAL_MTL_STATE_CACHE_H_

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#import <Metal/Metal.h>

#include <unordered_map>

#include "common/hash_utils.h"
#include "libANGLE/State.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/metal/mtl_common.h"
#include "libANGLE/renderer/metal/mtl_context_device.h"
#include "libANGLE/renderer/metal/mtl_resources.h"

static inline bool operator==(const MTLClearColor &lhs, const MTLClearColor &rhs);

namespace angle
{
struct FeaturesMtl;
}

namespace rx
{
class ContextMtl;

namespace mtl
{

ANGLE_ENABLE_STRUCT_PADDING_WARNINGS
class StencilDesc
{
  public:
    constexpr StencilDesc()
        : readMask(mtl::kStencilMaskAll),
          writeMask(mtl::kStencilMaskAll),
          mStencilFailureOperation(MTLStencilOperationKeep),
          mDepthFailureOperation(MTLStencilOperationKeep),
          mDepthStencilPassOperation(MTLStencilOperationKeep),
          mStencilCompareFunction(MTLCompareFunctionAlways)
    {}

    constexpr bool operator==(const StencilDesc &rhs) const = default;
    size_t hash() const;

    void setStencilFailureOperation(MTLStencilOperation op)
    {
        mStencilFailureOperation = static_cast<uint32_t>(op);
    }
    MTLStencilOperation getStencilFailureOperation() const
    {
        return static_cast<MTLStencilOperation>(mStencilFailureOperation);
    }
    void setDepthFailureOperation(MTLStencilOperation op)
    {
        mDepthFailureOperation = static_cast<uint32_t>(op);
    }
    MTLStencilOperation getDepthFailureOperation() const
    {
        return static_cast<MTLStencilOperation>(mDepthFailureOperation);
    }
    void setDepthStencilPassOperation(MTLStencilOperation op)
    {
        mDepthStencilPassOperation = static_cast<uint32_t>(op);
    }
    MTLStencilOperation getDepthStencilPassOperation() const
    {
        return static_cast<MTLStencilOperation>(mDepthStencilPassOperation);
    }
    void setStencilCompareFunction(MTLCompareFunction func)
    {
        mStencilCompareFunction = static_cast<uint32_t>(func);
    }
    MTLCompareFunction getStencilCompareFunction() const
    {
        return static_cast<MTLCompareFunction>(mStencilCompareFunction);
    }

    // All bitfields in same access section to avoid padding between access specifiers.
    uint32_t readMask : 8;   // uint8_t.
    uint32_t writeMask : 8;  // uint8_t.

  private:
    uint32_t mStencilFailureOperation : 3;    // MTLStencilOperation.
    uint32_t mDepthFailureOperation : 3;      // MTLStencilOperation.
    uint32_t mDepthStencilPassOperation : 3;  // MTLStencilOperation.
    uint32_t mStencilCompareFunction : 7;     // MTLCompareFunction 3 bits + 4 bits padding.
};
static_assert(alignof(StencilDesc) <= 4);
static_assert(sizeof(StencilDesc) == 4);
ANGLE_DISABLE_STRUCT_PADDING_WARNINGS

inline size_t StencilDesc::hash() const
{
    return angle::HashMultiple<uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t>(
        readMask, writeMask, mStencilFailureOperation, mDepthFailureOperation,
        mDepthStencilPassOperation, mStencilCompareFunction);
}

ANGLE_ENABLE_STRUCT_PADDING_WARNINGS
class DepthStencilDesc
{
  public:
    constexpr DepthStencilDesc() : DepthStencilDesc(MTLCompareFunctionAlways, true) {}
    constexpr DepthStencilDesc(MTLCompareFunction func, bool depthWriteEnabled)
        : mDepthCompareFunction(static_cast<uint32_t>(func)), mDepthWriteEnabled(depthWriteEnabled)
    {}

    constexpr bool operator==(const DepthStencilDesc &rhs) const = default;
    size_t hash() const;

    void updateDepthTestEnabled(const gl::DepthStencilState &dsState);
    void updateDepthWriteEnabled(const gl::DepthStencilState &dsState);
    void updateDepthCompareFunc(const gl::DepthStencilState &dsState);
    void updateStencilTestEnabled(const gl::DepthStencilState &dsState);
    void updateStencilFrontOps(const gl::DepthStencilState &dsState);
    void updateStencilBackOps(const gl::DepthStencilState &dsState);
    void updateStencilFrontFuncs(const gl::DepthStencilState &dsState);
    void updateStencilBackFuncs(const gl::DepthStencilState &dsState);
    void updateStencilFrontWriteMask(const gl::DepthStencilState &dsState);
    void updateStencilBackWriteMask(const gl::DepthStencilState &dsState);

    MTLCompareFunction getDepthCompareFunction() const
    {
        return static_cast<MTLCompareFunction>(mDepthCompareFunction);
    }
    void setDepthWriteDisabled()
    {
        mDepthCompareFunction = MTLCompareFunctionAlways;
        mDepthWriteEnabled    = false;
    }
    bool isDepthWriteEnabled() const { return mDepthWriteEnabled; }

    StencilDesc backFaceStencil;
    StencilDesc frontFaceStencil;

  private:
    // All bitfields in same access section to avoid padding between access specifiers.
    uint32_t mDepthCompareFunction : 3;  // MTLCompareFunction.
    uint32_t mDepthWriteEnabled : 29;    // bool + 28 bit padding.
};
static_assert(alignof(DepthStencilDesc) <= 4);
static_assert(sizeof(DepthStencilDesc) == 12);
ANGLE_DISABLE_STRUCT_PADDING_WARNINGS

inline size_t DepthStencilDesc::hash() const
{
    return angle::HashMultiple<StencilDesc, StencilDesc, uint8_t, bool>(
        backFaceStencil, frontFaceStencil, mDepthCompareFunction, mDepthWriteEnabled);
}

ANGLE_ENABLE_STRUCT_PADDING_WARNINGS
class SamplerDesc
{
  public:
    constexpr SamplerDesc()
        : mRAddressMode(MTLSamplerAddressModeClampToEdge),
          mSAddressMode(MTLSamplerAddressModeClampToEdge),
          mTAddressMode(MTLSamplerAddressModeClampToEdge),
          mMinFilter(MTLSamplerMinMagFilterNearest),
          mMagFilter(MTLSamplerMinMagFilterNearest),
          mMipFilter(MTLSamplerMipFilterNearest),
          mMaxAnisotropy(1),
          mCompareFunction(MTLCompareFunctionNever)
    {}
    explicit SamplerDesc(const gl::SamplerState &glState);

    constexpr bool operator==(const SamplerDesc &rhs) const = default;
    size_t hash() const;

    void setRAddressMode(MTLSamplerAddressMode mode)
    {
        mRAddressMode = static_cast<uint32_t>(mode);
    }
    MTLSamplerAddressMode getRAddressMode() const
    {
        return static_cast<MTLSamplerAddressMode>(mRAddressMode);
    }
    void setSAddressMode(MTLSamplerAddressMode mode)
    {
        mSAddressMode = static_cast<uint32_t>(mode);
    }
    MTLSamplerAddressMode getSAddressMode() const
    {
        return static_cast<MTLSamplerAddressMode>(mSAddressMode);
    }
    void setTAddressMode(MTLSamplerAddressMode mode)
    {
        mTAddressMode = static_cast<uint32_t>(mode);
    }
    MTLSamplerAddressMode getTAddressMode() const
    {
        return static_cast<MTLSamplerAddressMode>(mTAddressMode);
    }
    void setMinFilter(MTLSamplerMinMagFilter filter) { mMinFilter = static_cast<uint32_t>(filter); }
    MTLSamplerMinMagFilter getMinFilter() const
    {
        return static_cast<MTLSamplerMinMagFilter>(mMinFilter);
    }
    void setMagFilter(MTLSamplerMinMagFilter filter) { mMagFilter = static_cast<uint32_t>(filter); }
    MTLSamplerMinMagFilter getMagFilter() const
    {
        return static_cast<MTLSamplerMinMagFilter>(mMagFilter);
    }
    void setMipFilter(MTLSamplerMipFilter filter) { mMipFilter = static_cast<uint32_t>(filter); }
    MTLSamplerMipFilter getMipFilter() const
    {
        return static_cast<MTLSamplerMipFilter>(mMipFilter);
    }
    void setMaxAnisotropy(NSUInteger value) { mMaxAnisotropy = static_cast<uint32_t>(value); }
    NSUInteger getMaxAnisotropy() const { return static_cast<NSUInteger>(mMaxAnisotropy); }
    void setCompareFunction(MTLCompareFunction func)
    {
        mCompareFunction = static_cast<uint32_t>(func);
    }
    MTLCompareFunction getCompareFunction() const
    {
        return static_cast<MTLCompareFunction>(mCompareFunction);
    }

  private:
    uint32_t mRAddressMode : 3;      // MTLSamplerAddressMode.
    uint32_t mSAddressMode : 3;      // MTLSamplerAddressMode.
    uint32_t mTAddressMode : 3;      // MTLSamplerAddressMode.
    uint32_t mMinFilter : 1;         // MTLSamplerMinMagFilter.
    uint32_t mMagFilter : 1;         // MTLSamplerMinMagFilter
    uint32_t mMipFilter : 2;         // MTLSamplerMipFilter.
    uint32_t mMaxAnisotropy : 5;     // NSUInteger.
    uint32_t mCompareFunction : 14;  // MTLCompareFunction 3 bits + 11 bits padding.
};
static_assert(alignof(SamplerDesc) <= 4);
static_assert(sizeof(SamplerDesc) == 4);
ANGLE_DISABLE_STRUCT_PADDING_WARNINGS

inline size_t SamplerDesc::hash() const
{
    return angle::HashMultiple<uint8_t, uint8_t, uint8_t, bool, bool, uint8_t, uint8_t, uint8_t>(
        mRAddressMode, mSAddressMode, mTAddressMode, mMinFilter, mMagFilter, mMipFilter,
        mMaxAnisotropy, mCompareFunction);
}

ANGLE_ENABLE_STRUCT_PADDING_WARNINGS
class VertexAttributeDesc
{
  public:
    constexpr VertexAttributeDesc() : mFormat(0), mOffset(0), mBufferIndex(0) {}
    constexpr VertexAttributeDesc(MTLVertexFormat format, NSUInteger offset, NSUInteger bufferIndex)
        : mFormat(static_cast<uint32_t>(format)),
          mOffset(static_cast<uint32_t>(offset)),
          mBufferIndex(static_cast<uint32_t>(bufferIndex))
    {}

    constexpr bool operator==(const VertexAttributeDesc &rhs) const = default;
    size_t hash() const;

    MTLVertexFormat getFormat() const { return static_cast<MTLVertexFormat>(mFormat); }
    NSUInteger getOffset() const { return static_cast<NSUInteger>(mOffset); }
    NSUInteger getBufferIndex() const { return static_cast<NSUInteger>(mBufferIndex); }

  private:
    uint32_t mFormat : 8;  // MTLVertexFormat.
    // Offset is only used for default attributes buffer. So 8 bits are enough.
    uint32_t mOffset : 8;        // NSUInteger.
    uint32_t mBufferIndex : 16;  // NSUInteger 8 bits + 8 bits padding.
};
static_assert(alignof(VertexAttributeDesc) <= 4);
static_assert(sizeof(VertexAttributeDesc) == 4);
ANGLE_DISABLE_STRUCT_PADDING_WARNINGS

inline size_t VertexAttributeDesc::hash() const
{
    return angle::HashMultiple<uint8_t, uint8_t, uint8_t>(mFormat, mOffset, mBufferIndex);
}

ANGLE_ENABLE_STRUCT_PADDING_WARNINGS
class VertexBufferLayoutDesc
{
  public:
    constexpr VertexBufferLayoutDesc() = default;
    constexpr VertexBufferLayoutDesc(uint32_t stepRate, uint32_t stride, MTLVertexStepFunction func)
        : stepRate(stepRate), stride(stride), mStepFunction(static_cast<uint32_t>(func))
    {}
    constexpr bool operator==(const VertexBufferLayoutDesc &rhs) const = default;
    size_t hash() const;

    uint32_t stepRate = 0;
    uint32_t stride   = 0;

    MTLVertexStepFunction getStepFunction() const
    {
        return static_cast<MTLVertexStepFunction>(mStepFunction);
    }

  private:
    uint32_t mStepFunction = MTLVertexStepFunctionConstant;  // MTLVertexStepFunction.
};
static_assert(alignof(VertexBufferLayoutDesc) == 4);
static_assert(sizeof(VertexBufferLayoutDesc) == 12);
ANGLE_DISABLE_STRUCT_PADDING_WARNINGS

inline size_t VertexBufferLayoutDesc::hash() const
{
    return angle::HashMultiple<uint32_t, uint32_t, uint8_t>(stepRate, stride, mStepFunction);
}

ANGLE_ENABLE_STRUCT_PADDING_WARNINGS
struct VertexDesc
{
    constexpr VertexDesc() : numAttribs(0), numBufferLayouts(0) {}
    constexpr bool operator==(const VertexDesc &rhs) const;
    size_t hash() const;

    VertexAttributeDesc attributes[kMaxVertexAttribs];
    VertexBufferLayoutDesc layouts[kMaxVertexAttribs];

    uint16_t numAttribs       = 0;
    uint16_t numBufferLayouts = 0;
};
static_assert(alignof(VertexDesc) == 4);
static_assert(sizeof(VertexDesc) == 260);
ANGLE_DISABLE_STRUCT_PADDING_WARNINGS

constexpr inline bool VertexDesc::operator==(const VertexDesc &rhs) const
{
    if (numAttribs != rhs.numAttribs || numBufferLayouts != rhs.numBufferLayouts)
    {
        return false;
    }
    for (uint8_t i = 0; i < numAttribs; ++i)
    {
        if (attributes[i] != rhs.attributes[i])
        {
            return false;
        }
    }
    for (uint8_t i = 0; i < numBufferLayouts; ++i)
    {
        if (layouts[i] != rhs.layouts[i])
        {
            return false;
        }
    }
    return true;
}

inline size_t VertexDesc::hash() const
{
    size_t hash = 0;
    angle::HashCombine(hash, numAttribs);
    angle::HashCombine(hash, numBufferLayouts);
    for (uint8_t i = 0; i < numAttribs; ++i)
    {
        angle::HashCombine(hash, attributes[i]);
    }
    for (uint8_t i = 0; i < numBufferLayouts; ++i)
    {
        angle::HashCombine(hash, layouts[i]);
    }
    return hash;
}

ANGLE_ENABLE_STRUCT_PADDING_WARNINGS
class BlendDesc
{
  public:
    constexpr BlendDesc()
        : mWriteMask(MTLColorWriteMaskAll),
          mSourceRgbBlendFactor(MTLBlendFactorOne),
          mSourceAlphaBlendFactor(MTLBlendFactorOne),
          mDestinationRgbBlendFactor(MTLBlendFactorZero),
          mDestinationAlphaBlendFactor(MTLBlendFactorZero),
          mRgbBlendOperation(MTLBlendOperationAdd),
          mAlphaBlendOperation(MTLBlendOperationAdd),
          mBlendingEnabled(false)
    {}
    constexpr bool operator==(const BlendDesc &rhs) const = default;
    size_t hash() const;

    // Set default values
    void reset(MTLColorWriteMask mask)
    {
        *this      = {};
        mWriteMask = static_cast<uint32_t>(mask);
    }
    void updateWriteMask(uint8_t angleMask);
    void setBlendingEnabled(MTLBlendFactor sourceRgb,
                            MTLBlendFactor sourceAlpha,
                            MTLBlendFactor destRgb,
                            MTLBlendFactor destAlpha,
                            MTLBlendOperation opRgb,
                            MTLBlendOperation opAlpha)
    {
        mSourceRgbBlendFactor        = static_cast<uint32_t>(sourceRgb);
        mSourceAlphaBlendFactor      = static_cast<uint32_t>(sourceAlpha);
        mDestinationRgbBlendFactor   = static_cast<uint32_t>(destRgb);
        mDestinationAlphaBlendFactor = static_cast<uint32_t>(destAlpha);
        mRgbBlendOperation           = static_cast<uint32_t>(opRgb);
        mAlphaBlendOperation         = static_cast<uint32_t>(opAlpha);
        mBlendingEnabled             = true;
    }
    void setBlendingDisabled()
    {
        mSourceRgbBlendFactor        = MTLBlendFactorOne;
        mSourceAlphaBlendFactor      = MTLBlendFactorOne;
        mDestinationRgbBlendFactor   = MTLBlendFactorZero;
        mDestinationAlphaBlendFactor = MTLBlendFactorZero;
        mRgbBlendOperation           = MTLBlendOperationAdd;
        mAlphaBlendOperation         = MTLBlendOperationAdd;
        mBlendingEnabled             = false;
    }
    void setWriteMask(MTLColorWriteMask mask) { mWriteMask = static_cast<uint32_t>(mask); }
    MTLColorWriteMask getWriteMask() const { return static_cast<uint8_t>(mWriteMask); }
    MTLBlendFactor getSourceRgbBlendFactor() const
    {
        return static_cast<MTLBlendFactor>(mSourceRgbBlendFactor);
    }
    MTLBlendFactor getSourceAlphaBlendFactor() const
    {
        return static_cast<MTLBlendFactor>(mSourceAlphaBlendFactor);
    }
    MTLBlendFactor getDestinationRgbBlendFactor() const
    {
        return static_cast<MTLBlendFactor>(mDestinationRgbBlendFactor);
    }
    MTLBlendFactor getDestinationAlphaBlendFactor() const
    {
        return static_cast<MTLBlendFactor>(mDestinationAlphaBlendFactor);
    }
    MTLBlendOperation getRgbBlendOperation() const
    {
        return static_cast<MTLBlendOperation>(mRgbBlendOperation);
    }
    MTLBlendOperation getAlphaBlendOperation() const
    {
        return static_cast<MTLBlendOperation>(mAlphaBlendOperation);
    }
    bool isBlendingEnabled() const { return mBlendingEnabled; }

  protected:
    uint32_t mWriteMask : 4;                    // MTLColorWriteMask.
    uint32_t mSourceRgbBlendFactor : 5;         // MTLBlendFactor.
    uint32_t mSourceAlphaBlendFactor : 5;       // MTLBlendFactor
    uint32_t mDestinationRgbBlendFactor : 5;    // MTLBlendFactor
    uint32_t mDestinationAlphaBlendFactor : 5;  // MTLBlendFactor
    uint32_t mRgbBlendOperation : 3;            // MTLBlendOperation.
    uint32_t mAlphaBlendOperation : 3;          // MTLBlendOperation.
    uint32_t mBlendingEnabled : 2;              // bool + 1 bit padding.
};
static_assert(alignof(BlendDesc) <= 4);
static_assert(sizeof(BlendDesc) == 4);
ANGLE_DISABLE_STRUCT_PADDING_WARNINGS

inline size_t BlendDesc::hash() const
{
    return angle::HashMultiple<uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, bool>(
        mWriteMask, mSourceRgbBlendFactor, mSourceAlphaBlendFactor, mDestinationRgbBlendFactor,
        mDestinationAlphaBlendFactor, mRgbBlendOperation, mAlphaBlendOperation, mBlendingEnabled);
}

using BlendDescArray = std::array<BlendDesc, kMaxRenderTargets>;
using WriteMaskArray = std::array<uint8_t, kMaxRenderTargets>;

ANGLE_ENABLE_STRUCT_PADDING_WARNINGS
class RenderPipelineColorAttachmentDesc : public BlendDesc
{
  public:
    constexpr bool operator==(const RenderPipelineColorAttachmentDesc &rhs) const = default;
    size_t hash() const;

    // Set default values
    void reset(MTLPixelFormat format)
    {
        *this        = {};
        mPixelFormat = static_cast<uint32_t>(format);
    }
    void reset(MTLPixelFormat format, MTLColorWriteMask writeMask);
    void reset(MTLPixelFormat format, const BlendDesc &blendDesc);

    void setPixelFormat(MTLPixelFormat format) { mPixelFormat = static_cast<uint32_t>(format); }
    MTLPixelFormat getPixelFormat() const { return static_cast<MTLPixelFormat>(mPixelFormat); }

  private:
    uint32_t mPixelFormat = MTLPixelFormatInvalid;  // MTLPixelFormat + 16 bits padding.
};
static_assert(alignof(RenderPipelineColorAttachmentDesc) == 4);
static_assert(sizeof(RenderPipelineColorAttachmentDesc) == 8);
ANGLE_DISABLE_STRUCT_PADDING_WARNINGS

inline size_t RenderPipelineColorAttachmentDesc::hash() const
{
    size_t hash = BlendDesc::hash();
    angle::HashCombine(hash, static_cast<uint16_t>(mPixelFormat));
    return hash;
}

ANGLE_ENABLE_STRUCT_PADDING_WARNINGS
class RenderPipelineOutputDesc
{
  public:
    constexpr RenderPipelineOutputDesc()
        : mDepthAttachmentPixelFormat(0),
          mStencilAttachmentPixelFormat(0),
          mNumColorAttachments(0),
          mRasterSampleCount(1)
    {}
    bool operator==(const RenderPipelineOutputDesc &rhs) const;
    size_t hash() const;

    void updateEnabledDrawBuffers(gl::DrawBufferMask enabledBuffers);

    std::array<RenderPipelineColorAttachmentDesc, kMaxRenderTargets> colorAttachments;

    void setDepthAttachmentPixelFormat(MTLPixelFormat value)
    {
        mDepthAttachmentPixelFormat = static_cast<uint32_t>(value);
    }
    MTLPixelFormat getDepthAttachmentPixelFormat() const
    {
        return static_cast<MTLPixelFormat>(mDepthAttachmentPixelFormat);
    }
    void setStencilAttachmentPixelFormat(MTLPixelFormat value)
    {
        mStencilAttachmentPixelFormat = static_cast<uint32_t>(value);
    }
    MTLPixelFormat getStencilAttachmentPixelFormat() const
    {
        return static_cast<MTLPixelFormat>(mStencilAttachmentPixelFormat);
    }
    void setNumColorAttachments(NSUInteger value)
    {
        mNumColorAttachments = static_cast<uint32_t>(value);
    }
    NSUInteger getNumColorAttachments() const
    {
        return static_cast<NSUInteger>(mNumColorAttachments);
    }
    void setRasterSampleCount(NSUInteger value)
    {
        mRasterSampleCount = static_cast<uint32_t>(value);
    }
    NSUInteger getRasterSampleCount() const { return static_cast<NSUInteger>(mRasterSampleCount); }

  private:
    uint32_t mDepthAttachmentPixelFormat : 16;    // MTLPixelFormat.
    uint32_t mStencilAttachmentPixelFormat : 16;  // MTLPixelFormat.
    uint32_t mNumColorAttachments : 16;           // NSUInteger.
    uint32_t mRasterSampleCount : 16;             // NSUInteger.
};
static_assert(alignof(RenderPipelineOutputDesc) == 4);
static_assert(sizeof(RenderPipelineOutputDesc) == 72);
ANGLE_DISABLE_STRUCT_PADDING_WARNINGS

enum class RenderPipelineRasterization : uint32_t
{
    // This flag is used for vertex shader not writing any stage output (e.g gl_Position).
    // This will disable fragment shader stage. This is useful for transform feedback ouput vertex
    // shader.
    Disabled,

    // Fragment shader is enabled.
    Enabled,

    // This flag is for rasterization discard emulation when vertex shader still writes to stage
    // output. Disabled flag cannot be used in this case since Metal doesn't allow that. The
    // emulation would insert a code snippet to move gl_Position out of clip space's visible area to
    // simulate the discard.
    EmulatedDiscard,

    EnumCount,
};

template <typename T>
using RenderPipelineRasterStateMap = angle::PackedEnumMap<RenderPipelineRasterization, T>;

ANGLE_ENABLE_STRUCT_PADDING_WARNINGS
class RenderPipelineDesc
{
  public:
    constexpr RenderPipelineDesc()
        : mInputPrimitiveTopology(MTLPrimitiveTopologyClassUnspecified),
          mAlphaToCoverageEnabled(false),
          mRasterizationType(static_cast<uint32_t>(RenderPipelineRasterization::Enabled))
    {}
    bool operator==(const RenderPipelineDesc &rhs) const = default;
    size_t hash() const;
    bool rasterizationEnabled() const;

    angle::ObjCPtr<MTLRenderPipelineDescriptor> createMetalDesc(
        id<MTLFunction> vertexShader,
        id<MTLFunction> fragmentShader) const;

    VertexDesc vertexDescriptor;
    RenderPipelineOutputDesc outputDescriptor;

    void setInputPrimitiveTopology(MTLPrimitiveTopologyClass value)
    {
        mInputPrimitiveTopology = static_cast<uint32_t>(value);
    }
    MTLPrimitiveTopologyClass getInputPrimitiveTopology() const
    {
        return static_cast<MTLPrimitiveTopologyClass>(mInputPrimitiveTopology);
    }
    void setAlphaToCoverageEnabled(bool value)
    {
        mAlphaToCoverageEnabled = static_cast<uint32_t>(value);
    }
    bool getAlphaToCoverageEnabled() const { return mAlphaToCoverageEnabled; }
    void setRasterizationType(RenderPipelineRasterization value)
    {
        mRasterizationType = static_cast<uint32_t>(value);
    }
    RenderPipelineRasterization getRasterizationType() const
    {
        return static_cast<RenderPipelineRasterization>(mRasterizationType);
    }

  private:
    uint32_t mInputPrimitiveTopology : 8;  // MTLPrimitiveTopologyClass.
    uint32_t mAlphaToCoverageEnabled : 8;  // bool.

    // These flags are for emulation and do not correspond to any flags in
    // MTLRenderPipelineDescriptor descriptor. These flags should be used by
    // RenderPipelineCacheSpecializeShaderFactory.
    uint32_t mRasterizationType : 16;  // RenderPipelineRasterization + padding.
};
static_assert(alignof(RenderPipelineDesc) <= 4);
static_assert(sizeof(RenderPipelineDesc) == 336);
ANGLE_DISABLE_STRUCT_PADDING_WARNINGS

inline size_t RenderPipelineDesc::hash() const
{
    return angle::HashMultiple(
        vertexDescriptor, outputDescriptor, static_cast<uint8_t>(mInputPrimitiveTopology),
        static_cast<bool>(mAlphaToCoverageEnabled), static_cast<uint8_t>(mRasterizationType));
}

struct RenderPassAttachmentDesc
{
    bool equalIgnoreLoadStoreOptions(const RenderPassAttachmentDesc &other) const;
    bool operator==(const RenderPassAttachmentDesc &other) const;

    ANGLE_INLINE bool hasResolveTexture() const { return resolveTexture.get(); }

    // When rendering with implicit multisample, |texture| is the texture that
    // will be rendered into and discarded at the end of a render pass. Its
    // result will be automatically resolved into |resolveTexture|.
    TextureRef texture;
    // Implicit multisample texture that will be rendered into and discarded at the end of
    // a render pass. Its result will be resolved into normal texture above.
    TextureRef resolveTexture;
    MipmapNativeLevel level = mtl::kZeroNativeMipLevel;
    uint32_t sliceOrDepth   = 0;

    MipmapNativeLevel resolveLevel;
    uint32_t resolveSliceOrDepth;

    // This attachment is blendable or not.
    bool blendable                           = false;
    MTLLoadAction loadAction                 = MTLLoadActionLoad;
    MTLStoreAction storeAction               = MTLStoreActionStore;
    MTLStoreActionOptions storeActionOptions = MTLStoreActionOptionNone;
};

struct RenderPassColorAttachmentDesc : public RenderPassAttachmentDesc
{
    inline bool operator==(const RenderPassColorAttachmentDesc &other) const
    {
        return RenderPassAttachmentDesc::operator==(other) && clearColor == other.clearColor;
    }
    inline bool operator!=(const RenderPassColorAttachmentDesc &other) const
    {
        return !(*this == other);
    }
    MTLClearColor clearColor = {0, 0, 0, 0};
};

struct RenderPassDepthAttachmentDesc : public RenderPassAttachmentDesc
{
    inline bool operator==(const RenderPassDepthAttachmentDesc &other) const
    {
        return RenderPassAttachmentDesc::operator==(other) && clearDepth == other.clearDepth;
    }
    inline bool operator!=(const RenderPassDepthAttachmentDesc &other) const
    {
        return !(*this == other);
    }

    double clearDepth = 1.0;
};

struct RenderPassStencilAttachmentDesc : public RenderPassAttachmentDesc
{
    inline bool operator==(const RenderPassStencilAttachmentDesc &other) const
    {
        return RenderPassAttachmentDesc::operator==(other) && clearStencil == other.clearStencil;
    }
    inline bool operator!=(const RenderPassStencilAttachmentDesc &other) const
    {
        return !(*this == other);
    }
    uint32_t clearStencil = 0;
};

//
// This is C++ equivalent of Objective-C MTLRenderPassDescriptor.
// We could use MTLRenderPassDescriptor directly, however, using C++ struct has benefits of fast
// copy, stack allocation, inlined comparing function, etc.
//
struct RenderPassDesc
{
    std::array<RenderPassColorAttachmentDesc, kMaxRenderTargets> colorAttachments;
    RenderPassDepthAttachmentDesc depthAttachment;
    RenderPassStencilAttachmentDesc stencilAttachment;

    void convertToMetalDesc(MTLRenderPassDescriptor *objCDesc,
                            uint32_t deviceMaxRenderTargets) const;

    // This will populate the RenderPipelineOutputDesc with default blend state and
    // MTLColorWriteMaskAll
    void populateRenderPipelineOutputDesc(RenderPipelineOutputDesc *outDesc) const;
    // This will populate the RenderPipelineOutputDesc with default blend state and the specified
    // MTLColorWriteMask
    void populateRenderPipelineOutputDesc(const WriteMaskArray &writeMaskArray,
                                          RenderPipelineOutputDesc *outDesc) const;
    // This will populate the RenderPipelineOutputDesc with the specified blend state
    void populateRenderPipelineOutputDesc(const BlendDescArray &blendDescArray,
                                          RenderPipelineOutputDesc *outDesc) const;

    bool equalIgnoreLoadStoreOptions(const RenderPassDesc &other) const;
    bool operator==(const RenderPassDesc &other) const;
    inline bool operator!=(const RenderPassDesc &other) const { return !(*this == other); }

    uint32_t numColorAttachments = 0;
    uint32_t rasterSampleCount   = 1;
    uint32_t defaultWidth        = 0;
    uint32_t defaultHeight       = 0;
};

}  // namespace mtl
}  // namespace rx

namespace std
{

template <>
struct hash<rx::mtl::StencilDesc>
{
    size_t operator()(const rx::mtl::StencilDesc &key) const { return key.hash(); }
};

template <>
struct hash<rx::mtl::DepthStencilDesc>
{
    size_t operator()(const rx::mtl::DepthStencilDesc &key) const { return key.hash(); }
};

template <>
struct hash<rx::mtl::SamplerDesc>
{
    size_t operator()(const rx::mtl::SamplerDesc &key) const { return key.hash(); }
};

template <>
struct hash<rx::mtl::VertexAttributeDesc>
{
    size_t operator()(const rx::mtl::VertexAttributeDesc &key) const { return key.hash(); }
};

template <>
struct hash<rx::mtl::VertexBufferLayoutDesc>
{
    size_t operator()(const rx::mtl::VertexBufferLayoutDesc &key) const { return key.hash(); }
};

template <>
struct hash<rx::mtl::VertexDesc>
{
    size_t operator()(const rx::mtl::VertexDesc &key) const { return key.hash(); }
};

template <>
struct hash<rx::mtl::BlendDesc>
{
    size_t operator()(const rx::mtl::BlendDesc &key) const { return key.hash(); }
};

template <>
struct hash<rx::mtl::RenderPipelineColorAttachmentDesc>
{
    size_t operator()(const rx::mtl::RenderPipelineColorAttachmentDesc &key) const
    {
        return key.hash();
    }
};

template <>
struct hash<rx::mtl::RenderPipelineOutputDesc>
{
    size_t operator()(const rx::mtl::RenderPipelineOutputDesc &key) const { return key.hash(); }
};

template <>
struct hash<rx::mtl::RenderPipelineDesc>
{
    size_t operator()(const rx::mtl::RenderPipelineDesc &key) const { return key.hash(); }
};

}  // namespace std

namespace rx
{
namespace mtl
{

class StateCache final : angle::NonCopyable
{
  public:
    StateCache(const angle::FeaturesMtl &features);
    ~StateCache();

    // Null depth stencil state has depth/stecil read & write disabled.
    angle::ObjCPtr<id<MTLDepthStencilState>> getNullDepthStencilState(
        const mtl::ContextDevice &device);
    angle::ObjCPtr<id<MTLDepthStencilState>> getDepthStencilState(const mtl::ContextDevice &device,
                                                                  const DepthStencilDesc &desc);
    angle::ObjCPtr<id<MTLSamplerState>> getSamplerState(const mtl::ContextDevice &device,
                                                        const SamplerDesc &desc);
    // Null sampler state uses default SamplerDesc
    angle::ObjCPtr<id<MTLSamplerState>> getNullSamplerState(ContextMtl *context);
    angle::ObjCPtr<id<MTLSamplerState>> getNullSamplerState(const mtl::ContextDevice &device);
    void clear();

  private:
    const angle::FeaturesMtl &mFeatures;

    angle::ObjCPtr<id<MTLDepthStencilState>> mNullDepthStencilState = nil;
    angle::HashMap<DepthStencilDesc, angle::ObjCPtr<id<MTLDepthStencilState>>> mDepthStencilStates;
    angle::HashMap<SamplerDesc, angle::ObjCPtr<id<MTLSamplerState>>> mSamplerStates;
};

}  // namespace mtl
}  // namespace rx

static inline bool operator==(const MTLClearColor &lhs, const MTLClearColor &rhs)
{
    return lhs.red == rhs.red && lhs.green == rhs.green && lhs.blue == rhs.blue &&
           lhs.alpha == rhs.alpha;
}

#endif /* LIBANGLE_RENDERER_METAL_MTL_STATE_CACHE_H_ */
