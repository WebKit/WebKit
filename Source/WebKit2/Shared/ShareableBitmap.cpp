/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ShareableBitmap.h"

#include "SharedMemory.h"
#include <WebCore/GraphicsContext.h>

using namespace WebCore;

namespace WebKit {

PassRefPtr<ShareableBitmap> ShareableBitmap::create(const WebCore::IntSize& size)
{
    size_t numBytes = numBytesNeededForBitmapSize(size);
    
    void* data = 0;
    if (!tryFastMalloc(numBytes).getValue(data))
        return 0;

    return adoptRef(new ShareableBitmap(size, data));
}

PassRefPtr<ShareableBitmap> ShareableBitmap::createShareable(const IntSize& size)
{
    size_t numBytes = numBytesNeededForBitmapSize(size);

    RefPtr<SharedMemory> sharedMemory = SharedMemory::create(numBytes);
    if (!sharedMemory)
        return 0;

    return adoptRef(new ShareableBitmap(size, sharedMemory));
}

PassRefPtr<ShareableBitmap> ShareableBitmap::create(const WebCore::IntSize& size, PassRefPtr<SharedMemory> sharedMemory)
{
    ASSERT(sharedMemory);

    size_t numBytes = numBytesNeededForBitmapSize(size);
    ASSERT_UNUSED(numBytes, sharedMemory->size() >= numBytes);
    
    return adoptRef(new ShareableBitmap(size, sharedMemory));
}

PassRefPtr<ShareableBitmap> ShareableBitmap::create(const WebCore::IntSize& size, const SharedMemory::Handle& handle)
{
    // Create the shared memory.
    RefPtr<SharedMemory> sharedMemory = SharedMemory::create(handle, SharedMemory::ReadWrite);
    if (!sharedMemory)
        return 0;

    return create(size, sharedMemory.release());
}

bool ShareableBitmap::createHandle(SharedMemory::Handle& handle)
{
    ASSERT(isBackedBySharedMemory());

    return m_sharedMemory->createHandle(handle, SharedMemory::ReadWrite);
}

ShareableBitmap::ShareableBitmap(const IntSize& size, void* data)
    : m_size(size)
    , m_data(data)
{
}

ShareableBitmap::ShareableBitmap(const IntSize& size, PassRefPtr<SharedMemory> sharedMemory)
    : m_size(size)
    , m_sharedMemory(sharedMemory)
    , m_data(0)
{
}

ShareableBitmap::~ShareableBitmap()
{
    if (!isBackedBySharedMemory())
        fastFree(m_data);
}

bool ShareableBitmap::resize(const IntSize& size)
{
    // We can't resize backing stores that are backed by shared memory.
    ASSERT(!isBackedBySharedMemory());

    if (size == m_size)
        return true;

    size_t newNumBytes = numBytesNeededForBitmapSize(size);
    
    // Try to resize.
    char* newData = 0;
    if (!tryFastRealloc(m_data, newNumBytes).getValue(newData)) {
        // We failed, but the backing store is still kept in a consistent state.
        return false;
    }

    m_size = size;
    m_data = newData;

    return true;
}

void* ShareableBitmap::data() const
{
    if (isBackedBySharedMemory())
        return m_sharedMemory->data();

    ASSERT(m_data);
    return m_data;
}

} // namespace WebKit
