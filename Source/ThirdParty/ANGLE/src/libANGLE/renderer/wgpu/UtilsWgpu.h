// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// UtilsWgpu.h:
//   Defines the UtilsWgpu class, a helper class for image copies with a draw.

#ifndef LIBANGLE_RENDERER_WGPU_UTILSWGPU_H_
#define LIBANGLE_RENDERER_WGPU_UTILSWGPU_H_

#include <map>
#include <tuple>

#include "libANGLE/Constants.h"
#include "libANGLE/renderer/Format.h"
#include "libANGLE/renderer/FormatID_autogen.h"
#include "libANGLE/renderer/RenderTargetCache.h"
#include "libANGLE/renderer/wgpu/wgpu_utils.h"

namespace rx
{
class ContextWgpu;
class RenderTargetWgpu;
namespace webgpu
{

class ImageHelper;

enum class WgpuPipelineOp : uint8_t
{
    ImageCopy,
    ColorBlit,
};

struct CopyVertex
{
    float position[2];
    float texCoord[2];
};

struct CopyKey
{
    GLenum srcComponentType;
    angle::FormatID dstActualFormatID;
    WgpuPipelineOp op;
    bool dstIntentedFormatHasAlphaBits;
    bool premultiplyAlpha;
    bool unmultiplyAlpha;
    bool srcFlipY;
    bool dstFlipY;

    bool operator<(const CopyKey &other) const
    {
        return std::tie(op, srcComponentType, dstActualFormatID, dstIntentedFormatHasAlphaBits,
                        premultiplyAlpha, unmultiplyAlpha, srcFlipY, dstFlipY) <
               std::tie(other.op, other.srcComponentType, other.dstActualFormatID,
                        other.dstIntentedFormatHasAlphaBits, other.premultiplyAlpha,
                        other.unmultiplyAlpha, other.srcFlipY, other.dstFlipY);
    }
};

struct ClearPipelineKey
{
    std::vector<angle::FormatID> actualColorFormats;
    std::vector<bool> intendedColorFormatHasAlphaBits;
    std::optional<angle::FormatID> depthStencilFormat;
    std::vector<WGPUColorWriteMask> colorMasks;
    bool clearDepth   = false;
    bool clearStencil = false;
    std::optional<uint32_t> stencilWriteMask;

    bool operator<(const ClearPipelineKey &other) const
    {
        return std::tie(actualColorFormats, intendedColorFormatHasAlphaBits, depthStencilFormat,
                        colorMasks, clearDepth, clearStencil, stencilWriteMask) <
               std::tie(other.actualColorFormats, other.intendedColorFormatHasAlphaBits,
                        other.depthStencilFormat, other.colorMasks, other.clearDepth,
                        other.clearStencil, other.stencilWriteMask);
    }
};

struct BlitKey
{
    WgpuPipelineOp op;
    GLenum srcComponentType;
    angle::FormatID dstActualFormatID;
    uint32_t srcSamples;
    bool dstIntentedFormatHasAlphaBits;
    GLenum filter;
    uint8_t stencilWriteMask;

    bool operator<(const BlitKey &other) const
    {
        return std::tie(op, srcComponentType, dstActualFormatID, srcSamples,
                        dstIntentedFormatHasAlphaBits, filter, stencilWriteMask) <
               std::tie(other.op, other.srcComponentType, other.dstActualFormatID, other.srcSamples,
                        other.dstIntentedFormatHasAlphaBits, other.filter, other.stencilWriteMask);
    }
};

struct CachedPipeline
{
    webgpu::RenderPipelineHandle pipeline;
    webgpu::BindGroupLayoutHandle bindGroupLayout;
};

using CopyPipelineCache  = std::map<CopyKey, CachedPipeline>;
using BlitPipelineCache  = std::map<BlitKey, CachedPipeline>;
using ClearPipelineCache = std::map<ClearPipelineKey, CachedPipeline>;

class UtilsWgpu : angle::NonCopyable
{
  public:
    struct ClearParams
    {
        // `clearArea` should be already flipped if the `colorTargets` require a flipped y.
        gl::Rectangle clearArea;
        gl::BlendStateExt::ColorMaskStorage::Type colorMasks;
        gl::DrawBufferMask clearColorBuffers;
        std::optional<gl::ColorF> clearColorValue;
        std::optional<float> clearDepthValue;
        std::optional<uint32_t> clearStencilValue;
        std::optional<uint32_t> stencilWriteMask;
        const RenderTargetCache<RenderTargetWgpu>::RenderTargetArray *colorTargets;
        RenderTargetWgpu *depthStencilTarget;
    };

    UtilsWgpu();
    ~UtilsWgpu();

    angle::Result copyImage(ContextWgpu *context,
                            webgpu::TextureViewHandle src,
                            webgpu::TextureViewHandle dst,
                            const gl::Rectangle &sourceArea,
                            const gl::Offset &destOffset,
                            const WGPUExtent3D &srcSize,
                            const WGPUExtent3D &dstSize,
                            bool premultiplyAlpha,
                            bool unmultiplyAlpha,
                            bool srcFlipY,
                            bool dstFlipY,
                            const angle::Format &srcFormat,
                            angle::FormatID dstIntendedFormatID,
                            angle::FormatID dstActualFormatID,
                            const gl::Rectangle *scissor);

    angle::Result blit(ContextWgpu *context,
                       webgpu::TextureViewHandle src,
                       webgpu::TextureViewHandle dst,
                       const gl::Rectangle &sourceArea,
                       const gl::Rectangle &destArea,
                       const WGPUExtent3D &srcSize,
                       const WGPUExtent3D &dstSize,
                       GLenum filter,
                       bool srcFlipY,
                       bool dstFlipY,
                       uint32_t srcSamples,
                       const angle::Format &srcFormat,
                       angle::FormatID dstIntendedFormatID,
                       angle::FormatID dstActualFormatID,
                       const gl::Rectangle *scissor);

    angle::Result clear(ContextWgpu *context, ClearParams params);

  private:
    webgpu::ShaderModuleHandle getCopyShaderModule(ContextWgpu *context, const CopyKey &key);
    webgpu::ShaderModuleHandle getBlitShaderModule(ContextWgpu *context, const BlitKey &key);
    webgpu::ShaderModuleHandle getClearShaderModule(ContextWgpu *context,
                                                    const ClearPipelineKey &key);
    webgpu::ShaderModuleHandle getShaderModule(ContextWgpu *context, const std::string &shader);

    angle::Result getCopyPipeline(ContextWgpu *context,
                                  GLenum srcComponentType,
                                  angle::FormatID dstFormatID,
                                  const webgpu::ShaderModuleHandle &shader,
                                  CachedPipeline *cachedPipelineOut);

    angle::Result getBlitPipeline(ContextWgpu *context,
                                  WgpuPipelineOp op,
                                  GLenum srcComponentType,
                                  angle::FormatID dstFormatID,
                                  const webgpu::ShaderModuleHandle &shader,
                                  CachedPipeline *cachedPipelineOut,
                                  uint8_t stencilWriteMask);

    angle::Result getClearPipeline(ContextWgpu *context,
                                   const ClearPipelineKey &key,
                                   const CachedPipeline **cachedPipelineOut);

    angle::Result createPipeline(ContextWgpu *context,
                                 WgpuPipelineOp op,
                                 angle::FormatID dstFormatID,
                                 const webgpu::ShaderModuleHandle &shader,
                                 webgpu::BindGroupLayoutHandle bindGroupLayout,
                                 uint8_t stencilWriteMask,
                                 CachedPipeline *cachedPipelineOut);

    // This method is intended for copying and blitting operations that use a simple quad
    // and potentially take source textures/samplers as input.
    angle::Result drawQuad(ContextWgpu *context,
                           WgpuPipelineOp op,
                           webgpu::TextureViewHandle dst,
                           const WGPUExtent3D &dstSize,
                           const CachedPipeline *cachedPipeline,
                           const webgpu::BindGroupHandle &bindGroup,
                           const gl::Rectangle &destArea,
                           const gl::Rectangle &sourceArea,
                           const WGPUExtent3D &srcSize,
                           bool srcFlipY,
                           bool dstFlipY,
                           bool srcTextureCoordsNormalized,
                           const gl::Rectangle *scissor,
                           uint32_t stencilReference);

    CopyPipelineCache mCopyPipelineCache;
    BlitPipelineCache mBlitPipelineCache;
    ClearPipelineCache mClearPipelineCache;
};

}  // namespace webgpu
}  // namespace rx

#endif  // LIBANGLE_RENDERER_WGPU_UTILSWGPU_H_
