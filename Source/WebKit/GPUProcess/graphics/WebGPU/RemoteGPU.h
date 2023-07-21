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

#include "RemoteGPURequestAdapterResponse.h"
#include "RemoteVideoFrameIdentifier.h"
#include "StreamConnectionWorkQueue.h"
#include "StreamMessageReceiver.h"
#include "StreamServerConnection.h"
#include "WebGPUIdentifier.h"
#include "WebGPUObjectHeap.h"
#include "WebGPUSupportedFeatures.h"
#include "WebGPUSupportedLimits.h"
#include <WebCore/MediaPlayerIdentifier.h>
#include <WebCore/ProcessIdentifier.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Ref.h>
#include <wtf/ThreadAssertions.h>

namespace WebCore::WebGPU {
class GPU;
struct PresentationContextDescriptor;
}

namespace IPC {
class StreamServerConnection;
}

namespace WebCore {
class MediaPlayer;
class VideoFrame;
}

namespace WebKit {

class GPUConnectionToWebProcess;
class RemoteRenderingBackend;

namespace WebGPU {
class ObjectHeap;
struct RequestAdapterOptions;
}

#if ENABLE(VIDEO) && PLATFORM(COCOA)
using PerformWithMediaPlayerOnMainThread = Function<void(std::variant<WebCore::MediaPlayerIdentifier, WebKit::RemoteVideoFrameReference>, Function<void(RefPtr<WebCore::VideoFrame>)>&&)>;
#else
using PerformWithMediaPlayerOnMainThread = Function<void()>;
#endif

class RemoteGPU final : public IPC::StreamMessageReceiver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<RemoteGPU> create(PerformWithMediaPlayerOnMainThread&& performWithMediaPlayerOnMainThread, WebGPUIdentifier identifier, GPUConnectionToWebProcess& gpuConnectionToWebProcess, RemoteRenderingBackend& renderingBackend, Ref<IPC::StreamServerConnection>&& serverConnection)
    {
        auto result = adoptRef(*new RemoteGPU(WTFMove(performWithMediaPlayerOnMainThread), identifier, gpuConnectionToWebProcess, renderingBackend, WTFMove(serverConnection)));
        result->initialize();
        return result;
    }

    virtual ~RemoteGPU();

    void stopListeningForIPC();

private:
    friend class WebGPU::ObjectHeap;

    RemoteGPU(PerformWithMediaPlayerOnMainThread&&, WebGPUIdentifier, GPUConnectionToWebProcess&, RemoteRenderingBackend&, Ref<IPC::StreamServerConnection>&&);

    RemoteGPU(const RemoteGPU&) = delete;
    RemoteGPU(RemoteGPU&&) = delete;
    RemoteGPU& operator=(const RemoteGPU&) = delete;
    RemoteGPU& operator=(RemoteGPU&&) = delete;

    void initialize();
    IPC::StreamConnectionWorkQueue& workQueue() const { return m_workQueue; }
    void workQueueInitialize();
    void workQueueUninitialize();

    template<typename T>
    IPC::Error send(T&& message) const
    {
        return m_streamConnection->send(WTFMove(message), m_identifier);
    }

    void didReceiveStreamMessage(IPC::StreamServerConnection&, IPC::Decoder&) final;

    void requestAdapter(const WebGPU::RequestAdapterOptions&, WebGPUIdentifier, CompletionHandler<void(std::optional<RemoteGPURequestAdapterResponse>&&)>&&);

    void createPresentationContext(const WebGPU::PresentationContextDescriptor&, WebGPUIdentifier);

    void createCompositorIntegration(WebGPUIdentifier);

    WeakPtr<GPUConnectionToWebProcess> m_gpuConnectionToWebProcess;
    Ref<IPC::StreamConnectionWorkQueue> m_workQueue;
    RefPtr<IPC::StreamServerConnection> m_streamConnection;
    RefPtr<WebCore::WebGPU::GPU> m_backing WTF_GUARDED_BY_CAPABILITY(workQueue());
    Ref<WebGPU::ObjectHeap> m_objectHeap WTF_GUARDED_BY_CAPABILITY(workQueue());
    PerformWithMediaPlayerOnMainThread m_performWithMediaPlayerOnMainThread;
    const WebGPUIdentifier m_identifier;
    Ref<RemoteRenderingBackend> m_renderingBackend;
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
