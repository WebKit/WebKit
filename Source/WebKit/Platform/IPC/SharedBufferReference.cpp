/*
 * Copyright (C) 2018-2022 Apple Inc. All rights reserved.
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
#include "SharedBufferReference.h"

#include "Decoder.h"
#include "Encoder.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/SharedMemory.h>

namespace IPC {

using namespace WebCore;
using namespace WebKit;

#if !USE(UNIX_DOMAIN_SOCKETS)
SharedBufferReference::SharedBufferReference(std::optional<SerializableBuffer>&& serializableBuffer)
{
    if (!serializableBuffer)
        return;

    if (!serializableBuffer->size) {
        m_buffer = SharedBuffer::create();
        return;
    }

    if (!serializableBuffer->handle)
        return;

    auto sharedMemoryBuffer = SharedMemory::map(WTFMove(*serializableBuffer->handle), SharedMemory::Protection::ReadOnly);
    if (!sharedMemoryBuffer || sharedMemoryBuffer->size() < serializableBuffer->size)
        return;

    m_size = serializableBuffer->size;
    m_memory = WTFMove(sharedMemoryBuffer);
}

auto SharedBufferReference::serializableBuffer() const -> std::optional<SerializableBuffer>
{
    if (isNull())
        return std::nullopt;
    if (!m_size)
        return SerializableBuffer { 0, std::nullopt };
    auto sharedMemoryBuffer = m_memory ? m_memory : SharedMemory::copyBuffer(*m_buffer);
    return SerializableBuffer { m_size, sharedMemoryBuffer->createHandle(SharedMemory::Protection::ReadOnly) };
}
#endif

RefPtr<WebCore::SharedBuffer> SharedBufferReference::unsafeBuffer() const
{
#if !USE(UNIX_DOMAIN_SOCKETS)
    RELEASE_ASSERT_WITH_MESSAGE(isEmpty() || (!m_buffer && m_memory), "Must only be called on IPC's receiver side");

    if (m_memory)
        return m_memory->createSharedBuffer(m_size);
#endif
    if (m_buffer)
        return m_buffer->makeContiguous();
    return nullptr;
}

const uint8_t* SharedBufferReference::data() const
{
#if !USE(UNIX_DOMAIN_SOCKETS)
    RELEASE_ASSERT_WITH_MESSAGE(isEmpty() || (!m_buffer && m_memory), "Must only be called on IPC's receiver side");

    if (m_memory)
        return static_cast<uint8_t*>(m_memory->data());
#endif
    if (!m_buffer)
        return nullptr;

    if (!m_buffer->isContiguous())
        m_buffer = m_buffer->makeContiguous();

    return downcast<SharedBuffer>(m_buffer.get())->data();
}

RefPtr<WebCore::SharedMemory> SharedBufferReference::sharedCopy() const
{
    if (!m_size)
        return nullptr;
    return SharedMemory::copyBuffer(*unsafeBuffer());
}

} // namespace IPC
