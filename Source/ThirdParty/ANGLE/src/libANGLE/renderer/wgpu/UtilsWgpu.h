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

// The vertices of a blit carry the mapping from destination pixels to source texels, which the
// fragment shader applies to its position, so that the source coordinate of each pixel center is
// not interpolated. GL_NEAREST uses the exact integer mapping in |nearest|, see
// UtilsWgpu::BlitParams::NearestAxis.
struct BlitVertex
{
    float position[2];
    float texScale[2];
    float texOffset[2];
    // (q0.x, q0.y, a.x, a.y), (r0.x, r0.y, b.x, b.y), (denominator.x, denominator.y, srcBase.x,
    // srcBase.y), (srcSign.x, srcSign.y, dstOrigin.x, dstOrigin.y).
    int32_t nearest[4][4];
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
    // Whether the source format can't be sampled with a filtering sampler, so that the shader
    // filters.
    bool srcUnfilterable;
    angle::FormatID dstActualFormatID;
    uint32_t srcSamples;
    bool srcIntendedFormatHasAlphaBits;
    bool dstIntentedFormatHasAlphaBits;
    GLenum filter;

    bool operator<(const BlitKey &other) const
    {
        return std::tie(op, srcComponentType, srcUnfilterable, dstActualFormatID, srcSamples,
                        srcIntendedFormatHasAlphaBits, dstIntentedFormatHasAlphaBits,
                        filter) < std::tie(other.op, other.srcComponentType, other.srcUnfilterable,
                                           other.dstActualFormatID, other.srcSamples,
                                           other.srcIntendedFormatHasAlphaBits,
                                           other.dstIntentedFormatHasAlphaBits, other.filter);
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

    // The geometry of a blit, in texels of the source and destination subresources, with row 0
    // first.
    struct BlitParams
    {
        // The destination rectangle. It is not reversed.
        gl::Rectangle dstArea;
        // The part of dstArea that the blit writes, the scissor.
        gl::Rectangle dstScissor;
        // The source rectangle that maps to dstArea. Its width or height is negative if the blit
        // flips.
        gl::RectangleImpl<float> srcArea;
        // The size of the destination subresource.
        WGPUExtent3D dstSize;
        GLenum filter;

        // The GL_NEAREST source texel of the destination texel dstOrigin + m, in one axis, is
        // srcBase + srcSign * (q0 + m * a + (r0 + m * b) / denominator), computed in integers, with
        // 0 <= r0, b < denominator.
        struct NearestAxis
        {
            int32_t q0          = 0;
            int32_t a           = 0;
            int32_t r0          = 0;
            int32_t b           = 0;
            int32_t denominator = 1;
            int32_t srcBase     = 0;
            int32_t srcSign     = 1;
            int32_t dstOrigin   = 0;
        };
        NearestAxis nearestX;
        NearestAxis nearestY;
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

    // Blits a color image by sampling it in a draw. The draw converts between the formats.
    angle::Result blitColor(ContextWgpu *context,
                            const BlitParams &params,
                            const webgpu::TextureViewHandle &src,
                            uint32_t srcSamples,
                            angle::FormatID srcIntendedFormatID,
                            angle::FormatID srcActualFormatID,
                            const webgpu::TextureViewHandle &dst,
                            angle::FormatID dstIntendedFormatID,
                            angle::FormatID dstActualFormatID);

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
                                  const BlitKey &key,
                                  const CachedPipeline **cachedPipelineOut);

    angle::Result getClearPipeline(ContextWgpu *context,
                                   const ClearPipelineKey &key,
                                   const CachedPipeline **cachedPipelineOut);

    // The vertices are CopyVertex, or BlitVertex with blitVertexLayout.
    angle::Result createPipeline(ContextWgpu *context,
                                 angle::FormatID dstFormatID,
                                 const webgpu::ShaderModuleHandle &shader,
                                 webgpu::BindGroupLayoutHandle bindGroupLayout,
                                 bool blitVertexLayout,
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

    // Starts a render pass with the given attachments and sets the vertex buffer, viewport and
    // scissor for a blit draw.
    angle::Result startBlitRenderPass(ContextWgpu *context,
                                      const BlitParams &params,
                                      const webgpu::PackedRenderPassDescriptor &renderPassDesc);

    CopyPipelineCache mCopyPipelineCache;
    BlitPipelineCache mBlitPipelineCache;
    ClearPipelineCache mClearPipelineCache;
};

}  // namespace webgpu
}  // namespace rx

#endif  // LIBANGLE_RENDERER_WGPU_UTILSWGPU_H_
