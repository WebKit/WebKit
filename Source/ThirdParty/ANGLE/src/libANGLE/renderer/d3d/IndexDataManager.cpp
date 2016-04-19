//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// IndexDataManager.cpp: Defines the IndexDataManager, a class that
// runs the Buffer translation process for index buffers.

#include "libANGLE/renderer/d3d/IndexDataManager.h"

#include "common/utilities.h"
#include "libANGLE/renderer/d3d/BufferD3D.h"
#include "libANGLE/renderer/d3d/IndexBuffer.h"
#include "libANGLE/Buffer.h"
#include "libANGLE/formatutils.h"

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
    else UNREACHABLE();
}

gl::Error StreamInIndexBuffer(IndexBufferInterface *buffer,
                              const GLvoid *data,
                              unsigned int count,
                              GLenum srcType,
                              GLenum dstType,
                              bool usePrimitiveRestartFixedIndex,
                              unsigned int *offset)
{
    const gl::Type &dstTypeInfo = gl::GetTypeInfo(dstType);

    if (count > (std::numeric_limits<unsigned int>::max() >> dstTypeInfo.bytesShift))
    {
        return gl::Error(GL_OUT_OF_MEMORY,
                         "Reserving %u indices of %u bytes each exceeds the maximum buffer size.",
                         count, dstTypeInfo.bytes);
    }

    unsigned int bufferSizeRequired = count << dstTypeInfo.bytesShift;
    gl::Error error = buffer->reserveBufferSpace(bufferSizeRequired, dstType);
    if (error.isError())
    {
        return error;
    }

    void *output = nullptr;
    error = buffer->mapBuffer(bufferSizeRequired, &output, offset);
    if (error.isError())
    {
        return error;
    }

    ConvertIndices(srcType, dstType, data, count, output, usePrimitiveRestartFixedIndex);

    error = buffer->unmapBuffer();
    if (error.isError())
    {
        return error;
    }

    return gl::Error(GL_NO_ERROR);
}

}  // anonymous namespace

IndexDataManager::IndexDataManager(BufferFactoryD3D *factory, RendererClass rendererClass)
    : mFactory(factory),
      mRendererClass(rendererClass),
      mStreamingBufferShort(nullptr),
      mStreamingBufferInt(nullptr)
{
}

IndexDataManager::~IndexDataManager()
{
    SafeDelete(mStreamingBufferShort);
    SafeDelete(mStreamingBufferInt);
}

// This function translates a GL-style indices into DX-style indices, with their description
// returned in translated.
// GL can specify vertex data in immediate mode (pointer to CPU array of indices), which is not
// possible in DX and requires streaming (Case 1). If the GL indices are specified with a buffer
// (Case 2), in a format supported by DX (subcase a) then all is good.
// When we have a buffer with an unsupported format (subcase b) then we need to do some translation:
// we will start by falling back to streaming, and after a while will start using a static translated
// copy of the index buffer.
gl::Error IndexDataManager::prepareIndexData(GLenum srcType,
                                             GLsizei count,
                                             gl::Buffer *glBuffer,
                                             const GLvoid *indices,
                                             TranslatedIndexData *translated,
                                             bool primitiveRestartFixedIndexEnabled)
{
    // Avoid D3D11's primitive restart index value
    // see http://msdn.microsoft.com/en-us/library/windows/desktop/bb205124(v=vs.85).aspx
    bool hasPrimitiveRestartIndex =
        translated->indexRange.vertexIndexCount < static_cast<size_t>(count) ||
        translated->indexRange.end == gl::GetPrimitiveRestartIndex(srcType);
    bool primitiveRestartWorkaround = mRendererClass == RENDERER_D3D11 &&
                                      !primitiveRestartFixedIndexEnabled &&
                                      hasPrimitiveRestartIndex && srcType == GL_UNSIGNED_SHORT;

    // We should never have to deal with MAX_UINT indices, since we restrict it via
    // MAX_ELEMENT_INDEX.
    ASSERT(!(mRendererClass == RENDERER_D3D11 && !primitiveRestartFixedIndexEnabled &&
             hasPrimitiveRestartIndex && srcType == GL_UNSIGNED_INT));

    const GLenum dstType = (srcType == GL_UNSIGNED_INT || primitiveRestartWorkaround) ?
                           GL_UNSIGNED_INT : GL_UNSIGNED_SHORT;

    const gl::Type &srcTypeInfo = gl::GetTypeInfo(srcType);
    const gl::Type &dstTypeInfo = gl::GetTypeInfo(dstType);

    BufferD3D *buffer = glBuffer ? GetImplAs<BufferD3D>(glBuffer) : nullptr;

    translated->indexType = dstType;
    translated->srcIndexData.srcBuffer    = buffer;
    translated->srcIndexData.srcIndices   = indices;
    translated->srcIndexData.srcIndexType = srcType;
    translated->srcIndexData.srcCount     = count;

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

    bool offsetAligned;
    switch (srcType)
    {
      case GL_UNSIGNED_BYTE:  offsetAligned = (offset % sizeof(GLubyte) == 0);  break;
      case GL_UNSIGNED_SHORT: offsetAligned = (offset % sizeof(GLushort) == 0); break;
      case GL_UNSIGNED_INT:   offsetAligned = (offset % sizeof(GLuint) == 0);   break;
      default: UNREACHABLE(); offsetAligned = false;
    }

    // Case 2a: the buffer can be used directly
    if (offsetAligned && buffer->supportsDirectBinding() &&
        dstType == srcType && !primitiveRestartWorkaround)
    {
        translated->storage = buffer;
        translated->indexBuffer = nullptr;
        translated->serial = buffer->getSerial();
        translated->startIndex = (offset >> srcTypeInfo.bytesShift);
        translated->startOffset = offset;
        return gl::Error(GL_NO_ERROR);
    }
    else
    {
        translated->storage = nullptr;
    }

    // Case 2b: use a static translated copy or fall back to streaming
    StaticIndexBufferInterface *staticBuffer = buffer->getStaticIndexBuffer();

    bool staticBufferInitialized = staticBuffer && staticBuffer->getBufferSize() != 0;
    bool staticBufferUsable = staticBuffer &&
                              offsetAligned && staticBuffer->getIndexType() == dstType;

    if (staticBufferInitialized && !staticBufferUsable)
    {
        buffer->invalidateStaticData();
        staticBuffer = nullptr;
    }

    if (staticBuffer == nullptr || !offsetAligned)
    {
        const uint8_t *bufferData = nullptr;
        gl::Error error = buffer->getData(&bufferData);
        if (error.isError())
        {
            return error;
        }
        ASSERT(bufferData != nullptr);

        error = streamIndexData(bufferData + offset, count, srcType, dstType,
                                primitiveRestartFixedIndexEnabled, translated);
        if (error.isError())
        {
            return error;
        }
        buffer->promoteStaticUsage(count << srcTypeInfo.bytesShift);
    }
    else
    {
        if (!staticBufferInitialized)
        {
            const uint8_t *bufferData = nullptr;
            gl::Error error = buffer->getData(&bufferData);
            if (error.isError())
            {
                return error;
            }
            ASSERT(bufferData != nullptr);

            unsigned int convertCount =
                static_cast<unsigned int>(buffer->getSize()) >> srcTypeInfo.bytesShift;
            error = StreamInIndexBuffer(staticBuffer, bufferData, convertCount, srcType, dstType,
                                        primitiveRestartFixedIndexEnabled, nullptr);
            if (error.isError())
            {
                return error;
            }
        }
        ASSERT(offsetAligned && staticBuffer->getIndexType() == dstType);

        translated->indexBuffer = staticBuffer->getIndexBuffer();
        translated->serial = staticBuffer->getSerial();
        translated->startIndex = (offset >> srcTypeInfo.bytesShift);
        translated->startOffset = (offset >> srcTypeInfo.bytesShift) << dstTypeInfo.bytesShift;
    }

    return gl::Error(GL_NO_ERROR);
}

gl::Error IndexDataManager::streamIndexData(const GLvoid *data,
                                            unsigned int count,
                                            GLenum srcType,
                                            GLenum dstType,
                                            bool usePrimitiveRestartFixedIndex,
                                            TranslatedIndexData *translated)
{
    const gl::Type &dstTypeInfo = gl::GetTypeInfo(dstType);

    IndexBufferInterface *indexBuffer = nullptr;
    gl::Error error = getStreamingIndexBuffer(dstType, &indexBuffer);
    if (error.isError())
    {
        return error;
    }
    ASSERT(indexBuffer != nullptr);

    unsigned int offset;
    StreamInIndexBuffer(indexBuffer, data, count, srcType, dstType, usePrimitiveRestartFixedIndex,
                        &offset);

    translated->indexBuffer = indexBuffer->getIndexBuffer();
    translated->serial = indexBuffer->getSerial();
    translated->startIndex = (offset >> dstTypeInfo.bytesShift);
    translated->startOffset = offset;

    return gl::Error(GL_NO_ERROR);
}

gl::Error IndexDataManager::getStreamingIndexBuffer(GLenum destinationIndexType,
                                                    IndexBufferInterface **outBuffer)
{
    ASSERT(outBuffer);
    if (destinationIndexType == GL_UNSIGNED_INT)
    {
        if (!mStreamingBufferInt)
        {
            mStreamingBufferInt = new StreamingIndexBufferInterface(mFactory);
            gl::Error error = mStreamingBufferInt->reserveBufferSpace(INITIAL_INDEX_BUFFER_SIZE,
                                                                      GL_UNSIGNED_INT);
            if (error.isError())
            {
                SafeDelete(mStreamingBufferInt);
                return error;
            }
        }

        *outBuffer = mStreamingBufferInt;
        return gl::Error(GL_NO_ERROR);
    }
    else
    {
        ASSERT(destinationIndexType == GL_UNSIGNED_SHORT);

        if (!mStreamingBufferShort)
        {
            mStreamingBufferShort = new StreamingIndexBufferInterface(mFactory);
            gl::Error error = mStreamingBufferShort->reserveBufferSpace(INITIAL_INDEX_BUFFER_SIZE,
                                                                        GL_UNSIGNED_SHORT);
            if (error.isError())
            {
                SafeDelete(mStreamingBufferShort);
                return error;
            }
        }

        *outBuffer = mStreamingBufferShort;
        return gl::Error(GL_NO_ERROR);
    }
}

}
