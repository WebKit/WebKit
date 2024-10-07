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

#include <WebCore/SharedMemory.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteBuffer);

RemoteBuffer::RemoteBuffer(WebCore::WebGPU::Buffer& buffer, WebGPU::ObjectHeap& objectHeap, Ref<IPC::StreamServerConnection>&& streamConnection, RemoteGPU& gpu, bool mappedAtCreation, WebGPUIdentifier identifier)
    : m_backing(buffer)
    , m_objectHeap(objectHeap)
    , m_streamConnection(WTFMove(streamConnection))
    , m_gpu(gpu)
    , m_identifier(identifier)
    , m_isMapped(mappedAtCreation)
    , m_mapModeFlags(mappedAtCreation ? WebCore::WebGPU::MapModeFlags(WebCore::WebGPU::MapMode::Write) : WebCore::WebGPU::MapModeFlags())
{
    protectedStreamConnection()->startReceivingMessages(*this, Messages::RemoteBuffer::messageReceiverName(), m_identifier.toUInt64());
}

RemoteBuffer::~RemoteBuffer() = default;

void RemoteBuffer::stopListeningForIPC()
{
    protectedStreamConnection()->stopReceivingMessages(Messages::RemoteBuffer::messageReceiverName(), m_identifier.toUInt64());
}

void RemoteBuffer::mapAsync(WebCore::WebGPU::MapModeFlags mapModeFlags, WebCore::WebGPU::Size64 offset, std::optional<WebCore::WebGPU::Size64> size, CompletionHandler<void(bool)>&& callback)
{
    m_isMapped = true;
    m_mapModeFlags = mapModeFlags;

    protectedBacking()->mapAsync(mapModeFlags, offset, size, [protectedThis = Ref<RemoteBuffer>(*this), callback = WTFMove(callback)] (bool success) mutable {
        if (!success) {
            callback(false);
            return;
        }

        callback(true);
    });
}

void RemoteBuffer::getMappedRange(WebCore::WebGPU::Size64 offset, std::optional<WebCore::WebGPU::Size64> size, CompletionHandler<void(std::optional<Vector<uint8_t>>&&)>&& callback)
{
    protectedBacking()->getMappedRange(offset, size, [&] (auto mappedRange) {
        m_isMapped = true;
        callback(mappedRange);
    });
}

void RemoteBuffer::unmap()
{
    if (m_isMapped)
        protectedBacking()->unmap();
    m_isMapped = false;
    m_mapModeFlags = { };
}

void RemoteBuffer::copy(std::optional<WebCore::SharedMemoryHandle>&& dataHandle, size_t offset, CompletionHandler<void(bool)>&& completionHandler)
{
    auto sharedData = dataHandle ? WebCore::SharedMemory::map(WTFMove(*dataHandle), WebCore::SharedMemory::Protection::ReadOnly) : nullptr;
    auto data = sharedData ? sharedData->span() : std::span<const uint8_t> { };
    if (!m_isMapped || !m_mapModeFlags.contains(WebCore::WebGPU::MapMode::Write)) {
        completionHandler(false);
        return;
    }

#if !ENABLE(WEBGPU_SWIFT)
    auto buffer = protectedBacking()->getBufferContents();
    if (buffer.empty()) {
        completionHandler(false);
        return;
    }

    auto endOffset = checkedSum<size_t>(offset, data.size());
    if (endOffset.hasOverflowed() || endOffset.value() > buffer.size()) {
        completionHandler(false);
        return;
    }

    memcpySpan(buffer.subspan(offset), data);
#else
    backing().copy(data, offset);
#endif
    completionHandler(true);

}

void RemoteBuffer::destroy()
{
    unmap();
    protectedBacking()->destroy();
}

void RemoteBuffer::destruct()
{
    Ref { m_objectHeap.get() }->removeObject(m_identifier);
}

void RemoteBuffer::setLabel(String&& label)
{
    protectedBacking()->setLabel(WTFMove(label));
}

Ref<WebCore::WebGPU::Buffer> RemoteBuffer::protectedBacking()
{
    return m_backing;
}

Ref<IPC::StreamServerConnection> RemoteBuffer::protectedStreamConnection() const
{
    return m_streamConnection;
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
