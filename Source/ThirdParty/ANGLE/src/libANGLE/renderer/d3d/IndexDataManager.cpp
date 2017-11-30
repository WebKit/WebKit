//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// IndexDataManager.cpp: Defines the IndexDataManager, a class that
// runs the Buffer translation process for index buffers.

#include "libANGLE/renderer/d3d/IndexDataManager.h"

#include "common/utilities.h"
#include "libANGLE/Buffer.h"
#include "libANGLE/Context.h"
#include "libANGLE/VertexArray.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/d3d/BufferD3D.h"
#include "libANGLE/renderer/d3d/IndexBuffer.h"

namespace rx
{

namespace
{

template <typename InputT, typename DestT>
void ConvertIndexArray(const void *input,
                       GLenum sourceType,
                       void *output,
                       GLenum destinationType,
                       GLsizei count,
                       bool usePrimitiveRestartFixedIndex)
{
    const InputT *in = static_cast<const InputT *>(input);
    DestT *out       = static_cast<DestT *>(output);

    if (usePrimitiveRestartFixedIndex)
    {
        InputT srcRestartIndex = static_cast<InputT>(gl::GetPrimitiveRestartIndex(sourceType));
        DestT destRestartIndex = static_cast<DestT>(gl::GetPrimitiveRestartIndex(destinationType));
        for (GLsizei i = 0; i < count; i++)
        {
            out[i] = (in[i] == srcRestartIndex ? destRestartIndex : static_cast<DestT>(in[i]));
        }
    }
    else
    {
        for (GLsizei i = 0; i < count; i++)
        {
            out[i] = static_cast<DestT>(in[i]);
        }
    }
}

void ConvertIndices(GLenum sourceType,
                    GLenum destinationType,
                    const void *input,
                    GLsizei count,
                    void *output,
                    bool usePrimitiveRestartFixedIndex)
{
    if (sourceType == destinationType)
    {
        const gl::Type &typeInfo = gl::GetTypeInfo(destinationType);
        memcpy(output, input, count * typeInfo.bytes);
        return;
    }

    if (sourceType == GL_UNSIGNED_BYTE)
    {
        ASSERT(destinationType == GL_UNSIGNED_SHORT);
        ConvertIndexArray<GLubyte, GLushort>(input, sourceType, output, destinationType, count,
                                             usePrimitiveRestartFixedIndex);
    }
    else if (sourceType == GL_UNSIGNED_SHORT)
    {
        ASSERT(destinationType == GL_UNSIGNED_INT);
        ConvertIndexArray<GLushort, GLuint>(input, sourceType, output, destinationType, count,
                                            usePrimitiveRestartFixedIndex);
    }
    else
        UNREACHABLE();
}

gl::Error StreamInIndexBuffer(IndexBufferInterface *buffer,
                              const void *data,
                              unsigned int count,
                              GLenum srcType,
                              GLenum dstType,
                              bool usePrimitiveRestartFixedIndex,
                              unsigned int *offset)
{
    const gl::Type &dstTypeInfo = gl::GetTypeInfo(dstType);

    if (count > (std::numeric_limits<unsigned int>::max() >> dstTypeInfo.bytesShift))
    {
        return gl::OutOfMemory() << "Reserving " << count << " indices of " << dstTypeInfo.bytes
                                 << " bytes each exceeds the maximum buffer size.";
    }

    unsigned int bufferSizeRequired = count << dstTypeInfo.bytesShift;
    ANGLE_TRY(buffer->reserveBufferSpace(bufferSizeRequired, dstType));

    void *output = nullptr;
    ANGLE_TRY(buffer->mapBuffer(bufferSizeRequired, &output, offset));

    ConvertIndices(srcType, dstType, data, count, output, usePrimitiveRestartFixedIndex);

    ANGLE_TRY(buffer->unmapBuffer());
    return gl::NoError();
}

unsigned int ElementTypeSize(GLenum elementType)
{
    switch (elementType)
    {
        case GL_UNSIGNED_BYTE:
            return sizeof(GLubyte);
        case GL_UNSIGNED_SHORT:
            return sizeof(GLushort);
        case GL_UNSIGNED_INT:
            return sizeof(GLuint);
        default:
            UNREACHABLE();
            return 0;
    }
}

}  // anonymous namespace

bool IsOffsetAligned(GLenum elementType, unsigned int offset)
{
    return (offset % ElementTypeSize(elementType) == 0);
}

// IndexDataManager implementation.
IndexDataManager::IndexDataManager(BufferFactoryD3D *factory)
    : mFactory(factory), mStreamingBufferShort(), mStreamingBufferInt()
{
}

IndexDataManager::~IndexDataManager()
{
}

void IndexDataManager::deinitialize()
{
    mStreamingBufferShort.reset();
    mStreamingBufferInt.reset();
}

// This function translates a GL-style indices into DX-style indices, with their description
// returned in translated.
// GL can specify vertex data in immediate mode (pointer to CPU array of indices), which is not
// possible in DX and requires streaming (Case 1). If the GL indices are specified with a buffer
// (Case 2), in a format supported by DX (subcase a) then all is good.
// When we have a buffer with an unsupported format (subcase b) then we need to do some translation:
// we will start by falling back to streaming, and after a while will start using a static
// translated copy of the index buffer.
gl::Error IndexDataManager::prepareIndexData(const gl::Context *context,
                                             GLenum srcType,
                                             GLenum dstType,
                                             GLsizei count,
                                             gl::Buffer *glBuffer,
                                             const void *indices,
                                             TranslatedIndexData *translated)
{
    const gl::Type &srcTypeInfo = gl::GetTypeInfo(srcType);
    const gl::Type &dstTypeInfo = gl::GetTypeInfo(dstType);

    BufferD3D *buffer = glBuffer ? GetImplAs<BufferD3D>(glBuffer) : nullptr;

    translated->indexType                 = dstType;
    translated->srcIndexData.srcBuffer    = buffer;
    translated->srcIndexData.srcIndices   = indices;
    translated->srcIndexData.srcIndexType = srcType;
    translated->srcIndexData.srcCount     = count;

    // Context can be nullptr in perf tests.
    bool primitiveRestartFixedIndexEnabled =
        context ? context->getGLState().isPrimitiveRestartEnabled() : false;

    // Case 1: the indices are passed by pointer, which forces the streaming of index data
    if (glBuffer == nullptr)
    {
        translated->storage = nullptr;
        return streamIndexData(indices, count, srcType, dstType, primitiveRestartFixedIndexEnabled,
                               translated);
    }

    // Case 2: the indices are already in a buffer
    unsigned int offset = static_cast<unsigned int>(reinterpret_cast<uintptr_t>(indices));
    ASSERT(srcTypeInfo.bytes * static_cast<unsigned int>(count) + offset <= buffer->getSize());

    bool offsetAligned = IsOffsetAligned(srcType, offset);

    // Case 2a: the buffer can be used directly
    if (offsetAligned && buffer->supportsDirectBinding() && dstType == srcType)
    {
        translated->storage     = buffer;
        translated->indexBuffer = nullptr;
        translated->serial      = buffer->getSerial();
        translated->startIndex  = (offset >> srcTypeInfo.bytesShift);
        translated->startOffset = offset;
        return gl::NoError();
    }

    translated->storage = nullptr;

    // Case 2b: use a static translated copy or fall back to streaming
    StaticIndexBufferInterface *staticBuffer = buffer->getStaticIndexBuffer();

    bool staticBufferInitialized = staticBuffer && staticBuffer->getBufferSize() != 0;
    bool staticBufferUsable =
        staticBuffer && offsetAligned && staticBuffer->getIndexType() == dstType;

    if (staticBufferInitialized && !staticBufferUsable)
    {
        buffer->invalidateStaticData(context);
        staticBuffer = nullptr;
    }

    if (staticBuffer == nullptr || !offsetAligned)
    {
        const uint8_t *bufferData = nullptr;
        ANGLE_TRY(buffer->getData(context, &bufferData));
        ASSERT(bufferData != nullptr);

        ANGLE_TRY(streamIndexData(bufferData + offset, count, srcType, dstType,
                                  primitiveRestartFixedIndexEnabled, translated));
        buffer->promoteStaticUsage(context, count << srcTypeInfo.bytesShift);
    }
    else
    {
        if (!staticBufferInitialized)
        {
            const uint8_t *bufferData = nullptr;
            ANGLE_TRY(buffer->getData(context, &bufferData));
            ASSERT(bufferData != nullptr);

            unsigned int convertCount =
                static_cast<unsigned int>(buffer->getSize()) >> srcTypeInfo.bytesShift;
            ANGLE_TRY(StreamInIndexBuffer(staticBuffer, bufferData, convertCount, srcType, dstType,
                                          primitiveRestartFixedIndexEnabled, nullptr));
        }
        ASSERT(offsetAligned && staticBuffer->getIndexType() == dstType);

        translated->indexBuffer = staticBuffer->getIndexBuffer();
        translated->serial      = staticBuffer->getSerial();
        translated->startIndex  = (offset >> srcTypeInfo.bytesShift);
        translated->startOffset = (offset >> srcTypeInfo.bytesShift) << dstTypeInfo.bytesShift;
    }

    return gl::NoError();
}

gl::Error IndexDataManager::streamIndexData(const void *data,
                                            unsigned int count,
                                            GLenum srcType,
                                            GLenum dstType,
                                            bool usePrimitiveRestartFixedIndex,
                                            TranslatedIndexData *translated)
{
    const gl::Type &dstTypeInfo = gl::GetTypeInfo(dstType);

    IndexBufferInterface *indexBuffer = nullptr;
    ANGLE_TRY(getStreamingIndexBuffer(dstType, &indexBuffer));
    ASSERT(indexBuffer != nullptr);

    unsigned int offset;
    ANGLE_TRY(StreamInIndexBuffer(indexBuffer, data, count, srcType, dstType,
                                  usePrimitiveRestartFixedIndex, &offset));

    translated->indexBuffer = indexBuffer->getIndexBuffer();
    translated->serial      = indexBuffer->getSerial();
    translated->startIndex  = (offset >> dstTypeInfo.bytesShift);
    translated->startOffset = offset;

    return gl::NoError();
}

gl::Error IndexDataManager::getStreamingIndexBuffer(GLenum destinationIndexType,
                                                    IndexBufferInterface **outBuffer)
{
    ASSERT(outBuffer);
    ASSERT(destinationIndexType == GL_UNSIGNED_SHORT || destinationIndexType == GL_UNSIGNED_INT);

    auto &streamingBuffer =
        (destinationIndexType == GL_UNSIGNED_INT) ? mStreamingBufferInt : mStreamingBufferShort;

    if (!streamingBuffer)
    {
        StreamingBuffer newBuffer(new StreamingIndexBufferInterface(mFactory));
        ANGLE_TRY(newBuffer->reserveBufferSpace(INITIAL_INDEX_BUFFER_SIZE, destinationIndexType));
        streamingBuffer = std::move(newBuffer);
    }

    *outBuffer = streamingBuffer.get();
    return gl::NoError();
}

GLenum GetIndexTranslationDestType(GLenum srcType,
                                   const gl::HasIndexRange &lazyIndexRange,
                                   bool usePrimitiveRestartWorkaround)
{
    // Avoid D3D11's primitive restart index value
    // see http://msdn.microsoft.com/en-us/library/windows/desktop/bb205124(v=vs.85).aspx
    if (usePrimitiveRestartWorkaround)
    {
        const gl::IndexRange &indexRange = lazyIndexRange.getIndexRange().value();
        if (indexRange.end == gl::GetPrimitiveRestartIndex(srcType))
        {
            return GL_UNSIGNED_INT;
        }
    }

    return (srcType == GL_UNSIGNED_INT) ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;
}

}  // namespace rx
