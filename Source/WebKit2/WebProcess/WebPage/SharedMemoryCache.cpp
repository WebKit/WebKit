/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "SharedMemoryCache.h"

#include "SharedMemory.h"

namespace WebKit {

// The time before we clear the shared memory cache.
static const double sharedMemoryClearCacheTimeout = 3;

SharedMemoryCache::SharedMemoryCache()
    : m_clearCacheTimer(RunLoop::current(), this, &SharedMemoryCache::clearCacheTimerFired)
{
}

SharedMemoryCache::~SharedMemoryCache()
{
}

PassRefPtr<SharedMemory> SharedMemoryCache::acquireSharedMemory(size_t size)
{
    if (m_lastUsedMemory && m_lastUsedMemory->size() >= size)
        return m_lastUsedMemory.release();

    m_lastUsedMemory = nullptr;
    return SharedMemory::create(size);
}

void SharedMemoryCache::releaseSharedMemory(PassRefPtr<SharedMemory> sharedMemory)
{
    size_t lastUsedSharedMemorySize = 0;
    if (m_lastUsedMemory)
        lastUsedSharedMemorySize = m_lastUsedMemory->size();

    if (sharedMemory->size() > lastUsedSharedMemorySize) {
        m_lastUsedMemory = sharedMemory;
        m_clearCacheTimer.startOneShot(sharedMemoryClearCacheTimeout);
    }
}

void SharedMemoryCache::clearCacheTimerFired()
{
    m_lastUsedMemory = nullptr;
}

} // namespace WebKit
