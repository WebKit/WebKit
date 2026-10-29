//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VertexArrayWgpu.cpp:
//    Implements the class methods for VertexArrayWgpu.
//

#include "libANGLE/renderer/wgpu/VertexArrayWgpu.h"

#include <array>
#include <sstream>
#include <string>

#include "common/unsafe_buffers.h"

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
    // WebGPU requires array strides to be multiples of 4. updateVertexBufferSlots() checks the
    // offsets that WebGPU requires.
    const size_t stride = ComputeVertexAttributeStride(attrib, binding);
    if (stride % 4 != 0)
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
        ANGLE_UNSAFE_TODO(memcpy(destData, sourceData, sizeof(SourceType) * count));
    }
    else
    {
        for (size_t i = 0; i < count; i++)
        {
            DestType *dst = ANGLE_UNSAFE_TODO(reinterpret_cast<DestType *>(destData) + i);
            const SourceType *src =
                ANGLE_UNSAFE_TODO(reinterpret_cast<const SourceType *>(sourceData) + i);
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

    static constexpr std::array<std::array<CopyIndexFunction, 3>, 3> copyFunctions = {{
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
    }};

    CopyIndexFunction copyFunction =
        copyFunctions[static_cast<size_t>(sourceType)][static_cast<size_t>(destType)];
    ASSERT(copyFunction != nullptr);
    return copyFunction;
}

std::string GetStagingBufferTooLargeMessage(size_t stagingBufferSize, uint64_t maxBufferSize)
{
    std::ostringstream message;
    message << "The streamed vertex and index data of " << stagingBufferSize
            << " bytes is larger than the maximum buffer size " << maxBufferSize << ".";
    return message.str();
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

    for (size_t dirtyBit : dirtyBits)
    {
        switch (dirtyBit)
        {
            case gl::VertexArray::DIRTY_BIT_ELEMENT_ARRAY_BUFFER:
            case gl::VertexArray::DIRTY_BIT_ELEMENT_ARRAY_BUFFER_DATA:
                ANGLE_TRY(syncDirtyElementArrayBuffer(contextWgpu));
                contextWgpu->invalidateIndexBuffer();
                break;

#define ANGLE_VERTEX_DIRTY_ATTRIB_FUNC(INDEX)                                               \
    case gl::VertexArray::DIRTY_BIT_ATTRIB_0 + INDEX:                                       \
        syncDirtyAttrib(contextWgpu, attribs[INDEX], bindings[attribs[INDEX].bindingIndex], \
                        INDEX);                                                             \
        (*attribBits)[INDEX].reset();                                                       \
        syncedAttributes.set(INDEX);                                                        \
        break;

                ANGLE_VERTEX_INDEX_CASES(ANGLE_VERTEX_DIRTY_ATTRIB_FUNC)

#define ANGLE_VERTEX_DIRTY_BINDING_FUNC(INDEX)                                              \
    case gl::VertexArray::DIRTY_BIT_BINDING_0 + INDEX:                                      \
        syncDirtyAttrib(contextWgpu, attribs[INDEX], bindings[attribs[INDEX].bindingIndex], \
                        INDEX);                                                             \
        (*bindingBits)[INDEX].reset();                                                      \
        syncedAttributes.set(INDEX);                                                        \
        break;

                ANGLE_VERTEX_INDEX_CASES(ANGLE_VERTEX_DIRTY_BINDING_FUNC)

#define ANGLE_VERTEX_DIRTY_BUFFER_DATA_FUNC(INDEX)                                          \
    case gl::VertexArray::DIRTY_BIT_BUFFER_DATA_0 + INDEX:                                  \
        syncDirtyAttrib(contextWgpu, attribs[INDEX], bindings[attribs[INDEX].bindingIndex], \
                        INDEX);                                                             \
        syncedAttributes.set(INDEX);                                                        \
        break;

                ANGLE_VERTEX_INDEX_CASES(ANGLE_VERTEX_DIRTY_BUFFER_DATA_FUNC)
            default:
                break;
        }
    }

    if (syncedAttributes.any())
    {
        contextWgpu->invalidateVertexBufferSlots();
    }
    return angle::Result::Continue;
}

angle::Result VertexArrayWgpu::updateVertexBufferSlots(ContextWgpu *contextWgpu,
                                                       const gl::AttributesMask &activeAttributes)
{
    const WGPULimits &limits    = contextWgpu->getDisplay()->getLimitsWgpu();
    const size_t maxSlotCount   = std::min<size_t>(limits.maxVertexBuffers, gl::MAX_VERTEX_ATTRIBS);
    const size_t maxArrayStride = limits.maxVertexBufferArrayStride;

    const std::vector<gl::VertexAttribute> &attribs = mState.getVertexAttributes();
    const std::vector<gl::VertexBinding> &bindings  = mState.getVertexBindings();

    // The attributes that read a buffer directly. The offsets of all attributes of a slot are in
    // [firstOffset, endOffset), which is within one element of the slot. The slot is bound at
    // firstOffset rounded down to a multiple of 4, as setVertexBuffer() requires.
    struct BufferSlot
    {
        webgpu::BufferHelper *buffer;
        size_t stride;
        GLuint divisor;
        uintptr_t firstOffset;
        uintptr_t endOffset;
        gl::AttributesMask attributes;
    };
    angle::FixedVector<BufferSlot, gl::MAX_VERTEX_ATTRIBS> bufferSlots;
    gl::AttributesMask streamedAttributes;
    // The attributes whose offset is past the end of their buffer.
    gl::AttributesMask zeroAttributes;

    for (size_t attribIndex : mState.getEnabledAttributesMask() & activeAttributes)
    {
        const gl::VertexAttribute &attrib = attribs[attribIndex];
        const gl::VertexBinding &binding  = bindings[attrib.bindingIndex];
        gl::Buffer *bufferGl              = getVertexArrayBuffer(attrib.bindingIndex);
        const uintptr_t offset            = gl::ComputeVertexAttributeOffset(attrib, binding);
        const size_t size                 = gl::ComputeVertexAttributeTypeSize(attrib);

        if (bufferGl)
        {
            const uint64_t bufferSize = static_cast<uint64_t>(bufferGl->getSize());
            if (ANGLE_UNLIKELY(offset >= bufferSize || bufferSize - offset < size))
            {
                // Not even one vertex fits after the offset. The fetched values are undefined in
                // ES (KHR_robust_buffer_access_behavior allows any value from the buffer, or
                // zero), but WebGPU's setVertexBuffer() rejects an offset past the end of the
                // buffer and draw() rejects vertex ranges that do not fit the bound size. Either
                // error invalidates the render pass and its command encoder, losing all work
                // recorded in it, and streaming the attribute would read past the end of the
                // buffer. Read zeros from mZeroBuffer instead, like the Vulkan backend's
                // empty-buffer path.
                zeroAttributes.set(attribIndex);
                continue;
            }
        }

        if (mForcedStreamingAttributes[attribIndex])
        {
            streamedAttributes.set(attribIndex);
            continue;
        }

        ASSERT(bufferGl);
        webgpu::BufferHelper *buffer = &webgpu::GetImpl(bufferGl)->getBuffer();
        const size_t stride          = binding.getStride();
        const GLuint divisor         = binding.getDivisor();
        // WebGPU requires that an attribute ends within the array stride, or within
        // maxVertexBufferArrayStride when the stride is 0.
        const size_t maxElementSize = stride == 0 ? maxArrayStride : stride;

        // WebGPU requires attribute offsets to be multiples of min(4, size).
        if (offset % std::min<size_t>(4, size) != 0)
        {
            streamedAttributes.set(attribIndex);
            continue;
        }

        bool shared = false;
        for (BufferSlot &slot : bufferSlots)
        {
            if (slot.buffer != buffer || slot.stride != stride || slot.divisor != divisor)
            {
                continue;
            }
            const uintptr_t firstOffset = std::min(slot.firstOffset, offset);
            const uintptr_t endOffset   = std::max(slot.endOffset, offset + size);
            if (endOffset - rx::roundDownPow2<uintptr_t>(firstOffset, 4) > maxElementSize)
            {
                continue;
            }
            slot.firstOffset = firstOffset;
            slot.endOffset   = endOffset;
            slot.attributes.set(attribIndex);
            shared = true;
            break;
        }
        if (shared)
        {
            continue;
        }

        if (offset + size - rx::roundDownPow2<uintptr_t>(offset, 4) > maxElementSize)
        {
            streamedAttributes.set(attribIndex);
            continue;
        }
        BufferSlot newSlot = {buffer, stride, divisor, offset, offset + size, {}};
        newSlot.attributes.set(attribIndex);
        bufferSlots.push_back(newSlot);
    }

    // The streamed attributes are interleaved into one slot for each divisor.
    auto getStreamedDivisors = [&]() {
        angle::FixedVector<GLuint, gl::MAX_VERTEX_ATTRIBS> divisors;
        for (size_t attribIndex : streamedAttributes)
        {
            const GLuint divisor = bindings[attribs[attribIndex].bindingIndex].getDivisor();
            if (std::find(divisors.begin(), divisors.end(), divisor) == divisors.end())
            {
                divisors.push_back(divisor);
            }
        }
        return divisors;
    };

    // When there are more slots than the device has, stream the attributes of the buffer slots
    // that have the fewest attributes. The attributes that read zeros share one slot.
    const size_t zeroSlotCount = zeroAttributes.any() ? 1 : 0;
    angle::FixedVector<GLuint, gl::MAX_VERTEX_ATTRIBS> streamedDivisors = getStreamedDivisors();
    while (bufferSlots.size() + streamedDivisors.size() + zeroSlotCount > maxSlotCount)
    {
        ANGLE_CHECK(contextWgpu, !bufferSlots.empty(),
                    "The vertex attributes need more vertex buffers than the device has.",
                    GL_INVALID_OPERATION);
        size_t streamedSlot = 0;
        for (size_t slot = 1; slot < bufferSlots.size(); ++slot)
        {
            if (bufferSlots[slot].attributes.count() <=
                bufferSlots[streamedSlot].attributes.count())
            {
                streamedSlot = slot;
            }
        }
        streamedAttributes |= bufferSlots[streamedSlot].attributes;
        bufferSlots[streamedSlot] = bufferSlots.back();
        bufferSlots.pop_back();
        streamedDivisors = getStreamedDivisors();
    }

    gl::AttribArray<webgpu::PackedVertexAttribute> newAttribs;
    auto initAttrib = [&](size_t attribIndex, size_t slot) {
        const webgpu::Format &webgpuFormat =
            contextWgpu->getFormat(attribs[attribIndex].format->glInternalFormat);
        webgpu::PackedVertexAttribute &newAttrib = newAttribs[attribIndex];
        SetBitField(newAttrib.enabled, true);
        SetBitField(newAttrib.format, webgpuFormat.getActualWgpuVertexFormat());
        SetBitField(newAttrib.shaderLocation, attribIndex);
        SetBitField(newAttrib.bufferSlot, slot);
        return webgpuFormat.getActualBufferFormat().pixelBytes;
    };

    mUsedVertexBufferSlots.reset();
    mStreamingVertexBufferSlots.reset();
    size_t slotCount = 0;
    for (const BufferSlot &bufferSlot : bufferSlots)
    {
        const uintptr_t slotOffset = rx::roundDownPow2<uintptr_t>(bufferSlot.firstOffset, 4);
        for (size_t attribIndex : bufferSlot.attributes)
        {
            const gl::VertexAttribute &attrib = attribs[attribIndex];
            initAttrib(attribIndex, slotCount);
            SetBitField(newAttribs[attribIndex].offset,
                        gl::ComputeVertexAttributeOffset(attrib, bindings[attrib.bindingIndex]) -
                            slotOffset);
            SetBitField(newAttribs[attribIndex].stride, bufferSlot.stride);
        }
        mCurrentArrayBuffers[slotCount] = {bufferSlot.buffer, slotOffset};
        mUsedVertexBufferSlots.set(slotCount);
        slotCount++;
    }

    for (GLuint divisor : streamedDivisors)
    {
        StreamingVertexBufferSlot &streamingSlot = mStreamingSlots[slotCount];
        streamingSlot                            = {};
        streamingSlot.divisor                    = divisor;
        for (size_t attribIndex : streamedAttributes)
        {
            if (bindings[attribs[attribIndex].bindingIndex].getDivisor() != divisor)
            {
                continue;
            }
            const size_t destTypeSize = initAttrib(attribIndex, slotCount);
            SetBitField(newAttribs[attribIndex].offset, streamingSlot.stride);
            streamingSlot.stride +=
                rx::roundUpPow2(destTypeSize, webgpu::kBufferCopyToBufferAlignment);
            streamingSlot.attributes.set(attribIndex);
        }
        for (size_t attribIndex : streamingSlot.attributes)
        {
            SetBitField(newAttribs[attribIndex].stride, streamingSlot.stride);
        }
        mCurrentArrayBuffers[slotCount] = {&mStreamingArrayBuffers[slotCount], 0};
        mUsedVertexBufferSlots.set(slotCount);
        mStreamingVertexBufferSlots.set(slotCount);
        slotCount++;
    }
    if (zeroAttributes.any())
    {
        // WebGPU initializes buffers to zero. Every attribute reads the element at offset 0 for
        // every vertex.
        if (!mZeroBuffer.valid())
        {
            ANGLE_TRY(mZeroBuffer.initBuffer(
                webgpu::GetProcs(contextWgpu), contextWgpu->getDisplay()->getDevice(),
                kZeroBufferSize, WGPUBufferUsage_Vertex, webgpu::MapAtCreation::No));
        }
        for (size_t attribIndex : zeroAttributes)
        {
            const size_t destTypeSize = initAttrib(attribIndex, slotCount);
            ASSERT(destTypeSize <= kZeroBufferSize);
            ANGLE_UNUSED_VARIABLE(destTypeSize);
            SetBitField(newAttribs[attribIndex].offset, 0);
            SetBitField(newAttribs[attribIndex].stride, 0);
        }
        mCurrentArrayBuffers[slotCount] = {&mZeroBuffer, 0};
        mUsedVertexBufferSlots.set(slotCount);
        slotCount++;
    }
    ASSERT(slotCount <= maxSlotCount);

    for (size_t attribIndex = 0; attribIndex < gl::MAX_VERTEX_ATTRIBS; ++attribIndex)
    {
        mCurrentAttribs[attribIndex] = newAttribs[attribIndex];
        contextWgpu->setVertexAttribute(attribIndex, mCurrentAttribs[attribIndex]);
    }
    contextWgpu->invalidateVertexBuffers();
    return angle::Result::Continue;
}

angle::Result VertexArrayWgpu::syncClientArrays(
    const gl::Context *context,
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

    const bool hasStreamedAttributes = mStreamingVertexBufferSlots.any();

    gl::DrawElementsType destDrawElementsTypeOrInvalid = sourceDrawElementsTypeOrInvalid;

    IndexDataNeedsStreaming indexDataNeedsStreaming = determineIndexDataNeedsStreaming(
        sourceDrawElementsTypeOrInvalid, count, mode, &destDrawElementsTypeOrInvalid);

    // An earlier draw may have streamed the indices of the element array buffer. Draw from the
    // element array buffer again when this draw does not stream them.
    if (indexDataNeedsStreaming == IndexDataNeedsStreaming::None && getElementArrayBuffer())
    {
        webgpu::BufferHelper *elementArrayBuffer =
            &GetImplAs<BufferWgpu>(getElementArrayBuffer())->getBuffer();
        if (mCurrentIndexBuffer != elementArrayBuffer)
        {
            mCurrentIndexBuffer = elementArrayBuffer;
            webgpu::GetImpl(context)->invalidateIndexBuffer();
        }
    }

    if (!hasStreamedAttributes && indexDataNeedsStreaming == IndexDataNeedsStreaming::None)
    {
        return angle::Result::Continue;
    }

    gl::Buffer *elementArrayBuffer = getElementArrayBuffer();
    ContextWgpu *contextWgpu       = webgpu::GetImpl(context);
    webgpu::DeviceHandle device    = webgpu::GetDevice(context);
    const uint8_t *srcIndexData    = static_cast<const uint8_t *>(indices);
    // Only line loops with primitive restart and triangle fans read the indices on the CPU here.
    // The element array buffer has no MapRead usage, so read the indices back through a staging
    // buffer.
    const bool readsIndicesOnCpu =
        sourceDrawElementsTypeOrInvalid != gl::DrawElementsType::InvalidEnum &&
        ((mode == gl::PrimitiveMode::LineLoop && primitiveRestartEnabled) ||
         mode == gl::PrimitiveMode::TriangleFan);
    webgpu::BufferReadback indexReadback;
    if (elementArrayBuffer && readsIndicesOnCpu)
    {
        webgpu::BufferHelper &srcBuffer = GetImplAs<BufferWgpu>(elementArrayBuffer)->getBuffer();
        ANGLE_TRY(srcBuffer.readDataImmediate(
            contextWgpu, reinterpret_cast<uintptr_t>(indices),
            static_cast<size_t>(count) *
                gl::GetDrawElementsTypeSize(sourceDrawElementsTypeOrInvalid),
            webgpu::RenderPassClosureReason::IndexRangeReadback, &indexReadback));
        srcIndexData = indexReadback.data;
    }
    const uint64_t adjustedCount64 = calculateAdjustedIndexCount(
        mode, primitiveRestartEnabled, sourceDrawElementsTypeOrInvalid, count, srcIndexData);
    // The emulated primitives of a triangle fan can have more indices than 32-bit indices can
    // index.
    ANGLE_CHECK(contextWgpu, adjustedCount64 <= std::numeric_limits<uint32_t>::max(),
                "The emulated primitives need more indices than 32-bit indices can index.",
                GL_OUT_OF_MEMORY);
    const uint32_t adjustedCount = static_cast<uint32_t>(adjustedCount64);

    if (indexCountOut)
    {
        *indexCountOut = adjustedCount;
    }
    // An adjusted count of 0 means that the emulated primitives have no indices, for example a
    // line loop of only primitive restart indices. The draw is a no-op, so there is nothing to
    // stream.
    if (adjustedCount == 0)
    {
        return angle::Result::Continue;
    }

    // If any attributes need to be streamed, we need to know the index range. We also need to know
    // the index range if there is a draw arrays call and we have to stream the index data for it.
    std::optional<gl::IndexRange> indexRange;
    if (hasStreamedAttributes)
    {
        GLint startVertex  = 0;
        size_t vertexCount = 0;
        ANGLE_TRY(GetVertexRangeInfo(context, first, count, sourceDrawElementsTypeOrInvalid,
                                     indices, baseVertex, &startVertex, &vertexCount));
        indexRange =
            gl::IndexRange(startVertex, static_cast<GLuint>(startVertex + vertexCount - 1));
    }
    else if (indexDataNeedsStreaming != IndexDataNeedsStreaming::None &&
             sourceDrawElementsTypeOrInvalid == gl::DrawElementsType::InvalidEnum)
    {
        // first + count - 1 can overflow GLint, but not uint32_t, as first and count are not
        // negative.
        const uint32_t firstVertex = static_cast<uint32_t>(first);
        indexRange = gl::IndexRange(firstVertex, firstVertex + static_cast<uint32_t>(count) - 1);
    }

    // Pre-compute the total size of all streamed vertex and index data so a single staging buffer
    // can be used
    size_t stagingBufferSize = 0;

    // A line loop with indices in an element array buffer copies them on the GPU when the copies
    // are 4-byte aligned, as copyBufferToBuffer() requires.
    const size_t destIndexUnitSize =
        indexDataNeedsStreaming != IndexDataNeedsStreaming::None
            ? static_cast<size_t>(gl::GetDrawElementsTypeSize(destDrawElementsTypeOrInvalid))
            : 0;
    const bool copyLineLoopIndicesOnGpu =
        indexDataNeedsStreaming == IndexDataNeedsStreaming::LineLoopStreaming &&
        !primitiveRestartEnabled && elementArrayBuffer &&
        sourceDrawElementsTypeOrInvalid == destDrawElementsTypeOrInvalid &&
        reinterpret_cast<uintptr_t>(indices) % webgpu::kBufferCopyToBufferAlignment == 0 &&
        (destIndexUnitSize * count) % webgpu::kBufferCopyToBufferAlignment == 0;

    ANGLE_TRY(calculateStagingBufferSize(contextWgpu, copyLineLoopIndicesOnGpu,
                                         primitiveRestartEnabled, indexDataNeedsStreaming,
                                         destDrawElementsTypeOrInvalid, adjustedCount,
                                         instanceCount, indexRange, &stagingBufferSize));

    ASSERT(stagingBufferSize % webgpu::kBufferSizeAlignment == 0);
    webgpu::BufferHelper stagingBuffer;
    uint8_t *stagingData              = nullptr;
    size_t currentStagingDataPosition = 0;
    if (stagingBufferSize > 0)
    {
        ASSERT(stagingBufferSize % webgpu::kBufferSizeAlignment == 0);
        ANGLE_TRY(stagingBuffer.initBuffer(wgpu, device, stagingBufferSize,
                                           WGPUBufferUsage_CopySrc | WGPUBufferUsage_MapWrite,
                                           webgpu::MapAtCreation::Yes));
        stagingData = stagingBuffer.getMapWritePointer(0, stagingBufferSize);
    }

    std::vector<BufferCopy> stagingUploads;

    if (indexDataNeedsStreaming != IndexDataNeedsStreaming::None)
    {
        // Indices are streamed to the start of the buffer. Tell the draw call command to use 0 for
        // firstIndex.
        *adjustedIndicesPtr = 0;
        size_t destIndexDataSize = destIndexUnitSize * adjustedCount;
        size_t destIndexBufferSize =
            rx::roundUpPow2(destIndexDataSize, webgpu::kBufferCopyToBufferAlignment);
        ANGLE_TRY(ensureBufferCreated(context, mStreamingIndexBuffer, destIndexBufferSize, 0,
                                      WGPUBufferUsage_CopyDst | WGPUBufferUsage_Index,
                                      BufferType::IndexBuffer));
        switch (indexDataNeedsStreaming)
        {
            case IndexDataNeedsStreaming::LineLoopStreaming:
                ANGLE_TRY(streamIndicesLineLoop(
                    contextWgpu, sourceDrawElementsTypeOrInvalid, destDrawElementsTypeOrInvalid,
                    count, adjustedCount, indices, primitiveRestartEnabled,
                    copyLineLoopIndicesOnGpu, stagingData, destIndexBufferSize, indexRange,
                    srcIndexData, &stagingBuffer, &stagingUploads, &currentStagingDataPosition));
                break;
            case IndexDataNeedsStreaming::TriangleFanStreaming:
                streamIndicesTriangleFan(
                    sourceDrawElementsTypeOrInvalid, destDrawElementsTypeOrInvalid, count,
                    adjustedCount, primitiveRestartEnabled, stagingData, destIndexBufferSize,
                    srcIndexData, &stagingBuffer, &stagingUploads, &currentStagingDataPosition);
                break;
            case IndexDataNeedsStreaming::NonEmulatedStreaming:
                ANGLE_TRY(streamIndicesDefault(contextWgpu, sourceDrawElementsTypeOrInvalid,
                                               destDrawElementsTypeOrInvalid, mode, count, indices,
                                               stagingData, destIndexBufferSize, &stagingBuffer,
                                               &stagingUploads, &currentStagingDataPosition));
                break;
            default:
                NOTREACHED();
                break;
        }
    }

    const std::vector<gl::VertexAttribute> &attribs = mState.getVertexAttributes();
    const std::vector<gl::VertexBinding> &bindings  = mState.getVertexBindings();
    for (size_t slot : mStreamingVertexBufferSlots)
    {
        const StreamingVertexBufferSlot &streamingSlot = mStreamingSlots[slot];

        size_t streamedVertexCount = gl::ComputeVertexBindingElementCount(
            streamingSlot.divisor, indexRange->vertexCount(), std::max(instanceCount, 1), 0);

        // Vertices do not apply the 'start' offset when the divisor is non-zero even when doing
        // a non-instanced draw call
        const size_t firstIndex = (streamingSlot.divisor == 0) ? indexRange->start() : 0;

        // The attributes of the slot are interleaved.
        const size_t destStride = streamingSlot.stride;
        ASSERT(stagingData != nullptr);
        uint8_t *slotStagingData = ANGLE_UNSAFE_TODO(stagingData + currentStagingDataPosition);

        for (size_t attribIndex : streamingSlot.attributes)
        {
            const gl::VertexAttribute &attrib = attribs[attribIndex];
            const gl::VertexBinding &binding  = bindings[attrib.bindingIndex];
            const gl::Buffer *buffer          = getVertexArrayBuffer(attrib.bindingIndex);

            const size_t sourceStride   = ComputeVertexAttributeStride(attrib, binding);
            const size_t sourceTypeSize = gl::ComputeVertexAttributeTypeSize(attrib);

            // Attributes using client memory ignore the VERTEX_ATTRIB_BINDING state.
            // https://www.opengl.org/registry/specs/ARB/vertex_attrib_binding.txt
            const uint8_t *inputPointer = static_cast<const uint8_t *>(attrib.pointer);

            webgpu::BufferReadback readbackBuffer;
            if (buffer)
            {
                webgpu::BufferHelper &srcBuffer = webgpu::GetImpl(buffer)->getBuffer();
                const uintptr_t sourceOffset    = gl::ComputeVertexAttributeOffset(attrib, binding);
                size_t sourceVertexDataSize =
                    sourceStride * (firstIndex + streamedVertexCount - 1) + sourceTypeSize;

                ANGLE_TRY(srcBuffer.readDataImmediate(
                    contextWgpu, 0, sourceOffset + sourceVertexDataSize,
                    webgpu::RenderPassClosureReason::IndexRangeReadback, &readbackBuffer));
                inputPointer = ANGLE_UNSAFE_TODO(readbackBuffer.data + sourceOffset);
            }

            const webgpu::Format &vertexFormat =
                contextWgpu->getFormat(attrib.format->glInternalFormat);
            VertexCopyFunction copyFunction = vertexFormat.getVertexLoadFunction();
            ASSERT(copyFunction != nullptr);
            const uint8_t *sourceData = ANGLE_UNSAFE_TODO(inputPointer + sourceStride * firstIndex);
            uint8_t *destData =
                ANGLE_UNSAFE_TODO(slotStagingData + mCurrentAttribs[attribIndex].offset);
            if (vertexFormat.getActualBufferFormat().pixelBytes == destStride)
            {
                // The attribute is the only one in the slot, so its data is tightly packed.
                copyFunction(sourceData, sourceStride, streamedVertexCount, destData);
            }
            else
            {
                for (size_t vertex = 0; vertex < streamedVertexCount; ++vertex)
                {
                    copyFunction(ANGLE_UNSAFE_TODO(sourceData + sourceStride * vertex),
                                 sourceStride, 1,
                                 ANGLE_UNSAFE_TODO(destData + destStride * vertex));
                }
            }
        }

        size_t copySize =
            rx::roundUpPow2(streamedVertexCount * destStride, webgpu::kBufferCopyToBufferAlignment);
        // Pad the streaming buffer with empty data at the beginning to put the vertex data at the
        // same index location.
        size_t destCopyOffset = firstIndex * destStride;

        ANGLE_TRY(ensureBufferCreated(
            context, mStreamingArrayBuffers[slot], destCopyOffset + copySize, slot,
            WGPUBufferUsage_CopyDst | WGPUBufferUsage_Vertex, BufferType::ArrayBuffer));

        stagingUploads.push_back({currentStagingDataPosition, &stagingBuffer,
                                  &mStreamingArrayBuffers[slot], destCopyOffset, copySize});

        currentStagingDataPosition += copySize;
    }

    if (stagingBuffer.valid())
    {
        ANGLE_TRY(stagingBuffer.unmap());
    }
    webgpu::CommandEncoderHandle commandEncoder;
    ANGLE_TRY(contextWgpu->getCurrentCommandEncoder(
        webgpu::RenderPassClosureReason::VertexArrayStreaming, &commandEncoder));

    for (const BufferCopy &copy : stagingUploads)
    {
        wgpu->commandEncoderCopyBufferToBuffer(commandEncoder.get(), copy.src->getBuffer().get(),
                                               copy.sourceOffset, copy.dest->getBuffer().get(),
                                               copy.destOffset, copy.size);
    }

    return angle::Result::Continue;
}

void VertexArrayWgpu::syncDirtyAttrib(ContextWgpu *contextWgpu,
                                      const gl::VertexAttribute &attrib,
                                      const gl::VertexBinding &binding,
                                      size_t attribIndex)
{
    gl::Buffer *bufferGl = getVertexArrayBuffer(attrib.bindingIndex);
    mForcedStreamingAttributes[attribIndex] =
        attrib.enabled && AttributeNeedsStreaming(contextWgpu, attrib, binding, bufferGl);
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
                                                   size_t vertexBufferSlot,
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
            contextWgpu->invalidateVertexBuffer(vertexBufferSlot);
        }
    }

    if (bufferType == BufferType::IndexBuffer && mCurrentIndexBuffer != &buffer)
    {
        mCurrentIndexBuffer = &buffer;
        contextWgpu->invalidateIndexBuffer();
    }
    return angle::Result::Continue;
}

IndexDataNeedsStreaming VertexArrayWgpu::determineIndexDataNeedsStreaming(
    gl::DrawElementsType sourceDrawElementsTypeOrInvalid,
    GLsizei count,
    gl::PrimitiveMode mode,
    gl::DrawElementsType *destDrawElementsTypeOrInvalidOut)
{
    if (mode == gl::PrimitiveMode::LineLoop)
    {
        // Promote 8-bit indices to 16-bit indices
        *destDrawElementsTypeOrInvalidOut =
            calculateDrawElementsType(sourceDrawElementsTypeOrInvalid, count);
        return IndexDataNeedsStreaming::LineLoopStreaming;
    }
    else if (mode == gl::PrimitiveMode::TriangleFan)
    {
        *destDrawElementsTypeOrInvalidOut =
            calculateDrawElementsType(sourceDrawElementsTypeOrInvalid, count);
        return IndexDataNeedsStreaming::TriangleFanStreaming;
    }
    else if (sourceDrawElementsTypeOrInvalid != gl::DrawElementsType::InvalidEnum &&
             (!getElementArrayBuffer() ||
              sourceDrawElementsTypeOrInvalid == gl::DrawElementsType::UnsignedByte))
    {
        // Index data needs to be uploaded to the GPU, or converted, as WebGPU has no 8-bit indices
        *destDrawElementsTypeOrInvalidOut =
            calculateDrawElementsType(sourceDrawElementsTypeOrInvalid, count);
        return IndexDataNeedsStreaming::NonEmulatedStreaming;
    }
    return IndexDataNeedsStreaming::None;
}

uint64_t VertexArrayWgpu::calculateAdjustedIndexCount(
    gl::PrimitiveMode mode,
    bool primitiveRestartEnabled,
    gl::DrawElementsType sourceDrawElementsTypeOrInvalid,
    GLsizei count,
    const uint8_t *srcIndexData)
{
    if (mode == gl::PrimitiveMode::LineLoop)
    {
        if (primitiveRestartEnabled)
        {
            return GetLineLoopWithRestartIndexCount(sourceDrawElementsTypeOrInvalid, count,
                                                    srcIndexData);
        }
        return static_cast<uint64_t>(count) + 1;
    }
    else if (mode == gl::PrimitiveMode::TriangleFan)
    {
        if (primitiveRestartEnabled)
        {
            return GetTriangleFanWithRestartIndexCount(sourceDrawElementsTypeOrInvalid, count,
                                                       srcIndexData);
        }
        return GetTriangleFanIndexCount(count);
    }
    return count;
}

angle::Result VertexArrayWgpu::calculateStagingBufferSize(
    ContextWgpu *contextWgpu,
    bool copyLineLoopIndicesOnGpu,
    bool primitiveRestartEnabled,
    IndexDataNeedsStreaming indexDataNeedsStreaming,
    gl::DrawElementsType destDrawElementsTypeOrInvalid,
    size_t adjustedCountForStreamedIndices,
    GLsizei instanceCount,
    std::optional<gl::IndexRange> indexRange,
    size_t *stagingBufferSizeOut)
{
    const uint64_t maxBufferSize = contextWgpu->getDisplay()->getLimitsWgpu().maxBufferSize;
    if (indexDataNeedsStreaming != IndexDataNeedsStreaming::None)
    {
        size_t destIndexUnitSize =
            static_cast<size_t>(gl::GetDrawElementsTypeSize(destDrawElementsTypeOrInvalid));
        size_t destIndexBufferSize =
            rx::roundUpPow2(destIndexUnitSize * adjustedCountForStreamedIndices,
                            webgpu::kBufferCopyToBufferAlignment);
        // The streaming index buffer is as large.
        ANGLE_CHECK(contextWgpu, destIndexBufferSize <= maxBufferSize,
                    "The streamed indices are larger than the maximum buffer size.",
                    GL_OUT_OF_MEMORY);
        // Allocating staging buffer space for indices is only needed when the indices are not
        // copied from the element array buffer on the GPU.
        if (!copyLineLoopIndicesOnGpu)
        {
            *stagingBufferSizeOut += destIndexBufferSize;
        }
    }

    for (size_t slot : mStreamingVertexBufferSlots)
    {
        const StreamingVertexBufferSlot &streamingSlot = mStreamingSlots[slot];
        ASSERT(streamingSlot.stride > 0);

        size_t elementCount = gl::ComputeVertexBindingElementCount(
            streamingSlot.divisor, indexRange->vertexCount(), std::max(instanceCount, 1), 0);

        size_t slotSize = streamingSlot.stride * elementCount;
        *stagingBufferSizeOut += rx::roundUpPow2(slotSize, webgpu::kBufferCopyToBufferAlignment);
    }

    // ANGLE_CHECK() evaluates the message only when the check fails.
    ANGLE_CHECK(contextWgpu, *stagingBufferSizeOut <= maxBufferSize,
                GetStagingBufferTooLargeMessage(*stagingBufferSizeOut, maxBufferSize).c_str(),
                GL_OUT_OF_MEMORY);
    return angle::Result::Continue;
}

gl::DrawElementsType VertexArrayWgpu::calculateDrawElementsType(
    gl::DrawElementsType sourceDrawElementsTypeOrInvalid,
    GLsizei count)
{
    if (sourceDrawElementsTypeOrInvalid == gl::DrawElementsType::UnsignedByte)
    {
        // Promote 8-bit indices to 16-bit indices
        return gl::DrawElementsType::UnsignedShort;
    }
    // Index data will always need streaming for line loop mode regardless of what type of draw
    // call it is.
    else if (sourceDrawElementsTypeOrInvalid == gl::DrawElementsType::InvalidEnum)
    {
        // Line loop draw array calls are emulated via indexed draw calls, so an index type must
        // be set.
        if (count >= std::numeric_limits<unsigned short>::max())
        {
            return gl::DrawElementsType::UnsignedInt;
        }
        return gl::DrawElementsType::UnsignedShort;
    }
    return sourceDrawElementsTypeOrInvalid;
}

angle::Result VertexArrayWgpu::streamIndicesLineLoop(
    ContextWgpu *contextWgpu,
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
    size_t *currentStagingDataPositionOut)
{
    size_t destIndexUnitSize =
        static_cast<size_t>(gl::GetDrawElementsTypeSize(destDrawElementsTypeOrInvalid));
    gl::Buffer *elementArrayBuffer = getElementArrayBuffer();
    // TODO(anglebug.com/401226623): Don't use the staging buffer when the adjustedCount for
    // primitive restarts is count + 1.
    if (primitiveRestartEnabled)
    {
        rx::StreamEmulatedLineLoopIndices(sourceDrawElementsTypeOrInvalid, count, srcIndexData,
                                          stagingData,
                                          /* shouldConvertUint8= */ true);
        stagingUploadsOut->push_back({*currentStagingDataPositionOut, stagingBufferPtr,
                                      &mStreamingIndexBuffer, 0, destIndexBufferSize});
        *currentStagingDataPositionOut += destIndexBufferSize;
    }
    else if (copyLineLoopIndicesOnGpu)
    {
        // Use the element array buffer as the source for the new streaming index buffer. This
        // condition is only hit when an indexed draw call has an element array buffer and is
        // trying to draw line loops.

        // When using an element array buffer, 'indices' is an offset to the first element.
        size_t sourceOffset                = reinterpret_cast<size_t>(indices);
        BufferWgpu *elementArrayBufferWgpu = GetImplAs<BufferWgpu>(elementArrayBuffer);
        webgpu::BufferHelper *sourceBuffer = &elementArrayBufferWgpu->getBuffer();
        // A buffer can stay mapped after its data was written. A command buffer that uses a
        // buffer that is still mapped fails to submit, which loses all the work recorded in it.
        if (sourceBuffer->getMappedState())
        {
            ANGLE_TRY(sourceBuffer->unmap());
        }

        // The caller checked that the copies are 4-byte aligned.
        size_t copySize = destIndexUnitSize * count;
        ASSERT(sourceOffset % webgpu::kBufferCopyToBufferAlignment == 0);
        ASSERT(copySize % webgpu::kBufferCopyToBufferAlignment == 0);
        stagingUploadsOut->push_back(
            {sourceOffset, sourceBuffer, &mStreamingIndexBuffer, 0, copySize});

        // Emulate line loops with an additional copy of the first index at the end of the
        // buffer. The copy is 4 bytes, which for 16-bit indices also copies the second index
        // after the end of the line loop.
        size_t lastOffset = copySize;
        stagingUploadsOut->push_back(
            {sourceOffset, sourceBuffer, &mStreamingIndexBuffer, lastOffset,
             rx::roundUpPow2(destIndexUnitSize, webgpu::kBufferCopyToBufferAlignment)});
    }
    // Handle emulating line loop for draw arrays calls.
    else if (sourceDrawElementsTypeOrInvalid == gl::DrawElementsType::InvalidEnum)
    {
        ASSERT(destDrawElementsTypeOrInvalid != gl::DrawElementsType::InvalidEnum);
        uint32_t clampedVertexCount = gl::clampCast<uint32_t>(indexRange->vertexCount());
        uint32_t startVertex        = static_cast<uint32_t>(indexRange->start());
        size_t index                = *currentStagingDataPositionOut;
        for (uint32_t i = 0; i < clampedVertexCount; i++)
        {
            uint32_t copyData = startVertex + i;
            ANGLE_UNSAFE_TODO(memcpy(stagingData + index, &copyData, destIndexUnitSize));
            index += destIndexUnitSize;
        }
        ANGLE_UNSAFE_TODO(
            memcpy(stagingData + *currentStagingDataPositionOut + destIndexUnitSize * count,
                   &startVertex, destIndexUnitSize));

        size_t copySize = destIndexBufferSize;
        stagingUploadsOut->push_back({*currentStagingDataPositionOut, stagingBufferPtr,
                                      &mStreamingIndexBuffer, 0, copySize});
        *currentStagingDataPositionOut += copySize;
    }
    else
    {
        ANGLE_TRY(streamIndicesDefault(
            contextWgpu, sourceDrawElementsTypeOrInvalid, destDrawElementsTypeOrInvalid,
            gl::PrimitiveMode::LineLoop, count, indices, stagingData, destIndexBufferSize,
            stagingBufferPtr, stagingUploadsOut, currentStagingDataPositionOut));
    }
    return angle::Result::Continue;
}

void VertexArrayWgpu::streamIndicesTriangleFan(gl::DrawElementsType sourceDrawElementsTypeOrInvalid,
                                               gl::DrawElementsType destDrawElementsTypeOrInvalid,
                                               GLsizei count,
                                               uint32_t adjustedCount,
                                               bool primitiveRestartEnabled,
                                               uint8_t *stagingData,
                                               size_t destIndexBufferSize,
                                               const uint8_t *srcIndexData,
                                               webgpu::BufferHelper *stagingBufferPtr,
                                               std::vector<BufferCopy> *stagingUploadsOut,
                                               size_t *currentStagingDataPositionOut)
{
    ASSERT(stagingData != nullptr);
    uint8_t *destIndexData = ANGLE_UNSAFE_TODO(stagingData + *currentStagingDataPositionOut);
    size_t writtenCount    = 0;
    if (sourceDrawElementsTypeOrInvalid == gl::DrawElementsType::InvalidEnum)
    {
        // glDrawArrays: the indices are relative to the first vertex, so they do not depend on it
        // and do not overflow the index type, which is chosen for the vertex count. The triangles
        // are in the order of rx::CopyTriangleFanIndices().
        const size_t destIndexUnitSize =
            static_cast<size_t>(gl::GetDrawElementsTypeSize(destDrawElementsTypeOrInvalid));
        for (uint32_t vertex = 2; vertex < static_cast<uint32_t>(count); vertex++)
        {
            const uint32_t triangle[] = {vertex, 0, vertex - 1};
            for (uint32_t index : triangle)
            {
                ANGLE_UNSAFE_TODO(memcpy(destIndexData, &index, destIndexUnitSize));
                destIndexData = ANGLE_UNSAFE_TODO(destIndexData + destIndexUnitSize);
                writtenCount++;
            }
        }
    }
    else
    {
        writtenCount = rx::StreamEmulatedTriangleFanIndices(sourceDrawElementsTypeOrInvalid, count,
                                                            srcIndexData, destIndexData,
                                                            primitiveRestartEnabled);
    }
    // The staging buffer has space for adjustedCount indices.
    ASSERT(writtenCount == adjustedCount);

    stagingUploadsOut->push_back({*currentStagingDataPositionOut, stagingBufferPtr,
                                  &mStreamingIndexBuffer, 0, destIndexBufferSize});
    *currentStagingDataPositionOut += destIndexBufferSize;
}

angle::Result VertexArrayWgpu::streamIndicesDefault(
    ContextWgpu *contextWgpu,
    gl::DrawElementsType sourceDrawElementsTypeOrInvalid,
    gl::DrawElementsType destDrawElementsTypeOrInvalid,
    gl::PrimitiveMode mode,
    GLsizei count,
    const void *indices,
    uint8_t *stagingData,
    size_t destIndexBufferSize,
    webgpu::BufferHelper *stagingBufferPtr,
    std::vector<BufferCopy> *stagingUploadsOut,
    size_t *currentStagingDataPositionOut)
{
    size_t destIndexUnitSize =
        static_cast<size_t>(gl::GetDrawElementsTypeSize(destDrawElementsTypeOrInvalid));

    webgpu::BufferReadback readbackBuffer;
    const uint8_t *srcIndexData = static_cast<const uint8_t *>(indices);
    if (getElementArrayBuffer())
    {
        webgpu::BufferHelper &srcBuffer = webgpu::GetImpl(getElementArrayBuffer())->getBuffer();

        const GLuint srcIndexTypeSize =
            gl::GetDrawElementsTypeSize(sourceDrawElementsTypeOrInvalid);
        const size_t srcIndexOffset = reinterpret_cast<uintptr_t>(indices);

        ANGLE_TRY(srcBuffer.readDataImmediate(contextWgpu, srcIndexOffset, count * srcIndexTypeSize,
                                              webgpu::RenderPassClosureReason::IndexRangeReadback,
                                              &readbackBuffer));
        srcIndexData = readbackBuffer.data;
    }

    CopyIndexFunction indexCopyFunction =
        GetCopyIndexFunction(sourceDrawElementsTypeOrInvalid, destDrawElementsTypeOrInvalid);
    ASSERT(stagingData != nullptr);
    indexCopyFunction(srcIndexData, count,
                      ANGLE_UNSAFE_TODO(stagingData + *currentStagingDataPositionOut));

    if (mode == gl::PrimitiveMode::LineLoop)
    {
        // Close the loop with a copy of the first index.
        indexCopyFunction(srcIndexData, 1,
                          ANGLE_UNSAFE_TODO(stagingData + *currentStagingDataPositionOut +
                                            (destIndexUnitSize * count)));
    }

    size_t copySize = destIndexBufferSize;
    stagingUploadsOut->push_back(
        {*currentStagingDataPositionOut, stagingBufferPtr, &mStreamingIndexBuffer, 0, copySize});
    *currentStagingDataPositionOut += copySize;
    return angle::Result::Continue;
}

}  // namespace rx
