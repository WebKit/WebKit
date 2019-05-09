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
#include "libANGLE/renderer/vulkan/CommandGraph.h"
#include "libANGLE/renderer/vulkan/ContextVk.h"
#include "libANGLE/renderer/vulkan/FramebufferVk.h"
#include "libANGLE/renderer/vulkan/RendererVk.h"
#include "libANGLE/renderer/vulkan/vk_format_utils.h"
#include "third_party/trace_event/trace_event.h"

namespace rx
{
namespace
{
constexpr size_t kDynamicVertexDataSize    = 1024 * 1024;
constexpr size_t kDynamicIndexDataSize     = 1024 * 8;
constexpr size_t kMaxVertexFormatAlignment = 4;
constexpr VkBufferUsageFlags kVertexBufferUsageFlags =
    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
constexpr VkBufferUsageFlags kIndexBufferUsageFlags = VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                                      VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
                                                      VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT;

ANGLE_INLINE bool BindingIsAligned(const gl::VertexBinding &binding,
                                   const angle::Format &angleFormat,
                                   unsigned int attribSize)
{
    GLuint mask = angleFormat.componentAlignmentMask;
    if (mask != std::numeric_limits<GLuint>::max())
    {
        return ((binding.getOffset() & mask) == 0 && (binding.getStride() & mask) == 0);
    }
    else
    {
        unsigned int formatSize = angleFormat.pixelBytes;
        return ((binding.getOffset() * attribSize) % formatSize == 0) &&
               ((binding.getStride() * attribSize) % formatSize == 0);
    }
}

angle::Result StreamVertexData(ContextVk *contextVk,
                               vk::DynamicBuffer *dynamicBuffer,
                               const uint8_t *sourceData,
                               size_t bytesToAllocate,
                               size_t destOffset,
                               size_t vertexCount,
                               size_t stride,
                               VertexCopyFunction vertexLoadFunction,
                               vk::BufferHelper **bufferOut,
                               VkDeviceSize *bufferOffsetOut)
{
    uint8_t *dst = nullptr;
    ANGLE_TRY(dynamicBuffer->allocate(contextVk, bytesToAllocate, &dst, nullptr, bufferOffsetOut,
                                      nullptr));
    *bufferOut = dynamicBuffer->getCurrentBuffer();
    dst += destOffset;
    vertexLoadFunction(sourceData, stride, vertexCount, dst);

    ANGLE_TRY(dynamicBuffer->flush(contextVk));
    return angle::Result::Continue;
}

size_t GetVertexCount(BufferVk *srcBuffer, const gl::VertexBinding &binding, uint32_t srcFormatSize)
{
    // Bytes usable for vertex data.
    GLint64 bytes = srcBuffer->getSize() - binding.getOffset();
    if (bytes < srcFormatSize)
        return 0;

    // Count the last vertex.  It may occupy less than a full stride.
    size_t numVertices = 1;
    bytes -= srcFormatSize;

    // Count how many strides fit remaining space.
    if (bytes > 0)
        numVertices += static_cast<size_t>(bytes) / binding.getStride();

    return numVertices;
}
}  // anonymous namespace

#define INIT                                    \
    {                                           \
        kVertexBufferUsageFlags, 1024 * 8, true \
    }

VertexArrayVk::VertexArrayVk(ContextVk *contextVk, const gl::VertexArrayState &state)
    : VertexArrayImpl(state),
      mCurrentArrayBufferHandles{},
      mCurrentArrayBufferOffsets{},
      mCurrentArrayBuffers{},
      mCurrentArrayBufferConversion{{
          INIT,
          INIT,
          INIT,
          INIT,
          INIT,
          INIT,
          INIT,
          INIT,
          INIT,
          INIT,
          INIT,
          INIT,
          INIT,
          INIT,
          INIT,
          INIT,
      }},
      mCurrentArrayBufferConversionCanRelease{},
      mCurrentElementArrayBufferOffset(0),
      mCurrentElementArrayBuffer(nullptr),
      mDynamicVertexData(kVertexBufferUsageFlags, kDynamicVertexDataSize, true),
      mDynamicIndexData(kIndexBufferUsageFlags, kDynamicIndexDataSize, true),
      mTranslatedByteIndexData(kIndexBufferUsageFlags, kDynamicIndexDataSize, true),
      mLineLoopHelper(contextVk->getRenderer()),
      mDirtyLineLoopTranslation(true)
{
    RendererVk *renderer = contextVk->getRenderer();

    VkBufferCreateInfo createInfo = {};
    createInfo.sType              = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    createInfo.size               = 16;
    createInfo.usage              = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    (void)mTheNullBuffer.init(contextVk, createInfo, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    mCurrentArrayBufferHandles.fill(mTheNullBuffer.getBuffer().getHandle());
    mCurrentArrayBufferOffsets.fill(0);
    mCurrentArrayBuffers.fill(&mTheNullBuffer);

    for (vk::DynamicBuffer &buffer : mCurrentArrayBufferConversion)
    {
        buffer.init(kMaxVertexFormatAlignment, renderer);
    }
    mDynamicVertexData.init(kMaxVertexFormatAlignment, renderer);
    mDynamicIndexData.init(1, renderer);
    mTranslatedByteIndexData.init(1, renderer);
}

VertexArrayVk::~VertexArrayVk() {}

void VertexArrayVk::destroy(const gl::Context *context)
{
    RendererVk *renderer = vk::GetImpl(context)->getRenderer();

    mTheNullBuffer.release(renderer);

    for (vk::DynamicBuffer &buffer : mCurrentArrayBufferConversion)
    {
        buffer.release(renderer);
    }
    mDynamicVertexData.release(renderer);
    mDynamicIndexData.release(renderer);
    mTranslatedByteIndexData.release(renderer);
    mLineLoopHelper.release(renderer);
}

angle::Result VertexArrayVk::streamIndexData(ContextVk *contextVk,
                                             gl::DrawElementsType indexType,
                                             size_t indexCount,
                                             const void *sourcePointer,
                                             vk::DynamicBuffer *dynamicBuffer)
{
    ASSERT(!mState.getElementArrayBuffer() || indexType == gl::DrawElementsType::UnsignedByte);

    dynamicBuffer->releaseRetainedBuffers(contextVk->getRenderer());

    const size_t amount = sizeof(GLushort) * indexCount;
    GLubyte *dst        = nullptr;

    ANGLE_TRY(dynamicBuffer->allocate(contextVk, amount, &dst, nullptr,
                                      &mCurrentElementArrayBufferOffset, nullptr));
    mCurrentElementArrayBuffer = dynamicBuffer->getCurrentBuffer();
    if (indexType == gl::DrawElementsType::UnsignedByte)
    {
        // Unsigned bytes don't have direct support in Vulkan so we have to expand the
        // memory to a GLushort.
        const GLubyte *in     = static_cast<const GLubyte *>(sourcePointer);
        GLushort *expandedDst = reinterpret_cast<GLushort *>(dst);
        for (size_t index = 0; index < indexCount; index++)
        {
            expandedDst[index] = static_cast<GLushort>(in[index]);
        }
    }
    else
    {
        memcpy(dst, sourcePointer, amount);
    }
    ANGLE_TRY(dynamicBuffer->flush(contextVk));
    return angle::Result::Continue;
}

// We assume the buffer is completely full of the same kind of data and convert
// and/or align it as we copy it to a DynamicBuffer. The assumption could be wrong
// but the alternative of copying it piecemeal on each draw would have a lot more
// overhead.
angle::Result VertexArrayVk::convertVertexBufferGpu(ContextVk *contextVk,
                                                    BufferVk *srcBuffer,
                                                    const gl::VertexBinding &binding,
                                                    size_t attribIndex,
                                                    const vk::Format &vertexFormat)
{
    RendererVk *renderer = contextVk->getRenderer();

    const angle::Format &srcFormat  = vertexFormat.angleFormat();
    const angle::Format &destFormat = vertexFormat.bufferFormat();

    ASSERT(binding.getStride() % (srcFormat.pixelBytes / srcFormat.channelCount()) == 0);

    unsigned srcFormatSize  = srcFormat.pixelBytes;
    unsigned destFormatSize = destFormat.pixelBytes;

    size_t numVertices = GetVertexCount(srcBuffer, binding, srcFormatSize);
    if (numVertices == 0)
    {
        return angle::Result::Continue;
    }

    ASSERT(GetVertexInputAlignment(vertexFormat) <= kMaxVertexFormatAlignment);

    // Allocate buffer for results
    mCurrentArrayBufferConversion[attribIndex].releaseRetainedBuffers(renderer);
    ANGLE_TRY(mCurrentArrayBufferConversion[attribIndex].allocate(
        contextVk, numVertices * destFormatSize, nullptr, nullptr,
        &mCurrentArrayBufferOffsets[attribIndex], nullptr));
    mCurrentArrayBuffers[attribIndex] =
        mCurrentArrayBufferConversion[attribIndex].getCurrentBuffer();

    UtilsVk::ConvertVertexParameters params;
    params.vertexCount = numVertices;
    params.srcFormat   = &srcFormat;
    params.destFormat  = &destFormat;
    params.srcStride   = binding.getStride();
    params.srcOffset   = binding.getOffset();
    params.destOffset  = static_cast<size_t>(mCurrentArrayBufferOffsets[attribIndex]);

    ANGLE_TRY(renderer->getUtils().convertVertexBuffer(contextVk, mCurrentArrayBuffers[attribIndex],
                                                       &srcBuffer->getBuffer(), params));

    mCurrentArrayBufferHandles[attribIndex] =
        mCurrentArrayBuffers[attribIndex]->getBuffer().getHandle();
    mCurrentArrayBufferConversionCanRelease[attribIndex] = true;

    return angle::Result::Continue;
}

angle::Result VertexArrayVk::convertVertexBufferCpu(ContextVk *contextVk,
                                                    BufferVk *srcBuffer,
                                                    const gl::VertexBinding &binding,
                                                    size_t attribIndex,
                                                    const vk::Format &vertexFormat)
{
    TRACE_EVENT0("gpu.angle", "VertexArrayVk::convertVertexBufferCpu");
    // Needed before reading buffer or we could get stale data.
    ANGLE_TRY(contextVk->finishImpl());

    unsigned srcFormatSize = vertexFormat.angleFormat().pixelBytes;
    unsigned dstFormatSize = vertexFormat.bufferFormat().pixelBytes;

    mCurrentArrayBufferConversion[attribIndex].releaseRetainedBuffers(contextVk->getRenderer());

    size_t numVertices = GetVertexCount(srcBuffer, binding, srcFormatSize);
    if (numVertices == 0)
    {
        return angle::Result::Continue;
    }

    void *src = nullptr;
    ANGLE_TRY(srcBuffer->mapImpl(contextVk, &src));
    const uint8_t *srcBytes = reinterpret_cast<const uint8_t *>(src);
    srcBytes += binding.getOffset();
    ASSERT(GetVertexInputAlignment(vertexFormat) <= kMaxVertexFormatAlignment);
    ANGLE_TRY(StreamVertexData(contextVk, &mCurrentArrayBufferConversion[attribIndex], srcBytes,
                               numVertices * dstFormatSize, 0, numVertices, binding.getStride(),
                               vertexFormat.vertexLoadFunction, &mCurrentArrayBuffers[attribIndex],
                               &mCurrentArrayBufferOffsets[attribIndex]));
    ANGLE_TRY(srcBuffer->unmapImpl(contextVk));

    mCurrentArrayBufferHandles[attribIndex] =
        mCurrentArrayBuffers[attribIndex]->getBuffer().getHandle();
    mCurrentArrayBufferConversionCanRelease[attribIndex] = true;

    return angle::Result::Continue;
}

ANGLE_INLINE void VertexArrayVk::ensureConversionReleased(RendererVk *renderer, size_t attribIndex)
{
    if (mCurrentArrayBufferConversionCanRelease[attribIndex])
    {
        mCurrentArrayBufferConversion[attribIndex].release(renderer);
        mCurrentArrayBufferConversionCanRelease[attribIndex] = false;
    }
}

angle::Result VertexArrayVk::syncState(const gl::Context *context,
                                       const gl::VertexArray::DirtyBits &dirtyBits,
                                       gl::VertexArray::DirtyAttribBitsArray *attribBits,
                                       gl::VertexArray::DirtyBindingBitsArray *bindingBits)
{
    ASSERT(dirtyBits.any());

    bool invalidateContext = false;

    ContextVk *contextVk = vk::GetImpl(context);

    // Rebuild current attribute buffers cache. This will fail horribly if the buffer changes.
    // TODO(jmadill): Handle buffer storage changes.
    const auto &attribs  = mState.getVertexAttributes();
    const auto &bindings = mState.getVertexBindings();

    for (size_t dirtyBit : dirtyBits)
    {
        switch (dirtyBit)
        {
            case gl::VertexArray::DIRTY_BIT_ELEMENT_ARRAY_BUFFER:
            {
                gl::Buffer *bufferGL = mState.getElementArrayBuffer();
                if (bufferGL)
                {
                    BufferVk *bufferVk         = vk::GetImpl(bufferGL);
                    mCurrentElementArrayBuffer = &bufferVk->getBuffer();
                }
                else
                {
                    mCurrentElementArrayBuffer = nullptr;
                }

                mCurrentElementArrayBufferOffset = 0;
                mLineLoopBufferFirstIndex.reset();
                mLineLoopBufferLastIndex.reset();
                contextVk->setIndexBufferDirty();
                mDirtyLineLoopTranslation = true;
                break;
            }

            case gl::VertexArray::DIRTY_BIT_ELEMENT_ARRAY_BUFFER_DATA:
                mLineLoopBufferFirstIndex.reset();
                mLineLoopBufferLastIndex.reset();
                mDirtyLineLoopTranslation = true;
                break;

#define ANGLE_VERTEX_DIRTY_ATTRIB_FUNC(INDEX)                                     \
    case gl::VertexArray::DIRTY_BIT_ATTRIB_0 + INDEX:                             \
        ANGLE_TRY(syncDirtyAttrib(contextVk, attribs[INDEX],                      \
                                  bindings[attribs[INDEX].bindingIndex], INDEX)); \
        invalidateContext = true;                                                 \
        (*attribBits)[INDEX].reset();                                             \
        break;

                ANGLE_VERTEX_INDEX_CASES(ANGLE_VERTEX_DIRTY_ATTRIB_FUNC)

#define ANGLE_VERTEX_DIRTY_BINDING_FUNC(INDEX)                                    \
    case gl::VertexArray::DIRTY_BIT_BINDING_0 + INDEX:                            \
        ANGLE_TRY(syncDirtyAttrib(contextVk, attribs[INDEX],                      \
                                  bindings[attribs[INDEX].bindingIndex], INDEX)); \
        invalidateContext = true;                                                 \
        (*bindingBits)[INDEX].reset();                                            \
        break;

                ANGLE_VERTEX_INDEX_CASES(ANGLE_VERTEX_DIRTY_BINDING_FUNC)

#define ANGLE_VERTEX_DIRTY_BUFFER_DATA_FUNC(INDEX)                                \
    case gl::VertexArray::DIRTY_BIT_BUFFER_DATA_0 + INDEX:                        \
        ANGLE_TRY(syncDirtyAttrib(contextVk, attribs[INDEX],                      \
                                  bindings[attribs[INDEX].bindingIndex], INDEX)); \
        break;

                ANGLE_VERTEX_INDEX_CASES(ANGLE_VERTEX_DIRTY_BUFFER_DATA_FUNC)

            default:
                UNREACHABLE();
                break;
        }
    }

    if (invalidateContext)
    {
        contextVk->invalidateVertexAndIndexBuffers();
    }

    return angle::Result::Continue;
}

ANGLE_INLINE void VertexArrayVk::setDefaultPackedInput(ContextVk *contextVk, size_t attribIndex)
{
    contextVk->onVertexAttributeChange(attribIndex, 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0);
}

angle::Result VertexArrayVk::syncDirtyAttrib(ContextVk *contextVk,
                                             const gl::VertexAttribute &attrib,
                                             const gl::VertexBinding &binding,
                                             size_t attribIndex)
{
    RendererVk *renderer               = contextVk->getRenderer();
    bool anyVertexBufferConvertedOnGpu = false;

    if (attrib.enabled)
    {
        gl::Buffer *bufferGL           = binding.getBuffer().get();
        const vk::Format &vertexFormat = renderer->getFormat(GetVertexFormatID(attrib));
        GLuint stride;

        if (bufferGL)
        {
            BufferVk *bufferVk               = vk::GetImpl(bufferGL);
            const angle::Format &angleFormat = vertexFormat.angleFormat();
            bool bindingIsAligned            = BindingIsAligned(binding, angleFormat, attrib.size);

            if (vertexFormat.vertexLoadRequiresConversion || !bindingIsAligned)
            {
                stride = vertexFormat.bufferFormat().pixelBytes;

                if (bindingIsAligned)
                {
                    ANGLE_TRY(convertVertexBufferGpu(contextVk, bufferVk, binding, attribIndex,
                                                     vertexFormat));
                    anyVertexBufferConvertedOnGpu = true;
                }
                else
                {
                    ANGLE_TRY(convertVertexBufferCpu(contextVk, bufferVk, binding, attribIndex,
                                                     vertexFormat));
                }
            }
            else
            {
                if (bufferVk->getSize() == 0)
                {
                    mCurrentArrayBuffers[attribIndex] = &mTheNullBuffer;
                    mCurrentArrayBufferHandles[attribIndex] =
                        mTheNullBuffer.getBuffer().getHandle();
                    mCurrentArrayBufferOffsets[attribIndex] = 0;
                    stride                                  = 0;
                }
                else
                {
                    mCurrentArrayBuffers[attribIndex] = &bufferVk->getBuffer();
                    mCurrentArrayBufferHandles[attribIndex] =
                        bufferVk->getBuffer().getBuffer().getHandle();
                    mCurrentArrayBufferOffsets[attribIndex] = binding.getOffset();
                    stride                                  = binding.getStride();
                }

                ensureConversionReleased(renderer, attribIndex);
            }
        }
        else
        {
            mCurrentArrayBuffers[attribIndex]       = &mTheNullBuffer;
            mCurrentArrayBufferHandles[attribIndex] = mTheNullBuffer.getBuffer().getHandle();
            mCurrentArrayBufferOffsets[attribIndex] = 0;
            stride                                  = vertexFormat.bufferFormat().pixelBytes;
            ensureConversionReleased(renderer, attribIndex);
        }

        contextVk->onVertexAttributeChange(attribIndex, stride, binding.getDivisor(),
                                           vertexFormat.vkBufferFormat, attrib.relativeOffset);
    }
    else
    {
        contextVk->invalidateDefaultAttribute(attribIndex);

        // These will be filled out by the ContextVk.
        mCurrentArrayBuffers[attribIndex]       = &mTheNullBuffer;
        mCurrentArrayBufferHandles[attribIndex] = mTheNullBuffer.getBuffer().getHandle();
        mCurrentArrayBufferOffsets[attribIndex] = 0;

        setDefaultPackedInput(contextVk, attribIndex);
        ensureConversionReleased(renderer, attribIndex);
    }

    if (anyVertexBufferConvertedOnGpu && renderer->getFeatures().flushAfterVertexConversion)
    {
        ANGLE_TRY(contextVk->flushImpl());
    }

    return angle::Result::Continue;
}

angle::Result VertexArrayVk::updateClientAttribs(const gl::Context *context,
                                                 GLint firstVertex,
                                                 GLsizei vertexOrIndexCount,
                                                 GLsizei instanceCount,
                                                 gl::DrawElementsType indexTypeOrInvalid,
                                                 const void *indices)
{
    ContextVk *contextVk                    = vk::GetImpl(context);
    const gl::AttributesMask &clientAttribs = context->getStateCache().getActiveClientAttribsMask();

    ASSERT(clientAttribs.any());

    GLint startVertex;
    size_t vertexCount;
    ANGLE_TRY(GetVertexRangeInfo(context, firstVertex, vertexOrIndexCount, indexTypeOrInvalid,
                                 indices, 0, &startVertex, &vertexCount));

    RendererVk *renderer = contextVk->getRenderer();
    mDynamicVertexData.releaseRetainedBuffers(renderer);

    const auto &attribs  = mState.getVertexAttributes();
    const auto &bindings = mState.getVertexBindings();

    // TODO(fjhenigman): When we have a bunch of interleaved attributes, they end up
    // un-interleaved, wasting space and copying time.  Consider improving on that.
    for (size_t attribIndex : clientAttribs)
    {
        const gl::VertexAttribute &attrib = attribs[attribIndex];
        const gl::VertexBinding &binding  = bindings[attrib.bindingIndex];
        ASSERT(attrib.enabled && binding.getBuffer().get() == nullptr);

        const vk::Format &vertexFormat = renderer->getFormat(GetVertexFormatID(attrib));
        GLuint stride                  = vertexFormat.bufferFormat().pixelBytes;

        ASSERT(GetVertexInputAlignment(vertexFormat) <= kMaxVertexFormatAlignment);

        const uint8_t *src = static_cast<const uint8_t *>(attrib.pointer);
        if (binding.getDivisor() > 0)
        {
            // instanced attrib
            size_t count           = UnsignedCeilDivide(instanceCount, binding.getDivisor());
            size_t bytesToAllocate = count * stride;

            ANGLE_TRY(StreamVertexData(contextVk, &mDynamicVertexData, src, bytesToAllocate, 0,
                                       count, binding.getStride(), vertexFormat.vertexLoadFunction,
                                       &mCurrentArrayBuffers[attribIndex],
                                       &mCurrentArrayBufferOffsets[attribIndex]));
        }
        else
        {
            // Allocate space for startVertex + vertexCount so indexing will work.  If we don't
            // start at zero all the indices will be off.
            // Only vertexCount vertices will be used by the upcoming draw so that is all we copy.
            size_t bytesToAllocate = (startVertex + vertexCount) * stride;
            src += startVertex * binding.getStride();
            size_t destOffset = startVertex * stride;

            ANGLE_TRY(StreamVertexData(
                contextVk, &mDynamicVertexData, src, bytesToAllocate, destOffset, vertexCount,
                binding.getStride(), vertexFormat.vertexLoadFunction,
                &mCurrentArrayBuffers[attribIndex], &mCurrentArrayBufferOffsets[attribIndex]));
        }

        mCurrentArrayBufferHandles[attribIndex] =
            mCurrentArrayBuffers[attribIndex]->getBuffer().getHandle();
    }

    return angle::Result::Continue;
}

angle::Result VertexArrayVk::handleLineLoop(ContextVk *contextVk,
                                            GLint firstVertex,
                                            GLsizei vertexOrIndexCount,
                                            gl::DrawElementsType indexTypeOrInvalid,
                                            const void *indices)
{
    if (indexTypeOrInvalid != gl::DrawElementsType::InvalidEnum)
    {
        // Handle GL_LINE_LOOP drawElements.
        if (mDirtyLineLoopTranslation)
        {
            gl::Buffer *elementArrayBuffer = mState.getElementArrayBuffer();

            if (!elementArrayBuffer)
            {
                ANGLE_TRY(mLineLoopHelper.streamIndices(
                    contextVk, indexTypeOrInvalid, vertexOrIndexCount,
                    reinterpret_cast<const uint8_t *>(indices), &mCurrentElementArrayBuffer,
                    &mCurrentElementArrayBufferOffset));
            }
            else
            {
                // When using an element array buffer, 'indices' is an offset to the first element.
                intptr_t offset                = reinterpret_cast<intptr_t>(indices);
                BufferVk *elementArrayBufferVk = vk::GetImpl(elementArrayBuffer);
                ANGLE_TRY(mLineLoopHelper.getIndexBufferForElementArrayBuffer(
                    contextVk, elementArrayBufferVk, indexTypeOrInvalid, vertexOrIndexCount, offset,
                    &mCurrentElementArrayBuffer, &mCurrentElementArrayBufferOffset));
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
            contextVk, clampedVertexCount, firstVertex, &mCurrentElementArrayBuffer,
            &mCurrentElementArrayBufferOffset));

        mLineLoopBufferFirstIndex = firstVertex;
        mLineLoopBufferLastIndex  = lastVertex;
    }

    return angle::Result::Continue;
}

angle::Result VertexArrayVk::updateIndexTranslation(ContextVk *contextVk,
                                                    GLsizei indexCount,
                                                    gl::DrawElementsType type,
                                                    const void *indices)
{
    ASSERT(type != gl::DrawElementsType::InvalidEnum);

    RendererVk *renderer = contextVk->getRenderer();
    gl::Buffer *glBuffer = mState.getElementArrayBuffer();

    if (!glBuffer)
    {
        ANGLE_TRY(streamIndexData(contextVk, type, indexCount, indices, &mDynamicIndexData));
    }
    else if (renderer->getFormat(angle::FormatID::R16_UINT).vkSupportsStorageBuffer)
    {
        BufferVk *bufferVk = vk::GetImpl(glBuffer);

        ASSERT(type == gl::DrawElementsType::UnsignedByte);

        intptr_t offsetIntoSrcData = reinterpret_cast<intptr_t>(indices);
        size_t srcDataSize         = static_cast<size_t>(bufferVk->getSize()) - offsetIntoSrcData;

        mTranslatedByteIndexData.releaseRetainedBuffers(renderer);

        ANGLE_TRY(mTranslatedByteIndexData.allocate(contextVk, sizeof(GLushort) * srcDataSize,
                                                    nullptr, nullptr,
                                                    &mCurrentElementArrayBufferOffset, nullptr));
        mCurrentElementArrayBuffer = mTranslatedByteIndexData.getCurrentBuffer();

        vk::BufferHelper *dest = mTranslatedByteIndexData.getCurrentBuffer();
        vk::BufferHelper *src  = &bufferVk->getBuffer();

        ANGLE_TRY(src->initBufferView(contextVk, renderer->getFormat(angle::FormatID::R8_UINT)));
        ANGLE_TRY(dest->initBufferView(contextVk, renderer->getFormat(angle::FormatID::R16_UINT)));

        // Copy relevant section of the source into destination at allocated offset.  Note that the
        // offset returned by allocate() above is in bytes, while our allocated array is of
        // GLushorts.
        UtilsVk::CopyParameters params = {};
        params.destOffset =
            static_cast<size_t>(mCurrentElementArrayBufferOffset) / sizeof(GLushort);
        params.srcOffset = offsetIntoSrcData;
        params.size      = srcDataSize;

        // Note: this is a copy, which implicitly converts between formats.  Once support for
        // primitive restart is added, a specialized shader is likely needed to special case 0xFF ->
        // 0xFFFF.
        ANGLE_TRY(renderer->getUtils().copyBuffer(contextVk, dest, src, params));
    }
    else
    {
        // If it's not possible to convert the buffer with compute, opt for a CPU read back for now.
        // TODO(syoussefi): R8G8B8A8_UINT is required to have storage texel buffer support, so a
        // specialized shader code can be made to read two ubyte indices and output them in R and B
        // (or A and G based on endianness?) with 0 on the other channels.  If specialized, we might
        // as well support the ubyte to ushort case with correct handling of primitive restart.
        // http://anglebug.com/3003

        TRACE_EVENT0("gpu.angle", "VertexArrayVk::updateIndexTranslation");
        // Needed before reading buffer or we could get stale data.
        ANGLE_TRY(contextVk->finishImpl());

        ASSERT(type == gl::DrawElementsType::UnsignedByte);
        // Unsigned bytes don't have direct support in Vulkan so we have to expand the
        // memory to a GLushort.
        BufferVk *bufferVk   = vk::GetImpl(glBuffer);
        void *srcDataMapping = nullptr;
        ASSERT(!glBuffer->isMapped());
        ANGLE_TRY(bufferVk->mapImpl(contextVk, &srcDataMapping));
        uint8_t *srcData           = static_cast<uint8_t *>(srcDataMapping);
        intptr_t offsetIntoSrcData = reinterpret_cast<intptr_t>(indices);
        srcData += offsetIntoSrcData;

        ANGLE_TRY(streamIndexData(contextVk, type,
                                  static_cast<size_t>(bufferVk->getSize()) - offsetIntoSrcData,
                                  srcData, &mTranslatedByteIndexData));

        ANGLE_TRY(bufferVk->unmapImpl(contextVk));
    }

    return angle::Result::Continue;
}

void VertexArrayVk::updateDefaultAttrib(ContextVk *contextVk,
                                        size_t attribIndex,
                                        VkBuffer bufferHandle,
                                        uint32_t offset)
{
    if (!mState.getEnabledAttributesMask().test(attribIndex))
    {
        mCurrentArrayBufferHandles[attribIndex] = bufferHandle;
        mCurrentArrayBufferOffsets[attribIndex] = offset;
        mCurrentArrayBuffers[attribIndex]       = nullptr;

        setDefaultPackedInput(contextVk, attribIndex);
    }
}
}  // namespace rx
