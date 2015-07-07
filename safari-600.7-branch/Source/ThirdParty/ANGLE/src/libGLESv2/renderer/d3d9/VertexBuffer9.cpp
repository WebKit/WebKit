#include "precompiled.h"
//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// VertexBuffer9.cpp: Defines the D3D9 VertexBuffer implementation.

#include "libGLESv2/renderer/d3d9/VertexBuffer9.h"
#include "libGLESv2/renderer/vertexconversion.h"
#include "libGLESv2/renderer/BufferStorage.h"
#include "libGLESv2/VertexAttribute.h"
#include "libGLESv2/renderer/d3d9/Renderer9.h"
#include "libGLESv2/renderer/d3d9/formatutils9.h"

#include "libGLESv2/Buffer.h"

namespace rx
{

VertexBuffer9::VertexBuffer9(rx::Renderer9 *const renderer) : mRenderer(renderer)
{
    mVertexBuffer = NULL;
    mBufferSize = 0;
    mDynamicUsage = false;
}

VertexBuffer9::~VertexBuffer9()
{
    SafeRelease(mVertexBuffer);
}

bool VertexBuffer9::initialize(unsigned int size, bool dynamicUsage)
{
    SafeRelease(mVertexBuffer);

    updateSerial();

    if (size > 0)
    {
        DWORD flags = D3DUSAGE_WRITEONLY;
        if (dynamicUsage)
        {
            flags |= D3DUSAGE_DYNAMIC;
        }

        HRESULT result = mRenderer->createVertexBuffer(size, flags, &mVertexBuffer);

        if (FAILED(result))
        {
            ERR("Out of memory allocating a vertex buffer of size %lu.", size);
            return false;
        }
    }

    mBufferSize = size;
    mDynamicUsage = dynamicUsage;
    return true;
}

VertexBuffer9 *VertexBuffer9::makeVertexBuffer9(VertexBuffer *vertexBuffer)
{
    ASSERT(HAS_DYNAMIC_TYPE(VertexBuffer9*, vertexBuffer));
    return static_cast<VertexBuffer9*>(vertexBuffer);
}

bool VertexBuffer9::storeVertexAttributes(const gl::VertexAttribute &attrib, const gl::VertexAttribCurrentValueData &currentValue,
                                          GLint start, GLsizei count, GLsizei instances, unsigned int offset)
{
    if (mVertexBuffer)
    {
        gl::Buffer *buffer = attrib.mBoundBuffer.get();

        int inputStride = attrib.stride();
        int elementSize = attrib.typeSize();

        DWORD lockFlags = mDynamicUsage ? D3DLOCK_NOOVERWRITE : 0;

        void *mapPtr = NULL;

        unsigned int mapSize;
        if (!spaceRequired(attrib, count, instances, &mapSize))
        {
            return false;
        }

        HRESULT result = mVertexBuffer->Lock(offset, mapSize, &mapPtr, lockFlags);

        if (FAILED(result))
        {
            ERR("Lock failed with error 0x%08x", result);
            return false;
        }

        const char *input = NULL;
        if (attrib.mArrayEnabled)
        {
            if (buffer)
            {
                BufferStorage *storage = buffer->getStorage();
                input = static_cast<const char*>(storage->getData()) + static_cast<int>(attrib.mOffset);
            }
            else
            {
                input = static_cast<const char*>(attrib.mPointer);
            }
        }
        else
        {
            input = reinterpret_cast<const char*>(currentValue.FloatValues);
        }

        if (instances == 0 || attrib.mDivisor == 0)
        {
            input += inputStride * start;
        }

        gl::VertexFormat vertexFormat(attrib, currentValue.Type);
        bool needsConversion = (d3d9::GetVertexConversionType(vertexFormat) & VERTEX_CONVERT_CPU) > 0;

        if (!needsConversion && inputStride == elementSize)
        {
            memcpy(mapPtr, input, count * inputStride);
        }
        else
        {
            VertexCopyFunction copyFunction = d3d9::GetVertexCopyFunction(vertexFormat);
            copyFunction(input, inputStride, count, mapPtr);
        }

        mVertexBuffer->Unlock();

        return true;
    }
    else
    {
        ERR("Vertex buffer not initialized.");
        return false;
    }
}

bool VertexBuffer9::getSpaceRequired(const gl::VertexAttribute &attrib, GLsizei count, GLsizei instances,
                                     unsigned int *outSpaceRequired) const
{
    return spaceRequired(attrib, count, instances, outSpaceRequired);
}

unsigned int VertexBuffer9::getBufferSize() const
{
    return mBufferSize;
}

bool VertexBuffer9::setBufferSize(unsigned int size)
{
    if (size > mBufferSize)
    {
        return initialize(size, mDynamicUsage);
    }
    else
    {
        return true;
    }
}

bool VertexBuffer9::discard()
{
    if (mVertexBuffer)
    {
        void *dummy;
        HRESULT result;

        result = mVertexBuffer->Lock(0, 1, &dummy, D3DLOCK_DISCARD);
        if (FAILED(result))
        {
            ERR("Discard lock failed with error 0x%08x", result);
            return false;
        }

        result = mVertexBuffer->Unlock();
        if (FAILED(result))
        {
            ERR("Discard unlock failed with error 0x%08x", result);
            return false;
        }

        return true;
    }
    else
    {
        ERR("Vertex buffer not initialized.");
        return false;
    }
}

IDirect3DVertexBuffer9 * VertexBuffer9::getBuffer() const
{
    return mVertexBuffer;
}

bool VertexBuffer9::spaceRequired(const gl::VertexAttribute &attrib, std::size_t count, GLsizei instances,
                                  unsigned int *outSpaceRequired)
{
    gl::VertexFormat vertexFormat(attrib, GL_FLOAT);
    unsigned int elementSize = d3d9::GetVertexElementSize(vertexFormat);

    if (attrib.mArrayEnabled)
    {
        unsigned int elementCount = 0;
        if (instances == 0 || attrib.mDivisor == 0)
        {
            elementCount = count;
        }
        else
        {
            if (static_cast<unsigned int>(instances) < std::numeric_limits<unsigned int>::max() - (attrib.mDivisor - 1))
            {
                // Round up
                elementCount = (static_cast<unsigned int>(instances) + (attrib.mDivisor - 1)) / attrib.mDivisor;
            }
            else
            {
                elementCount = static_cast<unsigned int>(instances) / attrib.mDivisor;
            }
        }

        if (elementSize <= std::numeric_limits<unsigned int>::max() / elementCount)
        {
            if (outSpaceRequired)
            {
                *outSpaceRequired = elementSize * elementCount;
            }
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        const unsigned int elementSize = 4;
        if (outSpaceRequired)
        {
            *outSpaceRequired = elementSize * 4;
        }
        return true;
    }
}

}
