/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "GPUProcessConnection.h"
#include "RenderingBackendIdentifier.h"
#include "StreamClientConnection.h"
#include "WebGPUIdentifier.h"
#include <pal/graphics/WebGPU/WebGPU.h>
#include <wtf/Deque.h>

namespace WebKit {
class GPUProcessConnection;
}

namespace WebKit {

namespace WebGPU {
class ConvertToBackingContext;
class DowncastConvertToBackingContext;
}

class RemoteGPUProxy final : public PAL::WebGPU::GPU, private IPC::Connection::Client, private GPUProcessConnection::Client {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteGPUProxy> create(GPUProcessConnection& gpuProcessConnection, WebGPU::ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier, RenderingBackendIdentifier renderingBackend)
    {
        return adoptRef(*new RemoteGPUProxy(gpuProcessConnection, convertToBackingContext, identifier, renderingBackend));
    }

    virtual ~RemoteGPUProxy();

    RemoteGPUProxy& root() { return *this; }

    IPC::StreamClientConnection& streamClientConnection() { return *m_streamConnection; }

private:
    friend class WebGPU::DowncastConvertToBackingContext;

    RemoteGPUProxy(GPUProcessConnection&, WebGPU::ConvertToBackingContext&, WebGPUIdentifier, RenderingBackendIdentifier);

    RemoteGPUProxy(const RemoteGPUProxy&) = delete;
    RemoteGPUProxy(RemoteGPUProxy&&) = delete;
    RemoteGPUProxy& operator=(const RemoteGPUProxy&) = delete;
    RemoteGPUProxy& operator=(RemoteGPUProxy&&) = delete;

    // IPC::Connection::Client
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    void didClose(IPC::Connection&) final { }
    void didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName) final { }

    // GPUProcessConnection::Client
    void gpuProcessConnectionDidClose(GPUProcessConnection&) final;

    // Messages to be received.
    void wasCreated(bool didSucceed, IPC::Semaphore&& wakeUpSemaphore, IPC::Semaphore&& clientWaitSemaphore);

    void waitUntilInitialized();

    WebGPUIdentifier backing() const { return m_backing; }
    
    static inline constexpr Seconds defaultSendTimeout = 30_s;
    template<typename T>
    WARN_UNUSED_RETURN bool send(T&& message)
    {
        return root().streamClientConnection().send(WTFMove(message), backing(), defaultSendTimeout);
    }
    template<typename T>
    WARN_UNUSED_RETURN IPC::Connection::SendSyncResult<T> sendSync(T&& message)
    {
        return root().streamClientConnection().sendSync(WTFMove(message), backing(), defaultSendTimeout);
    }
    IPC::Connection& connection() const { return m_gpuProcessConnection->connection(); }

    void requestAdapter(const PAL::WebGPU::RequestAdapterOptions&, CompletionHandler<void(RefPtr<PAL::WebGPU::Adapter>&&)>&&) final;

    void abandonGPUProcess();

    Deque<CompletionHandler<void(RefPtr<PAL::WebGPU::Adapter>&&)>> m_callbacks;

    WebGPUIdentifier m_backing;
    Ref<WebGPU::ConvertToBackingContext> m_convertToBackingContext;
    GPUProcessConnection* m_gpuProcessConnection;
    bool m_didInitialize { false };
    bool m_lost { false };
    RefPtr<IPC::StreamClientConnection> m_streamConnection;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
