//
// Copyright 2023 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// mtl_pipeline_cache.h:
//    Defines classes for caching of mtl pipelines
//

#ifndef LIBANGLE_RENDERER_METAL_MTL_PIPELINE_CACHE_H_
#define LIBANGLE_RENDERER_METAL_MTL_PIPELINE_CACHE_H_

#include "common/hash_utils.h"
#include "libANGLE/SizedMRUCache.h"
#include "libANGLE/renderer/metal/mtl_utils.h"

namespace rx
{
namespace mtl
{

struct RenderPipelineKey
{
    AutoObjCPtr<id<MTLFunction>> vertexShader;
    AutoObjCPtr<id<MTLFunction>> fragmentShader;
    RenderPipelineDesc pipelineDesc;

    bool operator==(const RenderPipelineKey &rhs) const;
    size_t hash() const;
};

}  // namespace mtl
}  // namespace rx

namespace std
{

template <>
struct hash<rx::mtl::RenderPipelineKey>
{
    size_t operator()(const rx::mtl::RenderPipelineKey &key) const { return key.hash(); }
};

}  // namespace std

namespace rx
{
namespace mtl
{

class PipelineCache : angle::NonCopyable
{
  public:
    PipelineCache();

    angle::Result getRenderPipeline(ContextMtl *context,
                                    id<MTLFunction> vertexShader,
                                    id<MTLFunction> fragmentShader,
                                    const RenderPipelineDesc &desc,
                                    AutoObjCPtr<id<MTLRenderPipelineState>> *outRenderPipeline);

  private:
    static constexpr unsigned int kMaxPipelines = 128;

    // The cache tries to clean up this many states at once.
    static constexpr unsigned int kGCLimit = 32;

    using RenderPipelineMap =
        angle::base::HashingMRUCache<RenderPipelineKey, AutoObjCPtr<id<MTLRenderPipelineState>>>;
    RenderPipelineMap mRenderPiplineCache;
};

}  // namespace mtl
}  // namespace rx

#endif  // LIBANGLE_RENDERER_METAL_MTL_PIPELINE_CACHE_H_
