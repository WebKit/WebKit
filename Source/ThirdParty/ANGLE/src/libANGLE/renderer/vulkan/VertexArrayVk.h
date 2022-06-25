//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VertexArrayVk.h:
//    Defines the class interface for VertexArrayVk, implementing VertexArrayImpl.
//

#ifndef LIBANGLE_RENDERER_VULKAN_VERTEXARRAYVK_H_
#define LIBANGLE_RENDERER_VULKAN_VERTEXARRAYVK_H_

#include "libANGLE/renderer/VertexArrayImpl.h"
#include "libANGLE/renderer/vulkan/vk_cache_utils.h"
#include "libANGLE/renderer/vulkan/vk_helpers.h"

namespace rx
{
class BufferVk;
struct ConversionBuffer;

enum class BufferBindingDirty
{
    No,
    Yes,
};

class VertexArrayVk : public VertexArrayImpl
{
  public:
    VertexArrayVk(ContextVk *contextVk, const gl::VertexArrayState &state);
    ~VertexArrayVk() override;

    void destroy(const gl::Context *context) override;

    angle::Result syncState(const gl::Context *context,
                            const gl::VertexArray::DirtyBits &dirtyBits,
                            gl::VertexArray::DirtyAttribBitsArray *attribBits,
                            gl::VertexArray::DirtyBindingBitsArray *bindingBits) override;

    angle::Result updateActiveAttribInfo(ContextVk *contextVk);

    angle::Result updateDefaultAttrib(ContextVk *contextVk, size_t attribIndex);

    angle::Result updateStreamedAttribs(const gl::Context *context,
                                        GLint firstVertex,
                                        GLsizei vertexOrIndexCount,
                                        GLsizei instanceCount,
                                        gl::DrawElementsType indexTypeOrInvalid,
                                        const void *indices);

    angle::Result handleLineLoop(ContextVk *contextVk,
                                 GLint firstVertex,
                                 GLsizei vertexOrIndexCount,
                                 gl::DrawElementsType indexTypeOrInvalid,
                                 const void *indices,
                                 uint32_t *indexCountOut);

    angle::Result handleLineLoopIndexIndirect(ContextVk *contextVk,
                                              gl::DrawElementsType glIndexType,
                                              vk::BufferHelper *srcIndirectBuf,
                                              VkDeviceSize indirectBufferOffset,
                                              vk::BufferHelper **indirectBufferOut);

    angle::Result handleLineLoopIndirectDraw(const gl::Context *context,
                                             vk::BufferHelper *indirectBufferVk,
                                             VkDeviceSize indirectBufferOffset,
                                             vk::BufferHelper **indirectBufferOut);

    const gl::AttribArray<VkBuffer> &getCurrentArrayBufferHandles() const
    {
        return mCurrentArrayBufferHandles;
    }

    const gl::AttribArray<VkDeviceSize> &getCurrentArrayBufferOffsets() const
    {
        return mCurrentArrayBufferOffsets;
    }

    const gl::AttribArray<vk::BufferHelper *> &getCurrentArrayBuffers() const
    {
        return mCurrentArrayBuffers;
    }

    const gl::AttribArray<angle::FormatID> &getCurrentArrayBufferFormats() const
    {
        return mCurrentArrayBufferFormats;
    }

    const gl::AttribArray<GLuint> &getCurrentArrayBufferStrides() const
    {
        return mCurrentArrayBufferStrides;
    }

    // Update mCurrentElementArrayBuffer based on the vertex array state
    void updateCurrentElementArrayBuffer();

    vk::BufferHelper *getCurrentElementArrayBuffer() const { return mCurrentElementArrayBuffer; }

    angle::Result convertIndexBufferGPU(ContextVk *contextVk,
                                        BufferVk *bufferVk,
                                        const void *indices);

    angle::Result convertIndexBufferIndirectGPU(ContextVk *contextVk,
                                                vk::BufferHelper *srcIndirectBuf,
                                                VkDeviceSize srcIndirectBufOffset,
                                                vk::BufferHelper **indirectBufferVkOut);

    angle::Result convertIndexBufferCPU(ContextVk *contextVk,
                                        gl::DrawElementsType indexType,
                                        size_t indexCount,
                                        const void *sourcePointer,
                                        BufferBindingDirty *bufferBindingDirty);

    const gl::AttributesMask &getStreamingVertexAttribsMask() const
    {
        return mStreamingVertexAttribsMask;
    }

  private:
    angle::Result setDefaultPackedInput(ContextVk *contextVk,
                                        size_t attribIndex,
                                        angle::FormatID *formatOut);

    angle::Result convertVertexBufferGPU(ContextVk *contextVk,
                                         BufferVk *srcBuffer,
                                         const gl::VertexBinding &binding,
                                         size_t attribIndex,
                                         const vk::Format &vertexFormat,
                                         ConversionBuffer *conversion,
                                         GLuint relativeOffset,
                                         bool compressed);
    angle::Result convertVertexBufferCPU(ContextVk *contextVk,
                                         BufferVk *srcBuffer,
                                         const gl::VertexBinding &binding,
                                         size_t attribIndex,
                                         const vk::Format &vertexFormat,
                                         ConversionBuffer *conversion,
                                         GLuint relativeOffset,
                                         bool compress);

    angle::Result syncDirtyAttrib(ContextVk *contextVk,
                                  const gl::VertexAttribute &attrib,
                                  const gl::VertexBinding &binding,
                                  size_t attribIndex,
                                  bool bufferOnly);

    gl::AttribArray<VkBuffer> mCurrentArrayBufferHandles;
    gl::AttribArray<VkDeviceSize> mCurrentArrayBufferOffsets;
    // The offset into the buffer to the first attrib
    gl::AttribArray<GLuint> mCurrentArrayBufferRelativeOffsets;
    gl::AttribArray<vk::BufferHelper *> mCurrentArrayBuffers;
    // Cache strides of attributes for a fast pipeline cache update when VAOs are changed
    gl::AttribArray<angle::FormatID> mCurrentArrayBufferFormats;
    gl::AttribArray<GLuint> mCurrentArrayBufferStrides;
    gl::AttributesMask mCurrentArrayBufferCompressed;
    vk::BufferHelper *mCurrentElementArrayBuffer;

    // Cached element array buffers for improving performance.
    vk::BufferHelperPointerVector mCachedStreamIndexBuffers;

    vk::BufferHelper mStreamedIndexData;
    vk::BufferHelper mTranslatedByteIndexData;
    vk::BufferHelper mTranslatedByteIndirectData;

    vk::LineLoopHelper mLineLoopHelper;
    Optional<GLint> mLineLoopBufferFirstIndex;
    Optional<size_t> mLineLoopBufferLastIndex;
    bool mDirtyLineLoopTranslation;

    // Track client and/or emulated attribs that we have to stream their buffer contents
    gl::AttributesMask mStreamingVertexAttribsMask;
};
}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_VERTEXARRAYVK_H_
