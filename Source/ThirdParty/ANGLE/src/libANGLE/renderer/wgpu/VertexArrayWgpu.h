//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VertexArrayWgpu.h:
//    Defines the class interface for VertexArrayWgpu, implementing VertexArrayImpl.
//

#ifndef LIBANGLE_RENDERER_WGPU_VERTEXARRAYWGPU_H_
#define LIBANGLE_RENDERER_WGPU_VERTEXARRAYWGPU_H_

#include "libANGLE/renderer/VertexArrayImpl.h"
#include "libANGLE/renderer/wgpu/BufferWgpu.h"
#include "libANGLE/renderer/wgpu/wgpu_pipeline_state.h"
#include "libANGLE/renderer/wgpu/wgpu_utils.h"

namespace rx
{

enum class BufferType
{
    IndexBuffer,
    ArrayBuffer,
};

enum class IndexDataNeedsStreaming
{
    Yes,
    No,
};

struct VertexBufferWithOffset
{
    webgpu::BufferHelper *buffer = nullptr;
    size_t offset                = 0;
};

class VertexArrayWgpu : public VertexArrayImpl
{
  public:
    VertexArrayWgpu(const gl::VertexArrayState &data,
                    const gl::VertexArrayBuffers &vertexArrayBuffers);

    angle::Result syncState(const gl::Context *context,
                            const gl::VertexArray::DirtyBits &dirtyBits,
                            gl::VertexArray::DirtyAttribBitsArray *attribBits,
                            gl::VertexArray::DirtyBindingBitsArray *bindingBits) override;

    const VertexBufferWithOffset &getVertexBuffer(size_t slot) const
    {
        return mCurrentArrayBuffers[slot];
    }
    webgpu::BufferHelper *getIndexBuffer() const { return mCurrentIndexBuffer; }

    angle::Result syncClientArrays(const gl::Context *context,
                                   const gl::AttributesMask &activeAttributesMask,
                                   gl::PrimitiveMode mode,
                                   GLint first,
                                   GLsizei count,
                                   GLsizei instanceCount,
                                   gl::DrawElementsType drawElementsTypeOrInvalid,
                                   const void *indices,
                                   GLint baseVertex,
                                   bool primitiveRestartEnabled,
                                   const void **adjustedIndicesPtr,
                                   uint32_t *indexCountOut);

  private:
    angle::Result syncDirtyAttrib(ContextWgpu *contextWgpu,
                                  const gl::VertexAttribute &attrib,
                                  const gl::VertexBinding &binding,
                                  size_t attribIndex);
    angle::Result syncDirtyElementArrayBuffer(ContextWgpu *contextWgpu);

    angle::Result ensureBufferCreated(const gl::Context *context,
                                      webgpu::BufferHelper &buffer,
                                      size_t size,
                                      size_t attribIndex,
                                      WGPUBufferUsage usage,
                                      BufferType bufferType);

    IndexDataNeedsStreaming determineIndexDataNeedsStreaming(
        gl::DrawElementsType sourceDrawElementsTypeOrInvalid,
        GLsizei count,
        gl::PrimitiveMode mode,
        gl::DrawElementsType *destDrawElementsTypeOrInvalidOut);

    // Calculates new index count for draw calls that need to be emulated.
    angle::Result calculateAdjustedIndexCount(gl::PrimitiveMode mode,
                                              bool primitiveRestartEnabled,
                                              gl::DrawElementsType destDrawElementsTypeOrInvalid,
                                              GLsizei count,
                                              const uint8_t *srcIndexData,
                                              GLsizei *adjustedCountOut);

    angle::Result calculateStagingBufferSize(bool srcDestDrawElementsTypeEqual,
                                             bool primitiveRestartEnabled,
                                             ContextWgpu *contextWgpu,
                                             IndexDataNeedsStreaming indexDataNeedsStreaming,
                                             std::optional<size_t> destIndexDataSize,
                                             gl::AttributesMask clientAttributesToSync,
                                             GLsizei instanceCount,
                                             std::optional<gl::IndexRange> indexRange,
                                             size_t *stagingBufferSizeOut);

    gl::AttribArray<webgpu::PackedVertexAttribute> mCurrentAttribs;
    gl::AttribArray<webgpu::BufferHelper> mStreamingArrayBuffers;
    gl::AttribArray<VertexBufferWithOffset> mCurrentArrayBuffers;

    // Attributes that need to be streamed due to incompatibilities
    gl::AttributesMask mForcedStreamingAttributes;

    webgpu::BufferHelper mStreamingIndexBuffer;
    webgpu::BufferHelper *mCurrentIndexBuffer = nullptr;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_WGPU_VERTEXARRAYWGPU_H_
