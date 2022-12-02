//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VertexArrayVk.cpp:
//    Implements the class methods for VertexArrayVk.
//

#include "libANGLE/renderer/vulkan/VertexArrayVk.h"

#include "common/debug.h"
#include "common/utilities.h"
#include "libANGLE/Context.h"
#include "libANGLE/renderer/vulkan/BufferVk.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/FramebufferVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/ResourceVk.h"
#include "libANGLE/renderer/vulkan/vk_format_utils.h"

namespace rx
{
namespace
{
constexpr int kStreamIndexBufferCachedIndexCount = 6;
constexpr int kMaxCachedStreamIndexBuffers       = 4;
constexpr size_t kDefaultValueSize               = sizeof(gl::VertexAttribCurrentValueData::Values);

ANGLE_INLINE bool BindingIsAligned(const gl::VertexBinding &binding,
                                   const angle::Format &angleFormat,
                                   unsigned int attribSize,
                                   GLuint relativeOffset)
{
    GLintptr totalOffset = binding.getOffset() + relativeOffset;
    GLuint mask          = angleFormat.componentAlignmentMask;
    if (mask != std::numeric_limits<GLuint>::max())
    {
        return ((totalOffset & mask) == 0 && (binding.getStride() & mask) == 0);
    }
    else
    {
        // To perform the GPU conversion for formats with components that aren't byte-aligned
        // (for example, A2BGR10 or RGB10A2), one element has to be placed in 4 bytes to perform
        // the compute shader. So, binding offset and stride has to be aligned to formatSize.
        unsigned int formatSize = angleFormat.pixelBytes;
        return (totalOffset % formatSize == 0) && (binding.getStride() % formatSize == 0);
    }
}

void WarnOnVertexFormatConversion(ContextVk *contextVk,
                                  const vk::Format &vertexFormat,
                                  bool compressed,
                                  bool insertEventMarker)
{
    if (!vertexFormat.getVertexLoadRequiresConversion(compressed))
    {
        return;
    }

    ANGLE_VK_PERF_WARNING(
        contextVk, GL_DEBUG_SEVERITY_LOW,
        "The Vulkan driver does not support vertex attribute format 0x%04X, emulating with 0x%04X",
        vertexFormat.getIntendedFormat().glInternalFormat,
        vertexFormat.getActualBufferFormat(compressed).glInternalFormat);
}

angle::Result StreamVertexData(ContextVk *contextVk,
                               vk::BufferHelper *dstBufferHelper,
                               const uint8_t *srcData,
                               size_t bytesToAllocate,
                               size_t dstOffset,
                               size_t vertexCount,
                               size_t srcStride,
                               VertexCopyFunction vertexLoadFunction)
{
    RendererVk *renderer = contextVk->getRenderer();

    uint8_t *dst = dstBufferHelper->getMappedMemory() + dstOffset;

    vertexLoadFunction(srcData, srcStride, vertexCount, dst);

    ANGLE_TRY(dstBufferHelper->flush(renderer));

    return angle::Result::Continue;
}

angle::Result StreamVertexDataWithDivisor(ContextVk *contextVk,
                                          vk::BufferHelper *dstBufferHelper,
                                          const uint8_t *srcData,
                                          size_t bytesToAllocate,
                                          size_t srcStride,
                                          size_t dstStride,
                                          VertexCopyFunction vertexLoadFunction,
                                          uint32_t divisor,
                                          size_t numSrcVertices)
{
    RendererVk *renderer = contextVk->getRenderer();

    uint8_t *dst = dstBufferHelper->getMappedMemory();

    // Each source vertex is used `divisor` times before advancing. Clamp to avoid OOB reads.
    size_t clampedSize = std::min(numSrcVertices * dstStride * divisor, bytesToAllocate);

    ASSERT(clampedSize % dstStride == 0);
    ASSERT(divisor > 0);

    uint32_t srcVertexUseCount = 0;
    for (size_t dataCopied = 0; dataCopied < clampedSize; dataCopied += dstStride)
    {
        vertexLoadFunction(srcData, srcStride, 1, dst);
        srcVertexUseCount++;
        if (srcVertexUseCount == divisor)
        {
            srcData += srcStride;
            srcVertexUseCount = 0;
        }
        dst += dstStride;
    }

    // Satisfy robustness constraints (only if extension enabled)
    if (contextVk->getExtensions().robustnessEXT)
    {
        if (clampedSize < bytesToAllocate)
        {
            memset(dst, 0, bytesToAllocate - clampedSize);
        }
    }

    ANGLE_TRY(dstBufferHelper->flush(renderer));

    return angle::Result::Continue;
}

size_t GetVertexCount(BufferVk *srcBuffer, const gl::VertexBinding &binding, uint32_t srcFormatSize)
{
    // Bytes usable for vertex data.
    GLint64 bytes = srcBuffer->getSize() - binding.getOffset();
    if (bytes < srcFormatSize)
        return 0;

    // Count the last vertex.  It may occupy less than a full stride.
    // This is also correct if stride happens to be less than srcFormatSize.
    size_t numVertices = 1;
    bytes -= srcFormatSize;

    // Count how many strides fit remaining space.
    if (bytes > 0)
        numVertices += static_cast<size_t>(bytes) / binding.getStride();

    return numVertices;
}
}  // anonymous namespace

VertexArrayVk::VertexArrayVk(ContextVk *contextVk, const gl::VertexArrayState &state)
    : VertexArrayImpl(state),
      mCurrentArrayBufferHandles{},
      mCurrentArrayBufferOffsets{},
      mCurrentArrayBufferRelativeOffsets{},
      mCurrentArrayBuffers{},
      mCurrentArrayBufferStrides{},
      mCurrentElementArrayBuffer(nullptr),
      mLineLoopHelper(contextVk->getRenderer()),
      mDirtyLineLoopTranslation(true)
{
    vk::BufferHelper &emptyBuffer = contextVk->getEmptyBuffer();

    mCurrentArrayBufferHandles.fill(emptyBuffer.getBuffer().getHandle());
    mCurrentArrayBufferOffsets.fill(0);
    mCurrentArrayBufferRelativeOffsets.fill(0);
    mCurrentArrayBuffers.fill(&emptyBuffer);
    mCurrentArrayBufferStrides.fill(0);
}

VertexArrayVk::~VertexArrayVk() {}

void VertexArrayVk::destroy(const gl::Context *context)
{
    ContextVk *contextVk = vk::GetImpl(context);

    RendererVk *renderer = contextVk->getRenderer();

    for (std::unique_ptr<vk::BufferHelper> &buffer : mCachedStreamIndexBuffers)
    {
        buffer->release(renderer);
    }

    mStreamedIndexData.release(renderer);
    mTranslatedByteIndexData.release(renderer);
    mTranslatedByteIndirectData.release(renderer);
    mLineLoopHelper.release(contextVk);
}

angle::Result VertexArrayVk::convertIndexBufferGPU(ContextVk *contextVk,
                                                   BufferVk *bufferVk,
                                                   const void *indices)
{
    intptr_t offsetIntoSrcData = reinterpret_cast<intptr_t>(indices);
    size_t srcDataSize         = static_cast<size_t>(bufferVk->getSize()) - offsetIntoSrcData;

    // Allocate buffer for results
    ANGLE_TRY(mTranslatedByteIndexData.allocateForVertexConversion(
        contextVk, sizeof(GLushort) * srcDataSize, vk::MemoryHostVisibility::NonVisible));
    mCurrentElementArrayBuffer = &mTranslatedByteIndexData;

    vk::BufferHelper *dst = &mTranslatedByteIndexData;
    vk::BufferHelper *src = &bufferVk->getBuffer();

    // Copy relevant section of the source into destination at allocated offset.  Note that the
    // offset returned by allocate() above is in bytes. As is the indices offset pointer.
    UtilsVk::ConvertIndexParameters params = {};
    params.srcOffset                       = static_cast<uint32_t>(offsetIntoSrcData);
    params.dstOffset                       = 0;
    params.maxIndex                        = static_cast<uint32_t>(bufferVk->getSize());

    return contextVk->getUtils().convertIndexBuffer(contextVk, dst, src, params);
}

angle::Result VertexArrayVk::convertIndexBufferIndirectGPU(ContextVk *contextVk,
                                                           vk::BufferHelper *srcIndirectBuf,
                                                           VkDeviceSize srcIndirectBufOffset,
                                                           vk::BufferHelper **indirectBufferVkOut)
{
    size_t srcDataSize = static_cast<size_t>(mCurrentElementArrayBuffer->getSize());
    ASSERT(mCurrentElementArrayBuffer ==
           &vk::GetImpl(getState().getElementArrayBuffer())->getBuffer());

    vk::BufferHelper *srcIndexBuf = mCurrentElementArrayBuffer;

    // Allocate buffer for results
    ANGLE_TRY(mTranslatedByteIndexData.allocateForVertexConversion(
        contextVk, sizeof(GLushort) * srcDataSize, vk::MemoryHostVisibility::NonVisible));
    vk::BufferHelper *dstIndexBuf = &mTranslatedByteIndexData;

    ANGLE_TRY(mTranslatedByteIndirectData.allocateForVertexConversion(
        contextVk, sizeof(VkDrawIndexedIndirectCommand), vk::MemoryHostVisibility::NonVisible));
    vk::BufferHelper *dstIndirectBuf = &mTranslatedByteIndirectData;

    // Save new element array buffer
    mCurrentElementArrayBuffer = dstIndexBuf;
    // Tell caller what new indirect buffer is
    *indirectBufferVkOut = dstIndirectBuf;

    // Copy relevant section of the source into destination at allocated offset.  Note that the
    // offset returned by allocate() above is in bytes. As is the indices offset pointer.
    UtilsVk::ConvertIndexIndirectParameters params = {};
    params.srcIndirectBufOffset                    = static_cast<uint32_t>(srcIndirectBufOffset);
    params.srcIndexBufOffset                       = 0;
    params.dstIndexBufOffset                       = 0;
    params.maxIndex                                = static_cast<uint32_t>(srcDataSize);
    params.dstIndirectBufOffset                    = 0;

    return contextVk->getUtils().convertIndexIndirectBuffer(contextVk, srcIndirectBuf, srcIndexBuf,
                                                            dstIndirectBuf, dstIndexBuf, params);
}

angle::Result VertexArrayVk::handleLineLoopIndexIndirect(ContextVk *contextVk,
                                                         gl::DrawElementsType glIndexType,
                                                         vk::BufferHelper *srcIndirectBuf,
                                                         VkDeviceSize indirectBufferOffset,
                                                         vk::BufferHelper **indirectBufferOut)
{
    ANGLE_TRY(mLineLoopHelper.streamIndicesIndirect(
        contextVk, glIndexType, mCurrentElementArrayBuffer, srcIndirectBuf, indirectBufferOffset,
        &mCurrentElementArrayBuffer, indirectBufferOut));

    return angle::Result::Continue;
}

angle::Result VertexArrayVk::handleLineLoopIndirectDraw(const gl::Context *context,
                                                        vk::BufferHelper *indirectBufferVk,
                                                        VkDeviceSize indirectBufferOffset,
                                                        vk::BufferHelper **indirectBufferOut)
{
    size_t maxVertexCount = 0;
    ContextVk *contextVk  = vk::GetImpl(context);
    const gl::AttributesMask activeAttribs =
        context->getStateCache().getActiveBufferedAttribsMask();

    const auto &attribs  = mState.getVertexAttributes();
    const auto &bindings = mState.getVertexBindings();

    for (size_t attribIndex : activeAttribs)
    {
        const gl::VertexAttribute &attrib = attribs[attribIndex];
        ASSERT(attrib.enabled);
        VkDeviceSize bufSize             = getCurrentArrayBuffers()[attribIndex]->getSize();
        const gl::VertexBinding &binding = bindings[attrib.bindingIndex];
        size_t stride                    = binding.getStride();
        size_t vertexCount               = static_cast<size_t>(bufSize / stride);
        if (vertexCount > maxVertexCount)
        {
            maxVertexCount = vertexCount;
        }
    }
    ANGLE_TRY(mLineLoopHelper.streamArrayIndirect(contextVk, maxVertexCount + 1, indirectBufferVk,
                                                  indirectBufferOffset, &mCurrentElementArrayBuffer,
                                                  indirectBufferOut));

    return angle::Result::Continue;
}

angle::Result VertexArrayVk::convertIndexBufferCPU(ContextVk *contextVk,
                                                   gl::DrawElementsType indexType,
                                                   size_t indexCount,
                                                   const void *sourcePointer,
                                                   BufferBindingDirty *bindingDirty)
{
    ASSERT(!mState.getElementArrayBuffer() || indexType == gl::DrawElementsType::UnsignedByte);
    RendererVk *renderer = contextVk->getRenderer();
    size_t elementSize   = contextVk->getVkIndexTypeSize(indexType);
    const size_t amount  = elementSize * indexCount;

    // Applications often time draw a quad with two triangles. This is try to catch all the
    // common used element array buffer with pre-created BufferHelper objects to improve
    // performance.
    if (indexCount == kStreamIndexBufferCachedIndexCount &&
        indexType == gl::DrawElementsType::UnsignedShort)
    {
        for (std::unique_ptr<vk::BufferHelper> &buffer : mCachedStreamIndexBuffers)
        {
            void *ptr = buffer->getMappedMemory();
            if (memcmp(sourcePointer, ptr, amount) == 0)
            {
                // Found a matching cached buffer, use the cached internal index buffer.
                *bindingDirty              = mCurrentElementArrayBuffer == buffer.get()
                                                 ? BufferBindingDirty::No
                                                 : BufferBindingDirty::Yes;
                mCurrentElementArrayBuffer = buffer.get();
                return angle::Result::Continue;
            }
        }

        // If we still have capacity, cache this index buffer for future use.
        if (mCachedStreamIndexBuffers.size() < kMaxCachedStreamIndexBuffers)
        {
            std::unique_ptr<vk::BufferHelper> buffer = std::make_unique<vk::BufferHelper>();
            ANGLE_TRY(buffer->initSuballocation(contextVk,
                                                renderer->getVertexConversionBufferMemoryTypeIndex(
                                                    vk::MemoryHostVisibility::Visible),
                                                amount,
                                                renderer->getVertexConversionBufferAlignment()));
            memcpy(buffer->getMappedMemory(), sourcePointer, amount);
            ANGLE_TRY(buffer->flush(renderer));

            mCachedStreamIndexBuffers.push_back(std::move(buffer));

            *bindingDirty              = BufferBindingDirty::Yes;
            mCurrentElementArrayBuffer = mCachedStreamIndexBuffers.back().get();
            return angle::Result::Continue;
        }
    }

    ANGLE_TRY(mStreamedIndexData.allocateForVertexConversion(contextVk, amount,
                                                             vk::MemoryHostVisibility::Visible));
    GLubyte *dst = mStreamedIndexData.getMappedMemory();

    *bindingDirty              = BufferBindingDirty::Yes;
    mCurrentElementArrayBuffer = &mStreamedIndexData;

    if (contextVk->shouldConvertUint8VkIndexType(indexType))
    {
        // Unsigned bytes don't have direct support in Vulkan so we have to expand the
        // memory to a GLushort.
        const GLubyte *in     = static_cast<const GLubyte *>(sourcePointer);
        GLushort *expandedDst = reinterpret_cast<GLushort *>(dst);
        bool primitiveRestart = contextVk->getState().isPrimitiveRestartEnabled();

        constexpr GLubyte kUnsignedByteRestartValue   = 0xFF;
        constexpr GLushort kUnsignedShortRestartValue = 0xFFFF;

        if (primitiveRestart)
        {
            for (size_t index = 0; index < indexCount; index++)
            {
                GLushort value = static_cast<GLushort>(in[index]);
                if (in[index] == kUnsignedByteRestartValue)
                {
                    // Convert from 8-bit restart value to 16-bit restart value
                    value = kUnsignedShortRestartValue;
                }
                expandedDst[index] = value;
            }
        }
        else
        {
            // Fast path for common case.
            for (size_t index = 0; index < indexCount; index++)
            {
                expandedDst[index] = static_cast<GLushort>(in[index]);
            }
        }
    }
    else
    {
        // The primitive restart value is the same for OpenGL and Vulkan,
        // so there's no need to perform any conversion.
        memcpy(dst, sourcePointer, amount);
    }
    return mStreamedIndexData.flush(contextVk->getRenderer());
}

// We assume the buffer is completely full of the same kind of data and convert
// and/or align it as we copy it to a buffer. The assumption could be wrong
// but the alternative of copying it piecemeal on each draw would have a lot more
// overhead.
angle::Result VertexArrayVk::convertVertexBufferGPU(ContextVk *contextVk,
                                                    BufferVk *srcBuffer,
                                                    const gl::VertexBinding &binding,
                                                    size_t attribIndex,
                                                    const vk::Format &vertexFormat,
                                                    ConversionBuffer *conversion,
                                                    GLuint relativeOffset,
                                                    bool compressed)
{
    const angle::Format &srcFormat = vertexFormat.getIntendedFormat();
    const angle::Format &dstFormat = vertexFormat.getActualBufferFormat(compressed);

    ASSERT(binding.getStride() % (srcFormat.pixelBytes / srcFormat.channelCount) == 0);

    unsigned srcFormatSize = srcFormat.pixelBytes;
    unsigned dstFormatSize = dstFormat.pixelBytes;

    size_t numVertices = GetVertexCount(srcBuffer, binding, srcFormatSize);
    if (numVertices == 0)
    {
        return angle::Result::Continue;
    }
    ASSERT(vertexFormat.getVertexInputAlignment(compressed) <= vk::kVertexBufferAlignment);

    // Allocate buffer for results
    vk::BufferHelper *dstBuffer = conversion->data.get();
    ANGLE_TRY(dstBuffer->allocateForVertexConversion(contextVk, numVertices * dstFormatSize,
                                                     vk::MemoryHostVisibility::NonVisible));

    ASSERT(conversion->dirty);
    conversion->dirty = false;

    vk::BufferHelper *srcBufferHelper = &srcBuffer->getBuffer();

    UtilsVk::ConvertVertexParameters params;
    params.vertexCount = numVertices;
    params.srcFormat   = &srcFormat;
    params.dstFormat   = &dstFormat;
    params.srcStride   = binding.getStride();
    params.srcOffset   = binding.getOffset() + relativeOffset;
    params.dstOffset   = 0;

    ANGLE_TRY(
        contextVk->getUtils().convertVertexBuffer(contextVk, dstBuffer, srcBufferHelper, params));

    return angle::Result::Continue;
}

angle::Result VertexArrayVk::convertVertexBufferCPU(ContextVk *contextVk,
                                                    BufferVk *srcBuffer,
                                                    const gl::VertexBinding &binding,
                                                    size_t attribIndex,
                                                    const vk::Format &vertexFormat,
                                                    ConversionBuffer *conversion,
                                                    GLuint relativeOffset,
                                                    bool compressed)
{
    ANGLE_TRACE_EVENT0("gpu.angle", "VertexArrayVk::convertVertexBufferCpu");

    unsigned srcFormatSize = vertexFormat.getIntendedFormat().pixelBytes;
    unsigned dstFormatSize = vertexFormat.getActualBufferFormat(compressed).pixelBytes;

    size_t numVertices = GetVertexCount(srcBuffer, binding, srcFormatSize);
    if (numVertices == 0)
    {
        return angle::Result::Continue;
    }

    void *src = nullptr;
    ANGLE_TRY(srcBuffer->mapImpl(contextVk, GL_MAP_READ_BIT, &src));
    const uint8_t *srcBytes = reinterpret_cast<const uint8_t *>(src);
    srcBytes += binding.getOffset() + relativeOffset;
    ASSERT(vertexFormat.getVertexInputAlignment(compressed) <= vk::kVertexBufferAlignment);

    vk::BufferHelper *dstBufferHelper = conversion->data.get();
    // Allocate buffer for results
    ANGLE_TRY(dstBufferHelper->allocateForVertexConversion(contextVk, numVertices * dstFormatSize,
                                                           vk::MemoryHostVisibility::Visible));

    ANGLE_TRY(StreamVertexData(contextVk, dstBufferHelper, srcBytes, numVertices * dstFormatSize, 0,
                               numVertices, binding.getStride(),
                               vertexFormat.getVertexLoadFunction(compressed)));
    ANGLE_TRY(srcBuffer->unmapImpl(contextVk));
    mCurrentArrayBuffers[attribIndex] = dstBufferHelper;

    ASSERT(conversion->dirty);
    conversion->dirty = false;

    return angle::Result::Continue;
}

void VertexArrayVk::updateCurrentElementArrayBuffer()
{
    ASSERT(mState.getElementArrayBuffer() != nullptr);
    ASSERT(mState.getElementArrayBuffer()->getSize() > 0);

    BufferVk *bufferVk         = vk::GetImpl(mState.getElementArrayBuffer());
    mCurrentElementArrayBuffer = &bufferVk->getBuffer();
}

angle::Result VertexArrayVk::syncState(const gl::Context *context,
                                       const gl::VertexArray::DirtyBits &dirtyBits,
                                       gl::VertexArray::DirtyAttribBitsArray *attribBits,
                                       gl::VertexArray::DirtyBindingBitsArray *bindingBits)
{
    ASSERT(dirtyBits.any());

    ContextVk *contextVk = vk::GetImpl(context);
    contextVk->getPerfCounters().vertexArraySyncStateCalls++;

    const std::vector<gl::VertexAttribute> &attribs = mState.getVertexAttributes();
    const std::vector<gl::VertexBinding> &bindings  = mState.getVertexBindings();

    for (auto iter = dirtyBits.begin(), endIter = dirtyBits.end(); iter != endIter; ++iter)
    {
        size_t dirtyBit = *iter;
        switch (dirtyBit)
        {
            case gl::VertexArray::DIRTY_BIT_ELEMENT_ARRAY_BUFFER:
            case gl::VertexArray::DIRTY_BIT_ELEMENT_ARRAY_BUFFER_DATA:
            {
                gl::Buffer *bufferGL = mState.getElementArrayBuffer();
                if (bufferGL && bufferGL->getSize() > 0)
                {
                    // Note that just updating buffer data may still result in a new
                    // vk::BufferHelper allocation.
                    updateCurrentElementArrayBuffer();
                }
                else
                {
                    mCurrentElementArrayBuffer = nullptr;
                }

                mLineLoopBufferFirstIndex.reset();
                mLineLoopBufferLastIndex.reset();
                ANGLE_TRY(contextVk->onIndexBufferChange(mCurrentElementArrayBuffer));
                mDirtyLineLoopTranslation = true;
                break;
            }

#define ANGLE_VERTEX_DIRTY_ATTRIB_FUNC(INDEX)                                                 \
    case gl::VertexArray::DIRTY_BIT_ATTRIB_0 + INDEX:                                         \
    {                                                                                         \
        const bool bufferOnly =                                                               \
            (*attribBits)[INDEX].to_ulong() ==                                                \
            angle::Bit<unsigned long>(gl::VertexArray::DIRTY_ATTRIB_POINTER_BUFFER);          \
        ANGLE_TRY(syncDirtyAttrib(contextVk, attribs[INDEX],                                  \
                                  bindings[attribs[INDEX].bindingIndex], INDEX, bufferOnly)); \
        (*attribBits)[INDEX].reset();                                                         \
        break;                                                                                \
    }

                ANGLE_VERTEX_INDEX_CASES(ANGLE_VERTEX_DIRTY_ATTRIB_FUNC)

// Since BINDING already implies DATA and ATTRIB change, we remove these here to avoid redundant
// processing.
#define ANGLE_VERTEX_DIRTY_BINDING_FUNC(INDEX)                                          \
    case gl::VertexArray::DIRTY_BIT_BINDING_0 + INDEX:                                  \
        for (size_t attribIndex : bindings[INDEX].getBoundAttributesMask())             \
        {                                                                               \
            ANGLE_TRY(syncDirtyAttrib(contextVk, attribs[attribIndex], bindings[INDEX], \
                                      attribIndex, false));                             \
            iter.resetLaterBit(gl::VertexArray::DIRTY_BIT_BUFFER_DATA_0 + attribIndex); \
            iter.resetLaterBit(gl::VertexArray::DIRTY_BIT_ATTRIB_0 + attribIndex);      \
            (*attribBits)[attribIndex].reset();                                         \
        }                                                                               \
        (*bindingBits)[INDEX].reset();                                                  \
        break;

                ANGLE_VERTEX_INDEX_CASES(ANGLE_VERTEX_DIRTY_BINDING_FUNC)

#define ANGLE_VERTEX_DIRTY_BUFFER_DATA_FUNC(INDEX)                                       \
    case gl::VertexArray::DIRTY_BIT_BUFFER_DATA_0 + INDEX:                               \
        ANGLE_TRY(syncDirtyAttrib(contextVk, attribs[INDEX],                             \
                                  bindings[attribs[INDEX].bindingIndex], INDEX, false)); \
        iter.resetLaterBit(gl::VertexArray::DIRTY_BIT_ATTRIB_0 + INDEX);                 \
        (*attribBits)[INDEX].reset();                                                    \
        break;

                ANGLE_VERTEX_INDEX_CASES(ANGLE_VERTEX_DIRTY_BUFFER_DATA_FUNC)

            default:
                UNREACHABLE();
                break;
        }
    }

    return angle::Result::Continue;
}

#undef ANGLE_VERTEX_DIRTY_ATTRIB_FUNC
#undef ANGLE_VERTEX_DIRTY_BINDING_FUNC
#undef ANGLE_VERTEX_DIRTY_BUFFER_DATA_FUNC

ANGLE_INLINE angle::Result VertexArrayVk::setDefaultPackedInput(ContextVk *contextVk,
                                                                size_t attribIndex,
                                                                angle::FormatID *formatOut)
{
    const gl::State &glState = contextVk->getState();
    const gl::VertexAttribCurrentValueData &defaultValue =
        glState.getVertexAttribCurrentValues()[attribIndex];

    *formatOut = GetCurrentValueFormatID(defaultValue.Type);

    return contextVk->onVertexAttributeChange(attribIndex, 0, 0, *formatOut, false, 0, nullptr);
}

angle::Result VertexArrayVk::updateActiveAttribInfo(ContextVk *contextVk)
{
    const std::vector<gl::VertexAttribute> &attribs = mState.getVertexAttributes();
    const std::vector<gl::VertexBinding> &bindings  = mState.getVertexBindings();

    // Update pipeline cache with current active attribute info
    for (size_t attribIndex : mState.getEnabledAttributesMask())
    {
        const gl::VertexAttribute &attrib = attribs[attribIndex];
        const gl::VertexBinding &binding  = bindings[attribs[attribIndex].bindingIndex];
        const angle::FormatID format      = attrib.format->id;

        ANGLE_TRY(contextVk->onVertexAttributeChange(
            attribIndex, mCurrentArrayBufferStrides[attribIndex], binding.getDivisor(), format,
            mCurrentArrayBufferCompressed.test(attribIndex),
            mCurrentArrayBufferRelativeOffsets[attribIndex], mCurrentArrayBuffers[attribIndex]));

        mCurrentArrayBufferFormats[attribIndex] = format;
    }

    return angle::Result::Continue;
}

angle::Result VertexArrayVk::syncDirtyAttrib(ContextVk *contextVk,
                                             const gl::VertexAttribute &attrib,
                                             const gl::VertexBinding &binding,
                                             size_t attribIndex,
                                             bool bufferOnly)
{
    RendererVk *renderer = contextVk->getRenderer();
    if (attrib.enabled)
    {
        const vk::Format &vertexFormat = renderer->getFormat(attrib.format->id);

        // Init attribute offset to the front-end value
        mCurrentArrayBufferRelativeOffsets[attribIndex] = attrib.relativeOffset;
        gl::Buffer *bufferGL                            = binding.getBuffer().get();
        // Emulated and/or client-side attribs will be streamed
        bool isStreamingVertexAttrib =
            (binding.getDivisor() > renderer->getMaxVertexAttribDivisor()) || (bufferGL == nullptr);
        mStreamingVertexAttribsMask.set(attribIndex, isStreamingVertexAttrib);
        bool compressed = false;

        if (bufferGL)
        {
            mContentsObservers->disableForBuffer(bufferGL, static_cast<uint32_t>(attribIndex));
        }

        if (!isStreamingVertexAttrib && bufferGL->getSize() > 0)
        {
            BufferVk *bufferVk                  = vk::GetImpl(bufferGL);
            const angle::Format &intendedFormat = vertexFormat.getIntendedFormat();
            bool bindingIsAligned               = BindingIsAligned(
                              binding, intendedFormat, intendedFormat.channelCount, attrib.relativeOffset);

            if (renderer->getFeatures().compressVertexData.enabled &&
                gl::IsStaticBufferUsage(bufferGL->getUsage()) &&
                vertexFormat.canCompressBufferData())
            {
                compressed = true;
            }

            bool needsConversion =
                vertexFormat.getVertexLoadRequiresConversion(compressed) || !bindingIsAligned;

            if (needsConversion)
            {
                mContentsObservers->enableForBuffer(bufferGL, static_cast<uint32_t>(attribIndex));

                WarnOnVertexFormatConversion(contextVk, vertexFormat, compressed, true);

                ConversionBuffer *conversion = bufferVk->getVertexConversionBuffer(
                    renderer, intendedFormat.id, binding.getStride(),
                    binding.getOffset() + attrib.relativeOffset, !bindingIsAligned);
                if (conversion->dirty)
                {
                    if (compressed)
                    {
                        INFO() << "Compressing vertex data in buffer " << bufferGL->id().value
                               << " from " << ToUnderlying(vertexFormat.getIntendedFormatID())
                               << " to "
                               << ToUnderlying(vertexFormat.getActualBufferFormat(true).id) << ".";
                    }

                    if (bindingIsAligned)
                    {
                        ANGLE_TRY(convertVertexBufferGPU(contextVk, bufferVk, binding, attribIndex,
                                                         vertexFormat, conversion,
                                                         attrib.relativeOffset, compressed));
                    }
                    else
                    {
                        ANGLE_VK_PERF_WARNING(
                            contextVk, GL_DEBUG_SEVERITY_HIGH,
                            "GPU stall due to vertex format conversion of unaligned data");

                        ANGLE_TRY(convertVertexBufferCPU(contextVk, bufferVk, binding, attribIndex,
                                                         vertexFormat, conversion,
                                                         attrib.relativeOffset, compressed));
                    }

                    // If conversion happens, the destination buffer stride may be changed,
                    // therefore an attribute change needs to be called. Note that it may trigger
                    // unnecessary vulkan PSO update when the destination buffer stride does not
                    // change, but for simplicity just make it conservative
                    bufferOnly = false;
                }

                vk::BufferHelper *bufferHelper    = conversion->data.get();
                mCurrentArrayBuffers[attribIndex] = bufferHelper;
                VkDeviceSize bufferOffset;
                mCurrentArrayBufferHandles[attribIndex] =
                    bufferHelper
                        ->getBufferForVertexArray(contextVk, bufferHelper->getSize(), &bufferOffset)
                        .getHandle();
                mCurrentArrayBufferOffsets[attribIndex] = bufferOffset;
                // Converted attribs are packed in their own VK buffer so offset is zero
                mCurrentArrayBufferRelativeOffsets[attribIndex] = 0;

                // Converted buffer is tightly packed
                mCurrentArrayBufferStrides[attribIndex] =
                    vertexFormat.getActualBufferFormat(compressed).pixelBytes;
            }
            else
            {
                if (bufferVk->getSize() == 0)
                {
                    vk::BufferHelper &emptyBuffer = contextVk->getEmptyBuffer();

                    mCurrentArrayBuffers[attribIndex]       = &emptyBuffer;
                    mCurrentArrayBufferHandles[attribIndex] = emptyBuffer.getBuffer().getHandle();
                    mCurrentArrayBufferOffsets[attribIndex] = emptyBuffer.getOffset();
                    mCurrentArrayBufferStrides[attribIndex] = 0;
                }
                else
                {
                    vk::BufferHelper &bufferHelper    = bufferVk->getBuffer();
                    mCurrentArrayBuffers[attribIndex] = &bufferHelper;
                    VkDeviceSize bufferOffset;
                    mCurrentArrayBufferHandles[attribIndex] =
                        bufferHelper
                            .getBufferForVertexArray(contextVk, bufferVk->getSize(), &bufferOffset)
                            .getHandle();

                    // Vulkan requires the offset is within the buffer. We use robust access
                    // behaviour to reset the offset if it starts outside the buffer.
                    mCurrentArrayBufferOffsets[attribIndex] =
                        binding.getOffset() < static_cast<GLint64>(bufferVk->getSize())
                            ? binding.getOffset() + bufferOffset
                            : bufferOffset;

                    mCurrentArrayBufferStrides[attribIndex] = binding.getStride();
                }
            }
        }
        else
        {
            vk::BufferHelper &emptyBuffer           = contextVk->getEmptyBuffer();
            mCurrentArrayBuffers[attribIndex]       = &emptyBuffer;
            mCurrentArrayBufferHandles[attribIndex] = emptyBuffer.getBuffer().getHandle();
            mCurrentArrayBufferOffsets[attribIndex] = emptyBuffer.getOffset();
            // Client side buffer will be transfered to a tightly packed buffer later
            mCurrentArrayBufferStrides[attribIndex] =
                vertexFormat.getActualBufferFormat(compressed).pixelBytes;
        }

        if (bufferOnly)
        {
            ANGLE_TRY(contextVk->onVertexBufferChange(mCurrentArrayBuffers[attribIndex]));
        }
        else
        {
            const angle::FormatID format = attrib.format->id;
            ANGLE_TRY(contextVk->onVertexAttributeChange(
                attribIndex, mCurrentArrayBufferStrides[attribIndex], binding.getDivisor(), format,
                compressed, mCurrentArrayBufferRelativeOffsets[attribIndex],
                mCurrentArrayBuffers[attribIndex]));

            mCurrentArrayBufferFormats[attribIndex]    = format;
            mCurrentArrayBufferCompressed[attribIndex] = compressed;
        }
    }
    else
    {
        contextVk->invalidateDefaultAttribute(attribIndex);

        // These will be filled out by the ContextVk.
        vk::BufferHelper &emptyBuffer                   = contextVk->getEmptyBuffer();
        mCurrentArrayBuffers[attribIndex]               = &emptyBuffer;
        mCurrentArrayBufferHandles[attribIndex]         = emptyBuffer.getBuffer().getHandle();
        mCurrentArrayBufferOffsets[attribIndex]         = emptyBuffer.getOffset();
        mCurrentArrayBufferStrides[attribIndex]         = 0;
        mCurrentArrayBufferCompressed[attribIndex]      = false;
        mCurrentArrayBufferRelativeOffsets[attribIndex] = 0;

        ANGLE_TRY(setDefaultPackedInput(contextVk, attribIndex,
                                        &mCurrentArrayBufferFormats[attribIndex]));
    }

    return angle::Result::Continue;
}

// Handle copying client attribs and/or expanding attrib buffer in case where attribute
//  divisor value has to be emulated.
angle::Result VertexArrayVk::updateStreamedAttribs(const gl::Context *context,
                                                   GLint firstVertex,
                                                   GLsizei vertexOrIndexCount,
                                                   GLsizei instanceCount,
                                                   gl::DrawElementsType indexTypeOrInvalid,
                                                   const void *indices)
{
    ContextVk *contextVk = vk::GetImpl(context);
    RendererVk *renderer = contextVk->getRenderer();

    const gl::AttributesMask activeAttribs =
        context->getStateCache().getActiveClientAttribsMask() |
        context->getStateCache().getActiveBufferedAttribsMask();
    const gl::AttributesMask activeStreamedAttribs = mStreamingVertexAttribsMask & activeAttribs;

    // Early return for corner case where emulated buffered attribs are not active
    if (!activeStreamedAttribs.any())
        return angle::Result::Continue;

    GLint startVertex;
    size_t vertexCount;
    ANGLE_TRY(GetVertexRangeInfo(context, firstVertex, vertexOrIndexCount, indexTypeOrInvalid,
                                 indices, 0, &startVertex, &vertexCount));

    const auto &attribs  = mState.getVertexAttributes();
    const auto &bindings = mState.getVertexBindings();

    // TODO: When we have a bunch of interleaved attributes, they end up
    // un-interleaved, wasting space and copying time.  Consider improving on that.
    for (size_t attribIndex : activeStreamedAttribs)
    {
        const gl::VertexAttribute &attrib = attribs[attribIndex];
        ASSERT(attrib.enabled);
        const gl::VertexBinding &binding = bindings[attrib.bindingIndex];

        const vk::Format &vertexFormat = renderer->getFormat(attrib.format->id);
        GLuint stride                  = vertexFormat.getActualBufferFormat(false).pixelBytes;

        bool compressed = false;
        WarnOnVertexFormatConversion(contextVk, vertexFormat, compressed, false);

        ASSERT(vertexFormat.getVertexInputAlignment(false) <= vk::kVertexBufferAlignment);

        vk::BufferHelper *vertexDataBuffer;
        const uint8_t *src     = static_cast<const uint8_t *>(attrib.pointer);
        const uint32_t divisor = binding.getDivisor();
        if (divisor > 0)
        {
            // Instanced attrib
            if (divisor > renderer->getMaxVertexAttribDivisor())
            {
                // Divisor will be set to 1 & so update buffer to have 1 attrib per instance
                size_t bytesToAllocate = instanceCount * stride;

                // Allocate buffer for results
                ANGLE_TRY(contextVk->allocateStreamedVertexBuffer(attribIndex, bytesToAllocate,
                                                                  &vertexDataBuffer));

                gl::Buffer *bufferGL = binding.getBuffer().get();
                if (bufferGL != nullptr)
                {
                    // Only do the data copy if src buffer is valid.
                    if (bufferGL->getSize() > 0)
                    {
                        // Map buffer to expand attribs for divisor emulation
                        BufferVk *bufferVk = vk::GetImpl(binding.getBuffer().get());
                        void *buffSrc      = nullptr;
                        ANGLE_TRY(bufferVk->mapImpl(contextVk, GL_MAP_READ_BIT, &buffSrc));
                        src = reinterpret_cast<const uint8_t *>(buffSrc) + binding.getOffset();

                        uint32_t srcAttributeSize =
                            static_cast<uint32_t>(ComputeVertexAttributeTypeSize(attrib));

                        size_t numVertices = GetVertexCount(bufferVk, binding, srcAttributeSize);

                        ANGLE_TRY(StreamVertexDataWithDivisor(
                            contextVk, vertexDataBuffer, src, bytesToAllocate, binding.getStride(),
                            stride, vertexFormat.getVertexLoadFunction(compressed), divisor,
                            numVertices));

                        ANGLE_TRY(bufferVk->unmapImpl(contextVk));
                    }
                    else if (contextVk->getExtensions().robustnessEXT)
                    {
                        // Satisfy robustness constraints (only if extension enabled)
                        uint8_t *dst = vertexDataBuffer->getMappedMemory();
                        memset(dst, 0, bytesToAllocate);
                    }
                }
                else
                {
                    size_t numVertices = instanceCount;
                    ANGLE_TRY(StreamVertexDataWithDivisor(
                        contextVk, vertexDataBuffer, src, bytesToAllocate, binding.getStride(),
                        stride, vertexFormat.getVertexLoadFunction(compressed), divisor,
                        numVertices));
                }
            }
            else
            {
                ASSERT(binding.getBuffer().get() == nullptr);
                size_t count           = UnsignedCeilDivide(instanceCount, divisor);
                size_t bytesToAllocate = count * stride;

                // Allocate buffer for results
                ANGLE_TRY(contextVk->allocateStreamedVertexBuffer(attribIndex, bytesToAllocate,
                                                                  &vertexDataBuffer));

                ANGLE_TRY(StreamVertexData(contextVk, vertexDataBuffer, src, bytesToAllocate, 0,
                                           count, binding.getStride(),
                                           vertexFormat.getVertexLoadFunction(compressed)));
            }
        }
        else
        {
            ASSERT(binding.getBuffer().get() == nullptr);
            // Allocate space for startVertex + vertexCount so indexing will work.  If we don't
            // start at zero all the indices will be off.
            // Only vertexCount vertices will be used by the upcoming draw so that is all we copy.
            src += startVertex * binding.getStride();
            size_t destOffset      = startVertex * stride;
            size_t bytesToAllocate = (startVertex + vertexCount) * stride;

            // Allocate buffer for results
            ANGLE_TRY(contextVk->allocateStreamedVertexBuffer(attribIndex, bytesToAllocate,
                                                              &vertexDataBuffer));

            ANGLE_TRY(StreamVertexData(contextVk, vertexDataBuffer, src, bytesToAllocate,
                                       destOffset, vertexCount, binding.getStride(),
                                       vertexFormat.getVertexLoadFunction(compressed)));
        }

        mCurrentArrayBuffers[attribIndex] = vertexDataBuffer;
        VkDeviceSize bufferOffset;
        mCurrentArrayBufferHandles[attribIndex] =
            vertexDataBuffer
                ->getBufferForVertexArray(contextVk, vertexDataBuffer->getSize(), &bufferOffset)
                .getHandle();
        mCurrentArrayBufferOffsets[attribIndex] = bufferOffset;
        mCurrentArrayBufferStrides[attribIndex] = stride;
    }

    return angle::Result::Continue;
}

angle::Result VertexArrayVk::handleLineLoop(ContextVk *contextVk,
                                            GLint firstVertex,
                                            GLsizei vertexOrIndexCount,
                                            gl::DrawElementsType indexTypeOrInvalid,
                                            const void *indices,
                                            uint32_t *indexCountOut)
{
    if (indexTypeOrInvalid != gl::DrawElementsType::InvalidEnum)
    {
        // Handle GL_LINE_LOOP drawElements.
        if (mDirtyLineLoopTranslation)
        {
            gl::Buffer *elementArrayBuffer = mState.getElementArrayBuffer();

            if (!elementArrayBuffer)
            {
                ANGLE_TRY(
                    mLineLoopHelper.streamIndices(contextVk, indexTypeOrInvalid, vertexOrIndexCount,
                                                  reinterpret_cast<const uint8_t *>(indices),
                                                  &mCurrentElementArrayBuffer, indexCountOut));
            }
            else
            {
                // When using an element array buffer, 'indices' is an offset to the first element.
                intptr_t offset                = reinterpret_cast<intptr_t>(indices);
                BufferVk *elementArrayBufferVk = vk::GetImpl(elementArrayBuffer);
                ANGLE_TRY(mLineLoopHelper.getIndexBufferForElementArrayBuffer(
                    contextVk, elementArrayBufferVk, indexTypeOrInvalid, vertexOrIndexCount, offset,
                    &mCurrentElementArrayBuffer, indexCountOut));
            }
        }

        // If we've had a drawArrays call with a line loop before, we want to make sure this is
        // invalidated the next time drawArrays is called since we use the same index buffer for
        // both calls.
        mLineLoopBufferFirstIndex.reset();
        mLineLoopBufferLastIndex.reset();
        return angle::Result::Continue;
    }

    // Note: Vertex indexes can be arbitrarily large.
    uint32_t clampedVertexCount = gl::clampCast<uint32_t>(vertexOrIndexCount);

    // Handle GL_LINE_LOOP drawArrays.
    size_t lastVertex = static_cast<size_t>(firstVertex + clampedVertexCount);
    if (!mLineLoopBufferFirstIndex.valid() || !mLineLoopBufferLastIndex.valid() ||
        mLineLoopBufferFirstIndex != firstVertex || mLineLoopBufferLastIndex != lastVertex)
    {
        ANGLE_TRY(mLineLoopHelper.getIndexBufferForDrawArrays(
            contextVk, clampedVertexCount, firstVertex, &mCurrentElementArrayBuffer));

        mLineLoopBufferFirstIndex = firstVertex;
        mLineLoopBufferLastIndex  = lastVertex;
    }
    *indexCountOut = vertexOrIndexCount + 1;

    return angle::Result::Continue;
}

angle::Result VertexArrayVk::updateDefaultAttrib(ContextVk *contextVk, size_t attribIndex)
{
    if (!mState.getEnabledAttributesMask().test(attribIndex))
    {
        vk::BufferHelper *bufferHelper;
        ANGLE_TRY(
            contextVk->allocateStreamedVertexBuffer(attribIndex, kDefaultValueSize, &bufferHelper));

        const gl::VertexAttribCurrentValueData &defaultValue =
            contextVk->getState().getVertexAttribCurrentValues()[attribIndex];
        uint8_t *ptr = bufferHelper->getMappedMemory();
        memcpy(ptr, &defaultValue.Values, kDefaultValueSize);
        ANGLE_TRY(bufferHelper->flush(contextVk->getRenderer()));

        VkDeviceSize bufferOffset;
        mCurrentArrayBufferHandles[attribIndex] =
            bufferHelper->getBufferForVertexArray(contextVk, kDefaultValueSize, &bufferOffset)
                .getHandle();
        mCurrentArrayBufferOffsets[attribIndex] = bufferOffset;
        mCurrentArrayBuffers[attribIndex]       = bufferHelper;
        mCurrentArrayBufferStrides[attribIndex] = 0;

        ANGLE_TRY(setDefaultPackedInput(contextVk, attribIndex,
                                        &mCurrentArrayBufferFormats[attribIndex]));
    }

    return angle::Result::Continue;
}
}  // namespace rx
