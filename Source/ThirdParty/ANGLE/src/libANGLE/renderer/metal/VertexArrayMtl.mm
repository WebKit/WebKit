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
                               SimpleWeakBufferHolderMtl *bufferHolder,
                               size_t *bufferOffsetOut)
{
    ANGLE_CHECK(contextMtl, vertexLoadFunction, gl::err::kInternalError, GL_INVALID_OPERATION);
    uint8_t *dst = nullptr;
    mtl::BufferRef newBuffer;
    ANGLE_TRY(dynamicBuffer->allocate(contextMtl, bytesToAllocate, &dst, &newBuffer,
                                      bufferOffsetOut, nullptr));
    bufferHolder->set(newBuffer);
    dst += destOffset;
    vertexLoadFunction(sourceData, stride, vertexCount, dst);

    ANGLE_TRY(dynamicBuffer->commit(contextMtl));
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

size_t GetIndexConvertedBufferSize(gl::DrawElementsType indexType, size_t indexCount)
{
    size_t elementSize = gl::GetDrawElementsTypeSize(indexType);
    if (indexType == gl::DrawElementsType::UnsignedByte)
    {
        // 8-bit indices are not supported by Metal, so they are promoted to
        // 16-bit indices below
        elementSize = sizeof(GLushort);
    }

    const size_t amount = elementSize * indexCount;

    return amount;
}

angle::Result StreamIndexData(ContextMtl *contextMtl,
                              mtl::BufferPool *dynamicBuffer,
                              const uint8_t *sourcePointer,
                              gl::DrawElementsType indexType,
                              size_t indexCount,
                              bool primitiveRestartEnabled,
                              mtl::BufferRef *bufferOut,
                              size_t *bufferOffsetOut)
{
    dynamicBuffer->releaseInFlightBuffers(contextMtl);
    const size_t amount = GetIndexConvertedBufferSize(indexType, indexCount);
    GLubyte *dst        = nullptr;
    ANGLE_TRY(
        dynamicBuffer->allocate(contextMtl, amount, &dst, bufferOut, bufferOffsetOut, nullptr));

    if (indexType == gl::DrawElementsType::UnsignedByte)
    {
        // Unsigned bytes don't have direct support in Metal so we have to expand the
        // memory to a GLushort.
        const GLubyte *in     = static_cast<const GLubyte *>(sourcePointer);
        GLushort *expandedDst = reinterpret_cast<GLushort *>(dst);

        if (primitiveRestartEnabled)
        {
            for (size_t index = 0; index < indexCount; index++)
            {
                if (in[index] == 0xFF)
                {
                    expandedDst[index] = 0xFFFF;
                }
                else
                {
                    expandedDst[index] = static_cast<GLushort>(in[index]);
                }
            }
        }  // if (primitiveRestartEnabled)
        else
        {
            for (size_t index = 0; index < indexCount; index++)
            {
                expandedDst[index] = static_cast<GLushort>(in[index]);
            }
        }  // if (primitiveRestartEnabled)
    }
    else
    {
        memcpy(dst, sourcePointer, amount);
    }
    ANGLE_TRY(dynamicBuffer->commit(contextMtl));

    return angle::Result::Continue;
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
}
inline size_t GetIndexCount(BufferMtl *srcBuffer, size_t offset, gl::DrawElementsType indexType)
{
    size_t elementSize = gl::GetDrawElementsTypeSize(indexType);
    return (srcBuffer->size() - offset) / elementSize;
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
                ANGLE_TRY(StreamVertexData(contextMtl, &mDynamicVertexData, src, bytesToAllocate,
                                           destOffset, elementCount, binding.getStride(),
                                           streamFormat.vertexLoadFunction,
                                           &mConvertedArrayBufferHolders[attribIndex],
                                           &mCurrentArrayBufferOffsets[attribIndex]));
                if (contextMtl->getDisplay()->getFeatures().flushAfterStreamVertexData.enabled)
                {
                    // WaitUntilScheduled is needed for this workaround. NoWait does not have the
                    // needed effect.
                    contextMtl->flushCommandBuffer(mtl::WaitUntilScheduled);
                }

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

template <size_t indexRewind>
void AppendDrawCommands(std::vector<DrawCommandRange> &drawCommands,
                        size_t count,
                        size_t firstIndex,
                        size_t indexSize)
{
    // Break the draw into hunks of 100'663'290 to avoid overflowing index count uint32_t.
    // Preserves primitive boundaries as the limit is divisible by all the possible per primitive
    // counts.
    constexpr size_t perCommandIndexCount = 0xffffff * 2 * 3;
    while (count > 0)
    {
        size_t offset       = firstIndex * indexSize;
        size_t commandCount = std::min(count, perCommandIndexCount);
        drawCommands.emplace_back(static_cast<uint32_t>(commandCount), offset);
        count -= commandCount;
        firstIndex += commandCount - indexRewind;  // Underflow ok, loop will terminate.
    }
}

void AppendDrawCommands(std::vector<DrawCommandRange> &drawCommands,
                        gl::PrimitiveMode mode,
                        size_t count,
                        size_t firstIndex,
                        gl::PrimitiveMode drawMode,
                        gl::DrawElementsType type)
{
    size_t elementSize = gl::GetDrawElementsTypeSize(type);
    uint32_t perPrimitiveIndexCount;
    switch (drawMode)
    {
        case gl::PrimitiveMode::Points:
            perPrimitiveIndexCount = 1;
            break;
        case gl::PrimitiveMode::Lines:
        case gl::PrimitiveMode::LineStrip:
            perPrimitiveIndexCount = 2;
            break;
        case gl::PrimitiveMode::Triangles:
        case gl::PrimitiveMode::TriangleStrip:
            perPrimitiveIndexCount = 3;
            break;
        default:
            UNREACHABLE();
            return;
    }
    if (count < perPrimitiveIndexCount)
    {
        return;
    }
    if (mode != drawMode)
    {
        firstIndex *= perPrimitiveIndexCount;
        count = (count - perPrimitiveIndexCount + 1) * perPrimitiveIndexCount;
    }
    switch (drawMode)
    {
        case gl::PrimitiveMode::Points:
            AppendDrawCommands<0>(drawCommands, count, firstIndex, elementSize);
            break;
        case gl::PrimitiveMode::Lines:
            AppendDrawCommands<0>(drawCommands, count, firstIndex, elementSize);
            break;
        case gl::PrimitiveMode::LineStrip:
            AppendDrawCommands<1>(drawCommands, count, firstIndex, elementSize);
            break;
        case gl::PrimitiveMode::Triangles:
            AppendDrawCommands<0>(drawCommands, count, firstIndex, elementSize);
            break;
        case gl::PrimitiveMode::TriangleStrip:
            AppendDrawCommands<2>(drawCommands, count, firstIndex, elementSize);
            break;
        default:
            UNREACHABLE();
            return;
    }
}

// Computes draw command ranges from draw index ranges for primitive restart.
// The draw index ranges are in source buffer element space. The output draw commands
// are in draw buffer space, accounting for any expansion from mode conversion
// (e.g., TriangleStrip to Triangles via provoking vertex).
//
// drawIndexRanges: pre-computed ranges of consecutive non-restart indices in the source buffer
// firstIndex: first element index in the source buffer to draw from
// count: number of source elements in the draw window
// mode: source primitive mode, may be strip.
// drawMode: output primitive mode. Always a simple type: Points, Lines, Triangles.
// indexBufferType: type of the output buffer elements.
void AppendSimpleDrawCommandRanges(std::vector<DrawCommandRange> &drawCommands,
                                   gl::PrimitiveMode mode,
                                   uint32_t count,
                                   size_t firstIndex,
                                   const std::vector<DrawIndexRange> &drawIndexRanges,
                                   gl::PrimitiveMode drawMode,
                                   gl::DrawElementsType indexBufferType)
{
    uint32_t perPrimitiveIndexCount;
    switch (drawMode)
    {
        case gl::PrimitiveMode::Points:
            perPrimitiveIndexCount = 1;
            break;
        case gl::PrimitiveMode::Lines:
            perPrimitiveIndexCount = 2;
            break;
        case gl::PrimitiveMode::Triangles:
            perPrimitiveIndexCount = 3;
            break;
        default:
            UNREACHABLE();
            return;
    }
    if (count < perPrimitiveIndexCount)
    {
        return;
    }
    const size_t drawIndexSize = gl::GetDrawElementsTypeSize(indexBufferType);
    const size_t lastIndex     = firstIndex + count - 1;

    // For simple types (mode == drawMode), source and draw indices are 1:1.
    // For strips/fans/loops (mode != drawMode), the provoking vertex helper expands the buffer:
    //   TriangleStrip/Fan -> Triangles: N source -> (N-2)*3 draw indices.
    //   LineStrip/Loop -> Lines: N source -> (N-1)*2 draw indices.
    for (const auto &range : drawIndexRanges)
    {
        if (range.end < firstIndex)
        {
            continue;
        }
        if (range.begin > lastIndex)
        {
            break;
        }
        DrawIndexRange clippedRange{std::max(range.begin, firstIndex),
                                    std::min(range.end, lastIndex)};
        size_t indexCount = clippedRange.end - clippedRange.begin + 1;

        if (indexCount < perPrimitiveIndexCount)
        {
            continue;
        }

        size_t drawIndexCount;
        size_t drawBeginIndex;
        if (mode == drawMode)
        {
            // Simple types: 1:1 mapping, round down to complete primitives.
            drawIndexCount = indexCount - (indexCount % perPrimitiveIndexCount);
            drawBeginIndex = clippedRange.begin;
        }
        else
        {
            // Expanded modes: `N` source indices produce `(N - perPrimitiveIndexCount + 1)`
            // primitives that produce `perPrimitiveIndexCount` indices.
            drawIndexCount = (indexCount - perPrimitiveIndexCount + 1) * perPrimitiveIndexCount;
            drawBeginIndex = clippedRange.begin * perPrimitiveIndexCount;
        }
        AppendDrawCommands<0>(drawCommands, drawIndexCount, drawBeginIndex, drawIndexSize);
    }
}

angle::Result VertexArrayMtl::resolveDrawElementsDraw(
    const gl::Context *glContext,
    gl::PrimitiveMode mode,
    gl::DrawElementsType type,
    GLsizei count,
    const void *indices,
    bool rewriteProvokingVertex,
    bool isPrimitiveRestartEnabled,
    gl::PrimitiveMode *outNewMode,
    std::vector<DrawCommandRange> *outDrawCommands,
    mtl::BufferRef *outIndexBuffer,
    size_t *outIndexBufferOffset,
    gl::DrawElementsType *outIndexBufferType)
{
    ContextMtl *contextMtl = mtl::GetImpl(glContext);

    // Resulting draw is either `uint16_t` or `uint32_t`.
    gl::DrawElementsType indexBufferType =
        type == gl::DrawElementsType::UnsignedByte ? gl::DrawElementsType::UnsignedShort : type;

    // Resulting draw is made with `newMode`.
    gl::PrimitiveMode newMode = mode;
    if (rewriteProvokingVertex)
    {
        // Provoking vertex will convert strips to their simple equivalents.
        switch (mode)
        {
            case gl::PrimitiveMode::Triangles:
            case gl::PrimitiveMode::TriangleStrip:
                newMode = gl::PrimitiveMode::Triangles;
                break;
            case gl::PrimitiveMode::Lines:
            case gl::PrimitiveMode::LineStrip:
                newMode = gl::PrimitiveMode::Lines;
                break;
            default:
                UNREACHABLE();
                return angle::Result::Stop;
        }
    }

    size_t firstIndex;
    mtl::BufferRef indexBuffer;
    size_t indexBufferOffset;

    // Step 1: Get the index buffer of supported type and resolve the first index to
    // process.
    const gl::Buffer *glElementArrayBuffer = getElementArrayBuffer();
    if (glElementArrayBuffer == nullptr)
    {
        firstIndex = 0;
        ANGLE_TRY(streamIndexBufferFromClient(glContext, type, count, indices, &indexBuffer,
                                              &indexBufferOffset));
    }
    else
    {
        firstIndex = static_cast<size_t>(reinterpret_cast<uintptr_t>(indices)) /
                     gl::GetDrawElementsTypeSize(type);
        if (type != indexBufferType)
        {
            ANGLE_TRY(convertIndexBuffer(glContext, type, 0, &indexBuffer, &indexBufferOffset));
        }
        else
        {
            BufferMtl *bufferMtl = mtl::GetImpl(glElementArrayBuffer);
            indexBuffer          = bufferMtl->getCurrentBuffer();
            indexBufferOffset    = 0;
        }
    }

    // Step 2: GL draw range is firstIndex, count. In case Metal primitive topology for
    // `newMode` does not support primitive restart, compute the list of draw ranges
    // containing the primitives.
    const std::vector<DrawIndexRange> *indexRanges;
    std::vector<DrawIndexRange> indexRangesStorage;

    bool isSimpleType = newMode == gl::PrimitiveMode::Points ||
                        newMode == gl::PrimitiveMode::Lines ||
                        newMode == gl::PrimitiveMode::Triangles;

    if (isPrimitiveRestartEnabled && isSimpleType)
    {
        if (glElementArrayBuffer != nullptr)
        {
            BufferMtl *idxBuffer = mtl::GetImpl(glElementArrayBuffer);
            indexRanges          = &idxBuffer->getDrawIndexRanges(contextMtl, type);
        }
        else
        {
            indexRangesStorage = BufferMtl::GetDrawIndexRangesFromClientData(type, count, indices);
            indexRanges        = &indexRangesStorage;
        }
    }
    else
    {
        // No primitive restart splitting — inject a single full range.
        indexRangesStorage.emplace_back(firstIndex, firstIndex + count - 1);
        indexRanges = &indexRangesStorage;
    }

    // Step 3: Conditionally rewrite the index buffer for provoking vertex.
    // preconditionIndexBuffer dispatches per-range, so with primitive restart it only
    // processes the non-restart runs (not the full buffer).
    if (rewriteProvokingVertex)
    {
        mtl::BufferRef newIndexBuffer;
        size_t newIndexBufferOffset;
        ANGLE_TRY(contextMtl->getProvokingVertexHelper().preconditionIndexBuffer(
            contextMtl, count, mode, firstIndex, isPrimitiveRestartEnabled, *indexRanges,
            indexBuffer, indexBufferOffset, indexBufferType, &newIndexBuffer,
            &newIndexBufferOffset));
        indexBuffer       = std::move(newIndexBuffer);
        indexBufferOffset = newIndexBufferOffset;
    }

    // Step 4: Compute draw command ranges (handles primitive restart splitting and large draw
    // chunking).  Reuses the same index ranges computed in Step 2.
    std::vector<DrawCommandRange> drawCommands;

    if (isPrimitiveRestartEnabled && isSimpleType)
    {
        AppendSimpleDrawCommandRanges(drawCommands, mode, static_cast<uint32_t>(count), firstIndex,
                                      *indexRanges, newMode, indexBufferType);
    }
    else
    {
        AppendDrawCommands(drawCommands, mode, static_cast<uint32_t>(count), firstIndex, newMode,
                           indexBufferType);
    }

    *outNewMode           = newMode;
    *outDrawCommands      = std::move(drawCommands);
    *outIndexBuffer       = indexBuffer;
    *outIndexBufferOffset = indexBufferOffset;
    *outIndexBufferType   = indexBufferType;

    return angle::Result::Continue;
}

angle::Result VertexArrayMtl::convertIndexBuffer(const gl::Context *glContext,
                                                 gl::DrawElementsType indexType,
                                                 size_t offset,
                                                 mtl::BufferRef *idxBufferOut,
                                                 size_t *idxBufferOffsetOut)
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
        *idxBufferOut       = conversion->convertedBuffer;
        *idxBufferOffsetOut = conversion->convertedOffset + alignedOffset;
        return angle::Result::Continue;
    }

    size_t indexCount = GetIndexCount(idxBuffer, offsetModulo, indexType);
    if ((!contextMtl->getDisplay()->getFeatures().hasCheapRenderPass.enabled &&
         contextMtl->getRenderCommandEncoder()))
    {
        // We shouldn't use GPU to convert when we are in a middle of a render pass.
        ANGLE_TRY(StreamIndexData(contextMtl, &conversion->data,
                                  idxBuffer->getBufferDataReadOnly(contextMtl, offsetModulo).data(),
                                  indexType, indexCount, glState.isPrimitiveRestartEnabled(),
                                  &conversion->convertedBuffer, &conversion->convertedOffset));
    }
    else
    {
        ANGLE_TRY(convertIndexBufferGPU(glContext, indexType, idxBuffer, offsetModulo, indexCount,
                                        conversion));
    }
    // Calculate ranges for prim restart simple types.
    *idxBufferOut       = conversion->convertedBuffer;
    *idxBufferOffsetOut = conversion->convertedOffset + alignedOffset;

    return angle::Result::Continue;
}

angle::Result VertexArrayMtl::convertIndexBufferGPU(const gl::Context *glContext,
                                                    gl::DrawElementsType indexType,
                                                    BufferMtl *idxBuffer,
                                                    size_t offset,
                                                    size_t indexCount,
                                                    IndexConversionBufferMtl *conversion)
{
    ContextMtl *contextMtl = mtl::GetImpl(glContext);
    DisplayMtl *display    = contextMtl->getDisplay();

    const size_t amount = GetIndexConvertedBufferSize(indexType, indexCount);

    // Allocate new buffer, save it in conversion struct so that we can reuse it when the content
    // of the original buffer is not dirty.
    conversion->data.releaseInFlightBuffers(contextMtl);
    ANGLE_TRY(conversion->data.allocate(contextMtl, amount, nullptr, &conversion->convertedBuffer,
                                        &conversion->convertedOffset));

    // Do the conversion on GPU.
    ANGLE_TRY(display->getUtils().convertIndexBufferGPU(
        contextMtl, {indexType, static_cast<uint32_t>(indexCount), idxBuffer->getCurrentBuffer(),
                     static_cast<uint32_t>(offset), conversion->convertedBuffer,
                     static_cast<uint32_t>(conversion->convertedOffset),
                     glContext->getState().isPrimitiveRestartEnabled()}));

    ANGLE_TRY(conversion->data.commit(contextMtl));

    ASSERT(conversion->dirty);
    conversion->dirty = false;

    return angle::Result::Continue;
}

angle::Result VertexArrayMtl::streamIndexBufferFromClient(const gl::Context *context,
                                                          gl::DrawElementsType indexType,
                                                          size_t indexCount,
                                                          const void *sourcePointer,
                                                          mtl::BufferRef *idxBufferOut,
                                                          size_t *idxBufferOffsetOut)
{
    ASSERT(getElementArrayBuffer() == nullptr);
    ContextMtl *contextMtl = mtl::GetImpl(context);

    auto srcData = static_cast<const uint8_t *>(sourcePointer);
    ANGLE_TRY(StreamIndexData(contextMtl, &mDynamicIndexData, srcData, indexType, indexCount,
                              context->getState().isPrimitiveRestartEnabled(), idxBufferOut,
                              idxBufferOffsetOut));

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
        mConvertedArrayBufferHolders[attribIndex].set(conversion->convertedBuffer);
        mCurrentArrayBufferOffsets[attribIndex] =
            conversion->convertedOffset +
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

    conversion->data.releaseInFlightBuffers(contextMtl);
    conversion->data.updateAlignment(contextMtl, convertedAngleFormat.pixelBytes);

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

    mConvertedArrayBufferHolders[attribIndex].set(conversion->convertedBuffer);
    mCurrentArrayBufferOffsets[attribIndex] =
        conversion->convertedOffset +
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
    SimpleWeakBufferHolderMtl conversionBufferHolder;
    ANGLE_TRY(StreamVertexData(contextMtl, &conversion->data, srcBytes.data(),
                               numVertices * targetStride, 0, numVertices, binding.getStride(),
                               convertedFormat.vertexLoadFunction, &conversionBufferHolder,
                               &conversion->convertedOffset));
    conversion->convertedBuffer = conversionBufferHolder.getCurrentBuffer();
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

    mtl::BufferRef newBuffer;
    size_t newBufferOffset;
    ANGLE_TRY(conversion->data.allocate(contextMtl, numVertices * targetStride, nullptr, &newBuffer,
                                        &newBufferOffset));

    GLintptr bindingOffset = binding.getOffset();

    if constexpr (sizeof(bindingOffset) > sizeof(uint32_t))
    {
        ANGLE_CHECK_GL_MATH(contextMtl, static_cast<std::make_unsigned_t<decltype(bindingOffset)>>(
                                            bindingOffset) <= std::numeric_limits<uint32_t>::max());
    }
    ANGLE_CHECK_GL_MATH(contextMtl, newBufferOffset <= std::numeric_limits<uint32_t>::max());
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

    params.dstBuffer            = newBuffer;
    params.dstBufferStartOffset = static_cast<uint32_t>(newBufferOffset);
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

    ANGLE_TRY(conversion->data.commit(contextMtl));

    conversion->convertedBuffer = newBuffer;
    conversion->convertedOffset = newBufferOffset;

    return angle::Result::Continue;
}
}  // namespace rx
