//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProvokingVertexHelper.h:
//    Manages re-writing index buffers when provoking vertex support is needed.
//    Handles points, lines, triangles, line strips, and triangle strips.
//    Line loops, and triangle fans should be pre-processed.
//
#ifndef PrimitiveRestartHelper_h
#define PrimitiveRestartHelper_h

#include "libANGLE/Context.h"
#include "libANGLE/renderer/ContextImpl.h"
#include "libANGLE/renderer/metal/DisplayMtl.h"
#include "libANGLE/renderer/metal/mtl_buffer_pool.h"
#include "libANGLE/renderer/metal/mtl_command_buffer.h"
#include "libANGLE/renderer/metal/mtl_state_cache.h"
namespace rx
{
class ContextMtl;

class ProvokingVertexHelper : public mtl::ProvokingVertexCacheSpecializeShaderFactory
{
public:
    ProvokingVertexHelper(ContextMtl * context, mtl::CommandQueue * commandQueue, DisplayMtl *display);
    mtl::BufferRef  preconditionIndexBuffer(ContextMtl * context,
                                            mtl::BufferRef indexBuffer,
                                            size_t indexCount,
                                            size_t indexOffset,
                                            bool primitiveRestartEnabled,
                                            gl::PrimitiveMode primitiveMode,
                                            gl::DrawElementsType elementsType,
                                            size_t & outIndexcount,
                                            gl::PrimitiveMode & outPrimitiveMode);
    void commitPreconditionCommandBuffer(ContextMtl * context);
    void ensureCommandBufferReady();
    void onDestroy(ContextMtl * context);
private:
    id<MTLLibrary> mProvokingVertexLibrary;
    mtl::CommandBuffer mCommandBuffer;
    mtl::BufferPool    mIndexBuffers;
    mtl::ProvokingVertexComputePipelineCache mPipelineCache;
    mtl::ProvokingVertexComputePipelineDesc mCachedDesc;
    mtl::ComputeCommandEncoder mCurrentEncoder;

    mtl::ComputeCommandEncoder * getComputeCommandEncoder();
    //Program cache
    virtual angle::Result getSpecializedShader(rx::mtl::Context *context,
                                               gl::ShaderType shaderType,
                                               const mtl::ProvokingVertexComputePipelineDesc &renderPipelineDesc,
                                               id<MTLFunction> *shaderOut) override;
    //Private command buffer
    virtual bool hasSpecializedShader(gl::ShaderType shaderType,
                                      const mtl::ProvokingVertexComputePipelineDesc &renderPipelineDesc) override;
    
    void prepareCommandEncoderForDescriptor(ContextMtl * context, mtl::ComputeCommandEncoder * encoder, mtl::ProvokingVertexComputePipelineDesc desc);
};
} // namespace rx
#endif /* PrimitiveRestartHelper_h */
