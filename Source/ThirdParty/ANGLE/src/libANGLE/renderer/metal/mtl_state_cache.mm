//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// mtl_state_cache.mm:
//    Implements StateCache, RenderPipelineCache and various
//    C struct versions of Metal sampler, depth stencil, render pass, render pipeline descriptors.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "libANGLE/renderer/metal/mtl_state_cache.h"

#include <sstream>

#include "common/debug.h"
#include "common/hash_utils.h"
#include "common/span.h"
#include "libANGLE/renderer/metal/ContextMtl.h"
#include "libANGLE/renderer/metal/mtl_resources.h"
#include "libANGLE/renderer/metal/mtl_utils.h"
#include "platform/autogen/FeaturesMtl_autogen.h"

#define ANGLE_OBJC_CP_PROPERTY(DST, SRC, PROPERTY) \
    (DST).PROPERTY = static_cast<__typeof__((DST).PROPERTY)>(ToObjC((SRC).PROPERTY))

#define ANGLE_PROP_EQ(LHS, RHS, PROP) ((LHS).PROP == (RHS).PROP)

namespace rx
{
namespace mtl
{

namespace
{

template <class T>
inline T ToObjC(const T p)
{
    return p;
}

inline angle::ObjCPtr<MTLStencilDescriptor> ToObjC(const StencilDesc &desc)
{
    auto objCDesc = angle::adoptObjCPtr([[MTLStencilDescriptor alloc] init]);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, stencilFailureOperation);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, depthFailureOperation);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, depthStencilPassOperation);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, stencilCompareFunction);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, readMask);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, writeMask);
    return objCDesc;
}

inline angle::ObjCPtr<MTLDepthStencilDescriptor> ToObjC(const DepthStencilDesc &desc)
{
    auto objCDesc = angle::adoptObjCPtr([[MTLDepthStencilDescriptor alloc] init]);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, backFaceStencil);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, frontFaceStencil);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, depthCompareFunction);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, depthWriteEnabled);
    return objCDesc;
}

inline angle::ObjCPtr<MTLSamplerDescriptor> ToObjC(const SamplerDesc &desc)
{
    auto objCDesc = angle::adoptObjCPtr([[MTLSamplerDescriptor alloc] init]);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, rAddressMode);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, sAddressMode);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, tAddressMode);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, minFilter);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, magFilter);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, mipFilter);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, maxAnisotropy);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, compareFunction);
    return objCDesc;
}

inline angle::ObjCPtr<MTLVertexAttributeDescriptor> ToObjC(const VertexAttributeDesc &desc)
{
    auto objCDesc = angle::adoptObjCPtr([[MTLVertexAttributeDescriptor alloc] init]);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, format);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, offset);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, bufferIndex);
    ASSERT(desc.bufferIndex >= kVboBindingIndexStart);
    return objCDesc;
}

inline angle::ObjCPtr<MTLVertexBufferLayoutDescriptor> ToObjC(const VertexBufferLayoutDesc &desc)
{
    auto objCDesc = angle::adoptObjCPtr([[MTLVertexBufferLayoutDescriptor alloc] init]);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, stepFunction);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, stepRate);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, stride);
    return objCDesc;
}

inline angle::ObjCPtr<MTLVertexDescriptor> ToObjC(const VertexDesc &desc)
{
    auto objCDesc = angle::adoptObjCPtr([[MTLVertexDescriptor alloc] init]);
    [objCDesc reset];

    for (uint8_t i = 0; i < desc.numAttribs; ++i)
    {
        [objCDesc.get().attributes setObject:ToObjC(desc.attributes[i]) atIndexedSubscript:i];
    }

    for (uint8_t i = 0; i < desc.numBufferLayouts; ++i)
    {
        // Ignore if stepFunction is kVertexStepFunctionInvalid.
        // If we don't set this slot, it will apparently be disabled by metal runtime.
        if (desc.layouts[i].stepFunction != kVertexStepFunctionInvalid)
        {
            [objCDesc.get().layouts setObject:ToObjC(desc.layouts[i]) atIndexedSubscript:i];
        }
    }

    return objCDesc;
}

inline angle::ObjCPtr<MTLRenderPipelineColorAttachmentDescriptor> ToObjC(
    const RenderPipelineColorAttachmentDesc &desc)
{
    auto objCDesc = angle::adoptObjCPtr([[MTLRenderPipelineColorAttachmentDescriptor alloc] init]);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, pixelFormat);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, writeMask);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, alphaBlendOperation);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, rgbBlendOperation);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, destinationAlphaBlendFactor);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, destinationRGBBlendFactor);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, sourceAlphaBlendFactor);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, sourceRGBBlendFactor);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), desc, blendingEnabled);
    return objCDesc;
}

id<MTLTexture> ToObjC(const TextureRef &texture)
{
    auto textureRef = texture;
    return textureRef ? textureRef->get() : nil;
}

void BaseRenderPassAttachmentDescToObjC(const RenderPassAttachmentDesc &src,
                                        MTLRenderPassAttachmentDescriptor *dst)
{
    dst.texture = ToObjC(src.texture);
    dst.level   = src.level.get();
    if (dst.texture.textureType == MTLTextureType3D)
    {
        dst.slice      = 0;
        dst.depthPlane = src.sliceOrDepth;
    }
    else
    {
        dst.slice      = src.sliceOrDepth;
        dst.depthPlane = 0;
    }

    dst.resolveTexture = ToObjC(src.resolveTexture);
    dst.resolveLevel   = src.resolveLevel.get();
    if (dst.resolveTexture.textureType == MTLTextureType3D)
    {
        dst.resolveSlice      = 0;
        dst.resolveDepthPlane = src.resolveSliceOrDepth;
    }
    else
    {
        dst.resolveSlice      = src.resolveSliceOrDepth;
        dst.resolveDepthPlane = 0;
    }

    ANGLE_OBJC_CP_PROPERTY(dst, src, loadAction);
    ANGLE_OBJC_CP_PROPERTY(dst, src, storeAction);
    ANGLE_OBJC_CP_PROPERTY(dst, src, storeActionOptions);
}

void ToObjC(const RenderPassColorAttachmentDesc &desc,
            MTLRenderPassColorAttachmentDescriptor *objCDesc)
{
    BaseRenderPassAttachmentDescToObjC(desc, objCDesc);

    ANGLE_OBJC_CP_PROPERTY(objCDesc, desc, clearColor);
}

void ToObjC(const RenderPassDepthAttachmentDesc &desc,
            MTLRenderPassDepthAttachmentDescriptor *objCDesc)
{
    BaseRenderPassAttachmentDescToObjC(desc, objCDesc);

    ANGLE_OBJC_CP_PROPERTY(objCDesc, desc, clearDepth);
}

void ToObjC(const RenderPassStencilAttachmentDesc &desc,
            MTLRenderPassStencilAttachmentDescriptor *objCDesc)
{
    BaseRenderPassAttachmentDescToObjC(desc, objCDesc);

    ANGLE_OBJC_CP_PROPERTY(objCDesc, desc, clearStencil);
}

MTLColorWriteMask AdjustColorWriteMaskForSharedExponent(MTLColorWriteMask mask)
{
    // For RGB9_E5 color buffers, ANGLE frontend validation ignores alpha writemask value.
    // Metal validation is more strict and allows only all-enabled or all-disabled.
    ASSERT((mask == MTLColorWriteMaskAll) ||
           (mask == (MTLColorWriteMaskAll ^ MTLColorWriteMaskAlpha)) ||
           (mask == MTLColorWriteMaskAlpha) || (mask == MTLColorWriteMaskNone));
    return (mask & MTLColorWriteMaskBlue) ? MTLColorWriteMaskAll : MTLColorWriteMaskNone;
}

}  // namespace

// StencilDesc implementation
bool StencilDesc::operator==(const StencilDesc &rhs) const
{
    return ANGLE_PROP_EQ(*this, rhs, stencilFailureOperation) &&
           ANGLE_PROP_EQ(*this, rhs, depthFailureOperation) &&
           ANGLE_PROP_EQ(*this, rhs, depthStencilPassOperation) &&

           ANGLE_PROP_EQ(*this, rhs, stencilCompareFunction) &&

           ANGLE_PROP_EQ(*this, rhs, readMask) && ANGLE_PROP_EQ(*this, rhs, writeMask);
}

void StencilDesc::reset()
{
    stencilFailureOperation = depthFailureOperation = depthStencilPassOperation =
        MTLStencilOperationKeep;

    stencilCompareFunction = MTLCompareFunctionAlways;
    readMask = writeMask = std::numeric_limits<uint32_t>::max() & mtl::kStencilMaskAll;
}

// DepthStencilDesc implementation
DepthStencilDesc::DepthStencilDesc()
{
    memset(this, 0, sizeof(*this));
}
DepthStencilDesc::DepthStencilDesc(const DepthStencilDesc &src)
{
    memcpy(this, &src, sizeof(*this));
}
DepthStencilDesc::DepthStencilDesc(DepthStencilDesc &&src)
{
    memcpy(this, &src, sizeof(*this));
}

DepthStencilDesc &DepthStencilDesc::operator=(const DepthStencilDesc &src)
{
    memcpy(this, &src, sizeof(*this));
    return *this;
}

bool DepthStencilDesc::operator==(const DepthStencilDesc &rhs) const
{
    return ANGLE_PROP_EQ(*this, rhs, backFaceStencil) &&
           ANGLE_PROP_EQ(*this, rhs, frontFaceStencil) &&

           ANGLE_PROP_EQ(*this, rhs, depthCompareFunction) &&

           ANGLE_PROP_EQ(*this, rhs, depthWriteEnabled);
}

void DepthStencilDesc::reset()
{
    frontFaceStencil.reset();
    backFaceStencil.reset();

    depthCompareFunction = MTLCompareFunctionAlways;
    depthWriteEnabled    = true;
}

void DepthStencilDesc::updateDepthTestEnabled(const gl::DepthStencilState &dsState)
{
    if (!dsState.depthTest)
    {
        depthCompareFunction = MTLCompareFunctionAlways;
        depthWriteEnabled    = false;
    }
    else
    {
        updateDepthCompareFunc(dsState);
        updateDepthWriteEnabled(dsState);
    }
}

void DepthStencilDesc::updateDepthWriteEnabled(const gl::DepthStencilState &dsState)
{
    depthWriteEnabled = dsState.depthTest && dsState.depthMask;
}

void DepthStencilDesc::updateDepthCompareFunc(const gl::DepthStencilState &dsState)
{
    if (!dsState.depthTest)
    {
        return;
    }
    depthCompareFunction = GetCompareFunc(dsState.depthFunc);
}

void DepthStencilDesc::updateStencilTestEnabled(const gl::DepthStencilState &dsState)
{
    if (!dsState.stencilTest)
    {
        frontFaceStencil.stencilCompareFunction    = MTLCompareFunctionAlways;
        frontFaceStencil.depthFailureOperation     = MTLStencilOperationKeep;
        frontFaceStencil.depthStencilPassOperation = MTLStencilOperationKeep;
        frontFaceStencil.writeMask                 = 0;

        backFaceStencil.stencilCompareFunction    = MTLCompareFunctionAlways;
        backFaceStencil.depthFailureOperation     = MTLStencilOperationKeep;
        backFaceStencil.depthStencilPassOperation = MTLStencilOperationKeep;
        backFaceStencil.writeMask                 = 0;
    }
    else
    {
        updateStencilFrontFuncs(dsState);
        updateStencilFrontOps(dsState);
        updateStencilFrontWriteMask(dsState);
        updateStencilBackFuncs(dsState);
        updateStencilBackOps(dsState);
        updateStencilBackWriteMask(dsState);
    }
}

void DepthStencilDesc::updateStencilFrontOps(const gl::DepthStencilState &dsState)
{
    if (!dsState.stencilTest)
    {
        return;
    }
    frontFaceStencil.stencilFailureOperation   = GetStencilOp(dsState.stencilFail);
    frontFaceStencil.depthFailureOperation     = GetStencilOp(dsState.stencilPassDepthFail);
    frontFaceStencil.depthStencilPassOperation = GetStencilOp(dsState.stencilPassDepthPass);
}

void DepthStencilDesc::updateStencilBackOps(const gl::DepthStencilState &dsState)
{
    if (!dsState.stencilTest)
    {
        return;
    }
    backFaceStencil.stencilFailureOperation   = GetStencilOp(dsState.stencilBackFail);
    backFaceStencil.depthFailureOperation     = GetStencilOp(dsState.stencilBackPassDepthFail);
    backFaceStencil.depthStencilPassOperation = GetStencilOp(dsState.stencilBackPassDepthPass);
}

void DepthStencilDesc::updateStencilFrontFuncs(const gl::DepthStencilState &dsState)
{
    if (!dsState.stencilTest)
    {
        return;
    }
    frontFaceStencil.stencilCompareFunction = GetCompareFunc(dsState.stencilFunc);
    frontFaceStencil.readMask               = dsState.stencilMask & mtl::kStencilMaskAll;
}

void DepthStencilDesc::updateStencilBackFuncs(const gl::DepthStencilState &dsState)
{
    if (!dsState.stencilTest)
    {
        return;
    }
    backFaceStencil.stencilCompareFunction = GetCompareFunc(dsState.stencilBackFunc);
    backFaceStencil.readMask               = dsState.stencilBackMask & mtl::kStencilMaskAll;
}

void DepthStencilDesc::updateStencilFrontWriteMask(const gl::DepthStencilState &dsState)
{
    if (!dsState.stencilTest)
    {
        return;
    }
    frontFaceStencil.writeMask = dsState.stencilWritemask & mtl::kStencilMaskAll;
}

void DepthStencilDesc::updateStencilBackWriteMask(const gl::DepthStencilState &dsState)
{
    if (!dsState.stencilTest)
    {
        return;
    }
    backFaceStencil.writeMask = dsState.stencilBackWritemask & mtl::kStencilMaskAll;
}

size_t DepthStencilDesc::hash() const
{
    return angle::ComputeGenericHash(angle::byte_span_from_ref(*this));
}

// SamplerDesc implementation
SamplerDesc::SamplerDesc()
{
    memset(this, 0, sizeof(*this));
}
SamplerDesc::SamplerDesc(const SamplerDesc &src)
{
    memcpy(this, &src, sizeof(*this));
}
SamplerDesc::SamplerDesc(SamplerDesc &&src)
{
    memcpy(this, &src, sizeof(*this));
}

SamplerDesc::SamplerDesc(const gl::SamplerState &glState) : SamplerDesc()
{
    rAddressMode = GetSamplerAddressMode(glState.getWrapR());
    sAddressMode = GetSamplerAddressMode(glState.getWrapS());
    tAddressMode = GetSamplerAddressMode(glState.getWrapT());

    minFilter = GetFilter(glState.getMinFilter());
    magFilter = GetFilter(glState.getMagFilter());
    mipFilter = GetMipmapFilter(glState.getMinFilter());

    maxAnisotropy = static_cast<uint32_t>(glState.getMaxAnisotropy());

    compareFunction = GetCompareFunc(glState.getCompareFunc());
}

SamplerDesc &SamplerDesc::operator=(const SamplerDesc &src)
{
    memcpy(this, &src, sizeof(*this));
    return *this;
}

void SamplerDesc::reset()
{
    rAddressMode = MTLSamplerAddressModeClampToEdge;
    sAddressMode = MTLSamplerAddressModeClampToEdge;
    tAddressMode = MTLSamplerAddressModeClampToEdge;

    minFilter = MTLSamplerMinMagFilterNearest;
    magFilter = MTLSamplerMinMagFilterNearest;
    mipFilter = MTLSamplerMipFilterNearest;

    maxAnisotropy = 1;

    compareFunction = MTLCompareFunctionNever;
}

bool SamplerDesc::operator==(const SamplerDesc &rhs) const
{
    return ANGLE_PROP_EQ(*this, rhs, rAddressMode) && ANGLE_PROP_EQ(*this, rhs, sAddressMode) &&
           ANGLE_PROP_EQ(*this, rhs, tAddressMode) &&

           ANGLE_PROP_EQ(*this, rhs, minFilter) && ANGLE_PROP_EQ(*this, rhs, magFilter) &&
           ANGLE_PROP_EQ(*this, rhs, mipFilter) &&

           ANGLE_PROP_EQ(*this, rhs, maxAnisotropy) &&

           ANGLE_PROP_EQ(*this, rhs, compareFunction);
}

size_t SamplerDesc::hash() const
{
    return angle::ComputeGenericHash(angle::byte_span_from_ref(*this));
}

// BlendDesc implementation
bool BlendDesc::operator==(const BlendDesc &rhs) const
{
    return ANGLE_PROP_EQ(*this, rhs, writeMask) &&

           ANGLE_PROP_EQ(*this, rhs, alphaBlendOperation) &&
           ANGLE_PROP_EQ(*this, rhs, rgbBlendOperation) &&

           ANGLE_PROP_EQ(*this, rhs, destinationAlphaBlendFactor) &&
           ANGLE_PROP_EQ(*this, rhs, destinationRGBBlendFactor) &&
           ANGLE_PROP_EQ(*this, rhs, sourceAlphaBlendFactor) &&
           ANGLE_PROP_EQ(*this, rhs, sourceRGBBlendFactor) &&

           ANGLE_PROP_EQ(*this, rhs, blendingEnabled);
}

void BlendDesc::reset()
{
    reset(MTLColorWriteMaskAll);
}

void BlendDesc::reset(MTLColorWriteMask _writeMask)
{
    writeMask = _writeMask;

    blendingEnabled     = false;
    alphaBlendOperation = rgbBlendOperation = MTLBlendOperationAdd;

    destinationAlphaBlendFactor = destinationRGBBlendFactor = MTLBlendFactorZero;
    sourceAlphaBlendFactor = sourceRGBBlendFactor = MTLBlendFactorOne;
}

void BlendDesc::updateWriteMask(const uint8_t angleMask)
{
    ASSERT(angleMask == (angleMask & 0xF));

// ANGLE's packed color mask is abgr (matches Vulkan & D3D11), while Metal expects rgba.
#if defined(__aarch64__)
    // ARM64 can reverse bits in a single instruction
    writeMask = __builtin_bitreverse8(angleMask) >> 4;
#else
    /* On other architectures, Clang generates a polyfill that uses more
       instructions than the following expression optimized for a 4-bit value.

       (abgr * 0x41) & 0x14A:
        .......abgr +
        .abgr...... &
        00101001010 =
        ..b.r..a.g.

       (b.r..a.g.) * 0x111:
                b.r..a.g. +
            b.r..a.g..... +
        b.r..a.g......... =
        b.r.bargbarg.a.g.
              ^^^^
    */
    writeMask = ((((angleMask * 0x41) & 0x14A) * 0x111) >> 7) & 0xF;
#endif
}

// RenderPipelineColorAttachmentDesc implementation
bool RenderPipelineColorAttachmentDesc::operator==(
    const RenderPipelineColorAttachmentDesc &rhs) const
{
    if (!BlendDesc::operator==(rhs))
    {
        return false;
    }
    return ANGLE_PROP_EQ(*this, rhs, pixelFormat);
}

void RenderPipelineColorAttachmentDesc::reset()
{
    reset(MTLPixelFormatInvalid);
}

void RenderPipelineColorAttachmentDesc::reset(MTLPixelFormat format)
{
    reset(format, MTLColorWriteMaskAll);
}

void RenderPipelineColorAttachmentDesc::reset(MTLPixelFormat format, MTLColorWriteMask _writeMask)
{
    this->pixelFormat = format;

    if (format == MTLPixelFormatRGB9E5Float)
    {
        _writeMask = AdjustColorWriteMaskForSharedExponent(_writeMask);
    }

    BlendDesc::reset(_writeMask);
}

void RenderPipelineColorAttachmentDesc::reset(MTLPixelFormat format, const BlendDesc &blendDesc)
{
    this->pixelFormat = format;

    BlendDesc::operator=(blendDesc);

    if (format == MTLPixelFormatRGB9E5Float)
    {
        writeMask = AdjustColorWriteMaskForSharedExponent(writeMask);
    }
}

// RenderPipelineOutputDesc implementation
void RenderPipelineOutputDesc::updateEnabledDrawBuffers(gl::DrawBufferMask enabledBuffers)
{
    for (uint32_t colorIndex = 0; colorIndex < this->numColorAttachments; ++colorIndex)
    {
        if (!enabledBuffers.test(colorIndex))
        {
            this->colorAttachments[colorIndex].writeMask = MTLColorWriteMaskNone;
        }
    }
}

// RenderPipelineDesc implementation
RenderPipelineDesc::RenderPipelineDesc()
{
    memset(this, 0, sizeof(*this));
    outputDescriptor.rasterSampleCount = 1;
    rasterizationType                  = RenderPipelineRasterization::Enabled;
}

RenderPipelineDesc::RenderPipelineDesc(const RenderPipelineDesc &src)
{
    memcpy(this, &src, sizeof(*this));
}

RenderPipelineDesc::RenderPipelineDesc(RenderPipelineDesc &&src)
{
    memcpy(this, &src, sizeof(*this));
}

RenderPipelineDesc &RenderPipelineDesc::operator=(const RenderPipelineDesc &src)
{
    memcpy(this, &src, sizeof(*this));
    return *this;
}

bool RenderPipelineDesc::operator==(const RenderPipelineDesc &rhs) const
{
    // NOTE(hqle): Use a faster way to compare, i.e take into account
    // the number of active vertex attributes & render targets.
    // If that way is used, hash() method must be changed also.
    return memcmp(this, &rhs, sizeof(*this)) == 0;
}

size_t RenderPipelineDesc::hash() const
{
    return angle::ComputeGenericHash(angle::byte_span_from_ref(*this));
}

bool RenderPipelineDesc::rasterizationEnabled() const
{
    return rasterizationType != RenderPipelineRasterization::Disabled;
}

angle::ObjCPtr<MTLRenderPipelineDescriptor> RenderPipelineDesc::createMetalDesc(
    id<MTLFunction> vertexShader,
    id<MTLFunction> fragmentShader) const
{
    auto objCDesc = angle::adoptObjCPtr([[MTLRenderPipelineDescriptor alloc] init]);
    [objCDesc reset];

    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), *this, vertexDescriptor);

    for (uint8_t i = 0; i < outputDescriptor.numColorAttachments; ++i)
    {
        [objCDesc.get().colorAttachments setObject:ToObjC(outputDescriptor.colorAttachments[i])
                                atIndexedSubscript:i];
    }
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), outputDescriptor, depthAttachmentPixelFormat);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), outputDescriptor, stencilAttachmentPixelFormat);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), outputDescriptor, rasterSampleCount);

    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), *this, inputPrimitiveTopology);
    ANGLE_OBJC_CP_PROPERTY(objCDesc.get(), *this, alphaToCoverageEnabled);

    // rasterizationEnabled will be true for both EmulatedDiscard & Enabled.
    objCDesc.get().rasterizationEnabled = rasterizationEnabled();

    objCDesc.get().vertexFunction   = vertexShader;
    objCDesc.get().fragmentFunction = objCDesc.get().rasterizationEnabled ? fragmentShader : nil;

    return objCDesc;
}

// RenderPassDesc implementation
RenderPassAttachmentDesc::RenderPassAttachmentDesc()
{
    reset();
}

void RenderPassAttachmentDesc::reset()
{
    texture.reset();
    resolveTexture.reset();
    level               = mtl::kZeroNativeMipLevel;
    sliceOrDepth        = 0;
    resolveLevel        = mtl::kZeroNativeMipLevel;
    resolveSliceOrDepth = 0;
    blendable           = false;
    loadAction          = MTLLoadActionLoad;
    storeAction         = MTLStoreActionStore;
    storeActionOptions  = MTLStoreActionOptionNone;
}

bool RenderPassAttachmentDesc::equalIgnoreLoadStoreOptions(
    const RenderPassAttachmentDesc &other) const
{
    return texture == other.texture && resolveTexture == other.resolveTexture &&
           level == other.level && sliceOrDepth == other.sliceOrDepth &&
           blendable == other.blendable;
}

bool RenderPassAttachmentDesc::operator==(const RenderPassAttachmentDesc &other) const
{
    if (!equalIgnoreLoadStoreOptions(other))
    {
        return false;
    }

    return loadAction == other.loadAction && storeAction == other.storeAction &&
           storeActionOptions == other.storeActionOptions;
}

void RenderPassDesc::populateRenderPipelineOutputDesc(RenderPipelineOutputDesc *outDesc) const
{
    WriteMaskArray writeMaskArray;
    writeMaskArray.fill(MTLColorWriteMaskAll);
    populateRenderPipelineOutputDesc(writeMaskArray, outDesc);
}

void RenderPassDesc::populateRenderPipelineOutputDesc(const WriteMaskArray &writeMaskArray,
                                                      RenderPipelineOutputDesc *outDesc) const
{
    // Default blend state with replaced color write masks.
    BlendDescArray blendDescArray;
    for (size_t i = 0; i < blendDescArray.size(); i++)
    {
        blendDescArray[i].reset(writeMaskArray[i]);
    }
    populateRenderPipelineOutputDesc(blendDescArray, outDesc);
}

void RenderPassDesc::populateRenderPipelineOutputDesc(const BlendDescArray &blendDescArray,
                                                      RenderPipelineOutputDesc *outDesc) const
{
    RenderPipelineOutputDesc &outputDescriptor = *outDesc;
    outputDescriptor.numColorAttachments       = this->numColorAttachments;
    outputDescriptor.rasterSampleCount         = this->rasterSampleCount;
    for (uint32_t i = 0; i < this->numColorAttachments; ++i)
    {
        auto &renderPassColorAttachment = this->colorAttachments[i];
        auto texture                    = renderPassColorAttachment.texture;

        if (texture)
        {
            if (renderPassColorAttachment.blendable &&
                blendDescArray[i].writeMask != MTLColorWriteMaskNone)
            {
                // Copy parameters from blend state
                outputDescriptor.colorAttachments[i].reset(texture->pixelFormat(),
                                                           blendDescArray[i]);
            }
            else
            {
                // Disable blending if the attachment's render target doesn't support blending
                // or if all its color channels are masked out. The latter is needed because:
                //
                // * When blending is enabled and *Source1* blend factors are used, Metal
                //   requires a fragment shader to bind both primary and secondary outputs
                //
                // * ANGLE frontend validation allows draw calls on draw buffers without
                //   bound fragment outputs if all their color channels are masked out
                //
                // * When all color channels are masked out, blending has no effect anyway
                //
                // Besides disabling blending, use default values for factors and
                // operations to reduce the number of unique pipeline states.
                outputDescriptor.colorAttachments[i].reset(texture->pixelFormat(),
                                                           blendDescArray[i].writeMask);
            }

            // Combine the masks. This is useful when the texture is not supposed to have alpha
            // channel such as GL_RGB8, however, Metal doesn't natively support 24 bit RGB, so
            // we need to use RGBA texture, and then disable alpha write to this texture
            outputDescriptor.colorAttachments[i].writeMask &= texture->getColorWritableMask();
        }
        else
        {

            outputDescriptor.colorAttachments[i].blendingEnabled = false;
            outputDescriptor.colorAttachments[i].pixelFormat     = MTLPixelFormatInvalid;
        }
    }

    // Reset the unused output slots to ensure consistent hash value
    for (uint32_t i = this->numColorAttachments; i < outputDescriptor.colorAttachments.size(); ++i)
    {
        outputDescriptor.colorAttachments[i].reset();
    }

    auto depthTexture = this->depthAttachment.texture;
    outputDescriptor.depthAttachmentPixelFormat =
        depthTexture ? depthTexture->pixelFormat() : MTLPixelFormatInvalid;

    auto stencilTexture = this->stencilAttachment.texture;
    outputDescriptor.stencilAttachmentPixelFormat =
        stencilTexture ? stencilTexture->pixelFormat() : MTLPixelFormatInvalid;
}

bool RenderPassDesc::equalIgnoreLoadStoreOptions(const RenderPassDesc &other) const
{
    if (numColorAttachments != other.numColorAttachments)
    {
        return false;
    }

    for (uint32_t i = 0; i < numColorAttachments; ++i)
    {
        auto &renderPassColorAttachment = colorAttachments[i];
        auto &otherRPAttachment         = other.colorAttachments[i];
        if (!renderPassColorAttachment.equalIgnoreLoadStoreOptions(otherRPAttachment))
        {
            return false;
        }
    }

    if (defaultWidth != other.defaultWidth || defaultHeight != other.defaultHeight)
    {
        return false;
    }

    if (rasterSampleCount != other.rasterSampleCount)
    {
        return false;
    }

    return depthAttachment.equalIgnoreLoadStoreOptions(other.depthAttachment) &&
           stencilAttachment.equalIgnoreLoadStoreOptions(other.stencilAttachment);
}

bool RenderPassDesc::operator==(const RenderPassDesc &other) const
{
    if (numColorAttachments != other.numColorAttachments)
    {
        return false;
    }

    for (uint32_t i = 0; i < numColorAttachments; ++i)
    {
        auto &renderPassColorAttachment = colorAttachments[i];
        auto &otherRPAttachment         = other.colorAttachments[i];
        if (renderPassColorAttachment != (otherRPAttachment))
        {
            return false;
        }
    }

    if (defaultWidth != other.defaultWidth || defaultHeight != other.defaultHeight)
    {
        return false;
    }

    return depthAttachment == other.depthAttachment && stencilAttachment == other.stencilAttachment;
}

// Convert to Metal object
void RenderPassDesc::convertToMetalDesc(MTLRenderPassDescriptor *objCDesc,
                                        uint32_t deviceMaxRenderTargets) const
{
    ASSERT(deviceMaxRenderTargets <= kMaxRenderTargets);

    for (uint32_t i = 0; i < numColorAttachments; ++i)
    {
        ToObjC(colorAttachments[i], objCDesc.colorAttachments[i]);
    }
    for (uint32_t i = numColorAttachments; i < deviceMaxRenderTargets; ++i)
    {
        // Inactive render target
        objCDesc.colorAttachments[i].texture     = nil;
        objCDesc.colorAttachments[i].level       = 0;
        objCDesc.colorAttachments[i].slice       = 0;
        objCDesc.colorAttachments[i].depthPlane  = 0;
        objCDesc.colorAttachments[i].loadAction  = MTLLoadActionDontCare;
        objCDesc.colorAttachments[i].storeAction = MTLStoreActionDontCare;
    }

    ToObjC(depthAttachment, objCDesc.depthAttachment);
    ToObjC(stencilAttachment, objCDesc.stencilAttachment);

    if ((defaultWidth | defaultHeight) != 0)
    {
        objCDesc.renderTargetWidth        = defaultWidth;
        objCDesc.renderTargetHeight       = defaultHeight;
        objCDesc.defaultRasterSampleCount = 1;
    }
}

// StateCache implementation
StateCache::StateCache(const angle::FeaturesMtl &features) : mFeatures(features) {}

StateCache::~StateCache() {}

angle::ObjCPtr<id<MTLDepthStencilState>> StateCache::getNullDepthStencilState(
    const mtl::ContextDevice &device)
{
    if (!mNullDepthStencilState)
    {
        DepthStencilDesc desc;
        desc.reset();
        ASSERT(desc.frontFaceStencil.stencilCompareFunction == MTLCompareFunctionAlways);
        desc.depthWriteEnabled = false;
        mNullDepthStencilState = getDepthStencilState(device, desc);
    }
    return mNullDepthStencilState;
}

angle::ObjCPtr<id<MTLDepthStencilState>> StateCache::getDepthStencilState(
    const mtl::ContextDevice &device,
    const DepthStencilDesc &desc)
{
    auto ite = mDepthStencilStates.find(desc);
    if (ite == mDepthStencilStates.end())
    {
        auto re = mDepthStencilStates.insert(
            std::make_pair(desc, device.newDepthStencilStateWithDescriptor(ToObjC(desc))));
        if (!re.second)
        {
            return nil;
        }

        ite = re.first;
    }

    return ite->second;
}

angle::ObjCPtr<id<MTLSamplerState>> StateCache::getSamplerState(const mtl::ContextDevice &device,
                                                                const SamplerDesc &desc)
{
    auto ite = mSamplerStates.find(desc);
    if (ite == mSamplerStates.end())
    {
        auto objCDesc = ToObjC(desc);
        if (!mFeatures.allowRuntimeSamplerCompareMode.enabled)
        {
            // Runtime sampler compare mode is not supported, fallback to never.
            objCDesc.get().compareFunction = MTLCompareFunctionNever;
        }
        auto re = mSamplerStates.insert(
            std::make_pair(desc, device.newSamplerStateWithDescriptor(objCDesc)));
        if (!re.second)
            return nil;

        ite = re.first;
    }

    return ite->second;
}

angle::ObjCPtr<id<MTLSamplerState>> StateCache::getNullSamplerState(ContextMtl *context)
{
    return getNullSamplerState(context->getMetalDevice());
}

angle::ObjCPtr<id<MTLSamplerState>> StateCache::getNullSamplerState(
    const mtl::ContextDevice &device)
{
    SamplerDesc desc;
    desc.reset();

    return getSamplerState(device, desc);
}

void StateCache::clear()
{
    mNullDepthStencilState = nil;
    mDepthStencilStates.clear();
    mSamplerStates.clear();
}

}  // namespace mtl
}  // namespace rx
