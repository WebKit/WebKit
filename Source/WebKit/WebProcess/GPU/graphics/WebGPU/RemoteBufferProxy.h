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

#pragma once

#if ENABLE(GPU_PROCESS)

#include "RemoteDeviceProxy.h"
#include "WebGPUIdentifier.h"
#include <WebCore/WebGPUBuffer.h>
#include <wtf/Deque.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit::WebGPU {

class ConvertToBackingContext;

class RemoteBufferProxy final : public WebCore::WebGPU::Buffer {
    WTF_MAKE_TZONE_ALLOCATED(RemoteBufferProxy);
public:
    static Ref<RemoteBufferProxy> create(RemoteDeviceProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier, bool mappedAtCreation)
    {
        return adoptRef(*new RemoteBufferProxy(parent, convertToBackingContext, identifier, mappedAtCreation));
    }

    virtual ~RemoteBufferProxy();

    RemoteDeviceProxy& parent() { return m_parent; }
    RemoteGPUProxy& root() { return m_parent->root(); }

private:
    friend class DowncastConvertToBackingContext;

    RemoteBufferProxy(RemoteDeviceProxy&, ConvertToBackingContext&, WebGPUIdentifier, bool mappedAtCreation);

    RemoteBufferProxy(const RemoteBufferProxy&) = delete;
    RemoteBufferProxy(RemoteBufferProxy&&) = delete;
    RemoteBufferProxy& operator=(const RemoteBufferProxy&) = delete;
    RemoteBufferProxy& operator=(RemoteBufferProxy&&) = delete;

    WebGPUIdentifier backing() const { return m_backing; }
    
    template<typename T>
    WARN_UNUSED_RETURN IPC::Error send(T&& message)
    {
        return root().streamClientConnection().send(WTFMove(message), backing());
    }
    template<typename T>
    WARN_UNUSED_RETURN IPC::Connection::SendSyncResult<T> sendSync(T&& message)
    {
        return root().streamClientConnection().sendSync(WTFMove(message), backing());
    }
    template<typename T, typename C>
    WARN_UNUSED_RETURN IPC::StreamClientConnection::AsyncReplyID sendWithAsyncReply(T&& message, C&& completionHandler)
    {
        return root().streamClientConnection().sendWithAsyncReply(WTFMove(message), completionHandler, backing());
    }

    void mapAsync(WebCore::WebGPU::MapModeFlags, WebCore::WebGPU::Size64 offset, std::optional<WebCore::WebGPU::Size64> sizeForMap, CompletionHandler<void(bool)>&&) final;
    void getMappedRange(WebCore::WebGPU::Size64 offset, std::optional<WebCore::WebGPU::Size64>, Function<void(std::span<uint8_t>)>&&) final;
    std::span<uint8_t> getBufferContents() final;
    void unmap() final;
    void copy(std::span<const uint8_t>, size_t offset) final;

    void destroy() final;

    void setLabelInternal(const String&) final;

    Deque<CompletionHandler<void()>> m_callbacks;

    WebGPUIdentifier m_backing;
    Ref<ConvertToBackingContext> m_convertToBackingContext;
    Ref<RemoteDeviceProxy> m_parent;
    WebCore::WebGPU::MapModeFlags m_mapModeFlags;
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
