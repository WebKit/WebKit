/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary Fform must reproduce the above copyright
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
#include "StreamConnectionBuffer.h"

#include "WebCoreArgumentCoders.h"

namespace IPC {

static Ref<WebKit::SharedMemory> createMemory(size_t size)
{
    auto memory = WebKit::SharedMemory::allocate(size);
    if (!memory)
        CRASH();
    return memory.releaseNonNull();
}

StreamConnectionBuffer::StreamConnectionBuffer(size_t memorySize)
    : m_dataSize(memorySize - headerSize())
    , m_sharedMemory(createMemory(memorySize))
{
    ASSERT(m_dataSize <= maximumSize());
}

StreamConnectionBuffer::StreamConnectionBuffer(Ref<WebKit::SharedMemory>&& memory, size_t memorySize, Semaphore&& clientWaitSemaphore)
    : m_dataSize(memorySize - headerSize())
    , m_sharedMemory(WTFMove(memory))
    , m_clientWaitSemaphore(WTFMove(clientWaitSemaphore))
{
    ASSERT(m_dataSize <= maximumSize());
}

StreamConnectionBuffer::StreamConnectionBuffer(StreamConnectionBuffer&& other) = default;

StreamConnectionBuffer::~StreamConnectionBuffer() = default;

StreamConnectionBuffer& StreamConnectionBuffer::operator=(StreamConnectionBuffer&& other)
{
    if (this != &other) {
        m_dataSize = other.m_dataSize;
        m_sharedMemory = WTFMove(other.m_sharedMemory);
        m_clientWaitSemaphore = WTFMove(other.m_clientWaitSemaphore);
    }
    return *this;
}

void StreamConnectionBuffer::encode(Encoder& encoder) const
{
    WebKit::SharedMemory::Handle handle;
    if (!m_sharedMemory->createHandle(handle, WebKit::SharedMemory::Protection::ReadWrite))
        CRASH();
    WebKit::SharedMemory::IPCHandle ipcHandle { WTFMove(handle), m_sharedMemory->size() };
    encoder << ipcHandle;
    encoder << m_clientWaitSemaphore;
}

std::optional<StreamConnectionBuffer> StreamConnectionBuffer::decode(Decoder& decoder)
{
    std::optional<WebKit::SharedMemory::IPCHandle> ipcHandle;
    decoder >> ipcHandle;
    if (!ipcHandle)
        return std::nullopt;
    std::optional<Semaphore> semaphore;
    decoder >> semaphore;
    if (!semaphore)
        return std::nullopt;
    size_t dataSize = static_cast<size_t>(ipcHandle->dataSize);
    if (dataSize < headerSize())
        return std::nullopt;
    if (dataSize > headerSize() + maximumSize())
        return std::nullopt;
    auto sharedMemory = WebKit::SharedMemory::map(ipcHandle->handle, WebKit::SharedMemory::Protection::ReadWrite);
    if (sharedMemory->size() < dataSize)
        return std::nullopt;
    return StreamConnectionBuffer { sharedMemory.releaseNonNull(), dataSize,  WTFMove(*semaphore) };
}

}
