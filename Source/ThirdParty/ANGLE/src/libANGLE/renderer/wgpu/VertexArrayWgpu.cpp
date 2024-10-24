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

VertexArrayWgpu::VertexArrayWgpu(const gl::VertexArrayState &data) : VertexArrayImpl(data)
{
    // Pre-initialize mCurrentIndexBuffer to a streaming buffer because no index buffer dirty bit is
    // triggered if our first draw call has no buffer bound.
    mCurrentIndexBuffer = &mStreamingIndexBuffer;
}

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
                                                GLsizei instanceCount,
                                                gl::DrawElementsType drawElementsTypeOrInvalid,
                                                const void *indices,
                                                GLint baseVertex,
                                                bool primitiveRestartEnabled,
                                                const void **adjustedIndicesPtr)
{
    *adjustedIndicesPtr = indices;

    gl::AttributesMask clientAttributesToSync = mState.getClientMemoryAttribsMask() &
                                                mState.getEnabledAttributesMask() &
                                                activeAttributesMask;
    bool indexedDrawCallWithNoIndexBuffer =
        drawElementsTypeOrInvalid != gl::DrawElementsType::InvalidEnum &&
        !mState.getElementArrayBuffer();

    if (!clientAttributesToSync.any() && !indexedDrawCallWithNoIndexBuffer)
    {
        return angle::Result::Continue;
    }

    ContextWgpu *contextWgpu = webgpu::GetImpl(context);
    wgpu::Device device      = webgpu::GetDevice(context);

    // If any attributes need to be streamed, we need to know the index range.
    std::optional<gl::IndexRange> indexRange;
    if (clientAttributesToSync.any())
    {
        GLint startVertex  = 0;
        size_t vertexCount = 0;
        ANGLE_TRY(GetVertexRangeInfo(context, first, count, drawElementsTypeOrInvalid, indices,
                                     baseVertex, &startVertex, &vertexCount));
        indexRange = gl::IndexRange(startVertex, startVertex + vertexCount - 1, 0);
    }

    // Pre-compute the total size of all streamed vertex and index data so a single staging buffer
    // can be used
    size_t stagingBufferSize = 0;

    std::optional<size_t> indexDataSize;
    if (indexedDrawCallWithNoIndexBuffer)
    {
        indexDataSize = gl::GetDrawElementsTypeSize(drawElementsTypeOrInvalid) * count;
        stagingBufferSize +=
            rx::roundUpPow2(indexDataSize.value(), webgpu::kBufferCopyToBufferAlignment);
    }

    const std::vector<gl::VertexAttribute> &attribs = mState.getVertexAttributes();
    const std::vector<gl::VertexBinding> &bindings  = mState.getVertexBindings();

    if (clientAttributesToSync.any())
    {
        for (size_t attribIndex : clientAttributesToSync)
        {
            const gl::VertexAttribute &attrib = attribs[attribIndex];
            const gl::VertexBinding &binding  = bindings[attrib.bindingIndex];

            size_t typeSize = gl::ComputeVertexAttributeTypeSize(attrib);
            size_t attribSize =
                typeSize * gl::ComputeVertexBindingElementCount(
                               binding.getDivisor(), indexRange->vertexCount(), instanceCount);
            stagingBufferSize += rx::roundUpPow2(attribSize, webgpu::kBufferCopyToBufferAlignment);
        }
    }

    ASSERT(stagingBufferSize > 0);
    ASSERT(stagingBufferSize % webgpu::kBufferSizeAlignment == 0);
    webgpu::BufferHelper stagingBuffer;
    ANGLE_TRY(stagingBuffer.initBuffer(device, stagingBufferSize,
                                       wgpu::BufferUsage::CopySrc | wgpu::BufferUsage::MapWrite,
                                       webgpu::MapAtCreation::Yes));

    struct BufferCopy
    {
        uint64_t sourceOffset;
        webgpu::BufferHelper *dest;
        uint64_t size;
    };
    std::vector<BufferCopy> stagingUploads;

    uint8_t *stagingData              = stagingBuffer.getMapWritePointer(0, stagingBufferSize);
    size_t currentStagingDataPosition = 0;

    auto ensureStreamingBufferCreated = [device](webgpu::BufferHelper &buffer, size_t size,
                                                 wgpu::BufferUsage usage, bool *outNewBuffer) {
        if (buffer.valid() && buffer.requestedSize() >= size)
        {
            ASSERT(buffer.getBuffer().GetUsage() == usage);
            *outNewBuffer = false;
            return angle::Result::Continue;
        }

        ANGLE_TRY(buffer.initBuffer(device, size, usage, webgpu::MapAtCreation::No));
        *outNewBuffer = true;
        return angle::Result::Continue;
    };

    if (indexedDrawCallWithNoIndexBuffer)
    {
        ASSERT(indexDataSize.has_value());
        memcpy(stagingData + currentStagingDataPosition, indices, indexDataSize.value());

        size_t copySize =
            rx::roundUpPow2(indexDataSize.value(), webgpu::kBufferCopyToBufferAlignment);

        bool newBuffer = false;
        ANGLE_TRY(ensureStreamingBufferCreated(
            mStreamingIndexBuffer, copySize, wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Index,
            &newBuffer));
        if (newBuffer)
        {
            contextWgpu->invalidateIndexBuffer();
        }

        stagingUploads.push_back({currentStagingDataPosition, &mStreamingIndexBuffer, copySize});

        currentStagingDataPosition += copySize;

        // Indices are streamed to the start of the buffer. Tell the draw call command to use 0 for
        // firstIndex.
        *adjustedIndicesPtr = 0;
    }

    for (size_t attribIndex : clientAttributesToSync)
    {
        const gl::VertexAttribute &attrib = attribs[attribIndex];
        const gl::VertexBinding &binding  = bindings[attrib.bindingIndex];

        size_t streamedVertexCount = gl::ComputeVertexBindingElementCount(
            binding.getDivisor(), indexRange->vertexCount(), instanceCount);

        const size_t sourceStride = ComputeVertexAttributeStride(attrib, binding);
        const size_t typeSize     = gl::ComputeVertexAttributeTypeSize(attrib);

        // Vertices do not apply the 'start' offset when the divisor is non-zero even when doing
        // a non-instanced draw call
        const size_t firstIndex = (binding.getDivisor() == 0) ? indexRange->start : 0;

        // Attributes using client memory ignore the VERTEX_ATTRIB_BINDING state.
        // https://www.opengl.org/registry/specs/ARB/vertex_attrib_binding.txt
        const uint8_t *inputPointer = static_cast<const uint8_t *>(attrib.pointer);

        // Pack the data when copying it, user could have supplied a very large stride that
        // would cause the buffer to be much larger than needed.
        if (typeSize == sourceStride)
        {
            // Can copy in one go, the data is packed
            memcpy(stagingData + currentStagingDataPosition,
                   inputPointer + (sourceStride * firstIndex), streamedVertexCount * typeSize);
        }
        else
        {
            for (size_t vertexIdx = 0; vertexIdx < streamedVertexCount; vertexIdx++)
            {
                uint8_t *out = stagingData + currentStagingDataPosition + (typeSize * vertexIdx);
                const uint8_t *in = inputPointer + sourceStride * (vertexIdx + firstIndex);
                memcpy(out, in, typeSize);
            }
        }

        size_t copySize =
            rx::roundUpPow2(streamedVertexCount * typeSize, webgpu::kBufferCopyToBufferAlignment);

        bool newBuffer = false;
        ANGLE_TRY(ensureStreamingBufferCreated(
            mStreamingArrayBuffers[attribIndex], copySize,
            wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Vertex, &newBuffer));
        if (newBuffer)
        {
            contextWgpu->invalidateVertexBuffer(attribIndex);
        }

        stagingUploads.push_back(
            {currentStagingDataPosition, &mStreamingArrayBuffers[attribIndex], copySize});

        currentStagingDataPosition += copySize;
    }

    ANGLE_TRY(stagingBuffer.unmap());
    ANGLE_TRY(contextWgpu->flush(webgpu::RenderPassClosureReason::VertexArrayStreaming));

    contextWgpu->ensureCommandEncoderCreated();
    wgpu::CommandEncoder &commandEncoder = contextWgpu->getCurrentCommandEncoder();

    for (const BufferCopy &copy : stagingUploads)
    {
        commandEncoder.CopyBufferToBuffer(stagingBuffer.getBuffer(), copy.sourceOffset,
                                          copy.dest->getBuffer(), 0, copy.size);
    }

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
            SetBitField(mCurrentAttribs[attribIndex].offset, 0);
            mCurrentArrayBuffers[attribIndex] = &mStreamingArrayBuffers[attribIndex];
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
        mCurrentIndexBuffer = &mStreamingIndexBuffer;
    }

    return angle::Result::Continue;
}

}  // namespace rx
