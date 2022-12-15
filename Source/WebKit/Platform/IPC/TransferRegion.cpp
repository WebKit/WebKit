/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "TransferRegion.h"

namespace IPC {

constexpr size_t minimumTransferBufferAllocation { 64 * KB };
constexpr Seconds releaseUnusedBuffersTimeout = 5_s;

Ref<TransferRegionPool> TransferRegionPool::create(Connection& connection)
{
    return adoptRef(*new TransferRegionPool { connection } );
}

TransferRegionPool::TransferRegionPool(Connection& connection)
    : m_connection(&connection)
{
}

void TransferRegionPool::invalidate()
{
    std::optional<TransferBuffer> freeBuffer;
    {
        Locker locker { m_lock };
        unscheduleReleaseUnusedMemory();
        freeBuffer = WTFMove(m_free);
        m_connection = nullptr;
    }
}

std::optional<ScopedTransferRegion> TransferRegionPool::reserveRegion(size_t size)
{
    if (!size)
        return ScopedTransferRegion { *this, *TransferBuffer::create(0), 0 };

    {
        Locker locker { m_lock };
        if (m_free && m_free->size() >= size) {
            unscheduleReleaseUnusedMemory();
            return ScopedTransferRegion { *this, *std::exchange(m_free, std::nullopt), size };
        }
    }
    auto buffer = TransferBuffer::create(std::max(minimumTransferBufferAllocation, size));
    if (!buffer)
        return std::nullopt;
    return ScopedTransferRegion { *this, WTFMove(*buffer), size };
}

void TransferRegionPool::releaseTransferBuffer(TransferBuffer&& buffer)
{
    if (!buffer.size())
        return;
    RefPtr<Connection> connection;
    {
        Locker locker { m_lock };
        if (!m_free) {
            scheduleReleaseUnusedMemory();
            m_free = WTFMove(buffer);
            return;
        }
        std::swap(*m_free, buffer);
        connection = m_connection;
    }

    if (!connection || !buffer.hasBeenPassed())
        return;
    auto encoder = makeUniqueRef<Encoder>(MessageName::ReleaseTransferBuffer, buffer.identifier().toUInt64());
    connection->sendMessage(WTFMove(encoder), { });
}

void TransferRegionPool::scheduleReleaseUnusedMemory()
{
    if (m_releaseUnusedTimer)
        return;
    m_releaseUnusedTimer = RunLoop::main().dispatchAfter(releaseUnusedBuffersTimeout, [self = Ref { *this }] {
        self->releaseUnusedMemory();
    });
}

void TransferRegionPool::unscheduleReleaseUnusedMemory()
{
    if (!m_releaseUnusedTimer)
        return;
    m_releaseUnusedTimer->stop();
    m_releaseUnusedTimer->setFunction(nullptr); // FIXME: stopped DispatchTimer leaks.
    m_releaseUnusedTimer = nullptr;
}

void TransferRegionPool::releaseUnusedMemory()
{
    std::optional<TransferBuffer> buffer;
    RefPtr<Connection> connection;
    {
        Locker locker { m_lock };
        buffer = WTFMove(m_free);
        m_free = std::nullopt;
        connection = m_connection;
        unscheduleReleaseUnusedMemory();
    }
    if (!buffer || !connection)
        return;
    auto encoder = makeUniqueRef<Encoder>(MessageName::ReleaseTransferBuffer, buffer->identifier().toUInt64());
    connection->sendMessage(WTFMove(encoder), { });
}

Ref<TransferRegionMapper> TransferRegionMapper::create()
{
    return adoptRef(*new TransferRegionMapper);
}

std::optional<ScopedTransferRegionMapping> TransferRegionMapper::mapRegion(TransferRegion&& region, const WebCore::ProcessIdentity& resourceOwner)
{
    if (!region.size)
        return ScopedTransferRegionMapping { };
    if (!TransferBufferMap::isValidKey(region.identifier))
        return std::nullopt;
    Locker locker { m_lock };
    TransferBufferMap::iterator iterator;

    if (!region.buffer) {
        iterator = m_buffers.find(region.identifier);
        if (iterator == m_buffers.end())
            return std::nullopt;
    } else {
        region.buffer->setOwnershipOfMemory(resourceOwner, WebKit::MemoryLedger::Default);
        auto buffer = WebKit::SharedMemory::map(WTFMove(*region.buffer), WebKit::SharedMemory::Protection::ReadWrite);
        if (!buffer)
            return std::nullopt;
        auto addResult = m_buffers.add(region.identifier, buffer.releaseNonNull());
        if (!addResult.isNewEntry)
            return std::nullopt;
        iterator = addResult.iterator;
    }
    auto& buffer = iterator->value;
    if (region.size > buffer->size())
        return std::nullopt;
    return ScopedTransferRegionMapping { buffer, region.size };
}

void TransferRegionMapper::releaseBufferMapping(TransferBufferIdentifier identifier)
{
    Locker locker { m_lock };
    m_buffers.remove(identifier);
}

void TransferRegionMapper::invalidate()
{
    Locker locker { m_lock };
    m_buffers.clear();
}

}
