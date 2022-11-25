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

#include "StreamConnectionWorkQueue.h"
#include "StreamMessageReceiver.h"
#include "StreamServerConnection.h"
#include "WebGPUIdentifier.h"
#include "WebGPUObjectHeap.h"
#include "WebGPUSupportedFeatures.h"
#include "WebGPUSupportedLimits.h"
#include <WebCore/ProcessIdentifier.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Ref.h>
#include <wtf/ThreadAssertions.h>

namespace PAL::WebGPU {
class GPU;
}

namespace IPC {
class StreamServerConnection;
}

namespace WebKit {

class GPUConnectionToWebProcess;
class RemoteRenderingBackend;

namespace WebGPU {
class ObjectHeap;
struct RequestAdapterOptions;
}

class RemoteGPU final : public IPC::StreamMessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteGPU> create(WebGPUIdentifier identifier, GPUConnectionToWebProcess& gpuConnectionToWebProcess, RemoteRenderingBackend& renderingBackend, IPC::StreamServerConnection::Handle&& serverConnection)
    {
        auto result = adoptRef(*new RemoteGPU(identifier, gpuConnectionToWebProcess, renderingBackend, WTFMove(serverConnection)));
        result->initialize();
        return result;
    }

    virtual ~RemoteGPU();

    void stopListeningForIPC(Ref<RemoteGPU>&& refFromConnection);

    struct RequestAdapterResponse {
        String name;
        WebGPU::SupportedFeatures features;
        WebGPU::SupportedLimits limits;
        bool isFallbackAdapter;

        template<class Encoder> void encode(Encoder& encoder) const
        {
            encoder << name;
            encoder << features;
            encoder << limits;
            encoder << isFallbackAdapter;
        }

        template<class Decoder> static std::optional<RequestAdapterResponse> decode(Decoder& decoder)
        {
            std::optional<String> name;
            decoder >> name;
            if (!name)
                return std::nullopt;

            std::optional<WebGPU::SupportedFeatures> features;
            decoder >> features;
            if (!features)
                return std::nullopt;

            std::optional<WebGPU::SupportedLimits> limits;
            decoder >> limits;
            if (!limits)
                return std::nullopt;

            std::optional<bool> isFallbackAdapter;
            decoder >> isFallbackAdapter;
            if (!isFallbackAdapter)
                return std::nullopt;

            return { { WTFMove(*name), WTFMove(*features), WTFMove(*limits), *isFallbackAdapter } };
        }
    };

private:
    friend class WebGPU::ObjectHeap;

    RemoteGPU(WebGPUIdentifier, GPUConnectionToWebProcess&, RemoteRenderingBackend&, IPC::StreamServerConnection::Handle&&);

    RemoteGPU(const RemoteGPU&) = delete;
    RemoteGPU(RemoteGPU&&) = delete;
    RemoteGPU& operator=(const RemoteGPU&) = delete;
    RemoteGPU& operator=(RemoteGPU&&) = delete;

    void initialize();
    IPC::StreamConnectionWorkQueue& workQueue() const { return m_workQueue; }
    void workQueueInitialize();
    void workQueueUninitialize();

    template<typename T>
    bool send(T&& message) const
    {
        return m_streamConnection->send(WTFMove(message), m_identifier);
    }

    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder&) final;

    void requestAdapter(const WebGPU::RequestAdapterOptions&, WebGPUIdentifier, CompletionHandler<void(std::optional<RequestAdapterResponse>&&)>&&);

    WeakPtr<GPUConnectionToWebProcess> m_gpuConnectionToWebProcess;
    Ref<IPC::StreamConnectionWorkQueue> m_workQueue;
    RefPtr<IPC::StreamServerConnection> m_streamConnection;
    RefPtr<PAL::WebGPU::GPU> m_backing WTF_GUARDED_BY_CAPABILITY(workQueue());
    Ref<WebGPU::ObjectHeap> m_objectHeap WTF_GUARDED_BY_CAPABILITY(workQueue());
    const WebGPUIdentifier m_identifier;
    Ref<RemoteRenderingBackend> m_renderingBackend;
    const WebCore::ProcessIdentifier m_webProcessIdentifier;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
