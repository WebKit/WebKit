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

#include "RenderingBackendIdentifier.h"
#include "StreamClientConnection.h"
#include "WebGPUIdentifier.h"
#include <WebCore/WebGPU.h>
#include <WebCore/WebGPUPresentationContext.h>
#include <wtf/Deque.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {
class IntSize;
class GraphicsContext;
class NativeImage;
}

namespace WebKit {

namespace WebGPU {
class ConvertToBackingContext;
class DowncastConvertToBackingContext;
}

class RemoteGPUProxy final : public WebCore::WebGPU::GPU, private IPC::Connection::Client, public ThreadSafeRefCounted<RemoteGPUProxy> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static RefPtr<RemoteGPUProxy> create(IPC::Connection&, WebGPU::ConvertToBackingContext&, WebGPUIdentifier, RenderingBackendIdentifier);

    virtual ~RemoteGPUProxy();

    RemoteGPUProxy& root() { return *this; }

    IPC::StreamClientConnection& streamClientConnection() { return *m_streamConnection; }

    void ref() const final { return ThreadSafeRefCounted<RemoteGPUProxy>::ref(); }
    void deref() const final { return ThreadSafeRefCounted<RemoteGPUProxy>::deref(); }

    void paintToCanvas(WebCore::NativeImage&, const WebCore::IntSize&, WebCore::GraphicsContext&) final;

private:
    friend class WebGPU::DowncastConvertToBackingContext;

    RemoteGPUProxy(IPC::Connection&, Ref<IPC::StreamClientConnection>, WebGPU::ConvertToBackingContext&, WebGPUIdentifier);
    void initializeIPC(IPC::StreamServerConnection::Handle&&, RenderingBackendIdentifier);

    RemoteGPUProxy(const RemoteGPUProxy&) = delete;
    RemoteGPUProxy(RemoteGPUProxy&&) = delete;
    RemoteGPUProxy& operator=(const RemoteGPUProxy&) = delete;
    RemoteGPUProxy& operator=(RemoteGPUProxy&&) = delete;

    // IPC::Connection::Client
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    void didClose(IPC::Connection&) final;
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName) final { }

    // Messages to be received.
    void wasCreated(bool didSucceed, IPC::Semaphore&& wakeUpSemaphore, IPC::Semaphore&& clientWaitSemaphore);

    void waitUntilInitialized();

    WebGPUIdentifier backing() const { return m_backing; }
    
    static inline constexpr Seconds defaultSendTimeout = 30_s;
    template<typename T>
    WARN_UNUSED_RETURN IPC::Error send(T&& message)
    {
        return root().streamClientConnection().send(std::forward<T>(message), backing(), defaultSendTimeout);
    }
    template<typename T>
    WARN_UNUSED_RETURN IPC::Connection::SendSyncResult<T> sendSync(T&& message)
    {
        return root().streamClientConnection().sendSync(std::forward<T>(message), backing(), defaultSendTimeout);
    }

    void requestAdapter(const WebCore::WebGPU::RequestAdapterOptions&, CompletionHandler<void(RefPtr<WebCore::WebGPU::Adapter>&&)>&&) final;

    Ref<WebCore::WebGPU::PresentationContext> createPresentationContext(const WebCore::WebGPU::PresentationContextDescriptor&) final;

    Ref<WebCore::WebGPU::CompositorIntegration> createCompositorIntegration() final;

    void abandonGPUProcess();
    void disconnectGpuProcessIfNeeded();

    Deque<CompletionHandler<void(RefPtr<WebCore::WebGPU::Adapter>&&)>> m_callbacks;

    WebGPUIdentifier m_backing;
    Ref<WebGPU::ConvertToBackingContext> m_convertToBackingContext;
    RefPtr<IPC::Connection> m_connection;
    bool m_didInitialize { false };
    bool m_lost { false };
    RefPtr<IPC::StreamClientConnection> m_streamConnection;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
