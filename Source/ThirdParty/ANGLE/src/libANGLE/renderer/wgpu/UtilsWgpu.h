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
#include "libANGLE/renderer/Format.h"
#include "libANGLE/renderer/wgpu/wgpu_utils.h"

namespace rx
{
class ContextWgpu;
namespace webgpu
{

enum class WgpuPipelineOp : uint8_t
{
    ImageCopy,
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

struct CachedPipeline
{
    webgpu::RenderPipelineHandle pipeline;
    webgpu::BindGroupLayoutHandle bindGroupLayout;
};

class UtilsWgpu : angle::NonCopyable
{
  public:
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
                            angle::FormatID dstActualFormatID);

  private:
    webgpu::ShaderModuleHandle getCopyShaderModule(ContextWgpu *context, const CopyKey &key);
    webgpu::ShaderModuleHandle getShaderModule(ContextWgpu *context, const std::string &shader);

    angle::Result getPipeline(ContextWgpu *context,
                              const CopyKey &key,
                              const webgpu::ShaderModuleHandle &shader,
                              CachedPipeline *cachedPipelineOut);

    std::map<CopyKey, CachedPipeline> mCopyPipelineCache;
};

}  // namespace webgpu
}  // namespace rx

#endif  // LIBANGLE_RENDERER_WGPU_UTILSWGPU_H_
