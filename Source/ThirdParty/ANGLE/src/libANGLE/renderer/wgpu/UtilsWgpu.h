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

struct PipelineKey
{
    GLenum srcComponentType;
    angle::FormatID dstActualFormatID;
    bool dstIntentedFormatHasAlphaBits;
    WgpuPipelineOp op;

    bool operator<(const PipelineKey &other) const
    {
        return std::tie(srcComponentType, dstActualFormatID, dstIntentedFormatHasAlphaBits, op) <
               std::tie(other.srcComponentType, other.dstActualFormatID,
                        other.dstIntentedFormatHasAlphaBits, other.op);
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
                            const WGPUExtent3D &size,
                            bool flipY,
                            const angle::Format &srcFormat,
                            angle::FormatID dstIntendedFormatID,
                            angle::FormatID dstActualFormatID);

  private:
    webgpu::ShaderModuleHandle getShaderModule(ContextWgpu *context, const PipelineKey &key);

    angle::Result getPipeline(ContextWgpu *context,
                              const PipelineKey &key,
                              const CachedPipeline **cachedPipelineOut);

    std::map<PipelineKey, CachedPipeline> mPipelineCache;
};

}  // namespace webgpu
}  // namespace rx

#endif  // LIBANGLE_RENDERER_WGPU_UTILSWGPU_H_
