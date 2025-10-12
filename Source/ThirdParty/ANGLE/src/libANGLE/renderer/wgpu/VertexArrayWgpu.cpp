//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VertexArrayWgpu.cpp:
//    Implements the class methods for VertexArrayWgpu.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "libANGLE/renderer/wgpu/VertexArrayWgpu.h"

#include "common/PackedEnums.h"
#include "common/debug.h"
#include "libANGLE/Error.h"
#include "libANGLE/renderer/wgpu/ContextWgpu.h"
#include "libANGLE/renderer/wgpu/wgpu_utils.h"

namespace rx
{

namespace
{
bool AttributeNeedsStreaming(ContextWgpu *context,
                             const gl::VertexAttribute &attrib,
                             const gl::VertexBinding &binding,
                             const gl::Buffer *bufferGl)
{
    const size_t stride = ComputeVertexAttributeStride(attrib, binding);
    if (stride % 4 != 0)
    {
        return true;
    }

    const size_t typeSize = gl::ComputeVertexAttributeTypeSize(attrib);
    if (stride % typeSize != 0)
    {
        return true;
    }

    const webgpu::Format &vertexFormat = context->getFormat(attrib.format->glInternalFormat);
    if (vertexFormat.vertexLoadRequiresConversion())
    {
        return true;
    }

    if (!bufferGl || bufferGl->getSize() == 0)
    {
        return true;
    }

    return false;
}

template <typename SourceType, typename DestType>
void CopyIndexData(const uint8_t *sourceData, size_t count, uint8_t *destData)
{
    if constexpr (std::is_same<SourceType, DestType>::value)
    {
        memcpy(destData, sourceData, sizeof(SourceType) * count);
    }
    else
    {
        for (size_t i = 0; i < count; i++)
        {
            DestType *dst         = reinterpret_cast<DestType *>(destData) + i;
            const SourceType *src = reinterpret_cast<const SourceType *>(sourceData) + i;
            *dst                  = static_cast<DestType>(*src);
        }
    }
}
using CopyIndexFunction = void (*)(const uint8_t *sourceData, size_t count, uint8_t *destData);

CopyIndexFunction GetCopyIndexFunction(gl::DrawElementsType sourceType,
                                       gl::DrawElementsType destType)
{
    static_assert(static_cast<size_t>(gl::DrawElementsType::UnsignedByte) == 0);
    static_assert(static_cast<size_t>(gl::DrawElementsType::UnsignedShort) == 1);
    static_assert(static_cast<size_t>(gl::DrawElementsType::UnsignedInt) == 2);
    ASSERT(static_cast<size_t>(sourceType) <= 2);
    ASSERT(static_cast<size_t>(destType) <= 2);
    ASSERT(static_cast<size_t>(destType) >=
           static_cast<size_t>(sourceType));  // Can't copy to a smaller type

    constexpr CopyIndexFunction copyFunctions[3][3] = {
        {
            CopyIndexData<GLubyte, GLubyte>,
            CopyIndexData<GLubyte, GLushort>,
            CopyIndexData<GLubyte, GLuint>,
        },
        {
            nullptr,
            CopyIndexData<GLushort, GLushort>,
            CopyIndexData<GLushort, GLuint>,
        },
        {
            nullptr,
            nullptr,
            CopyIndexData<GLuint, GLuint>,
        },
    };

    CopyIndexFunction copyFunction =
        copyFunctions[static_cast<size_t>(sourceType)][static_cast<size_t>(destType)];
    ASSERT(copyFunction != nullptr);
    return copyFunction;
}

}  // namespace

VertexArrayWgpu::VertexArrayWgpu(const gl::VertexArrayState &data,
                                 const gl::VertexArrayBuffers &vertexArrayBuffers)
    : VertexArrayImpl(data, vertexArrayBuffers)
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
            case gl::VertexArray::DIRTY_BIT_ELEMENT_ARRAY_BUFFER:
            case gl::VertexArray::DIRTY_BIT_ELEMENT_ARRAY_BUFFER_DATA:
                ANGLE_TRY(syncDirtyElementArrayBuffer(contextWgpu));
                contextWgpu->invalidateIndexBuffer();
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

angle::Result VertexArrayWgpu::syncClientArrays(
    const gl::Context *context,
    const gl::AttributesMask &activeAttributesMask,
    gl::PrimitiveMode mode,
    GLint first,
    GLsizei count,
    GLsizei instanceCount,
    gl::DrawElementsType sourceDrawElementsTypeOrInvalid,
    const void *indices,
    GLint baseVertex,
    bool primitiveRestartEnabled,
    const void **adjustedIndicesPtr,
    uint32_t *indexCountOut)
{
    const DawnProcTable *wgpu = webgpu::GetProcs(context);

    *adjustedIndicesPtr = indices;

    gl::AttributesMask clientAttributesToSync =
        (mState.getClientMemoryAttribsMask() | mForcedStreamingAttributes) &
        mState.getEnabledAttributesMask() & activeAttributesMask;

    gl::DrawElementsType destDrawElementsTypeOrInvalid = sourceDrawElementsTypeOrInvalid;

    IndexDataNeedsStreaming indexDataNeedsStreaming = determineIndexDataNeedsStreaming(
        sourceDrawElementsTypeOrInvalid, count, mode, &destDrawElementsTypeOrInvalid);

    if (!clientAttributesToSync.any() && indexDataNeedsStreaming == IndexDataNeedsStreaming::No)
    {
        return angle::Result::Continue;
    }

    gl::Buffer *elementArrayBuffer = getElementArrayBuffer();
    ContextWgpu *contextWgpu       = webgpu::GetImpl(context);
    webgpu::DeviceHandle device    = webgpu::GetDevice(context);
    GLsizei adjustedCount          = count;
    const uint8_t *srcIndexData    = static_cast<const uint8_t *>(indices);
    if (elementArrayBuffer)
    {
        BufferWgpu *elementArrayBufferWgpu = GetImplAs<BufferWgpu>(elementArrayBuffer);
        size_t sourceOffset =
            rx::roundDownPow2(reinterpret_cast<size_t>(indices), webgpu::kBufferMapOffsetAlignment);
        ASSERT(sourceOffset < elementArrayBufferWgpu->getBuffer().actualSize());
        size_t mapReadSize = rx::roundUpPow2(
            static_cast<size_t>(elementArrayBufferWgpu->getBuffer().actualSize()) - sourceOffset,
            webgpu::kBufferCopyToBufferAlignment);
        if (!elementArrayBufferWgpu->getBuffer().isMappedForRead())
        {
            ANGLE_TRY(elementArrayBufferWgpu->getBuffer().mapImmediate(
                contextWgpu, WGPUMapMode_Read, sourceOffset, mapReadSize));
        }
        srcIndexData =
            elementArrayBufferWgpu->getBuffer().getMapReadPointer(sourceOffset, mapReadSize);
    }
    ANGLE_TRY(calculateAdjustedIndexCount(mode, primitiveRestartEnabled,
                                          destDrawElementsTypeOrInvalid, count, srcIndexData,
                                          &adjustedCount));

    // If there aren't any client attributes to sync but the adjusted count is 0, that means
    // there no indices outside the primitive restart index, so this is a no-op.
    if (!clientAttributesToSync.any() && adjustedCount == 0)
    {
        return angle::Result::Continue;
    }
    if (indexCountOut)
    {
        *indexCountOut = adjustedCount;
    }

    // If any attributes need to be streamed, we need to know the index range. We also need to know
    // the index range if there is a draw arrays call and we have to stream the index data for it.
    std::optional<gl::IndexRange> indexRange;
    if (clientAttributesToSync.any())
    {
        GLint startVertex  = 0;
        size_t vertexCount = 0;
        ANGLE_TRY(GetVertexRangeInfo(context, first, count, sourceDrawElementsTypeOrInvalid,
                                     indices, baseVertex, &startVertex, &vertexCount));
        indexRange =
            gl::IndexRange(startVertex, static_cast<GLuint>(startVertex + vertexCount - 1));
    }
    else if (indexDataNeedsStreaming == IndexDataNeedsStreaming::Yes &&
             sourceDrawElementsTypeOrInvalid == gl::DrawElementsType::InvalidEnum)
    {
        indexRange = gl::IndexRange(first, first + count - 1);
    }

    // Pre-compute the total size of all streamed vertex and index data so a single staging buffer
    // can be used
    size_t stagingBufferSize = 0;

    std::optional<size_t> destIndexDataSize;
    std::optional<size_t> destIndexUnitSize;
    if (indexDataNeedsStreaming != IndexDataNeedsStreaming::No)
    {
        destIndexUnitSize =
            static_cast<size_t>(gl::GetDrawElementsTypeSize(destDrawElementsTypeOrInvalid));
        destIndexDataSize = destIndexUnitSize.value() * adjustedCount;
    }

    ANGLE_TRY(calculateStagingBufferSize(
        sourceDrawElementsTypeOrInvalid == destDrawElementsTypeOrInvalid, primitiveRestartEnabled,
        contextWgpu, indexDataNeedsStreaming, destIndexDataSize, clientAttributesToSync,
        instanceCount, indexRange, &stagingBufferSize));

    ASSERT(stagingBufferSize % webgpu::kBufferSizeAlignment == 0);
    webgpu::BufferHelper stagingBuffer;
    uint8_t *stagingData              = nullptr;
    size_t currentStagingDataPosition = 0;
    if (stagingBufferSize > 0)
    {
        ASSERT(stagingBufferSize > 0);
        ASSERT(stagingBufferSize % webgpu::kBufferSizeAlignment == 0);
        ANGLE_TRY(stagingBuffer.initBuffer(wgpu, device, stagingBufferSize,
                                           WGPUBufferUsage_CopySrc | WGPUBufferUsage_MapWrite,
                                           webgpu::MapAtCreation::Yes));
        stagingData = stagingBuffer.getMapWritePointer(0, stagingBufferSize);
    }

    struct BufferCopy
    {
        uint64_t sourceOffset;
        webgpu::BufferHelper *src;
        webgpu::BufferHelper *dest;
        uint64_t destOffset;
        uint64_t size;
    };
    std::vector<BufferCopy> stagingUploads;

    if (indexDataNeedsStreaming == IndexDataNeedsStreaming::Yes)
    {
        // Indices are streamed to the start of the buffer. Tell the draw call command to use 0 for
        // firstIndex.
        *adjustedIndicesPtr = 0;
        ASSERT(destIndexDataSize.has_value());
        ASSERT(destIndexUnitSize.has_value());

        size_t destIndexBufferSize =
            rx::roundUpPow2(destIndexDataSize.value(), webgpu::kBufferCopyToBufferAlignment);
        ANGLE_TRY(ensureBufferCreated(context, mStreamingIndexBuffer, destIndexBufferSize, 0,
                                      WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index,
                                      BufferType::IndexBuffer));
        // TODO(anglebug.com/401226623): Don't use the staging buffer when the adjustedCount for
        // primitive restarts is count + 1.
        if (primitiveRestartEnabled && mode == gl::PrimitiveMode::LineLoop)
        {
            rx::StreamEmulatedLineLoopIndices(destDrawElementsTypeOrInvalid, count, srcIndexData,
                                              stagingData,
                                              /* shouldConvertUint8= */ true);
            if (elementArrayBuffer)
            {
                BufferWgpu *elementArrayBufferWgpu = GetImplAs<BufferWgpu>(elementArrayBuffer);
                ANGLE_TRY(elementArrayBufferWgpu->getBuffer().unmap());
            }
            stagingUploads.push_back({currentStagingDataPosition, &stagingBuffer,
                                      &mStreamingIndexBuffer, 0, destIndexBufferSize});
        }
        else if (sourceDrawElementsTypeOrInvalid == destDrawElementsTypeOrInvalid &&
                 elementArrayBuffer)
        {
            // Use the element array buffer as the source for the new streaming index buffer. This
            // condition is only hit when an indexed draw call has an element array buffer and is
            // trying to draw line loops.

            // When using an element array buffer, 'indices' is an offset to the first element.
            size_t sourceOffset                = reinterpret_cast<size_t>(indices);
            BufferWgpu *elementArrayBufferWgpu = GetImplAs<BufferWgpu>(elementArrayBuffer);
            webgpu::BufferHelper *sourceBuffer = &elementArrayBufferWgpu->getBuffer();

            size_t copySize = rx::roundUpPow2(destIndexUnitSize.value() * count,
                                              webgpu::kBufferCopyToBufferAlignment);
            stagingUploads.push_back(
                {sourceOffset, sourceBuffer, &mStreamingIndexBuffer, 0, copySize});

            if (mode == gl::PrimitiveMode::LineLoop)
            {
                // Emulate line loops with an additional copy of the first index at the end of the
                // buffer
                size_t lastOffset = copySize;
                stagingUploads.push_back({sourceOffset, sourceBuffer, &mStreamingIndexBuffer,
                                          lastOffset,
                                          rx::roundUpPow2(destIndexUnitSize.value(),
                                                          webgpu::kBufferCopyToBufferAlignment)});
            }
        }
        // Handle emulating line loop for draw arrays calls.
        else if (sourceDrawElementsTypeOrInvalid == gl::DrawElementsType::InvalidEnum)
        {
            ASSERT(destDrawElementsTypeOrInvalid != gl::DrawElementsType::InvalidEnum);
            ASSERT(mode == gl::PrimitiveMode::LineLoop);
            uint32_t clampedVertexCount = gl::clampCast<uint32_t>(indexRange->vertexCount());
            uint32_t startVertex        = static_cast<uint32_t>(indexRange->start());
            size_t index                = currentStagingDataPosition;
            for (uint32_t i = 0; i < clampedVertexCount; i++)
            {
                uint32_t copyData = startVertex + i;
                memcpy(stagingData + index, &copyData, destIndexUnitSize.value());
                index += destIndexUnitSize.value();
            }
            memcpy(stagingData + currentStagingDataPosition + destIndexUnitSize.value() * count,
                   &startVertex, destIndexUnitSize.value());

            size_t copySize = destIndexBufferSize;
            stagingUploads.push_back(
                {currentStagingDataPosition, &stagingBuffer, &mStreamingIndexBuffer, 0, copySize});
            currentStagingDataPosition += copySize;
        }
        else
        {
            webgpu::BufferReadback readbackBuffer;
            if (getElementArrayBuffer())
            {
                webgpu::BufferHelper &srcBuffer =
                    webgpu::GetImpl(getElementArrayBuffer())->getBuffer();

                const GLuint srcIndexTypeSize =
                    gl::GetDrawElementsTypeSize(sourceDrawElementsTypeOrInvalid);
                const size_t srcIndexOffset = reinterpret_cast<uintptr_t>(indices);

                ANGLE_TRY(srcBuffer.readDataImmediate(
                    contextWgpu, srcIndexOffset, count * srcIndexTypeSize,
                    webgpu::RenderPassClosureReason::IndexRangeReadback, &readbackBuffer));
                srcIndexData = readbackBuffer.data;
            }

            CopyIndexFunction indexCopyFunction = GetCopyIndexFunction(
                sourceDrawElementsTypeOrInvalid, destDrawElementsTypeOrInvalid);
            ASSERT(stagingData != nullptr);
            indexCopyFunction(srcIndexData, count, stagingData + currentStagingDataPosition);

            if (mode == gl::PrimitiveMode::LineLoop)
            {
                indexCopyFunction(
                    srcIndexData, count,
                    stagingData + currentStagingDataPosition + (destIndexUnitSize.value() * count));
            }

            size_t copySize = destIndexBufferSize;
            stagingUploads.push_back(
                {currentStagingDataPosition, &stagingBuffer, &mStreamingIndexBuffer, 0, copySize});
            currentStagingDataPosition += copySize;
        }
    }

    const std::vector<gl::VertexAttribute> &attribs = mState.getVertexAttributes();
    const std::vector<gl::VertexBinding> &bindings  = mState.getVertexBindings();
    for (size_t attribIndex : clientAttributesToSync)
    {
        const gl::VertexAttribute &attrib = attribs[attribIndex];
        const gl::VertexBinding &binding  = bindings[attrib.bindingIndex];
        const gl::Buffer *buffer          = getVertexArrayBuffer(attrib.bindingIndex);

        size_t streamedVertexCount = gl::ComputeVertexBindingElementCount(
            binding.getDivisor(), indexRange->vertexCount(), instanceCount);

        const size_t sourceStride   = ComputeVertexAttributeStride(attrib, binding);
        const size_t sourceTypeSize = gl::ComputeVertexAttributeTypeSize(attrib);

        // Vertices do not apply the 'start' offset when the divisor is non-zero even when doing
        // a non-instanced draw call
        const size_t firstIndex = (binding.getDivisor() == 0) ? indexRange->start() : 0;

        // Attributes using client memory ignore the VERTEX_ATTRIB_BINDING state.
        // https://www.opengl.org/registry/specs/ARB/vertex_attrib_binding.txt
        const uint8_t *inputPointer = static_cast<const uint8_t *>(attrib.pointer);

        webgpu::BufferReadback readbackBuffer;
        if (buffer)
        {
            webgpu::BufferHelper &srcBuffer = webgpu::GetImpl(buffer)->getBuffer();
            size_t sourceVertexDataSize =
                sourceStride * (firstIndex + streamedVertexCount - 1) + sourceTypeSize;

            ANGLE_TRY(srcBuffer.readDataImmediate(
                contextWgpu, 0, reinterpret_cast<uintptr_t>(attrib.pointer) + sourceVertexDataSize,
                webgpu::RenderPassClosureReason::IndexRangeReadback, &readbackBuffer));
            inputPointer = readbackBuffer.data + reinterpret_cast<uintptr_t>(attrib.pointer);
        }

        const webgpu::Format &vertexFormat =
            contextWgpu->getFormat(attrib.format->glInternalFormat);
        size_t destTypeSize = vertexFormat.getActualBufferFormat().pixelBytes;

        VertexCopyFunction copyFunction = vertexFormat.getVertexLoadFunction();
        ASSERT(copyFunction != nullptr);
        ASSERT(stagingData != nullptr);
        copyFunction(inputPointer + (sourceStride * firstIndex), sourceStride, streamedVertexCount,
                     stagingData + currentStagingDataPosition);

        size_t copySize = rx::roundUpPow2(streamedVertexCount * destTypeSize,
                                          webgpu::kBufferCopyToBufferAlignment);
        // Pad the streaming buffer with empty data at the beginning to put the vertex data at the
        // same index location. The stride is tightly packed.
        size_t destCopyOffset = firstIndex * destTypeSize;

        ANGLE_TRY(ensureBufferCreated(
            context, mStreamingArrayBuffers[attribIndex], destCopyOffset + copySize, attribIndex,
            WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex, BufferType::ArrayBuffer));

        stagingUploads.push_back({currentStagingDataPosition, &stagingBuffer,
                                  &mStreamingArrayBuffers[attribIndex], destCopyOffset, copySize});

        currentStagingDataPosition += copySize;
    }

    if (stagingBuffer.valid())
    {
        ANGLE_TRY(stagingBuffer.unmap());
    }
    ANGLE_TRY(contextWgpu->flush(webgpu::RenderPassClosureReason::VertexArrayStreaming));

    contextWgpu->ensureCommandEncoderCreated();
    webgpu::CommandEncoderHandle &commandEncoder = contextWgpu->getCurrentCommandEncoder();

    for (const BufferCopy &copy : stagingUploads)
    {
        wgpu->commandEncoderCopyBufferToBuffer(commandEncoder.get(), copy.src->getBuffer().get(),
                                               copy.sourceOffset, copy.dest->getBuffer().get(),
                                               copy.destOffset, copy.size);
    }

    return angle::Result::Continue;
}

angle::Result VertexArrayWgpu::syncDirtyAttrib(ContextWgpu *contextWgpu,
                                               const gl::VertexAttribute &attrib,
                                               const gl::VertexBinding &binding,
                                               size_t attribIndex)
{
    gl::Buffer *bufferGl = getVertexArrayBuffer(attrib.bindingIndex);
    mForcedStreamingAttributes[attribIndex] =
        AttributeNeedsStreaming(contextWgpu, attrib, binding, bufferGl);

    if (attrib.enabled)
    {
        SetBitField(mCurrentAttribs[attribIndex].enabled, true);
        const webgpu::Format &webgpuFormat =
            contextWgpu->getFormat(attrib.format->glInternalFormat);
        SetBitField(mCurrentAttribs[attribIndex].format, webgpuFormat.getActualWgpuVertexFormat());
        SetBitField(mCurrentAttribs[attribIndex].shaderLocation, attribIndex);

        if (!mForcedStreamingAttributes[attribIndex])
        {
            // Data is sourced directly from the array buffer.
            SetBitField(mCurrentAttribs[attribIndex].offset, 0);
            SetBitField(mCurrentAttribs[attribIndex].stride, binding.getStride());

            ASSERT(bufferGl);
            BufferWgpu *bufferWgpu                   = webgpu::GetImpl(bufferGl);
            mCurrentArrayBuffers[attribIndex].buffer = &(bufferWgpu->getBuffer());
            mCurrentArrayBuffers[attribIndex].offset = reinterpret_cast<uintptr_t>(attrib.pointer);
        }
        else
        {
            SetBitField(mCurrentAttribs[attribIndex].offset, 0);
            SetBitField(mCurrentAttribs[attribIndex].stride,
                        webgpuFormat.getActualBufferFormat().pixelBytes);
            mCurrentArrayBuffers[attribIndex].buffer = &mStreamingArrayBuffers[attribIndex];
            mCurrentArrayBuffers[attribIndex].offset = 0;
        }
    }
    else
    {
        memset(&mCurrentAttribs[attribIndex], 0, sizeof(webgpu::PackedVertexAttribute));
        mCurrentArrayBuffers[attribIndex].buffer = nullptr;
        mCurrentArrayBuffers[attribIndex].offset = 0;
    }

    return angle::Result::Continue;
}

angle::Result VertexArrayWgpu::syncDirtyElementArrayBuffer(ContextWgpu *contextWgpu)
{
    gl::Buffer *bufferGl = getElementArrayBuffer();
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

angle::Result VertexArrayWgpu::ensureBufferCreated(const gl::Context *context,
                                                   webgpu::BufferHelper &buffer,
                                                   size_t size,
                                                   size_t attribIndex,
                                                   WGPUBufferUsage usage,
                                                   BufferType bufferType)
{
    ContextWgpu *contextWgpu = webgpu::GetImpl(context);
    const DawnProcTable *wgpu = webgpu::GetProcs(contextWgpu);
    if (!buffer.valid() || buffer.requestedSize() < size ||
        wgpu->bufferGetUsage(buffer.getBuffer().get()) != usage)
    {
        webgpu::DeviceHandle device = webgpu::GetDevice(context);
        ANGLE_TRY(buffer.initBuffer(wgpu, device, size, usage, webgpu::MapAtCreation::No));

        if (bufferType == BufferType::IndexBuffer)
        {
            contextWgpu->invalidateIndexBuffer();
        }
        else
        {
            ASSERT(bufferType == BufferType::ArrayBuffer);
            contextWgpu->invalidateVertexBuffer(attribIndex);
        }
    }

    if (bufferType == BufferType::IndexBuffer)
    {
        mCurrentIndexBuffer = &buffer;
    }
    return angle::Result::Continue;
}

IndexDataNeedsStreaming VertexArrayWgpu::determineIndexDataNeedsStreaming(
    gl::DrawElementsType sourceDrawElementsTypeOrInvalid,
    GLsizei count,
    gl::PrimitiveMode mode,
    gl::DrawElementsType *destDrawElementsTypeOrInvalidOut)
{
    if (sourceDrawElementsTypeOrInvalid == gl::DrawElementsType::UnsignedByte)
    {
        // Promote 8-bit indices to 16-bit indices
        *destDrawElementsTypeOrInvalidOut = gl::DrawElementsType::UnsignedShort;
        return IndexDataNeedsStreaming::Yes;
    }
    else if (mode == gl::PrimitiveMode::LineLoop)
    {
        // Index data will always need streaming for line loop mode regardless of what type of draw
        // call it is.
        if (sourceDrawElementsTypeOrInvalid == gl::DrawElementsType::InvalidEnum)
        {
            // Line loop draw array calls are emulated via indexed draw calls, so an index type must
            // be set.
            if (count >= std::numeric_limits<unsigned short>::max())
            {
                *destDrawElementsTypeOrInvalidOut = gl::DrawElementsType::UnsignedInt;
            }
            else
            {
                *destDrawElementsTypeOrInvalidOut = gl::DrawElementsType::UnsignedShort;
            }
        }
        return IndexDataNeedsStreaming::Yes;
    }
    else if (sourceDrawElementsTypeOrInvalid != gl::DrawElementsType::InvalidEnum &&
             !getElementArrayBuffer())
    {
        // Index data needs to be uploaded to the GPU
        return IndexDataNeedsStreaming::Yes;
    }
    return IndexDataNeedsStreaming::No;
}

angle::Result VertexArrayWgpu::calculateAdjustedIndexCount(
    gl::PrimitiveMode mode,
    bool primitiveRestartEnabled,
    gl::DrawElementsType destDrawElementsTypeOrInvalid,
    GLsizei count,
    const uint8_t *srcIndexData,
    GLsizei *adjustedCountOut)
{
    if (mode == gl::PrimitiveMode::LineLoop)
    {
        if (primitiveRestartEnabled)
        {
            *adjustedCountOut = GetLineLoopWithRestartIndexCount(destDrawElementsTypeOrInvalid,
                                                                 count, srcIndexData);
        }
        else
        {
            ++*adjustedCountOut;
        }
    }
    return angle::Result::Continue;
}

angle::Result VertexArrayWgpu::calculateStagingBufferSize(
    bool srcDestDrawElementsTypeEqual,
    bool primitiveRestartEnabled,
    ContextWgpu *contextWgpu,
    IndexDataNeedsStreaming indexDataNeedsStreaming,
    std::optional<size_t> destIndexDataSize,
    gl::AttributesMask clientAttributesToSync,
    GLsizei instanceCount,
    std::optional<gl::IndexRange> indexRange,
    size_t *stagingBufferSizeOut)
{
    if (indexDataNeedsStreaming == IndexDataNeedsStreaming::Yes)
    {  // Allocating staging buffer space for indices is only needed when there is no source index
        // buffer or index data conversion is needed
        if (primitiveRestartEnabled || !getElementArrayBuffer() || !srcDestDrawElementsTypeEqual)
        {
            *stagingBufferSizeOut +=
                rx::roundUpPow2(destIndexDataSize.value(), webgpu::kBufferCopyToBufferAlignment);
        }
    }

    const std::vector<gl::VertexAttribute> &attribs = mState.getVertexAttributes();
    const std::vector<gl::VertexBinding> &bindings  = mState.getVertexBindings();

    if (clientAttributesToSync.any())
    {
        for (size_t attribIndex : clientAttributesToSync)
        {
            const gl::VertexAttribute &attrib = attribs[attribIndex];
            const gl::VertexBinding &binding  = bindings[attrib.bindingIndex];

            size_t elementCount = gl::ComputeVertexBindingElementCount(
                binding.getDivisor(), indexRange->vertexCount(), instanceCount);

            const webgpu::Format &vertexFormat =
                contextWgpu->getFormat(attrib.format->glInternalFormat);
            size_t destTypeSize = vertexFormat.getActualBufferFormat().pixelBytes;
            ASSERT(destTypeSize > 0);

            size_t attribSize = destTypeSize * elementCount;
            *stagingBufferSizeOut +=
                rx::roundUpPow2(attribSize, webgpu::kBufferCopyToBufferAlignment);
        }
    }

    if (*stagingBufferSizeOut > contextWgpu->getDisplay()->getLimitsWgpu().maxBufferSize)
    {
        ERR() << "Staging buffer size of " << stagingBufferSizeOut
              << " in sync client arrays is larger than the max buffer size "
              << contextWgpu->getDisplay()->getLimitsWgpu().maxBufferSize;
        return angle::Result::Stop;
    }
    return angle::Result::Continue;
}

}  // namespace rx
