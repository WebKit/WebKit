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

#include "RemoteAdapterProxy.h"
#include "WebGPUIdentifier.h"
#include <WebCore/WebGPUQueue.h>
#include <wtf/Deque.h>

namespace WebKit::WebGPU {

class ConvertToBackingContext;

class RemoteQueueProxy final : public WebCore::WebGPU::Queue {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteQueueProxy> create(RemoteAdapterProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    {
        return adoptRef(*new RemoteQueueProxy(parent, convertToBackingContext, identifier));
    }

    virtual ~RemoteQueueProxy();

    RemoteAdapterProxy& parent() { return m_parent; }
    RemoteGPUProxy& root() { return m_parent->root(); }

private:
    friend class DowncastConvertToBackingContext;

    RemoteQueueProxy(RemoteAdapterProxy&, ConvertToBackingContext&, WebGPUIdentifier);

    RemoteQueueProxy(const RemoteQueueProxy&) = delete;
    RemoteQueueProxy(RemoteQueueProxy&&) = delete;
    RemoteQueueProxy& operator=(const RemoteQueueProxy&) = delete;
    RemoteQueueProxy& operator=(RemoteQueueProxy&&) = delete;

    WebGPUIdentifier backing() const { return m_backing; }
    
    static inline constexpr Seconds defaultSendTimeout = 30_s;
    template<typename T>
    WARN_UNUSED_RETURN IPC::Error send(T&& message)
    {
        return root().streamClientConnection().send(WTFMove(message), backing(), defaultSendTimeout);
    }
    template<typename T>
    WARN_UNUSED_RETURN IPC::Connection::SendSyncResult<T> sendSync(T&& message)
    {
        return root().streamClientConnection().sendSync(WTFMove(message), backing(), defaultSendTimeout);
    }

    void submit(Vector<std::reference_wrapper<WebCore::WebGPU::CommandBuffer>>&&) final;

    void onSubmittedWorkDone(CompletionHandler<void()>&&) final;

    void writeBuffer(
        const WebCore::WebGPU::Buffer&,
        WebCore::WebGPU::Size64 bufferOffset,
        const void* source,
        size_t byteLength,
        WebCore::WebGPU::Size64 dataOffset = 0,
        std::optional<WebCore::WebGPU::Size64> = std::nullopt) final;

    void writeTexture(
        const WebCore::WebGPU::ImageCopyTexture& destination,
        const void* source,
        size_t byteLength,
        const WebCore::WebGPU::ImageDataLayout&,
        const WebCore::WebGPU::Extent3D& size) final;

    void writeBuffer(
        const WebCore::WebGPU::Buffer&,
        WebCore::WebGPU::Size64 bufferOffset,
        void* source,
        size_t byteLength,
        WebCore::WebGPU::Size64 dataOffset = 0,
        std::optional<WebCore::WebGPU::Size64> = std::nullopt) final;

    void writeTexture(
        const WebCore::WebGPU::ImageCopyTexture& destination,
        void* source,
        size_t byteLength,
        const WebCore::WebGPU::ImageDataLayout&,
        const WebCore::WebGPU::Extent3D& size) final;

    void copyExternalImageToTexture(
        const WebCore::WebGPU::ImageCopyExternalImage& source,
        const WebCore::WebGPU::ImageCopyTextureTagged& destination,
        const WebCore::WebGPU::Extent3D& copySize) final;

    void setLabelInternal(const String&) final;

    Deque<CompletionHandler<void()>> m_callbacks;

    WebGPUIdentifier m_backing;
    Ref<ConvertToBackingContext> m_convertToBackingContext;
    Ref<RemoteAdapterProxy> m_parent;
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
