//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VertexArrayMtl.h:
//    Defines the class interface for VertexArrayMtl, implementing VertexArrayImpl.
//

#ifndef LIBANGLE_RENDERER_METAL_VERTEXARRAYMTL_H_
#define LIBANGLE_RENDERER_METAL_VERTEXARRAYMTL_H_

#include "libANGLE/renderer/VertexArrayImpl.h"
#include "libANGLE/renderer/metal/BufferMtl.h"
#include "libANGLE/renderer/metal/mtl_buffer_pool.h"
#include "libANGLE/renderer/metal/mtl_command_buffer.h"
#include "libANGLE/renderer/metal/mtl_context_device.h"
#include "libANGLE/renderer/metal/mtl_format_utils.h"
#include "libANGLE/renderer/metal/mtl_resources.h"

namespace rx
{
class ContextMtl;

class VertexArrayMtl : public VertexArrayImpl
{
  public:
    VertexArrayMtl(const gl::VertexArrayState &state,
                   const gl::VertexArrayBuffers &vertexArrayBuffers,
                   ContextMtl *context);
    ~VertexArrayMtl() override;

    void destroy(const gl::Context *context) override;

    angle::Result syncState(const gl::Context *context,
                            const gl::VertexArray::DirtyBits &dirtyBits,
                            gl::VertexArray::DirtyAttribBitsArray *attribBits,
                            gl::VertexArray::DirtyBindingBitsArray *bindingBits) override;

    // Feed client side's vertex/index data
    angle::Result updateClientAttribs(const gl::Context *context,
                                      GLint firstVertex,
                                      GLsizei vertexOrIndexCount,
                                      GLsizei instanceCount,
                                      GLuint baseInstance,
                                      gl::DrawElementsType indexTypeOrInvalid,
                                      const void *indices);

    // vertexDescChanged is both input and output, the input value if is true, will force new
    // mtl::VertexDesc to be returned via vertexDescOut. This typically happens when active shader
    // program is changed.
    // Otherwise, it is only returned when the vertex array is dirty.
    angle::Result setupDraw(const gl::Context *glContext,
                            mtl::RenderCommandEncoder *cmdEncoder,
                            bool *vertexDescChanged,
                            mtl::VertexDesc *vertexDescOut);

    angle::Result resolveDrawElementsDraw(const gl::Context *glContext,
                                          gl::PrimitiveMode mode,
                                          gl::DrawElementsType type,
                                          GLsizei count,
                                          const void *indices,
                                          bool rewriteProvokingVertex,
                                          bool isPrimitiveRestartEnabled,
                                          gl::PrimitiveMode *outNewMode,
                                          std::vector<DrawCommandRange> *outDrawCommands,
                                          mtl::BufferSlice *outIndexBuffer,
                                          gl::DrawElementsType *outIndexBufferType);

  private:
    void reset(ContextMtl *context);

    angle::Result syncDirtyAttrib(const gl::Context *glContext,
                                  const gl::VertexAttribute &attrib,
                                  const gl::VertexBinding &binding,
                                  size_t attribIndex);

    angle::Result convertIndexBuffer(const gl::Context *glContext,
                                     gl::DrawElementsType indexType,
                                     size_t offset,
                                     mtl::BufferSlice *outIdxBuffer);
    angle::Result streamIndexBufferFromClient(const gl::Context *glContext,
                                              gl::DrawElementsType indexType,
                                              size_t indexCount,
                                              const void *sourcePointer,
                                              mtl::BufferSlice *outIdxBuffer);

    angle::Result convertVertexBuffer(const gl::Context *glContext,
                                      BufferMtl *srcBuffer,
                                      const gl::VertexBinding &binding,
                                      size_t attribIndex,
                                      const mtl::VertexFormat &vertexFormat);

    angle::Result convertVertexBufferCPU(ContextMtl *contextMtl,
                                         BufferMtl *srcBuffer,
                                         const gl::VertexBinding &binding,
                                         size_t attribIndex,
                                         const mtl::VertexFormat &convertedFormat,
                                         GLuint targetStride,
                                         size_t vertexCount,
                                         ConversionBufferMtl *conversion);
    angle::Result convertVertexBufferGPU(const gl::Context *glContext,
                                         BufferMtl *srcBuffer,
                                         const gl::VertexBinding &binding,
                                         size_t attribIndex,
                                         const mtl::VertexFormat &convertedFormat,
                                         GLuint targetStride,
                                         size_t vertexCount,
                                         bool isExpandingComponents,
                                         ConversionBufferMtl *conversion);

    // These can point to real BufferMtl or converted buffer in mConvertedArrayBufferHolders
    gl::AttribArray<BufferHolderMtl *> mCurrentArrayBuffers;
    gl::AttribArray<SimpleWeakBufferHolderMtl> mConvertedArrayBufferHolders;
    gl::AttribArray<size_t> mCurrentArrayBufferOffsets;

    // Size to be uploaded as inline constant data. Used for client vertex attribute's data that
    // is small enough that we can send directly as inline constant data instead of streaming
    // through a buffer.
    gl::AttribArray<size_t> mCurrentArrayInlineDataSizes;
    // Array of host buffers storing converted data for client attributes that are small enough.
    gl::AttribArray<angle::MemoryBuffer> mConvertedClientSmallArrays;
    gl::AttribArray<const uint8_t *> mCurrentArrayInlineDataPointers;
    // Max size of inline constant data that can be used for client vertex attribute.
    size_t mInlineDataMaxSize;

    // Stride per vertex attribute
    gl::AttribArray<GLuint> mCurrentArrayBufferStrides;
    // Format per vertex attribute
    gl::AttribArray<const mtl::VertexFormat *> mCurrentArrayBufferFormats;

    const mtl::VertexFormat &mDefaultFloatVertexFormat;

    mtl::BufferPool mDynamicVertexData;
    mtl::BufferPool mDynamicIndexData;

    std::vector<uint32_t> mEmulatedInstanceAttribs;

    bool mVertexArrayDirty = true;
    bool mVertexDataDirty  = true;
};
// Free function for computing draw command ranges from draw index ranges.
// Exposed for unit testing.
void AppendSimpleDrawCommandRanges(std::vector<DrawCommandRange> &drawCommands,
                                   gl::PrimitiveMode mode,
                                   uint32_t count,
                                   size_t firstIndex,
                                   const std::vector<DrawIndexRange> &drawIndexRanges,
                                   gl::PrimitiveMode drawMode,
                                   gl::DrawElementsType indexBufferType);

}  // namespace rx

#endif /* LIBANGLE_RENDERER_METAL_VERTEXARRAYMTL_H_ */
