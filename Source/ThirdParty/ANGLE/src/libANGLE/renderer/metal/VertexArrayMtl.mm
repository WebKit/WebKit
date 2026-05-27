//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// VertexArrayMtl.mm:
//    Implements the class methods for VertexArrayMtl.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "libANGLE/renderer/metal/VertexArrayMtl.h"

#include <TargetConditionals.h>

#include "common/mathutil.h"
#include "libANGLE/ErrorStrings.h"
#include "libANGLE/renderer/metal/BufferMtl.h"
#include "libANGLE/renderer/metal/ContextMtl.h"
#include "libANGLE/renderer/metal/DisplayMtl.h"
#include "libANGLE/renderer/metal/mtl_format_utils.h"

#include "common/debug.h"
#include "common/span_util.h"
#include "common/utilities.h"

namespace rx
{
namespace
{
constexpr size_t kDynamicIndexDataSize = 1024 * 8;

angle::Result StreamVertexData(ContextMtl *contextMtl,
                               mtl::BufferPool *dynamicBuffer,
                               const uint8_t *sourceData,
                               size_t bytesToAllocate,
                               size_t destOffset,
                               size_t vertexCount,
                               size_t stride,
                               VertexCopyFunction vertexLoadFunction,
                               mtl::BufferSlice *outBuffer)
{
    ANGLE_CHECK(contextMtl, vertexLoadFunction, gl::err::kInternalError, GL_INVALID_OPERATION);
    angle::Span<uint8_t> newBufferData;
    mtl::BufferSlice newBuffer;
    ANGLE_TRY(dynamicBuffer->allocate(contextMtl, bytesToAllocate, &newBufferData, &newBuffer));
    vertexLoadFunction(sourceData, stride, vertexCount, newBufferData.subspan(destOffset).data());

    ANGLE_TRY(dynamicBuffer->commit(contextMtl));
    *outBuffer = std::move(newBuffer);
    return angle::Result::Continue;
}

template <typename SizeT>
const mtl::VertexFormat &GetVertexConversionFormat(ContextMtl *contextMtl,
                                                   angle::FormatID originalFormat,
                                                   SizeT *strideOut)
{
    // Convert to tightly packed format
    const mtl::VertexFormat &packedFormat = contextMtl->getVertexFormat(originalFormat, true);
    *strideOut                            = packedFormat.actualAngleFormat().pixelBytes;
    return packedFormat;
}

void StreamIndexData(ContextMtl *contextMtl,
                     gl::DrawElementsType indexType,
                     size_t indexCount,
                     angle::Span<const uint8_t> source,
                     angle::Span<uint8_t> destination,
                     bool primitiveRestartEnabled)
{
    RELEASE_ASSERT(source.size() <= destination.size());
    if (indexType == gl::DrawElementsType::UnsignedByte)
    {
        // Unsigned bytes don't have direct support in Metal so we have to expand the
        // memory to a GLushort.
        angle::Span<uint16_t> expanded = reinterpret_span<uint16_t>(destination);
        auto s                         = source.begin();
        auto sourceEnd                 = source.end();
        auto d                         = expanded.begin();
        if (primitiveRestartEnabled)
        {
            for (; s != sourceEnd; s++, d++)
            {
                uint8_t value = *s;
                *d            = value == 0xFF ? 0xFFFF : static_cast<uint16_t>(value);
            }
        }
        else
        {
            for (; s != sourceEnd; s++, d++)
            {
                *d = static_cast<uint16_t>(*s);
            }
        }  // if (primitiveRestartEnabled)
    }
    else
    {
        SpanMemcpy(destination, source);
    }
}

size_t GetVertexCount(BufferMtl *srcBuffer,
                      const gl::VertexBinding &binding,
                      uint32_t srcFormatSize)
{
    GLintptr bindingOffset = binding.getOffset();
    if (bindingOffset < 0)
        return 0;

    // Bytes usable for vertex data.
    angle::CheckedNumeric<size_t> bytes = srcBuffer->size();
    bytes -= static_cast<size_t>(bindingOffset);
    // Count the last vertex.  It may occupy less than a full stride.
    bytes -= srcFormatSize;
    if (!bytes.IsValid())
        return 0;

    // Count how many strides fit remaining space.
    size_t effectiveStride = binding.getStride() > 0 ? binding.getStride() : srcFormatSize;
    return 1 + static_cast<size_t>(bytes.ValueOrDie()) / effectiveStride;
}

size_t GetVertexCountWithConversion(BufferMtl *srcBuffer,
                                    VertexConversionBufferMtl *conversionBuffer,
                                    const gl::VertexBinding &binding,
                                    uint32_t srcFormatSize)
{
    // Use the smaller of the conversion buffer offset and the binding offset.
    GLintptr bindingOffset = binding.getOffset();
    if (bindingOffset < 0)
        return 0;
    size_t effectiveOffset =
        std::min(conversionBuffer->offset, static_cast<size_t>(bindingOffset));

    // Bytes usable for vertex data.
    angle::CheckedNumeric<size_t> bytes = srcBuffer->size();
    bytes -= effectiveOffset;
    // Count the last vertex.  It may occupy less than a full stride.
    bytes -= srcFormatSize;
    if (!bytes.IsValid())
        return 0;

    // Count how many strides fit remaining space.
    size_t effectiveStride = binding.getStride() > 0 ? binding.getStride() : srcFormatSize;
    return 1 + static_cast<size_t>(bytes.ValueOrDie()) / effectiveStride;
<<<<<<< HEAD
=======
}
inline size_t GetIndexCount(BufferMtl *srcBuffer, size_t offset, gl::DrawElementsType indexType)
{
    size_t elementSize = gl::GetDrawElementsTypeSize(indexType);
    return (srcBuffer->size() - offset) / elementSize;
>>>>>>> 845230f29cf4 ([ANGLE] Metal backend: Fix crash on OOB vertex attribute offset in syncDirtyAttrib)
}

inline void SetDefaultVertexBufferLayout(mtl::VertexBufferLayoutDesc *layout)
{
    layout->stepFunction = mtl::kVertexStepFunctionInvalid;
    layout->stepRate     = 0;
    layout->stride       = 0;
}

inline MTLVertexFormat GetCurrentAttribFormat(GLenum type)
{
    switch (type)
    {
        case GL_INT:
        case GL_INT_VEC2:
        case GL_INT_VEC3:
        case GL_INT_VEC4:
            return MTLVertexFormatInt4;
        case GL_UNSIGNED_INT:
        case GL_UNSIGNED_INT_VEC2:
        case GL_UNSIGNED_INT_VEC3:
        case GL_UNSIGNED_INT_VEC4:
            return MTLVertexFormatUInt4;
        default:
            return MTLVertexFormatFloat4;
    }
}

}  // namespace

// VertexArrayMtl implementation
VertexArrayMtl::VertexArrayMtl(const gl::VertexArrayState &state,
                               const gl::VertexArrayBuffers &vertexArrayBuffers,
                               ContextMtl *context)
    : VertexArrayImpl(state, vertexArrayBuffers),
      mDefaultFloatVertexFormat(
          context->getVertexFormat(angle::FormatID::R32G32B32A32_FLOAT, false))
{
    reset(context);

    mDynamicVertexData.initialize(context, 0, mtl::kVertexAttribBufferStrideAlignment,
                                  /** maxBuffers */ 10 * mtl::kMaxVertexAttribs);

    mDynamicIndexData.initialize(context, kDynamicIndexDataSize, mtl::kIndexBufferOffsetAlignment,
                                 0);
}
VertexArrayMtl::~VertexArrayMtl() {}

void VertexArrayMtl::destroy(const gl::Context *context)
{
    ContextMtl *contextMtl = mtl::GetImpl(context);

    reset(contextMtl);

    mDynamicVertexData.destroy(contextMtl);
    mDynamicIndexData.destroy(contextMtl);
}

void VertexArrayMtl::reset(ContextMtl *context)
{
    for (BufferHolderMtl *&buffer : mCurrentArrayBuffers)
    {
        buffer = nullptr;
    }
    for (size_t &offset : mCurrentArrayBufferOffsets)
    {
        offset = 0;
    }
    for (GLuint &stride : mCurrentArrayBufferStrides)
    {
        stride = 0;
    }
    for (const mtl::VertexFormat *&format : mCurrentArrayBufferFormats)
    {
        format = &mDefaultFloatVertexFormat;
    }

    for (size_t &inlineDataSize : mCurrentArrayInlineDataSizes)
    {
        inlineDataSize = 0;
    }

    for (angle::MemoryBuffer &convertedClientArray : mConvertedClientSmallArrays)
    {
        convertedClientArray.clear();
    }

    for (const uint8_t *&clientPointer : mCurrentArrayInlineDataPointers)
    {
        clientPointer = nullptr;
    }

    if (context->getDisplay()->getFeatures().allowInlineConstVertexData.enabled)
    {
        mInlineDataMaxSize = mtl::kInlineConstDataMaxSize;
    }
    else
    {
        mInlineDataMaxSize = 0;
    }

    mVertexArrayDirty = true;
}

angle::Result VertexArrayMtl::syncState(const gl::Context *context,
                                        const gl::VertexArray::DirtyBits &dirtyBits,
                                        gl::VertexArray::DirtyAttribBitsArray *attribBits,
                                        gl::VertexArray::DirtyBindingBitsArray *bindingBits)
{
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
                mVertexDataDirty = true;
                break;
            }

#define ANGLE_VERTEX_DIRTY_ATTRIB_FUNC(INDEX)                                                     \
    case gl::VertexArray::DIRTY_BIT_ATTRIB_0 + INDEX:                                             \
        ANGLE_TRY(syncDirtyAttrib(context, attribs[INDEX], bindings[attribs[INDEX].bindingIndex], \
                                  INDEX));                                                        \
        mVertexArrayDirty = true;                                                                 \
        (*attribBits)[INDEX].reset();                                                             \
        break;

                ANGLE_VERTEX_INDEX_CASES(ANGLE_VERTEX_DIRTY_ATTRIB_FUNC)

#define ANGLE_VERTEX_DIRTY_BINDING_FUNC(INDEX)                                                    \
    case gl::VertexArray::DIRTY_BIT_BINDING_0 + INDEX:                                            \
        ANGLE_TRY(syncDirtyAttrib(context, attribs[INDEX], bindings[attribs[INDEX].bindingIndex], \
                                  INDEX));                                                        \
        mVertexArrayDirty = true;                                                                 \
        (*bindingBits)[INDEX].reset();                                                            \
        break;

                ANGLE_VERTEX_INDEX_CASES(ANGLE_VERTEX_DIRTY_BINDING_FUNC)

#define ANGLE_VERTEX_DIRTY_BUFFER_DATA_FUNC(INDEX)                                                \
    case gl::VertexArray::DIRTY_BIT_BUFFER_DATA_0 + INDEX:                                        \
        ANGLE_TRY(syncDirtyAttrib(context, attribs[INDEX], bindings[attribs[INDEX].bindingIndex], \
                                  INDEX));                                                        \
        mVertexDataDirty = true;                                                                  \
        break;

                ANGLE_VERTEX_INDEX_CASES(ANGLE_VERTEX_DIRTY_BUFFER_DATA_FUNC)

            default:
                UNREACHABLE();
                break;
        }
    }

    return angle::Result::Continue;
}

// vertexDescChanged is both input and output, the input value if is true, will force new
// mtl::VertexDesc to be returned via vertexDescOut. This typically happens when active shader
// program is changed.
// Otherwise, it is only returned when the vertex array is dirty.
angle::Result VertexArrayMtl::setupDraw(const gl::Context *glContext,
                                        mtl::RenderCommandEncoder *cmdEncoder,
                                        bool *vertexDescChanged,
                                        mtl::VertexDesc *vertexDescOut)
{
    // NOTE(hqle): consider only updating dirty attributes
    bool dirty = mVertexArrayDirty || *vertexDescChanged;

    if (dirty)
    {

        mVertexArrayDirty = false;
        mEmulatedInstanceAttribs.clear();

        const gl::ProgramExecutable *executable = glContext->getState().getProgramExecutable();
        const gl::AttributesMask &programActiveAttribsMask =
            executable->getActiveAttribLocationsMask();

        const std::vector<gl::VertexAttribute> &attribs = mState.getVertexAttributes();
        const std::vector<gl::VertexBinding> &bindings  = mState.getVertexBindings();

        mtl::VertexDesc &desc = *vertexDescOut;

        desc.numAttribs       = mtl::kMaxVertexAttribs;
        desc.numBufferLayouts = mtl::kMaxVertexAttribs;

        // Initialize the buffer layouts with constant step rate
        for (uint32_t b = 0; b < mtl::kMaxVertexAttribs; ++b)
        {
            SetDefaultVertexBufferLayout(&desc.layouts[b]);
        }

        // Cache vertex shader input types
        std::array<uint8_t, mtl::kMaxVertexAttribs> currentAttribFormats{};
        for (auto &input : executable->getProgramInputs())
        {
            if (input.isBuiltIn())
            {
                continue;
            }

            ASSERT(input.getLocation() != -1);
            ASSERT(input.getLocation() < static_cast<int>(mtl::kMaxVertexAttribs));
            currentAttribFormats[input.getLocation()] = GetCurrentAttribFormat(input.getType());
        }
        MTLVertexFormat currentAttribFormat = MTLVertexFormatInvalid;

        for (uint32_t v = 0; v < mtl::kMaxVertexAttribs; ++v)
        {
            if (!programActiveAttribsMask.test(v))
            {
                desc.attributes[v].format      = MTLVertexFormatInvalid;
                desc.attributes[v].bufferIndex = 0;
                desc.attributes[v].offset      = 0;
                continue;
            }

            const auto &attrib               = attribs[v];
            const gl::VertexBinding &binding = bindings[attrib.bindingIndex];

            bool attribEnabled = attrib.enabled;
            if (attribEnabled &&
                !(mCurrentArrayBuffers[v] && mCurrentArrayBuffers[v]->getCurrentBuffer()) &&
                !mCurrentArrayInlineDataPointers[v])
            {
                // Disable it to avoid crash.
                attribEnabled = false;
            }

            if (currentAttribFormats[v] != MTLVertexFormatInvalid)
            {
                currentAttribFormat = static_cast<MTLVertexFormat>(currentAttribFormats[v]);
            }
            else
            {
                // This is a non-first matrix column
                ASSERT(currentAttribFormat != MTLVertexFormatInvalid);
            }

            if (!attribEnabled)
            {
                // Use default attribute
                desc.attributes[v].bufferIndex = mtl::kDefaultAttribsBindingIndex;
                desc.attributes[v].offset      = v * mtl::kDefaultAttributeSize;
                desc.attributes[v].format      = currentAttribFormat;
            }
            else
            {
                uint32_t bufferIdx    = mtl::kVboBindingIndexStart + v;
                uint32_t bufferOffset = static_cast<uint32_t>(mCurrentArrayBufferOffsets[v]);

                desc.attributes[v].format = mCurrentArrayBufferFormats[v]->metalFormat;

                desc.attributes[v].bufferIndex = bufferIdx;
                desc.attributes[v].offset      = 0;
                ASSERT((bufferOffset % mtl::kVertexAttribBufferStrideAlignment) == 0);

                ASSERT(bufferIdx < mtl::kMaxVertexAttribs);
                if (binding.getDivisor() == 0)
                {
                    desc.layouts[bufferIdx].stepFunction = MTLVertexStepFunctionPerVertex;
                    desc.layouts[bufferIdx].stepRate     = 1;
                }
                else
                {
                    desc.layouts[bufferIdx].stepFunction = MTLVertexStepFunctionPerInstance;
                    desc.layouts[bufferIdx].stepRate     = binding.getDivisor();
                }

                // Metal does not allow the sum of the buffer binding
                // offset and the vertex layout stride to be greater
                // than the buffer length.
                // In OpenGL, this is valid only when a draw call accesses just
                // one vertex, so just replace the stride with the format size.
                uint32_t stride = mCurrentArrayBufferStrides[v];
                if (mCurrentArrayBuffers[v])
                {
                    const size_t length = mCurrentArrayBuffers[v]->getCurrentBuffer()->size();
                    const size_t offset = mCurrentArrayBufferOffsets[v];
                    ASSERT(offset < length);
                    if (length - offset < stride)
                    {
                        stride = mCurrentArrayBufferFormats[v]->actualAngleFormat().pixelBytes;
                        ASSERT(stride % mtl::kVertexAttribBufferStrideAlignment == 0);
                    }
                }
                desc.layouts[bufferIdx].stride = stride;
            }
        }  // for (v)
    }

    if (dirty || mVertexDataDirty)
    {
        mVertexDataDirty                        = false;
        const gl::ProgramExecutable *executable = glContext->getState().getProgramExecutable();
        const gl::AttributesMask &programActiveAttribsMask =
            executable->getActiveAttribLocationsMask();

        for (uint32_t v = 0; v < mtl::kMaxVertexAttribs; ++v)
        {
            if (!programActiveAttribsMask.test(v))
            {
                continue;
            }
            uint32_t bufferIdx    = mtl::kVboBindingIndexStart + v;
            if (mCurrentArrayBuffers[v])
            {
                size_t bufferOffset = mCurrentArrayBufferOffsets[v];
                cmdEncoder->setVertexBuffer(mCurrentArrayBuffers[v]->getCurrentBuffer(),
                                            bufferOffset, bufferIdx);
            }
            else if (mCurrentArrayInlineDataPointers[v])
            {
                // No buffer specified, use the client memory directly as inline constant data
                ASSERT(mCurrentArrayInlineDataSizes[v] <= mInlineDataMaxSize);
                cmdEncoder->setVertexBytes(mCurrentArrayInlineDataPointers[v],
                                           mCurrentArrayInlineDataSizes[v], bufferIdx);
            }
        }
    }

    *vertexDescChanged = dirty;

    return angle::Result::Continue;
}

angle::Result VertexArrayMtl::updateClientAttribs(const gl::Context *context,
                                                  GLint firstVertex,
                                                  GLsizei vertexOrIndexCount,
                                                  GLsizei instanceCount,
                                                  gl::DrawElementsType indexTypeOrInvalid,
                                                  const void *indices)
{
    ContextMtl *contextMtl                  = mtl::GetImpl(context);
    const gl::AttributesMask &clientAttribs = context->getActiveClientAttribsMask();

    ASSERT(clientAttribs.any());

    GLint startVertex;
    size_t vertexCount;
    ANGLE_TRY(GetVertexRangeInfo(context, firstVertex, vertexOrIndexCount, indexTypeOrInvalid,
                                 indices, 0, &startVertex, &vertexCount));

    mDynamicVertexData.releaseInFlightBuffers(contextMtl);

    const std::vector<gl::VertexAttribute> &attribs = mState.getVertexAttributes();
    const std::vector<gl::VertexBinding> &bindings  = mState.getVertexBindings();

    for (size_t attribIndex : clientAttribs)
    {
        const gl::VertexAttribute &attrib = attribs[attribIndex];
        const gl::VertexBinding &binding  = bindings[attrib.bindingIndex];
        ASSERT(attrib.enabled && getVertexArrayBuffer(attrib.bindingIndex) == nullptr);

        // Source client memory pointer
        const uint8_t *src = static_cast<const uint8_t *>(attrib.pointer);
        ASSERT(src);

        GLint startElement;
        size_t elementCount;
        if (binding.getDivisor() == 0)
        {
            // Per vertex attribute
            startElement = startVertex;
            elementCount = vertexCount;
        }
        else
        {
            // Per instance attribute
            startElement = 0;
            elementCount = UnsignedCeilDivide(instanceCount, binding.getDivisor());
        }
        size_t bytesIntendedToUse = (startElement + elementCount) * binding.getStride();

        const mtl::VertexFormat &format = contextMtl->getVertexFormat(attrib.format->id, false);
        bool needStreaming              = format.actualFormatId != format.intendedFormatId ||
                             (binding.getStride() % mtl::kVertexAttribBufferStrideAlignment) != 0 ||
                             (binding.getStride() < format.actualAngleFormat().pixelBytes) ||
                             bytesIntendedToUse > mInlineDataMaxSize;

        if (!needStreaming)
        {
            // Data will be uploaded directly as inline constant data
            mCurrentArrayBuffers[attribIndex]            = nullptr;
            mCurrentArrayInlineDataPointers[attribIndex] = src;
            mCurrentArrayInlineDataSizes[attribIndex]    = bytesIntendedToUse;
            mCurrentArrayBufferOffsets[attribIndex]      = 0;
            mCurrentArrayBufferFormats[attribIndex]      = &format;
            mCurrentArrayBufferStrides[attribIndex]      = binding.getStride();
        }
        else
        {
            GLuint convertedStride;
            // Need to stream the client vertex data to a buffer.
            const mtl::VertexFormat &streamFormat =
                GetVertexConversionFormat(contextMtl, attrib.format->id, &convertedStride);

            // Allocate space for startElement + elementCount so indexing will work.  If we don't
            // start at zero all the indices will be off.
            // Only elementCount vertices will be used by the upcoming draw so that is all we copy.
            size_t bytesToAllocate = (startElement + elementCount) * convertedStride;
            src += startElement * binding.getStride();
            size_t destOffset = startElement * convertedStride;

            mCurrentArrayBufferFormats[attribIndex] = &streamFormat;
            mCurrentArrayBufferStrides[attribIndex] = convertedStride;

            if (bytesToAllocate <= mInlineDataMaxSize)
            {
                // If the data is small enough, use host memory instead of creating GPU buffer. To
                // avoid synchronizing access to GPU buffer that is still in use.
                angle::MemoryBuffer &convertedClientArray =
                    mConvertedClientSmallArrays[attribIndex];
                if (bytesToAllocate > convertedClientArray.size())
                {
                    ANGLE_CHECK_GL_ALLOC(contextMtl, convertedClientArray.resize(bytesToAllocate));
                }

                ASSERT(streamFormat.vertexLoadFunction);
                streamFormat.vertexLoadFunction(src, binding.getStride(), elementCount,
                                                convertedClientArray.data() + destOffset);

                mCurrentArrayBuffers[attribIndex]            = nullptr;
                mCurrentArrayInlineDataPointers[attribIndex] = convertedClientArray.data();
                mCurrentArrayInlineDataSizes[attribIndex]    = bytesToAllocate;
                mCurrentArrayBufferOffsets[attribIndex]      = 0;
            }
            else
            {
                // Stream the client data to a GPU buffer. Synchronization might happen if buffer is
                // in use.
                mDynamicVertexData.updateAlignment(contextMtl,
                                                   streamFormat.actualAngleFormat().pixelBytes);
                mtl::BufferSlice streamedBuffer;
                ANGLE_TRY(StreamVertexData(contextMtl, &mDynamicVertexData, src, bytesToAllocate,
                                           destOffset, elementCount, binding.getStride(),
                                           streamFormat.vertexLoadFunction, &streamedBuffer));
                if (contextMtl->getDisplay()->getFeatures().flushAfterStreamVertexData.enabled)
                {
                    // WaitUntilScheduled is needed for this workaround. NoWait does not have the
                    // needed effect.
                    contextMtl->flushCommandBuffer(mtl::WaitUntilScheduled);
                }

                mConvertedArrayBufferHolders[attribIndex].set(streamedBuffer.buffer());
                mCurrentArrayBufferOffsets[attribIndex] = streamedBuffer.offset();
                mCurrentArrayBuffers[attribIndex] = &mConvertedArrayBufferHolders[attribIndex];
            }
        }  // if (needStreaming)
    }

    mVertexArrayDirty = true;

    return angle::Result::Continue;
}

angle::Result VertexArrayMtl::syncDirtyAttrib(const gl::Context *glContext,
                                              const gl::VertexAttribute &attrib,
                                              const gl::VertexBinding &binding,
                                              size_t attribIndex)
{
    ContextMtl *contextMtl = mtl::GetImpl(glContext);
    ASSERT(mtl::kMaxVertexAttribs > attribIndex);
    mContentsObserverBindingsMask.reset(attrib.bindingIndex);

    if (attrib.enabled)
    {
        gl::Buffer *bufferGL            = getVertexArrayBuffer(attrib.bindingIndex);
        const mtl::VertexFormat &format = contextMtl->getVertexFormat(attrib.format->id, false);

        if (bufferGL)
        {
            BufferMtl *bufferMtl = mtl::GetImpl(bufferGL);
            // https://bugs.webkit.org/show_bug.cgi?id=236733
            // even non-converted buffers need to be observed for potential
            // data rebinds.
            mContentsObserverBindingsMask.set(attrib.bindingIndex);
            bool needConversion =
                format.actualFormatId != format.intendedFormatId ||
                (binding.getOffset() % mtl::kVertexAttribBufferStrideAlignment) != 0 ||
                (binding.getStride() < format.actualAngleFormat().pixelBytes) ||
                (binding.getStride() % mtl::kVertexAttribBufferStrideAlignment) != 0;

            unsigned srcFormatSize = format.actualAngleFormat().pixelBytes;
            size_t numVertices = GetVertexCount(bufferMtl, binding, srcFormatSize);
            if (numVertices == 0)
            {
                // Out of bound buffer access, can return any values.
                // See KHR_robust_buffer_access_behavior.
                mCurrentArrayBuffers[attribIndex]       = bufferMtl;
                mCurrentArrayBufferFormats[attribIndex] = &format;
                mCurrentArrayBufferOffsets[attribIndex] = 0;
                mCurrentArrayBufferStrides[attribIndex] = 16;
            }
            else if (needConversion)
            {
                ANGLE_TRY(convertVertexBuffer(glContext, bufferMtl, binding, attribIndex, format));
            }
            else
            {
                mCurrentArrayBuffers[attribIndex]       = bufferMtl;
                mCurrentArrayBufferFormats[attribIndex] = &format;
                mCurrentArrayBufferOffsets[attribIndex] = binding.getOffset();
                mCurrentArrayBufferStrides[attribIndex] = binding.getStride();
            }
        }
        else
        {
            // ContextMtl must feed the client data using updateClientAttribs()
        }
    }
    else
    {
        // Use default attribute value. Handled in setupDraw().
        mCurrentArrayBuffers[attribIndex]       = nullptr;
        mCurrentArrayBufferOffsets[attribIndex] = 0;
        mCurrentArrayBufferStrides[attribIndex] = 0;
        mCurrentArrayBufferFormats[attribIndex] =
            &contextMtl->getVertexFormat(angle::FormatID::NONE, false);
    }

    return angle::Result::Continue;
}

angle::Result VertexArrayMtl::getIndexBuffer(const gl::Context *context,
                                             gl::DrawElementsType type,
                                             size_t count,
                                             const void *indices,
                                             mtl::BufferSlice *outIdxBuffer,
                                             gl::DrawElementsType *indexTypeOut)
{
    const gl::Buffer *glElementArrayBuffer = getElementArrayBuffer();

    size_t convertedOffset = reinterpret_cast<size_t>(indices);
    if (!glElementArrayBuffer)
    {
        ANGLE_TRY(streamIndexBufferFromClient(context, type, count, indices, outIdxBuffer));
    }
    else
    {
        bool needConversion = type == gl::DrawElementsType::UnsignedByte;
        if (needConversion)
        {
            ANGLE_TRY(convertIndexBuffer(context, type, convertedOffset, outIdxBuffer));
        }
        else
        {
            // No conversion needed:
            BufferMtl *bufferMtl     = mtl::GetImpl(glElementArrayBuffer);
            mtl::BufferRef bufferRef = bufferMtl->getCurrentBuffer();
            *outIdxBuffer            = mtl::BufferSlice(bufferRef).subslice(convertedOffset);
        }
    }

    *indexTypeOut = type;
    if (type == gl::DrawElementsType::UnsignedByte)
    {
        // This buffer is already converted to ushort indices above
        *indexTypeOut = gl::DrawElementsType::UnsignedShort;
    }

    return angle::Result::Continue;
}

std::vector<DrawCommandRange> VertexArrayMtl::getDrawIndices(const gl::Context *glContext,
                                                             gl::DrawElementsType originalIndexType,
                                                             gl::DrawElementsType indexType,
                                                             gl::PrimitiveMode originalMode,
                                                             gl::PrimitiveMode mode,
                                                             mtl::BufferRef clientBuffer,
                                                             uint32_t indexCount,
                                                             const void *originalOffsetOrClientPtr,
                                                             size_t offsetInBytes)
{
    ContextMtl *contextMtl = mtl::GetImpl(glContext);
    std::vector<DrawCommandRange> drawCommands;
    // The indexed draw needs to be split to separate draw commands in case primitive restart is
    // enabled and the drawn primitive supports primitive restart. Otherwise the whole indexed draw
    // can be sent as one draw command.
    bool isSimpleType = mode == gl::PrimitiveMode::Points || mode == gl::PrimitiveMode::Lines ||
                        mode == gl::PrimitiveMode::Triangles;
    bool indicesRewritten = (originalMode != mode);
    if ((!isSimpleType && !indicesRewritten) || !glContext->getState().isPrimitiveRestartEnabled())
    {
        drawCommands.push_back({indexCount, offsetInBytes});
        return drawCommands;
    }

    const std::vector<IndexRange> *restartIndices;
    std::vector<IndexRange> clientIndexRange;
    const gl::Buffer *glElementArrayBuffer = getElementArrayBuffer();
    size_t startIndex;
    if (glElementArrayBuffer)
    {
        BufferMtl *idxBuffer = mtl::GetImpl(glElementArrayBuffer);
        restartIndices       = &idxBuffer->getRestartIndices(contextMtl, originalIndexType);
        // restartIndices is relative to the original element buffer, hence we use the original
        // offset to calculate startIndex.
        size_t originalOffsetInBytes = reinterpret_cast<size_t>(originalOffsetOrClientPtr);
        startIndex = originalOffsetInBytes / gl::GetDrawElementsTypeSize(originalIndexType);
    }
    else
    {
        clientIndexRange =
            BufferMtl::getRestartIndicesFromClientData(contextMtl, indexType, clientBuffer);
        restartIndices = &clientIndexRange;
        // For client indices, restartIndices is relative to the clientBuffer, we use offsetInBytes
        // to calculate startIndex.
        startIndex = offsetInBytes / gl::GetDrawElementsTypeSize(indexType);
    }

    uint32_t nIndicesPerPrimitive;
    // If the indices were rewritten to fix the provoking vertex convention, the index buffer
    // might have been expanded (e.g. from TriangleStrip to Triangles). The restart indices
    // we found earlier are based on the original unexpanded buffer. We must transform the slices
    // we find to match the layout of the expanded rewritten buffer.
    uint32_t factor       = 1;
    uint32_t stripExclude = 0;

    if (indicesRewritten)
    {
        switch (mode)
        {
            case gl::PrimitiveMode::Lines:
                if (originalMode == gl::PrimitiveMode::LineStrip ||
                    originalMode == gl::PrimitiveMode::LineLoop)
                {
                    nIndicesPerPrimitive = 1;
                    // LineStrip/LineLoop to Lines expansion:
                    // N indices produce max(N-1, 0) lines.
                    // Rewritten buffer contains max(N-1, 0) * 2 indices.
                    factor       = 2;
                    stripExclude = 1;
                }
                else
                {
                    UNREACHABLE();
                    return drawCommands;
                }
                break;
            case gl::PrimitiveMode::Triangles:
                if (originalMode == gl::PrimitiveMode::TriangleStrip ||
                    originalMode == gl::PrimitiveMode::TriangleFan)
                {
                    nIndicesPerPrimitive = 1;
                    // TriangleStrip/TriangleFan to Triangles expansion:
                    // N indices produce max(N-2, 0) triangles.
                    // Rewritten buffer contains max(N-2, 0) * 3 indices.
                    factor       = 3;
                    stripExclude = 2;
                }
                else
                {
                    UNREACHABLE();
                    return drawCommands;
                }
                break;
            default:
                UNREACHABLE();
                return drawCommands;
        }
    }
    else
    {
        ASSERT(originalMode == mode);
        switch (mode)
        {
            case gl::PrimitiveMode::Points:
                nIndicesPerPrimitive = 1;
                break;
            case gl::PrimitiveMode::Lines:
                nIndicesPerPrimitive = 2;
                break;
            case gl::PrimitiveMode::Triangles:
                nIndicesPerPrimitive = 3;
                break;
            default:
                UNREACHABLE();
                return drawCommands;
        }
    }

    const GLuint indexTypeBytes = gl::GetDrawElementsTypeSize(indexType);
    uint32_t indicesLeft        = indexCount;
    size_t currentIndexOffset   = startIndex;

    auto addDrawCommand = [&](uint32_t count, size_t elementOffset) {
        // Skip slices that don't contain enough indices to form a single primitive.
        if (count > stripExclude)
        {
            // Scale the count to account for the expansion factor and stripped indices.
            uint32_t transformedCount = (count - stripExclude) * factor;
            // Scale the offset to account for the expansion factor.
            size_t transformedOffset =
                offsetInBytes + (elementOffset - startIndex) * (size_t)factor * indexTypeBytes;
            drawCommands.push_back({transformedCount, transformedOffset});
        }
    };

    for (auto &range : *restartIndices)
    {
        if (range.restartBegin > currentIndexOffset)
        {
            int64_t nIndicesInSlice =
                MIN(((int64_t)range.restartBegin - currentIndexOffset) -
                        ((int64_t)range.restartBegin - currentIndexOffset) % nIndicesPerPrimitive,
                    indicesLeft);
            size_t restartSize = (range.restartEnd - range.restartBegin) + 1;
            if (nIndicesInSlice >= nIndicesPerPrimitive)
            {
                addDrawCommand((uint32_t)nIndicesInSlice, currentIndexOffset);
            }
            // Account for dropped indices due to incomplete primitives.
            size_t indicesUsed = ((range.restartBegin + restartSize) - currentIndexOffset);
            if (indicesLeft <= indicesUsed)
            {
                indicesLeft = 0;
            }
            else
            {
                indicesLeft -= indicesUsed;
            }
            currentIndexOffset = (size_t)(range.restartBegin + restartSize);
        }
        // If the initial offset into the index buffer is within a restart zone, move to the end of
        // the restart zone.
        else if (range.restartEnd >= currentIndexOffset)
        {
            size_t restartSize = (range.restartEnd - currentIndexOffset) + 1;
            if (indicesLeft <= restartSize)
            {
                indicesLeft = 0;
            }
            else
            {
                indicesLeft -= restartSize;
            }
            currentIndexOffset = (size_t)(currentIndexOffset + restartSize);
        }
    }
    if (indicesLeft >= nIndicesPerPrimitive)
    {
        addDrawCommand(indicesLeft, currentIndexOffset);
    }

    return drawCommands;
}

angle::Result VertexArrayMtl::convertIndexBuffer(const gl::Context *glContext,
                                                 gl::DrawElementsType indexType,
                                                 size_t offset,
                                                 mtl::BufferSlice *outIdxBuffer)
{
    size_t offsetModulo = offset % mtl::kIndexBufferOffsetAlignment;
    ASSERT(offsetModulo != 0 || indexType == gl::DrawElementsType::UnsignedByte);

    size_t alignedOffset = offset - offsetModulo;
    if (indexType == gl::DrawElementsType::UnsignedByte)
    {
        // Unsigned byte index will be promoted to unsigned short, thus double its offset.
        alignedOffset = alignedOffset << 1;
    }

    ContextMtl *contextMtl   = mtl::GetImpl(glContext);
    const gl::State &glState = glContext->getState();
    BufferMtl *idxBuffer     = mtl::GetImpl(getElementArrayBuffer());

    IndexConversionBufferMtl *conversion = idxBuffer->getIndexConversionBuffer(
        contextMtl, indexType, glState.isPrimitiveRestartEnabled(), offsetModulo);

    // Has the content of the buffer has changed since last conversion?
    if (!conversion->dirty)
    {
        // reuse the converted buffer
        *outIdxBuffer = conversion->buffer.subslice(alignedOffset);
        return angle::Result::Continue;
    }

    DisplayMtl *display = contextMtl->getDisplay();

    const size_t elementSize = gl::GetDrawElementsTypeSize(indexType);
    const size_t indexCount  = (idxBuffer->size() - offsetModulo) / elementSize;
    const size_t indexSize   = indexCount * elementSize;
    const size_t convertedIndexSize =
        indexType == gl::DrawElementsType::UnsignedByte ? indexSize * 2 : indexSize;
    conversion->bufferPool.releaseInFlightBuffers(contextMtl);
    mtl::BufferSlice converted;

    if ((!display->getFeatures().hasCheapRenderPass.enabled &&
         contextMtl->getRenderCommandEncoder()))
    {
        angle::Span<uint8_t> destination;
        ANGLE_TRY(conversion->bufferPool.allocate(contextMtl, convertedIndexSize, &destination,
                                                  &converted));

        // We shouldn't use GPU to convert when we are in a middle of a render pass.
        angle::Span<const uint8_t> source =
            idxBuffer->getBufferDataReadOnly(contextMtl, offsetModulo).first(indexSize);
        StreamIndexData(contextMtl, indexType, indexCount, source, destination,
                        glState.isPrimitiveRestartEnabled());
    }
    else
    {
        mtl::BufferSlice source =
            mtl::BufferSlice(idxBuffer->getCurrentBuffer()).subslice(offsetModulo);
        ANGLE_TRY(conversion->bufferPool.allocate(contextMtl, convertedIndexSize, &converted));
        ANGLE_TRY(display->getUtils().convertIndexBufferGPU(
            contextMtl, indexType, static_cast<uint32_t>(indexCount), source, converted,
            glState.isPrimitiveRestartEnabled()));
    }
    ANGLE_TRY(conversion->bufferPool.commit(contextMtl));
    conversion->dirty = false;
    conversion->buffer = std::move(converted);
    // Calculate ranges for prim restart simple types.
    *outIdxBuffer = conversion->buffer.subslice(alignedOffset);

    return angle::Result::Continue;
}

angle::Result VertexArrayMtl::streamIndexBufferFromClient(const gl::Context *context,
                                                          gl::DrawElementsType indexType,
                                                          size_t indexCount,
                                                          const void *sourcePointer,
                                                          mtl::BufferSlice *outIdxBuffer)
{
    ASSERT(getElementArrayBuffer() == nullptr);
    ContextMtl *contextMtl = mtl::GetImpl(context);

    const size_t elementSize = gl::GetDrawElementsTypeSize(indexType);
    const size_t indexSize   = indexCount * elementSize;
    angle::Span<const uint8_t> source(static_cast<const uint8_t *>(sourcePointer), indexSize);
    const size_t convertedIndexSize =
        indexType == gl::DrawElementsType::UnsignedByte ? indexSize * 2 : indexSize;
    mDynamicIndexData.releaseInFlightBuffers(contextMtl);
    mtl::BufferSlice converted;
    angle::Span<uint8_t> dstData;
    ANGLE_TRY(mDynamicIndexData.allocate(contextMtl, convertedIndexSize, &dstData, &converted));
    StreamIndexData(contextMtl, indexType, indexCount, source, dstData,
                    context->getState().isPrimitiveRestartEnabled());
    ANGLE_TRY(mDynamicIndexData.commit(contextMtl));
    *outIdxBuffer = std::move(converted);
    return angle::Result::Continue;
}

angle::Result VertexArrayMtl::convertVertexBuffer(const gl::Context *glContext,
                                                  BufferMtl *srcBuffer,
                                                  const gl::VertexBinding &binding,
                                                  size_t attribIndex,
                                                  const mtl::VertexFormat &srcVertexFormat)
{
    unsigned srcFormatSize = srcVertexFormat.intendedAngleFormat().pixelBytes;

    ContextMtl *contextMtl = mtl::GetImpl(glContext);

    // Convert to tightly packed format
    GLuint stride;
    const mtl::VertexFormat &convertedFormat =
        GetVertexConversionFormat(contextMtl, srcVertexFormat.intendedFormatId, &stride);

    ConversionBufferMtl *conversion = srcBuffer->getVertexConversionBuffer(
        contextMtl, srcVertexFormat.intendedFormatId, binding.getStride(), binding.getOffset());

    // Has the content of the buffer has changed since last conversion?
    if (!conversion->dirty)
    {
        VertexConversionBufferMtl *vertexConversionMtl =
            static_cast<VertexConversionBufferMtl *>(conversion);
        ASSERT((binding.getOffset() - vertexConversionMtl->offset) % binding.getStride() == 0);
        mConvertedArrayBufferHolders[attribIndex].set(conversion->buffer.buffer());
        mCurrentArrayBufferOffsets[attribIndex] =
            conversion->buffer.offset() +
            stride * ((binding.getOffset() - vertexConversionMtl->offset) / binding.getStride());

        mCurrentArrayBuffers[attribIndex]       = &mConvertedArrayBufferHolders[attribIndex];
        mCurrentArrayBufferFormats[attribIndex] = &convertedFormat;
        mCurrentArrayBufferStrides[attribIndex] = stride;
        return angle::Result::Continue;
    }
    size_t numVertices = GetVertexCountWithConversion(
        srcBuffer, static_cast<VertexConversionBufferMtl *>(conversion), binding, srcFormatSize);

    const angle::Format &convertedAngleFormat = convertedFormat.actualAngleFormat();
    bool canConvertToFloatOnGPU =
        convertedAngleFormat.isFloat() && !convertedAngleFormat.isVertexTypeHalfFloat();

    bool canExpandComponentsOnGPU = convertedFormat.actualSameGLType;

    conversion->bufferPool.releaseInFlightBuffers(contextMtl);
    conversion->bufferPool.updateAlignment(contextMtl, convertedAngleFormat.pixelBytes);

    if (canConvertToFloatOnGPU || canExpandComponentsOnGPU)
    {
        ANGLE_TRY(convertVertexBufferGPU(glContext, srcBuffer, binding, attribIndex,
                                         convertedFormat, stride, numVertices,
                                         canExpandComponentsOnGPU, conversion));
    }
    else
    {
        ANGLE_TRY(convertVertexBufferCPU(contextMtl, srcBuffer, binding, attribIndex,
                                         convertedFormat, stride, numVertices, conversion));
    }

    mConvertedArrayBufferHolders[attribIndex].set(conversion->buffer.buffer());
    mCurrentArrayBufferOffsets[attribIndex] =
        conversion->buffer.offset() +
        stride *
            ((binding.getOffset() - static_cast<VertexConversionBufferMtl *>(conversion)->offset) /
             binding.getStride());
    mCurrentArrayBuffers[attribIndex]       = &mConvertedArrayBufferHolders[attribIndex];
    mCurrentArrayBufferFormats[attribIndex] = &convertedFormat;
    mCurrentArrayBufferStrides[attribIndex] = stride;

    ASSERT(conversion->dirty);
    conversion->dirty = false;

#ifndef NDEBUG
    ANGLE_MTL_OBJC_SCOPE
    {
        mConvertedArrayBufferHolders[attribIndex].getCurrentBuffer()->get().label =
            [NSString stringWithFormat:@"Converted from %p offset=%zu stride=%u", srcBuffer,
                                       binding.getOffset(), binding.getStride()];
    }
#endif

    return angle::Result::Continue;
}

angle::Result VertexArrayMtl::convertVertexBufferCPU(ContextMtl *contextMtl,
                                                     BufferMtl *srcBuffer,
                                                     const gl::VertexBinding &binding,
                                                     size_t attribIndex,
                                                     const mtl::VertexFormat &convertedFormat,
                                                     GLuint targetStride,
                                                     size_t numVertices,
                                                     ConversionBufferMtl *conversion)
{
    VertexConversionBufferMtl *vertexConverison =
        static_cast<VertexConversionBufferMtl *>(conversion);
    size_t srcOffset = MIN(binding.getOffset(), static_cast<GLintptr>(vertexConverison->offset));
    angle::Span<const uint8_t> srcBytes = srcBuffer->getBufferDataReadOnly(contextMtl, srcOffset);
    ANGLE_CHECK_GL_ALLOC(contextMtl, !srcBytes.empty());
    mtl::BufferSlice convertedBuffer;
    ANGLE_TRY(StreamVertexData(contextMtl, &conversion->bufferPool, srcBytes.data(),
                               numVertices * targetStride, 0, numVertices, binding.getStride(),
                               convertedFormat.vertexLoadFunction, &convertedBuffer));
    conversion->buffer = std::move(convertedBuffer);
    return angle::Result::Continue;
}

angle::Result VertexArrayMtl::convertVertexBufferGPU(const gl::Context *glContext,
                                                     BufferMtl *srcBuffer,
                                                     const gl::VertexBinding &binding,
                                                     size_t attribIndex,
                                                     const mtl::VertexFormat &convertedFormat,
                                                     GLuint targetStride,
                                                     size_t numVertices,
                                                     bool isExpandingComponents,
                                                     ConversionBufferMtl *conversion)
{
    ContextMtl *contextMtl = mtl::GetImpl(glContext);

    mtl::BufferSlice newBuffer;
    ANGLE_TRY(conversion->bufferPool.allocate(contextMtl, numVertices * targetStride, &newBuffer));

    GLintptr bindingOffset = binding.getOffset();

    if constexpr (sizeof(bindingOffset) > sizeof(uint32_t))
    {
        ANGLE_CHECK_GL_MATH(contextMtl, static_cast<std::make_unsigned_t<decltype(bindingOffset)>>(
                                            bindingOffset) <= std::numeric_limits<uint32_t>::max());
    }
    ANGLE_CHECK_GL_MATH(contextMtl, newBuffer.offset() <= std::numeric_limits<uint32_t>::max());
    ANGLE_CHECK_GL_MATH(contextMtl, numVertices <= std::numeric_limits<uint32_t>::max());

    mtl::VertexFormatConvertParams params;
    VertexConversionBufferMtl *vertexConversion =
        static_cast<VertexConversionBufferMtl *>(conversion);
    if constexpr (sizeof(vertexConversion->offset) > sizeof(uint32_t))
    {
        ANGLE_CHECK_GL_MATH(contextMtl,
                            vertexConversion->offset <= std::numeric_limits<uint32_t>::max());
    }
    params.srcBuffer            = srcBuffer->getCurrentBuffer();
    params.srcBufferStartOffset = std::min(static_cast<uint32_t>(vertexConversion->offset),
                                           static_cast<uint32_t>(bindingOffset));
    params.srcStride            = binding.getStride();
    params.srcDefaultAlphaData  = convertedFormat.defaultAlpha;

    params.dstBuffer            = newBuffer.buffer();
    params.dstBufferStartOffset = static_cast<uint32_t>(newBuffer.offset());
    params.dstStride            = targetStride;
    params.dstComponents        = convertedFormat.actualAngleFormat().channelCount;

    params.vertexCount = static_cast<uint32_t>(numVertices);

    mtl::RenderUtils &utils = contextMtl->getDisplay()->getUtils();

    // Compute based buffer conversion.
    if (!isExpandingComponents)
    {
        ANGLE_TRY(utils.convertVertexFormatToFloatCS(
            contextMtl, convertedFormat.intendedAngleFormat(), params));
    }
    else
    {
        ANGLE_TRY(utils.expandVertexFormatComponentsCS(
            contextMtl, convertedFormat.intendedAngleFormat(), params));
    }

    ANGLE_TRY(conversion->bufferPool.commit(contextMtl));

    conversion->buffer = std::move(newBuffer);

    return angle::Result::Continue;
}
}  // namespace rx
