//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// geometry/VertexDataManager.h: Defines the VertexDataManager, a class that
// runs the Buffer translation process.

#include "libGLESv2/geometry/VertexDataManager.h"

#include <limits>

#include "common/debug.h"

#include "libGLESv2/Buffer.h"
#include "libGLESv2/Program.h"

#include "libGLESv2/geometry/backend.h"
#include "libGLESv2/geometry/IndexDataManager.h"

namespace
{
    enum { INITIAL_STREAM_BUFFER_SIZE = 1024*1024 };
}

namespace gl
{

VertexDataManager::VertexDataManager(Context *context, BufferBackEnd *backend)
    : mContext(context), mBackend(backend), mDirtyCurrentValues(true), mCurrentValueOffset(0)
{
    mStreamBuffer = mBackend->createVertexBuffer(INITIAL_STREAM_BUFFER_SIZE);
    try
    {
        mCurrentValueBuffer = mBackend->createVertexBufferForStrideZero(4 * sizeof(float) * MAX_VERTEX_ATTRIBS);
    }
    catch (...)
    {
        delete mStreamBuffer;
        throw;
    }
}

VertexDataManager::~VertexDataManager()
{
    delete mStreamBuffer;
    delete mCurrentValueBuffer;
}

std::bitset<MAX_VERTEX_ATTRIBS> VertexDataManager::getActiveAttribs() const
{
    std::bitset<MAX_VERTEX_ATTRIBS> active;

    Program *program = mContext->getCurrentProgram();

    for (int attributeIndex = 0; attributeIndex < MAX_VERTEX_ATTRIBS; attributeIndex++)
    {
        active[attributeIndex] = (program->getSemanticIndex(attributeIndex) != -1);
    }

    return active;
}

GLenum VertexDataManager::preRenderValidate(GLint start, GLsizei count,
                                            TranslatedAttribute *translated)
{
    const AttributeState *attribs = mContext->getVertexAttribBlock();
    const std::bitset<MAX_VERTEX_ATTRIBS> activeAttribs = getActiveAttribs();

    for (int i = 0; i < MAX_VERTEX_ATTRIBS; i++)
    {
        if (!activeAttribs[i] && attribs[i].mEnabled && attribs[i].mBoundBuffer != 0 && !mContext->getBuffer(attribs[i].mBoundBuffer))
            return GL_INVALID_OPERATION;
    }

    for (int i = 0; i < MAX_VERTEX_ATTRIBS; i++)
    {
        translated[i].enabled = activeAttribs[i];
    }

    bool usesCurrentValues = false;

    for (int i = 0; i < MAX_VERTEX_ATTRIBS; i++)
    {
        if (activeAttribs[i] && !attribs[i].mEnabled)
        {
            usesCurrentValues = true;
            break;
        }
    }

    // Handle the identity-mapped attributes.
    // Process array attributes.

    std::size_t requiredSpace = 0;

    for (int i = 0; i < MAX_VERTEX_ATTRIBS; i++)
    {
        if (activeAttribs[i] && attribs[i].mEnabled)
        {
            requiredSpace += spaceRequired(attribs[i], count);
        }
    }

    if (requiredSpace > mStreamBuffer->size())
    {
        std::size_t newSize = std::max(requiredSpace, 3 * mStreamBuffer->size() / 2); // 1.5 x mStreamBuffer->size() is arbitrary and should be checked to see we don't have too many reallocations.

        TranslatedVertexBuffer *newStreamBuffer = mBackend->createVertexBuffer(newSize);

        delete mStreamBuffer;
        mStreamBuffer = newStreamBuffer;
    }

    mStreamBuffer->reserveSpace(requiredSpace);

    for (size_t i = 0; i < MAX_VERTEX_ATTRIBS; i++)
    {
        if (activeAttribs[i] && attribs[i].mEnabled)
        {
            FormatConverter formatConverter = mBackend->getFormatConverter(attribs[i].mType, attribs[i].mSize, attribs[i].mNormalized);

            translated[i].nonArray = false;
            translated[i].type = attribs[i].mType;
            translated[i].size = attribs[i].mSize;
            translated[i].normalized = attribs[i].mNormalized;
            translated[i].stride = formatConverter.outputVertexSize;
            translated[i].buffer = mStreamBuffer;

            size_t inputStride = interpretGlStride(attribs[i]);
            size_t elementSize = typeSize(attribs[i].mType) * attribs[i].mSize;

            void *output = mStreamBuffer->map(spaceRequired(attribs[i], count), &translated[i].offset);

            const void *input;
            if (attribs[i].mBoundBuffer)
            {
                Buffer *buffer = mContext->getBuffer(attribs[i].mBoundBuffer);

                size_t offset = reinterpret_cast<size_t>(attribs[i].mPointer);

                // Before we calculate the required size below, make sure it can be computed without integer overflow.
                if (std::numeric_limits<std::size_t>::max() - start < static_cast<std::size_t>(count)
                    || std::numeric_limits<std::size_t>::max() / inputStride < static_cast<std::size_t>(start + count - 1) // it's a prerequisite that count >= 1, so start+count-1 >= 0.
                    || std::numeric_limits<std::size_t>::max() - offset < inputStride * (start + count - 1)
                    || std::numeric_limits<std::size_t>::max() - elementSize < offset + inputStride * (start + count - 1) + elementSize)
                {
                    mStreamBuffer->unmap();
                    return GL_INVALID_OPERATION;
                }

                if (offset + inputStride * (start + count - 1) + elementSize > buffer->size())
                {
                    mStreamBuffer->unmap();
                    return GL_INVALID_OPERATION;
                }

                input = static_cast<const char*>(buffer->data()) + offset;
            }
            else
            {
                input = attribs[i].mPointer;
            }

            input = static_cast<const char*>(input) + inputStride * start;

            if (formatConverter.identity && inputStride == elementSize)
            {
                memcpy(output, input, count * inputStride);
            }
            else
            {
                formatConverter.convertArray(input, inputStride, count, output);
            }

            mStreamBuffer->unmap();
        }
    }

    if (usesCurrentValues)
    {
        processNonArrayAttributes(attribs, activeAttribs, translated, count);
    }

    return GL_NO_ERROR;
}

std::size_t VertexDataManager::typeSize(GLenum type) const
{
    switch (type)
    {
      case GL_BYTE: case GL_UNSIGNED_BYTE: return sizeof(GLbyte);
      case GL_SHORT: case GL_UNSIGNED_SHORT: return sizeof(GLshort);
      case GL_FIXED: return sizeof(GLfixed);
      case GL_FLOAT: return sizeof(GLfloat);
      default: UNREACHABLE(); return sizeof(GLfloat);
    }
}

std::size_t VertexDataManager::interpretGlStride(const AttributeState &attrib) const
{
    return attrib.mStride ? attrib.mStride : typeSize(attrib.mType) * attrib.mSize;
}

// Round up x (>=0) to the next multiple of multiple (>0).
// 0 rounds up to 0.
std::size_t VertexDataManager::roundUp(std::size_t x, std::size_t multiple) const
{
    ASSERT(x >= 0);
    ASSERT(multiple > 0);

    std::size_t remainder = x % multiple;
    if (remainder != 0)
    {
        return x + multiple - remainder;
    }
    else
    {
        return x;
    }
}

std::size_t VertexDataManager::spaceRequired(const AttributeState &attrib, std::size_t maxVertex) const
{
    std::size_t size = mBackend->getFormatConverter(attrib.mType, attrib.mSize, attrib.mNormalized).outputVertexSize;
    size *= maxVertex;

    return roundUp(size, 4 * sizeof(GLfloat));
}

void VertexDataManager::processNonArrayAttributes(const AttributeState *attribs, const std::bitset<MAX_VERTEX_ATTRIBS> &activeAttribs, TranslatedAttribute *translated, std::size_t count)
{
    if (mDirtyCurrentValues)
    {
        std::size_t totalSize = 4 * sizeof(float) * MAX_VERTEX_ATTRIBS;

        mCurrentValueBuffer->reserveSpace(totalSize);

        float* currentValues = static_cast<float*>(mCurrentValueBuffer->map(totalSize, &mCurrentValueOffset));

        for (int i = 0; i < MAX_VERTEX_ATTRIBS; i++)
        {
            // This assumes that the GL_FLOATx4 is supported by the back-end. (For D3D9, it is a mandatory format.)
            currentValues[i*4+0] = attribs[i].mCurrentValue[0];
            currentValues[i*4+1] = attribs[i].mCurrentValue[1];
            currentValues[i*4+2] = attribs[i].mCurrentValue[2];
            currentValues[i*4+3] = attribs[i].mCurrentValue[3];
        }

        mCurrentValueBuffer->unmap();
    }

    for (std::size_t i = 0; i < MAX_VERTEX_ATTRIBS; i++)
    {
        if (activeAttribs[i] && !attribs[i].mEnabled)
        {
            translated[i].nonArray = true;

            translated[i].buffer = mCurrentValueBuffer;

            translated[i].type = GL_FLOAT;
            translated[i].size = 4;
            translated[i].normalized = false;
            translated[i].stride = 0;
            translated[i].offset = mCurrentValueOffset + 4 * sizeof(float) * i;
        }
    }
}

}
