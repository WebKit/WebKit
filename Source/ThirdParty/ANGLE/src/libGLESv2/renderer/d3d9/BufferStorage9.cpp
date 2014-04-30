#include "precompiled.h"
//
// Copyright (c) 2013-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// BufferStorage9.cpp Defines the BufferStorage9 class.

#include "libGLESv2/renderer/d3d9/BufferStorage9.h"
#include "common/debug.h"

namespace rx
{

BufferStorage9::BufferStorage9()
{
    mMemory = NULL;
    mAllocatedSize = 0;
    mSize = 0;
}

BufferStorage9::~BufferStorage9()
{
    SafeDeleteArray(mMemory);
}

BufferStorage9 *BufferStorage9::makeBufferStorage9(BufferStorage *bufferStorage)
{
    ASSERT(HAS_DYNAMIC_TYPE(BufferStorage9*, bufferStorage));
    return static_cast<BufferStorage9*>(bufferStorage);
}

void *BufferStorage9::getData()
{
    return mMemory;
}

void BufferStorage9::setData(const void* data, unsigned int size, unsigned int offset)
{
    if (!mMemory || offset + size > mAllocatedSize)
    {
        unsigned int newAllocatedSize = offset + size;
        void *newMemory = new char[newAllocatedSize];

        if (offset > 0 && mMemory && mAllocatedSize > 0)
        {
            memcpy(newMemory, mMemory, std::min(offset, mAllocatedSize));
        }

        delete[] mMemory;
        mMemory = newMemory;
        mAllocatedSize = newAllocatedSize;
    }

    mSize = std::max(mSize, offset + size);
    if (data)
    {
        memcpy(reinterpret_cast<char*>(mMemory) + offset, data, size);
    }
}

void BufferStorage9::copyData(BufferStorage* sourceStorage, unsigned int size,
                              unsigned int sourceOffset, unsigned int destOffset)
{
    BufferStorage9* source = makeBufferStorage9(sourceStorage);
    if (source)
    {
        void* sourceMemory = reinterpret_cast<char*>(source->mMemory) + sourceOffset;
        void* destMemory = reinterpret_cast<char*>(mMemory) + destOffset;

        memcpy(destMemory, sourceMemory, size);
    }
}

void BufferStorage9::clear()
{
    mSize = 0;
}

void BufferStorage9::markTransformFeedbackUsage()
{
    UNREACHABLE();
}

unsigned int BufferStorage9::getSize() const
{
    return mSize;
}

bool BufferStorage9::supportsDirectBinding() const
{
    return false;
}

// We do not suppot buffer mapping facility in D3D9
bool BufferStorage9::isMapped() const
{
    UNREACHABLE();
    return false;
}

void *BufferStorage9::map(GLbitfield access)
{
    UNREACHABLE();
    return NULL;
}

void BufferStorage9::unmap()
{
    UNREACHABLE();
}

}
