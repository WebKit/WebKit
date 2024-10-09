//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VertexArrayWgpu.cpp:
//    Implements the class methods for VertexArrayWgpu.
//

#include "libANGLE/renderer/wgpu/VertexArrayWgpu.h"

#include "common/debug.h"
#include "libANGLE/renderer/wgpu/ContextWgpu.h"

namespace rx
{

VertexArrayWgpu::VertexArrayWgpu(const gl::VertexArrayState &data) : VertexArrayImpl(data) {}

angle::Result VertexArrayWgpu::syncState(const gl::Context *context,
                                         const gl::VertexArray::DirtyBits &dirtyBits,
                                         gl::VertexArray::DirtyAttribBitsArray *attribBits,
                                         gl::VertexArray::DirtyBindingBitsArray *bindingBits)
{
    ASSERT(dirtyBits.any());

    ContextWgpu *contextWgpu = GetImplAs<ContextWgpu>(context);

    const std::vector<gl::VertexAttribute> &attribs = mState.getVertexAttributes();
    const std::vector<gl::VertexBinding> &bindings  = mState.getVertexBindings();

    gl::AttributesMask syncedAttributes;

    for (auto iter = dirtyBits.begin(), endIter = dirtyBits.end(); iter != endIter; ++iter)
    {
        size_t dirtyBit = *iter;
        switch (dirtyBit)
        {
            case gl::VertexArray::DIRTY_BIT_LOST_OBSERVATION:
                break;

            case gl::VertexArray::DIRTY_BIT_ELEMENT_ARRAY_BUFFER:
                ANGLE_TRY(syncDirtyElementArrayBuffer(contextWgpu));
                contextWgpu->invalidateIndexBuffer();
                break;

            case gl::VertexArray::DIRTY_BIT_ELEMENT_ARRAY_BUFFER_DATA:
                break;

#define ANGLE_VERTEX_DIRTY_ATTRIB_FUNC(INDEX)                                     \
    case gl::VertexArray::DIRTY_BIT_ATTRIB_0 + INDEX:                             \
        ANGLE_TRY(syncDirtyAttrib(contextWgpu, attribs[INDEX],                    \
                                  bindings[attribs[INDEX].bindingIndex], INDEX)); \
        (*attribBits)[INDEX].reset();                                             \
        syncedAttributes.set(INDEX);                                              \
        break;

                ANGLE_VERTEX_INDEX_CASES(ANGLE_VERTEX_DIRTY_ATTRIB_FUNC)

#define ANGLE_VERTEX_DIRTY_BINDING_FUNC(INDEX)                                    \
    case gl::VertexArray::DIRTY_BIT_BINDING_0 + INDEX:                            \
        ANGLE_TRY(syncDirtyAttrib(contextWgpu, attribs[INDEX],                    \
                                  bindings[attribs[INDEX].bindingIndex], INDEX)); \
        (*bindingBits)[INDEX].reset();                                            \
        syncedAttributes.set(INDEX);                                              \
        break;

                ANGLE_VERTEX_INDEX_CASES(ANGLE_VERTEX_DIRTY_BINDING_FUNC)

#define ANGLE_VERTEX_DIRTY_BUFFER_DATA_FUNC(INDEX)                                \
    case gl::VertexArray::DIRTY_BIT_BUFFER_DATA_0 + INDEX:                        \
        ANGLE_TRY(syncDirtyAttrib(contextWgpu, attribs[INDEX],                    \
                                  bindings[attribs[INDEX].bindingIndex], INDEX)); \
        syncedAttributes.set(INDEX);                                              \
        break;

                ANGLE_VERTEX_INDEX_CASES(ANGLE_VERTEX_DIRTY_BUFFER_DATA_FUNC)
            default:
                break;
        }
    }

    for (size_t syncedAttribIndex : syncedAttributes)
    {
        contextWgpu->setVertexAttribute(syncedAttribIndex, mCurrentAttribs[syncedAttribIndex]);
        contextWgpu->invalidateVertexBuffer(syncedAttribIndex);
    }
    return angle::Result::Continue;
}

angle::Result VertexArrayWgpu::syncClientArrays(const gl::Context *context,
                                                const gl::AttributesMask &activeAttributesMask,
                                                GLint first,
                                                GLsizei count,
                                                gl::DrawElementsType drawElementsTypeOrInvalid,
                                                const void *indices,
                                                GLsizei instanceCount,
                                                bool primitiveRestartEnabled,
                                                const void **adjustedIndicesPtr)
{
    gl::AttributesMask clientAttributesToSync =
        mState.getClientMemoryAttribsMask() & activeAttributesMask;
    if (clientAttributesToSync.any())
    {
        UNIMPLEMENTED();
    }

    if (drawElementsTypeOrInvalid != gl::DrawElementsType::InvalidEnum &&
        !mState.getElementArrayBuffer())
    {
        UNIMPLEMENTED();
    }

    *adjustedIndicesPtr = indices;
    return angle::Result::Continue;
}

angle::Result VertexArrayWgpu::syncDirtyAttrib(ContextWgpu *contextWgpu,
                                               const gl::VertexAttribute &attrib,
                                               const gl::VertexBinding &binding,
                                               size_t attribIndex)
{
    if (attrib.enabled)
    {
        SetBitField(mCurrentAttribs[attribIndex].enabled, true);
        const webgpu::Format &webgpuFormat =
            contextWgpu->getFormat(attrib.format->glInternalFormat);
        SetBitField(mCurrentAttribs[attribIndex].format, webgpuFormat.getActualWgpuVertexFormat());
        SetBitField(mCurrentAttribs[attribIndex].shaderLocation, attribIndex);
        SetBitField(mCurrentAttribs[attribIndex].stride, binding.getStride());

        gl::Buffer *bufferGl = binding.getBuffer().get();
        if (bufferGl && bufferGl->getSize() > 0)
        {
            SetBitField(mCurrentAttribs[attribIndex].offset,
                        reinterpret_cast<uintptr_t>(attrib.pointer));
            BufferWgpu *bufferWgpu            = webgpu::GetImpl(bufferGl);
            mCurrentArrayBuffers[attribIndex] = &(bufferWgpu->getBuffer());
        }
        else
        {
            SetBitField(mCurrentAttribs[attribIndex].offset, binding.getOffset());
            mCurrentArrayBuffers[attribIndex] = nullptr;
        }
    }
    else
    {
        memset(&mCurrentAttribs[attribIndex], 0, sizeof(webgpu::PackedVertexAttribute));
        mCurrentArrayBuffers[attribIndex] = nullptr;
    }

    return angle::Result::Continue;
}

angle::Result VertexArrayWgpu::syncDirtyElementArrayBuffer(ContextWgpu *contextWgpu)
{
    gl::Buffer *bufferGl = mState.getElementArrayBuffer();
    if (bufferGl)
    {
        BufferWgpu *buffer  = webgpu::GetImpl(bufferGl);
        mCurrentIndexBuffer = &buffer->getBuffer();
    }
    else
    {
        mCurrentIndexBuffer = nullptr;
    }

    return angle::Result::Continue;
}

}  // namespace rx
