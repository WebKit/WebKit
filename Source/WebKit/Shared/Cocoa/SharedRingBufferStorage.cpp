/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "SharedRingBufferStorage.h"

#if USE(MEDIATOOLBOX)

namespace WebKit {

void SharedRingBufferStorage::setStorage(RefPtr<SharedMemory>&& storage)
{
    ASSERT(storage || !m_readOnly);
    m_storage = WTFMove(storage);
    if (m_client)
        m_client->storageChanged(m_storage.get());
}

void SharedRingBufferStorage::setReadOnly(bool readOnly)
{
    ASSERT(m_storage || !readOnly);
    m_readOnly = readOnly;
}

void SharedRingBufferStorage::allocate(size_t byteCount)
{
    if (!m_readOnly) {
        auto sharedMemory = SharedMemory::allocate(byteCount + sizeof(FrameBounds));
        new (NotNull, sharedMemory->data()) FrameBounds;
        setStorage(WTFMove(sharedMemory));
    }
}

void SharedRingBufferStorage::deallocate()
{
    if (!m_readOnly)
        setStorage(nullptr);
}

auto SharedRingBufferStorage::sharedFrameBounds() -> FrameBounds*
{
    return m_storage ? reinterpret_cast<FrameBounds*>(m_storage->data()) : nullptr;
}

void* SharedRingBufferStorage::data()
{
    return m_storage ? static_cast<Byte*>(m_storage->data()) + sizeof(FrameBounds) : nullptr;
}

void SharedRingBufferStorage::getCurrentFrameBounds(uint64_t& startFrame, uint64_t& endFrame)
{
    startFrame = m_startFrame;
    endFrame = m_endFrame;
}

void SharedRingBufferStorage::setCurrentFrameBounds(uint64_t startFrame, uint64_t endFrame)
{
    m_startFrame = startFrame;
    m_endFrame = endFrame;

    ASSERT(!m_readOnly);
    if (m_readOnly)
        return;

    auto* sharedBounds = sharedFrameBounds();
    if (!sharedBounds)
        return;

    unsigned indexToWrite = (sharedBounds->boundsBufferIndex.load(std::memory_order_acquire) + 1) % boundsBufferSize;
    sharedBounds->boundsBuffer[indexToWrite] = std::make_pair(startFrame, endFrame);
    sharedBounds->boundsBufferIndex.store(indexToWrite, std::memory_order_release);
}

void SharedRingBufferStorage::updateFrameBounds()
{
    auto* sharedBounds = sharedFrameBounds();
    if (!sharedBounds) {
        m_startFrame = m_endFrame = 0;
        return;
    }

    auto pair = sharedBounds->boundsBuffer[sharedBounds->boundsBufferIndex.load(std::memory_order_acquire)];
    m_startFrame = pair.first;
    m_endFrame = pair.second;
}

void SharedRingBufferStorage::flush()
{
    m_startFrame = m_endFrame = 0;
}

} // namespace WebKit

#endif
