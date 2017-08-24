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

ShareableBitmap::Handle::Handle()
{
}

void ShareableBitmap::Handle::encode(IPC::Encoder& encoder) const
{
    encoder << m_handle;
    encoder << m_size;
    encoder << m_configuration;
}

bool ShareableBitmap::Handle::decode(IPC::Decoder& decoder, Handle& handle)
{
    if (!decoder.decode(handle.m_handle))
        return false;
    if (!decoder.decode(handle.m_size))
        return false;
    if (!decoder.decode(handle.m_configuration))
        return false;
    return true;
}

void ShareableBitmap::Handle::clear()
{
    m_handle.clear();
    m_size = IntSize();
    m_configuration = { };
}

void ShareableBitmap::Configuration::encode(IPC::Encoder& encoder) const
{
    encoder << isOpaque;
#if PLATFORM(COCOA)
    encoder << colorSpace;
#endif
}

bool ShareableBitmap::Configuration::decode(IPC::Decoder& decoder, Configuration& configuration)
{
    if (!decoder.decode(configuration.isOpaque))
        return false;
#if PLATFORM(COCOA)
    if (!decoder.decode(configuration.colorSpace))
        return false;
#endif
    return true;
}

RefPtr<ShareableBitmap> ShareableBitmap::create(const IntSize& size, Configuration configuration)
{
    auto numBytes = numBytesForSize(size, configuration);
    if (numBytes.hasOverflowed())
        return nullptr;

    void* data = 0;
    if (!tryFastMalloc(numBytes.unsafeGet()).getValue(data))
        return nullptr;

    return adoptRef(new ShareableBitmap(size, configuration, data));
}

RefPtr<ShareableBitmap> ShareableBitmap::createShareable(const IntSize& size, Configuration configuration)
{
    auto numBytes = numBytesForSize(size, configuration);
    if (numBytes.hasOverflowed())
        return nullptr;

    RefPtr<SharedMemory> sharedMemory = SharedMemory::allocate(numBytes.unsafeGet());
    if (!sharedMemory)
        return nullptr;

    return adoptRef(new ShareableBitmap(size, configuration, sharedMemory));
}

RefPtr<ShareableBitmap> ShareableBitmap::create(const IntSize& size, Configuration configuration, RefPtr<SharedMemory> sharedMemory)
{
    ASSERT(sharedMemory);

    auto numBytes = numBytesForSize(size, configuration);
    if (numBytes.hasOverflowed())
        return nullptr;
    if (sharedMemory->size() < numBytes.unsafeGet()) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }
    
    return adoptRef(new ShareableBitmap(size, configuration, sharedMemory));
}

RefPtr<ShareableBitmap> ShareableBitmap::create(const Handle& handle, SharedMemory::Protection protection)
{
    // Create the shared memory.
    auto sharedMemory = SharedMemory::map(handle.m_handle, protection);
    if (!sharedMemory)
        return nullptr;

    return create(handle.m_size, handle.m_configuration, WTFMove(sharedMemory));
}

bool ShareableBitmap::createHandle(Handle& handle, SharedMemory::Protection protection) const
{
    ASSERT(isBackedBySharedMemory());

    if (!m_sharedMemory->createHandle(handle.m_handle, protection))
        return false;
    handle.m_size = m_size;
    handle.m_configuration = m_configuration;
    return true;
}

ShareableBitmap::ShareableBitmap(const IntSize& size, Configuration configuration, void* data)
    : m_size(size)
    , m_configuration(configuration)
    , m_data(data)
{
}

ShareableBitmap::ShareableBitmap(const IntSize& size, Configuration configuration, RefPtr<SharedMemory> sharedMemory)
    : m_size(size)
    , m_configuration(configuration)
    , m_sharedMemory(sharedMemory)
    , m_data(nullptr)
{
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

Checked<unsigned, RecordOverflow> ShareableBitmap::numBytesForSize(WebCore::IntSize size, const ShareableBitmap::Configuration& configuration)
{
    return calculateBytesPerRow(size, configuration) * size.height();
}

} // namespace WebKit
