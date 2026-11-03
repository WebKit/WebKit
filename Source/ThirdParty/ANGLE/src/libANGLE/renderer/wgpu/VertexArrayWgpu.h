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
    LineLoopStreaming,
    TriangleFanStreaming,
    NonEmulatedStreaming,
    None,
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

    // Assigns the enabled attributes that the program uses to WebGPU vertex buffer slots, and sets
    // them in the context's render pipeline description. Attributes that read the same buffer with
    // the same stride and divisor share a slot when their offsets fit in one element of the slot.
    // The other attributes, and the attributes of the buffers that do not fit in the device's
    // vertex buffer slots, are streamed into one interleaved slot for each divisor.
    angle::Result updateVertexBufferSlots(ContextWgpu *contextWgpu,
                                          const gl::AttributesMask &activeAttributes);

    const VertexBufferWithOffset &getVertexBuffer(size_t slot) const
    {
        return mCurrentArrayBuffers[slot];
    }
    gl::AttributesMask getUsedVertexBufferSlots() const { return mUsedVertexBufferSlots; }
    webgpu::BufferHelper *getIndexBuffer() const { return mCurrentIndexBuffer; }

    angle::Result syncClientArrays(const gl::Context *context,
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
    struct BufferCopy
    {
        uint64_t sourceOffset;
        webgpu::BufferHelper *src;
        webgpu::BufferHelper *dest;
        uint64_t destOffset;
        uint64_t size;
    };

    // The attributes of a vertex buffer slot whose data is streamed by syncClientArrays().
    struct StreamingVertexBufferSlot
    {
        gl::AttributesMask attributes;
        GLuint divisor = 0;
        size_t stride  = 0;
    };

    void syncDirtyAttrib(ContextWgpu *contextWgpu,
                         const gl::VertexAttribute &attrib,
                         const gl::VertexBinding &binding,
                         size_t attribIndex);
    angle::Result syncDirtyElementArrayBuffer(ContextWgpu *contextWgpu);

    angle::Result ensureBufferCreated(const gl::Context *context,
                                      webgpu::BufferHelper &buffer,
                                      size_t size,
                                      size_t vertexBufferSlot,
                                      WGPUBufferUsage usage,
                                      BufferType bufferType);

    IndexDataNeedsStreaming determineIndexDataNeedsStreaming(
        gl::DrawElementsType sourceDrawElementsTypeOrInvalid,
        GLsizei count,
        gl::PrimitiveMode mode,
        gl::DrawElementsType *destDrawElementsTypeOrInvalidOut);

    // Returns the number of indices that the draw draws, which for emulated primitives can be
    // larger than 32-bit indices can index.
    uint64_t calculateAdjustedIndexCount(gl::PrimitiveMode mode,
                                         bool primitiveRestartEnabled,
                                         gl::DrawElementsType sourceDrawElementsTypeOrInvalid,
                                         GLsizei count,
                                         const uint8_t *srcIndexData);

    angle::Result calculateStagingBufferSize(ContextWgpu *contextWgpu,
                                             bool copyLineLoopIndicesOnGpu,
                                             bool primitiveRestartEnabled,
                                             IndexDataNeedsStreaming indexDataNeedsStreaming,
                                             gl::DrawElementsType destDrawElementsTypeOrInvalid,
                                             size_t adjustedCountForStreamedIndices,
                                             GLsizei instanceCount,
                                             std::optional<gl::IndexRange> indexRange,
                                             size_t *stagingBufferSizeOut);

    gl::DrawElementsType calculateDrawElementsType(
        gl::DrawElementsType sourceDrawElementsTypeOrInvalid,
        GLsizei count);

    angle::Result streamIndicesLineLoop(ContextWgpu *contextWgpu,
                                        gl::DrawElementsType sourceDrawElementsTypeOrInvalid,
                                        gl::DrawElementsType destDrawElementsTypeOrInvalid,
                                        GLsizei count,
                                        GLsizei adjustedCount,
                                        const void *indices,
                                        bool primitiveRestartEnabled,
                                        bool copyLineLoopIndicesOnGpu,
                                        uint8_t *stagingData,
                                        size_t destIndexBufferSize,
                                        std::optional<gl::IndexRange> indexRange,
                                        const uint8_t *srcIndexData,
                                        webgpu::BufferHelper *stagingBufferPtr,
                                        std::vector<BufferCopy> *stagingUploadsOut,
                                        size_t *currentStagingDataPositionOut);

    // Writes the triangle list indices that emulate a triangle fan. For glDrawArrays, the indices
    // are relative to the first vertex, which the draw passes as the base vertex.
    void streamIndicesTriangleFan(gl::DrawElementsType sourceDrawElementsTypeOrInvalid,
                                  gl::DrawElementsType destDrawElementsTypeOrInvalid,
                                  GLsizei count,
                                  uint32_t adjustedCount,
                                  uint8_t *stagingData,
                                  size_t destIndexBufferSize,
                                  const uint8_t *srcIndexData,
                                  webgpu::BufferHelper *stagingBufferPtr,
                                  std::vector<BufferCopy> *stagingUploadsOut,
                                  size_t *currentStagingDataPositionOut);

    angle::Result streamIndicesDefault(ContextWgpu *contextWgpu,
                                       gl::DrawElementsType sourceDrawElementsTypeOrInvalid,
                                       gl::DrawElementsType destDrawElementsTypeOrInvalid,
                                       gl::PrimitiveMode mode,
                                       GLsizei count,
                                       const void *indices,
                                       uint8_t *stagingData,
                                       size_t destIndexBufferSize,
                                       webgpu::BufferHelper *stagingBufferPtr,
                                       std::vector<BufferCopy> *stagingUploadsOut,
                                       size_t *currentStagingDataPositionOut);

    // Indexed by attribute index.
    gl::AttribArray<webgpu::PackedVertexAttribute> mCurrentAttribs;

    // The element that the attributes whose offset is past the end of their buffer read: zeros, as
    // large as the largest vertex format.
    static constexpr size_t kZeroBufferSize = 16;
    webgpu::BufferHelper mZeroBuffer;

    // Indexed by vertex buffer slot.
    gl::AttribArray<webgpu::BufferHelper> mStreamingArrayBuffers;
    gl::AttribArray<VertexBufferWithOffset> mCurrentArrayBuffers;
    gl::AttribArray<StreamingVertexBufferSlot> mStreamingSlots;
    gl::AttributesMask mUsedVertexBufferSlots;
    gl::AttributesMask mStreamingVertexBufferSlots;

    // Attributes that need to be streamed due to incompatibilities
    gl::AttributesMask mForcedStreamingAttributes;

    webgpu::BufferHelper mStreamingIndexBuffer;
    webgpu::BufferHelper *mCurrentIndexBuffer = nullptr;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_WGPU_VERTEXARRAYWGPU_H_
