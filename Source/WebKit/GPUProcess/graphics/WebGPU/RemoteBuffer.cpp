/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "RemoteBuffer.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteBufferMessages.h"
#include "StreamServerConnection.h"
#include "WebGPUObjectHeap.h"

namespace WebKit {

RemoteBuffer::RemoteBuffer(WebCore::WebGPU::Buffer& buffer, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, WebGPUIdentifier identifier)
    : m_backing(buffer)
    , m_objectHeap(objectHeap)
    , m_streamConnection(WTFMove(streamConnection))
    , m_identifier(identifier)
{
    m_streamConnection->startReceivingMessages(*this, Messages::RemoteBuffer::messageReceiverName(), m_identifier.toUInt64());
}

RemoteBuffer::~RemoteBuffer() = default;

void RemoteBuffer::stopListeningForIPC()
{
    m_streamConnection->stopReceivingMessages(Messages::RemoteBuffer::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteBuffer::mapAsync(WebCore::WebGPU::MapModeFlags mapModeFlags, WebCore::WebGPU::Size64 offset, std::optional<WebCore::WebGPU::Size64> size, CompletionHandler<void(std::optional<Vector<uint8_t>>&&)>&& callback)
{
    m_isMapped = true;
    m_mapModeFlags = mapModeFlags;

    m_backing->mapAsync(mapModeFlags, offset, size, [protectedThis = Ref<RemoteBuffer>(*this), callback = WTFMove(callback)] (bool success) mutable {
        if (!success) {
            callback(std::nullopt);
            return;
        }

        auto mappedRange = protectedThis->m_backing->getMappedRange(0, std::nullopt);
        protectedThis->m_mappedRange = mappedRange;
        callback(Vector<uint8_t>(static_cast<const uint8_t*>(mappedRange.source), mappedRange.byteLength));
    });
}

void RemoteBuffer::getMappedRange(WebCore::WebGPU::Size64 offset, std::optional<WebCore::WebGPU::Size64> size, CompletionHandler<void(std::optional<Vector<uint8_t>>&&)>&& callback)
{
    auto mappedRange = m_backing->getMappedRange(offset, size);
    m_mappedRange = mappedRange;
    m_mapModeFlags = { WebCore::WebGPU::MapMode::Write };
    m_isMapped = true;

    callback(Vector<uint8_t>(static_cast<const uint8_t*>(mappedRange.source), mappedRange.byteLength));
}

void RemoteBuffer::unmap(Vector<uint8_t>&& data)
{
    if (m_isMapped && m_mappedRange && m_mappedRange->byteLength >= data.size() && m_mapModeFlags.contains(WebCore::WebGPU::MapMode::Write))
        memcpy(m_mappedRange->source, data.data(), data.size());

    m_backing->unmap();
    m_isMapped = false;
    m_mappedRange = std::nullopt;
    m_mapModeFlags = { };
}

void RemoteBuffer::destroy()
{
    m_backing->destroy();
}

void RemoteBuffer::destruct()
{
    m_objectHeap.removeObject(m_identifier);
}

void RemoteBuffer::setLabel(String&& label)
{
    m_backing->setLabel(WTFMove(label));
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
