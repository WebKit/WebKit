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
#include "WebCoreArgumentCoders.h"
#include <WebCore/GraphicsContext.h>

using namespace WebCore;

namespace WebKit {
    
static unsigned calculateBytesPerPixel(ShareableBitmap::Flags flags)
{
    if (flags & ShareableBitmap::SupportsExtendedColor)
        return 8; // for extended color, we are using half-float representations
    return 4;
}

ShareableBitmap::Handle::Handle()
    : m_flags(0)
{
}

void ShareableBitmap::Handle::encode(IPC::Encoder& encoder) const
{
    encoder << m_handle;
    encoder << m_size;
    encoder << m_flags;
}

bool ShareableBitmap::Handle::decode(IPC::Decoder& decoder, Handle& handle)
{
    if (!decoder.decode(handle.m_handle))
        return false;
    if (!decoder.decode(handle.m_size))
        return false;
    if (!decoder.decode(handle.m_flags))
        return false;
    handle.m_bytesPerPixel = calculateBytesPerPixel(handle.m_flags);
    return true;
}

void ShareableBitmap::Handle::clear()
{
    m_handle.clear();
    m_size = IntSize();
    m_flags = Flag::NoFlags;
    m_bytesPerPixel = calculateBytesPerPixel(m_flags);
}

RefPtr<ShareableBitmap> ShareableBitmap::create(const IntSize& size, Flags flags)
{
    unsigned bytesPerPixel = calculateBytesPerPixel(flags);
    auto numBytes = numBytesForSize(size, bytesPerPixel);
    if (numBytes.hasOverflowed())
        return nullptr;

    void* data = 0;
    if (!tryFastMalloc(numBytes.unsafeGet()).getValue(data))
        return nullptr;

    return adoptRef(new ShareableBitmap(size, flags, data));
}

RefPtr<ShareableBitmap> ShareableBitmap::createShareable(const IntSize& size, Flags flags)
{
    unsigned bytesPerPixel = calculateBytesPerPixel(flags);
    auto numBytes = numBytesForSize(size, bytesPerPixel);
    if (numBytes.hasOverflowed())
        return nullptr;

    RefPtr<SharedMemory> sharedMemory = SharedMemory::allocate(numBytes.unsafeGet());
    if (!sharedMemory)
        return nullptr;

    return adoptRef(new ShareableBitmap(size, flags, sharedMemory));
}

RefPtr<ShareableBitmap> ShareableBitmap::create(const IntSize& size, Flags flags, RefPtr<SharedMemory> sharedMemory)
{
    ASSERT(sharedMemory);

    unsigned bytesPerPixel = calculateBytesPerPixel(flags);
    auto numBytes = numBytesForSize(size, bytesPerPixel);
    if (numBytes.hasOverflowed())
        return nullptr;
    if (sharedMemory->size() < numBytes.unsafeGet()) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    
    return adoptRef(new ShareableBitmap(size, flags, sharedMemory));
}

RefPtr<ShareableBitmap> ShareableBitmap::create(const Handle& handle, SharedMemory::Protection protection)
{
    // Create the shared memory.
    auto sharedMemory = SharedMemory::map(handle.m_handle, protection);
    if (!sharedMemory)
        return nullptr;

    return create(handle.m_size, handle.m_flags, WTFMove(sharedMemory));
}

bool ShareableBitmap::createHandle(Handle& handle, SharedMemory::Protection protection) const
{
    ASSERT(isBackedBySharedMemory());

    if (!m_sharedMemory->createHandle(handle.m_handle, protection))
        return false;
    handle.m_size = m_size;
    handle.m_flags = m_flags;
    handle.m_bytesPerPixel = m_bytesPerPixel;
    return true;
}

ShareableBitmap::ShareableBitmap(const IntSize& size, Flags flags, void* data)
    : m_size(size)
    , m_flags(flags)
    , m_data(data)
{
    m_bytesPerPixel = calculateBytesPerPixel(flags);
}

ShareableBitmap::ShareableBitmap(const IntSize& size, Flags flags, RefPtr<SharedMemory> sharedMemory)
    : m_size(size)
    , m_flags(flags)
    , m_sharedMemory(sharedMemory)
    , m_data(0)
{
    m_bytesPerPixel = calculateBytesPerPixel(flags);
}

ShareableBitmap::~ShareableBitmap()
{
    if (!isBackedBySharedMemory())
        fastFree(m_data);
}

void* ShareableBitmap::data() const
{
    if (isBackedBySharedMemory())
        return m_sharedMemory->data();

    ASSERT(m_data);
    return m_data;
}

} // namespace WebKit
