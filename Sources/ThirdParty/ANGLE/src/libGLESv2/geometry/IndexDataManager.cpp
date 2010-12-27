//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// geometry/IndexDataManager.cpp: Defines the IndexDataManager, a class that
// runs the Buffer translation process for index buffers.

#include "libGLESv2/geometry/IndexDataManager.h"

#include "common/debug.h"

#include "libGLESv2/Buffer.h"
#include "libGLESv2/mathutil.h"
#include "libGLESv2/geometry/backend.h"

namespace
{
    enum { INITIAL_INDEX_BUFFER_SIZE = 4096 * sizeof(GLuint) };
}

namespace gl
{

IndexDataManager::IndexDataManager(Context *context, BufferBackEnd *backend)
  : mContext(context), mBackend(backend), mIntIndicesSupported(backend->supportIntIndices())
{
    mCountingBuffer = NULL;
    mCountingBufferSize = 0;

    mLineLoopBuffer = NULL;

    mStreamBufferShort = mBackend->createIndexBuffer(INITIAL_INDEX_BUFFER_SIZE, GL_UNSIGNED_SHORT);

    if (mIntIndicesSupported)
    {
        mStreamBufferInt = mBackend->createIndexBuffer(INITIAL_INDEX_BUFFER_SIZE, GL_UNSIGNED_INT);
    }
    else
    {
        mStreamBufferInt = NULL;
    }
}

IndexDataManager::~IndexDataManager()
{
    delete mStreamBufferShort;
    delete mStreamBufferInt;
    delete mCountingBuffer;
    delete mLineLoopBuffer;
}

namespace
{

template <class InputIndexType, class OutputIndexType>
void copyIndices(const InputIndexType *in, GLsizei count, OutputIndexType *out, GLuint *minIndex, GLuint *maxIndex)
{
    InputIndexType first = *in;
    GLuint minIndexSoFar = first;
    GLuint maxIndexSoFar = first;

    for (GLsizei i = 0; i < count; i++)
    {
        if (minIndexSoFar > *in) minIndexSoFar = *in;
        if (maxIndexSoFar < *in) maxIndexSoFar = *in;

        *out++ = *in++;
    }

    // It might be a line loop, so copy the loop index.
    *out = first;

    *minIndex = minIndexSoFar;
    *maxIndex = maxIndexSoFar;
}

}

GLenum IndexDataManager::preRenderValidate(GLenum mode, GLenum type, GLsizei count, Buffer *arrayElementBuffer, const void *indices, TranslatedIndexData *translated)
{
    ASSERT(type == GL_UNSIGNED_SHORT || type == GL_UNSIGNED_BYTE || type == GL_UNSIGNED_INT);
    ASSERT(count > 0);

    if (arrayElementBuffer != NULL)
    {
        GLsizei offset = reinterpret_cast<GLsizei>(indices);

        if (typeSize(type) * count + offset > static_cast<std::size_t>(arrayElementBuffer->size()))
        {
            return GL_INVALID_OPERATION;
        }

        indices = static_cast<const GLubyte*>(arrayElementBuffer->data()) + offset;
    }

    translated->count = count;

    std::size_t requiredSpace = spaceRequired(type, count);

    TranslatedIndexBuffer *streamIb = prepareIndexBuffer(type, requiredSpace);

    size_t offset;
    void *output = streamIb->map(requiredSpace, &offset);

    translated->buffer = streamIb;
    translated->offset = offset;
    translated->indexSize = indexSize(type);

    if (type == GL_UNSIGNED_BYTE)
    {
        const GLubyte *in = static_cast<const GLubyte*>(indices);
        GLushort *out = static_cast<GLushort*>(output);

        copyIndices(in, count, out, &translated->minIndex, &translated->maxIndex);
    }
    else if (type == GL_UNSIGNED_INT)
    {
        const GLuint *in = static_cast<const GLuint*>(indices);

        if (mIntIndicesSupported)
        {
            GLuint *out = static_cast<GLuint*>(output);

            copyIndices(in, count, out, &translated->minIndex, &translated->maxIndex);
        }
        else
        {
            // When 32-bit indices are unsupported, fake them by truncating to 16-bit.

            GLushort *out = static_cast<GLushort*>(output);

            copyIndices(in, count, out, &translated->minIndex, &translated->maxIndex);
        }
    }
    else
    {
        const GLushort *in = static_cast<const GLushort*>(indices);
        GLushort *out = static_cast<GLushort*>(output);

        copyIndices(in, count, out, &translated->minIndex, &translated->maxIndex);
    }

    streamIb->unmap();

    return GL_NO_ERROR;
}

std::size_t IndexDataManager::indexSize(GLenum type) const
{
    return (type == GL_UNSIGNED_INT && mIntIndicesSupported) ? sizeof(GLuint) : sizeof(GLushort);
}

std::size_t IndexDataManager::typeSize(GLenum type) const
{
    switch (type)
    {
      case GL_UNSIGNED_INT: return sizeof(GLuint);
      case GL_UNSIGNED_SHORT: return sizeof(GLushort);
      default: UNREACHABLE();
      case GL_UNSIGNED_BYTE: return sizeof(GLubyte);
    }
}

std::size_t IndexDataManager::spaceRequired(GLenum type, GLsizei count) const
{
    return (count + 1) * indexSize(type); // +1 because we always leave an extra for line loops
}

TranslatedIndexBuffer *IndexDataManager::prepareIndexBuffer(GLenum type, std::size_t requiredSpace)
{
    bool use32 = (type == GL_UNSIGNED_INT && mIntIndicesSupported);

    TranslatedIndexBuffer *streamIb = use32 ? mStreamBufferInt : mStreamBufferShort;

    if (requiredSpace > streamIb->size())
    {
        std::size_t newSize = std::max(requiredSpace, 2 * streamIb->size());

        TranslatedIndexBuffer *newStreamBuffer = mBackend->createIndexBuffer(newSize, use32 ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT);

        delete streamIb;

        streamIb = newStreamBuffer;

        if (use32)
        {
            mStreamBufferInt = streamIb;
        }
        else
        {
            mStreamBufferShort = streamIb;
        }
    }

    streamIb->reserveSpace(requiredSpace);

    return streamIb;
}

GLenum IndexDataManager::preRenderValidateUnindexed(GLenum mode, GLsizei count, TranslatedIndexData *indexInfo)
{
    if (count >= 65535) return GL_OUT_OF_MEMORY;

    if (mode == GL_LINE_LOOP)
    {
        // For line loops, create a single-use buffer that runs 0 - count-1, 0.
        delete mLineLoopBuffer;
        mLineLoopBuffer = mBackend->createIndexBuffer((count+1) * sizeof(unsigned short), GL_UNSIGNED_SHORT);

        unsigned short *indices = static_cast<unsigned short *>(mLineLoopBuffer->map());

        for (int i = 0; i < count; i++)
        {
            indices[i] = i;
        }

        indices[count] = 0;

        mLineLoopBuffer->unmap();

        indexInfo->buffer = mLineLoopBuffer;
        indexInfo->count = count + 1;
        indexInfo->maxIndex = count - 1;
    }
    else if (mCountingBufferSize < count)
    {
        mCountingBufferSize = std::max(static_cast<GLsizei>(ceilPow2(count)), mCountingBufferSize*2);

        delete mCountingBuffer;
        mCountingBuffer = mBackend->createIndexBuffer(count * sizeof(unsigned short), GL_UNSIGNED_SHORT);

        unsigned short *indices = static_cast<unsigned short *>(mCountingBuffer->map());

        for (int i = 0; i < count; i++)
        {
            indices[i] = i;
        }

        mCountingBuffer->unmap();

        indexInfo->buffer = mCountingBuffer;
        indexInfo->count = count;
        indexInfo->maxIndex = count - 1;
    }
    else
    {
        indexInfo->buffer = mCountingBuffer;
        indexInfo->count = count;
        indexInfo->maxIndex = count - 1;
    }

    indexInfo->indexSize = sizeof(unsigned short);
    indexInfo->minIndex = 0;
    indexInfo->offset = 0;

    return GL_NO_ERROR;
}

}
