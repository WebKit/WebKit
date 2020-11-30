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
#include "libANGLE/renderer/metal/mtl_format_utils.h"
#include "libANGLE/renderer/metal/mtl_resources.h"

namespace rx
{
class ContextMtl;

class VertexArrayMtl : public VertexArrayImpl
{
  public:
    VertexArrayMtl(const gl::VertexArrayState &state, ContextMtl *context);
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
                                      gl::DrawElementsType indexTypeOrInvalid,
                                      const void *indices);

    // vertexDescChanged is both input and output, the input value if is true, will force new
    // mtl::VertexDesc to be returned via vertexDescOut. Otherwise, it is only returned when the
    // vertex array is dirty
    angle::Result setupDraw(const gl::Context *glContext,
                            mtl::RenderCommandEncoder *cmdEncoder,
                            bool *vertexDescChanged,
                            mtl::VertexDesc *vertexDescOut);

    angle::Result getIndexBuffer(const gl::Context *glContext,
                                 gl::DrawElementsType indexType,
                                 size_t indexCount,
                                 const void *sourcePointer,
                                 mtl::BufferRef *idxBufferOut,
                                 size_t *idxBufferOffsetOut,
                                 gl::DrawElementsType *indexTypeOut);

  private:
    void reset(ContextMtl *context);

    angle::Result syncDirtyAttrib(const gl::Context *glContext,
                                  const gl::VertexAttribute &attrib,
                                  const gl::VertexBinding &binding,
                                  size_t attribIndex);

    angle::Result convertIndexBuffer(const gl::Context *glContext,
                                     gl::DrawElementsType indexType,
                                     size_t offset,
                                     mtl::BufferRef *idxBufferOut,
                                     size_t *idxBufferOffsetOut);
    angle::Result streamIndexBufferFromClient(const gl::Context *glContext,
                                              gl::DrawElementsType indexType,
                                              size_t indexCount,
                                              const void *sourcePointer,
                                              mtl::BufferRef *idxBufferOut,
                                              size_t *idxBufferOffsetOut);

    angle::Result convertIndexBufferGPU(const gl::Context *glContext,
                                        gl::DrawElementsType indexType,
                                        BufferMtl *idxBuffer,
                                        size_t offset,
                                        size_t indexCount,
                                        IndexConversionBufferMtl *conversion);

    angle::Result convertVertexBuffer(const gl::Context *glContext,
                                      BufferMtl *srcBuffer,
                                      const gl::VertexBinding &binding,
                                      size_t attribIndex,
                                      const mtl::VertexFormat &vertexFormat);

    angle::Result convertVertexBufferCPU(const gl::Context *glContext,
                                         BufferMtl *srcBuffer,
                                         const gl::VertexBinding &binding,
                                         size_t attribIndex,
                                         const mtl::VertexFormat &vertexFormat,
                                         ConversionBufferMtl *conversion);

    // These can point to real BufferMtl or converted buffer in mConvertedArrayBufferHolders
    gl::AttribArray<BufferHolderMtl *> mCurrentArrayBuffers;
    gl::AttribArray<SimpleWeakBufferHolderMtl> mConvertedArrayBufferHolders;
    gl::AttribArray<size_t> mCurrentArrayBufferOffsets;
    gl::AttribArray<GLuint> mCurrentArrayBufferStrides;
    gl::AttribArray<const mtl::VertexFormat *> mCurrentArrayBufferFormats;

    mtl::BufferPool mDynamicVertexData;
    mtl::BufferPool mDynamicIndexData;

    bool mVertexArrayDirty = true;
};
}  // namespace rx

#endif /* LIBANGLE_RENDERER_METAL_VERTEXARRAYMTL_H_ */
